#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rminibwa_internal.h"

#define RMB_STREAM_MAX_QNAME_BYTES 1024U
#define RMB_STREAM_TAGS_PER_RECORD 8U

#define RMB_SAM_PAIRED        0x1U
#define RMB_SAM_PROPER        0x2U
#define RMB_SAM_UNMAPPED      0x4U
#define RMB_SAM_MATE_UNMAPPED 0x8U
#define RMB_SAM_REVERSE       0x10U
#define RMB_SAM_MATE_REVERSE  0x20U
#define RMB_SAM_READ1         0x40U
#define RMB_SAM_READ2         0x80U
#define RMB_SAM_SECONDARY     0x100U
#define RMB_SAM_SUPPLEMENTARY 0x800U

typedef struct RmbSeenName {
    struct RmbSeenName *next;
    size_t length;
    char name[];
} RmbSeenName;

struct RmbQueryGroup {
    struct RmbQueryStream *owner;
    uint64_t generation;
    uint64_t input_order;
    char *query_name;
    size_t query_name_length;
    uint32_t n_reads;
    mb_bseq1_t reads[2];
    Rminibwa_read_view read_views[2];
    Rminibwa_record_view *records;
    uint32_t n_records;
    Rminibwa_tag_view *tags;
    uint32_t *cigar_words;
    size_t n_cigar_words;
    size_t cigar_used;
    char *tag_strings;
    size_t n_tag_strings;
    size_t tag_strings_used;
};

struct RmbQueryStream {
    RmbIndex *index;
    SEXP index_ref;
    mb_bseq_file_t *fp[2];
    int n_fp;
    int paired;
    mb_opt_t opt;
    uint64_t next_input_order;
    uint64_t generation;
    int cancelled;
    int eof;
    int error_code;
    char error_message[256];
    char *read_group_record;
    size_t read_group_record_length;
    char *read_group_id;
    size_t read_group_id_length;
    Rminibwa_contig_view *contigs;
    uint32_t n_contigs;
    RmbSeenName **seen_slots;
    size_t n_seen_slots;
    size_t max_seen;
    size_t n_seen;
    RmbQueryGroup *current;
};

typedef struct {
    RmbQueryStream *stream;
    uint64_t generation;
} RmbQueryGroupHandle;

static SEXP rmb_stream_tag = NULL;
static SEXP rmb_query_group_tag = NULL;

void rminibwa_stream_init(void)
{
    if (rmb_stream_tag == NULL) rmb_stream_tag = Rf_install("Rminibwa_query_stream");
    if (rmb_query_group_tag == NULL) rmb_query_group_tag = Rf_install("Rminibwa_query_group");
}

static void stream_set_error(RmbQueryStream *stream, int code, const char *fmt, ...)
{
    va_list ap;
    if (stream == NULL || stream->error_code != RMINIBWA_STREAM_ERROR_NONE) return;
    stream->error_code = code;
    va_start(ap, fmt);
    vsnprintf(stream->error_message, sizeof(stream->error_message), fmt, ap);
    va_end(ap);
}

static int checked_mul_size(size_t a, size_t b, size_t *out)
{
    if (a != 0 && b > SIZE_MAX / a) return 0;
    *out = a * b;
    return 1;
}

static char *copy_bytes(const char *src, size_t n)
{
    char *out = (char *) malloc(n + 1);
    if (out == NULL) return NULL;
    if (n != 0) memcpy(out, src, n);
    out[n] = '\0';
    return out;
}

static void query_group_free(RmbQueryGroup *group)
{
    uint32_t i;
    if (group == NULL) return;
    for (i = 0; i < group->n_reads; ++i) mb_bseq_destroy1(&group->reads[i]);
    free(group->query_name);
    free(group->records);
    free(group->tags);
    free(group->cigar_words);
    free(group->tag_strings);
    free(group);
}

static void stream_drop_current(RmbQueryStream *stream)
{
    if (stream == NULL) return;
    query_group_free(stream->current);
    stream->current = NULL;
}

static void seen_free(RmbQueryStream *stream)
{
    size_t i;
    if (stream == NULL || stream->seen_slots == NULL) return;
    for (i = 0; i < stream->n_seen_slots; ++i) {
        RmbSeenName *node = stream->seen_slots[i];
        while (node != NULL) {
            RmbSeenName *next = node->next;
            free(node);
            node = next;
        }
    }
    free(stream->seen_slots);
    stream->seen_slots = NULL;
}

static void stream_free(RmbQueryStream *stream)
{
    int i;
    if (stream == NULL) return;
    stream_drop_current(stream);
    for (i = 0; i < stream->n_fp; ++i) {
        if (stream->fp[i] != NULL) mb_bseq_close(stream->fp[i]);
    }
    if (stream->index_ref != R_NilValue) R_ReleaseObject(stream->index_ref);
    free(stream->read_group_record);
    free(stream->read_group_id);
    free(stream->contigs);
    seen_free(stream);
    free(stream);
}

static void stream_finalizer(SEXP xptr)
{
    RmbQueryStream *stream;
    if (TYPEOF(xptr) != EXTPTRSXP) return;
    stream = (RmbQueryStream *) R_ExternalPtrAddr(xptr);
    if (stream != NULL) {
        stream_free(stream);
        R_ClearExternalPtr(xptr);
    }
}

static void query_group_handle_finalizer(SEXP xptr)
{
    RmbQueryGroupHandle *handle;
    if (TYPEOF(xptr) != EXTPTRSXP) return;
    handle = (RmbQueryGroupHandle *) R_ExternalPtrAddr(xptr);
    free(handle);
    R_ClearExternalPtr(xptr);
}

