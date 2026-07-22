#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include "rminibwa_internal.h"

SEXP RC_mb_index_load(SEXP prefix_x, SEXP meth_x);
SEXP RC_mb_index_build(SEXP fasta_x, SEXP prefix_x, SEXP meth_x, SEXP threads_x, SEXP low_memory_x);
SEXP RC_mb_index_contigs(SEXP index_x);
SEXP RC_mb_map_raw(SEXP x, SEXP index_x, SEXP opt_x, SEXP name_x, SEXP meth_x);
SEXP RC_mb_map_batch(SEXP x, SEXP index_x, SEXP opt_x, SEXP name_x);
SEXP RC_mb_fastx_iter(SEXP path_x, SEXP mode_x, SEXP chunk_size_x, SEXP max_chunk_size_x, SEXP with_qual_x, SEXP with_comment_x);
SEXP RC_mb_fastx_next(SEXP iter_x);
SEXP RC_mb_fastx_n(SEXP batch_x);
SEXP RC_mb_map_fastx_batch(SEXP batch_x, SEXP index_x, SEXP opt_x);
SEXP RC_mb_map_count_raw(SEXP x, SEXP index_x, SEXP opt_x, SEXP name_x, SEXP meth_x);
SEXP RC_mb_align_n(SEXP batch_x);
SEXP RC_mb_align_read_n(SEXP batch_x);
SEXP RC_mb_align_col(SEXP batch_x, SEXP name_x);
SEXP RC_mb_align_read_col(SEXP batch_x, SEXP name_x);
SEXP RC_mb_align_cigar_words(SEXP batch_x);
SEXP RC_mb_query_stream_open(SEXP path_x, SEXP mode_x, SEXP index_x, SEXP opt_x, SEXP threads_x, SEXP read_group_x, SEXP max_seen_x);
SEXP RC_mb_query_stream_next(SEXP stream_x);
SEXP RC_mb_query_stream_cancel(SEXP stream_x);
SEXP RC_mb_query_stream_error(SEXP stream_x);
SEXP RC_mb_query_group_n_reads(SEXP group_x);
SEXP RC_mb_query_group_n_records(SEXP group_x);
SEXP RC_mb_query_group_name(SEXP group_x);
SEXP RC_mb_query_group_input_order(SEXP group_x);
SEXP RC_simd_set_backend(SEXP backend_s);
SEXP RC_simd_backend(void);
SEXP RC_simd_counters(SEXP reset_s);
SEXP RC_simd_info(void);

static const R_CallMethodDef call_methods[] = {
    {"RC_mb_index_load",        (DL_FUNC) &RC_mb_index_load,        2},
    {"RC_mb_index_build",       (DL_FUNC) &RC_mb_index_build,       5},
    {"RC_mb_index_contigs",     (DL_FUNC) &RC_mb_index_contigs,     1},
    {"RC_mb_map_raw",           (DL_FUNC) &RC_mb_map_raw,           5},
    {"RC_mb_map_batch",         (DL_FUNC) &RC_mb_map_batch,         4},
    {"RC_mb_fastx_iter",        (DL_FUNC) &RC_mb_fastx_iter,        6},
    {"RC_mb_fastx_next",        (DL_FUNC) &RC_mb_fastx_next,        1},
    {"RC_mb_fastx_n",           (DL_FUNC) &RC_mb_fastx_n,           1},
    {"RC_mb_map_fastx_batch",   (DL_FUNC) &RC_mb_map_fastx_batch,   3},
    {"RC_mb_map_count_raw",     (DL_FUNC) &RC_mb_map_count_raw,     5},
    {"RC_mb_align_n",           (DL_FUNC) &RC_mb_align_n,           1},
    {"RC_mb_align_read_n",      (DL_FUNC) &RC_mb_align_read_n,      1},
    {"RC_mb_align_col",         (DL_FUNC) &RC_mb_align_col,         2},
    {"RC_mb_align_read_col",    (DL_FUNC) &RC_mb_align_read_col,    2},
    {"RC_mb_align_cigar_words",      (DL_FUNC) &RC_mb_align_cigar_words,      1},
    {"RC_mb_query_stream_open",      (DL_FUNC) &RC_mb_query_stream_open,      7},
    {"RC_mb_query_stream_next",      (DL_FUNC) &RC_mb_query_stream_next,      1},
    {"RC_mb_query_stream_cancel",    (DL_FUNC) &RC_mb_query_stream_cancel,    1},
    {"RC_mb_query_stream_error",     (DL_FUNC) &RC_mb_query_stream_error,     1},
    {"RC_mb_query_group_n_reads",    (DL_FUNC) &RC_mb_query_group_n_reads,    1},
    {"RC_mb_query_group_n_records",  (DL_FUNC) &RC_mb_query_group_n_records,  1},
    {"RC_mb_query_group_name",       (DL_FUNC) &RC_mb_query_group_name,       1},
    {"RC_mb_query_group_input_order", (DL_FUNC) &RC_mb_query_group_input_order, 1},
    {"RC_simd_set_backend",          (DL_FUNC) &RC_simd_set_backend,          1},
    {"RC_simd_backend",          (DL_FUNC) &RC_simd_backend,          0},
    {"RC_simd_counters",         (DL_FUNC) &RC_simd_counters,         1},
    {"RC_simd_info",             (DL_FUNC) &RC_simd_info,             0},
    {NULL, NULL, 0}
};

void R_init_Rminibwa(DllInfo *dll) {
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
    rminibwa_init_altrep(dll);
    rminibwa_stream_init();
    rminibwa_ksw_init_dispatch();

    R_RegisterCCallable("Rminibwa", "Rminibwa_align_from_sexp",       (DL_FUNC) Rminibwa_align_from_sexp);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_n",               (DL_FUNC) Rminibwa_align_n);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_n_read",          (DL_FUNC) Rminibwa_align_n_read);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_read_i32_col",    (DL_FUNC) Rminibwa_align_read_i32_col);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_i32_col",         (DL_FUNC) Rminibwa_align_i32_col);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_i64_col",         (DL_FUNC) Rminibwa_align_i64_col);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_cigar_words",     (DL_FUNC) Rminibwa_align_cigar_words);
    R_RegisterCCallable("Rminibwa", "Rminibwa_align_cigar_i32_col",   (DL_FUNC) Rminibwa_align_cigar_i32_col);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_abi_version",     (DL_FUNC) Rminibwa_stream_abi_version);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_from_sexp",       (DL_FUNC) Rminibwa_stream_from_sexp);
    R_RegisterCCallable("Rminibwa", "Rminibwa_query_group_from_sexp",  (DL_FUNC) Rminibwa_query_group_from_sexp);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_next",            (DL_FUNC) Rminibwa_stream_next);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_cancel",          (DL_FUNC) Rminibwa_stream_cancel);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_header",          (DL_FUNC) Rminibwa_stream_header);
    R_RegisterCCallable("Rminibwa", "Rminibwa_query_group_view",       (DL_FUNC) Rminibwa_query_group_view);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_error_code",      (DL_FUNC) Rminibwa_stream_error_code);
    R_RegisterCCallable("Rminibwa", "Rminibwa_stream_error_message",   (DL_FUNC) Rminibwa_stream_error_message);
}
