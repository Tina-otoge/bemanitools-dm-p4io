exes              += p4iotest

ldflags_p4iotest  := \
    -lsetupapi \

libs_p4iotest     := \
    p4iodrv \
    util \

src_p4iotest      := \
    main.c \
