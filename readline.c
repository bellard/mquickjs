/*
 * readline utility
 *
 * Copyright (c) 2003-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cutils.h"
#include "readline.h"

#define IS_NORM 0
#define IS_ESC  1
#define IS_CSI  2

static void term_show_prompt2(ReadlineState *s)
{
    term_printf("%s", s->term_prompt);
    term_flush();
    /* XXX: assuming no unicode chars */
    s->term_cursor_x = strlen(s->term_prompt) % s->term_width;
    s->term_cursor_pos = 0;
    s->term_esc_state = IS_NORM;
    s->utf8_pos = 0;
}

static void term_show_prompt(ReadlineState *s)
{
    term_show_prompt2(s);
    s->term_cmd_buf_index = 0;
    s->term_cmd_buf_len = 0;
}

static void print_csi(int n, int code)
{
    if (n == 1) {
        term_printf("\033[%c", code);
    } else {
        term_printf("\033[%d%c", n, code);
    }
}

const char *term_colors[17] = {
    "\033[0m",
    "\033[30m",
    "\033[31m",
    "\033[32m",
    "\033[33m",
    "\033[34m",
    "\033[35m",
    "\033[36m",
    "\033[37m",
    "\033[30;1m",
    "\033[31;1m",
    "\033[32;1m",
    "\033[33;1m",
    "\033[34;1m",
    "\033[35;1m",
    "\033[36;1m",
    "\033[37;1m",
};

static void print_color(int c)
{
    term_printf("%s", term_colors[c]);
}

static void move_cursor(ReadlineState *s, int delta)
{
    int l;
    if (delta > 0) {
        while (delta != 0) {
            if (s->term_cursor_x == (s->term_width - 1)) {
                term_printf("\r\n"); /* translated to CRLF */
                s->term_cursor_x = 0;
                delta--;
            } else {
                l = min_int(s->term_width - 1 - s->term_cursor_x, delta);
                print_csi(l, 'C'); /* right */
                delta -= l;
                s->term_cursor_x += l;
            }
        }
    } else if (delta < 0) {
        delta = -delta;
        while (delta != 0) {
            if (s->term_cursor_x == 0) {
                print_csi(1, 'A'); /* up */
                print_csi(s->term_width - 1, 'C'); /* right */
                delta--;
                s->term_cursor_x = s->term_width - 1;
            } else {
                l = min_int(delta, s->term_cursor_x);
                print_csi(l, 'D'); /* left */
                delta -= l;
                s->term_cursor_x -= l;
            }
        }
    }
}

static int char_width(int c)
{
    /* XXX: complete or find a way to use wcwidth() */
    if (c < 0x100) {
        return 1;
    } else if ((c >= 0x4E00 && c <= 0x9FFF) || /* CJK */
               (c >= 0xFF01 && c <= 0xFF5E) || /* fullwidth ASCII */
               (c >= 0x1F600 && c <= 0x1F64F)) { /* emoji */
        return 2;
    } else {
        return 1;
    }
}

