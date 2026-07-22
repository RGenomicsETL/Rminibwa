#define _Complex
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <R.h>
#include <Rinternals.h>
#include <Rminibwa.h>

static int clamp_size_to_int(size_t x)
{
    return x > (size_t) INT_MAX ? INT_MAX : (int) x;
}

/* This example exercises the stream entirely through the installed C API: it
 * does not request a query-group R object or materialize any record in R. */
SEXP rminibwa_capi_stream_summary(SEXP stream_x)
{
    RmbQueryStream *stream;
    const RmbQueryGroup *group = NULL;
    Rminibwa_header_view header;
    Rminibwa_query_group_view_t view;
    int status;
    int has_rg = 0, has_mc = 0, has_mq = 0, has_mate_view = 0;
    uint32_t i, j;

    if (Rminibwa_stream_abi_version() != RMINIBWA_STREAM_ABI_VERSION) {
        Rf_error("Rminibwa stream ABI mismatch");
    }
    stream = Rminibwa_stream_from_sexp(stream_x);
    if (stream == NULL) Rf_error("expected a Rminibwa query stream");
    if (Rminibwa_stream_header(stream, &header) != RMINIBWA_STREAM_OK) {
        Rf_error("could not obtain Rminibwa stream header");
    }
    status = Rminibwa_stream_next(stream, &group);
    if (status != RMINIBWA_STREAM_OK || group == NULL) Rf_error("could not obtain first query group");
    if (Rminibwa_query_group_view(group, &view) != RMINIBWA_STREAM_OK) {
        Rf_error("could not obtain Rminibwa query-group view");
    }

    for (i = 0; i < view.n_records; ++i) {
        if (view.records[i].mate_cigar != NULL && view.records[i].mate_cigar_n > 0 &&
            view.records[i].mate_mapq >= 0) has_mate_view = 1;
        for (j = 0; j < view.records[i].n_tags; ++j) {
            const Rminibwa_tag_view *tag = &view.records[i].tags[j];
            if (tag->tag == RMINIBWA_TAG('R', 'G')) has_rg = 1;
            if (tag->tag == RMINIBWA_TAG('M', 'C')) has_mc = 1;
            if (tag->tag == RMINIBWA_TAG('M', 'Q')) has_mq = 1;
        }
    }

    SEXP out = PROTECT(Rf_allocVector(INTSXP, 13));
    INTEGER(out)[0] = (int) header.abi_version;
    INTEGER(out)[1] = clamp_size_to_int(header.read_group_record_length);
    INTEGER(out)[2] = (int) header.n_contigs;
    INTEGER(out)[3] = clamp_size_to_int(view.query_name_length);
    INTEGER(out)[4] = (int) view.n_reads;
    INTEGER(out)[5] = (int) view.n_records;
    INTEGER(out)[6] = view.n_records ? view.records[0].flag : NA_INTEGER;
    INTEGER(out)[7] = view.n_records ? (int) view.records[0].n_tags : NA_INTEGER;
    INTEGER(out)[8] = (int) header.sort_order;
    INTEGER(out)[9] = (int) header.group_order;
    INTEGER(out)[10] = has_rg;
    INTEGER(out)[11] = has_mc && has_mq;
    INTEGER(out)[12] = has_mate_view;

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 13));
    SET_STRING_ELT(names, 0, Rf_mkChar("abi"));
    SET_STRING_ELT(names, 1, Rf_mkChar("read_group_bytes"));
    SET_STRING_ELT(names, 2, Rf_mkChar("n_contigs"));
    SET_STRING_ELT(names, 3, Rf_mkChar("qname_bytes"));
    SET_STRING_ELT(names, 4, Rf_mkChar("n_reads"));
    SET_STRING_ELT(names, 5, Rf_mkChar("n_records"));
    SET_STRING_ELT(names, 6, Rf_mkChar("first_flag"));
    SET_STRING_ELT(names, 7, Rf_mkChar("first_n_tags"));
    SET_STRING_ELT(names, 8, Rf_mkChar("sort_order"));
    SET_STRING_ELT(names, 9, Rf_mkChar("group_order"));
    SET_STRING_ELT(names, 10, Rf_mkChar("has_rg_tag"));
    SET_STRING_ELT(names, 11, Rf_mkChar("has_mate_tags"));
    SET_STRING_ELT(names, 12, Rf_mkChar("has_mate_view"));
    Rf_setAttrib(out, R_NamesSymbol, names);
    UNPROTECT(2);
    return out;
}
