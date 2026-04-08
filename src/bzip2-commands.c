//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// SPDX-License-Identifier: MIT
// =============================================================================
// Rebol/Bzip2 extension commands
// =============================================================================


#include "bzip2-rebol-extension.h"
#include <stdio.h>
#include <stdlib.h> // malloc
#include <math.h>   // fmin, fmax

#define COMMAND int

#define FRM_IS_HANDLE(n, t)   (RXA_TYPE(frm,n) == RXT_HANDLE && RXA_HANDLE_TYPE(frm, n) == t)
#define ARG_Double(n)         RXA_DEC64(frm,n)
#define ARG_Float(n)          (float)RXA_DEC64(frm,n)
#define ARG_Int32(n)          RXA_INT32(frm,n)
#define ARG_Handle_Series(n)  RXA_HANDLE_CONTEXT(frm, n)->series;
#define OPT_SERIES(n)         (RXA_TYPE(frm,n) == RXT_NONE ? NULL : RXA_SERIES(frm, n))

#define RETURN_HANDLE(hob)                   \
	RXA_HANDLE(frm, 1)       = hob;          \
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;     \
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;   \
	RXA_TYPE(frm, 1) = RXT_HANDLE;           \
	return RXR_VALUE

#define RETURN_ERROR(err)  do {RXA_SERIES(frm, 1) = (REBSER*)err; return RXR_ERROR;} while(0)
static const REBYTE* ERR_INVALID_HANDLE = (const REBYTE*)"Invalid Bzip2 encoder or decoder handle!";
static const REBYTE* ERR_NO_DECODER     = (const REBYTE*)"Failed to create Bzip2 decoder!";
static const REBYTE* ERR_NO_ENCODER     = (const REBYTE*)"Failed to create Bzip2 encoder!";
static const REBYTE* ERR_NO_COMPRESS    = (const REBYTE*)"Failed to compress using the Bzip2 encoder!";
static const REBYTE* ERR_NO_DECOMPRESS  = (const REBYTE*)"Failed to decompress using the Bzip2 decoder!";
static const REBYTE* ERR_ENCODER_FINISHED = (const REBYTE*)"Bzip2 encoder is finished!";

#define APPEND_STRING(str, ...) \
	len = snprintf(NULL,0,__VA_ARGS__);\
	if (len > SERIES_REST(str)-SERIES_LEN(str)) {\
		RL_EXPAND_SERIES(str, SERIES_TAIL(str), len);\
		SERIES_TAIL(str) -= len;\
	}\
	len = snprintf( \
		SERIES_TEXT(str)+SERIES_TAIL(str),\
		SERIES_REST(str)-SERIES_TAIL(str),\
		__VA_ARGS__\
	);\
	SERIES_TAIL(str) += len;

int Common_mold(REBHOB *hob, REBSER *str) {
	size_t len;
	if (!str) return 0;
	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "0#%lx", (unsigned long)(uintptr_t)hob->handle);
	return len;
}


COMMAND cmd_init_words(RXIFRM *frm, void *ctx) {
	arg_words  = RL_MAP_WORDS(RXA_SERIES(frm,1));
	type_words = RL_MAP_WORDS(RXA_SERIES(frm,2));

	// custom initialization may be done here...

	return RXR_TRUE;
}

COMMAND cmd_version(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = RXT_TUPLE;
	RXA_TUPLE(frm, 1)[0] = 1;
	RXA_TUPLE(frm, 1)[1] = 1;
	RXA_TUPLE(frm, 1)[2] = 0;
	RXA_TUPLE_LEN(frm, 1) = 3;
	return RXR_VALUE;
}


