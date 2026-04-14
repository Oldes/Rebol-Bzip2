
[![rebol-bzip2](https://github.com/user-attachments/assets/ec6a714e-dd5e-4f36-a7b8-8558a0804e61)](https://github.com/Oldes/Rebol-Bzip2)
[![Rebol-Bzip2 CI](https://github.com/Oldes/Rebol-Bzip2/actions/workflows/main.yml/badge.svg)](https://github.com/Oldes/Rebol-Bzip2/actions/workflows/main.yml)
[![Gitter](https://badges.gitter.im/rebol3/community.svg)](https://app.gitter.im/#/room/#Rebol3:gitter.im)
[![Zulip](https://img.shields.io/badge/zulip-join_chat-brightgreen.svg)](https://rebol.zulipchat.com/)

# Rebol/Bzip2

Bzip2 compression extension for [Rebol3](https://github.com/Oldes/Rebol3) (version 3.20.5 and higher)

## Basic usage
Use Bzip2 as a codec for the standard compress and decompress functions:
```rebol
import bzip2
bin: compress "some data" 'bzip2
txt: to string! decompress bin 'bzip2
```

## Installation

Build the extension using Siskin and copy the produced `.rebx` into your Rebol modules directory.

```bash
# Build (Linux x64 example)
./siskin Rebol-Bzip2.nest static-lib-x64
./siskin Rebol-Bzip2.nest bzip2-linux-x64
mv ./build/bzip2-linux-x64.so ./bzip2.rebx

# Install
mkdir -p ~/.rebol/modules
cp -f ./bzip2.rebx ~/.rebol/modules/bzip2.rebx
```

If you see `invalid ELF header`, you likely installed the wrong file (e.g. the static archive `libbzip2-*.a`) instead of the extension `.rebx`.

## Streaming API

Incremental compression uses libbzip2 `bz_stream` handles (`make-encoder`, `make-decoder`, `write`, `read`). Output is accumulated on the handle until you call `write` with `/flush` or `/finish` (encoder), which returns a **copy** of the pending compressed binary and clears the buffer—similar to [Rebol-Zstd](https://github.com/Oldes/Rebol-Zstd).

```rebol
bzip2: import 'bzip2
enc: bzip2/make-encoder
bzip2/write :enc "Hello "
bzip2/write :enc "World"
bin: bzip2/write/finish :enc "!"
text: to string! decompress bin 'bzip2
;== "Hello World!"

dec: bzip2/make-decoder
bzip2/write :dec bin
plain: to string! bzip2/read :dec
```

For large inputs you can feed the decoder in multiple `write` calls (splitting the compressed binary at byte boundaries is fine once the stream is valid).

## Extension commands:


#### `version`
Libbzip2 version string (BZ2_bzlibVersion)

#### `compress` `:data`
Compress data using bzip2
* `data` `[binary! any-string!]` Input data to compress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/level`
* `quality` `[integer!]` Block size 100k: 1 (fast) to 9 (best).

#### `decompress` `:data`
Decompress bzip2 data
* `data` `[binary! any-string!]` Input data to decompress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/size` Limit the output size.
* `bytes` `[integer!]` Maximum number of uncompressed bytes.
* `/max` Cap allocated output (ZIP bomb guard).
* `ceiling` `[integer!]` Maximum bytes to allocate while decompressing.

#### `make-encoder`
Create a new bzip2 encoder handle.
* `/level`
* `quality` `[integer!]` Block size 100k: 1 (fast) to 9 (best).

#### `make-decoder`
Create a new bzip2 decoder handle.

#### `write` `:codec` `:data`
Feed data into a bzip2 streaming codec.
* `codec` `[handle!]` Encoder or decoder handle.
* `data` `[binary! any-string! none!]` Data to compress or decompress, or NONE to finish encoder output.
* `/flush` Flush encoder output (BZ_FLUSH).
* `/finish` Finish encoder stream (BZ_FINISH).

#### `read` `:codec`
Retrieve pending data from the codec buffer.
* `codec` `[handle!]`

