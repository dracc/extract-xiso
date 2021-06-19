#include "exiso_xiso.h"

#include "boyer_moore.h"
#include "exiso_log.h"
#include "exiso_avl.h"
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include "win32/dirent.c"
#else
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#endif

static xoff_t s_xbox_disc_lseek = 0;
static xoff_t s_total_bytes = 0;
static int    s_total_files = 0;
static bool   s_remove_systemupdate = false; 
static char   *s_systemupdate = "$SystemUpdate";
static xoff_t s_total_bytes_all_isos = 0;
static int    s_total_files_all_isos = 0;
static char   *s_copy_buffer;

int verify_xiso(int in_xiso, int32_t *out_root_dir_sector, int32_t *out_root_dir_size, char *in_iso_name) {
    int  err = 0;
    char buffer[XISO_HEADER_DATA_LENGTH];

    if (lseek(in_xiso, (xoff_t) XISO_HEADER_OFFSET, SEEK_SET) == -1) {
        seek_err();
    }
    if (!err && read(in_xiso, buffer, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
        read_err();
    }
    if (!err && memcmp(buffer, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH)) 
    {
        if (lseek(in_xiso, (xoff_t) XISO_HEADER_OFFSET + GLOBAL_LSEEK_OFFSET, SEEK_SET) == -1) {
            seek_err();
        }
        if (!err && read(in_xiso, buffer, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
            read_err();
        }
        if (!err && memcmp(buffer, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH)) {
            if (lseek(in_xiso, (xoff_t) XISO_HEADER_OFFSET + XGD3_LSEEK_OFFSET, SEEK_SET) == -1) {
                seek_err();
            }
            if (!err && read(in_xiso, buffer, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
                read_err();
            }
            if (!err && memcmp(buffer, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH)) {
                if (lseek(in_xiso, (xoff_t)XISO_HEADER_OFFSET + XGD1_LSEEK_OFFSET, SEEK_SET) == -1) {
                    seek_err();
                }
                if (!err && read(in_xiso, buffer, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
                    read_err();
                }
                if (!err && memcmp(buffer, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH)) {
                    misc_err("%s does not appear to be a valid xbox iso image\n", in_iso_name, 0, 0);
                } else {
                    s_xbox_disc_lseek = XGD1_LSEEK_OFFSET;
                }
            } else {
                s_xbox_disc_lseek = XGD3_LSEEK_OFFSET;
            }
        } else {
            s_xbox_disc_lseek = GLOBAL_LSEEK_OFFSET;
        }
    } else {
        s_xbox_disc_lseek = 0;
    }

    // read root directory information
    if (!err && read(in_xiso, out_root_dir_sector, XISO_SECTOR_OFFSET_SIZE) != XISO_SECTOR_OFFSET_SIZE) {
        read_err();
    }
    if (!err && read(in_xiso, out_root_dir_size, XISO_DIRTABLE_SIZE) != XISO_DIRTABLE_SIZE) {
        read_err();
    }

    little32(*out_root_dir_sector);
    little32(*out_root_dir_size);
  
    // seek to header tail and verify media tag
    if (!err && lseek(in_xiso, (xoff_t) XISO_FILETIME_SIZE + XISO_UNUSED_SIZE, SEEK_CUR) == -1) {
        seek_err();
    }
    if (!err && read(in_xiso, buffer, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
        read_err();
    }
    if (!err && memcmp(buffer, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH)) {
        misc_err("%s appears to be corrupt\n", in_iso_name, 0, 0);
    }

    // seek to root directory sector
    if (!err) {
        if (!*out_root_dir_sector && !*out_root_dir_size) {
            exiso_log("xbox image %s contains no files.\n", in_iso_name);
            err = err_iso_no_files;
        } else {
            if (lseek(in_xiso, (xoff_t) *out_root_dir_sector * XISO_SECTOR_SIZE, SEEK_SET) == -1) {
                seek_err();
            }
        }
    }
  
    return err;
}




int create_xiso(char *in_root_directory, char *in_output_directory,
                dir_node_avl *in_root, int in_xiso, char **out_iso_path, char *in_name,
                progress_callback in_progress_callback, bool media_enable) {
    xoff_t pos;
    dir_node_avl root;
    FILE_TIME *ft = NULL;
    write_tree_context wt_context;
    unsigned long start_sector;
    int i, n, xiso = -1, err = 0;
    char *cwd = NULL, *buf = NULL, *iso_name, *xiso_path, *iso_dir;

    s_total_bytes = s_total_files = 0;

    memset(&root, 0, sizeof(dir_node_avl));

    if ((cwd = getcwd(NULL, 0)) == NULL) {
        mem_err();
    }
    if (!err) {
        if (!in_root) {
            if (chdir(in_root_directory) == -1) {
                chdir_err(in_root_directory);
            }
            if (!err) {
                if (in_root_directory[i = (int) strlen(in_root_directory) - 1] == '/' || in_root_directory[i] == '\\') {
                    in_root_directory[i--] = 0;
                }
                for (iso_dir = &in_root_directory[i]; iso_dir >= in_root_directory && *iso_dir != PATH_CHAR; --iso_dir) ;
                ++iso_dir;

                iso_name = in_name ? in_name : iso_dir;
            }
        } else {
            iso_dir = iso_name = in_root_directory;
        }
    }
    if (!err) {
        if (!*iso_dir) {
            iso_dir = PATH_CHAR_STR;
        }
        if (!in_output_directory) {
            in_output_directory = cwd;
        }
        if (in_output_directory[i = (int) strlen(in_output_directory) - 1] == PATH_CHAR) {
            in_output_directory[i--] = 0;
        }

        if (!iso_name || !*iso_name) {
            iso_name = "root";
        }
        else if (iso_name[1] == ':') {
            iso_name[1] = iso_name[0];
            ++iso_name;
        }

#if defined(_WIN32)
        if ((asprintf(&xiso_path, "%s%c%s%s",
                      *in_output_directory ? in_output_directory : cwd,
                      PATH_CHAR, iso_name, in_name ? "" : ".iso")) == -1) {
            mem_err();
        }
#else
        if ((asprintf(&xiso_path, "%s%s%s%c%s%s",
                      *in_output_directory == PATH_CHAR ? "" : cwd,
                      *in_output_directory == PATH_CHAR ? "" : PATH_CHAR_STR,
                      in_output_directory, PATH_CHAR, iso_name, in_name ? "" : ".iso")) == -1) {
            mem_err();
        }
#endif
    }
    if (!err) {
        exiso_log("%s %s%s:\n\n", in_root ? "rewriting" : "\ncreating", iso_name, in_name ? "" : ".iso");

        root.start_sector = XISO_ROOT_DIRECTORY_SECTOR;   

        s_total_bytes = s_total_files = 0;

        if (in_root) {
            root.subdirectory = in_root;
            avl_traverse_depth_first(in_root, (traversal_callback) calculate_total_files_and_bytes, NULL, k_prefix, 0);
        } else {
            int       i, n = 0;

            exiso_log("generating avl tree from %sfilesystem: ", ""); flush();
      
            err = generate_avl_tree_local(&root.subdirectory, &n, s_total_files, s_total_bytes);

            for (i = 0; i < n; ++i) {
                exiso_log("\b");
            }
            for (i = 0; i < n; ++i) {
                exiso_log(" ");
            }
            for (i = 0; i < n; ++i) {
                exiso_log("\b");
            }

            exiso_log("%s\n\n", err ? "failed!" : "[OK]");
        }
    }
    if (!err && in_progress_callback) {
        (*in_progress_callback)(0, s_total_bytes);
    }
    if (!err) {
        wt_context.final_bytes = s_total_bytes;
    
        s_total_bytes = s_total_files = 0;
    
        if (root.subdirectory == EMPTY_SUBDIRECTORY) {
            root.start_sector = root.file_size = 0;
        }
    
        start_sector = root.start_sector;
    
        avl_traverse_depth_first(&root, (traversal_callback) calculate_directory_requirements, NULL, k_prefix, 0);
        avl_traverse_depth_first(&root, (traversal_callback) calculate_directory_offsets, &start_sector, k_prefix, 0);
    }
    if (!err && (buf = (char *)malloc(n = std::max(READWRITE_BUFFER_SIZE, XISO_HEADER_OFFSET))) == NULL) {
        mem_err();
    }
    if (!err) {
        if ((xiso = open(xiso_path, WRITEFLAGS, 0644)) == -1) {
            open_err(xiso_path);
        }
        if (out_iso_path) {
            *out_iso_path = xiso_path;
        }
        else {
            free(xiso_path);
        }
    }
    if (!err) {
        memset(buf, 0, n);
        if (write(xiso, buf, XISO_HEADER_OFFSET) != XISO_HEADER_OFFSET) {
            write_err();
        }
    }
    if (!err && write(xiso, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
        write_err();
    }
    if (!err) {
        little32(root.start_sector);
        if (write(xiso, &root.start_sector, XISO_SECTOR_OFFSET_SIZE) != XISO_SECTOR_OFFSET_SIZE) {
            write_err();
        }
        little32(root.start_sector);
    }
    if (!err) {
        little32(root.file_size);
        if (write(xiso, &root.file_size, XISO_DIRTABLE_SIZE) != XISO_DIRTABLE_SIZE) {
            write_err();
        }
        little32(root.file_size);
    }
    if (!err) {
        if (in_root) {
            if (lseek(in_xiso, ((xoff_t)XISO_HEADER_OFFSET + XISO_HEADER_DATA_LENGTH +
                                XISO_SECTOR_OFFSET_SIZE + XISO_DIRTABLE_SIZE + s_xbox_disc_lseek),
                      SEEK_SET) == -1) {
                seek_err();
            }
            if (!err && read(in_xiso, buf, XISO_FILETIME_SIZE) != XISO_FILETIME_SIZE) {
                read_err();
            }
            if (!err && write(xiso, buf, XISO_FILETIME_SIZE) != XISO_FILETIME_SIZE) {
                write_err();
            }

            memset(buf, 0, XISO_FILETIME_SIZE);
        } else {
            if ((ft = alloc_filetime_now()) == NULL) {
                mem_err();
            }
            if (!err && write(xiso, ft, XISO_FILETIME_SIZE) != XISO_FILETIME_SIZE) {
                write_err();
            }
        }
    }
    if (!err && write(xiso, buf, XISO_UNUSED_SIZE) != XISO_UNUSED_SIZE) {
        write_err();
    }
    if (!err && write(xiso, XISO_HEADER_DATA, XISO_HEADER_DATA_LENGTH) != XISO_HEADER_DATA_LENGTH) {
        write_err();
    }
  
    if (!err && !in_root) {
        if (chdir("..") == -1) chdir_err("..");
    }
    if (!err && (root.filename = strdup(iso_dir)) == NULL) {
        mem_err();
    }

    if (!err && root.start_sector && lseek(xiso, (xoff_t) root.start_sector * XISO_SECTOR_SIZE, SEEK_SET) == -1) {
        seek_err();
    }
    if (!err) {
        wt_context.path = NULL;
        wt_context.xiso = xiso;
        wt_context.from = in_root ? in_xiso : -1;
        wt_context.progress = in_progress_callback;

        err = avl_traverse_depth_first(&root, (traversal_callback) write_tree, &wt_context, k_prefix, 0, media_enable);
    }

    if (!err && (pos = lseek(xiso, (xoff_t) 0, SEEK_END)) == -1) {
        seek_err();
    }
    if (!err && write(xiso, buf, i = (int) ((XISO_FILE_MODULUS - pos % XISO_FILE_MODULUS) % XISO_FILE_MODULUS)) != i) {
        write_err();
    }

    if (!err) {
        err = write_volume_descriptors(xiso, (pos + (xoff_t) i) / XISO_SECTOR_SIZE);
    }

    if (!err && lseek(xiso, (xoff_t) XISO_OPTIMIZED_TAG_OFFSET, SEEK_SET) == -1) {
        seek_err();
    }
    if (!err && write(xiso, XISO_OPTIMIZED_TAG, XISO_OPTIMIZED_TAG_LENGTH) != XISO_OPTIMIZED_TAG_LENGTH) {
        write_err();
    }

    if (!in_root) {
        if (err) {
            exiso_log("\ncould not create %s%s\n", iso_name ? iso_name : "xiso", iso_name && !in_name ? ".iso" : "");
        } else {
            exiso_log("\nsucessfully created %s%s (%u files totalling %lld bytes added)\n", iso_name ? iso_name : "xiso",
                      iso_name && !in_name ? ".iso" : "", s_total_files, (long long int) s_total_bytes);
        }
    }
    if (root.subdirectory != EMPTY_SUBDIRECTORY) {
        avl_traverse_depth_first(root.subdirectory, free_dir_node_avl, NULL, k_postfix, 0);
    }
  
    if (xiso != -1) {
        close(xiso);
        if (err) {
            unlink(xiso_path);
        }
    }
  
    if (root.filename) {
        free(root.filename);
    }
    if (buf) {
        free(buf);
    }
    if (ft) {
        free(ft);
    }

    if (cwd) {
        if (chdir(cwd) == -1) {
            chdir_err(cwd);
        }
        free(cwd);
    }
  
    return err;
}


int decode_xiso(char *in_xiso, char *in_path, modes in_mode, char **out_iso_path, bool in_ll_compat) {
    dir_node_avl         *root = NULL;
    bool            repair = false;
    int32_t           root_dir_sect, root_dir_size;
    int               xiso, err = 0, len, path_len = 0, add_slash = 0;
    char             *buf, *cwd = NULL, *name = NULL, *short_name = NULL, *iso_name, *folder = NULL;

    if ((xiso = open(in_xiso, READFLAGS, 0)) == -1) open_err(in_xiso);
  
    if (!err) {
        len = (int) strlen(in_xiso);
  
        if (in_mode == k_rewrite) {
            in_xiso[len -= 4] = 0;
            repair = true;
        }
  
        for (name = &in_xiso[len]; name >= in_xiso && *name != PATH_CHAR; --name) ; ++name;

        len = (int) strlen(name);

        // create a directory of the same name as the file we are working on, minus the ".iso" portion
        if (len > 4 && name[len - 4] == '.' && (name[len - 3] | 0x20) == 'i' && (name[len - 2] | 0x20) == 's' && (name[len - 1] | 0x20) == 'o') {
            name[len -= 4] = 0;
            if ((short_name = strdup(name)) == NULL) mem_err();
            name[len] = '.';
        }
    }
  
    if (!err && !len) misc_err("invalid xiso image name: %s\n", in_xiso, 0, 0);

    if (!err && in_mode == k_extract && in_path) {
        if ((cwd = getcwd(NULL, 0)) == NULL) mem_err();
        if (!err && mkdir(in_path, 0755));
        if (!err && chdir(in_path) == -1) chdir_err(in_path);
    }

    if (!err) err = verify_xiso(xiso, &root_dir_sect, &root_dir_size, name);

    iso_name = short_name ? short_name : name;

    if (!err && in_mode != k_rewrite) {
        exiso_log("%s %s:\n\n", in_mode == k_extract ? "extracting" : "listing", name);

        if (in_mode == k_extract) {
            if (!in_path) {
                if ((err = mkdir(iso_name, 0755))) mkdir_err(iso_name);
                if (!err && (err = chdir(iso_name))) chdir_err(iso_name);
            }
        }
    }
  
    if (!err && root_dir_sect && root_dir_size) {              
        if (in_path) {
            path_len = (int) strlen(in_path);
            if (in_path[path_len - 1] != PATH_CHAR) ++add_slash;
        }
    
        if ((buf = (char *) malloc(path_len + add_slash + strlen(iso_name) + 2)) == NULL) mem_err();
    
        if (!err) {
            sprintf(buf, "%s%s%s%c", in_path ? in_path : "", add_slash && (!in_path) ? PATH_CHAR_STR : "", in_mode != k_list && (!in_path) ? iso_name : "", PATH_CHAR);

            if (in_mode == k_rewrite) {
  
                if (!err && lseek(xiso, (xoff_t) root_dir_sect * XISO_SECTOR_SIZE + s_xbox_disc_lseek, SEEK_SET) == -1) seek_err();
                if (!err) err = traverse_xiso(xiso, NULL, (xoff_t) root_dir_sect * XISO_SECTOR_SIZE + s_xbox_disc_lseek, buf, k_generate_avl, &root, in_ll_compat);
                if (!err) err = create_xiso(iso_name, in_path, root, xiso, out_iso_path, NULL, NULL);
      
            } else {
                if (!err && lseek(xiso, (xoff_t) root_dir_sect * XISO_SECTOR_SIZE + s_xbox_disc_lseek, SEEK_SET) == -1) seek_err();
                if (!err) err = traverse_xiso(xiso, NULL, (xoff_t) root_dir_sect * XISO_SECTOR_SIZE + s_xbox_disc_lseek, buf, in_mode, NULL, in_ll_compat);
            }

            free(buf);
        }
    }
  
    if (err == err_iso_rewritten) err = 0;
    if (err) misc_err("failed to %s xbox iso image %s\n", in_mode == k_rewrite ? "rewrite" : in_mode == k_extract ? "extract" : "list", name, 0);

    if (xiso != -1) close(xiso);
    
    if (short_name) free(short_name);
    if (cwd) {
        chdir(cwd);
        free(cwd);
    }
  
    if (repair) in_xiso[strlen(in_xiso)] = '.';

    return err;
}


int traverse_xiso(int in_xiso, dir_node *in_dir_node, xoff_t in_dir_start, char *in_path, modes in_mode, dir_node_avl **in_root, bool in_ll_compat) {
    dir_node_avl         *avl;
    char             *path;
    xoff_t            curpos;
    dir_node            subdir;
    dir_node             *dir, node;
    int               err = 0, sector;
    unsigned short        l_offset = 0, tmp;

    if (in_dir_node == NULL) in_dir_node = &node;

    memset(dir = in_dir_node, 0, sizeof(dir_node));

read_entry:

    if (!err && read(in_xiso, &tmp, XISO_TABLE_OFFSET_SIZE) != XISO_TABLE_OFFSET_SIZE) read_err();

    if (!err) {
        if (tmp == XISO_PAD_SHORT) {
            if (l_offset == 0) {    // Directory is empty
                if (in_mode == k_generate_avl) {
                    avl_insert(in_root, EMPTY_SUBDIRECTORY);
                }
                goto end_traverse;
            }

            l_offset = l_offset * XISO_DWORD_SIZE + (XISO_SECTOR_SIZE - (l_offset * XISO_DWORD_SIZE) % XISO_SECTOR_SIZE);
            err = lseek(in_xiso, in_dir_start + (xoff_t) l_offset, SEEK_SET) == -1 ? 1 : 0;

            if (!err) goto read_entry;       // me and my silly comments
        } else {
            l_offset = tmp;
        }
    }

    if (!err && read(in_xiso, &dir->r_offset, XISO_TABLE_OFFSET_SIZE) != XISO_TABLE_OFFSET_SIZE) read_err();
    if (!err && read(in_xiso, &dir->start_sector, XISO_SECTOR_OFFSET_SIZE) != XISO_SECTOR_OFFSET_SIZE) read_err();
    if (!err && read(in_xiso, &dir->file_size, XISO_FILESIZE_SIZE) != XISO_FILESIZE_SIZE) read_err();
    if (!err && read(in_xiso, &dir->attributes, XISO_ATTRIBUTES_SIZE) != XISO_ATTRIBUTES_SIZE) read_err();
    if (!err && read(in_xiso, &dir->filename_length, XISO_FILENAME_LENGTH_SIZE) != XISO_FILENAME_LENGTH_SIZE) read_err();

    if (!err) {
        little16(l_offset);
        little16(dir->r_offset);
        little32(dir->file_size);
        little32(dir->start_sector);

        if ((dir->filename = (char *) malloc(dir->filename_length + 1)) == NULL) mem_err();
    }
  
    if (!err) {
        if (read(in_xiso, dir->filename, dir->filename_length) != dir->filename_length) read_err();
        if (!err) {
            dir->filename[dir->filename_length] = 0;
  
            // security patch (Chris Bainbridge), modified by in to support "...", etc. 02.14.06 (in)
            if (!strcmp(dir->filename, ".") || !strcmp(dir->filename, "..") || strchr(dir->filename, '/') || strchr(dir->filename, '\\')) {
                log_err(__FILE__, __LINE__, "filename '%s' contains invalid character(s), aborting.", dir->filename);
                exit(1);
            }
        }
    }
  
    if (!err && in_mode == k_generate_avl) {
        if ((avl = (dir_node_avl *) malloc(sizeof(dir_node_avl))) == NULL) mem_err();
        if (!err) {
            memset(avl, 0, sizeof(dir_node_avl));
      
            if ((avl->filename = strdup(dir->filename)) == NULL) mem_err();
        }
        if (!err) {
            dir->avl_node = avl;

            avl->file_size = dir->file_size;
            avl->old_start_sector = dir->start_sector;

            if (avl_insert(in_root, avl) == k_avl_error) misc_err("this iso appears to be corrupt\n", 0, 0, 0);
        }
    }

    if (!err && l_offset) {
        in_ll_compat = false;
  
        if ((dir->left = (dir_node *) malloc(sizeof(dir_node))) == NULL) mem_err();
        if (!err) {
            memset(dir->left, 0, sizeof(dir_node));
            if (lseek(in_xiso, in_dir_start + (xoff_t) l_offset * XISO_DWORD_SIZE, SEEK_SET) == -1) seek_err();
        }
        if (!err) {
            dir->left->parent = dir;
            dir = dir->left;
      
            goto read_entry;
        }
    }

left_processed:
      
    if (dir->left) { free(dir->left); dir->left = NULL; }

    if (!err && (curpos = lseek(in_xiso, 0, SEEK_CUR)) == -1) seek_err();
  
    if (!err) {
        if (dir->attributes & XISO_ATTRIBUTE_DIR) {
            if (in_path) {
                if ((path = (char *) malloc(strlen(in_path) + dir->filename_length + 2)) == NULL) mem_err();
      
                if (!err) {
                    sprintf(path, "%s%s%c", in_path, dir->filename, PATH_CHAR);
                    if (dir->start_sector && lseek(in_xiso, (xoff_t) dir->start_sector * XISO_SECTOR_SIZE + s_xbox_disc_lseek, SEEK_SET) == -1) seek_err();
                }
            } else path = NULL;
  
            if (!err) {
                if (!s_remove_systemupdate || !strstr(dir->filename, s_systemupdate))
                {
                    if (in_mode == k_extract) {
                        if ((err = mkdir(dir->filename, 0755))) mkdir_err(dir->filename);
                        if (!err && dir->start_sector && (err = chdir(dir->filename))) chdir_err(dir->filename);
                    }
                    if(!err && in_mode != k_generate_avl) {
                        exiso_log("%s%s%s%s (0 bytes)%s", in_mode == k_extract ? "creating " : "", in_path, dir->filename, PATH_CHAR_STR, in_mode == k_extract ? " [OK]" : ""); flush();
                        exiso_log("\n");
                    }
                }
            }
      
            if (!err && dir->start_sector) {
                memcpy(&subdir, dir, sizeof(dir_node));
        
                subdir.parent = NULL;
                if (!err && dir->file_size > 0) err = traverse_xiso(in_xiso, &subdir, (xoff_t) dir->start_sector * XISO_SECTOR_SIZE + s_xbox_disc_lseek, path, in_mode, in_mode == k_generate_avl ? &dir->avl_node->subdirectory : NULL, in_ll_compat);  

                if (!s_remove_systemupdate || !strstr(dir->filename, s_systemupdate))
                {
  
                    if (!err && in_mode == k_extract && (err = chdir(".."))) chdir_err("..");
                }
            }
  
            if (path) free(path);
        } else if (in_mode != k_generate_avl) {
            if (!err) {
                if (!s_remove_systemupdate || !strstr(in_path, s_systemupdate))
                {

                    if (in_mode == k_extract) {
                        err = extract_file(in_xiso, dir, in_mode, in_path);
                    } else {
                        exiso_log("%s%s%s (%lu bytes)%s", in_mode == k_extract ? "extracting " : "", in_path, dir->filename, dir->file_size , ""); flush();
                        exiso_log("\n");
                    }

                    ++s_total_files;
                    ++s_total_files_all_isos;
                    s_total_bytes += dir->file_size;
                    s_total_bytes_all_isos += dir->file_size;
                }
            }
        }
    }
  
    if (!err && dir->r_offset) {
        // compatibility for iso's built as linked lists (bleh!)
        if (in_ll_compat && (xoff_t) dir->r_offset * XISO_DWORD_SIZE / XISO_SECTOR_SIZE > (sector = (int) ((curpos - in_dir_start) / XISO_SECTOR_SIZE))) dir->r_offset = sector * (XISO_SECTOR_SIZE / XISO_DWORD_SIZE) + (XISO_SECTOR_SIZE / XISO_DWORD_SIZE);
    
        if (!err && lseek(in_xiso, in_dir_start + (xoff_t) dir->r_offset * XISO_DWORD_SIZE, SEEK_SET) == -1) seek_err();
        if (!err) {
            if (dir->filename) { free(dir->filename); dir->filename = NULL; }

            l_offset = dir->r_offset;
      
            goto read_entry;
        }
    }

end_traverse:

    if (dir->filename) free(dir->filename);
  
    if ((dir = dir->parent)) goto left_processed;
  
    return err;
}

int extract_file(int in_xiso, dir_node *in_file, modes in_mode , char* path) {
    char            c;
    int               err = 0;
    bool            warn = false;
    unsigned long       i, size, totalsize = 0, totalpercent = 0;
    int               out;

    if (s_remove_systemupdate && strstr(path, s_systemupdate))
    {
        if (! err && lseek(in_xiso, (xoff_t) in_file->start_sector * XISO_SECTOR_SIZE + s_xbox_disc_lseek, SEEK_SET) == -1) seek_err();
    }
    else
    {

        if (in_mode == k_extract) {
            if ((out = open(in_file->filename, WRITEFLAGS, 0644)) == -1) open_err(in_file->filename);
        }  else err = 1;
  
        if (! err && lseek(in_xiso, (xoff_t) in_file->start_sector * XISO_SECTOR_SIZE + s_xbox_disc_lseek, SEEK_SET) == -1) seek_err();

        if (! err) {
            if (in_file->file_size == 0)
                exiso_log("%s%s%s (0 bytes) [100%%]%s\r", in_mode == k_extract ? "extracting " : "", path, in_file->filename, "");
            if (in_mode == k_extract) {
                for (i = 0, size = std::min(in_file->file_size, READWRITE_BUFFER_SIZE);
                     i < in_file->file_size && read(in_xiso, s_copy_buffer, size) == (int) size;
                     i += size, size = std::min(in_file->file_size - i, READWRITE_BUFFER_SIZE))
                {
                    if (write(out, s_copy_buffer, size) != (int) size) {
                        write_err();
                        break;
                    }
                    totalsize += size;
                    totalpercent = (totalsize * 100.0) / in_file->file_size;
                    exiso_log("%s%s%s (%lu bytes) [%lu%%]%s\r", in_mode == k_extract ? "extracting " : "", path, in_file->filename, in_file->file_size , totalpercent, "");
                }
      
                close(out);
            } else {
                for (i = 0, size = std::min(in_file->file_size, READWRITE_BUFFER_SIZE);
                     i < in_file->file_size && read(in_xiso, s_copy_buffer, size) == (int) size;
                     i += size, size = std::min(in_file->file_size - i, READWRITE_BUFFER_SIZE))
                {
                    totalsize += size;
                    totalpercent = (totalsize * 100.0) / in_file->file_size;
                    exiso_log("%s%s%s (%lu bytes) [%lu%%]%s\r", in_mode == k_extract ? "extracting " : "", path, in_file->filename, in_file->file_size , totalpercent, "");
                }
            }
        }

    }

    if (! err) exiso_log("\n");

    return err;
}

int write_tree(dir_node_avl *in_avl, write_tree_context *in_context, int in_depth, bool media_enable) {
    xoff_t            pos;
    write_tree_context        context;
    int               err = 0, pad;
    char            sector[XISO_SECTOR_SIZE];

    if (in_avl->subdirectory) {
        if (in_context->path) {
            if (asprintf(&context.path, "%s%s%c", in_context->path, in_avl->filename, PATH_CHAR) == -1) mem_err(); }
        else { if (asprintf(&context.path, "%c", PATH_CHAR) == -1) mem_err(); }

        if (! err) {
            exiso_log("adding %s (0 bytes) [OK]\n", context.path);
  
            if (in_avl->subdirectory != EMPTY_SUBDIRECTORY) {
                context.xiso = in_context->xiso;
                context.from = in_context->from;
                context.progress = in_context->progress;
                context.final_bytes = in_context->final_bytes;
    
                if (in_context->from == -1) {
                    if (chdir(in_avl->filename) == -1) chdir_err(in_avl->filename);
                }
                if (! err && lseek(in_context->xiso, (xoff_t) in_avl->start_sector * XISO_SECTOR_SIZE, SEEK_SET) == -1) seek_err();
                if (! err) err = avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback) write_directory, (void *) in_context->xiso, k_prefix, 0);
                if (! err && (pos = lseek(in_context->xiso, 0, SEEK_CUR)) == -1) seek_err();
                if (! err && (pad = (int) ((XISO_SECTOR_SIZE - (pos % XISO_SECTOR_SIZE)) % XISO_SECTOR_SIZE))) {
                    memset(sector, XISO_PAD_BYTE, pad);
                    if (write(in_context->xiso, sector, pad) != pad) write_err();
                }
                if (! err) err = avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback) write_file, &context, k_prefix, 0, media_enable);
                if (! err) err = avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback) write_tree, &context, k_prefix, 0, media_enable);
                if (! err && in_context->from == -1) {
                    if (chdir("..") == -1) chdir_err("..");
                }
        
                if (context.path) free(context.path);
            } else {
                memset(sector, XISO_PAD_BYTE, XISO_SECTOR_SIZE);
                if ((pos = lseek(in_context->xiso, in_avl->start_sector * XISO_SECTOR_SIZE, SEEK_SET)) == -1) seek_err();
                if (!err && write(in_context->xiso, sector, XISO_SECTOR_SIZE) != XISO_SECTOR_SIZE) write_err();
            }
        }
    }

    return err;
}