int CompressBzip2(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
    // bzip2 worst-case output size: input + 1% + 600 bytes
    unsigned int bound = (unsigned int)(len + (len / 100) + 600);
    if (bound > MAX_I32) return FALSE;

    *output = RL_MAKE_BINARY((REBLEN)bound);

    unsigned int destLen = (unsigned int)SERIES_REST(*output);

    // level maps to blockSize100k: 1 (fast) to 9 (best), default 9
    int blockSize = (level == 0) ? 9 : MAX(1, MIN(9, (int)level));

    int ret = BZ2_bzBuffToBuffCompress(
        (char *)BIN_HEAD(*output), &destLen,
        (char *)input, (unsigned int)len,
        blockSize,  // block size (compression level)
        0,          // verbosity
        0           // workFactor (0 = default 30)
    );

    if (ret != BZ_OK) {
        if (error) *error = ret;
        return FALSE;
    }

    SERIES_TAIL(*output) = (REBLEN)destLen;
    return TRUE;
}


COMMAND cmd_compress(RXIFRM *frm, void *ctx) {
	REBSER *data    = RXA_SERIES(frm, 1);
	REBINT index    = RXA_INDEX(frm, 1);
	REBFLG ref_part = RXA_REF(frm, 2);
	REBLEN length   = SERIES_TAIL(data) - index;
	REBINT level    = RXA_REF(frm, 4) ? RXA_INT32(frm, 5) : 6;
	REBSER *output  = NULL;
	REBINT  error   = 0;

	if (ref_part) length = (REBLEN)MAX(0, MIN(length, RXA_INT64(frm, 3)));

	if (!CompressBzip2((const REBYTE*)BIN_SKIP(data, index), length, (REBCNT)level, &output, &error)) {
		RETURN_ERROR(ERR_NO_COMPRESS);
	}

	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}

int DecompressBzip2(const REBYTE *input, REBLEN len, REBLEN limit, REBSER **output, REBINT *error) {
    REBU64 out_len;

    out_len = (limit != NO_LIMIT) ? limit : (REBU64)len << 2;

    if (out_len == 0) {
        // Return empty binary.
        *output = RL_MAKE_BINARY(1);
        return TRUE;
    }
    if (out_len > MAX_I32) out_len = MAX_I32;
    *output = RL_MAKE_BINARY((REBLEN)out_len);

    unsigned int destLen = (unsigned int)SERIES_REST(*output);

    int ret = BZ2_bzBuffToBuffDecompress(
        (char *)BIN_HEAD(*output), &destLen,
        (char *)input, (unsigned int)len,
        0,  // small (0 = fast mode, 1 = low memory)
        0   // verbosity
    );

    // If output buffer was too small, retry with a larger buffer
    if (ret == BZ_OUTBUFF_FULL && limit == NO_LIMIT) {
        out_len = MIN((REBU64)out_len << 2, (REBU64)MAX_I32);
        *output = RL_MAKE_BINARY((REBLEN)out_len);
        destLen = (unsigned int)SERIES_REST(*output);

        ret = BZ2_bzBuffToBuffDecompress(
            (char *)BIN_HEAD(*output), &destLen,
            (char *)input, (unsigned int)len,
            0, 0
        );
    }

    if (ret != BZ_OK) {
        if (error) *error = ret;
        return FALSE;
    }

    if (limit != NO_LIMIT && destLen > (unsigned int)limit) destLen = (unsigned int)limit;

    SERIES_TAIL(*output) = (REBLEN)destLen;
    return TRUE;
}

COMMAND cmd_decompress(RXIFRM *frm, void *ctx) {
	REBSER *data    = RXA_SERIES(frm, 1);
	REBINT index    = RXA_INDEX(frm, 1);
	REBFLG ref_part = RXA_REF(frm, 2);
	REBI64 length   = SERIES_TAIL(data) - index;
	REBI64 limit    = RXA_REF(frm, 4) ? RXA_INT64(frm, 5) : NO_LIMIT;
	REBSER *output  = NULL;
	REBINT  error   = 0;

	if (ref_part) length = MAX(0, MIN(length, RXA_INT64(frm, 3)));
	if (length < 0 || length > MAX_I32) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	if (!DecompressBzip2((const REBYTE*)BIN_SKIP(data, index), (REBLEN)length, (REBCNT)limit, &output, &error)) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}