/* update the displayed command line */
static void term_update(ReadlineState *s)
{
    int i, len, c,  new_cursor_pos, last_color, color_len;
    uint8_t buf[UTF8_CHAR_LEN_MAX + 1];
    size_t c_len;
    
    new_cursor_pos = 0;
    if (s->term_cmd_updated) {
        move_cursor(s, -s->term_cursor_pos);
        s->term_cursor_pos = 0;
        last_color = COLOR_NONE;
        color_len = 0;
        s->term_cmd_buf[s->term_cmd_buf_len] = '\0'; /* add a trailing '\0' to ease colorization */
        for(i = 0; i < s->term_cmd_buf_len; i += c_len) {
            if (i == s->term_cmd_buf_index)
                new_cursor_pos = s->term_cursor_pos;
            c = utf8_get(s->term_cmd_buf + i, &c_len);
            if (s->term_is_password) {
                len = 1;
                buf[0] = '*';
                buf[1] = '\0';
            } else {
                len = char_width(c);
                memcpy(buf, s->term_cmd_buf + i, c_len);
                buf[c_len] = '\0';
            }
            /* the wide char does not fit so we display it on the next
               line by enlarging the previous char */
            if (s->term_cursor_x + len > s->term_width && i > 0) {
                while (s->term_cursor_x < s->term_width) {
                    term_printf(" ");
                    s->term_cursor_x++;
                    s->term_cursor_pos++;
                }
                s->term_cursor_x = 0;
            }
            s->term_cursor_pos += len;
            s->term_cursor_x += len;
            if (s->term_cursor_x >= s->term_width)
                s->term_cursor_x = 0;
            if (!s->term_is_password && s->get_color) {
                if (color_len == 0) {
                    int new_color = s->get_color(&color_len, (const char *)s->term_cmd_buf, i, s->term_cmd_buf_len);
                    if (new_color != last_color) {
                        last_color = new_color;
                        print_color(COLOR_NONE); /* reset last color */
                        print_color(last_color);
                    }
                }
                color_len--;
            }
            term_printf("%s", buf);
        }
        if (last_color != COLOR_NONE)
            print_color(COLOR_NONE);
        if (i == s->term_cmd_buf_index)
            new_cursor_pos = s->term_cursor_pos;
        if (s->term_cursor_x == 0) {
            /* show the cursor on the next line */
            term_printf(" \x08");
        }
        /* remove the trailing characters */
        print_csi(1, 'J');
        s->term_cmd_updated = FALSE;
    } else {
        int cursor_x;
        /* compute the new cursor pos without display */
        cursor_x = (s->term_cursor_x - s->term_cursor_pos) % s->term_width;
        if (cursor_x < 0)
            cursor_x += s->term_width;
        new_cursor_pos = 0;
        for(i = 0; i < s->term_cmd_buf_index; i += c_len) {
            c = utf8_get(s->term_cmd_buf + i, &c_len);
            if (s->term_is_password)
                c = '*';
            len = char_width(c);
            /* the wide char does not fit so we display it on the next
               line by enlarging the previous char */
            if (cursor_x + len > s->term_width && i > 0) {
                new_cursor_pos += s->term_width - cursor_x;
                cursor_x = 0;
            }
            new_cursor_pos += len;
            cursor_x += len;
            if (cursor_x >= s->term_width)
                cursor_x = 0;
        }
    }
    move_cursor(s, new_cursor_pos - s->term_cursor_pos);
    s->term_cursor_pos = new_cursor_pos;
    term_flush();
}

static void term_kill_region(ReadlineState *s, int to, int kill)
{
    int start = s->term_cmd_buf_index;
    int end = s->term_cmd_buf_index;
    if (to < start)
        start = to;
    else
        end = to;
    if (end > s->term_cmd_buf_len)
        end = s->term_cmd_buf_len;
    if (start < end) {
        int len = end - start;
        if (kill) {
            memcpy(s->term_kill_buf, s->term_cmd_buf + start,
                   len * sizeof(s->term_cmd_buf[0]));
            s->term_kill_buf_len = len;
        }
        memmove(s->term_cmd_buf + start, s->term_cmd_buf + end,
                (s->term_cmd_buf_len - end) * sizeof(s->term_cmd_buf[0]));
        s->term_cmd_buf_len -= len;
        s->term_cmd_buf_index = start;
        s->term_cmd_updated = TRUE;
    }
}

static void term_insert_region(ReadlineState *s, const uint8_t *p, int len)
{
    int pos = s->term_cmd_buf_index;

    if (pos + len < s->term_cmd_buf_size) {
        int nchars = s->term_cmd_buf_len - pos;
        if (nchars > 0) {
            memmove(s->term_cmd_buf + pos + len,
                    s->term_cmd_buf + pos,
                    nchars * sizeof(s->term_cmd_buf[0]));
        }
        memcpy(s->term_cmd_buf + pos, p, len * sizeof(s->term_cmd_buf[0]));
        s->term_cmd_buf_len += len;
        s->term_cmd_buf_index += len;
        s->term_cmd_updated = TRUE;
    }
}

static void term_insert_char(ReadlineState *s, int ch)
{
    uint8_t buf[UTF8_CHAR_LEN_MAX + 1];
    term_insert_region(s, buf, unicode_to_utf8(buf, ch));
}

static BOOL is_utf8_ext(int c)
{
    return (c >= 0x80 && c < 0xc0);
}

static void term_backward_char(ReadlineState *s)
{
    if (s->term_cmd_buf_index > 0) {
        s->term_cmd_buf_index--;
        while (s->term_cmd_buf_index > 0 &&
               is_utf8_ext(s->term_cmd_buf[s->term_cmd_buf_index])) {
            s->term_cmd_buf_index--;
        }
    }
}

