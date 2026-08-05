#!/usr/bin/env Rscript

# Vendor minibwa from the pinned upstream commit declared in
# tools/minibwa-upstream.dcf. This script intentionally vendors source from a
# specific commit because lh3/minibwa is still moving quickly.
#
# Usage from package root:
#   Rscript tools/vendor-minibwa.R download
#   Rscript tools/vendor-minibwa.R unpack
#   Rscript tools/vendor-minibwa.R refresh
#   Rscript tools/vendor-minibwa.R status

args <- commandArgs(trailingOnly = FALSE)
file_arg <- sub("^--file=", "", args[grepl("^--file=", args)])
script_dir <- if (length(file_arg)) dirname(normalizePath(file_arg[[1]], mustWork = TRUE)) else "tools"
root <- normalizePath(file.path(script_dir, ".."), mustWork = TRUE)

trailing <- commandArgs(trailingOnly = TRUE)
mode <- if (length(trailing)) trailing[[1]] else "status"

lock_path <- file.path(root, "tools", "minibwa-upstream.dcf")
lock <- read.dcf(lock_path, all = TRUE)
field <- function(name) {
  value <- lock[1L, name]
  if (is.na(value) || !nzchar(value)) stop("Missing field in minibwa lock file: ", name, call. = FALSE)
  value
}

component <- field("Component")
version <- field("Version")
repo <- field("Repository")
git_repo <- field("GitRepository")
commit <- field("Commit")
commit_date <- field("Date")
archive_url <- field("ArchiveURL")
archive_sha256 <- field("ArchiveSHA256")
patch_dir <- file.path(root, field("PatchDirectory"))
short <- substr(commit, 1L, 12L)

vendor_root <- file.path(root, "src", "vendor")
archive_dir <- file.path(vendor_root, "_archives")
archive_path <- file.path(archive_dir, paste0(component, "-", short, ".tar.gz"))
target_dir <- file.path(vendor_root, component)

run <- function(cmd, args, wd = root) {
  message("$ ", cmd, " ", paste(args, collapse = " "))
  old <- setwd(wd)
  on.exit(setwd(old), add = TRUE)
  status <- system2(cmd, args, stdout = "", stderr = "")
  if (!identical(status, 0L)) stop("Command failed: ", cmd, call. = FALSE)
}

sha256_file <- function(path) {
  unname(tools::sha256sum(path))
}

patches <- function() {
  if (!dir.exists(patch_dir)) return(character())
  sort(list.files(patch_dir, pattern = "\\.patch$", full.names = TRUE))
}

print_status <- function() {
  cat("Component: ", component, "\n", sep = "")
  cat("Version: ", version, "\n", sep = "")
  cat("Repository: ", repo, "\n", sep = "")
  cat("GitRepository: ", git_repo, "\n", sep = "")
  cat("Commit: ", commit, "\n", sep = "")
  cat("Date: ", commit_date, "\n", sep = "")
  cat("ArchiveURL: ", archive_url, "\n", sep = "")
  cat("ArchiveSHA256: ", archive_sha256, "\n", sep = "")
  cat("ArchivePath: ", archive_path, "\n", sep = "")
  cat("VendorDir: ", target_dir, "\n", sep = "")
  cat("Patches:\n")
  ps <- patches()
  if (length(ps)) {
    cat(paste0("  - ", basename(ps), collapse = "\n"), "\n", sep = "")
  } else {
    cat("  <none>\n")
  }
}

verify_archive <- function(path) {
  actual <- sha256_file(path)
  if (!identical(actual, archive_sha256)) {
    stop(
      "Unexpected archive SHA256: ", actual,
      " (expected ", archive_sha256, ")",
      call. = FALSE
    )
  }
  message("Archive SHA256: ", actual)
  invisible(actual)
}

download_archive <- function() {
  dir.create(archive_dir, recursive = TRUE, showWarnings = FALSE)
  message("Downloading ", component, " ", short, " to ", archive_path)
  utils::download.file(archive_url, archive_path, quiet = FALSE, mode = "wb")
  tryCatch(
    verify_archive(archive_path),
    error = function(e) {
      unlink(archive_path, force = TRUE)
      stop(e)
    }
  )
  invisible(archive_path)
}