static SEXP with_class(SEXP x, const char *class_name)
{
    SEXP class_x = PROTECT(Rf_mkString(class_name));
    Rf_setAttrib(x, R_ClassSymbol, class_x);
    UNPROTECT(1);
    return x;
}

static SEXP stream_xptr_new(RmbQueryStream *stream)
{
    SEXP xptr;
    rminibwa_stream_init();
    xptr = PROTECT(R_MakeExternalPtr(stream, rmb_stream_tag, R_NilValue));
    R_RegisterCFinalizerEx(xptr, stream_finalizer, TRUE);
    with_class(xptr, "rminibwa_query_stream");
    UNPROTECT(1);
    return xptr;
}

static SEXP query_group_xptr_new(RmbQueryStream *stream, SEXP stream_x)
{
    RmbQueryGroupHandle *handle = (RmbQueryGroupHandle *) calloc(1, sizeof(*handle));
    SEXP xptr;
    if (handle == NULL) Rf_error("could not allocate query-group handle");
    handle->stream = stream;
    handle->generation = stream->generation;
    xptr = PROTECT(R_MakeExternalPtr(handle, rmb_query_group_tag, stream_x));
    R_RegisterCFinalizerEx(xptr, query_group_handle_finalizer, TRUE);
    with_class(xptr, "rminibwa_query_group");
    UNPROTECT(1);
    return xptr;
}

RmbQueryStream *Rminibwa_stream_from_sexp(SEXP x)
{
    RmbQueryStream *stream;
    rminibwa_stream_init();
    if (TYPEOF(x) != EXTPTRSXP || R_ExternalPtrTag(x) != rmb_stream_tag) return NULL;
    stream = (RmbQueryStream *) R_ExternalPtrAddr(x);
    return stream;
}

const RmbQueryGroup *Rminibwa_query_group_from_sexp(SEXP x)
{
    RmbQueryGroupHandle *handle;
    RmbQueryStream *stream;
    rminibwa_stream_init();
    if (TYPEOF(x) != EXTPTRSXP || R_ExternalPtrTag(x) != rmb_query_group_tag) return NULL;
    handle = (RmbQueryGroupHandle *) R_ExternalPtrAddr(x);
    if (handle == NULL || handle->stream == NULL) return NULL;
    stream = handle->stream;
    if (stream->generation != handle->generation || stream->current == NULL) return NULL;
    return stream->current;
}

static RmbQueryStream *stream_from_sexp_or_error(SEXP x)
{
    RmbQueryStream *stream = Rminibwa_stream_from_sexp(x);
    if (stream == NULL) Rf_error("expected a live Rminibwa query stream");
    return stream;
}

static const RmbQueryGroup *group_from_sexp_or_error(SEXP x)
{
    const RmbQueryGroup *group = Rminibwa_query_group_from_sexp(x);
    if (group == NULL) Rf_error("query group is no longer live; consume it before requesting the next group");
    return group;
}

static size_t qname_normalized_length(const char *name)
{
    size_t n = strlen(name);
    if (n >= 2 && name[n - 2] == '/' && name[n - 1] >= '0' && name[n - 1] <= '9') return n - 2;
    return n;
}

static int qname_mate_number(const char *name)
{
    size_t n = strlen(name);
    if (n >= 2 && name[n - 2] == '/' && name[n - 1] >= '0' && name[n - 1] <= '9') {
        return name[n - 1] - '0';
    }
    return 0;
}

static uint64_t qname_hash(const char *name, size_t n)
{
    size_t i;
    uint64_t hash = UINT64_C(1469598103934665603);
    for (i = 0; i < n; ++i) {
        hash ^= (uint8_t) name[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

/* Returns 0 after insertion, 1 for a duplicate, and -1 when the bounded
 * exact tracker is full or allocation fails. */
static int seen_insert(RmbQueryStream *stream, const char *name, size_t n)
{
    uint64_t hash;
    size_t slot;
    RmbSeenName *node;
    hash = qname_hash(name, n);
    slot = (size_t) (hash % stream->n_seen_slots);
    for (node = stream->seen_slots[slot]; node != NULL; node = node->next) {
        if (node->length == n && memcmp(node->name, name, n) == 0) return 1;
    }
    if (stream->n_seen >= stream->max_seen) return -1;
    node = (RmbSeenName *) malloc(sizeof(*node) + n + 1);
    if (node == NULL) return -1;
    node->next = stream->seen_slots[slot];
    node->length = n;
    memcpy(node->name, name, n);
    node->name[n] = '\0';
    stream->seen_slots[slot] = node;
    ++stream->n_seen;
    return 0;
}

static int read_one_fastq(RmbQueryStream *stream, mb_bseq1_t *out)
{
    int status = mb_bseq_read1(stream->fp[0], 1, 0, out);
    if (status < 0) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MALFORMED, "malformed or truncated FASTQ input");
        return -1;
    }
    if (status == 0) return 0;
    if (out->name == NULL || out->name[0] == '\0' || out->seq == NULL || out->l_seq == 0 || out->l_seq > INT32_MAX) {
        mb_bseq_destroy1(out);
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MALFORMED, "FASTQ record has an empty name or invalid sequence length");
        return -1;
    }
    if (out->qual == NULL) {
        mb_bseq_destroy1(out);
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MISSING_QUALITY, "FASTQ record has no quality string");
        return -1;
    }
    return 1;
}

