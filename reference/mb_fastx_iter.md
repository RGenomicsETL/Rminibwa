# Iterate FASTA/FASTQ batches without R sequence materialization

`mb_fastx_iter()` opens one FASTA/FASTQ file, or two mate files, using
the vendored minibwa reader. `mb_fastx_next()` returns a native FASTX
batch, and `mb_map_fastx_batch()` maps that batch directly from native
sequence buffers.

## Usage

``` r
mb_fastx_iter(
  path,
  paired = c("auto", "none", "by_name", "two_file"),
  batch_bases = 100000000L,
  max_batch_bases = batch_bases,
  with_qual = FALSE,
  with_comment = FALSE
)

mb_fastx_next(iter)

mb_fastx_n(batch)

mb_map_fastx_batch(batch, index, opt = mb_opts())
```

## Arguments

- path:

  One FASTA/FASTQ path, or two paths for `paired = "two_file"`.

- paired:

  Pairing mode.

- batch_bases:

  Approximate number of sequence bases to read per batch.

- max_batch_bases:

  Maximum bases to read when extending a batch to keep a short paired
  fragment together.

- with_qual, with_comment:

  Preserve qualities/comments in the native batch.

- iter:

  Native FASTX iterator returned by `mb_fastx_iter()`.

- batch:

  Native FASTX batch returned by `mb_fastx_next()`.

- index:

  Index handle returned by
  [`mb_index_load()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_index_load.md).

- opt:

  Options list from
  [`mb_opts()`](https://rgenomicsetl.github.io/Rminibwa/reference/mb_opts.md).

## Value

`mb_fastx_iter()` returns a native iterator. `mb_fastx_next()` returns a
native FASTX batch or `NULL` at EOF. `mb_fastx_n()` returns the number
of records in a FASTX batch. `mb_map_fastx_batch()` returns a native
alignment batch.

## Details

Pairing modes follow the CLI reader shape: `"none"` treats each record
as a fragment, `"by_name"` groups adjacent records in one file with
matching query names, and `"two_file"` reads one record from each of two
files per fragment. `"auto"` selects `"two_file"` for two paths and
`"none"` for one path.
