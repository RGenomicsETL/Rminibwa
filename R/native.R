#' @useDynLib Rminibwa, .registration = TRUE
NULL

#' Build a minibwa index with the native library
#'
#' Builds index files at `prefix` using the vendored minibwa C library. The
#' package is GPL, so the native build includes minibwa's GPL-compatible
#' low-memory indexing path; `low_memory = FALSE` uses the default `libsais`
#' path.
#'
#' @param fasta Reference FASTA path. Gzip-compressed input is supported by
#'   minibwa/zlib.
#' @param prefix Output prefix for `.l2b`, `.mbw`, and optionally `.meth.mbw`.
#' @param meth Build the directional methylation index as well.
#' @param threads Number of index-build threads requested by minibwa.
#' @param low_memory Use minibwa's low-memory BWT builder.
#' @return `prefix`, invisibly.
#' @export
mb_index_build <- function(fasta, prefix, meth = FALSE, threads = 1L, low_memory = FALSE) {
  fasta <- rmb_path_scalar(fasta, "fasta", must_work = TRUE)
  prefix <- rmb_path_scalar(prefix, "prefix", must_work = FALSE)
  parent <- dirname(prefix)
  if (!dir.exists(parent)) {
    stop("output directory does not exist: ", parent, call. = FALSE)
  }
  meth <- rmb_logical_scalar(meth, "meth")
  low_memory <- rmb_logical_scalar(low_memory, "low_memory")
  threads <- rmb_positive_int_scalar(threads, "threads")
  invisible(.Call(RC_mb_index_build, fasta, prefix, meth, threads, low_memory))
  invisible(prefix)
}

#' Load a native minibwa index
#'
#' @param prefix Index prefix.
#' @param meth Load the methylation BWT index.
#' @return An external pointer handle to a native minibwa index.
#' @export
mb_index_load <- function(prefix, meth = FALSE) {
  prefix <- rmb_path_scalar(prefix, "prefix", must_work = FALSE)
  meth <- rmb_logical_scalar(meth, "meth")
  .Call(RC_mb_index_load, prefix, meth)
}

#' Return minibwa index contigs
#'
#' Names are returned as raw byte vectors to avoid forced string materialization.
#'
#' @param index Index handle returned by `mb_index_load()`.
#' @return A list with raw-vector `name` entries and numeric `length` values.
#' @export
mb_index_contigs <- function(index) {
  .Call(RC_mb_index_contigs, index)
}

#' Native minibwa options
#'
#' Returns a plain list consumed by native mapping functions. Only the selected
#' options are exposed initially; all other fields use minibwa defaults for the
#' chosen preset.
#'
#' @param preset One of `"adap"`, `"sr"`, or `"lr"`.
#' @param paired,methylation Optional logical overrides for minibwa flags.
#' @param out_n,min_seed_len,threads,match_score,mismatch_penalty,gap_open,gap_extend
#'   Optional integer overrides for common minibwa fields.
#' @return A plain list.
#' @export
mb_opts <- function(preset = c("adap", "sr", "lr"),
                    paired = NULL,
                    methylation = NULL,
                    out_n = NULL,
                    min_seed_len = NULL,
                    threads = NULL,
                    match_score = NULL,
                    mismatch_penalty = NULL,
                    gap_open = NULL,
                    gap_extend = NULL) {
  preset <- match.arg(preset)
  list(
    preset = preset,
    paired = rmb_nullable_logical_scalar(paired, "paired"),
    methylation = rmb_nullable_logical_scalar(methylation, "methylation"),
    out_n = rmb_nullable_int_scalar(out_n, "out_n"),
    min_seed_len = rmb_nullable_int_scalar(min_seed_len, "min_seed_len"),
    threads = rmb_nullable_positive_int_scalar(threads, "threads"),
    match_score = rmb_nullable_int_scalar(match_score, "match_score"),
    mismatch_penalty = rmb_nullable_int_scalar(mismatch_penalty, "mismatch_penalty"),
    gap_open = rmb_nullable_int_scalar(gap_open, "gap_open"),
    gap_extend = rmb_nullable_int_scalar(gap_extend, "gap_extend")
  )
}

