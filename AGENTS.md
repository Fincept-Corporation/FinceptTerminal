# AGENTS.md
This file is for coding agents working in this repository.

## Scope
- Repo-wide guide; almost all implementation work happens in `fincept-qt/`.
- Main stack: `C++20` + `Qt6 Widgets`, plus embedded Python scripts.
- Do not assume a Node/TypeScript/web-app workflow; there is no repo-level `package.json`.
- When docs disagree, prefer enforced build/config sources first: `fincept-qt/CMakeLists.txt`, `fincept-qt/CMakePresets.json`, `.github/workflows/*.yml`, then `README.md` and `docs/CONTRIBUTING.md`.

## Key Docs
- `README.md` - build, run, pinned toolchain
- `docs/CONTRIBUTING.md` - workflow, conventions, repo layout
- `docs/CPP_CONTRIBUTOR_GUIDE.md` - C++/Qt rules
- `docs/PYTHON_CONTRIBUTOR_GUIDE.md` - Python script contract
- `docs/ARCHITECTURE.md` - module boundaries and dependency direction
- `.github/CONTRIBUTING.md` - PR policy and scope gate
- `.github/PULL_REQUEST_TEMPLATE/pull_request_for_terminal.md` - PR checklist and hard rules

## Cursor / Copilot Rules
- No `.cursor/rules/` directory found.
- No `.cursorrules` file found.
- No `.github/copilot-instructions.md` file found.
- If any of those files are added later, treat them as additional agent instructions.

## Working Directory Rules
- Run CMake configure/build/test/lint commands from `fincept-qt/`.
- Preset build output goes under `fincept-qt/build/<preset>/`.
- Docs and policy files live at repo root, `docs/`, and `.github/`.

## Build Commands
```bash
cmake --preset win-release
cmake --preset linux-release
cmake --preset macos-release
cmake --build --preset win-release
cmake --build --preset linux-release
cmake --build --preset macos-release
cmake --preset win-debug
cmake --preset linux-debug
cmake --preset macos-debug
cmake --build --preset win-debug
cmake --build --preset linux-debug
cmake --build --preset macos-debug
```
- Fast Windows iteration: `cmake --preset win-fast && cmake --build --preset win-fast`
- Windows LTO build: `cmake --preset win-release-lto && cmake --build --preset win-release-lto`
- macOS ASAN build: `cmake --preset macos-asan && cmake --build --preset macos-asan`
- On older machines, cap jobs with `--parallel 4`.

## Run Commands
```bash
./build/linux-release/FinceptTerminal
./build/macos-release/FinceptTerminal.app/Contents/MacOS/FinceptTerminal
.\build\win-release\FinceptTerminal.exe
```

## Lint and Static Analysis
```bash
find src \( -name "*.cpp" -o -name "*.h" \) | sort | xargs clang-format --style=file --dry-run --Werror
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DFINCEPT_BUILD_INSTALLER=OFF -DDEPLOY_QT=OFF
find src -name "*.cpp" | grep -v "/moc_" | grep -v "/qrc_" | sort | xargs clang-tidy --config-file=.clang-tidy -p build --extra-arg=-std=c++20 2>&1
cppcheck --enable=warning,performance,portability --suppressions-list=.cppcheck-suppressions --inline-suppr --std=c++20 --error-exitcode=1 -I src -j "$(nproc)" src 2>&1
```

## Test Commands
### C++ / Qt tests
- C++ tests are opt-in; configure with `-DFINCEPT_BUILD_TESTS=ON`.
```bash
cmake --preset linux-release -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset linux-release
ctest --test-dir build/linux-release --output-on-failure
ctest --test-dir build/linux-release -N
ctest --test-dir build/linux-release --output-on-failure -R "^<test_name>$"
ctest --test-dir build/linux-release --output-on-failure -R "^<prefix>"
```
- Swap `linux-release` for `win-release` or `macos-release` as needed.

