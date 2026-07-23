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

The runner is `DSView-format-integration-test`. It uses `QCoreApplication`,
does not create widgets or file dialogs, and reuses one process-level
libsigrok session to avoid repeated libusb initialization.

## Remaining fixture gaps

Import-only formats (`isf`, `logicport`, `saleae`, `protocoldata`, `raw_analog`,
`trace32_ad`, and `stf`) require external capture fixtures and are not claimed
as covered by this CSV-based suite.
