#!/usr/bin/env Rscript

args <- commandArgs(trailingOnly = TRUE)
if (
  length(args) < 1L || length(args) > 2L ||
    !nzchar(args[[1L]]) ||
    (length(args) == 2L && !args[[2L]] %in% c("native", "wasm-no-link"))
) {
  stop(
    "usage: rduckhts-htslib-contract.R OUTPUT [native|wasm-no-link]",
    call. = FALSE
  )
}
mode <- if (length(args) == 2L) args[[2L]] else "native"

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

link_values <- if (identical(mode, "wasm-no-link")) {
  # rwasm runs package configure under host R. Its installed Rduckhts package
  # therefore contains host-native htslib objects, which wasm-ld must not see.
  # Keep the validated provider receipt, but expose no incompatible flags.
  c("", "", "unavailable-wasm")
} else {
  c(config$cppflags, config$ldflags, config$link)
}

writeLines(
  c(
    make_escape(link_values[[1L]]),
    make_escape(link_values[[2L]]),
    config$htslib_version,
    config$source_id,
    config$build_id,
    link_values[[3L]]
  ),
  args[[1L]],
  useBytes = TRUE
)
