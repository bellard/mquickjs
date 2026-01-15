/*
 * Readline TTY support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
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
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

#include "readline_tty.h"

static int ctrl_c_pressed;

#ifdef _WIN32
/* Windows 10 built-in VT100 emulation */
#define __ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define __ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200

static BOOL WINAPI ctrl_handler(DWORD type)
{
    if (type == CTRL_C_EVENT) {
        ctrl_c_pressed++;
        if (ctrl_c_pressed >= 4) {
            /* just to be able to stop the process if it is hanged */
            return FALSE;
        } else {
            return TRUE;
        }
    } else {
        return FALSE;
    }
}

int readline_tty_init(void)
{
    HANDLE handle;
    CONSOLE_SCREEN_BUFFER_INFO info;
    int n_cols;
    
    handle = (HANDLE)_get_osfhandle(0);
    SetConsoleMode(handle, ENABLE_WINDOW_INPUT | __ENABLE_VIRTUAL_TERMINAL_INPUT);
    _setmode(0, _O_BINARY);

    handle = (HANDLE)_get_osfhandle(1); /* corresponding output */
    SetConsoleMode(handle, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | __ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    n_cols = 80;
    if (GetConsoleScreenBufferInfo(handle, &info)) {
        n_cols = info.dwSize.X;
    }
    return n_cols;
}

/* if processed input is enabled, Ctrl-C is handled by ctrl_handler() */
static void set_processed_input(BOOL enable)
{
    DWORD mode;
    HANDLE handle;

    handle = (HANDLE)_get_osfhandle(0);
    if (!GetConsoleMode(handle, &mode))
        return;
    if (enable)
        mode |= ENABLE_PROCESSED_INPUT;
    else
        mode &= ~ENABLE_PROCESSED_INPUT;
    SetConsoleMode(handle, mode);
}

#else
/* init terminal so that we can grab keys */
/* XXX: merge with cp_utils.c */
static struct termios oldtty;
static int old_fd0_flags;

static void term_exit(void)
{
    tcsetattr (0, TCSANOW, &oldtty);
    fcntl(0, F_SETFL, old_fd0_flags);
}

static void sigint_handler(int signo)
{
    ctrl_c_pressed++;
    if (ctrl_c_pressed >= 4) {
        /* just to be able to stop the process if it is hanged */
        signal(SIGINT, SIG_DFL);
    }
}

int readline_tty_init(void)
{
    struct termios tty;
    struct sigaction sa;
    struct winsize ws;
    int n_cols;
    
    tcgetattr (0, &tty);
    oldtty = tty;
    old_fd0_flags = fcntl(0, F_GETFL);

    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                          |INLCR|IGNCR|ICRNL|IXON);
    tty.c_oflag |= OPOST;
    tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
    //    tty.c_lflag &= ~ISIG; /* ctrl-C returns a signal */
    tty.c_cflag &= ~(CSIZE|PARENB);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    tcsetattr (0, TCSANOW, &tty);
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    atexit(term_exit);

    //    fcntl(0, F_SETFL, O_NONBLOCK);
    n_cols = 80;
    if (ioctl(0, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_col >= 4 && ws.ws_row >= 4) {
        n_cols = ws.ws_col;
    }
    return n_cols;
}
#endif

void term_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void term_flush(void)
{
    fflush(stdout);
}

const char *readline_tty(ReadlineState *s,
                         const char *prompt, BOOL multi_line)
{
    int len, i, ctrl_c_count, c, ret;
    const char *ret_str;
    uint8_t buf[128];
    
#ifdef _WIN32
    set_processed_input(FALSE);
    /* ctrl-C is no longer handled by the system */
#endif
    ret_str = NULL;
    readline_start(s, prompt, FALSE);
    ctrl_c_count = 0;
    while (ret_str == NULL) {
        len = read(0, buf, sizeof(buf));
        if (len == 0)
            break;
        for(i = 0; i < len; i++) {
            c = buf[i];
#ifdef _WIN32
            if (c == 3) {
                /* ctrl-C */
                ctrl_c_pressed++;
            } else
#endif
            {
                ret = readline_handle_byte(s, c);
                if (ret == READLINE_RET_EXIT) {
                    goto done;
                } else if (ret == READLINE_RET_ACCEPTED) {
                    ret_str = (const char *)s->term_cmd_buf;
                    goto done;
                }
                ctrl_c_count = 0;
            }
        }
        if (ctrl_c_pressed) {
            ctrl_c_pressed = 0;
            if (ctrl_c_count == 0) {
                printf("(Press Ctrl-C again to quit)\n");
                ctrl_c_count++;
            } else {
                printf("Exiting.\n");
                break;
            }
        }
    }
done:
#ifdef _WIN32
    set_processed_input(TRUE);
#endif
    return ret_str;
}

BOOL readline_is_interrupted(void)
{
    BOOL ret;
    ret = (ctrl_c_pressed != 0);
    ctrl_c_pressed = 0;
    return ret;
}