static void term_forward_char(ReadlineState *s)
{
    size_t c_len;
    if (s->term_cmd_buf_index < s->term_cmd_buf_len) {
        utf8_get(s->term_cmd_buf + s->term_cmd_buf_index, &c_len);
        s->term_cmd_buf_index += c_len;
    }
}

static void term_delete_char(ReadlineState *s)
{
    size_t c_len;
    if (s->term_cmd_buf_index < s->term_cmd_buf_len) {
        utf8_get(s->term_cmd_buf + s->term_cmd_buf_index, &c_len);
        term_kill_region(s, s->term_cmd_buf_index + c_len, 0);
    }
}

static void term_backspace(ReadlineState *s)
{
    if (s->term_cmd_buf_index > 0) {
        term_backward_char(s);
        term_delete_char(s);
    }
}

static int skip_word_backward(ReadlineState *s)
{
    int pos = s->term_cmd_buf_index;

    /* skip whitespace backwards */
    while (pos > 0 && isspace(s->term_cmd_buf[pos - 1]))
        --pos;

    /* skip word backwards */
    while (pos > 0 && !isspace(s->term_cmd_buf[pos - 1]))
        --pos;

    return pos;
}

static int skip_word_forward(ReadlineState *s)
{
    int pos = s->term_cmd_buf_index;

    /* skip whitespace */
    while (pos < s->term_cmd_buf_len && isspace(s->term_cmd_buf[pos]))
        pos++;

    /* skip word */
    while (pos < s->term_cmd_buf_len && !isspace(s->term_cmd_buf[pos]))
        pos++;

    return pos;
}

static void term_skip_word_backward(ReadlineState *s)
{
    s->term_cmd_buf_index = skip_word_backward(s);
}

static void term_skip_word_forward(ReadlineState *s)
{
    s->term_cmd_buf_index = skip_word_forward(s);
}

static void term_yank(ReadlineState *s)
{
    term_insert_region(s, s->term_kill_buf, s->term_kill_buf_len);
}

static void term_kill_word(ReadlineState *s)
{
    term_kill_region(s, skip_word_forward(s), 1);
}

static void term_kill_word_backward(ReadlineState *s)
{
    term_kill_region(s, skip_word_backward(s), 1);
}

static void term_bol(ReadlineState *s)
{
    s->term_cmd_buf_index = 0;
}

static void term_eol(ReadlineState *s)
{
    s->term_cmd_buf_index = s->term_cmd_buf_len;
}

static void update_cmdline_from_history(ReadlineState *s)
{
    int hist_entry_size;
    hist_entry_size = strlen(s->term_history + s->term_hist_entry);
    memcpy(s->term_cmd_buf, s->term_history + s->term_hist_entry, hist_entry_size);
    s->term_cmd_buf_len = hist_entry_size;
    s->term_cmd_buf_index = s->term_cmd_buf_len;
    s->term_cmd_updated = TRUE;
}

static void term_up_char(ReadlineState *s)
{
    int idx;
    if (s->term_hist_entry == -1) {
        s->term_hist_entry = s->term_history_size;
        // XXX: should save current contents to history
    }
    if (s->term_hist_entry == 0)
        return;
    /* move to previous entry */
    idx = s->term_hist_entry - 1;
    while (idx > 0 && s->term_history[idx - 1] != '\0')
        idx--;
    s->term_hist_entry = idx;
    update_cmdline_from_history(s);
}

static void term_down_char(ReadlineState *s)
{
    int hist_entry_size;
    if (s->term_hist_entry == -1)
        return;
    hist_entry_size = strlen(s->term_history + s->term_hist_entry) + 1;
    if (s->term_hist_entry + hist_entry_size < s->term_history_size) {
        s->term_hist_entry += hist_entry_size;
        update_cmdline_from_history(s);
    } else {
        s->term_hist_entry = -1;
        s->term_cmd_buf_index = s->term_cmd_buf_len;
    }
}

