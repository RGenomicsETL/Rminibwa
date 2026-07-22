#ifndef RMINIBWA_H
#define RMINIBWA_H

#include <stddef.h>
#include <stdint.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Alignment-batch API ----------------------------------------------------- */

typedef struct RmbAlignBatch RmbAlignBatch;
typedef struct RmbIndex RmbIndex;

/* Borrowed pointers. They remain valid while the owning R external pointer is
 * protected and alive. Downstream packages should not free or mutate them. */
typedef const RmbAlignBatch *(*Rminibwa_align_from_sexp_fn)(SEXP x);
typedef size_t (*Rminibwa_align_n_fn)(const RmbAlignBatch *x);
typedef size_t (*Rminibwa_align_n_read_fn)(const RmbAlignBatch *x);
typedef const int32_t *(*Rminibwa_align_read_i32_col_fn)(const RmbAlignBatch *x, const char *name);
typedef const int32_t *(*Rminibwa_align_i32_col_fn)(const RmbAlignBatch *x, const char *name);
typedef const int64_t *(*Rminibwa_align_i64_col_fn)(const RmbAlignBatch *x, const char *name);
typedef const uint32_t *(*Rminibwa_align_cigar_words_fn)(const RmbAlignBatch *x, size_t *n_words);
typedef const int32_t *(*Rminibwa_align_cigar_i32_col_fn)(const RmbAlignBatch *x, const char *name);

/* Query-group stream API --------------------------------------------------
 *
 * This is a versioned POD/view API.  It deliberately does not expose htslib
 * types, SAM text, or an R object per alignment record.  All pointers returned
 * in a group view are borrowed and valid until the next Rminibwa_stream_next(),
 * Rminibwa_stream_cancel(), or destruction of the stream.  A consumer must
 * finish, pause, or copy a group before asking for the next one.
 */

#define RMINIBWA_STREAM_ABI_VERSION UINT32_C(1)
#define RMINIBWA_TAG(a, b) ((uint16_t) ((((uint16_t) (uint8_t) (a)) << 8) | (uint8_t) (b)))

typedef struct RmbQueryStream RmbQueryStream;
typedef struct RmbQueryGroup RmbQueryGroup;

enum Rminibwa_stream_status {
    RMINIBWA_STREAM_OK = 0,
    RMINIBWA_STREAM_EOF = 1,
    RMINIBWA_STREAM_CANCELLED = 2,
    RMINIBWA_STREAM_ERROR = -1,
    RMINIBWA_STREAM_ABI_MISMATCH = -2
};

enum Rminibwa_stream_error_code {
    RMINIBWA_STREAM_ERROR_NONE = 0,
    RMINIBWA_STREAM_ERROR_CANCELLED = 1,
    RMINIBWA_STREAM_ERROR_FASTQ_MALFORMED = 2,
    RMINIBWA_STREAM_ERROR_FASTQ_MISSING_QUALITY = 3,
    RMINIBWA_STREAM_ERROR_FASTQ_MATE_COUNT = 4,
    RMINIBWA_STREAM_ERROR_FASTQ_NAME_MISMATCH = 5,
    RMINIBWA_STREAM_ERROR_FASTQ_MATE_ORDER = 6,
    RMINIBWA_STREAM_ERROR_QNAME_REPEATED = 7,
    RMINIBWA_STREAM_ERROR_QNAME_TRACKER_LIMIT = 8,
    RMINIBWA_STREAM_ERROR_NAME_TOO_LONG = 9,
    RMINIBWA_STREAM_ERROR_MAPPING = 10,
    RMINIBWA_STREAM_ERROR_NO_MEMORY = 11,
    RMINIBWA_STREAM_ERROR_INVALID_READ_GROUP = 12,
    RMINIBWA_STREAM_ERROR_INVALID_ARGUMENT = 13
};

enum Rminibwa_tag_type {
    RMINIBWA_TAG_I32 = 1,
    RMINIBWA_TAG_STRING = 2,
    RMINIBWA_TAG_CIGAR = 3
};

enum Rminibwa_sequence_mode {
    RMINIBWA_SEQUENCE_ABSENT = 0,
    RMINIBWA_SEQUENCE_FULL = 1,
    RMINIBWA_SEQUENCE_SEGMENT = 2
};