int write_file(dir_node_avl *in_avl, write_tree_context *in_context, int in_depth, bool media_enable) {
    char         *buf, *p;
    unsigned long   bytes, n, size;
    int           err = 0, fd = -1, i;

    if (! in_avl->subdirectory) {
        if (lseek(in_context->xiso, (xoff_t) in_avl->start_sector * XISO_SECTOR_SIZE, SEEK_SET) == -1) seek_err();
    
        if (! err && (buf = (char *) malloc((size = std::max(XISO_SECTOR_SIZE, READWRITE_BUFFER_SIZE)) + 1)) == NULL) mem_err();
        if (! err) {
            if (in_context->from == -1) {
                if ((fd = open(in_avl->filename, READFLAGS, 0)) == -1) open_err(in_avl->filename);
            } else {
                if (lseek(fd = in_context->from, (xoff_t) in_avl->old_start_sector * XISO_SECTOR_SIZE + s_xbox_disc_lseek, SEEK_SET) == -1) seek_err();
            }
        }

        if (! err) {
            exiso_log("adding %s%s (%lu bytes) ", in_context->path, in_avl->filename, in_avl->file_size); flush();

            if (media_enable && (i = (int) strlen(in_avl->filename)) >= 4 && in_avl->filename[i - 4] == '.' && (in_avl->filename[i - 3] | 0x20) == 'x' && (in_avl->filename[i - 2] | 0x20) == 'b' && (in_avl->filename[i - 1] | 0x20) == 'e') {
                for (bytes = in_avl->file_size, i = 0; ! err && bytes;) {
                    if ((n = read(fd, buf + i, std::min(bytes, size - i))) == -1) read_err();
          
                    bytes -= n;
                    
                    if (! err) {
                        for (buf[n += i] = 0, p = buf; (p = boyer_moore_search(p, n - (p - buf))) != NULL; p += XISO_MEDIA_ENABLE_LENGTH) p[XISO_MEDIA_ENABLE_BYTE_POS] = XISO_MEDIA_ENABLE_BYTE;
          
                        if (bytes) {
                            if (write(in_context->xiso, buf, n - (i = XISO_MEDIA_ENABLE_LENGTH - 1)) != (int) n - i) write_err();
              
                            if (! err) memcpy(buf, &buf[n - (XISO_MEDIA_ENABLE_LENGTH - 1)], XISO_MEDIA_ENABLE_LENGTH - 1);
                        } else {
                            if (write(in_context->xiso, buf, n + i) != (int) n + i) write_err();
                        }
                    }
                }
            } else {
                for (bytes = in_avl->file_size; ! err && bytes; bytes -= n) {
                    if ((n = read(fd, buf, std::min(bytes, size))) == -1) read_err();
  
                    if (! err && write(in_context->xiso, buf, n) != (int) n) write_err();
                }
            }
      
            if (! err && (bytes = (XISO_SECTOR_SIZE - (in_avl->file_size % XISO_SECTOR_SIZE)) % XISO_SECTOR_SIZE)) {
                memset(buf, XISO_PAD_BYTE, bytes);
                if (write(in_context->xiso, buf, bytes) != (int) bytes) write_err();
            }
      
            if (err) { exiso_log("failed\n"); }
            else {
                exiso_log("[OK]\n");

                ++s_total_files;
                s_total_bytes += in_avl->file_size;

                if (in_context->progress) (*in_context->progress)(s_total_bytes, in_context->final_bytes);
            }
        }
        
        if (in_context->from == -1 && fd != -1) close(fd);
        if (buf) free(buf);
    }
  
    return err;
}

