# pico-xmodem
XMODEM driver for the Pico SDK over UART to send and receive data.

## Installation
Make sure that [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) is installed correctly before following the instructions below.

### Example Compilation
Clone the library using `git clone https://github.com/dcooperdalrymple/pico-xmodem` into a suitable location. Run the following commands to build the library example and ensure that your machine is capable of building this software.

````
cd pico-xmodem/example
mkdir build && cd build
cmake ..
make
````

### Install Extension into Your Project
In order to add this library as an extension of your project, insert it as a submodule using `git submodule add https://github.com/dcooperdalrymple/pico-xmodem.git` into the desired location. In the `CMakeLists.txt` file, insert `add_subdirectory(./{PATH_TO_SUBMODULE}/pico-xmodem)` below your source files (ie: `add_executable(...)`). Then add `pico_xmodem` to your list of _target_link_libraries_ such as `target_link_libraries(... pico_xmodem ...)`.

## Usage
See `./example/pico-xmodem-example.cpp` for full implementation.
