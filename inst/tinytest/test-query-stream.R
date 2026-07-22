local({
  tmp <- tempfile("rminibwa-query-stream-")
  dir.create(tmp)
  on.exit(unlink(tmp, recursive = TRUE), add = TRUE)

  ref <- paste(rep("ACGT", 1000), collapse = "")
  fa <- file.path(tmp, "ref.fa")
  writeLines(c(">chr1", ref), fa, useBytes = TRUE)
  prefix <- file.path(tmp, "idx")
  expect_silent(mb_index_build(fa, prefix, threads = 1L))
  idx <- mb_index_load(prefix)

  write_fastq <- function(path, names, reads) {
    lines <- as.vector(rbind(paste0("@", names), reads, "+", strrep("I", nchar(reads))))
    writeLines(lines, path, useBytes = TRUE)
  }
  mapped <- substr(ref, 1L, 100L)
  unmapped <- paste(rep("N", 100L), collapse = "")

  single <- file.path(tmp, "single.fq")
  write_fastq(single, c("one", "two"), c(mapped, unmapped))
  stream <- mb_query_stream(
    single, idx, mb_opts("sr", out_n = 0L), mode = "single", threads = 1L,
    read_group = "@RG\tID:rg1\tSM:sample\tLB:library", max_seen_qnames = 10L
  )
  first <- mb_query_stream_next(stream)
  expect_true(inherits(first, "rminibwa_query_group"))
  expect_equal(rawToChar(mb_query_group_name(first)), "one")
  expect_equal(mb_query_group_input_order(first), 0)
  expect_equal(mb_query_group_n_reads(first), 1L)
  expect_true(mb_query_group_n_records(first) >= 1L)
  second <- mb_query_stream_next(stream)
  expect_equal(rawToChar(mb_query_group_name(second)), "two")
  expect_equal(mb_query_group_input_order(second), 1)
  expect_equal(mb_query_group_n_reads(second), 1L)
  expect_equal(mb_query_group_n_records(second), 1L)
  expect_error(mb_query_group_n_records(first), "no longer live")
  expect_null(mb_query_stream_next(stream))

  mate1 <- file.path(tmp, "r1.fq")
  mate2 <- file.path(tmp, "r2.fq")
  write_fastq(mate1, "pair/1", mapped)
  write_fastq(mate2, "pair/2", unmapped)
  paired <- mb_query_stream(
    c(mate1, mate2), idx, mb_opts("sr", out_n = 0L), mode = "paired", threads = 1L,
    max_seen_qnames = 10L
  )
  pair_group <- mb_query_stream_next(paired)
  expect_equal(rawToChar(mb_query_group_name(pair_group)), "pair")
  expect_equal(mb_query_group_n_reads(pair_group), 2L)
  expect_true(mb_query_group_n_records(pair_group) >= 2L)
  expect_null(mb_query_stream_next(paired))

  repeated <- file.path(tmp, "repeated.fq")
  write_fastq(repeated, c("same", "same"), c(mapped, mapped))
  repeated_stream <- mb_query_stream(
    repeated, idx, mb_opts("sr", out_n = 0L), mode = "single", threads = 1L,
    max_seen_qnames = 10L
  )
  expect_true(inherits(mb_query_stream_next(repeated_stream), "rminibwa_query_group"))
  expect_error(mb_query_stream_next(repeated_stream), "query stream failed")
  expect_equal(mb_query_stream_error(repeated_stream)$code, 7L)

  mismatch <- file.path(tmp, "mismatch-r2.fq")
  write_fastq(mismatch, "other/2", mapped)
  mismatch_stream <- mb_query_stream(
    c(mate1, mismatch), idx, mb_opts("sr", out_n = 0L), mode = "paired", threads = 1L,
    max_seen_qnames = 10L
  )
  expect_error(mb_query_stream_next(mismatch_stream), "query stream failed")
  expect_equal(mb_query_stream_error(mismatch_stream)$code, 5L)

  truncated <- file.path(tmp, "truncated.fq")
  writeLines(c("@bad", mapped, "+"), truncated, useBytes = TRUE)
  truncated_stream <- mb_query_stream(
    truncated, idx, mb_opts("sr", out_n = 0L), mode = "single", threads = 1L,
    max_seen_qnames = 10L
  )
  expect_error(mb_query_stream_next(truncated_stream), "query stream failed")
  expect_equal(mb_query_stream_error(truncated_stream)$code, 2L)

  cancellable <- mb_query_stream(
    single, idx, mb_opts("sr", out_n = 0L), mode = "single", threads = 1L,
    max_seen_qnames = 10L
  )
  expect_true(inherits(mb_query_stream_next(cancellable), "rminibwa_query_group"))
  expect_silent(mb_query_stream_cancel(cancellable))
  expect_null(mb_query_stream_next(cancellable))
  expect_equal(mb_query_stream_error(cancellable)$code, 1L)
})
