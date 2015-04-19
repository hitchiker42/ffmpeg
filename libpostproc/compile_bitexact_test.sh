#!/bin/bash
(cd ..
gcc -L$PWD/libavutil -I. -I./ -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600 -DZLIB_CONST -DHAVE_AV_CONFIG_H -std=c99 libavutil/*.o libavutil/x86/*.o libpostproc/x86/{deinterlace.o,block_copy.o} libpostproc/postprocess_bitexact_test.c -o libpostproc/postprocess_bitexact_test -lm -Og -g)