int write_directory(dir_node_avl *in_avl, int in_xiso, int in_depth) {
    xoff_t          pos;
    int             err = 0, pad;
    unsigned short      l_offset, r_offset;
    unsigned long     file_size = in_avl->file_size + (in_avl->subdirectory ? (XISO_SECTOR_SIZE - (in_avl->file_size % XISO_SECTOR_SIZE)) % XISO_SECTOR_SIZE  : 0);
    char          length = (char) strlen(in_avl->filename), attributes = in_avl->subdirectory ? XISO_ATTRIBUTE_DIR : XISO_ATTRIBUTE_ARC, sector[XISO_SECTOR_SIZE];
    
    little32(in_avl->file_size);
    little32(in_avl->start_sector);
  
    l_offset = (unsigned short) (in_avl->left ? in_avl->left->offset / XISO_DWORD_SIZE : 0);
    r_offset = (unsigned short) (in_avl->right ? in_avl->right->offset / XISO_DWORD_SIZE : 0);
  
    little16(l_offset);
    little16(r_offset);
  
    memset(sector, XISO_PAD_BYTE, XISO_SECTOR_SIZE);
  
    if ((pos = lseek(in_xiso, 0, SEEK_CUR)) == -1) seek_err();
    if (! err && (pad = (int) ((xoff_t) in_avl->offset + in_avl->dir_start - pos)) && write(in_xiso, sector, pad) != pad) write_err();
    if (! err && write(in_xiso, &l_offset, XISO_TABLE_OFFSET_SIZE) != XISO_TABLE_OFFSET_SIZE) write_err();
    if (! err && write(in_xiso, &r_offset, XISO_TABLE_OFFSET_SIZE) != XISO_TABLE_OFFSET_SIZE) write_err();
    if (! err && write(in_xiso, &in_avl->start_sector, XISO_SECTOR_OFFSET_SIZE) != XISO_SECTOR_OFFSET_SIZE) write_err();
    if (! err && write(in_xiso, &file_size, XISO_FILESIZE_SIZE) != XISO_FILESIZE_SIZE) write_err();
    if (! err && write(in_xiso, &attributes, XISO_ATTRIBUTES_SIZE) != XISO_ATTRIBUTES_SIZE) write_err();
    if (! err && write(in_xiso, &length, XISO_FILENAME_LENGTH_SIZE) != XISO_FILENAME_LENGTH_SIZE) write_err();
    if (! err && write(in_xiso, in_avl->filename, length) != length) write_err();
  
    little32(in_avl->start_sector);
    little32(in_avl->file_size);
  
    return err;     
}

