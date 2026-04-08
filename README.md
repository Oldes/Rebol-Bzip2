
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
Native Bzip2 version

#### `compress` `:data`
Compress data using Zstandard
* `data` `[binary! any-string!]` Input data to compress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/level`
* `quality` `[integer!]` Compression level from 1 to 9.

#### `decompress` `:data`
Decompress data using Zstandard
* `data` `[binary! any-string!]` Input data to decompress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/size` Limit the output size.
* `bytes` `[integer!]` Maximum number of uncompressed bytes.

