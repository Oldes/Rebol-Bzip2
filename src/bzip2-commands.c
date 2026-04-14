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
#include <string.h>

#define COMMAND int

#define RETURN_ERROR(err)  do {RXA_SERIES(frm, 1) = (REBSER*)err; return RXR_ERROR;} while(0)
static const REBYTE* ERR_NO_COMPRESS   = (const REBYTE*)"Bzip2 compression failed.";
static const REBYTE* ERR_NO_DECOMPRESS = (const REBYTE*)"Bzip2 decompression failed.";
static const REBYTE* ERR_BAD_SIZE      = (const REBYTE*)"Invalid output size limit (/size).";
static const REBYTE* ERR_BAD_MAX       = (const REBYTE*)"Invalid /max (must be >= 0).";

/* One-shot decompress: grow output until BZ_OK or non-recoverable error. */
#define BZIP2_DECOMP_MAX_ATTEMPTS 32
#define BZIP2_DECOMP_MAX_OUT ((REBU64)MAX_I32)

COMMAND cmd_init_words(RXIFRM *frm, void *ctx) {
	arg_words  = RL_MAP_WORDS(RXA_SERIES(frm,1));
	type_words = RL_MAP_WORDS(RXA_SERIES(frm,2));
	return RXR_TRUE;
}

COMMAND cmd_version(RXIFRM *frm, void *ctx) {
	const char *ver = BZ2_bzlibVersion();
	REBCNT n = (REBCNT)strlen(ver);
	REBSER *s = RL_MAKE_STRING(n + 1, FALSE);
	memcpy(STR_HEAD(s), ver, n + 1);
	SERIES_TAIL(s) = n;
	RXA_SERIES(frm, 1) = s;
	RXA_TYPE(frm, 1) = RXT_STRING;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}

int CompressBzip2(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error) {
	unsigned int bound = (unsigned int)(len + (len / 100) + 600);
	if (bound > MAX_I32) return FALSE;

	*output = RL_MAKE_BINARY((REBLEN)bound);

	unsigned int destLen = (unsigned int)SERIES_REST(*output);
	int blockSize = (level == 0) ? 9 : MAX(1, MIN(9, (int)level));

	int ret = BZ2_bzBuffToBuffCompress(
		(char *)BIN_HEAD(*output), &destLen,
		(char *)input, (unsigned int)len,
		blockSize,
		0,
		0
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

/* Core one-shot decompress; max_alloc 0 means use BZIP2_DECOMP_MAX_OUT cap only. */
static int decompress_bzip2_impl(
	const REBYTE *input,
	REBLEN len,
	REBCNT limit,
	REBU64 max_alloc,
	REBSER **output,
	REBINT *error
) {
	REBU64 cap = BZIP2_DECOMP_MAX_OUT;
	REBU64 out_len;
	REBCNT attempt;
	int ret;
	unsigned int destLen;
	REBSER *ser;

	if (max_alloc != 0) {
		if (max_alloc > BZIP2_DECOMP_MAX_OUT)
			max_alloc = BZIP2_DECOMP_MAX_OUT;
		cap = max_alloc;
	}

	if (limit != NO_LIMIT) {
		out_len = (REBU64)limit;
		if (out_len > cap)
			out_len = cap;
	} else {
		out_len = (REBU64)len << 2;
		if (len != 0 && out_len < (REBU64)len)
			out_len = cap;
		if (out_len > cap)
			out_len = cap;
	}

	if (out_len == 0) {
		ser = RL_MAKE_BINARY(1);
		if (!ser) {
			if (error) *error = BZ_MEM_ERROR;
			*output = NULL;
			return FALSE;
		}
		*output = ser;
		return TRUE;
	}

	for (attempt = 0; attempt < BZIP2_DECOMP_MAX_ATTEMPTS; attempt++) {
		ser = RL_MAKE_BINARY((REBLEN)out_len);
		if (!ser) {
			if (error) *error = BZ_MEM_ERROR;
			*output = NULL;
			return FALSE;
		}
		*output = ser;
		destLen = (unsigned int)SERIES_REST(*output);

		ret = BZ2_bzBuffToBuffDecompress(
			(char *)BIN_HEAD(*output), &destLen,
			(char *)input, (unsigned int)len,
			0,
			0
		);

		if (ret == BZ_OK) {
			if (limit != NO_LIMIT && destLen > (unsigned int)limit)
				destLen = (unsigned int)limit;
			SERIES_TAIL(*output) = (REBLEN)destLen;
			return TRUE;
		}

		if (ret == BZ_OUTBUFF_FULL) {
			if (limit != NO_LIMIT) {
				if (error) *error = ret;
				return FALSE;
			}
			{
				REBU64 next = out_len << 2;
				if (next < out_len)
					next = cap;
				if (next > cap)
					next = cap;
				if (next <= out_len) {
					if (error) *error = ret;
					return FALSE;
				}
				out_len = next;
				continue;
			}
		}

		if (error) *error = ret;
		return FALSE;
	}

	if (error) *error = BZ_OUTBUFF_FULL;
	return FALSE;
}

/* Registered with Rebol as codec decoder (DECOMPRESS_FUNC). */
int DecompressBzip2(const REBYTE *input, REBLEN in_len, REBLEN out_limit, REBSER **output, REBINT *error) {
	return decompress_bzip2_impl(input, in_len, (REBCNT)out_limit, 0, output, error);
}

COMMAND cmd_decompress(RXIFRM *frm, void *ctx) {
	REBSER *data    = RXA_SERIES(frm, 1);
	REBINT index    = RXA_INDEX(frm, 1);
	REBFLG ref_part = RXA_REF(frm, 2);
	REBI64 length   = SERIES_TAIL(data) - index;
	REBCNT limit_u  = NO_LIMIT;
	REBU64 max_alloc = 0;
	REBSER *output  = NULL;
	REBINT  error   = 0;

	if (ref_part) length = MAX(0, MIN(length, RXA_INT64(frm, 3)));
	if (length < 0 || length > MAX_I32) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	if (RXA_REF(frm, 4)) {
		REBI64 lim = RXA_INT64(frm, 5);
		if (lim < 0) {
			RETURN_ERROR(ERR_BAD_SIZE);
		}
		if (lim > (REBI64)MAX_I32) {
			limit_u = (REBCNT)MAX_I32;
		} else {
			limit_u = (REBCNT)lim;
		}
	}

	if (RXA_REF(frm, 6)) {
		REBI64 ma = RXA_INT64(frm, 7);
		if (ma < 0) {
			RETURN_ERROR(ERR_BAD_MAX);
		}
		max_alloc = (ma > (REBI64)MAX_I32) ? (REBU64)MAX_I32 : (REBU64)ma;
	}

	if (!decompress_bzip2_impl((const REBYTE*)BIN_SKIP(data, index), (REBLEN)length, limit_u, max_alloc, &output, &error)) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}