verify_version <- function(path) {
  header <- file.path(path, "minibwa.h")
  if (!file.exists(header)) stop("Missing minibwa.h in vendored source", call. = FALSE)
  lines <- readLines(header, warn = FALSE)
  hit <- grep('^#define[[:space:]]+MB_VERSION[[:space:]]+"', lines, value = TRUE)
  if (!length(hit)) stop("Could not read MB_VERSION from minibwa.h", call. = FALSE)
  actual <- sub('^#define[[:space:]]+MB_VERSION[[:space:]]+"([^"]+)".*$', "\\1", hit[[1L]])
  if (!identical(actual, version)) {
    stop("Unexpected minibwa version: ", actual, " (expected ", version, ")", call. = FALSE)
  }
  invisible(actual)
}

apply_patches <- function(path) {
  ps <- patches()
  if (!length(ps)) return(character())
  if (!nzchar(Sys.which("git"))) stop("git is required to apply minibwa patches", call. = FALSE)
  helper <- file.path(root, "tools", "apply-minibwa-patches.sh")
  if (!file.exists(helper)) stop("Missing patch helper: ", helper, call. = FALSE)
  run(helper, path, wd = root)
  basename(ps)
}

prune_vendor_tree <- function(path) {
  # Keep top-level upstream source, headers, docs, and license notices. Drop
  # repository metadata, test data, manuscript assets, examples, and mimalloc;
  # Rminibwa links the ordinary allocator and compiles the library objects it
  # needs explicitly from Makevars.
  unlink(file.path(path, c(".git", ".github", ".gitignore", "api-test", "test", "tex", "mimalloc")),
         recursive = TRUE, force = TRUE)
  unlink(list.files(path, pattern = "\\.(o|a|so|dll|dylib|exe)$", full.names = TRUE),
         force = TRUE)
  unlink(file.path(path, "minibwa"), force = TRUE)
  invisible(TRUE)
}

write_vendor_metadata <- function(path, archive_sha, applied_patches) {
  metadata <- c(
    paste0("Component: ", component),
    paste0("Version: ", version),
    paste0("Repository: ", repo),
    paste0("GitRepository: ", git_repo),
    paste0("Commit: ", commit),
    paste0("Date: ", commit_date),
    paste0("ArchiveURL: ", archive_url),
    paste0("ArchiveSHA256: ", archive_sha),
    paste0("VendoredSource: src/vendor/", component),
    paste0("PatchDirectory: ", field("PatchDirectory")),
    paste0("Patches: ", if (length(applied_patches)) paste(applied_patches, collapse = ", ") else "none"),
    "VendoredScope: top-level minibwa C sources and headers; repository metadata, test data, manuscript assets, examples, and mimalloc omitted",
    paste0("VendoredAtUTC: ", format(Sys.time(), "%Y-%m-%dT%H:%M:%SZ", tz = "UTC")),
    paste0("License: ", field("License"))
  )
  writeLines(metadata, file.path(path, "RMINIBWA_VENDOR.dcf"), useBytes = TRUE)
}

unpack_archive <- function() {
  if (!file.exists(archive_path)) {
    stop("Archive not found: ", archive_path, "\nRun `Rscript tools/vendor-minibwa.R download` first.", call. = FALSE)
  }
  archive_sha <- verify_archive(archive_path)
  dir.create(vendor_root, recursive = TRUE, showWarnings = FALSE)
  unlink(target_dir, recursive = TRUE, force = TRUE)

  tmp <- tempfile("rminibwa-vendor-")
  dir.create(tmp)
  on.exit(unlink(tmp, recursive = TRUE, force = TRUE), add = TRUE)

  message("Unpacking ", archive_path)
  utils::untar(archive_path, exdir = tmp)
  dirs <- list.dirs(tmp, full.names = TRUE, recursive = FALSE)
  if (!length(dirs)) stop("Archive did not contain a top-level source directory", call. = FALSE)
  file.rename(dirs[[1L]], target_dir)

  verify_version(target_dir)
  applied <- apply_patches(target_dir)
  prune_vendor_tree(target_dir)
  write_vendor_metadata(target_dir, archive_sha, applied)
  message("Vendored ", component, " ", version, " commit ", commit)
  message("Wrote ", target_dir)
  invisible(target_dir)
}

clean_vendor <- function() {
  unlink(target_dir, recursive = TRUE, force = TRUE)
  message("Removed ", target_dir)
}

if (identical(mode, "status")) {
  print_status()
} else if (identical(mode, "download")) {
  download_archive()
} else if (identical(mode, "unpack")) {
  unpack_archive()
} else if (identical(mode, "refresh")) {
  download_archive()
  unpack_archive()
} else if (identical(mode, "clean")) {
  clean_vendor()
} else {
  stop("Unknown mode: ", mode, ". Use status, download, unpack, refresh, or clean.", call. = FALSE)
}
