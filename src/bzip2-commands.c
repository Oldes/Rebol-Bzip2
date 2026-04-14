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
#include <string.h>

#define COMMAND int

#define RETURN_ERROR(err)  do {RXA_SERIES(frm, 1) = (REBSER*)err; return RXR_ERROR;} while(0)
#define OPT_SERIES(n)      (RXA_TYPE(frm,n) == RXT_NONE ? NULL : RXA_SERIES(frm, n))
#define RETURN_HANDLE(hob)                   \
	RXA_HANDLE(frm, 1)       = hob;          \
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;     \
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;   \
	RXA_TYPE(frm, 1) = RXT_HANDLE;           \
	return RXR_VALUE

static const REBYTE* ERR_NO_COMPRESS   = (const REBYTE*)"Bzip2 compression failed.";
static const REBYTE* ERR_NO_DECOMPRESS = (const REBYTE*)"Bzip2 decompression failed.";
static const REBYTE* ERR_BAD_SIZE      = (const REBYTE*)"Invalid output size limit (/size).";
static const REBYTE* ERR_BAD_MAX       = (const REBYTE*)"Invalid /max (must be >= 0).";
static const REBYTE* ERR_INVALID_HANDLE = (const REBYTE*)"Invalid bzip2 streaming handle.";
static const REBYTE* ERR_NO_ENCODER    = (const REBYTE*)"Failed to create bzip2 encoder.";
static const REBYTE* ERR_NO_DECODER    = (const REBYTE*)"Failed to create bzip2 decoder.";
static const REBYTE* ERR_STREAM_CLOSED = (const REBYTE*)"Bzip2 stream is already finished.";

typedef struct Bzip2StreamCtx {
	bz_stream strm;
	int is_encoder;
	int inited;
	int lib_closed;
} Bzip2StreamCtx;

static void bzip2_ctx_library_close(Bzip2StreamCtx *c) {
	if (!c || !c->inited || c->lib_closed)
		return;
	if (c->is_encoder)
		BZ2_bzCompressEnd(&c->strm);
	else
		BZ2_bzDecompressEnd(&c->strm);
	c->lib_closed = 1;
}

int Common_mold(REBHOB *hob, REBSER *str) {
	int len;
	if (!str || !hob)
		return 0;
	SERIES_TAIL(str) = 0;
	len = snprintf(
		SERIES_TEXT(str),
		SERIES_REST(str),
		"bzip2:%s:%p",
		(hob->sym == Handle_Bzip2Encoder) ? "enc" : "dec",
		(void *)hob->handle
	);
	if (len < 0 || (REBCNT)len >= SERIES_REST(str))
		return 0;
	SERIES_TAIL(str) = (REBCNT)len;
	return len;
}

int Bzip2Handle_free(void *hndl) {
	REBHOB *hob = (REBHOB *)hndl;
	Bzip2StreamCtx *ctx;

	if (!hob)
		return 0;
	ctx = (Bzip2StreamCtx *)hob->handle;
	if (ctx) {
		bzip2_ctx_library_close(ctx);
		RL_MEM_FREE(RL, ctx);
		hob->handle = NULL;
	}
	UNMARK_HOB(hob);
	return 0;
}

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
				/* /size: return truncated output instead of failing */
				SERIES_TAIL(*output) = (REBLEN)out_len;
				return TRUE;
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

/* --- Streaming (bz_stream) ------------------------------------------------ */

static int bzip2_ensure_avail_out(REBSER *buf, REBLEN min_free) {
	REBLEN tail = SERIES_TAIL(buf);
	while ((REBLEN)(SERIES_REST(buf) - tail) < min_free) {
		RL_EXPAND_SERIES(buf, tail, SERIES_REST(buf));
		SERIES_TAIL(buf) = tail;
	}
	return TRUE;
}

COMMAND cmd_make_encoder(RXIFRM *frm, void *ctx) {
	int block = RXA_REF(frm, 1)
		? MAX(1, MIN(9, RXA_INT32(frm, 2)))
		: 6;
	Bzip2StreamCtx *c = (Bzip2StreamCtx *)RL_MEM_ALLOC(RL, sizeof(Bzip2StreamCtx));
	REBHOB *hob;
	int ret;

	if (!c)
		RETURN_ERROR(ERR_NO_ENCODER);
	memset(c, 0, sizeof(*c));
	c->is_encoder = 1;
	ret = BZ2_bzCompressInit(&c->strm, block, 0, 0);
	if (ret != BZ_OK) {
		RL_MEM_FREE(RL, c);
		RETURN_ERROR(ERR_NO_ENCODER);
	}
	c->inited = 1;
	hob = RL_MAKE_HANDLE_CONTEXT(Handle_Bzip2Encoder);
	if (!hob) {
		BZ2_bzCompressEnd(&c->strm);
		RL_MEM_FREE(RL, c);
		RETURN_ERROR(ERR_NO_ENCODER);
	}
	hob->handle = c;
	RETURN_HANDLE(hob);
}

