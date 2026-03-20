exes              += midi-dmio

ldflags_midi-dmio := \
    -lwinmm \

libs_midi-dmio    := \
    dmio \
    util \

src_midi-dmio     := \
    main.c \