static int read_one_fastq_from(RmbQueryStream *stream, int which, mb_bseq1_t *out)
{
    int status = mb_bseq_read1(stream->fp[which], 1, 0, out);
    if (status < 0) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MALFORMED, "malformed or truncated FASTQ input in mate %d", which + 1);
        return -1;
    }
    if (status == 0) return 0;
    if (out->name == NULL || out->name[0] == '\0' || out->seq == NULL || out->l_seq == 0 || out->l_seq > INT32_MAX) {
        mb_bseq_destroy1(out);
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MALFORMED, "FASTQ record in mate %d has an empty name or invalid sequence length", which + 1);
        return -1;
    }
    if (out->qual == NULL) {
        mb_bseq_destroy1(out);
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MISSING_QUALITY, "FASTQ record in mate %d has no quality string", which + 1);
        return -1;
    }
    return 1;
}

static int next_input_group(RmbQueryStream *stream, RmbQueryGroup **out)
{
    RmbQueryGroup *group;
    int first, second = 0;
    size_t qname_length;
    int seen;

    group = (RmbQueryGroup *) calloc(1, sizeof(*group));
    if (group == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not allocate a query group");
        return RMINIBWA_STREAM_ERROR;
    }
    group->owner = stream;
    group->input_order = stream->next_input_order;
    group->n_reads = stream->paired ? 2U : 1U;

    if (stream->paired) {
        first = read_one_fastq_from(stream, 0, &group->reads[0]);
        second = read_one_fastq_from(stream, 1, &group->reads[1]);
        if (first != 1 || second != 1) {
            if (first == 0 && second == 0) {
                query_group_free(group);
                stream->eof = 1;
                return RMINIBWA_STREAM_EOF;
            }
            if (stream->error_code == RMINIBWA_STREAM_ERROR_NONE) {
                stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MATE_COUNT,
                                 "paired FASTQ files have different record counts");
            }
            query_group_free(group);
            return RMINIBWA_STREAM_ERROR;
        }
    } else {
        first = read_one_fastq(stream, &group->reads[0]);
        if (first == 0) {
            query_group_free(group);
            stream->eof = 1;
            return RMINIBWA_STREAM_EOF;
        }
        if (first < 0) {
            query_group_free(group);
            return RMINIBWA_STREAM_ERROR;
        }
    }

    qname_length = qname_normalized_length(group->reads[0].name);
    if (qname_length == 0 || qname_length > RMB_STREAM_MAX_QNAME_BYTES) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NAME_TOO_LONG,
                         "normalized query name exceeds %u bytes", RMB_STREAM_MAX_QNAME_BYTES);
        query_group_free(group);
        return RMINIBWA_STREAM_ERROR;
    }
    if (stream->paired) {
        size_t mate_length = qname_normalized_length(group->reads[1].name);
        int mate1 = qname_mate_number(group->reads[0].name);
        int mate2 = qname_mate_number(group->reads[1].name);
        if (mate_length != qname_length || memcmp(group->reads[0].name, group->reads[1].name, qname_length) != 0) {
            stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_NAME_MISMATCH,
                             "paired FASTQ query names do not match after normalization");
            query_group_free(group);
            return RMINIBWA_STREAM_ERROR;
        }
        if ((mate1 != 0 || mate2 != 0) && (mate1 != 1 || mate2 != 2)) {
            stream_set_error(stream, RMINIBWA_STREAM_ERROR_FASTQ_MATE_ORDER,
                             "paired FASTQ records are not ordered as mate 1 then mate 2");
            query_group_free(group);
            return RMINIBWA_STREAM_ERROR;
        }
    }
    group->query_name = copy_bytes(group->reads[0].name, qname_length);
    if (group->query_name == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not copy query name");
        query_group_free(group);
        return RMINIBWA_STREAM_ERROR;
    }
    group->query_name_length = qname_length;
    seen = seen_insert(stream, group->query_name, qname_length);
    if (seen != 0) {
        stream_set_error(stream,
                         seen > 0 ? RMINIBWA_STREAM_ERROR_QNAME_REPEATED : RMINIBWA_STREAM_ERROR_QNAME_TRACKER_LIMIT,
                         seen > 0 ? "query name appears in more than one emitted group" :
                                    "bounded query-name tracker limit reached");
        query_group_free(group);
        return RMINIBWA_STREAM_ERROR;
    }
    *out = group;
    return RMINIBWA_STREAM_OK;
}

static void free_hit_batch(mb_hit_t **hit, const int32_t *n_hit, uint32_t n_reads)
{
    uint32_t i;
    if (hit == NULL) return;
    for (i = 0; i < n_reads; ++i) {
        int32_t j;
        for (j = 0; j < n_hit[i]; ++j) free(hit[i][j].p);
        free(hit[i]);
    }
    free(hit);
}

static const mb_hit_t *primary_hit(const mb_hit_t *hit, int32_t n_hit)
{
    int32_t i;
    for (i = 0; i < n_hit; ++i) if (hit[i].sam_pri) return &hit[i];
    return NULL;
}

static uint16_t record_flag(const RmbQueryStream *stream, const mb_hit_t *hit,
                            uint32_t read_index, const mb_hit_t *mate)
{
    uint16_t flag = 0;
    if (stream->paired) {
        flag |= RMB_SAM_PAIRED;
        if (read_index == 0) flag |= RMB_SAM_READ1;
        else flag |= RMB_SAM_READ2;
        if (mate == NULL) flag |= RMB_SAM_MATE_UNMAPPED;
        else if (mate->rev) flag |= RMB_SAM_MATE_REVERSE;
    }
    if (hit == NULL) return flag | RMB_SAM_UNMAPPED;
    if (hit->proper_pair) flag |= RMB_SAM_PROPER;
    if (hit->rev) flag |= RMB_SAM_REVERSE;
    if (hit->parent != hit->id) flag |= RMB_SAM_SECONDARY;
    else if (!hit->sam_pri) flag |= RMB_SAM_SUPPLEMENTARY;
    return flag;
}