### Python tests
- Python tests exist in some subpackages, not one unified repo harness.
- Run the smallest relevant scope first: single test, then file, then package.
```bash
pytest path/to/test_file.py::test_name -q
pytest path/to/test_file.py -q
pytest path/to/tests -q
pytest fincept-qt/scripts/agents/finagent_core/tests/test_persona_registry.py::test_register_persona -q
```

## Architecture Rules
- Keep dependency flow one-way: Presentation -> Application -> Data Plane -> Adapters -> Infrastructure.
- `src/screens/` is UI only; do not put HTTP, business logic, or raw data-fetching there.
- `src/services/` owns fetching, caching, orchestration, and Python bridging.
- For streaming/live data, prefer DataHub topics and subscriptions over direct service fetch calls from screens.
- Never call `PythonRunner::instance().run(...)` directly from `src/screens/`; route through services/DataHub.
- Never block the UI thread; avoid `waitForFinished()` on the main thread.
- Start/stop timers in `showEvent()` / `hideEvent()`, not constructors.

## C++ Style
- Standard: `C++20`.
- Follow `fincept-qt/.clang-format`: 4 spaces, no tabs, 120 columns, K&R braces.
- Include order: local headers, then Qt headers, then standard library headers.
- `PascalCase` for classes/structs/enums.
- `snake_case` for namespaces, functions, methods, variables, and parameters.
- Trailing `_` for members.
- Prefer explicit `std::` and `Qt::`; never `using namespace std;`.
- Bind pointers/references to the type: `QString* value`, not `QString *value`.
- Use pointer-to-member Qt signal/slot syntax, never `SIGNAL()` / `SLOT()`.
- Add `Q_OBJECT` to QObject subclasses that need it.

## C++ Error Handling and Logging
- Prefer `Result<T>` and `std::optional` over raw error-code plumbing.
- Use early returns for invalid states and failed lookups.
- In UI/service paths, avoid exception-heavy APIs across module boundaries.
- Use `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, and `LOG_DEBUG` with a clear context tag.
- Never log secrets, tokens, API keys, or credentials.
- Use `QPointer` guards for async UI callbacks when lifetime is uncertain.

## Python Style
- Match surrounding style; do not mass-reformat files.
- Use `snake_case` for functions and variables.
- Prefer type hints on new or modified functions.
- For app-driven scripts, accept structured input and return JSON on stdout.
- Prefer structured error payloads over ad hoc prints.
- Use `logging.getLogger(__name__)` and `logger.info(...)` / `logger.warning(...)` / `logger.error(...)`.
- Never use `print()` for production behavior the app depends on.

## Imports, Boundaries, and Review Hygiene
- Follow naming rules already encoded in `fincept-qt/.clang-tidy`.
- Keep includes/imports minimal and local to the file's responsibility.
- Do not add cross-context dependencies when a service, adapter, event, or DataHub topic already exists.
- Reuse existing widgets, services, repositories, and infrastructure before creating new abstractions.
- Keep diffs minimal and scoped to one logical change.
- No auto-formatter churn on untouched code.
- No drive-by wording/comment cleanups unless explicitly requested.
- If you add a new `.cpp` file, wire it into the relevant `CMakeLists.txt`.
- If a change affects UI behavior, verify it in the running app when possible.

## PR and Branch Expectations
- Use topic branches such as `feat/...`, `fix/...`, `docs/...`, `perf/...`, `refactor/...`.
- Preferred commit style: `type: short imperative subject line`.
- PRs must link an approved issue with `Closes #NNN`.
- One logical change per PR.
- Describe the user-visible effect and the reason for the change.

## Agent Checklist Before Finishing
- Read the closest relevant docs before editing.
- Run the narrowest useful verification command for what you changed.
- If you touched C++ build logic, compile it.
- If you touched tests, run the smallest relevant test first.
- If you touched screens/services/data flow, re-check the screen/service/DataHub boundaries.
- In your final summary, state exactly what you verified and what you did not verify.
