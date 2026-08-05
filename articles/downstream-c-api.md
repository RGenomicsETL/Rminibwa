# Downstream C API

Rminibwa alignment batches are native external pointers with borrowed
column buffers. R users can inspect them through ALTREP vectors, but
downstream packages should consume them through the installed C header:

``` c
#include <Rminibwa.h>
```

The header resolves functions with `R_GetCCallable()`. A downstream
package therefore needs `LinkingTo: Rminibwa` for the header and an
ordinary R package runtime dependency to ensure Rminibwa is loaded
before the first call.

This vignette uses `Rtinycc` to compile the downstream consumer
in-process. The C source is displayed with Rtinycc’s C rendering helper
and compiled from the same `rminibwa_capi_code` object.

## Build a tiny alignment batch

``` r

library(Rminibwa)

td <- tempfile("rminibwa-capi-")
dir.create(td)
ref <- paste(rep("ACGT", 1000), collapse = "")
fa <- file.path(td, "ref.fa")
writeLines(c(">chr1", ref), fa, useBytes = TRUE)
prefix <- file.path(td, "idx")

mb_index_build(fa, prefix, threads = 1L)
idx <- mb_index_load(prefix)
aln <- charToRaw(substr(ref, 1L, 100L)) |>
  mb_map(idx, opt = mb_opts("sr", out_n = 0L), name = charToRaw("read1"))

mb_align_n(aln)
#> [1] 51
mb_align_col(aln, "tid")[[1]]
#> [1] 0
```

## The downstream C consumer

``` c
#define _Complex
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <R.h>
#include <Rinternals.h>
#include <Rminibwa.h>

static int clamp_size_to_int(size_t x)
{
    return x > (size_t) INT_MAX ? INT_MAX : (int) x;
}

SEXP rminibwa_capi_summary(SEXP align_x)
{
    const RmbAlignBatch *batch = Rminibwa_align_from_sexp(align_x);
    const size_t n = Rminibwa_align_n(batch);
    const size_t n_read = Rminibwa_align_n_read(batch);
    const int32_t *read_len = Rminibwa_align_read_i32_col(batch, "length");
    const int32_t *tid = Rminibwa_align_i32_col(batch, "tid");
    const int32_t *qe = Rminibwa_align_i32_col(batch, "qe");
    const int64_t *ts = Rminibwa_align_i64_col(batch, "ts");
    size_t n_cigar_words = 0;
    (void) Rminibwa_align_cigar_words(batch, &n_cigar_words);

    SEXP out = PROTECT(Rf_allocVector(INTSXP, 5));
    INTEGER(out)[0] = clamp_size_to_int(n);
    INTEGER(out)[1] = clamp_size_to_int(n_read);
    INTEGER(out)[2] = n_read && read_len ? read_len[0] : NA_INTEGER;
    INTEGER(out)[3] = n && tid ? tid[0] : NA_INTEGER;
    INTEGER(out)[4] = n && qe ? qe[0] : NA_INTEGER;

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 5));
    SET_STRING_ELT(names, 0, Rf_mkChar("n"));
    SET_STRING_ELT(names, 1, Rf_mkChar("n_read"));
    SET_STRING_ELT(names, 2, Rf_mkChar("first_read_length"));
    SET_STRING_ELT(names, 3, Rf_mkChar("first_tid"));
    SET_STRING_ELT(names, 4, Rf_mkChar("first_qe"));
    Rf_setAttrib(out, R_NamesSymbol, names);

    /* Touch one 64-bit column so this example covers both integer-width paths. */
    if (n_cigar_words == 0) INTEGER(out)[4] = NA_INTEGER;
    if (n && ts && ts[0] < 0) INTEGER(out)[4] = NA_INTEGER;

    UNPROTECT(2);
    return out;
}
```

## Compile and call it with Rtinycc

``` r

ffi <- tryCatch(
  Rtinycc::tcc_ffi() |>
    Rtinycc::tcc_include(system.file("include", package = "Rminibwa")) |>
    Rtinycc::tcc_source(rminibwa_capi_code) |>
    Rtinycc::tcc_bind(
      rminibwa_capi_summary = list(args = list("sexp"), returns = "sexp")
    ) |>
    Rtinycc::tcc_compile(),
  error = identity
)

if (inherits(ffi, "error")) {
  cat("Rtinycc compilation is unavailable on this platform during vignette build:\n")
  cat(conditionMessage(ffi), "\n")
} else {
  ffi$rminibwa_capi_summary(aln)
}
#>                 n            n_read first_read_length         first_tid 
#>                51                 1               100                 0 
#>          first_qe 
#>               100
```

The C function never asks R to materialize a data frame. It receives the
batch SEXP, obtains the opaque `RmbAlignBatch *`, and reads borrowed
`int32_t`, `int64_t`, and packed CIGAR buffers directly.

## Lossless query-group stream

For BAM preparation, use
[`mb_query_stream()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_query_stream.md)
rather than repeatedly converting alignment batches. It requires an
explicit `mode` and total `threads` budget, and returns exactly one
normalized QNAME group per native
[`next`](https://rdrr.io/r/base/Control.html) call. In paired mode it
rejects mismatched names, mate ordering/count errors, and malformed or
truncated FASTQ before an ambiguous group is emitted. A supplied `@RG`
record is retained exactly; no sample or library metadata is invented.

The installed API is deliberately a versioned POD/view interface rather
than a `bam1_t` handoff. Rminibwa itself now links to the exact htslib
build supplied by Rduckhts, so native adapters can encode BAM or CRAM
without crossing package boundaries with allocator-owned `bam1_t`
objects or loading another htslib contract. Its header view reports
`SO:unsorted` and `GO:query`: contiguous groups preserve input order but
are not query-name sorted:

``` c
if (Rminibwa_stream_abi_version() != RMINIBWA_STREAM_ABI_VERSION)
    Rf_error("Rminibwa stream ABI mismatch");

RmbQueryStream *stream = Rminibwa_stream_from_sexp(stream_x);
const RmbQueryGroup *group = NULL;
Rminibwa_header_view header;
Rminibwa_query_group_view_t view;

Rminibwa_stream_header(stream, &header); /* @SQ, exact @RG, @PG facts */
while (Rminibwa_stream_next(stream, &group) == RMINIBWA_STREAM_OK) {
    Rminibwa_query_group_view(group, &view);
    /* Consume view.reads and view.records before requesting the next group. */
    /* record.cigar and typed AS/NM/MQ/MC/RG tags are native, not SAM text. */
}
```

Every pointer in `Rminibwa_query_group_view_t` is borrowed and is valid
only until `Rminibwa_stream_next()`, `Rminibwa_stream_cancel()`, or
stream destruction. This creates one-group back-pressure: a consumer
pauses simply by not requesting another group.
`Rminibwa_stream_cancel()` releases the current alignment buffers and
leaves a reason code/message available through the C API (or
[`mb_query_stream_error()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_query_stream.md)
for diagnostics). The producer has no extra worker pool; `threads` is
the complete minibwa mapping budget.
