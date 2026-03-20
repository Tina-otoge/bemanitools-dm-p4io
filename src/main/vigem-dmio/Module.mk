exes                  += vigem-dmio

cppflags_vigem-dmio   := \
    -I src/imports \

deplibs_vigem-dmio    := \
    ViGEmClient \

ldflags_vigem-dmio    := \
    -lsetupapi \

libs_vigem-dmio       := \
    dmio \
    util \
    vigemstub \

src_vigem-dmio        := \
    main.c \
