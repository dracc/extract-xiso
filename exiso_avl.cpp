#include "exiso_avl.h"

#include "exiso_xiso.h"
#include "exiso_log.h"
#include "exiso_types.h"
#include <cerrno>
#include <cstring>
#include <cstddef>
#include <cstdlib>

#if defined(__FREEBSD__) || defined(__OPENBSD__)
#include <machine/limits.h>
#endif

#if defined(_WIN32)
#include <direct.h>
#include "win32/dirent.c"
#else
#include <dirent.h>
#include <climits>
#include <unistd.h>
#endif



dir_node_avl *avl_fetch(dir_node_avl *in_root, char *in_filename) {
    int           result;

    for (;;) {
        if (in_root == NULL) return NULL;

        result = avl_compare_key(in_filename, in_root->filename);

        if (result < 0) {
            in_root = in_root->left;
        } else if (result > 0) {
            in_root = in_root->right;
        } else {
            return in_root;
        }
    }
}


avl_result avl_insert(dir_node_avl **in_root, dir_node_avl *in_node) {
    avl_result        tmp;
    int           result;

    if (*in_root == NULL) {
        *in_root = in_node;
        return k_avl_balanced;
    }

    result = avl_compare_key(in_node->filename, (*in_root)->filename);

    if (result < 0) {
        return (tmp = avl_insert(&(*in_root)->left, in_node)) == k_avl_balanced ? avl_left_grown(in_root) : tmp;
    }
    if (result > 0) {
        return (tmp = avl_insert(&(*in_root)->right, in_node)) == k_avl_balanced ? avl_right_grown(in_root) : tmp;
    }

    return k_avl_error;
}


avl_result avl_left_grown(dir_node_avl **in_root) {
    switch ((*in_root)->skew) {
    case k_left_skew:
        if ((*in_root)->left->skew == k_left_skew) {
            (*in_root)->skew = (*in_root)->left->skew = k_no_skew;
            avl_rotate_right(in_root);
        } else {
            switch ((*in_root)->left->right->skew) {
            case k_left_skew: {
                (*in_root)->skew = k_right_skew;
                (*in_root)->left->skew = k_no_skew;
            } break;

            case k_right_skew: {
                (*in_root)->skew = k_no_skew;
                (*in_root)->left->skew = k_left_skew;
            } break;

            default: {
                (*in_root)->skew = k_no_skew;
                (*in_root)->left->skew = k_no_skew;
            } break;
            }
            (*in_root)->left->right->skew = k_no_skew;
            avl_rotate_left(&(*in_root)->left);
            avl_rotate_right(in_root);
        }
        return no_err;

    case k_right_skew:
        (*in_root)->skew = k_no_skew;
        return no_err;

    default:
        (*in_root)->skew = k_left_skew;
        return k_avl_balanced;
    }
}


avl_result avl_right_grown(dir_node_avl **in_root) {
    switch ((*in_root)->skew) {
    case k_left_skew:
        (*in_root)->skew = k_no_skew;
        return no_err;

    case k_right_skew:
        if ((*in_root)->right->skew == k_right_skew) {
            (*in_root)->skew = (*in_root)->right->skew = k_no_skew;
            avl_rotate_left(in_root);
        } else {
            switch ((*in_root)->right->left->skew) {
            case k_left_skew: {
                (*in_root)->skew = k_no_skew;
                (*in_root)->right->skew = k_right_skew;
            } break;

            case k_right_skew: {
                (*in_root)->skew = k_left_skew;
                (*in_root)->right->skew = k_no_skew;
            } break;

            default: {
                (*in_root)->skew = k_no_skew;
                (*in_root)->right->skew = k_no_skew;
            } break;
            }
            (*in_root)->right->left->skew = k_no_skew;
            avl_rotate_right(&(*in_root)->right);
            avl_rotate_left(in_root);
        }
        return no_err;

    default:
        (*in_root)->skew = k_right_skew;
        return k_avl_balanced;
    }
}


void avl_rotate_left(dir_node_avl **in_root) {
    dir_node_avl *tmp = *in_root;

    *in_root = (*in_root)->right;
    tmp->right = (*in_root)->left;
    (*in_root)->left = tmp;
}


void avl_rotate_right(dir_node_avl **in_root) {
    dir_node_avl *tmp = *in_root;

    *in_root = (*in_root)->left;
    tmp->left = (*in_root)->right;
    (*in_root)->right = tmp;
}


int avl_compare_key(char *in_lhs, char *in_rhs) {
    char a, b;

    for (;;) {
        a = *in_lhs++;
        b = *in_rhs++;

        if (a >= 'a' && a <= 'z') {
            a -= 32;  // uppercase(a);
        }
        if (b >= 'a' && b <= 'z') {
            b -= 32;  // uppercase(b);
        }

        if (a) {
            if (b) {
                if (a < b) {
                    return -1;
                }
                if (a > b) {
                    return 1;
                }
            } else {
                return 1;
            }
        } else {
            return b ? -1 : 0;
        }
    }
}


