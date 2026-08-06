# Rminibwa

Rminibwa is an R interface to [minibwa](https://github.com/lh3/minibwa),
Heng Li’s genomic read aligner. It vendors a pinned upstream source
tree, builds a package-provided `minibwa` executable, and exposes a
native C-backed API for in-process alignment work.

The CLI wrappers are kept close to upstream behavior. The native
interface uses raw query bytes, external-pointer alignment batches,
ALTREP alignment/read column views, and a small C header for downstream
packages. SIMD-sensitive KSW code is compiled as separate staged
backends and selected at runtime.

## Installation

``` r

install.packages(
  "Rminibwa",
  repos = c(
    "https://rgenomicsetl.r-universe.dev",
    "https://cloud.r-project.org"
  )
)
```

From a local checkout:

``` bash
R CMD INSTALL .
```

Rminibwa builds `minibwa` from the vendored source during package
installation. No external `minibwa` executable is needed for normal use.

## CLI wrapper

``` r

library(Rminibwa)

minibwa_version()
prefix <- minibwa_index("ref.fa", threads = 8)
aln <- minibwa_map(prefix, "reads.fq.gz", format = "paf", threads = 8)
```

[`minibwa_map()`](https://rgenomicsetl.github.io/Rminibwa/reference/minibwa_map.md)
captures output by default. Pass `output = "aln.sam"` or
`output = "aln.paf"` to write directly to a file.

## Native batches

The native path avoids data frames in the hot alignment shape.
[`mb_map_batch()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_map_batch.md)
and
[`mb_map_fastx_batch()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_fastx_iter.md)
return external-pointer batches; columns are exposed lazily to R and can
also be read from C.

``` r

library(Rminibwa)

td <- tempfile("rminibwa-readme-")
dir.create(td)
ref <- paste(rep("ACGT", 1000), collapse = "")
fa <- file.path(td, "ref.fa")
writeLines(c(">chr1", ref), fa, useBytes = TRUE)
prefix <- file.path(td, "idx")

mb_index_build(fa, prefix, threads = 1L)
idx <- mb_index_load(prefix)

starts <- seq(1L, 1600L, by = 25L)
reads <- vapply(starts, function(i) substr(ref, i, i + 99L), character(1))
aln <- mb_map_batch(
  reads,
  idx,
  opt = mb_opts("sr", out_n = 0L),
  name = paste0("read", seq_along(reads))
)

aln
#> <rminibwa alignment batch>
#>   records:     3264
#>   reads:       64
#>   hit read ids: 0..63
#>   first tid:   0
#>   first qend:  100
#>   cigar bytes: 13056
```

## C consumers

`Rminibwa.h` is installed under `inst/include` and resolves the runtime
API with `R_GetCCallable()`. Downstream packages can use
`LinkingTo: Rminibwa` for the header and import Rminibwa at runtime
before calling the function pointers.

``` c
#include <Rminibwa.h>

SEXP summarize_alignment(SEXP x)
{
    const RmbAlignBatch *batch = Rminibwa_align_from_sexp(x);
    size_t n = Rminibwa_align_n(batch);
    size_t n_read = Rminibwa_align_n_read(batch);
    const int32_t *hit_count = Rminibwa_align_read_i32_col(batch, "hit_count");
    const int32_t *tid = Rminibwa_align_i32_col(batch, "tid");
    return Rf_ScalarInteger(n && n_read && hit_count && tid ? tid[0] : NA_INTEGER);
}
```

A complete in-process batch consumer compiled with Rtinycc is in
`vignettes/downstream-c-api.Rmd` and
`inst/capi/rminibwa_tinycc_consumer.c`.

## Query-group producer prototype

[`mb_query_stream()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_query_stream.md)
maps one complete normalized QNAME group at a time and exposes both
mates plus every primary, secondary, supplementary, or unmapped record
to a native consumer. The consumer can retain the template facts needed
for later duplicate decisions without materializing SAM or per-record R
objects. Paired groups currently use minibwa’s fixed option values; this
prototype is not a sample-level insert calibrator, coordinate sorter, or
BAM finalizer. The R helpers below are diagnostics only.

``` r

fq <- file.path(td, "query-groups.fq")
stream_reads <- reads[1:2]
writeLines(
  as.vector(rbind(c("@template-1", "@template-2"), stream_reads, "+", strrep("I", nchar(stream_reads)))),
  fq,
  useBytes = TRUE
)

stream <- mb_query_stream(
  fq, idx, mb_opts("sr", out_n = 0L),
  mode = "single", threads = 1L,
  read_group = "@RG\tID:rg1\tSM:example\tLB:example-library",
  max_seen_qnames = 10L
)
group <- mb_query_stream_next(stream)
group
#> <rminibwa query group>
#>   qname: template-1
#>   reads: 1
#>   records: 51
mb_query_stream_cancel(stream)
```

The C consumer checks `RMINIBWA_STREAM_ABI_VERSION`, obtains a
`Rminibwa_header_view` once, and then calls `Rminibwa_stream_next()`
followed by `Rminibwa_query_group_view()` for each group. The view
supplies original sequence/quality bytes, full BAM-packed CIGARs
including clips, SAM flags, mate fields, typed AS/NM/MQ/MC/RG tags,
input order, and exact `@SQ`, `@RG`, `@PG`, `SO:unsorted`, and
`GO:query` facts. Its pointers are borrowed until the next call, which
provides one-group back-pressure and bounded alignment memory.

This query-group/POD interface is a prototype. Rminibwa now has a hard
`Rduckhts` dependency: on native platforms, configure validates
[`Rduckhts::rduckhts_htslib_config()`](https://rgenomicsetl.r-universe.dev/Rduckhts/reference/rduckhts_htslib_config.html),
compiles against its installed headers, and links only to the exact
shared or static htslib artifact in that receipt.
[`rminibwa_htslib_info()`](https://rgenomicsetl.github.io/Rminibwa/reference/rminibwa_htslib_info.md)
proves that the native link and the validated Rduckhts receipt agree.
Browser-wasm builds keep the mapping runtime but omit this direct
adapter because `rwasm` exposes only the host-native Rduckhts link
artifact to a downstream configure script. Future native
alignment-format work can reuse this htslib rather than introduce
another build; this dependency does not select a BAM, CRAM, sorting, or
finalization architecture.

``` r

rminibwa_htslib_info()
```

A complete stream C-API smoke consumer is in
`inst/capi/rminibwa_tinycc_stream_consumer.c`; the full lifetime and
cancellation contract is documented in `vignettes/downstream-c-api.Rmd`.

## SIMD dispatch

The package follows the RsimdDispatch style: compile portable and
ISA-specific objects separately, then select an available backend at
runtime. On x86_64 this can include SSE4 and AVX2. On other
architectures the portable backend is used.

``` r

simd_info()
#> <rminibwa SIMD dispatch>
#>   selected:  avx2
#>   requested: auto
#>   mode:      rminibwa-ksw-staged
#>   compiled:  scalar, sse4, avx2
#>   available: scalar, sse4, avx2
#>   cpu:       <none>
#>   target:    x86_64
```

AVX-512 is not built by default. It is useful for experiments, but it
would add a larger dispatch and CI surface. Use `make asm` to inspect
the actual instruction families in local builds.

## Benchmarks

`make rdm MINIBWA_BINDINGS_ROOT=/path/to/minibwa-bindings` renders the
optional benchmark tables below. The workload uses a chrM-sized random
reference and an indel-mutated read so KSW is exercised.

## Development

``` bash
make rd       # roxygen docs
make test0    # tinytest suite against the installed package
make check    # R CMD check --as-cran --no-manual
make pkgdown  # local site build
make asm      # inspect staged backend assembly
```