COMMAND cmd_make_decoder(RXIFRM *frm, void *ctx) {
	Bzip2StreamCtx *c = (Bzip2StreamCtx *)RL_MEM_ALLOC(RL, sizeof(Bzip2StreamCtx));
	REBHOB *hob;
	int ret;

	if (!c)
		RETURN_ERROR(ERR_NO_DECODER);
	memset(c, 0, sizeof(*c));
	c->is_encoder = 0;
	ret = BZ2_bzDecompressInit(&c->strm, 0, 0);
	if (ret != BZ_OK) {
		RL_MEM_FREE(RL, c);
		RETURN_ERROR(ERR_NO_DECODER);
	}
	c->inited = 1;
	hob = RL_MAKE_HANDLE_CONTEXT(Handle_Bzip2Decoder);
	if (!hob) {
		BZ2_bzDecompressEnd(&c->strm);
		RL_MEM_FREE(RL, c);
		RETURN_ERROR(ERR_NO_DECODER);
	}
	hob->handle = c;
	RETURN_HANDLE(hob);
}

COMMAND cmd_write(RXIFRM *frm, void *ctx) {
	REBHOB *hob = RXA_HANDLE(frm, 1);
	REBSER *data = OPT_SERIES(2);
	REBINT index = RXA_INDEX(frm, 2);
	REBOOL ref_flush = RXA_REF(frm, 3);
	REBOOL ref_finish = RXA_REF(frm, 4);
	Bzip2StreamCtx *ctxx;
	REBSER *buffer;
	bz_stream *s;
	int ret;

	if (!hob || hob->handle == NULL
		|| !(hob->sym == Handle_Bzip2Encoder || hob->sym == Handle_Bzip2Decoder)) {
		RETURN_ERROR(ERR_INVALID_HANDLE);
	}
	ctxx = (Bzip2StreamCtx *)hob->handle;
	if (ctxx->lib_closed) {
		RETURN_ERROR(ERR_STREAM_CLOSED);
	}
	s = &ctxx->strm;
	buffer = hob->series;

	if (!data) {
		if (!buffer)
			return RXR_NONE;
		/* NONE is only meaningful for encoders (finish); decoders ignore. */
		if (hob->sym != Handle_Bzip2Encoder)
			return RXR_NONE;
		ref_finish = TRUE;
	} else {
		s->next_in = (char *)BIN_SKIP(data, index);
		s->avail_in = (unsigned int)(SERIES_TAIL(data) - index);
		if (buffer == NULL) {
			REBLEN init = (REBLEN)MAX(65536, (REBINT)s->avail_in * 2 + 256);
			buffer = hob->series = RL_MAKE_BINARY(init);
			if (!buffer)
				RETURN_ERROR(ERR_NO_COMPRESS);
			SERIES_TAIL(buffer) = 0;
		}
		if (!bzip2_ensure_avail_out(buffer, (REBLEN)MAX(4096, (REBINT)s->avail_in + 256)))
			RETURN_ERROR(ERR_NO_COMPRESS);
	}

	if (hob->sym == Handle_Bzip2Encoder) {
		while (data && s->avail_in > 0) {
			REBLEN tail = SERIES_TAIL(buffer);
			if (!bzip2_ensure_avail_out(buffer, 4096))
				RETURN_ERROR(ERR_NO_COMPRESS);
			tail = SERIES_TAIL(buffer);
			s->next_out = (char *)BIN_SKIP(buffer, tail);
			s->avail_out = (unsigned int)(SERIES_REST(buffer) - tail);
			ret = BZ2_bzCompress(s, BZ_RUN);
			SERIES_TAIL(buffer) = (REBLEN)((REBYTE *)s->next_out - BIN_HEAD(buffer));
			if (ret != BZ_OK && ret != BZ_RUN_OK)
				RETURN_ERROR(ERR_NO_COMPRESS);
		}
		if (ref_flush) {
			for (;;) {
				REBLEN tail = SERIES_TAIL(buffer);
				if (!bzip2_ensure_avail_out(buffer, 4096))
					RETURN_ERROR(ERR_NO_COMPRESS);
				tail = SERIES_TAIL(buffer);
				s->next_out = (char *)BIN_SKIP(buffer, tail);
				s->avail_out = (unsigned int)(SERIES_REST(buffer) - tail);
				ret = BZ2_bzCompress(s, BZ_FLUSH);
				SERIES_TAIL(buffer) = (REBLEN)((REBYTE *)s->next_out - BIN_HEAD(buffer));
				if (ret == BZ_FLUSH_OK)
					break;
				if (ret < 0 || ret == BZ_SEQUENCE_ERROR)
					RETURN_ERROR(ERR_NO_COMPRESS);
			}
		}
		if (ref_finish) {
			for (;;) {
				REBLEN tail = SERIES_TAIL(buffer);
				if (!bzip2_ensure_avail_out(buffer, 4096))
					RETURN_ERROR(ERR_NO_COMPRESS);
				tail = SERIES_TAIL(buffer);
				s->next_out = (char *)BIN_SKIP(buffer, tail);
				s->avail_out = (unsigned int)(SERIES_REST(buffer) - tail);
				ret = BZ2_bzCompress(s, BZ_FINISH);
				SERIES_TAIL(buffer) = (REBLEN)((REBYTE *)s->next_out - BIN_HEAD(buffer));
				if (ret == BZ_STREAM_END) {
					bzip2_ctx_library_close(ctxx);
					break;
				}
				if (ret != BZ_FINISH_OK && ret != BZ_OK && ret != BZ_RUN_OK)
					RETURN_ERROR(ERR_NO_COMPRESS);
			}
		}
	} else if (data && s->avail_in > 0) {
		for (;;) {
			REBLEN tail = SERIES_TAIL(buffer);
			if (!bzip2_ensure_avail_out(buffer, 4096))
				RETURN_ERROR(ERR_NO_DECOMPRESS);
			tail = SERIES_TAIL(buffer);
			s->next_out = (char *)BIN_SKIP(buffer, tail);
			s->avail_out = (unsigned int)(SERIES_REST(buffer) - tail);
			ret = BZ2_bzDecompress(s);
			SERIES_TAIL(buffer) = (REBLEN)((REBYTE *)s->next_out - BIN_HEAD(buffer));
			if (ret == BZ_STREAM_END) {
				bzip2_ctx_library_close(ctxx);
				break;
			}
			if (ret != BZ_OK)
				RETURN_ERROR(ERR_NO_DECOMPRESS);
			if (s->avail_in == 0)
				break;
			if (s->avail_out == 0)
				continue;
		}
	}

	if (ref_flush || ref_finish) {
		REBLEN tail = SERIES_TAIL(buffer);
		REBSER *output = RL_MAKE_BINARY(tail);
		const REBYTE *err = (hob->sym == Handle_Bzip2Encoder)
			? ERR_NO_COMPRESS
			: ERR_NO_DECOMPRESS;
		if (!output)
			RETURN_ERROR(err);
		COPY_MEM(BIN_HEAD(output), BIN_HEAD(buffer), tail);
		SERIES_TAIL(output) = tail;
		SERIES_TAIL(buffer) = 0;
		RXA_SERIES(frm, 1) = output;
		RXA_TYPE(frm, 1) = RXT_BINARY;
		RXA_INDEX(frm, 1) = 0;
	}
	return RXR_VALUE;
}

