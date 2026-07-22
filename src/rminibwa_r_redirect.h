#ifndef RMINIBWA_R_REDIRECT_H
#define RMINIBWA_R_REDIRECT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Print.h>

static FILE *rminibwa_stdout_sentinel = (FILE *) 0x1;
static FILE *rminibwa_stderr_sentinel = (FILE *) 0x2;

static inline int rminibwa_vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    if (stream == rminibwa_stdout_sentinel) {
        Rvprintf(fmt, ap);
        return 0;
    }
    if (stream == rminibwa_stderr_sentinel) {
        REvprintf(fmt, ap);
        return 0;
    }
    return vfprintf(stream, fmt, ap);
}

static inline int rminibwa_fprintf(FILE *stream, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = rminibwa_vfprintf(stream, fmt, ap);
    va_end(ap);
    return ret;
}

static inline int rminibwa_fputc(int c, FILE *stream)
{
    if (stream == rminibwa_stdout_sentinel) {
        Rprintf("%c", c);
        return c;
    }
    if (stream == rminibwa_stderr_sentinel) {
        REprintf("%c", c);
        return c;
    }
    return fputc(c, stream);
}

#ifdef stdout
#undef stdout
#endif
#ifdef stderr
#undef stderr
#endif
#define stdout rminibwa_stdout_sentinel
#define stderr rminibwa_stderr_sentinel
#define fprintf rminibwa_fprintf
#define fputc rminibwa_fputc
#define exit(status) Rf_error("minibwa attempted to terminate the R process with exit(%d)", (int)(status))
#define abort() Rf_error("minibwa attempted to abort the R process")

#endif
