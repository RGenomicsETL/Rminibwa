expect_true(file.exists(minibwa_path()))
expect_true(minibwa_available())
expect_equal(minibwa_version(), "0.4-r411-dirty")
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