enum Rminibwa_sort_order {
    RMINIBWA_SORT_UNSORTED = 0,
    RMINIBWA_SORT_QUERYNAME = 1,
    RMINIBWA_SORT_COORDINATE = 2
};

enum Rminibwa_group_order {
    RMINIBWA_GROUP_NONE = 0,
    RMINIBWA_GROUP_QUERY = 1
};

typedef struct {
    uint16_t tag;             /* RMINIBWA_TAG('N', 'M'), for example */
    uint8_t type;             /* enum Rminibwa_tag_type */
    uint8_t reserved;
    int32_t i32;              /* valid for RMINIBWA_TAG_I32 */
    const char *string;       /* byte string; not necessarily NUL-terminated */
    size_t string_length;     /* valid for RMINIBWA_TAG_STRING */
    const uint32_t *cigar;    /* valid for RMINIBWA_TAG_CIGAR */
    uint32_t cigar_n;
} Rminibwa_tag_view;

typedef struct {
    uint64_t input_order;     /* zero-based template order from the input */
    uint32_t read_number;     /* 0 for single-end, otherwise 1 or 2 */
    uint32_t sequence_length;
    const char *sequence;     /* original FASTQ sequence bytes */
    const char *qualities;    /* original FASTQ qualities, same length */
} Rminibwa_read_view;

typedef struct {
    uint64_t input_order;
    uint32_t record_order;    /* stable, zero-based order within its group */
    uint32_t read_index;      /* index into Rminibwa_query_group_view_t.reads */
    int32_t reference_id;     /* -1 when this record is unmapped */
    int64_t position;         /* zero-based; -1 when unmapped */
    int32_t mate_reference_id;
    int64_t mate_position;
    int64_t template_length;
    uint16_t flag;            /* SAM/BAM flag bits */
    uint8_t mapq;
    uint8_t sequence_mode;    /* enum Rminibwa_sequence_mode */
    uint32_t sequence_offset; /* offset into the source read */
    uint32_t sequence_length; /* source span used by this SAM/BAM record */
    uint8_t sequence_reverse_complement;
    uint8_t qualities_reverse;
    uint16_t reserved;
    const uint32_t *cigar;    /* BAM packed CIGAR, including terminal clips */
    uint32_t cigar_n;
    const uint32_t *mate_cigar; /* primary mate CIGAR, for a typed MC tag */
    uint32_t mate_cigar_n;
    int32_t mate_mapq;        /* -1 when no mapped primary mate */
    int32_t alignment_score;  /* AS:i, or 0 when not available */
    int32_t edit_distance;    /* NM:i, or 0 when not available */
    int32_t chain_score;
    int32_t chain_subscore;
    const Rminibwa_tag_view *tags;
    uint32_t n_tags;
} Rminibwa_record_view;

typedef struct {
    uint32_t abi_version;
    uint32_t reserved;
    size_t struct_size;
    uint64_t input_order;
    const char *query_name;   /* normalized QNAME bytes */
    size_t query_name_length;
    const char *read_group_id; /* NULL when no caller-supplied @RG record */
    size_t read_group_id_length;
    const Rminibwa_read_view *reads;
    uint32_t n_reads;
    const Rminibwa_record_view *records;
    uint32_t n_records;
} Rminibwa_query_group_view_t;

typedef struct {
    const char *name;
    size_t name_length;
    int64_t length;
} Rminibwa_contig_view;

typedef struct {
    uint32_t abi_version;
    uint32_t sort_order;      /* enum Rminibwa_sort_order */
    uint32_t group_order;     /* enum Rminibwa_group_order */
    size_t struct_size;
    const Rminibwa_contig_view *contigs;
    uint32_t n_contigs;
    const char *read_group_record; /* exact caller-supplied @RG record */
    size_t read_group_record_length;
    const char *read_group_id;
    size_t read_group_id_length;
    const char *program_id;   /* "minibwa" */
    const char *program_name; /* "minibwa" */
    const char *program_version;
} Rminibwa_header_view;

