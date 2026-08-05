#!/usr/bin/env Rscript

args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !nzchar(args[[1L]])) {
  stop("usage: rduckhts-htslib-contract.R OUTPUT", call. = FALSE)
}

config <- Rduckhts::rduckhts_htslib_config(validate = TRUE)
required <- c(
  "contract_version",
  "htslib_version",
  "source_id",
  "build_id",
  "link",
  "cppflags",
  "ldflags"
)
missing <- setdiff(required, names(config))
if (length(missing)) {
  stop(
    "Rduckhts htslib contract is missing: ",
    paste(missing, collapse = ", "),
    call. = FALSE
  )
}
if (!identical(config$contract_version, 1L)) {
  stop(
    "unsupported Rduckhts htslib contract version: ",
    config$contract_version,
    call. = FALSE
  )
}
if (!identical(config$runtime_version, config$htslib_version)) {
  stop(
    "Rduckhts htslib runtime/receipt mismatch: ",
    config$runtime_version,
    " != ",
    config$htslib_version,
    call. = FALSE
  )
}

make_escape <- function(value) {
  value <- gsub("$", "$$", value, fixed = TRUE)
  gsub("#", paste0(intToUtf8(92L), "#"), value, fixed = TRUE)
}

writeLines(
  c(
    make_escape(config$cppflags),
    make_escape(config$ldflags),
    config$htslib_version,
    config$source_id,
    config$build_id,
    config$link
  ),
  args[[1L]],
  useBytes = TRUE
)