#' Map raw sequence bytes with native minibwa
#'
#' `mb_map()` is pipe-friendly: the sequence bytes are the first argument.
#' The result is a native alignment batch external pointer. Use `mb_align_col()`
#' for ALTREP column views or pass the batch to downstream C consumers.
#'
#' @param x Raw vector of query sequence bytes.
#' @param index Index handle returned by `mb_index_load()`.
#' @param opt Options list from `mb_opts()`.
#' @param name Optional read name as raw bytes. Character names are accepted for
#'   convenience but raw bytes are preferred.
#' @param meth Methylation code: `"none"`, `"c2t"`, or `"g2a"`.
#' @return A native alignment batch external pointer.
#' @examples
#' # See vignettes for full index-backed examples.
#' @export
mb_map <- function(x, index, opt = mb_opts(), name = NULL, meth = c("none", "c2t", "g2a")) {
  if (!is.raw(x)) stop("x must be a raw vector of sequence bytes", call. = FALSE)
  meth <- match.arg(meth)
  if (!is.null(name) && !is.raw(name) && !(is.character(name) && length(name) == 1L && !is.na(name))) {
    stop("name must be NULL, raw bytes, or a non-missing character scalar", call. = FALSE)
  }
  .Call(RC_mb_map_raw, x, index, opt, name, meth)
}

#' Map a batch of query sequences with native minibwa
#'
#' `mb_map_batch()` maps many reads in one native call and returns the same
#' columnar alignment-batch object as `mb_map()`. Input may be a character
#' vector of sequence bytes or a list whose elements are raw vectors or
#' character scalars. Unlike the CLI reader, this direct batch API defaults to
#' unpaired mapping unless `opt = mb_opts(paired = TRUE)`. In paired mode, order
#' records as `R1, R2, R1, R2, ...`; this call pairs adjacent records and does
#' not group FASTQ records by name for you.
#'
#' @param x Character vector or list of raw vectors / character scalars.
#' @inheritParams mb_map
#' @param name Optional character vector of read names, or a list of raw vectors
#'   / character scalars. If supplied, it must have the same length as `x`.
#' @return A native alignment batch external pointer.
#' @export
mb_map_batch <- function(x, index, opt = mb_opts(), name = NULL) {
  if (!(is.character(x) || is.list(x)) || !length(x) || anyNA(x)) {
    stop("x must be a non-empty character vector or list of raw/character sequences", call. = FALSE)
  }
  if (!is.null(name)) {
    if (!(is.character(name) || is.list(name)) || length(name) != length(x) || anyNA(name)) {
      stop("name must be NULL or a character/list vector with the same length as x", call. = FALSE)
    }
  }
  .Call(RC_mb_map_batch, x, index, opt, name)
}

#' Map paired query sequence batches with native minibwa
#'
#' `mb_map_pair_batch()` is the explicit two-batch paired-end interface. `r1[i]`
#' and `r2[i]` are flattened as adjacent records before mapping, matching the
#' two-file CLI layout. The read sidecar records `fragment_id` and `mate`.
#'
#' @param r1,r2 Character vectors or lists of raw vectors / character scalars.
#'   They must have the same non-zero length.
#' @inheritParams mb_map_batch
#' @param name1,name2 Optional read-name vectors for `r1` and `r2`.
#' @return A native alignment batch external pointer.
#' @export
mb_map_pair_batch <- function(r1, r2, index, opt = mb_opts(), name1 = NULL, name2 = NULL) {
  if (!(is.character(r1) || is.list(r1)) || !length(r1) || anyNA(r1)) {
    stop("r1 must be a non-empty character vector or list of raw/character sequences", call. = FALSE)
  }
  if (!(is.character(r2) || is.list(r2)) || length(r2) != length(r1) || anyNA(r2)) {
    stop("r2 must be a character/list vector with the same length as r1", call. = FALSE)
  }
  if (is.null(name1) != is.null(name2)) {
    stop("name1 and name2 must both be NULL or both be supplied", call. = FALSE)
  }
  x <- vector("list", 2L * length(r1))
  x[seq.int(1L, length(x), by = 2L)] <- as.list(r1)
  x[seq.int(2L, length(x), by = 2L)] <- as.list(r2)
  name <- NULL
  if (!is.null(name1)) {
    if (!(is.character(name1) || is.list(name1)) || length(name1) != length(r1) || anyNA(name1)) {
      stop("name1 must be NULL or a character/list vector with the same length as r1", call. = FALSE)
    }
    if (!(is.character(name2) || is.list(name2)) || length(name2) != length(r2) || anyNA(name2)) {
      stop("name2 must be NULL or a character/list vector with the same length as r2", call. = FALSE)
    }
    name <- vector("list", length(x))
    name[seq.int(1L, length(name), by = 2L)] <- as.list(name1)
    name[seq.int(2L, length(name), by = 2L)] <- as.list(name2)
  }
  opt$paired <- TRUE
  mb_map_batch(x, index, opt = opt, name = name)
}

