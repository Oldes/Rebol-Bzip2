# Streaming bzip2 — plan

Goal: add a **chunked streaming API** to this extension, comparable to **[Rebol-Zstd](https://github.com/Oldes/Rebol-Zstd)** (`make-encoder`, `make-decoder`, `write`, `read`), backed by libbzip2’s **`bz_stream`** API (`BZ2_bzCompressInit` / `BZ2_bzCompress` / `BZ2_bzCompressEnd`, `BZ2_bzDecompressInit` / `BZ2_bzDecompress` / `BZ2_bzDecompressEnd`).

The existing **one-shot** path stays: `compress` / `decompress` commands, **`decompress_bzip2_impl`** (with `/size`, `/max`, growth loop), and the **`bzip2`** codec registration (`CompressBzip2` / `DecompressBzip2`). Streaming is **additional** surface area, not a replacement.

---

## Reference implementation

Mirror layout and behavior from Zstd where it reduces surprise:

| Piece | Zstd (`src/zstd-*.`) | Bzip2 (this repo) |
|--------|----------------------|-------------------|
| Command spec | `zstd-rebol-extension.r3` | `bzip2-rebol-extension.r3` |
| `RX_Init` handles | `RL_REGISTER_HANDLE_SPEC` ×2 | Same pattern, new symbols |
| Streaming C | `cmd_make_encoder`, `cmd_write`, … | New `cmd_*` in `bzip2-commands.c` |
| Global stream state | `g_decoder` in one-shot path (avoid copying) | **Do not** add globals; all state on the handle |

---

## Phase 1 — Rebol spec + codegen (`bzip2-rebol-extension.r3`)

1. After the `decompress:` block in `commands:`, append (names aligned with Zstd for portable scripts):

   - **`make-encoder`** — `/level quality [integer!]` → bzip2 block size **1–9** (same as `compress`).
   - **`make-decoder`** — no args (match `small`/verbosity policy to one-shot decompress).
   - **`write`** — `codec [handle!]`, `data [binary! any-string! none!]`, `/flush`, `/finish` (same refinement *shape* as Zstd so `RXA_*` layout can be copied from `zstd-commands.c` after regeneration).
   - **`read`** — `codec [handle!]`.

2. **`handles:` map** — define **`bzip2-encoder`** and **`bzip2-decoder`** entries (doc only for README unless you wire getters later). Remove stale “not used” placeholder if it blocks generation.

3. Run **`r3 bzip2-rebol-extension.r3`** from **`src/`** to refresh `bzip2-rebol-extension.h` and `bzip2-commands-table.c`, and README “Extension commands” section.

4. **Naming choice** (pick one before coding): identical words to Zstd (`make-encoder`, …) vs prefixed (`bz2-make-encoder`, …).

---

## Phase 2 — C: handle types and `RX_Init` (`bzip2-rebol-extension.c`)

1. **`typedef` or opaque struct** per direction holding at least:

   - `bz_stream strm;`
   - flags: `encoder` vs `decoder`, `initialized`, `finished`;
   - `REBSER *pending` (or use `hob->series` like Zstd for output accumulation).

2. **`RL_REGISTER_HANDLE_SPEC`** for two handle types (e.g. `"bzip2-encoder"`, `"bzip2-decoder"`):

   - **`free`**: `BZ2_bzCompressEnd` / `BZ2_bzDecompressEnd` (if initialized), release struct + any `pending` series.
   - **`mold`**: short pointer or tag string for debugging.

3. **No** shared global `bz_stream` between concurrent decompresses.

---

## Phase 3 — Encoder streaming (`bzip2-commands.c`)

1. **`cmd_make_encoder`**: `BZ2_bzCompressInit`, store state in `RL_MAKE_HANDLE_CONTEXT` HOB; default level like `cmd_compress` (e.g. 6) if `/level` absent.

2. **`cmd_write`** (encoder branch):

   - Point `strm.next_in` / `avail_in` at incoming series slice.
   - Loop **`BZ2_bzCompress`** with **`BZ_RUN`** until input consumed.
   - **`/flush`**: **`BZ_FLUSH`** until no further output (may take multiple calls).
   - **`/finish`** or **`none` end-game**: **`BZ_FINISH`** until **`BZ_STREAM_END`**, then **`BZ2_bzCompressEnd`**, mark finished.
   - Grow **`hob->series`** (or `pending`) like Zstd’s encoder path; on `/flush`/`/finish`, optionally return a **copy** binary and reset tail (match Zstd safety pattern).

3. **`cmd_read`**: drain pending compressed bytes (align with Zstd encoder `read` semantics).

---

## Phase 4 — Decoder streaming

1. **`cmd_make_decoder`**: `BZ2_bzDecompressInit` (`small` consistent with `decompress_bzip2_impl`).

2. **`cmd_write`** (decoder branch): feed compressed chunks; **`BZ2_bzDecompress`** until input chunk consumed or **`BZ_STREAM_END`**; expand output buffer when `avail_out == 0`.

3. **`cmd_read`**: return pending decompressed bytes; reset internal buffer as in Zstd.

4. **Errors**: map `BZ_*` to extension errors; invalid handle → same style as Zstd’s `ERR_INVALID_HANDLE`.

---

## Phase 5 — Semantics and limits

- **Finished handle**: reject further `write` or return `RXR_NONE` — document; match Zstd if possible.
- **Optional**: reuse **`/max`** idea as a **per-handle output cap** (separate from one-shot `decompress/max`); can be phase 2 polish.
- **Threading**: document one active user per handle.

---

## Phase 6 — Tests + docs

1. **`ci-test.r3`**: minimal **encoder `write/finish` → codec decompress** and **decoder `write` + `read`** round-trip (see current `ci-test.r3`).
2. **`README.md`**: regenerated command list includes streaming commands; add a dedicated **Streaming API** prose section when you want copy-paste examples (optional).

---

## Implementation order (recommended)

1. Spec + regenerate headers; stub **`make-encoder` / `make-decoder`** + **`free`** only (CI still green).
2. Encoder **`write`** + **`read`**, small round-trip.
3. Decoder **`write`** + **`read`**, round-trip.
4. README + CI streaming tests.
5. Optional: `get_path` / refinements on handles.

---

## Current codebase anchors (post one-shot hardening)

- One-shot decompress: **`decompress_bzip2_impl`** in `src/bzip2-commands.c` (`/size`, `/max`, bounded growth loop).
- Codec: **`DecompressBzip2`** matches **`DECOMPRESS_FUNC`** and forwards to **`decompress_bzip2_impl`** with `max_alloc == 0`.
- Tests: **`ci-test.r3`** (integration only; no C unit tests yet).

When this file and implementation drift, update **Phase 1** command lists and **Phase 6** CI paths to match.
