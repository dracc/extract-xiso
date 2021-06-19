#ifndef __EXISO_XISO
#define __EXISO_XISO

#include <cstdint>
#include <sys/stat.h>
#include <sys/types.h>
#include "exiso_platform.h"
#include "exiso_types.h"


#define exiso_version     "2.7.1 (01.11.14)"
#define VERSION_LENGTH    16
#define banner            "extract-xiso v" exiso_version " for " exiso_target " - written by in <in@fishtank.com>\n"


#define GLOBAL_LSEEK_OFFSET 0x0FD90000ul
#define XGD3_LSEEK_OFFSET   0x02080000ul
#define XGD1_LSEEK_OFFSET   0x18300000ul


#define n_sectors(size)     ((size) / XISO_SECTOR_SIZE + ((size) % XISO_SECTOR_SIZE ? 1 : 0))

#define XISO_HEADER_DATA              "MICROSOFT*XBOX*MEDIA"
#define XISO_HEADER_DATA_LENGTH       20
#define XISO_HEADER_OFFSET            0x10000ul

#define XISO_FILE_MODULUS             0x10000
#define XISO_ROOT_DIRECTORY_SECTOR    0x108

#define XISO_OPTIMIZED_TAG_OFFSET     31337
#define XISO_OPTIMIZED_TAG            "in!xiso!" exiso_version
#define XISO_OPTIMIZED_TAG_LENGTH     (8 + VERSION_LENGTH)
#define XISO_OPTIMIZED_TAG_LENGTH_MIN 7

#define XISO_ATTRIBUTES_SIZE          1
#define XISO_FILENAME_LENGTH_SIZE     1
#define XISO_TABLE_OFFSET_SIZE        2
#define XISO_SECTOR_OFFSET_SIZE       4
#define XISO_DIRTABLE_SIZE            4
#define XISO_FILESIZE_SIZE            4
#define XISO_DWORD_SIZE               4
#define XISO_FILETIME_SIZE            8

#define XISO_SECTOR_SIZE              2048ul
#define XISO_UNUSED_SIZE              0x7c8

#define XISO_FILENAME_OFFSET          14
#define XISO_FILENAME_LENGTH_OFFSET   (XISO_FILENAME_OFFSET - 1)
#define XISO_FILENAME_MAX_CHARS       255

#define XISO_ATTRIBUTE_RO             0x01
#define XISO_ATTRIBUTE_HID            0x02
#define XISO_ATTRIBUTE_SYS            0x04
#define XISO_ATTRIBUTE_DIR            0x10
#define XISO_ATTRIBUTE_ARC            0x20
#define XISO_ATTRIBUTE_NOR            0x80

#define XISO_PAD_BYTE                 0xff
#define XISO_PAD_SHORT                0xffff

#define XISO_MEDIA_ENABLE             "\xe8\xca\xfd\xff\xff\x85\xc0\x7d"
#define XISO_MEDIA_ENABLE_BYTE        '\xeb'
#define XISO_MEDIA_ENABLE_LENGTH      8
#define XISO_MEDIA_ENABLE_BYTE_POS    7

#define EMPTY_SUBDIRECTORY            ((dir_node_avl *) 1)
#define READWRITE_BUFFER_SIZE         0x00200000ul
#define DEBUG_DUMP_DIRECTORY          "/Volumes/c/xbox/iso/exiso"
#define GETOPT_STRING                 "c:d:Dhlmp:qQrsvx"

int decode_xiso(char *in_xiso, char *in_path, modes in_mode, char **out_iso_path, bool in_ll_compat);
int verify_xiso(int in_xiso, int32_t *out_root_dir_sector, int32_t *out_root_dir_size, char *in_iso_name);
int traverse_xiso(int in_xiso, dir_node *in_dir_node, xoff_t in_dir_start,
                  char *in_path, modes in_mode, dir_node_avl **in_root, bool in_ll_compat);
int create_xiso(char *in_root_directory, char *in_output_directory,
                dir_node_avl *in_root, int in_xiso, char **out_iso_path,
                char *in_name, progress_callback in_progress_callback, bool media_enable);

int extract_file(int in_xiso, dir_node *in_file, modes in_mode, char *path);
int write_tree(dir_node_avl *in_avl, write_tree_context *in_context, int in_depth, bool media_enable);
int write_file(dir_node_avl *in_avl, write_tree_context *in_context, int in_depth, bool media_enable);
int write_directory(dir_node_avl *in_avl, int in_xiso, int in_depth);

int calculate_total_files_and_bytes(dir_node_avl *in_avl, void *in_context, int in_depth);
int calculate_directory_size(dir_node_avl *in_avl, unsigned long *out_size, long in_depth);
int calculate_directory_requirements(dir_node_avl *in_avl, void *in_context, int in_depth);
int calculate_directory_offsets(dir_node_avl *in_avl, unsigned long *io_context, int in_depth);

int write_dir_start_and_file_positions(dir_node_avl *in_avl, wdsafp_context *io_context, int in_depth);
int write_volume_descriptors(int in_xiso, unsigned long in_total_sectors);

FILE_TIME *alloc_filetime_now(void);

#endif
