cmake_minimum_required(VERSION 3.13)

set(NAME pico-xmodem-example)
set(URL "https://github.com/dcooperdalrymple/pico-xmodem/main/example")

# Initialize the Raspberry Pi Pico SDK
set(PICO_SDK_FETCH_FROM_GIT on)
include(pico_sdk_import.cmake)
pico_sdk_init()

project(${NAME} C CXX ASM)

# Add executable. Default name is the project name,
add_executable(${NAME} pico-xmodem-example.cpp)
add_subdirectory(../ build)

pico_add_extra_outputs(${NAME})
target_link_libraries(${NAME} pico_stdlib pico_xmodem)

pico_set_program_url(${NAME} ${URL})

# Enable stdio over USB & UART
pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 1)
