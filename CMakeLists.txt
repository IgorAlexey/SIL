cmake_minimum_required(VERSION 3.27)
project(sil VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(src)

add_executable(sil_exe src/sil.c)
target_link_libraries(sil_exe sil)

set_target_properties(sil_exe PROPERTIES OUTPUT_NAME "sil")

# Link dynamic loader library on Linux/Unix
if(UNIX)
    target_link_libraries(sil_exe dl)
endif()

install(TARGETS sil_exe
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

install(TARGETS sil
    LIBRARY DESTINATION lib     # for shared
    ARCHIVE DESTINATION lib     # for static
    INCLUDES DESTINATION include
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/
    DESTINATION include/sil
    FILES_MATCHING PATTERN "*.h"
)

# Optional: Create silc compiler executable
# add_executable(silc_exe src/silc.c)
# target_link_libraries(silc_exe sil)
# set_target_properties(silc_exe PROPERTIES OUTPUT_NAME "silc")
# install(TARGETS silc_exe RUNTIME DESTINATION bin COMPONENT Runtime)