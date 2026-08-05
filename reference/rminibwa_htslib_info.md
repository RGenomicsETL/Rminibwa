# Inspect the htslib Supplied by Rduckhts

Rminibwa links to the exact installed htslib contract exported by
Rduckhts. This function compares the version reached through Rminibwa's
native library with Rduckhts's validated build receipt.

## Usage

``` r
rminibwa_htslib_info(validate = TRUE)
```

## Arguments

- validate:

  Whether Rduckhts should validate its installed headers, library,
  receipt, and loaded DuckHTS runtime.

## Value

An object of class `rminibwa_htslib_info` containing the provider,
linked and receipt versions, source and build identities, link mode,
runtime feature bits/string, and compiled htslib features.

## Examples

``` r
rminibwa_htslib_info()
#> <rminibwa_htslib_info>
#>   provider: Rduckhts
#>   version:  1.24
#>   build:    Rduckhts-1.5.1.9000-0.1.5-linux_amd64
#>   link:     shared
```
