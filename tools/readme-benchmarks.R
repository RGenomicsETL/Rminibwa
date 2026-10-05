run_readme_benchmarks <- function() {
  library(bench)
  library(reticulate)

  fmt_bench <- function(x, cols) {
    x <- as.data.frame(x[, cols])
    if ("expression" %in% names(x)) x$expression <- as.character(x$expression)
    fmt_us <- function(value) {
      if (is.numeric(value)) paste0(formatC(value, format = "f", digits = 1), " us")
      else as.character(value)
    }
    if ("min" %in% names(x)) x$min <- fmt_us(x$min)
    if ("median" %in% names(x)) x$median <- fmt_us(x$median)
    if ("itr/sec" %in% names(x)) x[["itr/sec"]] <- formatC(x[["itr/sec"]], format = "f", digits = 1)
    if ("mem_alloc" %in% names(x)) x$mem_alloc <- as.character(x$mem_alloc)
    x
  }

  bench_td <- tempfile("rminibwa-bench-")
  dir.create(bench_td, recursive = TRUE, showWarnings = FALSE)
  stopifnot(dir.exists(bench_td))
  on.exit(unlink(bench_td, recursive = TRUE), add = TRUE)
  on.exit(simd_set_backend("auto"), add = TRUE)

  set.seed(1)
  bench_ref <- paste(sample(c("A", "C", "G", "T"), 16569, replace = TRUE), collapse = "")
  delete_every <- function(x, every = 25L) {
    y <- strsplit(x, "", fixed = TRUE)[[1L]]
    paste(y[-seq(every, length(y), by = every)], collapse = "")
  }

  bench_fa <- file.path(bench_td, "ref.fa")
  writeLines(c(">chrM", bench_ref), bench_fa, useBytes = TRUE)
  bench_prefix <- file.path(bench_td, "idx")
  mb_index_build(bench_fa, bench_prefix, threads = 1L)
  bench_idx <- mb_index_load(bench_prefix)
  bench_opt <- mb_opts("sr", out_n = 0L)
  bench_query_raw <- charToRaw(delete_every(substr(bench_ref, 1000L, 12000L), every = 25L))
  bench_name_raw <- charToRaw("read1")

  internal_backends <- intersect(c("scalar", "sse4", "avx2"), simd_info()$available_backends)
  ksw_counts <- do.call(rbind, lapply(internal_backends, function(backend) {
    simd_set_backend(backend)
    invisible(Rminibwa:::simd_counters(TRUE))
    invisible(Rminibwa:::mb_map_count(bench_query_raw, bench_idx, bench_opt, bench_name_raw))
    data.frame(backend = backend, as.list(Rminibwa:::simd_counters(FALSE)), check.names = FALSE)
  }))

  cat("KSW calls on the benchmark read:\n\n")
  print(knitr::kable(ksw_counts, format = "pipe"))
  cat("\n")

  rminibwa_internal <- do.call(rbind, lapply(internal_backends, function(backend) {
    simd_set_backend(backend)
    out <- bench::mark(
      count_only = Rminibwa:::mb_map_count(bench_query_raw, bench_idx, bench_opt, bench_name_raw),
      iterations = 100,
      check = FALSE,
      time_unit = "us"
    )
    out$backend <- backend
    out
  }))

  cat("Count-only mapper timing by staged backend:\n\n")
  print(knitr::kable(
    fmt_bench(rminibwa_internal, c("backend", "min", "median", "itr/sec", "mem_alloc")),
    format = "pipe"
  ))
  cat("\n")

  py_path <- Sys.getenv("RMINIBWA_BENCH_PYTHONPATH")
  if (dir.exists(py_path)) {
    py_run_string(sprintf("import sys; sys.path.insert(0, %s)", shQuote(py_path)))
    py_minibwa <- import("minibwa", convert = FALSE)
    py_idx <- py_minibwa$Index$load(bench_prefix)
    py_opt <- py_minibwa$Opts("sr")
    invisible(py_opt$set_out_n(0L))

    compare_backend <- if ("avx2" %in% internal_backends) "avx2" else tail(internal_backends, 1L)
    simd_set_backend(compare_backend)
    bench_query <- rawToChar(bench_query_raw)
    external_compare <- bench::mark(
      rminibwa_count = Rminibwa:::mb_map_count(bench_query_raw, bench_idx, bench_opt, bench_name_raw),
      rminibwa_batch = mb_align_n(mb_map(bench_query_raw, bench_idx, bench_opt, bench_name_raw)),
      python_pyo3 = length(py_minibwa$map(py_idx, py_opt, "read1", bench_query, "none")),
      iterations = 100,
      check = FALSE,
      time_unit = "us"
    )

    cat("External comparison using Rminibwa backend `", compare_backend, "`:\n\n", sep = "")
    print(knitr::kable(
      fmt_bench(external_compare, c("expression", "min", "median", "itr/sec", "mem_alloc")),
      format = "pipe"
    ))
    cat("\n")
  }
}

run_readme_benchmarks()
rm(run_readme_benchmarks)
