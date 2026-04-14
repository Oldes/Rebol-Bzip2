Rebol [
    title: "Rebol/Bzip2 extension CI test"
]

print ["Running test on Rebol build:" mold to-block system/build]

;; make sure that we load a fresh extension
try [system/modules/bzip2: none]
;; use current directory as a modules location
system/options/modules: what-dir

;; import the extension
bzip2: import 'bzip2

print as-yellow "Content of the module..."
? bzip2

;; libbzip2 version string
ver: bzip2/version
print ["bzip2/version:" mold ver]
if any [
    not string? ver
    empty? ver
][
    print as-red "FAIL: version should be a non-empty string"
    quit/return 1
]

src: mold system
bin: compress src 'bzip2
str: to string! decompress bin 'bzip2
? bin
? str

if str <> src [
    print as-red "FAIL: codec round-trip"
    quit/return 1
]

;; /part and /size on extension commands (not the codec path)
small: bzip2/compress/part "abcdefghij" 5
if (to string! bzip2/decompress small) <> "abcde" [
    print as-red "FAIL: compress/part round-trip"
    quit/return 1
]

cap: bzip2/decompress/size bin 20
if (length? cap) > 20 [
    print as-red "FAIL: decompress/size cap"
    quit/return 1
]

print as-green "OK: Rebol/Bzip2 CI tests passed"
quit/return 0
