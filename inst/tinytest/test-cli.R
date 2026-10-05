expect_true(file.exists(minibwa_path()))
expect_true(minibwa_available())
expect_equal(minibwa_version(), "0.7-r427-dirty")
expect_false(minibwa_available(path = "definitely-not-a-minibwa-binary"))
expect_equal(minibwa_path("definitely-not-a-minibwa-binary", must_work = FALSE), NA_character_)
expect_error(
  minibwa_path("definitely-not-a-minibwa-binary", must_work = TRUE),
  "Could not find the minibwa executable"
)
expect_error(minibwa_cli(args = 1), "`args` must be a character vector")
expect_error(minibwa_index(reference = c("a", "b"), path = "missing"), "`reference`")
expect_error(minibwa_map(index = "idx", reads = character(), path = "missing"), "`reads`")

local({
  tmp <- tempfile("rminibwa-mmap-")
  dir.create(tmp)
  on.exit(unlink(tmp, recursive = TRUE, force = TRUE), add = TRUE)

  reference_sequence <- paste0(rep(
    "ACGTGCAATGCTAGCTACGATCGATGGCATCGTACCTGATCGTAGCTAGCTACGATCGATGC",
    8L
  ), collapse = "")
  read_sequence <- substr(reference_sequence, 101L, 200L)
  reference <- file.path(tmp, "reference.fa")
  reads <- file.path(tmp, "reads.fastq")
  prefix <- file.path(tmp, "reference")
  writeLines(c(">chr1", reference_sequence), reference, useBytes = TRUE)
  writeLines(c("@read1", read_sequence, "+", strrep("I", nchar(read_sequence))), reads, useBytes = TRUE)

  minibwa_index(reference, prefix = prefix)
  ordinary <- minibwa_map(prefix, reads)
  mapped <- minibwa_map(prefix, reads, extra_args = "--mmap=lite")
  ordinary <- ordinary[!startsWith(ordinary, "@")]
  mapped <- mapped[!startsWith(mapped, "@")]

  expect_true(length(ordinary) > 0L)
  expect_equal(mapped, ordinary)
})

local({
  tmp <- tempfile("rminibwa-gapped-alignment-")
  dir.create(tmp)
  on.exit(unlink(tmp, recursive = TRUE, force = TRUE), add = TRUE)

  reference_sequence <- paste0(
    "GAAGTGTCAGAGGAGGAGATGAAATATTTCTACTTTGTGACAAAGTTCAGAAAGGTATTTATTTATTTCATTGAATTTAGAATAAATTTT",
    "AGATTAATAGATGCAGTTACTTTGTTTTCCCATTTTTTTTTTTTTGGTTT"
  )
  read_sequence <- paste0(
    "GAAGTGTCAGAGGAGGAGATGAAATATTTCTACTTTGTGACAAAGTTCAGAAAGGTATTTATTTATTTCATTGAATTTAGAATAAATTTT",
    "AGATTAATAGATGCAGTTACTTTGTTTTCCCATTTTTTTTTTTTTTGGTT"
  )
  reference <- file.path(tmp, "reference.fa")
  reads <- file.path(tmp, "reads.fastq")
  prefix <- file.path(tmp, "reference")
  writeLines(c(">chr2", reference_sequence), reference, useBytes = TRUE)
  writeLines(c("@read", read_sequence, "+", strrep("I", nchar(read_sequence))), reads, useBytes = TRUE)

  minibwa_index(reference, prefix = prefix)
  output <- minibwa_map(prefix, reads)
  records <- output[!startsWith(output, "@")]
  expect_equal(length(records), 1L)
  fields <- strsplit(records, "\t", fixed = TRUE)[[1L]]
  expect_equal(fields[[6L]], "122M1I17M")
  expect_true("NM:i:1" %in% fields)
})

local({
  tmp <- tempfile("rminibwa-insert-size-")
  dir.create(tmp)
  on.exit(unlink(tmp, recursive = TRUE, force = TRUE), add = TRUE)

  set.seed(416L)
  reference_sequence <- paste0(sample(c("A", "C", "G", "T"), 50000L, replace = TRUE), collapse = "")
  complement <- function(x) {
    paste0(rev(strsplit(chartr("ACGT", "TGCA", x), "", fixed = TRUE)[[1L]]), collapse = "")
  }
  starts <- 501L + seq.int(0L, 39L) * 1000L
  inserts <- 300L + (seq_along(starts) - 1L) %% 5L * 10L
  r1 <- substring(reference_sequence, starts, starts + 99L)
  r2 <- vapply(
    seq_along(starts),
    function(i) complement(substring(
      reference_sequence,
      starts[[i]] + inserts[[i]] - 100L,
      starts[[i]] + inserts[[i]] - 1L
    )),
    character(1)
  )
  names <- paste0("pair", seq_along(starts) - 1L)
  fastq <- function(path, names, reads, mate) {
    records <- unlist(Map(
      function(name, read) c(paste0("@", name, "/", mate), read, "+", strrep("I", nchar(read))),
      names,
      reads
    ), use.names = FALSE)
    writeLines(records, path, useBytes = TRUE)
  }

  reference <- file.path(tmp, "reference.fa")
  reads1 <- file.path(tmp, "reads1.fastq")
  reads2 <- file.path(tmp, "reads2.fastq")
  prefix <- file.path(tmp, "reference")
  writeLines(c(">chr1", reference_sequence), reference, useBytes = TRUE)
  fastq(reads1, names, r1, 1L)
  fastq(reads2, names, r2, 2L)
  minibwa_index(reference, prefix = prefix)

  auto <- minibwa_map(prefix, c(reads1, reads2))
  fixed <- minibwa_map(prefix, c(reads1, reads2), extra_args = c("-I", "100,10,80,120"))
  primary_flags <- function(sam) {
    records <- sam[!startsWith(sam, "@")]
    flags <- as.integer(vapply(strsplit(records, "\t", fixed = TRUE), `[[`, character(1), 2L))
    flags[bitwAnd(flags, 0x900L) == 0L]
  }

  auto_flags <- primary_flags(auto)
  fixed_flags <- primary_flags(fixed)
  expect_equal(length(auto_flags), 80L)
  expect_equal(sum(bitwAnd(auto_flags, 0x2L) != 0L), 80L)
  expect_equal(sum(bitwAnd(fixed_flags, 0x2L) != 0L), 0L)
})
