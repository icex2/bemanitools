testexes            += ezusb-iidx-emu-fpga-test

srcdir_ezusb-iidx-emu-fpga-test := src/test/ezusb-iidx-emu

libs_ezusb-iidx-emu-fpga-test     := \
    ezusb-iidx-emu \
    ezusb-iidx-16seg-emu \
    ezusb-emu \
    iidxio-stub \
    eamio-stub \
    security \
    time-stub \
    test \
    util \

src_ezusb-iidx-emu-fpga-test     := \
    ezusb-iidx-emu-fpga-test.c \

################################################################################

testexes            += ezusb-iidx-emu-msg-test

srcdir_ezusb-iidx-emu-msg-test := src/test/ezusb-iidx-emu

libs_ezusb-iidx-emu-msg-test     := \
    ezusb-iidx-emu \
    ezusb-iidx-16seg-emu \
    ezusb-emu \
    iidxio-stub \
    eamio-stub \
    security \
    time-stub \
    test \
    util \

src_ezusb-iidx-emu-msg-test     := \
    ezusb-iidx-emu-msg-test.c \
