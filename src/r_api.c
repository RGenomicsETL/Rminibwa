#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "rminibwa_internal.h"
#include "vendor/minibwa/mbpriv.h"

extern int main_index(int argc, char **argv);

#define RMB_FLAG_PROPER        0x01
#define RMB_FLAG_PRIMARY       0x02
#define RMB_FLAG_SECONDARY     0x04
#define RMB_FLAG_SUPPLEMENTARY 0x08

static int is_true_scalar(SEXP x) {
    return TYPEOF(x) == LGLSXP && XLENGTH(x) == 1 && LOGICAL(x)[0] == TRUE;
}

static int is_false_scalar(SEXP x) {
    return TYPEOF(x) == LGLSXP && XLENGTH(x) == 1 && LOGICAL(x)[0] == FALSE;
}

static const char *chr_scalar(SEXP x, const char *what) {
    if (TYPEOF(x) != STRSXP || XLENGTH(x) != 1 || STRING_ELT(x, 0) == NA_STRING) {
        Rf_error("%s must be a non-missing character scalar", what);
    }
    return CHAR(STRING_ELT(x, 0));
}

static int int_scalar(SEXP x, const char *what, int default_value) {
    if (x == R_NilValue) return default_value;
    if (!Rf_isInteger(x) && !Rf_isReal(x)) Rf_error("%s must be a numeric scalar", what);
    if (XLENGTH(x) != 1) Rf_error("%s must be a numeric scalar", what);
    double val = Rf_asReal(x);
    if (!R_FINITE(val) || val < INT_MIN || val > INT_MAX) Rf_error("%s is out of range", what);
    return (int) val;
}

RmbIndex *rminibwa_index_from_sexp(SEXP x) {
    if (TYPEOF(x) != EXTPTRSXP) Rf_error("expected a minibwa index external pointer");
    RmbIndex *idx = (RmbIndex *) R_ExternalPtrAddr(x);
    if (idx == NULL || idx->ptr == NULL) Rf_error("minibwa index pointer is not valid");
    return idx;
}

RmbAlignBatch *rminibwa_align_mut_from_sexp(SEXP x) {
    if (TYPEOF(x) != EXTPTRSXP) Rf_error("expected a minibwa alignment batch external pointer");
    RmbAlignBatch *batch = (RmbAlignBatch *) R_ExternalPtrAddr(x);
    if (batch == NULL) Rf_error("minibwa alignment batch pointer is not valid");
    return batch;
}

static void idx_finalizer(SEXP xptr) {
    RmbIndex *idx = (RmbIndex *) R_ExternalPtrAddr(xptr);
    if (idx != NULL) {
        if (idx->ptr != NULL) mb_idx_destroy(idx->ptr);
        R_Free(idx);
        R_ClearExternalPtr(xptr);
    }
}

static void align_batch_free(RmbAlignBatch *x) {
    if (x == NULL) return;
    if (x->index_ref != R_NilValue) R_ReleaseObject(x->index_ref);
    R_Free(x->read_index);
    R_Free(x->read_length);
    R_Free(x->read_fragment_id);
    R_Free(x->read_mate);
    R_Free(x->read_hit_offset);
    R_Free(x->read_hit_count);
    R_Free(x->read_id);
    R_Free(x->target_id);
    R_Free(x->query_length);
    R_Free(x->query_start);
    R_Free(x->query_end);
    R_Free(x->target_length);
    R_Free(x->target_start);
    R_Free(x->target_end);
    R_Free(x->strand);
    R_Free(x->mapq);
    R_Free(x->score);
    R_Free(x->matches);
    R_Free(x->block_length);
    R_Free(x->flags);
    R_Free(x->n_sub);
    R_Free(x->cigar_offset);
    R_Free(x->cigar_n);
    R_Free(x->cigar_words);
    R_Free(x);
}

static void align_finalizer(SEXP xptr) {
    RmbAlignBatch *batch = (RmbAlignBatch *) R_ExternalPtrAddr(xptr);
    if (batch != NULL) {
        align_batch_free(batch);
        R_ClearExternalPtr(xptr);
    }
}

static RmbFastxIter *fastx_iter_from_sexp(SEXP x) {
    if (TYPEOF(x) != EXTPTRSXP) Rf_error("expected a minibwa FASTX iterator external pointer");
    RmbFastxIter *iter = (RmbFastxIter *) R_ExternalPtrAddr(x);
    if (iter == NULL) Rf_error("minibwa FASTX iterator pointer is not valid");
    return iter;
}

static RmbFastxBatch *fastx_batch_from_sexp(SEXP x) {
    if (TYPEOF(x) != EXTPTRSXP) Rf_error("expected a minibwa FASTX batch external pointer");
    RmbFastxBatch *batch = (RmbFastxBatch *) R_ExternalPtrAddr(x);
    if (batch == NULL) Rf_error("minibwa FASTX batch pointer is not valid");
    return batch;
}

static void fastx_iter_finalizer(SEXP xptr) {
    RmbFastxIter *iter = (RmbFastxIter *) R_ExternalPtrAddr(xptr);
    if (iter != NULL) {
        if (iter->fp != NULL) {
            for (int i = 0; i < iter->n_fp; ++i) if (iter->fp[i] != NULL) mb_bseq_close(iter->fp[i]);
            R_Free(iter->fp);
        }
        R_Free(iter);
        R_ClearExternalPtr(xptr);
    }
}

