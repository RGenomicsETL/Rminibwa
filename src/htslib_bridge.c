#include <R.h>
#include <Rinternals.h>

#ifndef RMINIBWA_WITH_HTSLIB
#error "RMINIBWA_WITH_HTSLIB must be defined by configure"
#endif

#if RMINIBWA_WITH_HTSLIB
#include <htslib/hts.h>

SEXP RC_rminibwa_htslib_runtime(void)
{
    const char *version = hts_version();
    const char *feature_string = hts_feature_string();
    SEXP out, names;

    if (version == NULL || version[0] == '\0') {
        Rf_error("linked htslib returned an empty version");
    }
    if (feature_string == NULL) {
        feature_string = "";
    }

    PROTECT(out = Rf_allocVector(VECSXP, 3));
    PROTECT(names = Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(names, 0, Rf_mkChar("version"));
    SET_STRING_ELT(names, 1, Rf_mkChar("feature_bits"));
    SET_STRING_ELT(names, 2, Rf_mkChar("feature_string"));
    SET_VECTOR_ELT(out, 0, Rf_mkString(version));
    SET_VECTOR_ELT(out, 1, Rf_ScalarReal((double)hts_features()));
    SET_VECTOR_ELT(out, 2, Rf_mkString(feature_string));
    Rf_setAttrib(out, R_NamesSymbol, names);
    UNPROTECT(2);
    return out;
}
#else
SEXP RC_rminibwa_htslib_runtime(void)
{
    Rf_error(
        "Rminibwa's direct htslib adapter is unavailable in browser-wasm builds; "
        "rwasm configure can see only Rduckhts's host-native link artifacts"
    );
    return R_NilValue;
}
#endif