static int64_t record_template_length(const mb_hit_t *hit, const mb_hit_t *mate)
{
    int64_t this_5, mate_5, tlen;
    if (hit == NULL || mate == NULL || hit->tid != mate->tid) return 0;
    this_5 = hit->rev ? hit->te - 1 : hit->ts;
    mate_5 = mate->rev ? mate->te - 1 : mate->ts;
    tlen = mate_5 - this_5;
    if (tlen > 0) ++tlen;
    else if (tlen < 0) --tlen;
    return tlen;
}

static uint32_t record_cigar_count(const RmbQueryStream *stream, const mb_hit_t *hit,
                                   int32_t qlen, uint16_t flag)
{
    int32_t left, right;
    uint32_t n;
    if (hit == NULL || hit->p == NULL || hit->p->n_cigar <= 0) return 0;
    left = hit->rev ? qlen - hit->qe : hit->qs;
    right = hit->rev ? hit->qs : qlen - hit->qe;
    n = (uint32_t) hit->p->n_cigar;
    if (left > 0) ++n;
    if (right > 0) ++n;
    (void) stream;
    (void) flag;
    return n;
}

static int write_record_cigar(RmbQueryGroup *group, const RmbQueryStream *stream,
                              Rminibwa_record_view *record, const mb_hit_t *hit,
                              int32_t qlen, uint16_t flag)
{
    int32_t left, right, i;
    uint32_t clip_op;
    if (hit == NULL || hit->p == NULL || hit->p->n_cigar <= 0) {
        record->cigar = NULL;
        record->cigar_n = 0;
        return 1;
    }
    left = hit->rev ? qlen - hit->qe : hit->qs;
    right = hit->rev ? hit->qs : qlen - hit->qe;
    if (left < 0 || right < 0 || (uint32_t) left > UINT32_C(0x0fffffff) || (uint32_t) right > UINT32_C(0x0fffffff)) {
        return 0;
    }
    record->cigar = group->cigar_words + group->cigar_used;
    clip_op = (((flag & RMB_SAM_SUPPLEMENTARY) ||
               ((flag & RMB_SAM_SECONDARY) && (stream->opt.flag & MB_F_2ND_SEQ))) &&
               !(stream->opt.flag & MB_F_SUPP_SOFT)) ? MB_CIGAR_HARDCLIP : MB_CIGAR_SOFTCLIP;
    if (left > 0) group->cigar_words[group->cigar_used++] = ((uint32_t) left << 4) | clip_op;
    for (i = 0; i < hit->p->n_cigar; ++i) group->cigar_words[group->cigar_used++] = hit->p->cigar[i];
    if (right > 0) group->cigar_words[group->cigar_used++] = ((uint32_t) right << 4) | clip_op;
    record->cigar_n = record_cigar_count(stream, hit, qlen, flag);
    return 1;
}

static void tag_i32(Rminibwa_record_view *record, uint16_t tag, int32_t value)
{
    Rminibwa_tag_view *out = (Rminibwa_tag_view *) record->tags + record->n_tags++;
    out->tag = tag;
    out->type = RMINIBWA_TAG_I32;
    out->i32 = value;
}

static int tag_string_copy(RmbQueryGroup *group, Rminibwa_record_view *record,
                           uint16_t tag, const char *value, size_t n)
{
    Rminibwa_tag_view *out = (Rminibwa_tag_view *) record->tags + record->n_tags++;
    if (group->tag_strings_used + n + 1 > group->n_tag_strings) return 0;
    out->tag = tag;
    out->type = RMINIBWA_TAG_STRING;
    out->string = group->tag_strings + group->tag_strings_used;
    out->string_length = n;
    memcpy(group->tag_strings + group->tag_strings_used, value, n);
    group->tag_strings_used += n;
    group->tag_strings[group->tag_strings_used++] = '\0';
    return 1;
}

static void tag_string_borrowed(Rminibwa_record_view *record, uint16_t tag,
                                const char *value, size_t n)
{
    Rminibwa_tag_view *out = (Rminibwa_tag_view *) record->tags + record->n_tags++;
    out->tag = tag;
    out->type = RMINIBWA_TAG_STRING;
    out->string = value;
    out->string_length = n;
}

static void tag_cigar(Rminibwa_record_view *record, uint16_t tag,
                      const uint32_t *cigar, uint32_t cigar_n)
{
    Rminibwa_tag_view *out = (Rminibwa_tag_view *) record->tags + record->n_tags++;
    out->tag = tag;
    out->type = RMINIBWA_TAG_CIGAR;
    out->cigar = cigar;
    out->cigar_n = cigar_n;
}

static size_t hit_tag_string_bytes(const mb_hit_t *hit)
{
    const char *tag;
    if (hit == NULL || hit->p == NULL || !hit->p->cs) return 0;
    tag = (const char *) &hit->p->cigar[hit->p->n_cigar];
    return strlen(tag) + 1;
}

