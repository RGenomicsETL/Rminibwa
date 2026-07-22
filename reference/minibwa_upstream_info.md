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
#> [1] "0.4-r411-dirty"
#> 
#> $Commit
#> [1] "84019dc454013134570479c50355741ed8318a8e"
#> 
```
