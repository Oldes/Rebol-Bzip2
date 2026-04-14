//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// SPDX-License-Identifier: MIT
// =============================================================================
// Rebol/Bzip2 extension
// =============================================================================

#include "bzip2-rebol-extension.h"

RL_LIB *RL; // Link back to reb-lib from embedded extensions

//==== Globals ===============================================================//
extern MyCommandPointer Command[];

int CompressBzip2(const REBYTE *input, REBLEN len, REBCNT level, REBSER **output, REBINT *error);
int DecompressBzip2(const REBYTE *input, REBLEN len, REBCNT limit, REBSER **output, REBINT *error);

u32* arg_words;
u32* type_words;
//============================================================================//

static const char* init_block = BZIP2_EXT_INIT_CODE;

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	RL = lib;
	REBYTE ver[8];
	RL_VERSION(ver);
	debug_print(
		"RXinit bzip2-extension; Rebol v%i.%i.%i\n",
		ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) {
		debug_print(
			"Needs at least Rebol v%i.%i.%i!\n",
			 MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD);
		return 0;
	}
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}

	RL_REGISTER_COMPRESS_METHOD(cb_cast("bzip2"), CompressBzip2, DecompressBzip2);

	return init_block;
}

RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Command[cmd](frm, ctx);
}