static void term_hist_add(ReadlineState *s, const char *cmdline)
{
    char *hist_entry;
    int idx, cmdline_size, hist_entry_size;

    if (cmdline[0] == '\0')
        return;
    cmdline_size = strlen(cmdline) + 1;
    if (s->term_hist_entry != -1) {
        /* We were editing an existing history entry: replace it */
        idx = s->term_hist_entry;
        hist_entry = s->term_history + idx;
        hist_entry_size = strlen(hist_entry) + 1;
        if (hist_entry_size == cmdline_size && !memcmp(hist_entry, cmdline, cmdline_size)) {
            goto same_entry;
        }
    }
    /* Search cmdline in the history */
    for (idx = 0; idx < s->term_history_size; idx += hist_entry_size) {
        hist_entry = s->term_history + idx;
        hist_entry_size = strlen(hist_entry) + 1;
        if (hist_entry_size == cmdline_size && !memcmp(hist_entry, cmdline, cmdline_size)) {
        same_entry:
            /* remove the identical entry */
            memmove(s->term_history + idx, s->term_history + idx + hist_entry_size,
                    s->term_history_size - (idx + hist_entry_size));
            s->term_history_size -= hist_entry_size;
            break;
        }
    }

    if (cmdline_size <= s->term_history_buf_size) {
        /* remove history entries if not enough space */
        while (s->term_history_size + cmdline_size > s->term_history_buf_size) {
            hist_entry_size = strlen(s->term_history) + 1;
            memmove(s->term_history, s->term_history + hist_entry_size,
                    s->term_history_size - hist_entry_size);
            s->term_history_size -= hist_entry_size;
        }

        /* add the cmdline */
        memcpy(s->term_history + s->term_history_size, cmdline, cmdline_size);
        s->term_history_size += cmdline_size;
    }
    s->term_hist_entry = -1;
}

/* completion support */

#if 0
void add_completion(const char *str)
{
    if (nb_completions < NB_COMPLETIONS_MAX) {
        completions[nb_completions++] = qemu_strdup(str);
    }
}

static void term_completion(ReadlineState *s)
{
    int len, i, j, max_width, nb_cols, max_prefix;
    char *cmdline;

    nb_completions = 0;

    cmdline = qemu_malloc(term_cmd_buf_index + 1);
    if (!cmdline)
        return;
    memcpy(cmdline, term_cmd_buf, term_cmd_buf_index);
    cmdline[term_cmd_buf_index] = '\0';
    readline_find_completion(cmdline);
    qemu_free(cmdline);

    /* no completion found */
    if (nb_completions <= 0)
        return;
    if (nb_completions == 1) {
        len = strlen(completions[0]);
        for(i = completion_index; i < len; i++) {
            term_insert_char(completions[0][i]);
        }
        /* extra space for next argument. XXX: make it more generic */
        if (len > 0 && completions[0][len - 1] != '/')
            term_insert_char(' ');
    } else {
        term_printf("\n");
        max_width = 0;
        max_prefix = 0;
        for(i = 0; i < nb_completions; i++) {
            len = strlen(completions[i]);
            if (i==0) {
                max_prefix = len;
            } else {
                if (len < max_prefix)
                    max_prefix = len;
                for(j=0; j<max_prefix; j++) {
                    if (completions[i][j] != completions[0][j])
                        max_prefix = j;
                }
            }
            if (len > max_width)
                max_width = len;
        }
        if (max_prefix > 0) {
            for(i = completion_index; i < max_prefix; i++) {
                term_insert_char(completions[0][i]);
            }
        }
        max_width += 2;
        if (max_width < 10)
            max_width = 10;
        else if (max_width > 80)
            max_width = 80;
        nb_cols = 80 / max_width;
        j = 0;
        for(i = 0; i < nb_completions; i++) {
            term_printf("%-*s", max_width, completions[i]);
            if (++j == nb_cols || i == (nb_completions - 1)) {
                term_printf("\n");
                j = 0;
            }
        }
        term_show_prompt2();
    }
}
#endif

static void term_return(ReadlineState *s)
{
    s->term_cmd_buf[s->term_cmd_buf_len] = '\0';
    if (!s->term_is_password)
        term_hist_add(s, (const char *)s->term_cmd_buf);
    s->term_cmd_buf_index = s->term_cmd_buf_len;
}

