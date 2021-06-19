#include "boyer_moore.h"

#include "exiso_log.h"
#include <algorithm>
#include <cstdlib>

static long s_pat_len;
static char *s_pattern = nullptr;
static long *s_gs_table = nullptr;
static long *s_bc_table = nullptr;

int boyer_moore_init(char *in_pattern, long in_pat_len, long in_alphabet_size) {
    long j, k;
    long *backup;
    long err = 0;

    s_pattern = in_pattern;
    s_pat_len = in_pat_len;
  
    if ((s_bc_table = (long *)malloc(in_alphabet_size * sizeof(long))) == nullptr) {
        mem_err();
    }
  
    if (! err) {
        for (long i = 0; i < in_alphabet_size; ++i) {
            s_bc_table[i] = in_pat_len;
        }
        for (long i = 0; i < in_pat_len - 1; ++i) {
            s_bc_table[(unsigned char)in_pattern[i]] = in_pat_len - i - 1;
        }
  
        if ((s_gs_table = (long *)malloc(2 * (in_pat_len + 1) * sizeof(long))) == nullptr) {
            mem_err();
        }
    }     

    if (! err) {
        backup = s_gs_table + in_pat_len + 1;
    
        for (long i = 1; i <= in_pat_len; ++i) {
            s_gs_table[i] = 2 * in_pat_len - i;
        }
        for (long i = in_pat_len, j = in_pat_len + 1; i; --i, --j) {
            backup[i] = j;
  
            while (j <= in_pat_len && in_pattern[i - 1] != in_pattern[j - 1]) {
                if (s_gs_table[j] > in_pat_len - i) {
                    s_gs_table[j] = in_pat_len - i;
                }
                j = backup[j];  
            }
        }
        for (long i = 1; i <= j; ++i) {
            if (s_gs_table[i] > in_pat_len + j - i) {
                s_gs_table[i] = in_pat_len + j - i;
            }
        }
    
        k = backup[j];
  
        for (; j <= in_pat_len; k = backup[k]) {
            for (; j <= k; ++j) {
                if (s_gs_table[j] >= k - j + in_pat_len) {
                    s_gs_table[j] = k - j + in_pat_len;
                }
            }
        }
    }
  
    return err;
}


void boyer_moore_done() {
    if (s_bc_table) {
        free(s_bc_table);
        s_bc_table = nullptr;
    }
    if (s_gs_table) {
        free(s_gs_table);
        s_gs_table = nullptr;
    }
}


char *boyer_moore_search(char *in_text, long in_text_len) {
    long i, j, k, l;

    for (i = j = s_pat_len - 1; j < in_text_len && i >= 0;) {
        if (in_text[j] == s_pattern[i]) {
            --i;
            --j;
        }
        else {
            k = s_gs_table[i + 1];
            l = s_bc_table[(unsigned char)in_text[j]];

            j += std::max(k, l);
      
            i = s_pat_len - 1;
        }
    }
  
    return i < 0 ? in_text + j + 1 : nullptr;
}
