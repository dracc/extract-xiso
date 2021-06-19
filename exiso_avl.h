#ifndef __EXISO_AVL
#define __EXISO_AVL

#include "exiso_types.h"

dir_node_avl *avl_fetch(dir_node_avl *in_root, char *in_filename);
avl_result avl_insert(dir_node_avl **in_root, dir_node_avl *in_node);
avl_result avl_left_grown(dir_node_avl **in_root);
avl_result avl_right_grown(dir_node_avl **in_root);
void avl_rotate_left(dir_node_avl **in_root);
void avl_rotate_right(dir_node_avl **in_root);
int avl_compare_key(char *in_lhs, char *in_rhs);
int avl_traverse_depth_first(dir_node_avl *in_root, traversal_callback in_callback,
                             void *in_context, avl_traversal_method in_method, long in_depth);

int free_dir_node_avl(void *in_dir_node_avl, void *, long);

int generate_avl_tree_local(dir_node_avl **out_root, int *io_n, int &s_total_files, xoff_t &s_total_bytes);
int generate_avl_tree_remote(dir_node_avl **out_root, int *io_n);

#endif
