# Contributing to Fincept Terminal

Fincept Terminal is a native C++20/Qt6 desktop financial terminal. All contributions are welcome!

## Ways to Contribute

- **C++ / Qt6** — new screens, performance fixes, core infrastructure
- **Python Analytics** — data scripts, AI agents, quant modules
- **Data Sources** — connect APIs, brokers, government financial databases
- **Bug Reports & Feature Requests** — open an issue

## Getting Started

```bash
# Fork & clone
git clone https://github.com/your-username/FinceptTerminal.git
cd FinceptTerminal/fincept-qt

# Build — Linux/macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel

# Build — Windows (VS 2022 Developer Command Prompt)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release

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
