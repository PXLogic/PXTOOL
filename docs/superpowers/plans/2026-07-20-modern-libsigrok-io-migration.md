# Modern libsigrok I/O Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move DSView's input/output integration to the current upstream libsigrok streaming APIs, expose all PulseView-visible export formats while preserving DSView formats, then load VCD/WAV/raw-binary files into waveforms through the same model.

**Architecture:** Update the direct libsigrok input/output contracts and their direct packet dependencies while retaining DSView hardware and native DSL session code. Make `StoreSession` use the upstream output lifecycle and a binary-safe writer. Add a small DSView option bridge and a standard analog packet adapter so UI code, output modules, future input, and later ADC acquisition share typed metadata.

**Tech Stack:** C/C++17, Qt 6 Widgets, GLib/GVariant, CMake, Boost.Test, libsigrok upstream sources at `/Users/yuanji/Desktop/project/libsigrok`.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `libsigrok/libsigrok.h` | DSView's public libsigrok datafeed/input/output declarations, updated to the current upstream I/O contract while retaining DSView-only declarations. |
| `libsigrok/libsigrok-internal.h` | Internal input/output module and instance definitions imported from the upstream I/O framework. |
| `libsigrok/input/input.c` | Current upstream input registration and option helper APIs. |
| `libsigrok/output/output.c` | Current upstream output registration, module lookup, options, lifecycle, extensions, and flags. |
| `libsigrok/input/*.c` | Current upstream VCD/WAV/binary input modules, plus their direct helpers. |
| `libsigrok/output/*.c` | Current upstream output modules. Preserve `gnuplot.c` as a DSView extension. |
| `PXTOOL/pv/data/formatcapability.{h,cpp}` | Runtime menu descriptors, extension filters, module options, and ordered export capability policy. |
| `PXTOOL/pv/data/iooptions.{h,cpp}` | RAII conversion between `sr_option` descriptors, Qt values, and a typed `GHashTable` passed to libsigrok. |
| `PXTOOL/pv/data/analogpacketadapter.{h,cpp}` | Constructs standard analog datafeed packets from DSView snapshots or future ADC packets. |
| `PXTOOL/pv/dialogs/inputoutputoptionsdlg.{h,cpp}` | Reusable Qt editor for typed libsigrok input/output options. |
| `PXTOOL/pv/storesession.{h,cpp}` | Standard output lifecycle, binary-safe file writing, format compatibility validation, and packet emission. |
| `PXTOOL/pv/mainwindow.{h,cpp}` | Runs the import transaction only after the export migration is complete. |
| `PXTOOL/pv/toolbars/filebar.{h,cpp}` | Continues to emit selected format IDs; no per-format widget logic. |
| `PXTOOL/test/test_formatcapability.cpp` | Runtime registration, order, extension, and menu descriptor tests. |
| `PXTOOL/test/test_io_options.cpp` | Typed option-map and dialog-independent option validation tests. |
| `PXTOOL/test/test_output_fixtures.cpp` | Deterministic logic/analog fixture output tests for all export IDs. |
| `PXTOOL/test/test_input_fixtures.cpp` | VCD/WAV/raw-binary streaming input and round-trip tests. |
| `PXTOOL/test/test_analogpacketadapter.cpp` | Standard analog packet contract tests. |
| `PXTOOL/test/test_upstream_io_stubs.c` | Narrow C stubs required by tests that compile I/O modules without hardware. |
| `CMakeLists.txt` / `PXTOOL/test/CMakeLists.txt` | Production and test source registration for every compiled I/O module. |

## Task 1: Lock the Runtime Format Manifest