#' Iterate FASTA/FASTQ batches without R sequence materialization
#'
#' `mb_fastx_iter()` opens one FASTA/FASTQ file, or two mate files, using the
#' vendored minibwa reader. `mb_fastx_next()` returns a native FASTX batch, and
#' `mb_map_fastx_batch()` maps that batch directly from native sequence buffers.
#'
#' Pairing modes follow the CLI reader shape: `"none"` treats each record as a
#' fragment, `"by_name"` groups adjacent records in one file with matching query
#' names, and `"two_file"` reads one record from each of two files per fragment.
#' `"auto"` selects `"two_file"` for two paths and `"none"` for one path.
#'
#' @param path One FASTA/FASTQ path, or two paths for `paired = "two_file"`.
#' @param paired Pairing mode.
#' @param batch_bases Approximate number of sequence bases to read per batch.
#' @param max_batch_bases Maximum bases to read when extending a batch to keep a
#'   short paired fragment together.
#' @param with_qual,with_comment Preserve qualities/comments in the native batch.
#' @return `mb_fastx_iter()` returns a native iterator. `mb_fastx_next()` returns
#'   a native FASTX batch or `NULL` at EOF. `mb_fastx_n()` returns the number of
#'   records in a FASTX batch. `mb_map_fastx_batch()` returns a native alignment
#'   batch.
#' @export
mb_fastx_iter <- function(path,
                          paired = c("auto", "none", "by_name", "two_file"),
                          batch_bases = 100000000L,
                          max_batch_bases = batch_bases,
                          with_qual = FALSE,
                          with_comment = FALSE) {
  if (!is.character(path) || !length(path) || length(path) > 2L || anyNA(path) || any(!nzchar(path))) {
    stop("path must be a non-empty character vector of length one or two", call. = FALSE)
  }
  paired <- match.arg(paired)
  if (paired == "auto") paired <- if (length(path) == 2L) "two_file" else "none"
  if (paired == "two_file" && length(path) != 2L) stop("paired = 'two_file' requires two paths", call. = FALSE)
  if (paired != "two_file" && length(path) != 1L) stop("single-file pairing modes require one path", call. = FALSE)
  path <- normalizePath(path, winslash = "/", mustWork = TRUE)
  batch_bases <- rmb_positive_int_scalar(batch_bases, "batch_bases")
  max_batch_bases <- rmb_positive_int_scalar(max_batch_bases, "max_batch_bases")
  with_qual <- rmb_logical_scalar(with_qual, "with_qual")
  with_comment <- rmb_logical_scalar(with_comment, "with_comment")
  mode <- match(paired, c("none", "by_name", "two_file")) - 1L
  .Call(RC_mb_fastx_iter, path, mode, batch_bases, max_batch_bases, with_qual, with_comment)
}

#' @rdname mb_fastx_iter
#' @param iter Native FASTX iterator returned by `mb_fastx_iter()`.
#' @export
mb_fastx_next <- function(iter) {
  .Call(RC_mb_fastx_next, iter)
}

#' @rdname mb_fastx_iter
#' @param batch Native FASTX batch returned by `mb_fastx_next()`.
#' @export
mb_fastx_n <- function(batch) {
  .Call(RC_mb_fastx_n, batch)
}

#' @rdname mb_fastx_iter
#' @inheritParams mb_map
#' @export
mb_map_fastx_batch <- function(batch, index, opt = mb_opts()) {
  .Call(RC_mb_map_fastx_batch, batch, index, opt)
}

#' @export
print.rminibwa_fastx_iter <- function(x, ...) {
  cat("<rminibwa FASTX iterator>\n")
  invisible(x)
}

