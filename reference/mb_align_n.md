# Inspect native minibwa alignment batches

`mb_align_col()` returns ALTREP views over native alignment columns.
These views do not copy columns unless R forces materialization.
`mb_align_read_col()` returns ALTREP views over the read-level sidecar
with one row per input read, including reads without hits. C consumers
should prefer the installed `Rminibwa.h` API.

## Usage

``` r
mb_align_n(x)

mb_align_read_n(x)

mb_align_col(x, name)

mb_align_read_col(x, name)

mb_align_cigar_words(x)
```

## Arguments

- x:

  Native alignment batch returned by
  [`mb_map()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_map.md)
  or
  [`mb_map_batch()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_map_batch.md).

- name:

  Column name. Alignment columns include `read_id`, `tid`, `qlen`, `qs`,
  `qe`, `tlen`, `ts`, `te`, `strand`, `mapq`, `score`, `matches`,
  `blen`, `flags`, `n_sub`, `cigar_offset`, and `cigar_n`. Read-sidecar
  columns include `read_id`, `length`, `fragment_id`, `mate`,
  `hit_offset`, and `hit_count`.

## Value

`mb_align_n()` returns the number of alignment records.
`mb_align_read_n()` returns the number of input reads represented in the
batch. `mb_align_col()` and `mb_align_read_col()` return ALTREP integer
or real vectors. `mb_align_cigar_words()` returns a raw debug copy of
packed minibwa CIGAR words.
