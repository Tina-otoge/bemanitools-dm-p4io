dlls                += dmio-p4io

ldflags_dmio-p4io   := \
    -lsetupapi \

libs_dmio-p4io      := \
    p4iodrv \
    util \

src_dmio-p4io       := \
    dmio.c \
