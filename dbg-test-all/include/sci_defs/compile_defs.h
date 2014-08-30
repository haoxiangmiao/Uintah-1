/*
 * The MIT License
 *
 * Copyright (c) 1997-2014 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef SCI_COMPILE_DEFS_H
#define SCI_COMPILE_DEFS_H

#define MAKE_COMMAND "/usr/bin/make"

// This define is used to allow the code to display (some of) the
// flags that were used to compile it.
#define CFLAGS "-fPIC  -fopenmp -Wall  -g -O0 -fno-inline-functions "

#ifdef _MSC_VER
#  define PATH_TO_VC "@PATH_TO_VC@"
#  define PATH_TO_PSDK "@PATH_TO_PSDK@"
#  define PATH_TO_MSYS_BIN "@PATH_TO_MSYS_BIN@"
#endif



#endif

