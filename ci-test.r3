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
src: mold system
bin: compress src 'bzip2
str: to string! decompress bin 'bzip2
? bin
? str