typedef uint32_t (*Rminibwa_stream_abi_version_fn)(void);
typedef RmbQueryStream *(*Rminibwa_stream_from_sexp_fn)(SEXP x);
typedef const RmbQueryGroup *(*Rminibwa_query_group_from_sexp_fn)(SEXP x);
typedef int (*Rminibwa_stream_next_fn)(RmbQueryStream *stream, const RmbQueryGroup **group);
typedef int (*Rminibwa_stream_cancel_fn)(RmbQueryStream *stream);
typedef int (*Rminibwa_stream_header_fn)(const RmbQueryStream *stream, Rminibwa_header_view *out);
typedef int (*Rminibwa_query_group_view_fn)(const RmbQueryGroup *group, Rminibwa_query_group_view_t *out);
typedef int (*Rminibwa_stream_error_code_fn)(const RmbQueryStream *stream);
typedef const char *(*Rminibwa_stream_error_message_fn)(const RmbQueryStream *stream);

#ifdef RMINIBWA_BUILDING

const RmbAlignBatch *Rminibwa_align_from_sexp(SEXP x);
size_t Rminibwa_align_n(const RmbAlignBatch *x);
size_t Rminibwa_align_n_read(const RmbAlignBatch *x);
const int32_t *Rminibwa_align_read_i32_col(const RmbAlignBatch *x, const char *name);
const int32_t *Rminibwa_align_i32_col(const RmbAlignBatch *x, const char *name);
const int64_t *Rminibwa_align_i64_col(const RmbAlignBatch *x, const char *name);
const uint32_t *Rminibwa_align_cigar_words(const RmbAlignBatch *x, size_t *n_words);
const int32_t *Rminibwa_align_cigar_i32_col(const RmbAlignBatch *x, const char *name);

uint32_t Rminibwa_stream_abi_version(void);
RmbQueryStream *Rminibwa_stream_from_sexp(SEXP x);
const RmbQueryGroup *Rminibwa_query_group_from_sexp(SEXP x);
int Rminibwa_stream_next(RmbQueryStream *stream, const RmbQueryGroup **group);
int Rminibwa_stream_cancel(RmbQueryStream *stream);
int Rminibwa_stream_header(const RmbQueryStream *stream, Rminibwa_header_view *out);
int Rminibwa_query_group_view(const RmbQueryGroup *group, Rminibwa_query_group_view_t *out);
int Rminibwa_stream_error_code(const RmbQueryStream *stream);
const char *Rminibwa_stream_error_message(const RmbQueryStream *stream);

#else

