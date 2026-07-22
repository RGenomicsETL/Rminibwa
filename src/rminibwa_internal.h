#ifndef RMINIBWA_INTERNAL_H
#define RMINIBWA_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <Rinternals.h>

#include "vendor/minibwa/minibwa.h"
#include "vendor/minibwa/bseq.h"
#include "Rminibwa.h"

typedef struct RmbFastxIter RmbFastxIter;
typedef struct RmbFastxBatch RmbFastxBatch;

struct RmbIndex {
    mb_idx_t *ptr;
    int meth;
};

struct RmbFastxIter {
    int n_fp;
    mb_bseq_file_t **fp;
    int64_t chunk_size;
    int64_t max_chunk_size;
    int with_qual;
    int with_comment;
    int paired;
    int64_t next_read_id;
    int64_t next_fragment_id;
};

struct RmbFastxBatch {
    int32_t n;
    int paired;
    mb_bseq1_t *seq;
    int32_t *read_id;
    int32_t *fragment_id;
    int32_t *mate;
};

struct RmbAlignBatch {
    size_t n;
    size_t n_reads;
    SEXP index_ref;
    int32_t *read_index;
    int32_t *read_length;
    int32_t *read_fragment_id;
    int32_t *read_mate;
    int32_t *read_hit_offset;
    int32_t *read_hit_count;
    int32_t *read_id;
    int32_t *target_id;
    int32_t *query_length;
    int32_t *query_start;
    int32_t *query_end;
    int64_t *target_length;
    int64_t *target_start;
    int64_t *target_end;
    int32_t *strand;
    int32_t *mapq;
    int32_t *score;
    int32_t *matches;
    int32_t *block_length;
    int32_t *flags;
    int32_t *n_sub;
    int32_t *cigar_offset;
    int32_t *cigar_n;
    uint32_t *cigar_words;
    size_t n_cigar_words;
};

void rminibwa_init_altrep(DllInfo *dll);
void rminibwa_stream_init(void);
SEXP rminibwa_align_altinteger(SEXP batch_xptr, const char *name);
SEXP rminibwa_align_read_altinteger(SEXP batch_xptr, const char *name);
SEXP rminibwa_align_altreal(SEXP batch_xptr, const char *name);
SEXP rminibwa_align_cigar_raw(SEXP batch_xptr);

void rminibwa_ksw_init_dispatch(void);
void rminibwa_ksw_set_backend(const char *backend);
const char *rminibwa_ksw_backend(void);

RmbIndex *rminibwa_index_from_sexp(SEXP x);
RmbAlignBatch *rminibwa_align_mut_from_sexp(SEXP x);
void rminibwa_apply_options(SEXP opts_x, mb_opt_t *opt);
SEXP rminibwa_align_xptr_new(RmbAlignBatch *batch);

#endif