**Files:**
- Modify: `PXTOOL/test/test_formatcapability.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

- [ ] **Step 1: Write the failing output-manifest test**

Add the expected final order, including DSView-only Gnuplot:

```cpp
BOOST_AUTO_TEST_CASE(enumerates_final_export_format_manifest)
{
    const QVector<pv::data::FormatCapability> formats = pv::data::exportFormats();
    const QStringList actual = ids(formats);
    const QStringList expected = {
        "csv", "vcd", "gnuplot", "srzip",
        "analog", "ascii", "binary", "bits", "chronovu-la8",
        "hex", "null", "ols", "wav", "wavedrom"
    };

    BOOST_CHECK_EQUAL_COLLECTIONS(actual.cbegin(), actual.cend(),
                                  expected.cbegin(), expected.cend());
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=enumerates_final_export_format_manifest
```

Expected: failure because the current runtime list contains only `csv`, `vcd`, `gnuplot`, and `srzip`.

- [ ] **Step 3: Add CMake test-file registration**

Add the future I/O test files now so every following task extends one executable:

```cmake
    test_io_options.cpp
    test_output_fixtures.cpp
    test_input_fixtures.cpp
    test_analogpacketadapter.cpp
    test_upstream_io_stubs.c
```

Create each C++ file with one disabled-by-registration test suite only after its module dependencies exist; do not add an empty source file. Add a minimal `BOOST_AUTO_TEST_SUITE(io_migration)` and `BOOST_AUTO_TEST_SUITE_END()` in each new C++ file.

- [ ] **Step 4: Build the existing test target**

Run:

```bash
cmake --build . --target DSView-test
```

Expected: successful compilation after the new test source files are added.

- [ ] **Step 5: Commit the manifest baseline**

```bash
git add PXTOOL/test/test_formatcapability.cpp PXTOOL/test/test_io_options.cpp \
  PXTOOL/test/test_output_fixtures.cpp PXTOOL/test/test_input_fixtures.cpp \
  PXTOOL/test/test_analogpacketadapter.cpp PXTOOL/test/test_upstream_io_stubs.c \
  PXTOOL/test/CMakeLists.txt
git commit -m "test: define modern io format manifest"
```

## Task 2A: Migrate Upstream I/O Direct-Core Dependencies

**Files:**
- Modify: `libsigrok/libsigrok.h`
- Modify: `libsigrok/libsigrok-internal.h`
- Modify: `libsigrok/dsdevice.c`
- Modify: `libsigrok/session.c`
- Modify: `libsigrok/std.c`
- Modify: `libsigrok/strutil.c`
- Modify: `libsigrok/log.c`
- Create: `libsigrok/analog.c`
- Modify: every DSView `libsigrok/**/*.c` caller of `sr_channel_new()` and
  `std_session_send_df_header()` reported by `rg`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `PXTOOL/test/test_upstream_io_stubs.c`
- Modify: `PXTOOL/test/test_input_fixtures.cpp`
- Modify: `PXTOOL/test/test_analogpacketadapter.cpp`

- [ ] **Step 1: Write failing direct-core contract tests**

Add tests that compile against the upstream signatures and verify the standard
datafeed helpers forward a header, metadata, logic, analog, and end packet:

```cpp
BOOST_AUTO_TEST_CASE(upstream_direct_core_creates_channels_on_the_device)
{
    sr_dev_inst sdi{};
    sr_channel *channel = sr_channel_new(&sdi, 3, SR_CHANNEL_LOGIC, TRUE, "D3");

    BOOST_REQUIRE(channel != nullptr);
    BOOST_CHECK_EQUAL(g_slist_length(sdi.channels), 1);
    BOOST_CHECK_EQUAL(channel->index, 3);
}

BOOST_AUTO_TEST_CASE(upstream_direct_core_initializes_standard_analog_packet)
{
    sr_datafeed_analog analog{};
    sr_analog_encoding encoding{};
    sr_analog_meaning meaning{};
    sr_analog_spec spec{};

    BOOST_CHECK_EQUAL(sr_analog_init(&analog, &encoding, &meaning, &spec, 3), SR_OK);
    BOOST_CHECK_EQUAL(analog.encoding, &encoding);
    BOOST_CHECK_EQUAL(analog.meaning, &meaning);
}
```

- [ ] **Step 2: Run the direct-core tests and verify they fail**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=upstream_direct_core_*
```

Expected: compile failure because DSView currently exposes the old
four-argument `sr_channel_new` and simplified analog packet model.

- [ ] **Step 3: Import the direct-core implementation**

Port the required upstream code from:

```text
/Users/yuanji/Desktop/project/libsigrok/src/device.c
/Users/yuanji/Desktop/project/libsigrok/src/session.c
/Users/yuanji/Desktop/project/libsigrok/src/std.c
/Users/yuanji/Desktop/project/libsigrok/src/analog.c
/Users/yuanji/Desktop/project/libsigrok/src/strutil.c
/Users/yuanji/Desktop/project/libsigrok/src/log.c
```

Replace copied source headers with the DSView/PXTOOL GPL header. Preserve
DSView-only `sr_dev_inst`, DSO, device-handle, and event members in the
merged headers; migrate all direct-core type and function definitions needed
by the input/output source set.

Update each DSView source caller of `sr_channel_new()` to pass its owning
`sr_dev_inst *` as the first argument. Update each
`std_session_send_df_header()` caller to its upstream signature. Do not add
an overload, macro, or compatibility wrapper for the old signatures.

Implement the upstream session-send helpers by forwarding standard packets to
DSView's installed datafeed callback. Keep DSView hardware session loops and
DSO-specific packet handling unchanged outside this forwarding boundary.

- [ ] **Step 4: Register the direct-core sources in production and tests**

Add the migrated sources to both `libsigrok_SOURCES` and `DSView-test`.
Replace test-only implementations only where a source would otherwise require
hardware. The test target must link the same direct-core APIs as production.

- [ ] **Step 5: Run direct-core and existing hardware compatibility tests**

Run:

```bash
cmake . -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DENABLE_TESTS=ON
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=upstream_direct_core_*
./build.macOS/DSView-test --run_test=upstream_fx2lafw
./build.macOS/DSView-test --run_test=formatcapability
```

Expected: all commands exit 0. Existing compiler warnings are recorded but
must not become new errors.

- [ ] **Step 6: Commit the direct-core migration**

```bash
git add libsigrok PXTOOL/test CMakeLists.txt
git commit -m "feat: migrate libsigrok io direct core"
```

## Task 2B: Import the Current Upstream I/O Contracts

**Files:**
- Modify: `libsigrok/input/input.c`
- Delete: `libsigrok/input/in_binary.c`
- Delete: `libsigrok/input/in_vcd.c`
- Delete: `libsigrok/input/in_wav.c`
- Create: `libsigrok/input/binary.c`
- Create: `libsigrok/input/vcd.c`
- Create: `libsigrok/input/wav.c`
- Modify: `libsigrok/output/output.c`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `PXTOOL/test/test_upstream_io_stubs.c`

- [ ] **Step 1: Write failing lifecycle API tests**

In `PXTOOL/test/test_io_options.cpp`, assert that the new public lifecycle can create and free a `null` output instance:

```cpp
BOOST_AUTO_TEST_CASE(creates_and_frees_output_with_default_options)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("null"));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *output = sr_output_new(module, nullptr, make_test_sdi());
    BOOST_REQUIRE(output != nullptr);
    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
}
```

Declare `make_test_sdi()` in the test file and define it in
`test_upstream_io_stubs.c` with one enabled logic channel.

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=creates_and_frees_output_with_default_options
```

Expected: failure because `null` is not registered in the current output list.

- [ ] **Step 3: Port the upstream contract without replacing DSView-only declarations**

Copy the current upstream definitions from:

```text
/Users/yuanji/Desktop/project/libsigrok/include/libsigrok/libsigrok.h
/Users/yuanji/Desktop/project/libsigrok/src/libsigrok-internal.h
/Users/yuanji/Desktop/project/libsigrok/src/input/input.c
/Users/yuanji/Desktop/project/libsigrok/src/output/output.c
```

Apply only the input/output-related declarations and helper APIs to DSView's
headers. Preserve all DSView-specific device, DSO, and event declarations.
Use the current upstream signatures for:

```c
const struct sr_option **sr_input_options_get(const struct sr_input_module *imod);
void sr_input_options_free(const struct sr_option **opts);
const struct sr_option **sr_output_options_get(const struct sr_output_module *omod);
void sr_output_options_free(const struct sr_option **opts);
const struct sr_output *sr_output_new(const struct sr_output_module *omod,
    GHashTable *options, const struct sr_dev_inst *sdi);
int sr_output_send(const struct sr_output *o,
    const struct sr_datafeed_packet *packet, GString **out);
int sr_output_free(const struct sr_output *o);
```

Import `sr_output_module::flags`, `sr_output_module::exts`, and the current
input-module streaming callbacks. The direct-core APIs required by these
contracts are supplied by Task 2A. In the same change, replace the existing
`in_binary.c`, `in_vcd.c`, and `in_wav.c` modules with their current upstream
streaming counterparts from `src/input/binary.c`, `src/input/vcd.c`, and
`src/input/wav.c`. Replace each imported upstream file header with the
DSView/PXTOOL GPL header style. Do not leave old `sr_input_format` and new
`sr_input_module` registrations active at the same time.

Keep `gnuplot` in DSView's output registration after the upstream module
entries. Register `null` temporarily so the lifecycle test becomes executable.

Replace every copied upstream source header with the DSView/PXTOOL GPL header
style used by neighboring `libsigrok` files.

- [ ] **Step 4: Register the migrated framework in production and tests**

Add each imported framework/helper source to both source lists. The test list
must include precisely the same input/output core sources as production, plus
test stubs in place of hardware-only services.

```cmake
    libsigrok/input/input.c
    libsigrok/input/binary.c
    libsigrok/input/vcd.c
    libsigrok/input/wav.c
    libsigrok/output/output.c
    libsigrok/output/null.c
```

- [ ] **Step 5: Run lifecycle and baseline tests**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=creates_and_frees_output_with_default_options
./build.macOS/DSView-test --run_test=formatcapability
```

Expected: the lifecycle test passes; the final export-manifest test remains
red until all modules are added.

- [ ] **Step 6: Commit the framework contract**

```bash
git add libsigrok/libsigrok.h libsigrok/libsigrok-internal.h \
  libsigrok/input/input.c libsigrok/input/binary.c libsigrok/input/vcd.c \
  libsigrok/input/wav.c libsigrok/output/output.c \
  libsigrok/output/null.c CMakeLists.txt PXTOOL/test/CMakeLists.txt \
  PXTOOL/test/test_io_options.cpp PXTOOL/test/test_upstream_io_stubs.c
git commit -m "feat: migrate libsigrok io lifecycle contracts"
```

## Task 3: Add the Typed DSView Option Bridge

**Files:**
- Create: `PXTOOL/pv/data/iooptions.h`
- Create: `PXTOOL/pv/data/iooptions.cpp`
- Create: `PXTOOL/pv/dialogs/inputoutputoptionsdlg.h`
- Create: `PXTOOL/pv/dialogs/inputoutputoptionsdlg.cpp`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `PXTOOL/test/test_io_options.cpp`

- [ ] **Step 1: Write failing option conversion tests**

Add tests for default insertion, type rejection, and enum membership:

```cpp
BOOST_AUTO_TEST_CASE(option_values_use_module_defaults)
{
    const sr_option *options[] = {
        make_int_option("channels", 8, 1, 32),
        make_uint64_option("samplerate", 0),
        nullptr
    };

    pv::data::IoOptions values(options);
    BOOST_CHECK_EQUAL(values.value("channels").toInt(), 8);
    BOOST_CHECK_EQUAL(values.value("samplerate").toULongLong(), 0ULL);
}

BOOST_AUTO_TEST_CASE(option_values_reject_wrong_variant_type)
{
    const sr_option *options[] = { make_int_option("channels", 8, 1, 32), nullptr };
    pv::data::IoOptions values(options);
    BOOST_CHECK_THROW(values.set("channels", QVariant("eight")), std::invalid_argument);
}
```

- [ ] **Step 2: Run the option tests and verify they fail**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=option_values_*
```

Expected: compile failure because `IoOptions` does not exist.

- [ ] **Step 3: Implement `IoOptions`**

Define this public shape:

```cpp
namespace pv::data {

class IoOptions {
public:
    explicit IoOptions(const sr_option *const *options);
    QVariant value(const QString &id) const;
    void set(const QString &id, const QVariant &value);
    bool empty() const;
    GHashTable *toGHashTable() const;

private:
    struct Entry {
        const sr_option *definition;
        QVariant value;
    };
    QMap<QString, Entry> entries_;
};

}
```

`toGHashTable()` must allocate string keys with `g_strdup`, sink/ref each
`GVariant`, and install `g_free`/`g_variant_unref` destroy callbacks so the
caller owns exactly one table. Convert only supported GLib scalar variants;
throw `std::invalid_argument` for an unknown option ID, a mismatched type, or
an enum value not present in `definition->values`.

- [ ] **Step 4: Implement the reusable dialog**

Define:

```cpp
class InputOutputOptionsDlg final : public DSDialog {
    Q_OBJECT
public:
    InputOutputOptionsDlg(const QString &title,
                          const sr_option *const *options,
                          QWidget *parent = nullptr);
    const pv::data::IoOptions &options() const;
};
```

Build the form from the `IoOptions` entries. Use `QCheckBox` for booleans,
`QSpinBox`/`QDoubleSpinBox` for numeric values, `QLineEdit` for strings, and
`QComboBox` when `values` is non-empty. Read every widget value into
`IoOptions` only in `accept()` so Cancel does not mutate values.

- [ ] **Step 5: Run the option tests**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=option_values_*
```

Expected: all option conversion tests pass.

- [ ] **Step 6: Commit the option bridge**

```bash
git add PXTOOL/pv/data/iooptions.h PXTOOL/pv/data/iooptions.cpp \
  PXTOOL/pv/dialogs/inputoutputoptionsdlg.h \
  PXTOOL/pv/dialogs/inputoutputoptionsdlg.cpp CMakeLists.txt \
  PXTOOL/test/CMakeLists.txt PXTOOL/test/test_io_options.cpp
git commit -m "feat: add typed libsigrok io options bridge"
```

## Task 4: Convert `StoreSession` to the Standard Output Lifecycle

**Files:**
- Modify: `PXTOOL/pv/storesession.h`
- Modify: `PXTOOL/pv/storesession.cpp`
- Modify: `PXTOOL/test/test_output_fixtures.cpp`

- [ ] **Step 1: Write failing binary-safe writer tests**

Use the `binary` and `null` output modules with a four-sample logic packet:

```cpp
BOOST_AUTO_TEST_CASE(binary_output_preserves_nul_bytes)
{
    const QByteArray bytes = export_logic_fixture("binary", {0x00, 0x7f, 0x80, 0xff});
    BOOST_REQUIRE_EQUAL(bytes.size(), 4);
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(bytes.at(0)), 0x00);
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(bytes.at(3)), 0xff);
}

BOOST_AUTO_TEST_CASE(null_output_writes_no_payload)
{
    BOOST_CHECK(export_logic_fixture("null", {0x00, 0x01}).isEmpty());
}
```

- [ ] **Step 2: Run the writer tests and verify they fail**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=binary_output_preserves_nul_bytes,null_output_writes_no_payload
```

Expected: failure because the binary module is not registered and the current
production implementation uses `QTextStream`.

- [ ] **Step 3: Introduce an output-instance guard**

In `storesession.cpp`, add a local RAII type:

```cpp
class OutputInstance final {
public:
    OutputInstance(const sr_output_module *module, GHashTable *options,
                   const sr_dev_inst *sdi)
        : value_(sr_output_new(module, options, sdi)) {}
    ~OutputInstance() { if (value_) sr_output_free(value_); }
    const sr_output *get() const { return value_; }
private:
    const sr_output *value_ = nullptr;
};
```

Replace direct `_outModule->init()` and `_outModule->receive()` calls with
`OutputInstance` and `sr_output_send()`. Build the option table with
`IoOptions::toGHashTable()`, call `g_hash_table_destroy()` after
`sr_output_new()`, and never call a module callback directly.

- [ ] **Step 4: Replace text writes with a binary-safe append helper**

Add this helper in `StoreSession`:

```cpp
bool StoreSession::append_output(QFile &file, GString *chunk)
{
    if (!chunk)
        return true;
    const qint64 written = file.write(chunk->str, static_cast<qint64>(chunk->len));
    g_string_free(chunk, TRUE);
    return written == static_cast<qint64>(chunk->len);
}
```

Store `const gsize length = chunk->len` before freeing `chunk`; compare
`written` to `length`. Open ordinary export files with `QIODevice::WriteOnly |
QIODevice::Truncate`, not `QIODevice::Text`. On any failed write, set
`_has_error`, set `_error`, close the file, remove the partial ordinary output
file, and return.

- [ ] **Step 5: Run output lifecycle tests and existing export tests**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=binary_output_preserves_nul_bytes,null_output_writes_no_payload
./build.macOS/DSView-test --run_test=formatcapability
```

Expected: the `null` lifecycle assertion and existing format capability tests
pass. Keep the binary assertion red until Task 6 imports and registers the
upstream binary module; Task 6 runs this same assertion as part of its
fixture suite. This avoids registering a Task 6 output module before the
standard StoreSession lifecycle is in place.

- [ ] **Step 6: Commit the output lifecycle conversion**

```bash
git add PXTOOL/pv/storesession.h PXTOOL/pv/storesession.cpp \
  PXTOOL/test/test_output_fixtures.cpp
git commit -m "refactor: use standard libsigrok output lifecycle"
```

## Task 5: Make Format Capabilities Carry Extensions and Options

**Files:**
- Modify: `PXTOOL/pv/data/formatcapability.h`
- Modify: `PXTOOL/pv/data/formatcapability.cpp`
- Modify: `PXTOOL/pv/storesession.cpp`
- Modify: `PXTOOL/test/test_formatcapability.cpp`

- [ ] **Step 1: Write failing extension tests**

```cpp
BOOST_AUTO_TEST_CASE(export_capabilities_keep_declared_extensions)
{
    const auto formats = pv::data::exportFormats();
    BOOST_CHECK_EQUAL(findFormatById(formats, "chronovu-la8")->extensions,
                      QStringList({"kdt"}));
    BOOST_CHECK_EQUAL(findFormatById(formats, "ols")->extensions,
                      QStringList({"ols"}));
    BOOST_CHECK_EQUAL(findFormatById(formats, "wavedrom")->extensions,
                      QStringList({"wavedrom", "json"}));
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=export_capabilities_keep_declared_extensions
```

Expected: compilation failure because `FormatCapability` has no `extensions`
field.

- [ ] **Step 3: Extend `FormatCapability`**

Add:

```cpp
QStringList extensions;
bool hasOptions = false;
bool supportsLogic = false;
bool supportsAnalog = false;
bool acceptsAnyData = false;
```

Populate `extensions` from `sr_output_extensions_get()`, and set
`hasOptions` from `sr_output_options_get()`. Keep the fixed export order with
a DSView-owned rank table:

```cpp
static const QStringList kExportOrder = {
    "csv", "vcd", "gnuplot", "srzip", "analog", "ascii", "binary",
    "bits", "chronovu-la8", "hex", "null", "ols", "wav", "wavedrom"
};
```

Do not use the format ID as a suffix. Change `MakeExportFile()` to compose
the selected filter from `FormatCapability::extensions` and use the first
extension only when the user supplied no suffix.

- [ ] **Step 4: Run capability tests**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=formatcapability
```

Expected: legacy capability checks pass; the extension test remains red until
the target modules are added.

- [ ] **Step 5: Commit descriptor support**

```bash
git add PXTOOL/pv/data/formatcapability.h PXTOOL/pv/data/formatcapability.cpp \
  PXTOOL/pv/storesession.cpp PXTOOL/test/test_formatcapability.cpp
git commit -m "feat: expose io extensions and options in format capabilities"
```

## Task 6: Port and Register Logic Output Modules

**Files:**
- Create: `libsigrok/output/ascii.c`
- Create: `libsigrok/output/binary.c`
- Create: `libsigrok/output/bits.c`
- Create: `libsigrok/output/chronovu_la8.c`
- Create: `libsigrok/output/hex.c`
- Create: `libsigrok/output/ols.c`
- Create: `libsigrok/output/wavedrom.c`
- Modify: `libsigrok/output/output.c`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `PXTOOL/test/test_output_fixtures.cpp`

- [ ] **Step 1: Write failing per-format logic fixture tests**

Add a table-driven test:

```cpp
struct LogicOutputExpectation {
    const char *id;
    QByteArray prefix;
    QString suffix;
};

const LogicOutputExpectation expectations[] = {
    {"ascii", QByteArray(""), "txt"},
    {"binary", QByteArray("\x00\x01\x03", 3), ""},
    {"bits", QByteArray(""), "txt"},
    {"chronovu-la8", QByteArray(), "kdt"},
    {"hex", QByteArray(""), "txt"},
    {"ols", QByteArray(), "ols"},
    {"vcd", QByteArray("$date"), "vcd"},
    {"wavedrom", QByteArray("{"), "wavedrom"},
};
```

For each expectation, export the same four-channel logic fixture, assert that
the module can be resolved, that the file exists, and that a non-empty prefix
matches when `prefix` is non-empty.

- [ ] **Step 2: Run the fixture test and verify it fails**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=exports_logic_formats
```

Expected: failure because only VCD is currently registered.

- [ ] **Step 3: Import current upstream output sources**

For each source, import from:

```text
/Users/yuanji/Desktop/project/libsigrok/src/output/ascii.c
/Users/yuanji/Desktop/project/libsigrok/src/output/binary.c
/Users/yuanji/Desktop/project/libsigrok/src/output/bits.c
/Users/yuanji/Desktop/project/libsigrok/src/output/chronovu_la8.c
/Users/yuanji/Desktop/project/libsigrok/src/output/hex.c
/Users/yuanji/Desktop/project/libsigrok/src/output/ols.c
/Users/yuanji/Desktop/project/libsigrok/src/output/wavedrom.c
```

Replace each upstream header with the DSView/PXTOOL GPL header. Preserve
upstream module IDs, descriptions, extensions, option definitions, lifecycle,
and packet handling. Adapt only direct DSView API differences at the
compatibility boundary; do not replace the module's format algorithm.

Register modules in `output.c` after `srzip`, in this exact order:

```c
&output_ascii,
&output_binary,
&output_bits,
&output_chronovu_la8,
&output_hex,
&output_ols,
&output_wavedrom,
```

- [ ] **Step 4: Compile the same sources in application and test targets**

Add every new source to both `libsigrok_SOURCES` and `DSView-test`. Do not
register a module before its source is compiled in both targets.

- [ ] **Step 5: Run logic fixtures and final-order test**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=exports_logic_formats
./build.macOS/DSView-test --run_test=enumerates_final_export_format_manifest
```

Expected: logic fixture checks pass for all registered logic formats; the
final manifest still lacks analog and WAV modules.

- [ ] **Step 6: Commit logic format support**

```bash
git add libsigrok/output/ascii.c libsigrok/output/binary.c \
  libsigrok/output/bits.c libsigrok/output/chronovu_la8.c \
  libsigrok/output/hex.c libsigrok/output/ols.c \
  libsigrok/output/wavedrom.c libsigrok/output/output.c \
  CMakeLists.txt PXTOOL/test/CMakeLists.txt PXTOOL/test/test_output_fixtures.cpp
git commit -m "feat: add upstream logic output formats"
```

## Task 7: Add Standard Analog Packets and Analog Output Modules

**Files:**
- Create: `PXTOOL/pv/data/analogpacketadapter.h`
- Create: `PXTOOL/pv/data/analogpacketadapter.cpp`
- Create: `libsigrok/output/analog.c`
- Create: `libsigrok/output/wav.c`
- Modify: `libsigrok/output/output.c`
- Modify: `PXTOOL/pv/storesession.cpp`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `PXTOOL/test/test_analogpacketadapter.cpp`
- Modify: `PXTOOL/test/test_output_fixtures.cpp`

- [ ] **Step 1: Write failing analog adapter tests**

```cpp
BOOST_AUTO_TEST_CASE(analog_adapter_preserves_channel_order_and_metadata)
{
    const auto packet = pv::data::makeAnalogPacket(
        {{"A0", 0}, {"A1", 1}},
        {0.0F, 1.0F, 0.5F, -0.5F},
        2,
        1'000'000,
        SR_MQ_VOLTAGE,
        SR_UNIT_VOLT);

    BOOST_CHECK_EQUAL(g_slist_length(packet.analog.meaning->channels), 2);
    BOOST_CHECK_EQUAL(packet.analog.num_samples, 2);
    BOOST_CHECK_EQUAL(packet.samplerate, 1'000'000ULL);
}

BOOST_AUTO_TEST_CASE(analog_outputs_accept_standard_packets)
{
    BOOST_CHECK(export_analog_fixture("analog").startsWith("FRAME-BEGIN"));
    BOOST_CHECK(has_wave_header(export_analog_fixture("wav")));
}
```

- [ ] **Step 2: Run analog tests and verify they fail**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=analog_adapter_*,analog_outputs_accept_standard_packets
```

Expected: compile failure because the adapter and modules do not exist.

- [ ] **Step 3: Implement the packet owner**

Define an owning packet object:

```cpp
namespace pv::data {

struct AnalogPacket {
    sr_datafeed_packet packet{};
    sr_datafeed_analog analog{};
    sr_datafeed_meta meta{};
    uint64_t samplerate = 0;
    std::vector<float> samples;
    GSList *channels = nullptr;
    sr_analog_encoding encoding{};
    sr_analog_meaning meaning{};

    ~AnalogPacket();
};

AnalogPacket makeAnalogPacket(const QVector<AnalogChannelRef> &channels,
                              const std::vector<float> &interleavedSamples,
                              uint64_t samplesPerChannel,
                              uint64_t samplerate,
                              int mq,
                              int unit);

}
```

Set `packet.type` to `SR_DF_ANALOG`, set `packet.payload` to `&analog`, and
free the `GSList` in the destructor. The adapter must reject empty channels,
non-interleaved data, and a sample count that does not equal
`samplesPerChannel * channelCount`.

- [ ] **Step 4: Import and register upstream analog/WAV modules**

Import:

```text
/Users/yuanji/Desktop/project/libsigrok/src/output/analog.c
/Users/yuanji/Desktop/project/libsigrok/src/output/wav.c
```

Replace source headers with the DSView/PXTOOL GPL header. Register `analog`
and `wav` after `srzip` and before the logic additions:

```c
&output_analog,
&output_wav,
```

Add both files to production and test CMake lists.

- [ ] **Step 5: Adapt `StoreSession` analog export**

Replace raw `sr_datafeed_analog` stack construction with
`makeAnalogPacket()`. Pass the current snapshot samplerate and DSView's
available voltage quantity/unit. If the current DSO snapshot cannot provide a
valid standard analog encoding, reject `analog` and `wav` before opening the
destination file:

```cpp
if (format_requires_standard_analog(_outModule) && !can_make_analog_packet(snapshot)) {
    _has_error = true;
    _error = tr("%1 requires standard analog samples.")
        .arg(QString::fromUtf8(_outModule->desc));
    return;
}
```

- [ ] **Step 6: Run analog, baseline, and manifest tests**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=analog_adapter_*,analog_outputs_accept_standard_packets
./build.macOS/DSView-test --run_test=enumerates_final_export_format_manifest
```

Expected: all analog tests and the complete fourteen-format manifest pass.

- [ ] **Step 7: Commit analog output support**

```bash
git add PXTOOL/pv/data/analogpacketadapter.h \
  PXTOOL/pv/data/analogpacketadapter.cpp libsigrok/output/analog.c \
  libsigrok/output/wav.c libsigrok/output/output.c \
  PXTOOL/pv/storesession.cpp CMakeLists.txt PXTOOL/test/CMakeLists.txt \
  PXTOOL/test/test_analogpacketadapter.cpp PXTOOL/test/test_output_fixtures.cpp
git commit -m "feat: add standard analog export support"
```

## Task 8: Complete Export UI and Compatibility Validation

**Files:**
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/mainwindow.h`
- Modify: `PXTOOL/pv/toolbars/filebar.cpp`
- Modify: `PXTOOL/pv/toolbars/filebar.h`
- Modify: `PXTOOL/pv/storesession.cpp`
- Modify: `PXTOOL/pv/dialogs/storeprogress.cpp`
- Modify: `PXTOOL/pv/dialogs/storeprogress.h`
- Modify: `PXTOOL/test/test_formatcapability.cpp`

- [ ] **Step 1: Write failing menu-order and option-routing tests**

```cpp
BOOST_AUTO_TEST_CASE(export_menu_order_keeps_dsview_formats_first)
{
    const QStringList expected = {
        "csv", "vcd", "gnuplot", "srzip", "analog", "ascii", "binary",
        "bits", "chronovu-la8", "hex", "null", "ols", "wav", "wavedrom"
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(exportMenuIds().cbegin(), exportMenuIds().cend(),
                                  expected.cbegin(), expected.cend());
}

BOOST_AUTO_TEST_CASE(option_dialog_is_required_only_for_optioned_formats)
{
    BOOST_CHECK(formatRequiresOptions("ascii"));
    BOOST_CHECK(formatRequiresOptions("wav"));
    BOOST_CHECK(!formatRequiresOptions("binary"));
    BOOST_CHECK(!formatRequiresOptions("null"));
}
```

- [ ] **Step 2: Run the tests and verify the missing UI helper fails**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=export_menu_order_keeps_dsview_formats_first,option_dialog_is_required_only_for_optioned_formats
```

Expected: failure until the capability layer exposes options and menu IDs.

- [ ] **Step 3: Route export selection through one method**

Add:

```cpp
void MainWindow::on_export_format(const QString &formatId)
{
    const auto *format = pv::data::findFormatById(pv::data::exportFormats(), formatId);
    if (!format)
        return;

    pv::data::IoOptions options(format->outputOptions);
    if (format->hasOptions) {
        dialogs::InputOutputOptionsDlg dlg(
            tr("Export %1").arg(format->description), format->outputOptions, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        options = dlg.options();
    }

    _selected_export_format_id = formatId;
    _selected_export_options = options;
    on_export();
}
```

Carry `_selected_export_options` through `StoreProgress` to `StoreSession`;
clear it immediately after each export transaction. `FileBar` only emits the
format ID and must not instantiate dialogs.

- [ ] **Step 4: Validate before file creation**

Add `StoreSession::validateExportFormat()` and call it before
`MakeExportFile(true)`. Its result must reject logic-only modules for analog
data, analog-only modules for logic data, and unsupported DSO adaptation with
the human-readable format description.

- [ ] **Step 5: Run UI-independent tests and launch the application**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=formatcapability
cmake --build . --target DSView
bash scripts/macOS/build_and_run.sh
```

Manual expected result:

- Both File menus show the existing four export formats first and the ten
  upstream formats after them.
- Choosing ASCII, bits, hex, analog, or WAV opens the options dialog.
- Choosing raw binary, null, OLS, ChronoVu, VCD, or WaveDrom goes directly to
  the save dialog.
- Exporting null completes with an intentionally empty file.
- Binary and WAV files are not corrupted by text conversion.

- [ ] **Step 6: Commit Export completion**

```bash
git add PXTOOL/pv/mainwindow.cpp PXTOOL/pv/mainwindow.h \
  PXTOOL/pv/toolbars/filebar.cpp PXTOOL/pv/toolbars/filebar.h \
  PXTOOL/pv/storesession.cpp PXTOOL/pv/dialogs/storeprogress.cpp \
  PXTOOL/pv/dialogs/storeprogress.h PXTOOL/test/test_formatcapability.cpp
git commit -m "feat: complete upstream export format integration"
```

## Task 9: Verify Streaming Input Modules Before Enabling Import UI

**Files:**
- Modify: `PXTOOL/test/test_input_fixtures.cpp`
- Modify: `PXTOOL/test/test_upstream_io_stubs.c`

- [ ] **Step 1: Write a failing streaming-input test**

```cpp
BOOST_AUTO_TEST_CASE(binary_input_streams_logic_packets)
{
    auto input = createInput("binary", options({
        {"numchannels", 2},
        {"samplerate", 1'000'000ULL},
    }));

    input.send(QByteArray::fromHex("00010302"));
    input.end();

    BOOST_CHECK_EQUAL(input.observer().logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.observer().logicSamples(), 4);
    BOOST_CHECK_EQUAL(input.observer().samplerate(), 1'000'000ULL);
}
```

- [ ] **Step 2: Run the input test and verify it fails**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=binary_input_streams_logic_packets
```

Expected: test failure because the migrated streaming modules are not yet
covered by a datafeed observer.

- [ ] **Step 3: Add a test input observer**

In `test_upstream_io_stubs.c`, register a test datafeed callback that records
header/meta/logic/analog/end packets. Expose C accessors consumed by
`test_input_fixtures.cpp`:

```c
void test_input_observer_reset(void);
unsigned int test_input_observer_logic_packets(void);
uint64_t test_input_observer_logic_samples(void);
unsigned int test_input_observer_analog_packets(void);
uint64_t test_input_observer_samplerate(void);
bool test_input_observer_saw_end(void);
```

- [ ] **Step 4: Run binary, VCD, and WAV parser tests**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=binary_input_streams_logic_packets
./build.macOS/DSView-test --run_test=vcd_input_streams_logic_packets
./build.macOS/DSView-test --run_test=wav_input_streams_analog_packets
```

Expected: all tests pass while MainWindow still retains the placeholder
non-DSL import behavior.

- [ ] **Step 5: Commit streaming input verification**

```bash
git add PXTOOL/test/test_input_fixtures.cpp PXTOOL/test/test_upstream_io_stubs.c
git commit -m "test: cover streaming input formats"
```

## Task 10: Connect VCD, WAV, and Raw Binary Import to Waveforms

**Files:**
- Create: `PXTOOL/pv/data/inputimporter.h`
- Create: `PXTOOL/pv/data/inputimporter.cpp`
- Modify: `PXTOOL/pv/mainwindow.cpp`
- Modify: `PXTOOL/pv/mainwindow.h`
- Modify: `PXTOOL/pv/sigsession.h`
- Modify: `PXTOOL/pv/sigsession.cpp`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/test_input_fixtures.cpp`

- [ ] **Step 1: Write failing import transaction tests**

```cpp
BOOST_AUTO_TEST_CASE(import_transaction_keeps_active_session_on_parse_error)
{
    FakeSession session;
    const auto result = pv::data::InputImporter::importFile(
        session, "vcd", "/tmp/not-a-vcd-file.vcd", {});

    BOOST_CHECK(!result.ok);
    BOOST_CHECK_EQUAL(session.switchCount(), 0);
}

BOOST_AUTO_TEST_CASE(raw_binary_import_uses_selected_options)
{
    FakeSession session;
    const auto result = pv::data::InputImporter::importFile(
        session, "binary", fixturePath("logic.bin"),
        options({{"numchannels", 2}, {"samplerate", 500000ULL}}));

    BOOST_REQUIRE(result.ok);
    BOOST_CHECK_EQUAL(session.logicChannelCount(), 2);
    BOOST_CHECK_EQUAL(session.samplerate(), 500000ULL);
}
```

- [ ] **Step 2: Run import tests and verify they fail**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=import_transaction_*
```

Expected: compile failure because `InputImporter` does not exist.

- [ ] **Step 3: Implement a two-phase importer**

Define:

```cpp
namespace pv::data {

struct ImportResult {
    bool ok = false;
    QString error;
};

class InputImporter {
public:
    static ImportResult importFile(SigSession &session,
                                   const QString &formatId,
                                   const QString &fileName,
                                   const IoOptions &options);
};

}
```

The implementation must:

1. Resolve the input module by ID.
2. Create the streaming input with a fresh typed option table.
3. Read the file in fixed-size binary chunks and submit them to the input.
4. Wait for header/device creation and validate at least one enabled channel.
5. Only then switch the active DSView session/device.
6. On parse failure, release the input device and return an error without
   changing the active session.

Use a `QFile` opened with `QIODevice::ReadOnly`, never `QTextStream`.

- [ ] **Step 4: Replace the import placeholder UI**

Replace `MainWindow::on_import_file()` with:

```cpp
const auto *format = pv::data::findFormatById(pv::data::importFormats(), formatId);
if (!format)
    return;

pv::data::IoOptions options(format->inputOptions);
if (format->hasOptions) {
    dialogs::InputOutputOptionsDlg dlg(
        tr("Import %1").arg(format->description), format->inputOptions, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    options = dlg.options();
}

const auto result = pv::data::InputImporter::importFile(*_session, formatId, fileName, options);
if (!result.ok)
    MsgBox::Show(result.error);
```

Keep `.dsl` mapped to `on_actionOpen_triggered()` and
`ds_device_from_file()`; it must not enter `InputImporter`.

- [ ] **Step 5: Run fixtures, round trips, and app build**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=import_transaction_*
./build.macOS/DSView-test --run_test=vcd_wav_binary_round_trip
cmake --build . --target DSView
bash scripts/macOS/build_and_run.sh
```

Manual expected result:

- `Open...` and `Import > Open DSView Data...` still open `.dsl`.
- Import VCD displays logic traces and source samplerate.
- Import WAV displays analog traces and source samplerate.
- Import raw binary opens the options dialog and honors selected channel count
  and samplerate.
- Invalid files keep the existing waveform session visible.

- [ ] **Step 6: Commit waveform import**

```bash
git add PXTOOL/pv/data/inputimporter.h PXTOOL/pv/data/inputimporter.cpp \
  PXTOOL/pv/mainwindow.cpp PXTOOL/pv/mainwindow.h \
  PXTOOL/pv/sigsession.h PXTOOL/pv/sigsession.cpp CMakeLists.txt \
  PXTOOL/test/test_input_fixtures.cpp
git commit -m "feat: import vcd wav and raw binary waveforms"
```

## Task 11: Finish Regression Coverage and Verify the Release Candidate

**Files:**
- Modify: `PXTOOL/test/test_formatcapability.cpp`
- Modify: `PXTOOL/test/test_output_fixtures.cpp`
- Modify: `PXTOOL/test/test_input_fixtures.cpp`
- Modify: `PXTOOL/test/test_analogpacketadapter.cpp`
- Modify: `docs/superpowers/specs/2026-07-20-libsigrok-modern-io-framework-design.md`

- [ ] **Step 1: Add final cross-contract tests**

Add tests that:

```cpp
BOOST_AUTO_TEST_CASE(compiled_registration_and_capability_lists_match)
{
    BOOST_CHECK_EQUAL(runtimeOutputIds(), capabilityOutputIds());
    BOOST_CHECK_EQUAL(runtimeInputIds(), capabilityInputIds());
}

BOOST_AUTO_TEST_CASE(dsview_native_dsl_is_not_a_generic_io_format)
{
    BOOST_CHECK(!capabilityInputIds().contains("dsl"));
    BOOST_CHECK(!capabilityOutputIds().contains("dsl"));
}
```

Add an output failure test that attempts analog export from an unsupported DSO
fixture and verifies no destination file exists after the error.

- [ ] **Step 2: Run the full focused I/O suite**

Run:

```bash
cmake --build . --target DSView-test
./build.macOS/DSView-test --run_test=formatcapability
./build.macOS/DSView-test --run_test=io_migration/*
```

Expected: all test cases pass with no skipped final-manifest or fixture tests.

- [ ] **Step 3: Run full build and macOS launch**

Run:

```bash
cmake --build . --target DSView
bash scripts/macOS/build_and_run.sh
```

Expected: both commands exit with status 0 and PXTOOL launches.

- [ ] **Step 4: Update the design verification status**

Append a `## Verification` section to the design document containing the exact
commands above, their date, and pass status. Do not change the approved
architecture sections.

- [ ] **Step 5: Commit release verification**

```bash
git add PXTOOL/test/test_formatcapability.cpp \
  PXTOOL/test/test_output_fixtures.cpp PXTOOL/test/test_input_fixtures.cpp \
  PXTOOL/test/test_analogpacketadapter.cpp \
  docs/superpowers/specs/2026-07-20-libsigrok-modern-io-framework-design.md
git commit -m "test: verify modern libsigrok io migration"
```

## Plan Self-Review

- Spec coverage: Tasks 1-2 establish one modern I/O contract; Tasks 3 and 8
  supply the shared option UI; Tasks 4-8 complete all fourteen output IDs;
  Tasks 9-10 delay waveform import until Export completion; Task 7 establishes
  the analog packet boundary required by later ADC work; Task 11 verifies
  runtime registration, native DSL isolation, and macOS launch.
- Scope: the plan does not migrate DSView hardware drivers, native DSL
  serialization, or implement ADC hardware acquisition.
- Type consistency: `IoOptions` is the only Qt-to-GVariant conversion type;
  `InputOutputOptionsDlg` returns it; `StoreSession` and `InputImporter` both
  create their libsigrok option tables through it; `AnalogPacket` is the only
  generic analog packet owner.
- Placeholder scan: every source import names an exact upstream file, every
  source/module build list is explicit, and each task has a concrete test,
  command, expected result, and commit.
