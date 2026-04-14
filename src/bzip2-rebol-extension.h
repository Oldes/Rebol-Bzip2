//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Bzip2 extension
// SPDX-License-Identifier: MIT
// =============================================================================
// NOTE: auto-generated file, do not modify!


#include "rebol-extension.h"
#include "bzlib.h"


#define SERIES_TEXT(s)   ((char*)SERIES_DATA(s))

#define MIN_REBOL_VER 3
#define MIN_REBOL_REV 20
#define MIN_REBOL_UPD 5
#define VERSION(a, b, c) (a << 16) + (b << 8) + c
#define MIN_REBOL_VERSION VERSION(MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD)

extern u32* arg_words;
extern u32* type_words;

enum ext_commands {
	CMD_BZIP2_INIT_WORDS,
	CMD_BZIP2_VERSION,
	CMD_BZIP2_COMPRESS,
	CMD_BZIP2_DECOMPRESS,
};


int cmd_init_words(RXIFRM *frm, void *ctx);
int cmd_version(RXIFRM *frm, void *ctx);
int cmd_compress(RXIFRM *frm, void *ctx);
int cmd_decompress(RXIFRM *frm, void *ctx);

enum ma_arg_words {W_ARG_0
};
enum ma_type_words {W_TYPE_0
};

typedef int (*MyCommandPointer)(RXIFRM *frm, void *ctx);

#define BZIP2_EXT_INIT_CODE \
	"REBOL [Title: \"Rebol Bzip2 Extension\" Name: Bzip2 Type: module Version: 1.1.0 Needs: 3.20.5 Author: Oldes Date: 14-Apr-2026/5:29 License: MIT Url: https://github.com/Oldes/Rebol-Bzip2]\n"\
	"init-words: command [args [block!] type [block!]]\n"\
	"version: command [\"Libbzip2 version string (BZ2_bzlibVersion)\"]\n"\
	"compress: command [\"Compress data using bzip2\" data [binary! any-string!] \"Input data to compress.\" /part \"Limit the input data to a given length.\" length [integer!] \"Length of input data.\" /level quality [integer!] \"Block size 100k: 1 (fast) to 9 (best).\"]\n"\
	"decompress: command [\"Decompress bzip2 data\" data [binary! any-string!] \"Input data to decompress.\" /part \"Limit the input data to a given length.\" length [integer!] \"Length of input data.\" /size \"Limit the output size.\" bytes [integer!] \"Maximum number of uncompressed bytes.\" /max \"Cap allocated output (ZIP bomb guard).\" ceiling [integer!] \"Maximum bytes to allocate while decompressing.\"]\n"\
	"init-words [][]\n"\
	"protect/hide 'init-words\n"\
	"\n"

#ifdef  USE_TRACES
#include <stdio.h>
#define debug_print(fmt, ...) do { printf(fmt, __VA_ARGS__); } while (0)
#define trace(str) puts(str)
#else
#define debug_print(fmt, ...)
#define trace(str) 
#endif

