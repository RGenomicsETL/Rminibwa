# Open a lossless native FASTQ query-group stream

Opens a one-template-at-a-time, bounded-memory native stream for a
downstream C/C++/Rcpp BAM producer. Each `mb_query_stream_next()` result
is one complete normalized QNAME group and remains valid only until the
next call on the stream or cancellation. The normal producer path
neither creates SAM text nor calls R once per alignment record;
downstream native consumers should use the versioned API in
`Rminibwa.h`.

## Usage

``` r
mb_query_stream(
  path,
  index,
  opt = mb_opts(),
  mode,
  threads,
  read_group = NULL,
  max_seen_qnames = 1000000L
)

mb_query_stream_next(stream)

mb_query_stream_cancel(stream)

mb_query_stream_error(stream)

mb_query_group_n_reads(group)

mb_query_group_n_records(group)

mb_query_group_name(group)

mb_query_group_input_order(group)
```

## Arguments

- path:

  One FASTQ path for `mode = "single"`, or two ordered mate FASTQ paths
  for `mode = "paired"`.

- index:

  A native minibwa index returned by
  [`mb_index_load()`](https://sounkou-bioinfo.github.io/Rminibwa/reference/mb_index_load.md).

- opt:

  Mapping options from
  [`mb_opts()`](https://sounkou-bioinfo.github.io/Rminibwa/reference/mb_opts.md).
  Its `threads` entry is replaced by `threads`; its paired flag is set
  from `mode`.

- mode:

  Explicit "single" or "paired" input mode.

- threads:

  Positive total thread budget owned by the caller.

- read_group:

  `NULL` or one exact `@RG` record with a non-empty `ID:`.

- max_seen_qnames:

  Positive maximum number of normalized QNAMEs tracked for exact
  repeated-name detection.

- stream:

  A native stream returned by `mb_query_stream()`.

- group:

  A native query group returned by `mb_query_stream_next()`.

## Value

`mb_query_stream()` returns a native query stream.
`mb_query_stream_next()` returns a native query group or `NULL` at EOF
or after cancellation. Group helper functions expose only debug
accounting; use the installed C API for record views.

## Details

`mode` and `threads` are deliberately required. `threads` is the
complete caller-owned minibwa thread budget: the stream creates no
additional mapping worker pool. `read_group` is either `NULL` or an
exact literal `@RG` record containing an `ID:` tag. It is retained
byte-for-byte; library and sample metadata are never inferred. The
native header view declares `SO:unsorted` and `GO:query`: groups are
contiguous input templates, not lexicographically query-name sorted.

Exact repeated-QNAME validation uses a caller-bounded name tracker. If
the limit is reached, the stream fails with a reason code rather than
emitting an ambiguous later group. Consumers must treat any stream error
as an incomplete downstream output.
