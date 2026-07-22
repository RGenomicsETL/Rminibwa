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
