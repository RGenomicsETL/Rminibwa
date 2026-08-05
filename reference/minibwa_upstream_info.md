# Report the pinned upstream minibwa source

Rminibwa pins a specific upstream `minibwa` commit for native-library
work. `minibwa_upstream_info()` reports the installed copy of that
provenance metadata.

## Usage

``` r
minibwa_upstream_info()
```

## Value

A named list with fields such as `Component`, `Version`, `Repository`,
`Commit`, and `PatchDirectory`.

## Examples

``` r
minibwa_upstream_info()[c("Version", "Commit")]
#> $Version
#> [1] "0.6-r416"
#> 
#> $Commit
#> [1] "ebc59eaff045898c6a9daed4689302547d1326cb"
#> 
```
