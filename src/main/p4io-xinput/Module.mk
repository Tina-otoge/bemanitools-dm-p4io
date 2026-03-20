exes                  += p4io-xinput

cppflags_p4io-xinput  := \
    -I src/imports \

deplibs_p4io-xinput   := \
    ViGEmClient \

ldflags_p4io-xinput   := \
    -lsetupapi \

libs_p4io-xinput      := \
    p4iodrv \
    util \
    vigemstub \

src_p4io-xinput       := \
    main.c \