static inline Rminibwa_align_from_sexp_fn Rminibwa_get_align_from_sexp(void)
{ return (Rminibwa_align_from_sexp_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_from_sexp"); }
static inline Rminibwa_align_n_fn Rminibwa_get_align_n(void)
{ return (Rminibwa_align_n_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_n"); }
static inline Rminibwa_align_n_read_fn Rminibwa_get_align_n_read(void)
{ return (Rminibwa_align_n_read_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_n_read"); }
static inline Rminibwa_align_read_i32_col_fn Rminibwa_get_align_read_i32_col(void)
{ return (Rminibwa_align_read_i32_col_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_read_i32_col"); }
static inline Rminibwa_align_i32_col_fn Rminibwa_get_align_i32_col(void)
{ return (Rminibwa_align_i32_col_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_i32_col"); }
static inline Rminibwa_align_i64_col_fn Rminibwa_get_align_i64_col(void)
{ return (Rminibwa_align_i64_col_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_i64_col"); }
static inline Rminibwa_align_cigar_words_fn Rminibwa_get_align_cigar_words(void)
{ return (Rminibwa_align_cigar_words_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_cigar_words"); }
static inline Rminibwa_align_cigar_i32_col_fn Rminibwa_get_align_cigar_i32_col(void)
{ return (Rminibwa_align_cigar_i32_col_fn) R_GetCCallable("Rminibwa", "Rminibwa_align_cigar_i32_col"); }
static inline Rminibwa_stream_abi_version_fn Rminibwa_get_stream_abi_version(void)
{ return (Rminibwa_stream_abi_version_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_abi_version"); }
static inline Rminibwa_stream_from_sexp_fn Rminibwa_get_stream_from_sexp(void)
{ return (Rminibwa_stream_from_sexp_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_from_sexp"); }
static inline Rminibwa_query_group_from_sexp_fn Rminibwa_get_query_group_from_sexp(void)
{ return (Rminibwa_query_group_from_sexp_fn) R_GetCCallable("Rminibwa", "Rminibwa_query_group_from_sexp"); }
static inline Rminibwa_stream_next_fn Rminibwa_get_stream_next(void)
{ return (Rminibwa_stream_next_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_next"); }
static inline Rminibwa_stream_cancel_fn Rminibwa_get_stream_cancel(void)
{ return (Rminibwa_stream_cancel_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_cancel"); }
static inline Rminibwa_stream_header_fn Rminibwa_get_stream_header(void)
{ return (Rminibwa_stream_header_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_header"); }
static inline Rminibwa_query_group_view_fn Rminibwa_get_query_group_view(void)
{ return (Rminibwa_query_group_view_fn) R_GetCCallable("Rminibwa", "Rminibwa_query_group_view"); }
static inline Rminibwa_stream_error_code_fn Rminibwa_get_stream_error_code(void)
{ return (Rminibwa_stream_error_code_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_error_code"); }
static inline Rminibwa_stream_error_message_fn Rminibwa_get_stream_error_message(void)
{ return (Rminibwa_stream_error_message_fn) R_GetCCallable("Rminibwa", "Rminibwa_stream_error_message"); }

static inline const RmbAlignBatch *Rminibwa_align_from_sexp(SEXP x)
{ return Rminibwa_get_align_from_sexp()(x); }
static inline size_t Rminibwa_align_n(const RmbAlignBatch *x)
{ return Rminibwa_get_align_n()(x); }
static inline size_t Rminibwa_align_n_read(const RmbAlignBatch *x)
{ return Rminibwa_get_align_n_read()(x); }
static inline const int32_t *Rminibwa_align_read_i32_col(const RmbAlignBatch *x, const char *name)
{ return Rminibwa_get_align_read_i32_col()(x, name); }
static inline const int32_t *Rminibwa_align_i32_col(const RmbAlignBatch *x, const char *name)
{ return Rminibwa_get_align_i32_col()(x, name); }
static inline const int64_t *Rminibwa_align_i64_col(const RmbAlignBatch *x, const char *name)
{ return Rminibwa_get_align_i64_col()(x, name); }
static inline const uint32_t *Rminibwa_align_cigar_words(const RmbAlignBatch *x, size_t *n_words)
{ return Rminibwa_get_align_cigar_words()(x, n_words); }
static inline const int32_t *Rminibwa_align_cigar_i32_col(const RmbAlignBatch *x, const char *name)
{ return Rminibwa_get_align_cigar_i32_col()(x, name); }
static inline uint32_t Rminibwa_stream_abi_version(void)
{ return Rminibwa_get_stream_abi_version()(); }
static inline RmbQueryStream *Rminibwa_stream_from_sexp(SEXP x)
{ return Rminibwa_get_stream_from_sexp()(x); }
static inline const RmbQueryGroup *Rminibwa_query_group_from_sexp(SEXP x)
{ return Rminibwa_get_query_group_from_sexp()(x); }
static inline int Rminibwa_stream_next(RmbQueryStream *stream, const RmbQueryGroup **group)
{ return Rminibwa_get_stream_next()(stream, group); }
static inline int Rminibwa_stream_cancel(RmbQueryStream *stream)
{ return Rminibwa_get_stream_cancel()(stream); }
static inline int Rminibwa_stream_header(const RmbQueryStream *stream, Rminibwa_header_view *out)
{ return Rminibwa_get_stream_header()(stream, out); }
static inline int Rminibwa_query_group_view(const RmbQueryGroup *group, Rminibwa_query_group_view_t *out)
{ return Rminibwa_get_query_group_view()(group, out); }
static inline int Rminibwa_stream_error_code(const RmbQueryStream *stream)
{ return Rminibwa_get_stream_error_code()(stream); }
static inline const char *Rminibwa_stream_error_message(const RmbQueryStream *stream)
{ return Rminibwa_get_stream_error_message()(stream); }

#endif

#ifdef __cplusplus
}
#endif

#endif
