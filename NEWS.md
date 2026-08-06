# Rminibwa 0.6.0-0.0.1.9000

- Make `Rduckhts` the required htslib provider. Configure now validates and
  consumes its exact installed linking receipt on Unix and Windows, and
  `rminibwa_htslib_info()` verifies the linked version and build identity at
  runtime. This provides one verified htslib identity for future native
  alignment-format work without selecting its output architecture. Browser-wasm
  builds now omit the direct adapter explicitly because `rwasm` exposes only
  Rduckhts's host-native library to downstream configure scripts.
- Add a Win32 file-mapping implementation for vendored `minibwa` so Windows
  and Rtools builds no longer require the unavailable POSIX `sys/mman.h`.
- Add a versioned, lossless native FASTQ query-group stream for downstream
  native record consumers. The installed C API exposes bounded one-template views,
  typed CIGAR/tag data, header facts, cooperative cancellation, explicit
  ownership/lifetime rules, exact read-group preservation, and reason-coded
  paired FASTQ validation without a SAM-text or per-record-R-callback path.
- Update vendored `minibwa` to the exact upstream v0.6 release commit
  `ebc59eaff045898c6a9daed4689302547d1326cb` (`MB_VERSION` `0.6-r416`;
  archive SHA256
  `f84b42299923ccfcab19f46bfdb7fdb4562d87c95cb0fc591823458474870474`).
  This includes the paired-end mapQ revision, memory-mapped index loading,
  `--outs`, BS-seq API support, and the corrected `-I` fixed insert-size
  distribution behavior from upstream v0.5 and v0.6.

# Rminibwa 0.3.0-0.0.1.9000

- Add a MinGW/Rtools-only `kthread.c` dead-return guard after `pthread_exit()`
  to avoid `-Wreturn-type` install warnings on Windows.
- Make `kseq.h`'s `kroundup64()` width-safe on 32-bit targets such as wasm32,
  avoiding Emscripten `-Wshift-count-overflow` diagnostics.
- Add `mb_map_batch()` for native columnar mapping of many query sequences in
  one call, plus `mb_map_pair_batch()` for explicit two-batch paired-end input.
- Add native FASTX iterators and `mb_map_fastx_batch()` so file-backed batches
  can be mapped without materializing sequence strings in R.
- Add a read-level sidecar to alignment batches with read IDs, query lengths,
  fragment/mate fields, and per-read hit offsets/counts, exposed through ALTREP
  read columns and the C API.
- Add a concise print method for native alignment batches.
- Return `simd_info()` as a classed `rminibwa_simd_info` object with a concise
  print method for dispatch diagnostics.
- Fold in upstream `minibwa` v0.3 / `MB_VERSION` `0.3-r391` at commit
  `a679d17cb822c6cca67ab78c439f9c9826f1b07d`.
- Switch package versions to the `<minibwa-version>-<Rminibwa-version>`
  scheme; development builds now use `0.3.0-0.0.1.9000`.
- Add a MinGW/Rtools-only `kalloc.c` format-string guard for `size_t`
  diagnostics while leaving the non-MinGW upstream code path unchanged.

# Rminibwa 0.0.0.9000

- Bootstrap `Rminibwa` as a GPL (>= 2) R package wrapping pinned upstream
  `lh3/minibwa` C sources.
- Add native bindings for index build/load, raw-vector query mapping, native
  external-pointer alignment batches, ALTREP column views, and packed CIGAR
  accessors.
- Install `inst/include/Rminibwa.h` and register C-callable accessors so
  downstream packages can consume alignment batches directly from C.
- Add staged KSW SIMD dispatch following the `RsimdDispatch` pattern: portable
  scalar/SIMDe fallback, SSE4, and AVX2 backends are compiled as separate
  objects and selected at runtime without global ISA compiler flags.
- Add AVX2 dual-gap `extd2` support from the minibwa binding work and keep it
  as an optional runtime-dispatched backend.
- Vendor minibwa at commit `f0fd108456d459e5da3755f29c8700dc7f6fe0df` with a
  patch queue and provenance metadata.
- Add Rtinycc inline downstream-C examples in the README, vignette, and tests.
- Build and install a package-provided `minibwa` executable from vendored
  sources; CLI wrappers now use it by default.
- Add developer-only benchmarks comparing internal staged backends and optional
  external Python/Rust bindings from an explicit local checkout.
- Add `make asm` / `tools/check-assembly.R` to audit generated SIMD instruction
  families in staged objects and shared libraries.
- Add pkgdown configuration and GitHub Actions workflows for standard R CMD
  check, Fedora/R-devel CRAN-style check, and GitHub Pages deployment.
