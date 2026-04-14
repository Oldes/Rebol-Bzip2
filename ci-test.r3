Rebol [
    title: "Rebol/Bzip2 extension CI tests"
]

errors: 0

ensure: func [description condition] [
    unless condition [
        print [as-red "FAIL:" description]
        errors: errors + 1
    ]
]

print ["Running tests on Rebol build:" mold to-block system/build]

try [system/modules/bzip2: none]
system/options/modules: what-dir

bzip2: import 'bzip2
print ["Loaded module:" mold words-of bzip2]

;; --- version ---------------------------------------------------------------
ver: bzip2/version
print ["bzip2/version:" mold ver]
ensure "version is non-empty string" all [string? ver  not empty? ver]

;; --- codec: round-trip text and empty --------------------------------------
src: mold system
bin: compress src 'bzip2
str: to string! decompress bin 'bzip2
ensure "codec round-trip (mold system)" str = src

empty-bin: compress #{} 'bzip2
ensure "codec empty binary round-trip" #{} = decompress empty-bin 'bzip2

empty-txt: compress "" 'bzip2
ensure "codec empty string round-trip" "" = to string! decompress empty-txt 'bzip2

;; --- extension: compress /level --------------------------------------------
foreach lvl [1 9] [
    c: bzip2/compress/level copy "level-test" lvl
    ensure rejoin ["compress/level " lvl " round-trip"] (
        "level-test" = to string! bzip2/decompress c
    )
]

;; --- extension: compress/part + decompress ---------------------------------
small: bzip2/compress/part "abcdefghij" 5
ensure "compress/part + decompress" "abcde" = to string! bzip2/decompress small

;; --- extension: decompress /size -------------------------------------------
cap: bzip2/decompress/size bin 20
ensure "decompress/size length" (length? cap) <= 20

;; --- extension: decompress /max -------------------------------------------
full: bzip2/decompress/max bin (10 * 1024 * 1024)
ensure "decompress/max large cap equals full decode" full = to binary! str
ensure "decompress/max tiny cap errors" error? try [bzip2/decompress/max bin 48]

;; --- extension: decompress /size + /max -----------------------------------
;; Cap output to 12 bytes; allocation ceiling high enough to hold stream work.
cap2: bzip2/decompress/size/max bin 12 (256 * 1024)
ensure "decompress/size/max length" (length? cap2) <= 12

;; --- extension: invalid arguments -------------------------------------------
ensure "decompress/size negative errors" error? try [bzip2/decompress/size bin -1]
ensure "decompress/max negative errors" error? try [bzip2/decompress/max bin -1]

;; --- extension: corrupt / invalid payload -----------------------------------
garbage: #{425A683181E8FFFFFF} ;; bzip2-like header, invalid stream body
ensure "decompress garbage errors" error? try [bzip2/decompress garbage]

;; --- streaming (bz_stream) -----------------------------------------------
stream-msg: copy "hello-bzip2-stream"
st-ok: false
if not error? try [
    enc: bzip2/make-encoder
    sbin: bzip2/write/finish :enc copy stream-msg
    dec: bzip2/make-decoder
    bzip2/write :dec sbin
    dbin: bzip2/read :dec
    st-ok: all [
        handle? enc
        binary? sbin
        handle? dec
        binary? dbin
        stream-msg = to string! decompress sbin 'bzip2
        stream-msg = to string! dbin
    ]
][
    ;; try failed
    st-ok: false
]
ensure "streaming encoder/decoder round-trip" st-ok

;; --- summary ---------------------------------------------------------------
either errors = 0 [
    print as-green "OK: all Rebol/Bzip2 tests passed"
][
    print as-red rejoin ["FAILED: " errors " test(s)"]
]
quit/return errors
