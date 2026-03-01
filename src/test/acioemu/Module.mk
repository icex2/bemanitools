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

################################################################################

testexes            += acioemu-icca-test

srcdir_acioemu-icca-test := src/test/acioemu

libs_acioemu-icca-test     := \
    acioemu \
    hook \
    eamio-stub \
    time-stub \
    test \
    util \

src_acioemu-icca-test     := \
    acioemu-icca-test.c \
