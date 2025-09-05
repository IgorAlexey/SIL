set(SIL_CORE_SOURCES
    lapi.c
    lcode.c
    lctype.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    llex.c
    lmem.c
    lobject.c
    lopcodes.c
    lparser.c
    lstate.c
    lstring.c
    ltable.c
    ltm.c
    lundump.c
    lvm.c
    lzio.c
)

set(SIL_AUX_SOURCES
    lauxlib.c
)

set(SIL_LIB_SOURCES
    lbaselib.c
    ldblib.c
    liolib.c
    lmathlib.c
    loslib.c
    ltablib.c
    lstrlib.c
    lutf8lib.c
    loadlib.c
    lcorolib.c
    linit.c
)

add_library(sil STATIC
    ${SIL_CORE_SOURCES}
    ${SIL_AUX_SOURCES}
    ${SIL_LIB_SOURCES}
)

target_compile_options(sil PRIVATE
    -Wall
    -O2
    -fno-stack-protector
    -fno-common
    -march=native
    -std=c99
    -Wfatal-errors
    -Wextra
    -Wshadow
    -Wundef
    -Wwrite-strings
    -Wredundant-decls
    -Wdisabled-optimization
    -Wdouble-promotion
    -Wmissing-declarations
    -Wconversion
    -Wstrict-overflow=2
    -Wdeclaration-after-statement
    -Wmissing-prototypes
    -Wnested-externs
    -Wstrict-prototypes
    -Wc++-compat
    -Wold-style-definition
)

# GCC-specific warnings
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sil PRIVATE
        -Wlogical-op
        -Wno-aggressive-loop-optimizations
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(sil PRIVATE SIL_USE_LINUX)
endif()

# Link math library
target_link_libraries(sil PRIVATE m)
set_target_properties(sil PROPERTIES OUTPUT_NAME "sil")