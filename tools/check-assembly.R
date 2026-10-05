#!/usr/bin/env Rscript

args <- commandArgs(trailingOnly = TRUE)
root <- normalizePath(if (length(args)) args[[1L]] else ".", mustWork = TRUE)

objdump <- Sys.which("objdump")
if (!nzchar(objdump)) {
  stop("objdump is required for assembly inspection", call. = FALSE)
}

files <- character()
files <- c(files, Sys.glob(file.path(root, "src", "rmb-ksw", "*.o")))
files <- c(files, Sys.glob(file.path(root, "src", "Rminibwa.*")))

py <- Sys.getenv("RMINIBWA_BENCH_PYTHONPATH", unset = "")
if (nzchar(py) && dir.exists(py)) {
  files <- c(files, Sys.glob(file.path(py, "minibwa", "_minibwa*.so")))
  files <- c(files, Sys.glob(file.path(py, "minibwa", "_minibwa*.pyd")))
  files <- c(files, Sys.glob(file.path(py, "minibwa", "_minibwa*.dylib")))
}

files <- unique(normalizePath(files[file.exists(files)], winslash = "/", mustWork = FALSE))
if (!length(files)) {
  stop("no built objects/shared libraries found; run R CMD INSTALL --preclean . first", call. = FALSE)
}

patterns <- list(
  xmm = "\\bxmm[0-9]+\\b",
  ymm = "\\bymm[0-9]+\\b",
  zmm = "\\bzmm[0-9]+\\b",
  avx_v_mnemonics = "\\bv[a-z0-9_]+\\b",
  avx2_byte_ops = paste0(
    "\\b(",
    paste(c(
      "vpaddb", "vpsubb", "vpmaxsb", "vpminsb", "vpmaxub", "vpminub",
      "vpcmpeqb", "vpcmpgtb", "vpbroadcastb", "vpblendvb", "vpor", "vpand",
      "vpslldq", "vpsrldq", "vperm[a-z0-9_]+", "vinserti128", "vextracti128"
    ), collapse = "|"),
    ")\\b"
  ),
  sse4ish = "\\b(pblendvb|pextr[a-z0-9_]+|pinsr[a-z0-9_]+|pmovsx[a-z0-9_]+|pmaxs[a-z0-9_]+|pmins[a-z0-9_]+)\\b",
  sse2_ssse3ish = "\\b(pshuf[a-z0-9_]+|punpck[a-z0-9_]+|pack[a-z0-9_]+|padd[a-z0-9_]+|psub[a-z0-9_]+|pmaxu[a-z0-9_]+|pminu[a-z0-9_]+|pcmpeq[a-z0-9_]+|pcmpgt[a-z0-9_]+|pmovmskb|movdqa|movdqu|por|pand|pxor)\\b"
)

count_re <- function(x, pat) {
  sum(lengths(regmatches(x, gregexpr(pat, x, perl = TRUE))))
}

relative_path <- function(path) {
  prefix <- paste0(root, "/")
  if (startsWith(path, prefix)) substring(path, nchar(prefix) + 1L) else path
}

summarise_one <- function(path) {
  dis <- system2(objdump, c("-d", "-M", "intel", "--no-show-raw-insn", path), stdout = TRUE, stderr = TRUE)
  data.frame(
    file = relative_path(path),
    size_kb = round(file.info(path)$size / 1024, 1),
    xmm = count_re(dis, patterns$xmm),
    ymm = count_re(dis, patterns$ymm),
    zmm = count_re(dis, patterns$zmm),
    avx_v_mnemonics = count_re(dis, patterns$avx_v_mnemonics),
    avx2_byte_ops = count_re(dis, patterns$avx2_byte_ops),
    sse4ish = count_re(dis, patterns$sse4ish),
    sse2_ssse3ish = count_re(dis, patterns$sse2_ssse3ish),
    stringsAsFactors = FALSE
  )
}

out <- do.call(rbind, lapply(files, summarise_one))
out <- out[order(out$file), , drop = FALSE]
print(out, row.names = FALSE)

cat("\nNotes:\n")
cat("- ymm > 0 is direct evidence of 256-bit AVX/AVX2 code in that object/library.\n")
cat("- avx_v_mnemonics > 0 with ymm == 0 usually means 128-bit VEX/SIMDe AVX code.\n")
cat("- scalar here means portable SIMDe/native-disabled fallback, not no-vector machine code.\n")
