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
# macOS
mkdir -p ~/Library/Application\ Support/DreamSourceLab/PXTOOL/cdecoders
cp spi.dylib ~/Library/Application\ Support/DreamSourceLab/PXTOOL/cdecoders/

# Linux
mkdir -p ~/.local/share/DreamSourceLab/PXTOOL/cdecoders
cp spi.so ~/.local/share/DreamSourceLab/PXTOOL/cdecoders/
```

Restart DSView. When you add an SPI decoder, its title will show `[C]` if
the C decoder was loaded successfully. Right-click the decoder title to
switch between C and Python engines.

## Streaming (ABI v2)

As of `C_DECODER_API_VERSION = 2` this example also exposes the streaming
triple:

| Function | Role |
|----------|------|
| `spi_create(samplerate, num_channels)` | Allocates a zeroed `spi_state` struct |
| `spi_decode_chunk(inst, start, end, n, ch, put, ctx, stop)` | Decodes a contiguous chunk; protocol state persists across calls via `inst` |
| `spi_destroy(inst)` | Frees the state |

The legacy `spi_decode()` is kept and now forwards to the same per-sample
loop using a transient stack-allocated state. Hosts that do not implement
streaming (older DSView builds, batch-only callers) still get correct
output via the v1 entry — just without progressive updates during a live
capture.

The host always picks streaming when all three new function pointers are
non-NULL — this matches the Python decoder path's behaviour (waits for
data, feeds chunks as they arrive, emits annotations progressively).

Rebuild after pulling:

```sh
cd pv/cdecoders/example_spi
cmake -S . -B build
cmake --build build
# macOS
cp build/spi.dylib ~/Library/Application\ Support/DSView/cdecoders/
# Linux
cp build/spi.so ~/.local/share/DSView/cdecoders/
```

You should see this in the DSView log at startup:

```
C decoder 'spi' loaded from <path>/spi.dylib (api=v2, batch=yes, streaming=yes)
```
