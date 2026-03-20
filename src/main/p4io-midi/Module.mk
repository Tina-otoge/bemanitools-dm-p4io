exes              += p4io-midi

ldflags_p4io-midi := \
    -lsetupapi \
    -lwinmm \

libs_p4io-midi    := \
    p4iodrv \
    util \

src_p4io-midi     := \
    main.c \
