# Task 5 Format Capabilities Report

## Result

`FormatCapability` now carries module-declared filename extensions and whether
an output module exposes options. Output extensions come from
`sr_output_extensions_get()`. Option arrays are obtained through
`sr_output_options_get()` and released with `sr_output_options_free()`.

The capability layer builds dialog filters from the extension list, using an
all-files pattern only for modules without a preferred extension. Export
capabilities now sort through the DSView-owned final-order rank table while
only returning currently registered modules.

`StoreSession` uses those capability filters, adds the first declared
extension only when a chosen filename has no suffix, and resolves the selected
module by format ID. It normalizes saved filters against current capabilities,
so an obsolete filter from the previous ID-as-extension implementation falls
back to CSV. This also preserves the selected format where multiple modules
share an extension.

## Tests

- Added coverage that registered `gnuplot` and `srzip` report `dat` and `sr`,
  while `null` has no preferred extension.
- Added coverage that CSV and srzip report options, while VCD and null do not.
- `cmake --build . --target DSView-test` passed.
- `cmake --build . --target DSView` passed.
- `git diff --check` passed.

`./build.macOS/DSView-test --run_test=formatcapability` has one expected RED
case: `chronovu-la8`, `ols`, and `wavedrom` are intentionally not registered
until Task 6. The pre-existing final export-manifest test also remains RED
until Tasks 6 and 7 register the remaining modules. No output modules were
added in this task.
