# fx2lafw Firmware Resources

Place licensed `fx2lafw-*.fw` firmware files in this directory when enabling
bootloader-state FX2 logic analyzers.

This repository intentionally does not add unknown-origin firmware binaries.
`manifest.txt` lists the filenames referenced by DSView's
`upstream-fx2lafw` profile table. A firmware-loaded device can be opened
without these files, but a bootloader-state device needs the matching file
before DSView can upload firmware and wait for re-enumeration.
