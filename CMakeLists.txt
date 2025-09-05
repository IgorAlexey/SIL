cmake_minimum_required(VERSION 3.27)
project(lua VERSION 5.5.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(src)

add_executable(lua_exe src/lua.c)
target_link_libraries(lua_exe lua)

set_target_properties(lua_exe PROPERTIES OUTPUT_NAME "lua")

# Link dynamic loader library on Linux/Unix
if(UNIX)
    target_link_libraries(lua_exe dl)
endif()

install(TARGETS lua_exe
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Optional: Create luac compiler executable if needed
# add_executable(luac_exe src/luac.c)
# target_link_libraries(luac_exe lua)
# set_target_properties(luac_exe PROPERTIES OUTPUT_NAME "luac")
# install(TARGETS luac_exe RUNTIME DESTINATION bin COMPONENT Runtime)