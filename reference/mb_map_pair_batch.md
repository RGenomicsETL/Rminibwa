# Map paired query sequence batches with native minibwa

`mb_map_pair_batch()` is the explicit two-batch paired-end interface.
`r1[i]` and `r2[i]` are flattened as adjacent records before mapping,
matching the two-file CLI layout. The read sidecar records `fragment_id`
and `mate`.

## Usage

``` r
mb_map_pair_batch(r1, r2, index, opt = mb_opts(), name1 = NULL, name2 = NULL)
```

## Arguments

- r1, r2:

  Character vectors or lists of raw vectors / character scalars. They
  must have the same non-zero length.

- index:

  Index handle returned by
  [`mb_index_load()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_index_load.md).

- opt:

  Options list from
  [`mb_opts()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_opts.md).

- name1, name2:

  Optional read-name vectors for `r1` and `r2`.

## Value

A native alignment batch external pointer.