#' @export
print.rminibwa_fastx_batch <- function(x, ...) {
  cat("<rminibwa FASTX batch>\n")
  cat("  records: ", mb_fastx_n(x), "\n", sep = "")
  invisible(x)
}

#' Open a lossless native FASTQ query-group stream
#'
#' Opens a one-template-at-a-time, bounded-memory native stream for a
#' downstream C/C++/Rcpp BAM producer. Each `mb_query_stream_next()` result is
#' one complete normalized QNAME group and remains valid only until the next
#' call on the stream or cancellation. The normal producer path neither creates
#' SAM text nor calls R once per alignment record; downstream native consumers
#' should use the versioned API in `Rminibwa.h`.
#'
#' `mode` and `threads` are deliberately required. `threads` is the complete
#' caller-owned minibwa thread budget: the stream creates no additional mapping
#' worker pool. `read_group` is either `NULL` or an exact literal `@RG` record
#' containing an `ID:` tag. It is retained byte-for-byte; library and sample
#' metadata are never inferred. The native header view declares `SO:unsorted`
#' and `GO:query`: groups are contiguous input templates, not lexicographically
#' query-name sorted.
#'
#' Exact repeated-QNAME validation uses a caller-bounded name tracker. If the
#' limit is reached, the stream fails with a reason code rather than emitting an
#' ambiguous later group. Consumers must treat any stream error as an incomplete
#' downstream output.
#'
#' @param path One FASTQ path for `mode = "single"`, or two ordered mate FASTQ
#'   paths for `mode = "paired"`.
#' @param index A native minibwa index returned by [mb_index_load()].
#' @param opt Mapping options from [mb_opts()]. Its `threads` entry is replaced
#'   by `threads`; its paired flag is set from `mode`.
#' @param mode Explicit "single" or "paired" input mode.
#' @param threads Positive total thread budget owned by the caller.
#' @param read_group `NULL` or one exact `@RG` record with a non-empty `ID:`.
#' @param max_seen_qnames Positive maximum number of normalized QNAMEs tracked
#'   for exact repeated-name detection.
#' @return `mb_query_stream()` returns a native query stream.
#'   `mb_query_stream_next()` returns a native query group or `NULL` at EOF or
#'   after cancellation. Group helper functions expose only debug accounting;
#'   use the installed C API for record views.
#' @export
mb_query_stream <- function(path, index, opt = mb_opts(), mode, threads,
                            read_group = NULL, max_seen_qnames = 1000000L) {
  if (missing(mode)) stop("mode must be supplied explicitly as 'single' or 'paired'", call. = FALSE)
  if (missing(threads)) stop("threads must be supplied as the total thread budget", call. = FALSE)
  mode <- match.arg(mode, c("single", "paired"))
  expected_paths <- if (identical(mode, "single")) 1L else 2L
  if (!is.character(path) || length(path) != expected_paths || anyNA(path) || any(!nzchar(path))) {
    stop("path length must match the explicit stream mode", call. = FALSE)
  }
  path <- normalizePath(path, winslash = "/", mustWork = TRUE)
  threads <- rmb_positive_int_scalar(threads, "threads")
  max_seen_qnames <- rmb_positive_int_scalar(max_seen_qnames, "max_seen_qnames")
  if (!is.null(read_group) && (!is.character(read_group) || length(read_group) != 1L || is.na(read_group))) {
    stop("read_group must be NULL or one @RG character scalar", call. = FALSE)
  }
  .Call(
    RC_mb_query_stream_open, path, if (identical(mode, "paired")) 1L else 0L,
    index, opt, threads, read_group, max_seen_qnames
  )
}

#' @rdname mb_query_stream
#' @param stream A native stream returned by `mb_query_stream()`.
#' @export
mb_query_stream_next <- function(stream) {
  .Call(RC_mb_query_stream_next, stream)
}

#' @rdname mb_query_stream
#' @export
mb_query_stream_cancel <- function(stream) {
  invisible(.Call(RC_mb_query_stream_cancel, stream))
}

#' @rdname mb_query_stream
#' @export
mb_query_stream_error <- function(stream) {
  .Call(RC_mb_query_stream_error, stream)
}

#' @rdname mb_query_stream
#' @param group A native query group returned by `mb_query_stream_next()`.
#' @export
mb_query_group_n_reads <- function(group) {
  .Call(RC_mb_query_group_n_reads, group)
}

