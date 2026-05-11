# Example C Decoder: SPI

A minimal SPI (Serial Peripheral Interface) Mode 0 decoder written in C,
demonstrating the `CDecoderDef` ABI.

## Protocol

- **Mode 0** (CPOL=0, CPHA=0): samples on the rising CLK edge
- **CS active-low**
- **MSB first**, 8-bit bytes

## Channels

| Index | ID   | Description  |
|-------|------|--------------|
| 0     | CLK  | Clock        |
| 1     | MOSI | Master → Slave |
| 2     | MISO | Slave → Master |
| 3     | CS   | Chip Select (active-low) |

## Annotations

| Row   | Class      | Description         |
|-------|------------|---------------------|
| bits  | mosi_bit   | Individual MOSI bits |
| bits  | miso_bit   | Individual MISO bits |
| data  | mosi_byte  | Assembled MOSI bytes |
| data  | miso_byte  | Assembled MISO bytes |

## Building

```bash
# macOS
clang -shared -fPIC -O2 -I.. -o spi.dylib spi_c_decoder.c

# Linux
gcc -shared -fPIC -O2 -I.. -o spi.so spi_c_decoder.c

# CMake (from this directory)
mkdir build && cd build
cmake ..
make
```

## Installing

Copy the built shared library to the DSView C decoders directory:

```bash
# macOS (typical location)
cp spi.dylib ~/Library/Application\ Support/DSView/cdecoders/

# Linux (typical location)
cp spi.so ~/.local/share/DSView/cdecoders/
```

Restart DSView. When you add an SPI decoder, its title will show `[C]` if
the C decoder was loaded successfully. Right-click the decoder title to
switch between C and Python engines.
