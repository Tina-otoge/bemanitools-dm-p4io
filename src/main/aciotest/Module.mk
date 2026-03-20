exes            += aciotest

ldflags_aciotest  := \
    -lsetupapi \

libs_aciotest     := \
    bio2drv \
    aciodrv \
    aciodrv-proc \
    p4iodrv \
    util \

src_aciotest      := \
    icca.c \
    kfca.c \
    panb.c \
    rvol.c \
    bi2a-iidx.c \
    bi2a-sdvx.c \
    p4io.c \
    main.c \
