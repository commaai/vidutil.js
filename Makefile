.PHONY: all
all: dist/vidutil.html

dist/vidutil.html: src/viddecode.c src/vidutil-post.js
	emcc -v -O3 \
    -o dist/vidutil.html \
    -I libs/ffmpeg/include/ \
    src/viddecode.c \
    libs/ffmpeg/lib/libavformat.a \
    libs/ffmpeg/lib/libavcodec.a \
    libs/ffmpeg/lib/libavutil.a \
    --post-js src/vidutil-post.js \
    --emrun \
    -s WASM=1 \
    -s "BINARYEN_METHOD='native-wasm'" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s RESERVED_FUNCTION_POINTERS=20 \
    -s EXPORTED_FUNCTIONS="['_viddecode_init','_viddecode_decode']"
