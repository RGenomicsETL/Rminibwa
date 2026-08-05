# Return minibwa index contigs

Names are returned as raw byte vectors to avoid forced string
materialization.

## Usage

``` r
mb_index_contigs(index)
```

## Arguments

- index:

  Index handle returned by
  [`mb_index_load()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_index_load.md).

## Value

A list with raw-vector `name` entries and numeric `length` values.
