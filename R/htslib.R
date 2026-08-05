#' Inspect the htslib Supplied by Rduckhts
#'
#' Rminibwa links to the exact installed htslib contract exported by
#' Rduckhts. This function compares the version reached through Rminibwa's
#' native library with Rduckhts's validated build receipt.
#'
#' @param validate Whether Rduckhts should validate its installed headers,
#'   library, receipt, and loaded DuckHTS runtime.
#'
#' @return An object of class `rminibwa_htslib_info` containing the provider,
#'   linked and receipt versions, source and build identities, link mode,
#'   runtime feature bits/string, and compiled htslib features.
#'
#' @examples
#' rminibwa_htslib_info()
#'
#' @export
rminibwa_htslib_info <- function(validate = TRUE) {
  if (!is.logical(validate) || length(validate) != 1L || is.na(validate)) {
    stop("`validate` must be TRUE or FALSE.", call. = FALSE)
  }

  config <- Rduckhts::rduckhts_htslib_config(validate = validate)
  runtime <- .Call(RC_rminibwa_htslib_runtime)
  linked_version <- runtime$version
  if (!identical(linked_version, config$htslib_version)) {
    stop(
      "Rminibwa/Rduckhts htslib version mismatch: linked=",
      linked_version,
      ", receipt=",
      config$htslib_version,
      call. = FALSE
    )
  }
  if (isTRUE(validate) &&
      !identical(runtime$feature_bits, config$runtime_feature_bits)) {
    stop(
      "Rminibwa/Rduckhts htslib feature mismatch: linked=",
      runtime$feature_bits,
      ", Rduckhts runtime=",
      config$runtime_feature_bits,
      call. = FALSE
    )
  }

  structure(
    list(
      provider = "Rduckhts",
      linked_version = linked_version,
      receipt_version = config$htslib_version,
      source_id = config$source_id,
      build_id = config$build_id,
      link = config$link,
      feature_bits = runtime$feature_bits,
      feature_string = runtime$feature_string,
      features = config$features
    ),
    class = "rminibwa_htslib_info"
  )
}

#' @export
print.rminibwa_htslib_info <- function(x, ...) {
  cat(
    "<rminibwa_htslib_info>",
    "\n  provider: ", x$provider,
    "\n  version:  ", x$linked_version,
    "\n  build:    ", x$build_id,
    "\n  link:     ", x$link,
    "\n",
    sep = ""
  )
  invisible(x)
}
