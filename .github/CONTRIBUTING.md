# Contributing to Fincept Terminal

Fincept Terminal is a native C++20/Qt6 desktop financial terminal. All contributions are welcome!

## Ways to Contribute

- **C++ / Qt6** — new screens, performance fixes, core infrastructure
- **Python Analytics** — data scripts, AI agents, quant modules
- **Data Sources** — connect APIs, brokers, government financial databases
- **Bug Reports & Feature Requests** — open an issue

## Getting Started

**Pinned toolchain (enforced by CMake):** Qt 6.7.2 · CMake 3.27.7 · MSVC 19.38 / GCC 12.3 / Apple Clang 15.0 · Python 3.11.9. See [docs/CONTRIBUTING.md](../docs/CONTRIBUTING.md) for the full list.

```bash
# Fork & clone
git clone https://github.com/your-username/FinceptTerminal.git
cd FinceptTerminal

# Automated setup (installs toolchain + Qt 6.7.2 via aqtinstall, then builds)
./setup.sh       # Linux / macOS
setup.bat        # Windows (run from VS 2022 Developer Cmd)

# ── OR manual build via CMake presets ──
cd fincept-qt
cmake --preset linux-release   && cmake --build --preset linux-release     # Linux
cmake --preset macos-release   && cmake --build --preset macos-release     # macOS
cmake --preset win-release     && cmake --build --preset win-release       # Windows

# Create a branch
git checkout -b feature/your-feature
```

## Code Guidelines

- C++20, `snake_case` for functions/variables, `PascalCase` for types
- Screens render UI only — no business logic or direct HTTP calls
- Never block the UI thread (`waitForFinished()` forbidden on main thread)
- Use `Result<T>` for error handling, `LOG_INFO`/`LOG_ERROR` for logging

## Submitting a PR

1. Push your branch and open a Pull Request against `main`
2. Link any related issues
3. Keep changes focused — one feature or fix per PR

Questions? [support@fincept.in](mailto:support@fincept.in) · [Discord](https://discord.gg/ae87a8ygbN)