int calculate_total_files_and_bytes(dir_node_avl *in_avl, void *in_context, int in_depth) {
    if (in_avl->subdirectory && in_avl->subdirectory != EMPTY_SUBDIRECTORY) {
        avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback) calculate_total_files_and_bytes, NULL, k_prefix, 0);
    } else {
        ++s_total_files;
        s_total_bytes += in_avl->file_size;
    }
  
    return 0;
}

int calculate_directory_size(dir_node_avl *in_avl, unsigned long *out_size, long in_depth) {
    unsigned long       length;

    if (in_depth == 0) *out_size = 0;
  
    length = XISO_FILENAME_OFFSET + strlen(in_avl->filename);
    length += (XISO_DWORD_SIZE - (length % XISO_DWORD_SIZE)) % XISO_DWORD_SIZE;
  
    if (n_sectors(*out_size + length) > n_sectors(*out_size)) {
        *out_size += (XISO_SECTOR_SIZE - (*out_size % XISO_SECTOR_SIZE)) % XISO_SECTOR_SIZE;
    }

    in_avl->offset = *out_size;

    *out_size += length;

    return 0;
}

int calculate_directory_requirements(dir_node_avl *in_avl, void *in_context, int in_depth) {
    if (in_avl->subdirectory) {
        if (in_avl->subdirectory != EMPTY_SUBDIRECTORY) {
            avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback)calculate_directory_size, &in_avl->file_size, k_prefix, 0);
            avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback)calculate_directory_requirements, in_context, k_prefix, 0);
        } else {
            in_avl->file_size = XISO_SECTOR_SIZE;
        }
    }
  
    return 0;
}

