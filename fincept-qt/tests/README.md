# Unit tests

Qt Test suites for the pure, side-effect-free units of Fincept Terminal.
Off by default: a normal `cmake --preset win-dev` build compiles none of this.

These complement — they do not replace — the headless `--selftest-*` flags on the
main binary, which cover end-to-end screen/engine behaviour and run in
`.github/workflows/build-pr.yml`.

## Configure

Add `-DFINCEPT_BUILD_TESTS=ON` to any normal configure. Everything else stays the
same; the option only adds test targets.

```powershell
cmake --preset win-dev -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset win-dev
```

Or without a preset:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/kit -DFINCEPT_BUILD_TESTS=ON
cmake --build build
```

## Run

```bash
ctest --test-dir build --output-on-failure
```

On a multi-config generator (the Visual Studio generator, which is what CI uses on
Windows) add the config:

```bash
ctest --test-dir build -C Release --output-on-failure
```

One suite at a time:

```bash
ctest --test-dir build --output-on-failure -R tst_order_validator
```

Or run an executable directly for Qt Test's own flags (`-functions`, `-v2`,
a single slot by name):

```bash
./build/tests/tst_order_validator -functions
./build/tests/tst_order_validator smart_flatten_with_zero_quantity_is_valid
```

Test binaries live in `build/tests/`, never next to `FinceptTerminal.exe`, and
have no `install()` rule — they cannot end up in a release artifact.

## THE RULE: a test target lists its own sources

**Every test target names the specific app sources it needs, explicitly, by path.
Never link the whole application into a test.**

```cmake
fincept_add_test(tst_order_validator
    tst_order_validator.cpp
    "${PROJECT_SOURCE_DIR}/src/trading/OrderValidator.cpp")
```

The tempting alternative — splitting the app into an `OBJECT` library and linking
it into both the app and the tests — is **forbidden here**. The release and CI
presets build with `CMAKE_UNITY_BUILD=ON` (batch size 20) while `win-dev` builds
unity OFF. Any change to the main source list re-batches every file after it, and
two file-scope symbols that land in the same batch become a hard redefinition
error **that appears only on CI and never reproduces locally**. A restructure to
make testing convenient is not worth breaking the release build.

The suites currently cost three executables and five translation units in total.
Keep it that way.

### When a unit resists testing

If you cannot test something without dragging in services, the broker registry,
`QNetworkAccessManager`, `PythonRunner` or a database, **that is a fact about the
unit, not a reason to grow the link line.** Extract the pure logic into a small
header (or a leaf `.cpp` with no app includes) and test that. If you decide not to
test it yet, say so in the PR rather than adding a heavyweight target.

Known example: `extract_json()` and `extract_error_envelope()` in
`src/python/PythonRunner.cpp` are exactly the kind of pure string/JSON logic that
belongs under test, but `extract_error_envelope` is file-`static` (unreachable
from another TU) and reaching `extract_json` means linking `PythonRunner.cpp`,
which pulls in `Logger`, `PythonSetupManager`, `PythonWorker`, `SecureStorage` and
`config/KeyedConnectorCredentials.inc`. They should move to a small
`PythonPayload.h` first; then a `tst_python_payload` target costs one TU.

## Conventions

- One executable per unit, named `tst_<unit>`; register it with
  `fincept_add_test()` in `CMakeLists.txt`.
- `QTEST_GUILESS_MAIN` — no `QApplication`, no display, no `QT_QPA_PLATFORM`
  juggling in CI.
- The `Q_OBJECT` test class lives in the `.cpp`; end the file with
  `#include "tst_<unit>.moc"` (AUTOMOC generates it).
- Link `Qt6::Test` and `Qt6::Core` only. Needing `Widgets`, `Network` or `Sql` in
  a unit test is a signal you are testing at the wrong seam.
- Name slots after the behaviour, not the function
  (`smart_flatten_with_zero_quantity_is_valid`, not `test_validate_smart_1`).
- Pin real incidents as named regression tests
  (`regression_iifl_price_modify`) so the next reader learns why the code is
  shaped the way it is.
- Assert behaviour that is actually documented or load-bearing. A test of an
  incidental implementation detail is a future false alarm.