static int add_hit_tags(RmbQueryGroup *group, Rminibwa_record_view *record,
                        const mb_hit_t *hit, const RmbQueryStream *stream)
{
    const char *raw_tag;
    size_t tag_length;
    if (hit == NULL || hit->p == NULL) return 1;
    tag_i32(record, RMINIBWA_TAG('A', 'S'), hit->p->dp_score);
    tag_i32(record, RMINIBWA_TAG('N', 'M'), hit->blen - hit->mlen + (int32_t) hit->p->n_ambi);
    tag_i32(record, RMINIBWA_TAG('m', 's'), hit->p->dp_max0);
    tag_i32(record, RMINIBWA_TAG('m', 'd'), hit->p->dp_max - hit->p->dp_max2);
    if (hit->p->cs) {
        raw_tag = (const char *) &hit->p->cigar[hit->p->n_cigar];
        tag_length = strlen(raw_tag);
        if (tag_length >= 5 && raw_tag[2] == ':' && raw_tag[3] == 'Z' && raw_tag[4] == ':') {
            if (!tag_string_copy(group, record, RMINIBWA_TAG(raw_tag[0], raw_tag[1]), raw_tag + 5, tag_length - 5)) return 0;
        }
    }
    if (stream->read_group_id != NULL) {
        tag_string_borrowed(record, RMINIBWA_TAG('R', 'G'), stream->read_group_id, stream->read_group_id_length);
    }
    return record->n_tags <= RMB_STREAM_TAGS_PER_RECORD;
}

static void set_record_sequence_view(Rminibwa_record_view *record, const RmbQueryStream *stream,
                                     const mb_hit_t *hit, int32_t qlen)
{
    if (hit == NULL) {
        record->sequence_mode = RMINIBWA_SEQUENCE_FULL;
        record->sequence_length = (uint32_t) qlen;
        return;
    }
    if ((record->flag & (RMB_SAM_SECONDARY | RMB_SAM_SUPPLEMENTARY)) == 0 ||
        (stream->opt.flag & MB_F_SUPP_SOFT)) {
        record->sequence_mode = RMINIBWA_SEQUENCE_FULL;
        record->sequence_length = (uint32_t) qlen;
        record->sequence_reverse_complement = hit->rev ? 1U : 0U;
        record->qualities_reverse = hit->rev ? 1U : 0U;
    } else if ((record->flag & RMB_SAM_SECONDARY) && !(stream->opt.flag & MB_F_2ND_SEQ)) {
        record->sequence_mode = RMINIBWA_SEQUENCE_ABSENT;
        record->sequence_length = 0;
    } else {
        record->sequence_mode = RMINIBWA_SEQUENCE_SEGMENT;
        record->sequence_offset = (uint32_t) hit->qs;
        record->sequence_length = (uint32_t) (hit->qe - hit->qs);
        record->sequence_reverse_complement = hit->rev ? 1U : 0U;
        record->qualities_reverse = hit->rev ? 1U : 0U;
    }
}

