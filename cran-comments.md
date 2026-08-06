## Test environments

- local Ubuntu 24.04.3 LTS, R 4.6.0
- GitHub Actions ubuntu-latest (devel), macos-latest (release, oldrel),
  windows-latest (release)
- Fedora/R-devel gcc16 container via `.github/workflows/fedora-cran-check.yaml`

## R CMD check results

- Local `R CMD check --no-manual`: 0 errors | 0 warnings | 0 notes
- Local `R CMD check --as-cran --no-manual`: 0 errors | 0 warnings | 2 notes

Expected development notes:

1. New submission / development version `0.3.0-0.0.1.9000`.
2. GitHub URLs may be reported unavailable until the public repository and
   GitHub Pages site are live.
3. The non-portable `-mno-omit-leaf-frame-pointer` flag is injected by the
   local R toolchain, not by this package. Rminibwa does not add global ISA
   flags such as `-mavx2`; ISA-specific objects are staged separately at
   configure time.

## Additional notes

- Rminibwa vendors a pinned upstream `minibwa` C source snapshot and records
  provenance in `src/vendor/minibwa/RMINIBWA_VENDOR.dcf`.
- Examples, tests, and vignettes use temporary files and clean up after
  themselves.
- Tests use at most two cores; native examples request one indexing thread.
- `Rduckhts` is required from `https://rgenomicsetl.r-universe.dev`; on native
  platforms it is Rminibwa's sole provider of the exact htslib headers and
  shared/static library. Browser-wasm builds explicitly omit the direct adapter
  because `rwasm` exposes only host-native dependency link artifacts during
  downstream configure.
- `RsimdDispatch` is required from `https://sounkou-bioinfo.r-universe.dev`
  until the needed development version is on CRAN.