int calculate_directory_offsets(dir_node_avl *in_avl, unsigned long *io_current_sector, int in_depth) {
    wdsafp_context        context;

    if (in_avl->subdirectory) {
        if (in_avl->subdirectory == EMPTY_SUBDIRECTORY) {
            in_avl->start_sector = *io_current_sector;
            *io_current_sector += 1;
        }
        else {
            context.current_sector = io_current_sector;
            context.dir_start = (xoff_t) (in_avl->start_sector = *io_current_sector) * XISO_SECTOR_SIZE;
    
            *io_current_sector += n_sectors(in_avl->file_size);
    
            avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback) write_dir_start_and_file_positions, &context, k_prefix, 0);
            avl_traverse_depth_first(in_avl->subdirectory, (traversal_callback) calculate_directory_offsets, io_current_sector, k_prefix, 0);
        }
    }
  
    return 0;
}

int write_dir_start_and_file_positions(dir_node_avl *in_avl, wdsafp_context *io_context, int in_depth) {
    in_avl->dir_start = io_context->dir_start;

    if (! in_avl->subdirectory) {
        in_avl->start_sector = *io_context->current_sector;
        *io_context->current_sector += n_sectors(in_avl->file_size);
    }
  
    return 0;
}


// Found the CD-ROM layout in ECMA-119.  Now burning software should correctly
// detect the format of the xiso and burn it correctly without the user having
// to specify sector sizes and so on. in 10.29.04

