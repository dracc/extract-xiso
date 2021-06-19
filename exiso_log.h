#ifndef __EXISO_LOG
#define __EXISO_LOG

#include <cerrno>

#define exiso_log  /*if (! s_quiet) printf*/
#define flush()    /*if (! s_quiet) fflush(stdout)*/

int log_err(const char *in_file, int in_line, const char *in_format, ...);
void usage(char *argv[]);

#define mem_err()                      { log_err(__FILE__, __LINE__, "out of memory error\n"); err = 1; }
#define read_err()                     { log_err(__FILE__, __LINE__, "read error: %s\n", strerror(errno)); err = 1; }
#define seek_err()                     { log_err(__FILE__, __LINE__, "seek error: %s\n", strerror(errno)); err = 1; }
#define write_err()                    { log_err(__FILE__, __LINE__, "write error: %s\n", strerror(errno)); err = 1; }
#define rread_err()                    { log_err(__FILE__, __LINE__, "unable to read remote file\n"); err = 1; }
#define rwrite_err()                   { log_err(__FILE__, __LINE__, "unable to write to remote file\n"); err = 1; }
#define unknown_err()                  { log_err(__FILE__, __LINE__, "an unrecoverable error has occurred\n"); err = 1; }
#define open_err(in_file)              { log_err(__FILE__, __LINE__, "open error: %s %s\n", (in_file), strerror(errno)); err = 1; }
#define chdir_err(in_dir)              { log_err(__FILE__, __LINE__, "unable to change to directory %s: %s\n", (in_dir), strerror(errno)); err = 1; }
#define mkdir_err(in_dir)              { log_err(__FILE__, __LINE__, "unable to create directory %s: %s\n", (in_dir), strerror(errno)); err = 1; }
#define ropen_err(in_file)             { log_err(__FILE__, __LINE__, "unable to open remote file %s\n", (in_file)); err = 1; }
#define rchdir_err(in_dir)             { log_err(__FILE__, __LINE__, "unable to change to remote directory %s\n", (in_dir)); err = 1; }
#define rmkdir_err(in_dir)             { log_err(__FILE__, __LINE__, "unable to create remote directory %s\n", (in_dir)); err = 1; }
#define misc_err(in_format, a, b, c)   { log_err(__FILE__, __LINE__, (in_format), (a), (b), (c)); err = 1; }

#endif
