# Map raw sequence bytes with native minibwa

`mb_map()` is pipe-friendly: the sequence bytes are the first argument.
The result is a native alignment batch external pointer. Use
[`mb_align_col()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_align_n.md)
for ALTREP column views or pass the batch to downstream C consumers.

## Usage

``` r
mb_map(x, index, opt = mb_opts(), name = NULL, meth = c("none", "c2t", "g2a"))
```

## Arguments

- x:

  Raw vector of query sequence bytes.

- index:

  Index handle returned by
  [`mb_index_load()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_index_load.md).

- opt:

  Options list from
  [`mb_opts()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_opts.md).

- name:

  Optional read name as raw bytes. Character names are accepted for
  convenience but raw bytes are preferred.

- meth:

  Methylation code: `"none"`, `"c2t"`, or `"g2a"`.

## Value

A native alignment batch external pointer.

## Examples

``` r
# See vignettes for full index-backed examples.
```
