testexes            += bio2emu-iidx-test

srcdir_bio2emu-iidx-test := src/test/bio2emu

libs_bio2emu-iidx-test     := \
    bio2emu-iidx \
    bio2emu \
    acioemu \
    iidxio-stub \
    hooklib \
    hook \
    time-stub \
    test \
    util \

src_bio2emu-iidx-test     := \
    bio2emu-iidx-test.c \
