# Load a native minibwa index

The index files are memory-mapped read-only and shared, as with
`minibwa map --mmap`, so processes that load the same index share one
page-cache copy. The `.l2b` and `.mbw` files must stay unchanged while a
handle is alive.

## Usage

``` r
mb_index_load(prefix, meth = FALSE)
```

## Arguments

- prefix:

  Index prefix.

- meth:

  Load the methylation BWT index.

## Value

An external pointer handle to a native minibwa index.
