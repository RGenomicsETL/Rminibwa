# Map a batch of query sequences with native minibwa

`mb_map_batch()` maps many reads in one native call and returns the same
columnar alignment-batch object as
[`mb_map()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_map.md).
Input may be a character vector of sequence bytes or a list whose
elements are raw vectors or character scalars. Unlike the CLI reader,
this direct batch API defaults to unpaired mapping unless
`opt = mb_opts(paired = TRUE)`. In paired mode, order records as
`R1, R2, R1, R2, ...`; this call pairs adjacent records and does not
group FASTQ records by name for you.

## Usage

``` r
mb_map_batch(x, index, opt = mb_opts(), name = NULL)
```

## Arguments

- x:

  Character vector or list of raw vectors / character scalars.

- index:

  Index handle returned by
  [`mb_index_load()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_index_load.md).

- opt:

  Options list from
  [`mb_opts()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_opts.md).

- name:

  Optional character vector of read names, or a list of raw vectors /
  character scalars. If supplied, it must have the same length as `x`.

## Value

A native alignment batch external pointer.
