cmake_minimum_required(VERSION 3.13)

set(NAME pico_xmodem)
set(URL "https://github.com/dcooperdalrymple/pico-xmodem")

add_library(${NAME} INTERFACE)

target_sources(${NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/xmodem.cpp
)

target_include_directories(${NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

# Pull in pico libraries that we need
target_link_libraries(${NAME} INTERFACE pico_stdlib pico_malloc)
