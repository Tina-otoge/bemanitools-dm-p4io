dlls                += dmio-p4io

ldflags_dmio-p4io   := \
    -lsetupapi \

libs_dmio-p4io      := \
    cconfig \
    p4iodrv \
    util \

src_dmio-p4io       := \
    config.c \
    dmio.c \
