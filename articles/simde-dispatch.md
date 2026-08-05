# SIMDe Dispatch Design

Rminibwa follows the `RsimdDispatch`/`SIMDe` downstream-package pattern
for the SIMD-sensitive minibwa KSW kernels:

- `configure` probes compiler support;
- backend objects are staged under `src/rmb-ksw/`;
- `src/Makevars.in` links those staged objects through `PKG_LIBS`;
- ordinary minibwa sources are compiled once with baseline package
  flags;
- runtime selection switches between compiled and CPU-supported
  backends.

This avoids putting ISA flags such as `-mavx2` into global
`PKG_CPPFLAGS` or `PKG_CFLAGS`.

## Boundary

Dispatch starts at the KSW functions called by minibwa:

- `ksw_extz2_sse()`;
- `ksw_extd2_sse()`;
- `ksw_ll_qinit()` and the `ksw_ll_*` helpers.

The rest of minibwa stays baseline-compiled once:

- index loading;
- BWT and seeding;
- chaining;
- pairing;
- formatting;
- R external-pointer glue.

## Backends

Rminibwa currently exposes:

- `scalar`: portable SIMDe fallback compiled with native intrinsics
  disabled and no ISA flags;
- `sse4`: native SSE4.1/SIMDe-alias build of the KSW files;
- `avx2`: native AVX2 build, including the widened dual-gap `extd2`
  kernel.

The `avx2` backend uses the wide `ksw2_extd2_wide.c` patch for the
dominant dual-gap gap-filling path and uses AVX2-compiled KSW objects
for the remaining KSW entry points.

## R surface

``` r

library(Rminibwa)
simd_info()
#> <rminibwa SIMD dispatch>
#>   selected:  avx2
#>   requested: auto
#>   mode:      rminibwa-ksw-staged
#>   compiled:  scalar, sse4, avx2
#>   available: scalar, sse4, avx2
#>   cpu:       <none>
#>   target:    x86_64
```

Select a backend explicitly for diagnostics or benchmarks:

``` r

old <- simd_backend()
simd_set_backend("scalar")
simd_backend()
#> [1] "scalar"
simd_set_backend("auto")
```

[`simd_set_backend()`](https://rgenomicsetl.github.io/Rminibwa/reference/simd_set_backend.md)
errors if the requested backend was not compiled or is not supported by
the current CPU/runtime.

## Validation

For every available backend, Rminibwa runs the same native mapping
fixture in `tinytest` and checks that the native alignment batch and
ALTREP column accessors remain valid. The README `make rdm` path
additionally benchmarks:

1.  internal Rminibwa-only `scalar` vs `sse4` vs `avx2`; and
2.  Rminibwa AVX2 vs locally built Python/Rust bindings compiled with
    native AVX2 codegen.

Backend timing must use a workload that actually reaches KSW. Very short
exact matches can be resolved by seeding/chaining and ungapped fast
paths without a single `ksw_ext*` or `ksw_ll*` call, so apparent
`scalar`/`sse4`/`avx2` differences on those examples are just end-to-end
mapper noise. The developer README benchmark uses an indel-mutated
random read and records internal KSW call counters before timing
count-only mapping.
