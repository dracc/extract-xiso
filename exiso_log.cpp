#include "exiso_log.h"

#include <cstdio>
#include <cstdarg>

static bool s_real_quiet = false;
#define banner            "extract-xiso - written by in <in@fishtank.com>\n"

int log_err(const char *in_file, int in_line, const char *in_format, ...) {
    va_list       ap;
    char         *format;
    int           ret;

#ifdef DEBUG
    asprintf(&format, "%s:%u %s", in_file, in_line, in_format);
#else
    format = (char *) in_format;
#endif
  
    if (s_real_quiet) {
        ret = 0;
    }
    else {
        va_start(ap, in_format);
        ret = vfprintf(stderr, format, ap);
        va_end(ap);
    }

#ifdef DEBUG
    free(format);
#endif

    return ret;
}

void usage(char *argv[]) {
  fprintf(stderr,
"%s\n\
  Usage:\n\
\n\
    %s [options] [-[lrx]] <file1.xiso> [file2.xiso] ...\n\
    %s [options] -c <dir> [name] [-c <dir> [name]] ...\n\
\n\
  Mutually exclusive modes:\n\
\n\
    -c <dir> [name] Create xiso from file(s) starting in <dir>.  If the\n\
        [name] parameter is specified, the xiso will be\n\
        created with the (path and) name given, otherwise\n\
        the xiso will be created in the current directory\n\
        with the name <dir>.iso.  The -c option may be\n\
        specified multiple times to create multiple xiso\n\
        images.\n\
    -l      List files in xiso(s).\n\
    -r      Rewrite xiso(s) as optimized xiso(s).\n\
    -x      Extract xiso(s) (the default mode if none is given).\n\
        If no directory is specified with -d, a directory\n\
        with the name of the xiso (minus the .iso portion)\n\
        will be created in the current directory and the\n\
        xiso will be expanded there.\n\
\n\
  Options:\n\
\n\
    -d <directory>  In extract mode, expand xiso in <directory>.\n\
      In rewrite mode, rewrite xiso in <directory>.\n\
    -D      In rewrite mode, delete old xiso after processing.\n\
    -h      Print this help text and exit.\n\
    -m      In create or rewrite mode, disable automatic .xbe\n\
        media enable patching (not recommended).\n\
    -q      Run quiet (suppress all non-error output).\n\
    -Q      Run silent (suppress all output).\n\
    -s      Skip $SystemUpdate folder.\n\
    -v      Print version information and exit.\n\
", banner, argv[0], argv[0]);
}
