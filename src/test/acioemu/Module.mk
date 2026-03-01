testexes            += acioemu-pipe-test

srcdir_acioemu-pipe-test := src/test/acioemu

libs_acioemu-pipe-test     := \
    acioemu \
    hook \
    eamio-stub \
    time-stub \
    test \
    util \

src_acioemu-pipe-test     := \
    acioemu-pipe-test.c \