#' @rdname mb_query_stream
#' @export
mb_query_group_n_records <- function(group) {
  .Call(RC_mb_query_group_n_records, group)
}

#' @rdname mb_query_stream
#' @export
mb_query_group_name <- function(group) {
  .Call(RC_mb_query_group_name, group)
}

#' @rdname mb_query_stream
#' @export
mb_query_group_input_order <- function(group) {
  .Call(RC_mb_query_group_input_order, group)
}

#' @export
print.rminibwa_query_stream <- function(x, ...) {
  cat("<rminibwa query stream>\n")
  invisible(x)
}

#' @export
print.rminibwa_query_group <- function(x, ...) {
  cat("<rminibwa query group>\n")
  cat("  qname: ", rawToChar(mb_query_group_name(x)), "\n", sep = "")
  cat("  reads: ", mb_query_group_n_reads(x), "\n", sep = "")
  cat("  records: ", mb_query_group_n_records(x), "\n", sep = "")
  invisible(x)
}

# Internal count-only mapper used to keep developer benchmarks honest.
mb_map_count <- function(x, index, opt = mb_opts(), name = NULL, meth = c("none", "c2t", "g2a")) {
  if (!is.raw(x)) stop("x must be a raw vector of sequence bytes", call. = FALSE)
  meth <- match.arg(meth)
  if (!is.null(name) && !is.raw(name) && !(is.character(name) && length(name) == 1L && !is.na(name))) {
    stop("name must be NULL, raw bytes, or a non-missing character scalar", call. = FALSE)
  }
  .Call(RC_mb_map_count_raw, x, index, opt, name, meth)
}

#' Inspect native minibwa alignment batches
#'
#' `mb_align_col()` returns ALTREP views over native alignment columns. These
#' views do not copy columns unless R forces materialization. `mb_align_read_col()`
#' returns ALTREP views over the read-level sidecar with one row per input read,
#' including reads without hits. C consumers should prefer the installed
#' `Rminibwa.h` API.
#'
#' @param x Native alignment batch returned by `mb_map()` or `mb_map_batch()`.
#' @param name Column name. Alignment columns include `read_id`, `tid`, `qlen`,
#'   `qs`, `qe`, `tlen`, `ts`, `te`, `strand`, `mapq`, `score`, `matches`,
#'   `blen`, `flags`, `n_sub`, `cigar_offset`, and `cigar_n`. Read-sidecar
#'   columns include `read_id`, `length`, `fragment_id`, `mate`, `hit_offset`,
#'   and `hit_count`.
#' @return `mb_align_n()` returns the number of alignment records.
#'   `mb_align_read_n()` returns the number of input reads represented in the
#'   batch. `mb_align_col()` and `mb_align_read_col()` return ALTREP integer or
#'   real vectors. `mb_align_cigar_words()` returns a raw debug copy of packed
#'   minibwa CIGAR words.
#' @export
mb_align_n <- function(x) {
  .Call(RC_mb_align_n, x)
}

#' @rdname mb_align_n
#' @export
mb_align_read_n <- function(x) {
  .Call(RC_mb_align_read_n, x)
}

#' @rdname mb_align_n
#' @export
mb_align_col <- function(x, name) {
  if (!is.character(name) || length(name) != 1L || is.na(name)) {
    stop("name must be a non-missing character scalar", call. = FALSE)
  }
  .Call(RC_mb_align_col, x, name)
}

#' @rdname mb_align_n
#' @export
mb_align_read_col <- function(x, name) {
  if (!is.character(name) || length(name) != 1L || is.na(name)) {
    stop("name must be a non-missing character scalar", call. = FALSE)
  }
  .Call(RC_mb_align_read_col, x, name)
}

#' @rdname mb_align_n
#' @export
mb_align_cigar_words <- function(x) {
  .Call(RC_mb_align_cigar_words, x)
}

