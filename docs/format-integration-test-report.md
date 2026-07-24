# Format Integration Test Report

## Canonical waveform

`docs/wave_data/Upstream-Compat-Demo-la-260723-120157.csv` is a 1 MHz,
1,024-sample capture with eight in-phase logic channels. Samples alternate
between `0x00` and `0xFF`.

## Automated coverage

| Format | Direction | Verification | Result |
| --- | --- | --- | --- |
| binary | export | non-empty 1,024-byte artifact | PASS |
| binary | export/import | canonical round-trip and `00 ff` raw-byte prefix | PASS |
| chronovu-la8 | export/import | fixed 8 MiB `.kdt` container and canonical logic prefix | PASS |
| ascii/bits/hex/gnuplot/wavedrom/ols/srzip | export | non-empty format artifact/signature | PASS |
| saleae | import | minimal Logic 2 digital fixture, sample count and waveform values | PASS |
| logicport | import | minimal `.lpf` fixture, sample count and waveform values | PASS |
| protocoldata | import | UART byte fixture, generated logic waveform | PASS |
| trace32_ad | import | minimal PowerIntegrator `.ad` fixture and first sample | PASS |
| stf | import | compressed Sigma Test File fixture, 448 logic samples | PASS |
| raw_analog | import | U8 fixture, three analog samples through probe and replay passes | PASS |
| isf | import | minimal binary ISF fixture, three analog samples | PASS |

The runner is `DSView-format-integration-test`. It uses `QCoreApplication`,
does not create widgets or file dialogs, and reuses one process-level
libsigrok session to avoid repeated libusb initialization.

## Fixture construction

The import-only cases construct deterministic minimal files in a temporary
directory. This keeps the runner self-contained while exercising each real
parser through `InputImporter`, `SigSession`, and the corresponding snapshot.
