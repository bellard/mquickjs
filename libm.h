/*
 * Tiny Math Library
 *
 * Copyright (c) 2024 Fabrice Bellard
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
#include <stdint.h>

double js_scalbn(double x, int n);
double js_floor(double x);
double js_ceil(double x);
double js_trunc(double x);
double js_round_inf(double a);
double js_fabs(double x);
double js_sqrt(double x);
int32_t js_lrint(double a);
double js_fmod(double x, double y);
double js_sin(double x);
double js_cos(double x);
double js_tan(double x);
double js_acos(double x);
double js_asin(double x);
double js_atan(double x);
double js_atan2(double y, double x);
double js_exp(double x);
double js_log(double x);
double js_log2(double x);
double js_log10(double x);
double js_pow(double x, double y);
/* exported only for tests */
int js_rem_pio2(double x, double *y);
