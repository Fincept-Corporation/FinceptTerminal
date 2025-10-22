# FinceptTerminal

A modern, cross-platform terminal application built with Tauri and React. Experience blazing-fast performance with a beautiful, intuitive interface.

<!-- DOWNLOAD_SECTION_START -->

## 📥 Download Latest Build

**Version:** `v3.0.0` | **Commit:** `7a750c4` | **Released:** 2025-10-22 11:47 UTC

### 🎯 Direct Downloads - Click to Download Immediately!

| Platform | Architecture | Direct Download |
|----------|-------------|----------------|
| 🍎 **macOS** | Apple Silicon (ARM64) | [⬇️ **Download .dmg**](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v3.0.0-build-41/FinceptTerminal-v3.0.0-macOS-arm64.dmg) |
| 🍎 **macOS** | Intel (x64) | [⬇️ **Download .dmg**](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v3.0.0-build-41/FinceptTerminal-v3.0.0-macOS-x64.dmg) |
| 🐧 **Linux** | x64 (AppImage) | [⬇️ **Download .AppImage**](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v3.0.0-build-41/FinceptTerminal-v3.0.0-Linux-x64.AppImage) |
| 🐧 **Linux** | x64 (Debian) | [⬇️ **Download .deb**](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v3.0.0-build-41/FinceptTerminal-v3.0.0-Linux-x64.deb) |
| 🪟 **Windows** | x64 | [⬇️ **Download .msi**](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v3.0.0-build-41/FinceptTerminal-v3.0.0-Windows-x64.msi) |

> ✨ **One-Click Downloads!** These links will start downloading immediately - no GitHub login required!

### 🛠️ Installation Instructions

#### 🍎 **macOS**
1. Click the download link above for your Mac type (Apple Silicon or Intel)
2. Open the downloaded `.dmg` file
3. Drag **FinceptTerminal** to your Applications folder
4. Launch from Applications or Spotlight search

#### 🐧 **Linux**

**AppImage (Recommended - works on all distributions):**
1. Download the `.AppImage` file
2. Make it executable: `chmod +x FinceptTerminal-*.AppImage`
3. Run it: `./FinceptTerminal-*.AppImage`

**Debian/Ubuntu (.deb package):**
1. Download the `.deb` file
2. Install: `sudo dpkg -i FinceptTerminal-*.deb`
3. Launch from applications menu or run `finceptterminal`

#### 🪟 **Windows**
1. Download the `.msi` installer
2. Run the installer and follow the setup wizard
3. Launch from Start Menu or Desktop shortcut

### 📊 Build Status & Info

