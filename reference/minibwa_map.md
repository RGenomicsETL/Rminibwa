# Map reads through the minibwa CLI

Map reads through the minibwa CLI

## Usage

``` r
minibwa_map(
  index,
  reads,
  output = NULL,
  format = c("sam", "paf"),
  threads = NULL,
  meth = FALSE,
  hic = FALSE,
  extra_args = character(),
  path = minibwa_path()
)
```

## Arguments

- index:

  Index prefix produced by
  [`minibwa_index()`](https://rgenomicsetl.github.io/Rminibwa/reference/minibwa_index.md)
  or the minibwa CLI.

- reads:

  One or more FASTA/FASTQ input paths. The common cases are one
  single-end file or two paired-end files.

- output:

  Optional output file. When `NULL`, standard output is captured and
  returned as a character vector.

- format:

  Output format: `"sam"` by default, or `"paf"` to pass `-f`.

- threads:

  Optional number of worker threads.

- meth:

  If `TRUE`, pass `--meth`.

- hic:

  If `TRUE`, pass `--hic`.

- extra_args:

  Additional CLI arguments inserted before positional arguments.

- path:

  Executable name or path.

## Value

If `output = NULL`, captured alignment records as a character vector;
otherwise the output path, invisibly.
