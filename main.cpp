#include "exiso_xiso.h"
#include "exiso_log.h"
#include "exiso_platform.h"
#include "boyer_moore.h"
#include <cstddef>
#include <stdarg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

static bool   s_media_enable = true;
static bool   s_quiet = true;
static bool   s_real_quiet = true;
static bool   s_remove_systemupdate = true;
static char   *s_copy_buffer;
static xoff_t s_total_bytes = 0;
static int    s_total_files = 0;
static xoff_t s_total_bytes_all_isos = 0;
static int    s_total_files_all_isos = 0;
static bool   s_warned = 0;


int main( int argc, char **argv) {
    struct stat sb;
    create_list *create = NULL, *p, *q, **r;
    int i, fd, opt_char, err = 0, isos = 0;
    bool extract = true, rewrite = false, free_user = false, free_pass = false;
    bool x_seen = false, delete_original = false, optimized;
    char *cwd = NULL, *path = NULL, *buf = NULL, *new_iso_path = NULL, tag[XISO_OPTIMIZED_TAG_LENGTH * sizeof(long)];

    if (argc < 2) {
        usage(argv);
        std::exit(1);
    }

    while (!err && (opt_char = getopt(argc, argv, GETOPT_STRING)) != -1) {
        switch (opt_char) {
        case 'c': {
            if (x_seen || rewrite || !extract) {
                usage(argv);
                std::exit(1);
            }

            for (r = &create; *r != NULL; r = &(*r)->next) ;

            if ((*r = (create_list *)malloc(sizeof(create_list))) == NULL) {
                mem_err();
            }
            if (!err) {
                (*r)->name = NULL;
                (*r)->next = NULL;

                if (((*r)->path = strdup(optarg)) == NULL) {
                    mem_err();
                }
            }
            if (!err &&
                argv[optind] && *argv[optind] != '-' && *argv[optind] &&
                ((*r)->name = strdup(argv[optind++])) == NULL) {
                mem_err();
            }
        } break;

        case 'd': {
            if (path) free(path);
            if ((path = strdup(optarg)) == NULL) {
                mem_err();
            }
        } break;

        case 'D': {
            delete_original = true;
        } break;

        case 'h': {
            usage(argv);
            std::exit(0);
        } break;

        case 'l': {
            if (x_seen || rewrite || create) {
                usage(argv);
                std::exit(1);
            }
            extract = false;
        } break;

        case 'm': {
            if (x_seen || !extract) {
                usage(argv);
                std::exit(1);
            }
            s_media_enable = false;
        } break;

        case 'q': {
            s_quiet = true;
        } break;

        case 'Q': {
            s_quiet = s_real_quiet = true;
        } break;

        case 'r': {
            if (x_seen || !extract || create) {
                usage(argv);
                std::exit(1);
            }
            rewrite = true;
        } break;

        case 's': {
            s_remove_systemupdate = true;
        } break;

        case 'v': {
            printf("%s", banner);
            std::exit(0);
        } break;

        case 'x': {
            if (!extract || rewrite || create) {
                usage(argv);
                std::exit(1);
            }
            x_seen = true;
        } break;

        default: {
            usage(argv);
            std::exit(1);
        } break;
        }
    }

    if (!err) {
        if (create) {
            if (optind < argc) {
                usage(argv);
                std::exit(1);
            }
        }
        else if (optind >= argc) {
            usage(argv);
            std::exit(1);
        }

        exiso_log("%s", banner);

        if ((extract) && (s_copy_buffer = (char *)malloc(READWRITE_BUFFER_SIZE)) == NULL) {
            mem_err();
        }
    }

    if (!err && (create || rewrite)) {
        err = boyer_moore_init(XISO_MEDIA_ENABLE, XISO_MEDIA_ENABLE_LENGTH, k_default_alphabet_size);
    }

    if (!err && create) {
        for (p = create; !err && p != NULL;) {
            char *tmp = NULL;

            if (p->name) {
                for (i = (int)strlen(p->name); i >= 0 && p->name[i] != PATH_CHAR; --i);
                ++i;

                if (i) {
                    if ((tmp = (char *)malloc(i + 1)) == NULL) {
                        mem_err();
                    }
                    if (!err) {
                        strncpy(tmp, p->name, i);
                        tmp[i] = 0;
                    }
                }
            }

            if (!err) {
                err = create_xiso(p->path, tmp, NULL, -1, NULL,
                                  p->name ? p->name + i : NULL, NULL, s_media_enable);
            }

            if (tmp) {
                free(tmp);
            }

            q = p->next;

            if (p->name) {
                free(p->name);
            }
            free(p->path);
            free(p);

            p = q;
        }
    } else {
        for (i = optind; !err && i < argc; ++i) {
            ++isos;
            exiso_log("\n");
            s_total_bytes = s_total_files = 0;


            if (!err) {
                optimized = false;

                if ((fd = open(argv[i], READFLAGS, 0)) == -1) {
                    open_err(argv[i]);
                }
                if (!err && lseek(fd, (xoff_t) XISO_OPTIMIZED_TAG_OFFSET, SEEK_SET) == -1) {
                    seek_err();
                }
                if (!err && read(fd, tag, XISO_OPTIMIZED_TAG_LENGTH) != XISO_OPTIMIZED_TAG_LENGTH) {
                    read_err();
                }

                if (fd != -1) close(fd);

                if (!err) {
                    tag[XISO_OPTIMIZED_TAG_LENGTH] = 0;

                    if (!strncmp(tag, XISO_OPTIMIZED_TAG, XISO_OPTIMIZED_TAG_LENGTH_MIN)) {
                        optimized = true;
                    }

                    if (rewrite) {
                        if (optimized) {
                            exiso_log("%s is already optimized, skipping...\n", argv[i]);
                            continue;
                        }

                        if (!err && (buf = (char *) malloc(strlen(argv[i]) + 5)) == NULL) { // + 5 magic number is for ".old\0"
                            mem_err();
                        }
                        if (!err) {
                            sprintf(buf, "%s.old", argv[i]);
                            if (stat(buf, &sb) != -1) {
                                misc_err("%s already exists, cannot rewrite %s\n", buf, argv[i], 0);
                            }
                            if (!err && rename(argv[i], buf) == -1) {
                                misc_err("cannot rename %s to %s\n", argv[i], buf, 0);
                            }

                            if (err) {
                                err = 0;
                                free(buf);
                                continue;
                            }
                        }
                        if (!err) {
                            err = decode_xiso(buf, path, k_rewrite, &new_iso_path, true);
                        }
                        if (!err && delete_original && unlink(buf) == -1) {
                            log_err(__FILE__, __LINE__, "unable to delete %s\n", buf);
                        }

                        if (buf) {
                            free(buf);
                        }
                    } else {
                        /* the order of the mutually exclusive options here is important,
                         * the extract ? k_extract : k_list test *must* be the final comparison */
                        if (!err) {
                            err = decode_xiso(argv[i], path,
                                              extract ? k_extract : k_list,
                                              NULL, !optimized);
                        }
                    }
                }
            }
        }

        if (!err) {
            exiso_log("\n%u files in %s total %lld bytes\n", s_total_files,
                      rewrite ? new_iso_path : argv[i],
                      (long long int)s_total_bytes);
        }

        if (new_iso_path) {
            if (!err) exiso_log("\n%s successfully rewritten%s%s\n", argv[i],
                                path ? " as " : ".",
                                path ? new_iso_path : "");

            free(new_iso_path);
            new_iso_path = NULL;
        }

        if (err == err_iso_no_files) {
            err = 0;
        }
    }

    if (!err && isos > 1) {
        exiso_log("\n%u files in %u xiso's total %lld bytes\n",
                  s_total_files_all_isos, isos, (long long int)
                  s_total_bytes_all_isos);
    }
    if (s_warned) {
        exiso_log("\nWARNING:  Warning(s) were issued during execution--review stderr!\n");
    }

    boyer_moore_done();

    if (s_copy_buffer) {
        free(s_copy_buffer);
    }
    if (path) {
        free(path);
    }

    return err;
}