![Build Status](https://github.com/Fincept-Corporation/FinceptTerminal/actions/workflows/build.yml/badge.svg)
![Latest Release](https://img.shields.io/github/v/release/Fincept-Corporation/FinceptTerminal?include_prereleases&label=Latest%20Build)
![Downloads](https://img.shields.io/github/downloads/Fincept-Corporation/FinceptTerminal/total?label=Total%20Downloads)

**Release Page:** [View all releases and changelogs](https://github.com/Fincept-Corporation/FinceptTerminal/releases)

---
<!-- DOWNLOAD_SECTION_END -->

## ✨ Features

- 🚀 **Lightning Fast**: Built with Rust backend for maximum performance
- 🎨 **Modern UI**: Clean, intuitive interface built with React
- 🔒 **Secure**: Tauri's security model ensures your data stays safe
- 🌍 **Cross-Platform**: Native performance on macOS, Linux, and Windows
- ⚡ **Low Resource Usage**: Minimal memory footprint and CPU usage
- 🎯 **Developer Friendly**: Extensive customization and plugin support
- 📱 **Responsive Design**: Adapts to any screen size seamlessly
- 🌙 **Theme Support**: Light/dark modes and custom themes

## 🚀 Quick Start

### System Requirements

- **macOS**: 10.15 (Catalina) or later
- **Linux**: Ubuntu 20.04+ or equivalent
- **Windows**: Windows 10 version 1903+ or Windows 11

### Installation

1. Download the appropriate installer for your platform from the links above
2. Follow the installation instructions for your OS
3. Launch FinceptTerminal and start coding!

## 🛠️ Development

### Prerequisites

Before you begin, ensure you have the following installed:

- **Node.js** (v18 or later) - [Download here](https://nodejs.org/)
- **Rust** (latest stable) - [Install via rustup](https://rustup.rs/)
- **Git** - [Download here](https://git-scm.com/)

#### Platform-specific dependencies:

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install -y \
  libwebkit2gtk-4.1-dev \
  libwebkit2gtk-4.0-dev \
  libjavascriptcoregtk-4.1-dev \
  libjavascriptcoregtk-4.0-dev \
  libsoup-3.0-dev \
  libsoup2.4-dev \
  build-essential \
  curl \
  wget \
  file \
  libssl-dev \
  libgtk-3-dev \
  libayatana-appindicator3-dev \
  librsvg2-dev \
  patchelf
```

**macOS:**
```bash
# Install Xcode Command Line Tools
xcode-select --install
```

**Windows:**
- Install [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
- Install [WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (usually pre-installed)

### Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
   cd FinceptTerminal
   ```

2. **Install dependencies**
   ```bash
   npm install
   ```

3. **Start development server**
   ```bash
   npm run tauri dev
   ```

### Building

**Development build:**
```bash
npm run tauri build
```

**Production build:**
```bash
npm run tauri build -- --release
```

**Build for specific target:**
```bash
npm run tauri build -- --target x86_64-unknown-linux-gnu
```

### Project Structure

```
FinceptTerminal/
├── src/                    # React frontend source
│   ├── components/         # React components
│   ├── hooks/             # Custom React hooks
│   ├── styles/            # CSS/styling files
│   └── utils/             # Utility functions
├── src-tauri/             # Rust backend source
│   ├── src/               # Rust source code
│   ├── icons/             # App icons
│   └── Cargo.toml         # Rust dependencies
├── public/                # Static assets
├── .github/               # GitHub Actions workflows
└── package.json           # Node.js dependencies
```

## 📖 Documentation

- [User Guide](docs/user-guide.md) - Learn how to use FinceptTerminal
- [API Reference](docs/api.md) - Technical documentation
- [Customization Guide](docs/customization.md) - Themes and plugins
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions

## 🤝 Contributing

We welcome contributions from the community! Here's how you can help:

### Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Make your changes
4. Run tests: `npm test`
5. Commit your changes: `git commit -m 'Add amazing feature'`
6. Push to branch: `git push origin feature/amazing-feature`
7. Open a Pull Request

### Code Style

- **Frontend**: We use ESLint and Prettier for JavaScript/TypeScript
- **Backend**: We follow standard Rust formatting with `rustfmt`
- **Commits**: Use [Conventional Commits](https://conventionalcommits.org/) format

### Running Tests

```bash
# Frontend tests
npm test

# Backend tests
cd src-tauri && cargo test

# Integration tests
npm run test:e2e
```

## 🐛 Bug Reports

Found a bug? Please help us improve by reporting it:

1. Check if the issue already exists in our [Issues](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
2. If not, create a new issue with:
   - Clear description of the problem
   - Steps to reproduce
   - Expected vs actual behavior
   - Your system information (OS, version, etc.)
   - Screenshots if applicable

## 💡 Feature Requests

Have an idea for a new feature? We'd love to hear it:

1. Check existing [Feature Requests](https://github.com/Fincept-Corporation/FinceptTerminal/issues?q=is%3Aissue+is%3Aopen+label%3A%22feature+request%22)
2. Open a new issue with the `feature request` label
3. Describe your idea in detail and explain why it would be useful

## 📜 Changelog

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.

## 🙏 Acknowledgments

- [Tauri](https://tauri.app/) - The incredible framework that makes this possible
- [React](https://reactjs.org/) - For the amazing frontend framework
- [Rust](https://www.rust-lang.org/) - For the blazing-fast backend
- Our amazing [contributors](https://github.com/Fincept-Corporation/FinceptTerminal/graphs/contributors)

## 📞 Support

- 📧 Email: support@fincept.in
- 💬 Discord: [Join our community](https://discord.gg/fincept)
- 🐦 Twitter: [@FinceptCorp](https://twitter.com/FinceptCorp)
- 📝 Documentation: [docs.fincept.in](https://docs.fincept.in)

## 🏢 About Fincept Corporation

FinceptTerminal is developed by [Fincept Corporation](https://fincept.in), a company dedicated to building innovative developer tools and financial technology solutions.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

<div align="center">
  <p>
    <strong>Made with ❤️ by Fincept Corporation</strong>
  </p>
  <p>
    <a href="https://github.com/Fincept-Corporation/FinceptTerminal/stargazers">⭐ Star us on GitHub</a> •
    <a href="https://github.com/Fincept-Corporation/FinceptTerminal/issues">🐛 Report Bug</a> •
    <a href="https://github.com/Fincept-Corporation/FinceptTerminal/issues">💡 Request Feature</a>
  </p>
</div>