int avl_traverse_depth_first(dir_node_avl *in_root, traversal_callback in_callback,
                             void *in_context, avl_traversal_method in_method, long in_depth,
                             bool media_enable) {
    int err;

    if (in_root == NULL) return 0;

    switch (in_method) {
    case k_prefix:
        err = (*in_callback)(in_root, in_context, in_depth);
        if (!err) {
            err = avl_traverse_depth_first(in_root->left, in_callback, in_context, in_method, in_depth + 1);
        }
        if (!err) {
            err = avl_traverse_depth_first(in_root->right, in_callback, in_context, in_method, in_depth + 1);
        }
        break;

    case k_infix:
        err = avl_traverse_depth_first(in_root->left, in_callback, in_context, in_method, in_depth + 1);
        if (!err) {
            err = (*in_callback)(in_root, in_context, in_depth);
        }
        if (!err) {
            err = avl_traverse_depth_first(in_root->right, in_callback, in_context, in_method, in_depth + 1);
        }
        break;

    case k_postfix:
        err = avl_traverse_depth_first(in_root->left, in_callback, in_context, in_method, in_depth + 1);
        if (!err) {
            err = avl_traverse_depth_first(in_root->right, in_callback, in_context, in_method, in_depth + 1);
        }
        if (!err) {
            err = (*in_callback)(in_root, in_context, in_depth);
        }
        break;

    default:
        err = 0;
        break;
    }

    return err;
}

int free_dir_node_avl(void *in_dir_node_avl, void *in_context, long in_depth) {
    dir_node_avl *avl = (dir_node_avl *) in_dir_node_avl;

    if (avl->subdirectory && avl->subdirectory != EMPTY_SUBDIRECTORY) {
        avl_traverse_depth_first(avl->subdirectory, free_dir_node_avl, NULL, k_postfix, 0);
    }

    free(avl->filename);
    free(avl);

    return 0;
}

int generate_avl_tree_local(dir_node_avl **out_root, int *io_n, int &s_total_files, xoff_t &s_total_bytes) {
    struct dirent      *p;
    struct stat         sb;
    dir_node_avl       *avl;
    DIR              *dir = NULL;
    int             err = 0, i, j;
    bool          empty_dir = true;

    if ((dir = opendir(".")) == NULL) {
        mem_err();
    }

    while (!err && (p = readdir(dir)) != NULL) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
            continue;
        }

        for (i = *io_n; i; --i) {
            exiso_log("\b");
        }
        exiso_log("%s", p->d_name);
        for (j = i = (int) strlen(p->d_name); j < *io_n; ++j) {
            exiso_log(" ");
        }
        for (j = i; j < *io_n; ++j) {
            exiso_log("\b");
        }
        *io_n = i;
        flush();

        if ((avl = (dir_node_avl *) malloc(sizeof(dir_node_avl))) == NULL) {
            mem_err();
        }
        if (!err) {
            memset(avl, 0, sizeof(dir_node_avl));
            if ((avl->filename = strdup(p->d_name)) == NULL) {
                mem_err();
            }
        }
        if (!err && stat(avl->filename, &sb) == -1) {
            read_err();
        }
        if (!err) {
            if (S_ISDIR(sb.st_mode)) {
                empty_dir = false;

                if (chdir(avl->filename) == -1) {
                    chdir_err(avl->filename);
                }

                if (!err) {
                    err = generate_avl_tree_local(&avl->subdirectory, io_n, s_total_files, s_total_bytes);
                }
                if (!err && chdir("..") == -1) {
                    chdir_err("..");
                }
            } else if (S_ISREG(sb.st_mode)) {
                empty_dir = false;
                if (sb.st_size > ULONG_MAX) {
                    log_err(__FILE__, __LINE__, "file %s is too large for xiso, skipping...\n", avl->filename);
                    free(avl->filename);
                    free(avl);
                    continue;
                }
                s_total_bytes += avl->file_size = (unsigned long) sb.st_size;
                ++s_total_files;
            } else {
                free(avl->filename);
                free(avl);
                continue;
            }
        }
        if (!err) {
            if (avl_insert(out_root, avl) == k_avl_error) {
                misc_err("error inserting file %s into tree (duplicate filename?)\n", avl->filename, 0, 0);
            }
        } else {
            if (avl) {
                if (avl->filename) {
                    free(avl->filename);
                }
                free(avl);
            }
        }
    }

    if (empty_dir) {
        *out_root = EMPTY_SUBDIRECTORY;
    }

    if (dir) closedir(dir);

    return err;
}