static int map_group(RmbQueryStream *stream, RmbQueryGroup *group)
{
    const char *seq[2];
    const char *qname[2];
    int32_t qlen[2];
    int32_t n_hit[2] = {0, 0};
    mb_hit_t **hit = NULL;
    const mb_hit_t *primary[2] = {NULL, NULL};
    Rminibwa_record_view *primary_record[2] = {NULL, NULL};
    size_t n_records = 0, n_cigar = 0, n_strings = 0, bytes;
    uint32_t i, j, record_order = 0;

    for (i = 0; i < group->n_reads; ++i) {
        seq[i] = group->reads[i].seq;
        qname[i] = group->query_name;
        qlen[i] = (int32_t) group->reads[i].l_seq;
    }
    hit = mb_map_batch(&stream->opt, stream->index->ptr, (int32_t) group->n_reads,
                       qlen, seq, n_hit, NULL, qname);
    if (hit == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_MAPPING, "minibwa did not return an alignment batch");
        return 0;
    }
    for (i = 0; i < group->n_reads; ++i) {
        primary[i] = primary_hit(hit[i], n_hit[i]);
        n_records += n_hit[i] > 0 ? (size_t) n_hit[i] : 1U;
        for (j = 0; j < (uint32_t) n_hit[i]; ++j) {
            uint16_t flag = record_flag(stream, &hit[i][j], i, stream->paired ? primary[1U - i] : NULL);
            n_cigar += record_cigar_count(stream, &hit[i][j], qlen[i], flag);
            n_strings += hit_tag_string_bytes(&hit[i][j]);
        }
    }
    if (n_records > UINT32_MAX || !checked_mul_size(n_records, sizeof(*group->records), &bytes)) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "query group is too large");
        free_hit_batch(hit, n_hit, group->n_reads);
        return 0;
    }
    group->records = (Rminibwa_record_view *) calloc(n_records, sizeof(*group->records));
    if (group->records == NULL || !checked_mul_size(n_records, RMB_STREAM_TAGS_PER_RECORD, &bytes)) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not allocate query-group records");
        free_hit_batch(hit, n_hit, group->n_reads);
        return 0;
    }
    group->tags = (Rminibwa_tag_view *) calloc(bytes, sizeof(*group->tags));
    group->cigar_words = n_cigar ? (uint32_t *) calloc(n_cigar, sizeof(*group->cigar_words)) : NULL;
    group->tag_strings = n_strings ? (char *) calloc(n_strings, 1) : NULL;
    group->n_records = (uint32_t) n_records;
    group->n_cigar_words = n_cigar;
    group->n_tag_strings = n_strings;
    if (group->tags == NULL || (n_cigar != 0 && group->cigar_words == NULL) ||
        (n_strings != 0 && group->tag_strings == NULL)) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not allocate query-group alignment storage");
        free_hit_batch(hit, n_hit, group->n_reads);
        return 0;
    }

    for (i = 0; i < group->n_reads; ++i) {
        const mb_hit_t *mate = stream->paired ? primary[1U - i] : NULL;
        if (n_hit[i] == 0) {
            Rminibwa_record_view *record = &group->records[record_order];
            record->input_order = group->input_order;
            record->record_order = record_order;
            record->read_index = i;
            record->reference_id = -1;
            record->position = -1;
            record->mate_reference_id = mate ? (int32_t) mate->tid : -1;
            record->mate_position = mate ? mate->ts : -1;
            record->template_length = 0;
            record->flag = record_flag(stream, NULL, i, mate);
            record->mapq = 0;
            record->mate_mapq = mate ? mate->mapq : -1;
            record->tags = group->tags + (size_t) record_order * RMB_STREAM_TAGS_PER_RECORD;
            set_record_sequence_view(record, stream, NULL, qlen[i]);
            if (stream->read_group_id != NULL) tag_string_borrowed(record, RMINIBWA_TAG('R', 'G'), stream->read_group_id, stream->read_group_id_length);
            ++record_order;
            continue;
        }
        for (j = 0; j < (uint32_t) n_hit[i]; ++j) {
            const mb_hit_t *h = &hit[i][j];
            Rminibwa_record_view *record = &group->records[record_order];
            record->input_order = group->input_order;
            record->record_order = record_order;
            record->read_index = i;
            record->reference_id = (int32_t) h->tid;
            record->position = h->ts;
            record->mate_reference_id = mate ? (int32_t) mate->tid : -1;
            record->mate_position = mate ? mate->ts : -1;
            record->template_length = record_template_length(h, mate);
            record->flag = record_flag(stream, h, i, mate);
            record->mapq = (uint8_t) h->mapq;
            record->mate_mapq = mate ? mate->mapq : -1;
            record->alignment_score = h->p ? h->p->dp_score : 0;
            record->edit_distance = h->p ? h->blen - h->mlen + (int32_t) h->p->n_ambi : 0;
            record->chain_score = h->score;
            record->chain_subscore = h->subsc;
            record->tags = group->tags + (size_t) record_order * RMB_STREAM_TAGS_PER_RECORD;
            set_record_sequence_view(record, stream, h, qlen[i]);
            if (!write_record_cigar(group, stream, record, h, qlen[i], record->flag) ||
                !add_hit_tags(group, record, h, stream)) {
                stream_set_error(stream, RMINIBWA_STREAM_ERROR_MAPPING, "could not encode a typed alignment record");
                free_hit_batch(hit, n_hit, group->n_reads);
                return 0;
            }
            if (h == primary[i]) primary_record[i] = record;
            ++record_order;
        }
    }
    for (i = 0; i < group->n_records; ++i) {
        Rminibwa_record_view *record = &group->records[i];
        if (stream->paired && primary_record[1U - record->read_index] != NULL) {
            Rminibwa_record_view *mate_record = primary_record[1U - record->read_index];
            record->mate_cigar = mate_record->cigar;
            record->mate_cigar_n = mate_record->cigar_n;
            tag_cigar(record, RMINIBWA_TAG('M', 'C'), mate_record->cigar, mate_record->cigar_n);
            tag_i32(record, RMINIBWA_TAG('M', 'Q'), mate_record->mapq);
        }
        if (record->n_tags > RMB_STREAM_TAGS_PER_RECORD) {
            stream_set_error(stream, RMINIBWA_STREAM_ERROR_MAPPING, "typed tag capacity exceeded");
            free_hit_batch(hit, n_hit, group->n_reads);
            return 0;
        }
    }
    for (i = 0; i < group->n_reads; ++i) {
        group->read_views[i].input_order = group->input_order;
        group->read_views[i].read_number = stream->paired ? i + 1U : 0U;
        group->read_views[i].sequence_length = (uint32_t) group->reads[i].l_seq;
        group->read_views[i].sequence = group->reads[i].seq;
        group->read_views[i].qualities = group->reads[i].qual;
    }
    free_hit_batch(hit, n_hit, group->n_reads);
    return 1;
}

uint32_t Rminibwa_stream_abi_version(void)
{
    return RMINIBWA_STREAM_ABI_VERSION;
}

int Rminibwa_stream_next(RmbQueryStream *stream, const RmbQueryGroup **group)
{
    RmbQueryGroup *next = NULL;
    int status;
    if (group != NULL) *group = NULL;
    if (stream == NULL) return RMINIBWA_STREAM_ERROR;
    stream_drop_current(stream);
    ++stream->generation;
    if (stream->cancelled) return RMINIBWA_STREAM_CANCELLED;
    if (stream->error_code != RMINIBWA_STREAM_ERROR_NONE) return RMINIBWA_STREAM_ERROR;
    if (stream->eof) return RMINIBWA_STREAM_EOF;
    status = next_input_group(stream, &next);
    if (status != RMINIBWA_STREAM_OK) return status;
    if (!map_group(stream, next)) {
        query_group_free(next);
        return RMINIBWA_STREAM_ERROR;
    }
    next->generation = stream->generation;
    stream->current = next;
    ++stream->next_input_order;
    if (group != NULL) *group = next;
    return RMINIBWA_STREAM_OK;
}

int Rminibwa_stream_cancel(RmbQueryStream *stream)
{
    if (stream == NULL) return RMINIBWA_STREAM_ERROR;
    stream_drop_current(stream);
    ++stream->generation;
    stream->cancelled = 1;
    if (stream->error_code == RMINIBWA_STREAM_ERROR_NONE) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_CANCELLED, "cancelled by consumer");
    }
    return RMINIBWA_STREAM_CANCELLED;
}

