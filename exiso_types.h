#ifndef __EXISO_TYPES
#define __EXISO_TYPES

#include <sys/stat.h>
#include <sys/types.h>
#include "exiso_platform.h"

typedef enum modes { k_generate_avl, k_extract, k_list, k_rewrite } modes;
typedef enum errors { err_end_of_sector = -5001, err_iso_rewritten = -5002, err_iso_no_files = -5003 } errors;

typedef void (*progress_callback)(xoff_t in_current_value, xoff_t in_final_value);
typedef int (*traversal_callback)(void *in_node, void *in_context, long in_depth);

typedef enum avl_skew { k_no_skew , k_left_skew , k_right_skew } avl_skew;
typedef enum avl_result { no_err, k_avl_error, k_avl_balanced } avl_result;
typedef enum avl_traversal_method { k_prefix, k_infix, k_postfix } avl_traversal_method;

typedef struct dir_node dir_node;
typedef struct create_list create_list;
typedef struct dir_node_avl dir_node_avl;

struct dir_node {
    dir_node                  *left;
    dir_node                  *parent;
    dir_node_avl              *avl_node;

    char                      *filename;

    unsigned short            r_offset;
    unsigned char             attributes;
    unsigned char             filename_length;
  
    unsigned long             file_size;
    unsigned long             start_sector;
};

struct dir_node_avl {
    unsigned long             offset;
    xoff_t                    dir_start;

    char                      *filename;
    unsigned long             file_size;
    unsigned long             start_sector;
    dir_node_avl              *subdirectory;
  
    unsigned long             old_start_sector;
  
    avl_skew                  skew;   
    dir_node_avl              *left;
    dir_node_avl              *right;
};

struct create_list {
    char                      *path;
    char                      *name;
    create_list               *next;
};

typedef enum bm_constants { k_default_alphabet_size = 256 } bm_constants;


typedef struct FILE_TIME {
    unsigned long             l;
    unsigned long             h;
} FILE_TIME;

typedef struct wdsafp_context {
    xoff_t                    dir_start;
    unsigned long             *current_sector;
} wdsafp_context;

typedef struct write_tree_context {
    int                       xiso;
    char                      *path;
    int                       from;
    progress_callback         progress;
    xoff_t                    final_bytes;
} write_tree_context;

#endif