COMMAND cmd_read(RXIFRM *frm, void *ctx) {
	REBHOB *hob = RXA_HANDLE(frm, 1);
	Bzip2StreamCtx *ctxx;
	REBSER *buffer;
	bz_stream *s;
	int ret;

	if (!hob || hob->handle == NULL
		|| !(hob->sym == Handle_Bzip2Encoder || hob->sym == Handle_Bzip2Decoder)) {
		RETURN_ERROR(ERR_INVALID_HANDLE);
	}
	ctxx = (Bzip2StreamCtx *)hob->handle;
	if (!(buffer = hob->series))
		return RXR_NONE;
	s = &ctxx->strm;

	if (hob->sym == Handle_Bzip2Encoder && !ctxx->lib_closed) {
		/* Prevent infinite loops if the library reports no progress. */
		for (int i = 0; i < 1000; i++) {
			REBLEN tail = SERIES_TAIL(buffer);
			if (!bzip2_ensure_avail_out(buffer, 4096))
				RETURN_ERROR(ERR_NO_COMPRESS);
			tail = SERIES_TAIL(buffer);
			s->next_out = (char *)BIN_SKIP(buffer, tail);
			s->avail_out = (unsigned int)(SERIES_REST(buffer) - tail);
			ret = BZ2_bzCompress(s, BZ_FLUSH);
			SERIES_TAIL(buffer) = (REBLEN)((REBYTE *)s->next_out - BIN_HEAD(buffer));
			if (ret == BZ_FLUSH_OK)
				break;
			if ((REBLEN)SERIES_TAIL(buffer) == tail && (ret == BZ_RUN_OK || ret == BZ_FLUSH_OK))
				break;
			if (ret < 0 || ret == BZ_SEQUENCE_ERROR)
				RETURN_ERROR(ERR_NO_COMPRESS);
		}
	}

	{
		REBLEN tail = SERIES_TAIL(buffer);
		REBSER *output = RL_MAKE_BINARY(tail);
		const REBYTE *err = (hob->sym == Handle_Bzip2Encoder)
			? ERR_NO_COMPRESS
			: ERR_NO_DECOMPRESS;
		if (!output)
			RETURN_ERROR(err);
		COPY_MEM(BIN_HEAD(output), BIN_HEAD(buffer), tail);
		SERIES_TAIL(output) = tail;
		SERIES_TAIL(buffer) = 0;
		RXA_SERIES(frm, 1) = output;
		RXA_TYPE(frm, 1) = RXT_BINARY;
		RXA_INDEX(frm, 1) = 0;
	}
	return RXR_VALUE;
}