int Rminibwa_stream_header(const RmbQueryStream *stream, Rminibwa_header_view *out)
{
    if (stream == NULL || out == NULL) return RMINIBWA_STREAM_ERROR;
    memset(out, 0, sizeof(*out));
    out->abi_version = RMINIBWA_STREAM_ABI_VERSION;
    out->sort_order = RMINIBWA_SORT_UNSORTED;
    out->group_order = RMINIBWA_GROUP_QUERY;
    out->struct_size = sizeof(*out);
    out->contigs = stream->contigs;
    out->n_contigs = stream->n_contigs;
    out->read_group_record = stream->read_group_record;
    out->read_group_record_length = stream->read_group_record_length;
    out->read_group_id = stream->read_group_id;
    out->read_group_id_length = stream->read_group_id_length;
    out->program_id = "minibwa";
    out->program_name = "minibwa";
    out->program_version = MB_VERSION;
    return RMINIBWA_STREAM_OK;
}

int Rminibwa_query_group_view(const RmbQueryGroup *group, Rminibwa_query_group_view_t *out)
{
    if (group == NULL || out == NULL) return RMINIBWA_STREAM_ERROR;
    memset(out, 0, sizeof(*out));
    out->abi_version = RMINIBWA_STREAM_ABI_VERSION;
    out->struct_size = sizeof(*out);
    out->input_order = group->input_order;
    out->query_name = group->query_name;
    out->query_name_length = group->query_name_length;
    out->read_group_id = group->owner->read_group_id;
    out->read_group_id_length = group->owner->read_group_id_length;
    out->reads = group->read_views;
    out->n_reads = group->n_reads;
    out->records = group->records;
    out->n_records = group->n_records;
    return RMINIBWA_STREAM_OK;
}

int Rminibwa_stream_error_code(const RmbQueryStream *stream)
{
    return stream == NULL ? RMINIBWA_STREAM_ERROR_INVALID_ARGUMENT : stream->error_code;
}

const char *Rminibwa_stream_error_message(const RmbQueryStream *stream)
{
    if (stream == NULL) return "invalid Rminibwa query stream";
    return stream->error_message;
}

static int parse_read_group(RmbQueryStream *stream, SEXP read_group_x)
{
    const char *record, *field, *end;
    size_t n;
    if (read_group_x == R_NilValue) return 1;
    if (TYPEOF(read_group_x) != STRSXP || XLENGTH(read_group_x) != 1 || STRING_ELT(read_group_x, 0) == NA_STRING) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_INVALID_READ_GROUP, "read_group must be NULL or one @RG character scalar");
        return 0;
    }
    record = CHAR(STRING_ELT(read_group_x, 0));
    n = strlen(record);
    if (n < 7 || strncmp(record, "@RG\t", 4) != 0 || strchr(record, '\n') != NULL || strchr(record, '\r') != NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_INVALID_READ_GROUP, "read_group must be one literal @RG record with an ID field");
        return 0;
    }
    field = record + 4;
    while (field != NULL && *field != '\0') {
        end = strchr(field, '\t');
        if (strncmp(field, "ID:", 3) == 0 && field[3] != '\0' && (end == NULL || end > field + 3)) {
            size_t id_length = end == NULL ? strlen(field + 3) : (size_t) (end - (field + 3));
            stream->read_group_id = copy_bytes(field + 3, id_length);
            if (stream->read_group_id == NULL) {
                stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not copy read-group ID");
                return 0;
            }
            stream->read_group_id_length = id_length;
            break;
        }
        field = end == NULL ? NULL : end + 1;
    }
    if (stream->read_group_id == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_INVALID_READ_GROUP, "read_group must include a non-empty ID tag");
        return 0;
    }
    stream->read_group_record = copy_bytes(record, n);
    if (stream->read_group_record == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not copy read-group record");
        return 0;
    }
    stream->read_group_record_length = n;
    return 1;
}

static int initialize_contigs(RmbQueryStream *stream)
{
    uint32_t n = 0, i;
    while (mb_idx_ctg_name(stream->index->ptr, (int32_t) n) != NULL) {
        if (n == UINT32_MAX) {
            stream_set_error(stream, RMINIBWA_STREAM_ERROR_INVALID_ARGUMENT, "index has too many contigs");
            return 0;
        }
        ++n;
    }
    stream->contigs = n ? (Rminibwa_contig_view *) calloc(n, sizeof(*stream->contigs)) : NULL;
    if (n != 0 && stream->contigs == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not allocate header contigs");
        return 0;
    }
    stream->n_contigs = n;
    for (i = 0; i < n; ++i) {
        stream->contigs[i].name = mb_idx_ctg_name(stream->index->ptr, (int32_t) i);
        stream->contigs[i].name_length = strlen(stream->contigs[i].name);
        stream->contigs[i].length = mb_idx_ctg_len(stream->index->ptr, (int32_t) i);
    }
    return 1;
}

