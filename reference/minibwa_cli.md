# Run the minibwa CLI

Low-level helper for invoking arbitrary `minibwa` commands from R.
Prefer
[`minibwa_index()`](https://rgenomicsetl.github.io/Rminibwa/reference/minibwa_index.md)
and
[`minibwa_map()`](https://rgenomicsetl.github.io/Rminibwa/reference/minibwa_map.md)
for common workflows.

## Usage

``` r
minibwa_cli(args = character(), stdout = TRUE, path = minibwa_path())
```

## Arguments

- args:

  Character vector of command-line arguments passed to `minibwa`.

- stdout:

  Passed to [`system2()`](https://rdrr.io/r/base/system2.html). Use
  `TRUE` to capture standard output, `FALSE` to inherit it, or a file
  path to redirect it.

- path:

  Executable name or path.

## Value

If `stdout = TRUE`, a character vector of captured output; otherwise an
invisible process status.
