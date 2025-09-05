set(LUA_CORE_SOURCES
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

set(LUA_AUX_SOURCES
    lauxlib.c
)

set(LUA_LIB_SOURCES
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

add_library(lua STATIC
    ${LUA_CORE_SOURCES}
    ${LUA_AUX_SOURCES}
    ${LUA_LIB_SOURCES}
)

target_compile_options(lua PRIVATE
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
    target_compile_options(lua PRIVATE
        -Wlogical-op
        -Wno-aggressive-loop-optimizations
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
endif()

# Link math library
target_link_libraries(lua PRIVATE m)
set_target_properties(lua PROPERTIES OUTPUT_NAME "lua")