#define ECMA_119_DATA_AREA_START      0x8000
#define ECMA_119_VOLUME_SPACE_SIZE      (ECMA_119_DATA_AREA_START + 80)
#define ECMA_119_VOLUME_SET_SIZE      (ECMA_119_DATA_AREA_START + 120)
#define ECMA_119_VOLUME_SET_IDENTIFIER    (ECMA_119_DATA_AREA_START + 190)
#define ECMA_119_VOLUME_CREATION_DATE   (ECMA_119_DATA_AREA_START + 813)


// write_volume_descriptors() assumes that the iso file block from offset
// 0x8000 to 0x8808 has been zeroed prior to entry.

int write_volume_descriptors(int in_xiso, unsigned long in_total_sectors) {
    int           big, err = 0, little;
    char        date[] = "0000000000000000";
    char        spaces[ECMA_119_VOLUME_CREATION_DATE - ECMA_119_VOLUME_SET_IDENTIFIER];

    big = little = in_total_sectors;
  
    big32(big);
    little32(little);

    memset(spaces, 0x20, sizeof(spaces));
  
    if (lseek(in_xiso, ECMA_119_DATA_AREA_START, SEEK_SET) == -1) seek_err();
    if (! err && write(in_xiso, "\x01" "CD001\x01", 7) == -1) write_err();
    if (! err && lseek(in_xiso, ECMA_119_VOLUME_SPACE_SIZE, SEEK_SET) == -1) seek_err();
    if (! err && write(in_xiso, &little, 4) == -1) write_err();
    if (! err && write(in_xiso, &big, 4) == -1) write_err();
    if (! err && lseek(in_xiso, ECMA_119_VOLUME_SET_SIZE, SEEK_SET) == -1) seek_err();
    if (! err && write(in_xiso, "\x01\x00\x00\x01\x01\x00\x00\x01\x00\x08\x08\x00", 12) == -1) write_err();
    if (! err && lseek(in_xiso, ECMA_119_VOLUME_SET_IDENTIFIER, SEEK_SET) == -1) seek_err();
    if (! err && write(in_xiso, spaces, sizeof(spaces)) == -1) write_err();
    if (! err && write(in_xiso, date, sizeof(date)) == -1) write_err();
    if (! err && write(in_xiso, date, sizeof(date)) == -1) write_err();
    if (! err && write(in_xiso, date, sizeof(date)) == -1) write_err();
    if (! err && write(in_xiso, date, sizeof(date)) == -1) write_err();
    if (! err && write(in_xiso, "\x01", 1) == -1) write_err();
    if (! err && lseek(in_xiso, ECMA_119_DATA_AREA_START + XISO_SECTOR_SIZE, SEEK_SET) == -1) seek_err();
    if (! err && write(in_xiso, "\xff" "CD001\x01", 7) == -1) write_err();
    
    return err;
}

FILE_TIME *alloc_filetime_now(void) {
    FILE_TIME          *ft;
    double          tmp;
    time_t          now;
    int             err = 0;

    if ((ft = (FILE_TIME *) malloc(sizeof(struct FILE_TIME))) == NULL) mem_err();
    if (! err && (now = time(NULL)) == -1) unknown_err();
    if (! err) {
        tmp = ((double) now + (369.0 * 365.25 * 24 * 60 * 60 - (3.0 * 24 * 60 * 60 + 6.0 * 60 * 60))) * 1.0e7;

        ft->h = (unsigned long) (tmp * (1.0 / (4.0 * (double) (1 << 30))));
        ft->l = (unsigned long) (tmp - ((double) ft->h) * 4.0 * (double) (1 << 30));
    
        little32(ft->h);    // convert to little endian here because this is a PC only struct and we won't read it anyway
        little32(ft->l);
    } else if (ft) {
        free(ft);
        ft = NULL;
    }
  
    return ft;
}
