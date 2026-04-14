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

int DecompressBzip2(const REBYTE *input, REBLEN len, REBCNT limit, REBSER **output, REBINT *error) {
	REBU64 out_len;

	out_len = (limit != NO_LIMIT) ? (REBU64)limit : (REBU64)len << 2;

	if (out_len == 0) {
		*output = RL_MAKE_BINARY(1);
		return TRUE;
	}
	if (out_len > MAX_I32) out_len = MAX_I32;
	*output = RL_MAKE_BINARY((REBLEN)out_len);

	unsigned int destLen = (unsigned int)SERIES_REST(*output);

	int ret = BZ2_bzBuffToBuffDecompress(
		(char *)BIN_HEAD(*output), &destLen,
		(char *)input, (unsigned int)len,
		0,
		0
	);

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

	if (limit != NO_LIMIT && destLen > (unsigned int)limit)
		destLen = (unsigned int)limit;

	SERIES_TAIL(*output) = (REBLEN)destLen;
	return TRUE;
}

COMMAND cmd_decompress(RXIFRM *frm, void *ctx) {
	REBSER *data    = RXA_SERIES(frm, 1);
	REBINT index    = RXA_INDEX(frm, 1);
	REBFLG ref_part = RXA_REF(frm, 2);
	REBI64 length   = SERIES_TAIL(data) - index;
	REBCNT limit_u  = NO_LIMIT;
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

	if (!DecompressBzip2((const REBYTE*)BIN_SKIP(data, index), (REBLEN)length, limit_u, &output, &error)) {
		RETURN_ERROR(ERR_NO_DECOMPRESS);
	}

	RXA_SERIES(frm, 1) = output;
	RXA_TYPE(frm, 1) = RXT_BINARY;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}
