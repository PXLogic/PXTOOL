cmake_minimum_required(VERSION 3.10)
project(spi_c_decoder C)

# Build a shared library from the SPI example decoder.
# The output (spi.dylib on macOS, spi.so on Linux) can be copied to
# $APP_DATA_DIR/cdecoders/ so DSView loads it at startup.

add_library(spi SHARED spi_c_decoder.c)

# The API header lives two directories up from this file.
target_include_directories(spi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(spi PROPERTIES
    C_STANDARD 99
    PREFIX ""        # Avoid "lib" prefix — produce spi.dylib / spi.so
)

if(APPLE)
    set_target_properties(spi PROPERTIES SUFFIX ".dylib")
endif()

# Installation target: copies the built library to the cdecoders directory.
# Usage: cmake --install . --prefix <APP_DATA_DIR>
install(TARGETS spi DESTINATION cdecoders)
