#ifndef __BOYER_MOORE
#define __BOYER_MOORE

void boyer_moore_done();
char *boyer_moore_search(char *in_text, long in_text_len);
int boyer_moore_init(char *in_pattern, long in_pat_len, long in_alphabet_size);

#endif