static RmbQueryStream *stream_new(SEXP path_x, SEXP mode_x, SEXP index_x, SEXP opt_x,
                                  SEXP threads_x, SEXP read_group_x, SEXP max_seen_x)
{
    RmbQueryStream *stream;
    int mode, threads, max_seen;
    int i;
    if (TYPEOF(path_x) != STRSXP || (XLENGTH(path_x) != 1 && XLENGTH(path_x) != 2)) {
        Rf_error("path must be one FASTQ path for single mode or two paths for paired mode");
    }
    if (!Rf_isInteger(mode_x) || XLENGTH(mode_x) != 1 || INTEGER(mode_x)[0] == NA_INTEGER) Rf_error("mode is invalid");
    mode = INTEGER(mode_x)[0];
    if (mode != 0 && mode != 1) Rf_error("mode is invalid");
    if ((mode == 0 && XLENGTH(path_x) != 1) || (mode == 1 && XLENGTH(path_x) != 2)) {
        Rf_error("path length does not match the explicit stream mode");
    }
    if (!Rf_isInteger(threads_x) || XLENGTH(threads_x) != 1 || INTEGER(threads_x)[0] == NA_INTEGER || INTEGER(threads_x)[0] < 1) {
        Rf_error("threads must be one positive integer total thread budget");
    }
    if (!Rf_isInteger(max_seen_x) || XLENGTH(max_seen_x) != 1 || INTEGER(max_seen_x)[0] == NA_INTEGER || INTEGER(max_seen_x)[0] < 1) {
        Rf_error("max_seen_qnames must be one positive integer");
    }
    threads = INTEGER(threads_x)[0];
    max_seen = INTEGER(max_seen_x)[0];
    stream = (RmbQueryStream *) calloc(1, sizeof(*stream));
    if (stream == NULL) Rf_error("could not allocate query stream");
    stream->index = rminibwa_index_from_sexp(index_x);
    stream->index_ref = index_x;
    R_PreserveObject(index_x);
    stream->n_fp = mode ? 2 : 1;
    stream->paired = mode;
    stream->max_seen = (size_t) max_seen;
    rminibwa_apply_options(opt_x, &stream->opt);
    stream->opt.n_thread = threads;
    if (stream->paired) stream->opt.flag |= MB_F_PE;
    else stream->opt.flag &= ~MB_F_PE;
    stream->n_seen_slots = (size_t) max_seen * 2U + 1U;
    stream->seen_slots = (RmbSeenName **) calloc(stream->n_seen_slots, sizeof(*stream->seen_slots));
    if (stream->seen_slots == NULL) {
        stream_set_error(stream, RMINIBWA_STREAM_ERROR_NO_MEMORY, "could not allocate bounded query-name tracker");
        stream_free(stream);
        Rf_error("could not allocate bounded query-name tracker");
    }
    if (!parse_read_group(stream, read_group_x) || !initialize_contigs(stream)) {
        char message[sizeof(stream->error_message)];
        snprintf(message, sizeof(message), "%s", stream->error_message);
        stream_free(stream);
        Rf_error("%s", message);
    }
    for (i = 0; i < stream->n_fp; ++i) {
        SEXP path = STRING_ELT(path_x, i);
        if (path == NA_STRING || CHAR(path)[0] == '\0') {
            stream_free(stream);
            Rf_error("path must not contain NA or empty values");
        }
        stream->fp[i] = mb_bseq_open(CHAR(path));
        if (stream->fp[i] == NULL) {
            char message[256];
            snprintf(message, sizeof(message), "failed to open FASTQ file: %s", CHAR(path));
            stream_free(stream);
            Rf_error("%s", message);
        }
    }
    return stream;
}

SEXP RC_mb_query_stream_open(SEXP path_x, SEXP mode_x, SEXP index_x, SEXP opt_x,
                             SEXP threads_x, SEXP read_group_x, SEXP max_seen_x)
{
    return stream_xptr_new(stream_new(path_x, mode_x, index_x, opt_x, threads_x, read_group_x, max_seen_x));
}

SEXP RC_mb_query_stream_next(SEXP stream_x)
{
    RmbQueryStream *stream = stream_from_sexp_or_error(stream_x);
    const RmbQueryGroup *group = NULL;
    int status = Rminibwa_stream_next(stream, &group);
    if (status == RMINIBWA_STREAM_OK) return query_group_xptr_new(stream, stream_x);
    if (status == RMINIBWA_STREAM_EOF || status == RMINIBWA_STREAM_CANCELLED) return R_NilValue;
    Rf_error("Rminibwa query stream failed [%d]: %s", stream->error_code, stream->error_message);
    return R_NilValue;
}

SEXP RC_mb_query_stream_cancel(SEXP stream_x)
{
    RmbQueryStream *stream = stream_from_sexp_or_error(stream_x);
    Rminibwa_stream_cancel(stream);
    return Rf_ScalarLogical(TRUE);
}

SEXP RC_mb_query_stream_error(SEXP stream_x)
{
    RmbQueryStream *stream = stream_from_sexp_or_error(stream_x);
    SEXP out = PROTECT(Rf_allocVector(VECSXP, 2));
    SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
    SET_VECTOR_ELT(out, 0, Rf_ScalarInteger(stream->error_code));
    SET_VECTOR_ELT(out, 1, Rf_mkString(stream->error_message));
    SET_STRING_ELT(names, 0, Rf_mkChar("code"));
    SET_STRING_ELT(names, 1, Rf_mkChar("message"));
    Rf_setAttrib(out, R_NamesSymbol, names);
    UNPROTECT(2);
    return out;
}

SEXP RC_mb_query_group_n_reads(SEXP group_x)
{
    const RmbQueryGroup *group = group_from_sexp_or_error(group_x);
    return Rf_ScalarInteger((int) group->n_reads);
}

SEXP RC_mb_query_group_n_records(SEXP group_x)
{
    const RmbQueryGroup *group = group_from_sexp_or_error(group_x);
    return Rf_ScalarInteger((int) group->n_records);
}

SEXP RC_mb_query_group_name(SEXP group_x)
{
    const RmbQueryGroup *group = group_from_sexp_or_error(group_x);
    SEXP out = PROTECT(Rf_allocVector(RAWSXP, (R_xlen_t) group->query_name_length));
    if (group->query_name_length != 0) memcpy(RAW(out), group->query_name, group->query_name_length);
    UNPROTECT(1);
    return out;
}

SEXP RC_mb_query_group_input_order(SEXP group_x)
{
    const RmbQueryGroup *group = group_from_sexp_or_error(group_x);
    return Rf_ScalarReal((double) group->input_order);
}
