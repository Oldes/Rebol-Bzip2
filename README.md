[![Rebol-Bzip2 CI](https://github.com/Oldes/Rebol-C-Extension-Bzip2/actions/workflows/main.yml/badge.svg)](https://github.com/Oldes/Rebol-C-Extension-Bzip2/actions/workflows/main.yml)

# Rebol/Bzip2

Bzip2 extension for [Rebol3](https://github.com/Oldes/Rebol3) (version 3.20.5 and higher)

## Usage
```rebol
import bzip2
bin: compress some-data 'bzip2
txt: decompress bin 'bzip2
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
* `quality` `[integer!]` Compression level from 1 to 22.

#### `decompress` `:data`
Decompress data using Zstandard
* `data` `[binary! any-string!]` Input data to decompress.
* `/part` Limit the input data to a given length.
* `length` `[integer!]` Length of input data.
* `/size` Limit the output size.
* `bytes` `[integer!]` Maximum number of uncompressed bytes.