static int readline_handle_char(ReadlineState *s, int ch)
{
    int ret = READLINE_RET_HANDLED;
    
    switch(s->term_esc_state) {
    case IS_NORM:
        switch(ch) {
        case 1: /* ^A */
            term_bol(s);
            break;
        case 4: /* ^D */
            if (s->term_cmd_buf_len == 0) {
                term_printf("^D\n");
                return READLINE_RET_EXIT;
            }
            term_delete_char(s);
            break;
        case 5: /* ^E */
            term_eol(s);
            break;
        case 9: /* TAB */
            //term_completion(s);
            break;
        case 10:
        case 13:
            term_return(s);
            ret = READLINE_RET_ACCEPTED;
            break;
        case 11: /* ^K */
            term_kill_region(s, s->term_cmd_buf_len, 1);
            break;
        case 21: /* ^U */
            term_kill_region(s, 0, 1);
            break;
        case 23: /* ^W */
            term_kill_word_backward(s);
            break;
        case 25: /* ^Y */
            term_yank(s);
            break;
        case 27:
            s->term_esc_state = IS_ESC;
            break;
        case 127: /* DEL */
        case 8: /* ^H */
            term_backspace(s);
            break;
        case 155: /* 0x9B */
            s->term_esc_state = IS_CSI;
            break;
        default:
            if (ch >= 32) {
                term_insert_char(s, ch);
                break;
            }
            return 0;
        }
        break;
    case IS_ESC:
        s->term_esc_state = IS_NORM;
        switch (ch) {
        case '[':
        case 'O':
            s->term_esc_state = IS_CSI;
            s->term_esc_param2 = 0;
            s->term_esc_param1 = 0;
            s->term_esc_param = 0;
            break;
        case 13:
            /* ESC+RET or M-RET: validate in multi-line */
            term_return(s);
            break;
        case 8:
        case 127:
            term_kill_word_backward(s);
            break;
        case 'b':
            term_skip_word_backward(s);
            break;
        case 'd':
            term_kill_word(s);
            break;
        case 'f':
            term_skip_word_forward(s);
            break;
        default:
            return 0;
        }
        break;
    case IS_CSI:
        s->term_esc_state = IS_NORM;
        switch(ch) {
        case 'A':
            term_up_char(s);
            break;
        case 'B':
        case 'E':
            term_down_char(s);
            break;
        case 'D':
            term_backward_char(s);
            break;
        case 'C':
            term_forward_char(s);
            break;
        case 'F':
            term_eol(s);
            break;
        case 'H':
            term_bol(s);
            break;
        case ';':
            s->term_esc_param2 = s->term_esc_param1;
            s->term_esc_param1 = s->term_esc_param;
            s->term_esc_param = 0;
            s->term_esc_state = IS_CSI;
            break;
        case '0' ... '9':
            s->term_esc_param = s->term_esc_param * 10 + (ch - '0');
            s->term_esc_state = IS_CSI;
            break;
        case '~':
            switch(s->term_esc_param) {
            case 1:
                term_bol(s);
                break;
            case 3:
                term_delete_char(s);
                break;
            case 4:
                term_eol(s);
                break;
            default:
                return READLINE_RET_NOT_HANDLED;
            }
            break;
        default:
            return READLINE_RET_NOT_HANDLED;
        }
        break;
    }
    term_update(s);
    if (ret == READLINE_RET_ACCEPTED) {
        term_printf("\n");
    }
    return ret;
}

/* return > 0 if command handled, -1 if exit */
/* XXX: could process buffers to avoid redisplaying at each char input
   (copy paste case) */
int readline_handle_byte(ReadlineState *s, int c)
{
    if (c < 0x80) {
        s->utf8_pos = 0;
        return readline_handle_char(s, c);
    } else if (c >= 0x80 && c < 0xc0) {
        if (s->utf8_pos != 0) {
            s->utf8_buf[s->utf8_pos++] = c;
            if (s->utf8_pos == s->utf8_len) {
                size_t clen;
                s->utf8_pos = 0;
                /* only accept valid UTF-8 */
                c = unicode_from_utf8(s->utf8_buf, s->utf8_len, &clen);
                if (c >= 0) {
                    return readline_handle_char(s, c);
                }
            }
        }
    } else if (c < 0xf8) {
        s->utf8_buf[0] = c;
        s->utf8_pos = 1;
        s->utf8_len = 2 + (c >= 0xe0) + (c >= 0xf0);
    } else {
        s->utf8_pos = 0;
    }
    return READLINE_RET_HANDLED;
}

void readline_start(ReadlineState *s, const char *prompt, int is_password)
{
    s->term_prompt = prompt;
    s->term_is_password = is_password;
    s->term_hist_entry = -1;
    s->term_cmd_buf_index = 0;
    s->term_cmd_buf_len = 0;
    term_show_prompt(s);
}
