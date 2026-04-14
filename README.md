
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

