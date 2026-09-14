# macOS Qt Framework Cleanup Design

## Goal

Make the development launcher discard stale packaged Qt artifacts, and make
the distributable bundle preserve Apple system-framework load paths after
`macdeployqt` runs on macOS 26.

## Scope

- `scripts/macOS/build_and_run.sh` removes packaged Qt frameworks, Qt dylibs,
  plugins, and `qt.conf` before signing and launching the development app.
- `scripts/macOS/package-macos.sh` repairs only unresolved `@rpath` framework
  imports whose framework exists under `/System/Library/Frameworks` and is not
  bundled in the app.
- Existing non-Qt frameworks such as `Python.framework` remain untouched.
- Existing Qt deployment, dependency verification, and signing remain in
  place.

## Behavior

The development launcher cleans `Contents/Frameworks/Qt*.framework` and Qt
dylibs in addition to its existing plugin and `qt.conf` cleanup. The rebuilt
development executable therefore loads the active Homebrew Qt installation
instead of stale framework copies from an earlier package run.

The package script starts from a clean Qt deployment, runs `macdeployqt`, then
walks the app's Mach-O files. For each dependency shaped like
`@rpath/Name.framework/...`, it rewrites the dependency to
`/System/Library/Frameworks/Name.framework/...` only when the app does not
contain `Name.framework` and macOS exposes that system framework directory.
All other dependencies are left unchanged. The existing verification then
rejects any dependency that remains unresolved or external.

## Error Handling

Failure to inspect or rewrite a Mach-O file terminates packaging with a clear
error. The script must not silently accept an unresolved framework import.

## Tests

- Extend the development-launcher regression test with stale Qt framework and
  dylib fixtures and assert that they are removed while Python remains.
- Add a package regression fixture in which `macdeployqt` leaves
  `@rpath/Carbon.framework/...`; assert that packaging requests the exact
  `/System/Library/Frameworks/Carbon.framework/...` rewrite.
- Run the macOS script tests, the real packaging script with `--no-dmg`, and
  launch the resulting app while checking dependency paths and code signing.