#' @export
print.rminibwa_align <- function(x, ...) {
  n <- mb_align_n(x)
  n_read <- mb_align_read_n(x)
  cat("<rminibwa alignment batch>\n")
  cat("  records:     ", n, "\n", sep = "")
  cat("  reads:       ", n_read, "\n", sep = "")
  if (n > 0) {
    read_id <- mb_align_col(x, "read_id")
    cat("  hit read ids: ", read_id[[1]], "..", read_id[[n]], "\n", sep = "")
    cat("  first tid:   ", mb_align_col(x, "tid")[[1]], "\n", sep = "")
    cat("  first qend:  ", mb_align_col(x, "qe")[[1]], "\n", sep = "")
  }
  cat("  cigar bytes: ", length(mb_align_cigar_words(x)), "\n", sep = "")
  invisible(x)
}

#' Select the runtime SIMD backend
#'
#' Rminibwa compiles staged KSW backend objects and switches among them at
#' runtime. Use `"auto"` for the best available backend, or force one of
#' `"scalar"`, `"sse4"`, or `"avx2"` for diagnostics and benchmarks. The
#' `"scalar"` backend is the portable SIMDe baseline with native intrinsics
#' disabled; compilers may still lower portable vector types efficiently.
#'
#' @param backend Character scalar backend name.
#' @return The selected backend, invisibly.
#' @export
simd_set_backend <- function(backend = "auto") {
  if (!is.character(backend) || length(backend) != 1L || is.na(backend) || !nzchar(backend)) {
    stop("backend must be a non-empty character scalar", call. = FALSE)
  }
  invisible(.Call(RC_simd_set_backend, backend))
}

#' Report the selected SIMD backend
#'
#' @return A character scalar.
#' @export
simd_backend <- function() {
  .Call(RC_simd_backend)
}

#' Report SIMD dispatch diagnostics
#'
#' @return An object of class `rminibwa_simd_info`: a named list with dispatch
#'   mode, requested backend, selected backend, compiled backends,
#'   CPU-supported backends, available backends, and target.
#' @export
simd_info <- function() {
  structure(.Call(RC_simd_info), class = "rminibwa_simd_info")
}

#' @export
print.rminibwa_simd_info <- function(x, ...) {
  collapse <- function(value) {
    if (!length(value)) return("<none>")
    paste(value, collapse = ", ")
  }
  cat("<rminibwa SIMD dispatch>\n")
  cat("  selected:  ", x$selected_backend, "\n", sep = "")
  cat("  requested: ", x$requested_backend, "\n", sep = "")
  cat("  mode:      ", x$dispatch_mode, "\n", sep = "")
  cat("  compiled:  ", collapse(x$compiled_backends), "\n", sep = "")
  cat("  available: ", collapse(x$available_backends), "\n", sep = "")
  cat("  cpu:       ", collapse(x$cpu_backends), "\n", sep = "")
  cat("  target:    ", x$target, "\n", sep = "")
  invisible(x)
}

# Internal diagnostic used by developer benchmarks.
simd_counters <- function(reset = FALSE) {
  reset <- rmb_logical_scalar(reset, "reset")
  .Call(RC_simd_counters, reset)
}

rmb_path_scalar <- function(x, what, must_work) {
  if (!is.character(x) || length(x) != 1L || is.na(x) || !nzchar(x)) {
    stop(what, " must be a non-empty character scalar", call. = FALSE)
  }
  normalizePath(x, winslash = "/", mustWork = must_work)
}

rmb_logical_scalar <- function(x, what) {
  if (!is.logical(x) || length(x) != 1L || is.na(x)) {
    stop(what, " must be TRUE or FALSE", call. = FALSE)
  }
  x
}

rmb_nullable_logical_scalar <- function(x, what) {
  if (is.null(x)) return(NULL)
  rmb_logical_scalar(x, what)
}

rmb_positive_int_scalar <- function(x, what) {
  x <- rmb_nullable_positive_int_scalar(x, what)
  if (is.null(x)) stop(what, " must be supplied", call. = FALSE)
  x
}

rmb_nullable_positive_int_scalar <- function(x, what) {
  if (is.null(x)) return(NULL)
  x <- rmb_nullable_int_scalar(x, what)
  if (x < 1L) stop(what, " must be positive", call. = FALSE)
  x
}

rmb_nullable_int_scalar <- function(x, what) {
  if (is.null(x)) return(NULL)
  if (!is.numeric(x) || length(x) != 1L || is.na(x)) {
    stop(what, " must be an integer scalar", call. = FALSE)
  }
  as.integer(x)
}
