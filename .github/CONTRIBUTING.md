# Contribute to Fincept Terminal

Want to make Fincept Terminal even better? We'd love your help!

## Where We Need You

**C++ Development**
   - Build new screens and features using Qt6 Widgets
   - Improve core infrastructure, performance, and stability

**Data Source Integration**
   - Connect government financial databases (e.g., data.gov.in, data.gov)
   - Write Python data fetcher scripts for new APIs

**Analytics & AI**
   - Build Python analytics modules (portfolio, derivatives, risk)
   - Implement new AI agent frameworks

**Brokerage API Integration**
   - Connect stock brokers so users can trade directly from the terminal
   - Work on country-specific broker adapters in C++

**Documentation**
   - Create and maintain comprehensive documentation
   - Cover build setup, architecture, and developer guides

---

## How to Get Started

1. **Fork** this repo & clone it:
   ```bash
   git clone https://github.com/your-username/FinceptTerminal.git
   ```

2. **Build** the project:
   ```bash
   cd FinceptTerminal/fincept-qt

   # Linux / macOS
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel

   # Windows (Developer Command Prompt for VS 2022)
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
   cmake --build build --config Release --parallel
   ```

3. **Create a new branch** for your feature:
   ```bash
   git checkout -b feature/awesome-feature
   ```

4. **Code**, follow the [C++ Contributing Guide](../fincept-qt/CONTRIBUTING.md) or [Python Guide](../docs/PYTHON_CONTRIBUTOR_GUIDE.md), and document your changes.

5. **Push your changes** and open a **pull request**:
   ```bash
   git push origin feature/awesome-feature
   ```

---

## Guidelines

- **Bug Reports:** [File an issue](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new)
- **Feature Requests:** Open a [GitHub Discussion](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)
- **Code Contributions:** Keep it clean & simple. Well-documented code is appreciated.

**Join the movement!** Let's build the future of finance together. Questions? Ping us at [support@fincept.in](mailto:support@fincept.in).