static void fastx_batch_free(RmbFastxBatch *batch) {
    if (batch == NULL) return;
    if (batch->seq != NULL) {
        for (int32_t i = 0; i < batch->n; ++i) {
            free(batch->seq[i].name);
            free(batch->seq[i].seq);
            free(batch->seq[i].qual);
            free(batch->seq[i].comment);
        }
        free(batch->seq);
    }
    R_Free(batch->read_id);
    R_Free(batch->fragment_id);
    R_Free(batch->mate);
    R_Free(batch);
}

static void fastx_batch_finalizer(SEXP xptr) {
    RmbFastxBatch *batch = (RmbFastxBatch *) R_ExternalPtrAddr(xptr);
    if (batch != NULL) {
        fastx_batch_free(batch);
        R_ClearExternalPtr(xptr);
    }
}

static SEXP set_class(SEXP x, const char *cls) {
    SEXP klass = PROTECT(Rf_mkString(cls));
    Rf_setAttrib(x, R_ClassSymbol, klass);
    UNPROTECT(1);
    return x;
}

SEXP rminibwa_align_xptr_new(RmbAlignBatch *batch) {
    SEXP xptr = PROTECT(R_MakeExternalPtr(batch, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(xptr, align_finalizer, TRUE);
    set_class(xptr, "rminibwa_align");
    UNPROTECT(1);
    return xptr;
}

static SEXP fastx_iter_xptr_new(RmbFastxIter *iter) {
    SEXP xptr = PROTECT(R_MakeExternalPtr(iter, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(xptr, fastx_iter_finalizer, TRUE);
    set_class(xptr, "rminibwa_fastx_iter");
    UNPROTECT(1);
    return xptr;
}

static SEXP fastx_batch_xptr_new(RmbFastxBatch *batch) {
    SEXP xptr = PROTECT(R_MakeExternalPtr(batch, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(xptr, fastx_batch_finalizer, TRUE);
    set_class(xptr, "rminibwa_fastx_batch");
    UNPROTECT(1);
    return xptr;
}

static RmbAlignBatch *align_batch_alloc(size_t n, size_t n_cigar_words, size_t n_reads, SEXP index_ref) {
    RmbAlignBatch *x = R_Calloc(1, RmbAlignBatch);
    x->n = n;
    x->n_reads = n_reads;
    x->index_ref = index_ref;
    R_PreserveObject(index_ref);
#define ALLOC_N_I32(field, len) x->field = (len) ? R_Calloc((len), int32_t) : NULL
#define ALLOC_I32(field) ALLOC_N_I32(field, n)
#define ALLOC_I64(field) x->field = n ? R_Calloc(n, int64_t) : NULL
    ALLOC_N_I32(read_index, n_reads);
    ALLOC_N_I32(read_length, n_reads);
    ALLOC_N_I32(read_fragment_id, n_reads);
    ALLOC_N_I32(read_mate, n_reads);
    ALLOC_N_I32(read_hit_offset, n_reads);
    ALLOC_N_I32(read_hit_count, n_reads);
    ALLOC_I32(read_id);
    ALLOC_I32(target_id);
    ALLOC_I32(query_length);
    ALLOC_I32(query_start);
    ALLOC_I32(query_end);
    ALLOC_I64(target_length);
    ALLOC_I64(target_start);
    ALLOC_I64(target_end);
    ALLOC_I32(strand);
    ALLOC_I32(mapq);
    ALLOC_I32(score);
    ALLOC_I32(matches);
    ALLOC_I32(block_length);
    ALLOC_I32(flags);
    ALLOC_I32(n_sub);
    ALLOC_I32(cigar_offset);
    ALLOC_I32(cigar_n);
#undef ALLOC_N_I32
#undef ALLOC_I32
#undef ALLOC_I64
    x->n_cigar_words = n_cigar_words;
    x->cigar_words = n_cigar_words ? R_Calloc(n_cigar_words, uint32_t) : NULL;
    return x;
}

static void free_hits(mb_hit_t *hit, int32_t n_hit) {
    if (hit == NULL) return;
    for (int32_t i = 0; i < n_hit; ++i) {
        if (hit[i].p != NULL) free(hit[i].p);
    }
    free(hit);
}

static void free_hit_batch(mb_hit_t **hit, const int32_t *n_hit, int32_t n_seq) {
    if (hit == NULL) return;
    for (int32_t i = 0; i < n_seq; ++i) free_hits(hit[i], n_hit ? n_hit[i] : 0);
    free(hit);
}

static int32_t r_xlen_to_i32(R_xlen_t n, const char *what) {
    if (n < 0 || n > INT_MAX) Rf_error("%s exceeds minibwa's int32 length limit", what);
    return (int32_t) n;
}

static const char *charsxp_bytes(SEXP chr, int32_t *len, const char *what) {
    if (chr == NA_STRING) Rf_error("%s contains NA", what);
    *len = (int32_t) LENGTH(chr);
    return CHAR(chr);
}

static const char *sequence_bytes_one(SEXP x, int32_t *len, const char *what) {
    if (TYPEOF(x) == RAWSXP) {
        *len = r_xlen_to_i32(XLENGTH(x), what);
        return (const char *) RAW(x);
    }
    if (TYPEOF(x) == STRSXP && XLENGTH(x) == 1) return charsxp_bytes(STRING_ELT(x, 0), len, what);
    Rf_error("%s must contain raw vectors or character scalars", what);
    return NULL;
}

static void fill_sequence_views(SEXP x, const char ***seq_out, int32_t **qlen_out, int32_t *n_seq_out) {
    if (TYPEOF(x) != STRSXP && TYPEOF(x) != VECSXP) Rf_error("x must be a character vector or list");
    int32_t n_seq = r_xlen_to_i32(XLENGTH(x), "x");
    if (n_seq <= 0) Rf_error("x must not be empty");
    const char **seq = R_Calloc(n_seq, const char *);
    int32_t *qlen = R_Calloc(n_seq, int32_t);
    for (int32_t i = 0; i < n_seq; ++i) {
        if (TYPEOF(x) == STRSXP) seq[i] = charsxp_bytes(STRING_ELT(x, i), &qlen[i], "x");
        else seq[i] = sequence_bytes_one(VECTOR_ELT(x, i), &qlen[i], "x");
        if (qlen[i] <= 0) Rf_error("x contains an empty sequence");
    }
    *seq_out = seq;
    *qlen_out = qlen;
    *n_seq_out = n_seq;
}

static const char *name_bytes_one(SEXP x, char **owned, const char *what) {
    int32_t len = 0;
    if (TYPEOF(x) == STRSXP && XLENGTH(x) == 1) return charsxp_bytes(STRING_ELT(x, 0), &len, what);
    if (TYPEOF(x) != RAWSXP) Rf_error("%s must contain raw vectors or character scalars", what);
    len = r_xlen_to_i32(XLENGTH(x), what);
    char *buf = R_Calloc((size_t) len + 1, char);
    if (len > 0) memcpy(buf, RAW(x), (size_t) len);
    buf[len] = '\0';
    *owned = buf;
    return buf;
}

static void fill_name_views(SEXP name_x, int32_t n_seq, const char ***qname_out, char ***owned_out) {
    *qname_out = NULL;
    *owned_out = NULL;
    if (name_x == R_NilValue) return;
    if (TYPEOF(name_x) != STRSXP && TYPEOF(name_x) != VECSXP) Rf_error("name must be a character vector or list");
    if (XLENGTH(name_x) != n_seq) Rf_error("name must have the same length as x");
    const char **qname = R_Calloc(n_seq, const char *);
    char **owned = R_Calloc(n_seq, char *);
    for (int32_t i = 0; i < n_seq; ++i) {
        int32_t len = 0;
        if (TYPEOF(name_x) == STRSXP) qname[i] = charsxp_bytes(STRING_ELT(name_x, i), &len, "name");
        else qname[i] = name_bytes_one(VECTOR_ELT(name_x, i), &owned[i], "name");
    }
    *qname_out = qname;
    *owned_out = owned;
}

static void free_name_views(const char **qname, char **owned, int32_t n_seq) {
    (void) qname;
    if (owned != NULL) {
        for (int32_t i = 0; i < n_seq; ++i) R_Free(owned[i]);
        R_Free(owned);
    }
    if (qname != NULL) R_Free(qname);
}

static int hit_flags(const mb_hit_t *h) {
    int flags = 0;
    if (h->proper_pair) flags |= RMB_FLAG_PROPER;
    if (h->parent != h->id) {
        flags |= RMB_FLAG_SECONDARY;
    } else if (!h->sam_pri) {
        flags |= RMB_FLAG_SUPPLEMENTARY;
    } else {
        flags |= RMB_FLAG_PRIMARY;
    }
    return flags;
}

static void fill_alignment_row(RmbAlignBatch *out, size_t row, size_t *off, RmbIndex *idx, const mb_hit_t *h, int32_t read_id, int32_t qlen) {
    out->read_id[row] = read_id;
    out->target_id[row] = (int32_t) h->tid;
    out->query_length[row] = qlen;
    out->query_start[row] = h->qs;
    out->query_end[row] = h->qe;
    out->target_length[row] = mb_idx_ctg_len(idx->ptr, (int32_t) h->tid);
    out->target_start[row] = h->ts;
    out->target_end[row] = h->te;
    out->strand[row] = h->rev ? 1 : 0;
    out->mapq[row] = h->mapq;
    out->score[row] = h->score;
    out->matches[row] = h->mlen;
    out->block_length[row] = h->blen;
    out->flags[row] = hit_flags(h);
    out->n_sub[row] = h->n_sub;
    out->cigar_offset[row] = (int32_t) *off;
    out->cigar_n[row] = 0;
    if (h->p != NULL && h->p->n_cigar > 0) {
        int32_t nc = h->p->n_cigar;
        out->cigar_n[row] = nc;
        for (int32_t j = 0; j < nc; ++j) out->cigar_words[*off + (size_t) j] = h->p->cigar[j];
        *off += (size_t) nc;
    }
}

static RmbAlignBatch *copy_hits_to_batch(mb_hit_t *hit, int32_t n_hit, int32_t qlen, SEXP index_ref) {
    RmbIndex *idx = rminibwa_index_from_sexp(index_ref);
    size_t n_cigar = 0;
    for (int32_t i = 0; i < n_hit; ++i) {
        if (hit[i].p != NULL && hit[i].p->n_cigar > 0) n_cigar += (size_t) hit[i].p->n_cigar;
    }
    RmbAlignBatch *out = align_batch_alloc((size_t) n_hit, n_cigar, 1, index_ref);
    out->read_index[0] = 0;
    out->read_length[0] = qlen;
    out->read_fragment_id[0] = 0;
    out->read_mate[0] = 0;
    out->read_hit_offset[0] = 0;
    out->read_hit_count[0] = n_hit;
    size_t off = 0;
    for (int32_t i = 0; i < n_hit; ++i) fill_alignment_row(out, (size_t) i, &off, idx, &hit[i], 0, qlen);
    return out;
}

static RmbAlignBatch *copy_hit_batch_to_batch(mb_hit_t **hit, const int32_t *n_hit, const int32_t *qlen, int32_t n_seq, int paired, const int32_t *read_id, const int32_t *fragment_id, const int32_t *mate, SEXP index_ref) {
    RmbIndex *idx = rminibwa_index_from_sexp(index_ref);
    size_t n_total = 0, n_cigar = 0;
    for (int32_t i = 0; i < n_seq; ++i) {
        n_total += (size_t) n_hit[i];
        for (int32_t j = 0; j < n_hit[i]; ++j) {
            if (hit[i][j].p != NULL && hit[i][j].p->n_cigar > 0) n_cigar += (size_t) hit[i][j].p->n_cigar;
        }
    }
    RmbAlignBatch *out = align_batch_alloc(n_total, n_cigar, (size_t) n_seq, index_ref);
    size_t row = 0, off = 0;
    for (int32_t i = 0; i < n_seq; ++i) {
        int32_t rid = read_id ? read_id[i] : i;
        out->read_index[i] = rid;
        out->read_length[i] = qlen[i];
        out->read_fragment_id[i] = fragment_id ? fragment_id[i] : (paired ? i / 2 : i);
        out->read_mate[i] = mate ? mate[i] : (paired ? (i & 1) + 1 : 0);
        out->read_hit_offset[i] = (int32_t) row;
        out->read_hit_count[i] = n_hit[i];
        for (int32_t j = 0; j < n_hit[i]; ++j) fill_alignment_row(out, row++, &off, idx, &hit[i][j], rid, qlen[i]);
    }
    return out;
}

static int opt_list_index(SEXP names, const char *key) {
    if (names == R_NilValue) return -1;
    for (R_xlen_t i = 0; i < XLENGTH(names); ++i) {
        SEXP nm = STRING_ELT(names, i);
        if (nm != NA_STRING && strcmp(CHAR(nm), key) == 0) return (int) i;
    }
    return -1;
}

static SEXP opt_value(SEXP opts, const char *key) {
    if (opts == R_NilValue) return R_NilValue;
    if (TYPEOF(opts) != VECSXP) Rf_error("opt must be a list returned by mb_opts()");
    int idx = opt_list_index(Rf_getAttrib(opts, R_NamesSymbol), key);
    return idx >= 0 ? VECTOR_ELT(opts, idx) : R_NilValue;
}

void rminibwa_apply_options(SEXP opts_x, mb_opt_t *opt) {
    mb_opt_init(opt);
    SEXP preset_x = opt_value(opts_x, "preset");
    if (preset_x != R_NilValue) {
        const char *preset = chr_scalar(preset_x, "opt$preset");
        if (mb_opt_preset(opt, preset) != 0) Rf_error("unknown minibwa preset: %s", preset);
    }
    SEXP paired_x = opt_value(opts_x, "paired");
    if (is_true_scalar(paired_x)) opt->flag |= MB_F_PE;
    else if (is_false_scalar(paired_x)) opt->flag &= ~MB_F_PE;
    SEXP methylation_x = opt_value(opts_x, "methylation");
    if (is_true_scalar(methylation_x)) opt->flag |= MB_F_METH;
    else if (is_false_scalar(methylation_x)) opt->flag &= ~MB_F_METH;

#define SET_INT_FIELD(key, field) do { \
    SEXP v__ = opt_value(opts_x, key); \
    if (v__ != R_NilValue) opt->field = int_scalar(v__, "opt$" key, opt->field); \
} while (0)
    SET_INT_FIELD("out_n", out_n);
    SET_INT_FIELD("min_seed_len", min_len);
    SET_INT_FIELD("threads", n_thread);
    SET_INT_FIELD("match_score", a);
    SET_INT_FIELD("mismatch_penalty", b);
    SET_INT_FIELD("gap_open", q);
    SET_INT_FIELD("gap_extend", e);
#undef SET_INT_FIELD
    if (opt->n_thread < 1) opt->n_thread = 1;
}

SEXP RC_mb_index_load(SEXP prefix_x, SEXP meth_x) {
    const char *prefix = chr_scalar(prefix_x, "prefix");
    int meth = is_true_scalar(meth_x);
    mb_idx_t *ptr = mb_idx_load(prefix, meth);
    if (ptr == NULL) Rf_error("failed to load minibwa index at prefix: %s", prefix);
    RmbIndex *idx = R_Calloc(1, RmbIndex);
    idx->ptr = ptr;
    idx->meth = meth;
    SEXP xptr = PROTECT(R_MakeExternalPtr(idx, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(xptr, idx_finalizer, TRUE);
    set_class(xptr, "rminibwa_idx");
    UNPROTECT(1);
    return xptr;
}

SEXP RC_mb_index_build(SEXP fasta_x, SEXP prefix_x, SEXP meth_x, SEXP threads_x, SEXP low_memory_x) {
    const char *fasta = chr_scalar(fasta_x, "fasta");
    const char *prefix = chr_scalar(prefix_x, "prefix");
    int threads = int_scalar(threads_x, "threads", 1);
    if (threads < 1) Rf_error("threads must be positive");
    char threads_buf[32];
    snprintf(threads_buf, sizeof threads_buf, "%d", threads);
    char *argv[9];
    int argc = 0;
    argv[argc++] = "index";
    argv[argc++] = "-t";
    argv[argc++] = threads_buf;
    if (is_true_scalar(low_memory_x)) argv[argc++] = "-l";
    if (is_true_scalar(meth_x)) argv[argc++] = "--meth";
    argv[argc++] = (char *) fasta;
    argv[argc++] = (char *) prefix;
    argv[argc] = NULL;
    int rc = main_index(argc, argv);
    if (rc != 0) Rf_error("minibwa index build failed with status %d", rc);
    return Rf_ScalarLogical(TRUE);
}

SEXP RC_mb_index_contigs(SEXP index_x) {
    RmbIndex *idx = rminibwa_index_from_sexp(index_x);
    int32_t n = 0;
    while (mb_idx_ctg_name(idx->ptr, n) != NULL) ++n;
    SEXP names = PROTECT(Rf_allocVector(VECSXP, n));
    SEXP lengths = PROTECT(Rf_allocVector(REALSXP, n));
    for (int32_t i = 0; i < n; ++i) {
        const char *nm = mb_idx_ctg_name(idx->ptr, i);
        size_t len = strlen(nm);
        SEXP raw = PROTECT(Rf_allocVector(RAWSXP, (R_xlen_t) len));
        memcpy(RAW(raw), nm, len);
        SET_VECTOR_ELT(names, i, raw);
        UNPROTECT(1);
        REAL(lengths)[i] = (double) mb_idx_ctg_len(idx->ptr, i);
    }
    SEXP out = PROTECT(Rf_allocVector(VECSXP, 2));
    SET_VECTOR_ELT(out, 0, names);
    SET_VECTOR_ELT(out, 1, lengths);
    SEXP out_names = PROTECT(Rf_allocVector(STRSXP, 2));
    SET_STRING_ELT(out_names, 0, Rf_mkChar("name"));
    SET_STRING_ELT(out_names, 1, Rf_mkChar("length"));
    Rf_setAttrib(out, R_NamesSymbol, out_names);
    UNPROTECT(4);
    return out;
}

static const char *meth_name_to_code(SEXP meth_x, int *code) {
    const char *meth = chr_scalar(meth_x, "meth");
    if (strcmp(meth, "none") == 0) { *code = 0; return meth; }
    if (strcmp(meth, "c2t") == 0) { *code = 1; return meth; }
    if (strcmp(meth, "g2a") == 0) { *code = 2; return meth; }
    Rf_error("meth must be one of 'none', 'c2t', or 'g2a'");
    return meth;
}

static const char *map_name_from_sexp(SEXP name_x, char **name_buf) {
    const char *qname = "read0";
    *name_buf = NULL;
    if (name_x != R_NilValue) {
        if (TYPEOF(name_x) == RAWSXP) {
            R_xlen_t nl = XLENGTH(name_x);
            *name_buf = R_Calloc((size_t) nl + 1, char);
            memcpy(*name_buf, RAW(name_x), (size_t) nl);
            (*name_buf)[nl] = '\0';
            qname = *name_buf;
        } else if (TYPEOF(name_x) == STRSXP) {
            qname = chr_scalar(name_x, "name");
        } else {
            Rf_error("name must be NULL, raw, or a non-missing character scalar");
        }
    }
    return qname;
}

SEXP RC_mb_map_raw(SEXP x, SEXP index_x, SEXP opt_x, SEXP name_x, SEXP meth_x) {
    if (TYPEOF(x) != RAWSXP) Rf_error("x must be a raw vector of sequence bytes");
    R_xlen_t n = XLENGTH(x);
    if (n <= 0) Rf_error("x must not be empty");
    if (n > INT_MAX) Rf_error("x is longer than minibwa's int32 query length limit");
    RmbIndex *idx = rminibwa_index_from_sexp(index_x);
    mb_opt_t opt;
    rminibwa_apply_options(opt_x, &opt);
    int meth = 0;
    meth_name_to_code(meth_x, &meth);

    char *name_buf = NULL;
    const char *qname = map_name_from_sexp(name_x, &name_buf);
    int32_t n_hit = 0;
    mb_hit_t *hit = mb_map(&opt, idx->ptr, (int32_t) n, (const char *) RAW(x), meth, &n_hit, NULL, qname);
    if (name_buf != NULL) R_Free(name_buf);
    RmbAlignBatch *batch = copy_hits_to_batch(hit, n_hit, (int32_t) n, index_x);
    free_hits(hit, n_hit);
    return rminibwa_align_xptr_new(batch);
}

SEXP RC_mb_map_batch(SEXP x, SEXP index_x, SEXP opt_x, SEXP name_x) {
    const char **seq = NULL, **qname = NULL;
    char **qname_owned = NULL;
    int32_t *qlen = NULL, *n_hit = NULL;
    int32_t n_seq = 0;
    fill_sequence_views(x, &seq, &qlen, &n_seq);
    fill_name_views(name_x, n_seq, &qname, &qname_owned);

    RmbIndex *idx = rminibwa_index_from_sexp(index_x);
    mb_opt_t opt;
    rminibwa_apply_options(opt_x, &opt);
    if (opt_value(opt_x, "paired") == R_NilValue) opt.flag &= ~MB_F_PE;
    n_hit = R_Calloc(n_seq, int32_t);
    mb_hit_t **hit = mb_map_batch(&opt, idx->ptr, n_seq, qlen, seq, n_hit, NULL, qname);
    RmbAlignBatch *batch = copy_hit_batch_to_batch(hit, n_hit, qlen, n_seq, !!(opt.flag & MB_F_PE), NULL, NULL, NULL, index_x);
    free_hit_batch(hit, n_hit, n_seq);
    free_name_views(qname, qname_owned, n_seq);
    R_Free(n_hit);
    R_Free(qlen);
    R_Free(seq);
    return rminibwa_align_xptr_new(batch);
}

SEXP RC_mb_fastx_iter(SEXP path_x, SEXP mode_x, SEXP chunk_size_x, SEXP max_chunk_size_x, SEXP with_qual_x, SEXP with_comment_x) {
    if (TYPEOF(path_x) != STRSXP || XLENGTH(path_x) < 1 || XLENGTH(path_x) > 2) {
        Rf_error("path must be a character vector of length one or two");
    }
    int n_fp = (int) XLENGTH(path_x);
    int mode = int_scalar(mode_x, "mode", 0);
    if (mode < 0 || mode > 2) Rf_error("invalid FASTX pairing mode");
    if (mode == 2 && n_fp != 2) Rf_error("two-file FASTX mode requires two paths");
    if (mode != 2 && n_fp != 1) Rf_error("single-file FASTX modes require one path");
    int chunk_size = int_scalar(chunk_size_x, "batch_bases", 100000000);
    int max_chunk_size = int_scalar(max_chunk_size_x, "max_batch_bases", chunk_size);
    if (chunk_size <= 0) Rf_error("batch_bases must be positive");
    if (max_chunk_size < chunk_size) max_chunk_size = chunk_size;

    RmbFastxIter *iter = R_Calloc(1, RmbFastxIter);
    iter->n_fp = n_fp;
    iter->chunk_size = chunk_size;
    iter->max_chunk_size = max_chunk_size;
    iter->with_qual = is_true_scalar(with_qual_x);
    iter->with_comment = is_true_scalar(with_comment_x);
    iter->paired = mode != 0;
    iter->fp = R_Calloc(n_fp, mb_bseq_file_t *);
    for (int i = 0; i < n_fp; ++i) {
        SEXP s = STRING_ELT(path_x, i);
        if (s == NA_STRING) Rf_error("path must not contain NA");
        iter->fp[i] = mb_bseq_open(CHAR(s));
        if (iter->fp[i] == NULL) Rf_error("failed to open FASTX file: %s", CHAR(s));
    }
    return fastx_iter_xptr_new(iter);
}

static void fastx_batch_assign_unpaired(RmbFastxIter *iter, RmbFastxBatch *batch) {
    for (int32_t i = 0; i < batch->n; ++i) {
        if (iter->next_read_id > INT_MAX || iter->next_fragment_id > INT_MAX) Rf_error("FASTX read IDs exceed int32 range");
        batch->read_id[i] = (int32_t) iter->next_read_id++;
        batch->fragment_id[i] = (int32_t) iter->next_fragment_id++;
        batch->mate[i] = 0;
        batch->seq[i].id = (uint64_t) batch->read_id[i];
    }
}

static void fastx_batch_assign_two_file(RmbFastxIter *iter, RmbFastxBatch *batch) {
    for (int32_t i = 0; i < batch->n; ++i) {
        if (iter->next_read_id > INT_MAX || iter->next_fragment_id > INT_MAX) Rf_error("FASTX read IDs exceed int32 range");
        batch->read_id[i] = (int32_t) iter->next_read_id++;
        batch->fragment_id[i] = (int32_t) (iter->next_fragment_id + i / 2);
        batch->mate[i] = (i & 1) + 1;
        batch->seq[i].id = (uint64_t) batch->read_id[i];
    }
    iter->next_fragment_id += (batch->n + 1) / 2;
}

static void fastx_batch_assign_by_name(RmbFastxIter *iter, RmbFastxBatch *batch) {
    int32_t i = 0;
    while (i < batch->n) {
        int32_t cnt = 1;
        if (i + 1 < batch->n && mb_qname_same(batch->seq[i].name, batch->seq[i + 1].name)) cnt = 2;
        if (i + 2 < batch->n && mb_qname_same(batch->seq[i].name, batch->seq[i + 2].name)) {
            Rf_error("more than two adjacent FASTX records share the same query name");
        }
        if (iter->next_fragment_id > INT_MAX) Rf_error("FASTX fragment IDs exceed int32 range");
        int32_t frag = (int32_t) iter->next_fragment_id++;
        for (int32_t j = 0; j < cnt; ++j) {
            if (iter->next_read_id > INT_MAX) Rf_error("FASTX read IDs exceed int32 range");
            batch->read_id[i + j] = (int32_t) iter->next_read_id++;
            batch->fragment_id[i + j] = frag;
            batch->mate[i + j] = cnt == 2 ? j + 1 : 0;
            batch->seq[i + j].id = (uint64_t) batch->read_id[i + j];
        }
        i += cnt;
    }
}

SEXP RC_mb_fastx_next(SEXP iter_x) {
    RmbFastxIter *iter = fastx_iter_from_sexp(iter_x);
    int n = 0;
    mb_bseq1_t *seq = NULL;
    if (iter->n_fp > 1) {
        seq = mb_bseq_read_frag(iter->n_fp, iter->fp, iter->chunk_size, iter->with_qual, iter->with_comment, &n);
    } else {
        seq = mb_bseq_read(iter->fp[0], iter->chunk_size, iter->with_qual, iter->with_comment, iter->paired, 40000, iter->max_chunk_size, &n);
    }
    if (seq == NULL || n <= 0) return R_NilValue;
    if (n > INT_MAX) Rf_error("FASTX batch has too many records");
    RmbFastxBatch *batch = R_Calloc(1, RmbFastxBatch);
    batch->n = (int32_t) n;
    batch->paired = iter->paired;
    batch->seq = seq;
    batch->read_id = R_Calloc((size_t) n, int32_t);
    batch->fragment_id = R_Calloc((size_t) n, int32_t);
    batch->mate = R_Calloc((size_t) n, int32_t);
    if (!iter->paired) fastx_batch_assign_unpaired(iter, batch);
    else if (iter->n_fp > 1) fastx_batch_assign_two_file(iter, batch);
    else fastx_batch_assign_by_name(iter, batch);
    return fastx_batch_xptr_new(batch);
}

SEXP RC_mb_fastx_n(SEXP batch_x) {
    RmbFastxBatch *batch = fastx_batch_from_sexp(batch_x);
    return Rf_ScalarInteger(batch->n);
}

SEXP RC_mb_map_fastx_batch(SEXP batch_x, SEXP index_x, SEXP opt_x) {
    RmbFastxBatch *fq = fastx_batch_from_sexp(batch_x);
    if (fq->n <= 0) Rf_error("FASTX batch is empty");
    const char **seq = R_Calloc(fq->n, const char *), **qname = R_Calloc(fq->n, const char *);
    int32_t *qlen = R_Calloc(fq->n, int32_t), *n_hit = R_Calloc(fq->n, int32_t);
    for (int32_t i = 0; i < fq->n; ++i) {
        if (fq->seq[i].l_seq == 0 || fq->seq[i].l_seq > INT_MAX) Rf_error("FASTX sequence length is outside minibwa's int32 range");
        qlen[i] = (int32_t) fq->seq[i].l_seq;
        seq[i] = fq->seq[i].seq;
        qname[i] = fq->seq[i].name;
    }
    RmbIndex *idx = rminibwa_index_from_sexp(index_x);
    mb_opt_t opt;
    rminibwa_apply_options(opt_x, &opt);
    SEXP paired_x = opt_value(opt_x, "paired");
    if (fq->paired) opt.flag |= MB_F_PE;
    else if (paired_x == R_NilValue || is_false_scalar(paired_x)) opt.flag &= ~MB_F_PE;
    mb_hit_t **hit = mb_map_batch(&opt, idx->ptr, fq->n, qlen, seq, n_hit, NULL, qname);
    RmbAlignBatch *batch = copy_hit_batch_to_batch(
        hit, n_hit, qlen, fq->n, !!(opt.flag & MB_F_PE), fq->read_id,
        fq->paired ? fq->fragment_id : NULL, fq->paired ? fq->mate : NULL, index_x
    );
    free_hit_batch(hit, n_hit, fq->n);
    R_Free(n_hit);
    R_Free(qlen);
    R_Free(qname);
    R_Free(seq);
    return rminibwa_align_xptr_new(batch);
}

SEXP RC_mb_map_count_raw(SEXP x, SEXP index_x, SEXP opt_x, SEXP name_x, SEXP meth_x) {
    if (TYPEOF(x) != RAWSXP) Rf_error("x must be a raw vector of sequence bytes");
    R_xlen_t n = XLENGTH(x);
    if (n <= 0) Rf_error("x must not be empty");
    if (n > INT_MAX) Rf_error("x is longer than minibwa's int32 query length limit");
    RmbIndex *idx = rminibwa_index_from_sexp(index_x);
    mb_opt_t opt;
    rminibwa_apply_options(opt_x, &opt);
    int meth = 0;
    meth_name_to_code(meth_x, &meth);

    char *name_buf = NULL;
    const char *qname = map_name_from_sexp(name_x, &name_buf);
    int32_t n_hit = 0;
    mb_hit_t *hit = mb_map(&opt, idx->ptr, (int32_t) n, (const char *) RAW(x), meth, &n_hit, NULL, qname);
    if (name_buf != NULL) R_Free(name_buf);
    free_hits(hit, n_hit);
    return Rf_ScalarInteger(n_hit);
}

SEXP RC_mb_align_n(SEXP batch_x) {
    RmbAlignBatch *batch = rminibwa_align_mut_from_sexp(batch_x);
    return Rf_ScalarReal((double) batch->n);
}

SEXP RC_mb_align_read_n(SEXP batch_x) {
    RmbAlignBatch *batch = rminibwa_align_mut_from_sexp(batch_x);
    return Rf_ScalarReal((double) batch->n_reads);
}

static int is_i64_col(const char *name) {
    return strcmp(name, "ts") == 0 || strcmp(name, "te") == 0 || strcmp(name, "tlen") == 0 ||
           strcmp(name, "target_start") == 0 || strcmp(name, "target_end") == 0 || strcmp(name, "target_length") == 0;
}

SEXP RC_mb_align_col(SEXP batch_x, SEXP name_x) {
    const char *name = chr_scalar(name_x, "name");
    if (is_i64_col(name)) return rminibwa_align_altreal(batch_x, name);
    return rminibwa_align_altinteger(batch_x, name);
}

SEXP RC_mb_align_read_col(SEXP batch_x, SEXP name_x) {
    const char *name = chr_scalar(name_x, "name");
    return rminibwa_align_read_altinteger(batch_x, name);
}

SEXP RC_mb_align_cigar_words(SEXP batch_x) {
    RmbAlignBatch *batch = rminibwa_align_mut_from_sexp(batch_x);
    R_xlen_t nbytes = (R_xlen_t) (batch->n_cigar_words * sizeof(uint32_t));
    SEXP out = PROTECT(Rf_allocVector(RAWSXP, nbytes));
    if (nbytes > 0) memcpy(RAW(out), batch->cigar_words, (size_t) nbytes);
    UNPROTECT(1);
    return out;
}

const RmbAlignBatch *Rminibwa_align_from_sexp(SEXP x) {
    return rminibwa_align_mut_from_sexp(x);
}

size_t Rminibwa_align_n(const RmbAlignBatch *x) {
    return x ? x->n : 0;
}

size_t Rminibwa_align_n_read(const RmbAlignBatch *x) {
    return x ? x->n_reads : 0;
}

const int32_t *Rminibwa_align_read_i32_col(const RmbAlignBatch *x, const char *name) {
    if (x == NULL || name == NULL) return NULL;
    if (strcmp(name, "id") == 0 || strcmp(name, "read_id") == 0 || strcmp(name, "read_index") == 0) return x->read_index;
    if (strcmp(name, "length") == 0 || strcmp(name, "qlen") == 0 || strcmp(name, "query_length") == 0) return x->read_length;
    if (strcmp(name, "fragment_id") == 0 || strcmp(name, "frag") == 0) return x->read_fragment_id;
    if (strcmp(name, "mate") == 0) return x->read_mate;
    if (strcmp(name, "hit_offset") == 0 || strcmp(name, "alignment_offset") == 0) return x->read_hit_offset;
    if (strcmp(name, "hit_count") == 0 || strcmp(name, "n_hit") == 0 || strcmp(name, "alignment_count") == 0) return x->read_hit_count;
    return NULL;
}

const int32_t *Rminibwa_align_i32_col(const RmbAlignBatch *x, const char *name) {
    if (x == NULL || name == NULL) return NULL;
    if (strcmp(name, "read") == 0 || strcmp(name, "read_id") == 0) return x->read_id;
    if (strcmp(name, "tid") == 0 || strcmp(name, "target_id") == 0) return x->target_id;
    if (strcmp(name, "qlen") == 0 || strcmp(name, "query_length") == 0) return x->query_length;
    if (strcmp(name, "qs") == 0 || strcmp(name, "query_start") == 0) return x->query_start;
    if (strcmp(name, "qe") == 0 || strcmp(name, "query_end") == 0) return x->query_end;
    if (strcmp(name, "strand") == 0) return x->strand;
    if (strcmp(name, "mapq") == 0) return x->mapq;
    if (strcmp(name, "score") == 0) return x->score;
    if (strcmp(name, "matches") == 0) return x->matches;
    if (strcmp(name, "block_length") == 0 || strcmp(name, "blen") == 0) return x->block_length;
    if (strcmp(name, "flags") == 0) return x->flags;
    if (strcmp(name, "n_sub") == 0) return x->n_sub;
    if (strcmp(name, "cigar_offset") == 0) return x->cigar_offset;
    if (strcmp(name, "cigar_n") == 0) return x->cigar_n;
    return NULL;
}

const int64_t *Rminibwa_align_i64_col(const RmbAlignBatch *x, const char *name) {
    if (x == NULL || name == NULL) return NULL;
    if (strcmp(name, "tlen") == 0 || strcmp(name, "target_length") == 0) return x->target_length;
    if (strcmp(name, "ts") == 0 || strcmp(name, "target_start") == 0) return x->target_start;
    if (strcmp(name, "te") == 0 || strcmp(name, "target_end") == 0) return x->target_end;
    return NULL;
}

const uint32_t *Rminibwa_align_cigar_words(const RmbAlignBatch *x, size_t *n_words) {
    if (n_words != NULL) *n_words = x ? x->n_cigar_words : 0;
    return x ? x->cigar_words : NULL;
}

const int32_t *Rminibwa_align_cigar_i32_col(const RmbAlignBatch *x, const char *name) {
    if (x == NULL || name == NULL) return NULL;
    if (strcmp(name, "cigar_offset") == 0) return x->cigar_offset;
    if (strcmp(name, "cigar_n") == 0) return x->cigar_n;
    return NULL;
}
