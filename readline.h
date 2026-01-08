/*
 * readline utility
 *
 * Copyright (c) 2003-2025 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef READLINE_H
#define READLINE_H

#include <stdint.h>

typedef struct ReadlineState ReadlineState;

typedef void ReadLineFunc(void *opaque, const char *str);
typedef int ReadLineGetColor(int *plen, const char *buf, int pos, int buf_len);

struct ReadlineState {
    int term_cmd_buf_index; /* byte position in the command line */
    int term_cmd_buf_len; /* byte length of the command line */
    
    uint32_t utf8_val;
    uint8_t term_cmd_updated; /* if the command line was updated */
    uint8_t utf8_state;
    uint8_t term_esc_state;
    int term_esc_param;
    int term_esc_param1;
    int term_esc_param2;
    int term_cursor_x; /* 0 <= term_cursor_x < term_width */
    int term_cursor_pos; /* linear position */
    
    int term_hist_entry; /* position in term_history  or -1 */
    int term_history_size; /* size of term_historyf */
    uint8_t term_is_password;
    const char *term_prompt;
    /* the following fields must be initialized by the user */
    int term_width;
    int term_cmd_buf_size;
    int term_kill_buf_len;
    uint8_t *term_cmd_buf;  /* allocated length is term_cmd_buf_size */
    uint8_t *term_kill_buf;  /* allocated length is term_cmd_buf_size */
    int term_history_buf_size;
    char *term_history; /* zero separated history entries */
    ReadLineGetColor *get_color; /* NULL if no colorization */
};

#define COLOR_NONE            0
#define COLOR_BLACK           1
#define COLOR_RED             2
#define COLOR_GREEN           3
#define COLOR_YELLOW          4
#define COLOR_BLUE            5
#define COLOR_MAGENTA         6
#define COLOR_CYAN            7
#define COLOR_WHITE           8
#define COLOR_GRAY            9
#define COLOR_BRIGHT_RED     10
#define COLOR_BRIGHT_GREEN   11
#define COLOR_BRIGHT_YELLOW  12
#define COLOR_BRIGHT_BLUE    13
#define COLOR_BRIGHT_MAGENTA 14
#define COLOR_BRIGHT_CYAN    15
#define COLOR_BRIGHT_WHITE   16

extern const char *term_colors[17];

void add_completion(const char *str);

#define READLINE_RET_EXIT        (-1)
#define READLINE_RET_NOT_HANDLED 0 /* command not handled */
#define READLINE_RET_HANDLED     1 /* command handled */
#define READLINE_RET_ACCEPTED    2 /* return pressed */
/* return READLINE_RET_x */
int readline_handle_byte(ReadlineState *s, int c);
void readline_start(ReadlineState *s, const char *prompt, int is_password);

/* the following functions must be provided */
void readline_find_completion(const char *cmdline);
void term_printf(const char *fmt, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
void term_flush(void);

#endif /* READLINE_H */
