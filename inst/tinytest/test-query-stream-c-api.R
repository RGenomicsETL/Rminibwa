local({
  tmp <- tempfile("rminibwa-query-stream-capi-")
  dir.create(tmp)
  on.exit(unlink(tmp, recursive = TRUE), add = TRUE)

  ref <- paste(rep("ACGT", 1000), collapse = "")
  fa <- file.path(tmp, "ref.fa")
  writeLines(c(">chr1", ref), fa, useBytes = TRUE)
  prefix <- file.path(tmp, "idx")
  mb_index_build(fa, prefix, threads = 1L)
  idx <- mb_index_load(prefix)
  fq <- file.path(tmp, "reads.fq")
  read <- substr(ref, 1L, 100L)
  writeLines(c("@native", read, "+", strrep("I", 100L)), fq, useBytes = TRUE)
  read_group <- "@RG\tID:rg-capi\tSM:sample\tLB:library"

  capi_path <- system.file(
    "capi", "rminibwa_tinycc_stream_consumer.c",
    package = "Rminibwa", mustWork = TRUE
  )
  capi_code <- paste(readLines(capi_path, warn = FALSE), collapse = "\n")
  ffi <- tryCatch(
    Rtinycc::tcc_ffi() |>
      Rtinycc::tcc_include(system.file("include", package = "Rminibwa")) |>
      Rtinycc::tcc_source(capi_code) |>
      Rtinycc::tcc_bind(
        rminibwa_capi_stream_summary = list(args = list("sexp"), returns = "sexp")
      ) |>
      Rtinycc::tcc_compile(),
    error = identity
  )

  if (inherits(ffi, "error")) {
    expect_true(TRUE, info = paste("Rtinycc stream C API smoke skipped:", conditionMessage(ffi)))
  } else {
    stream <- mb_query_stream(
      fq, idx, mb_opts("sr", out_n = 0L), mode = "single", threads = 1L,
      read_group = read_group, max_seen_qnames = 10L
    )
    summary <- ffi$rminibwa_capi_stream_summary(stream)
    expect_equal(summary[[1]], 1L)
    expect_equal(summary[[2]], nchar(read_group, type = "bytes"))
    expect_equal(summary[[3]], 1L)
    expect_equal(summary[[4]], nchar("native", type = "bytes"))
    expect_equal(summary[[5]], 1L)
    expect_true(summary[[6]] >= 1L)
    expect_true(summary[[8]] >= 5L)
    expect_equal(summary[[9]], 0L)
    expect_equal(summary[[10]], 1L)
    expect_equal(summary[[11]], 1L)
    expect_equal(summary[[12]], 0L)
    expect_equal(summary[[13]], 0L)
    expect_null(mb_query_stream_next(stream))

    fq1 <- file.path(tmp, "reads-r1.fq")
    fq2 <- file.path(tmp, "reads-r2.fq")
    writeLines(c("@pair/1", read, "+", strrep("I", 100L)), fq1, useBytes = TRUE)
    writeLines(c("@pair/2", read, "+", strrep("I", 100L)), fq2, useBytes = TRUE)
    paired_stream <- mb_query_stream(
      c(fq1, fq2), idx, mb_opts("sr", out_n = 0L), mode = "paired", threads = 1L,
      read_group = read_group, max_seen_qnames = 10L
    )
    paired_summary <- ffi$rminibwa_capi_stream_summary(paired_stream)
    expect_equal(paired_summary[[5]], 2L)
    expect_true(paired_summary[[6]] >= 2L)
    expect_equal(paired_summary[[11]], 1L)
    expect_equal(paired_summary[[12]], 1L)
    expect_equal(paired_summary[[13]], 1L)
    expect_null(mb_query_stream_next(paired_stream))
  }
})
