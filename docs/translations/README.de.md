# Fincept-Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **Ihr Denken ist die einzige Grenze. Die Daten sind es nicht.**

Hochmoderne Financial-Intelligence-Plattform mit Analysen auf CFA-Ebene, KI-Automatisierung und unbegrenzter Datenkonnektivität.

[📥 Herunterladen](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 Dokumente](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 Diskussionen](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 Zwietracht](https://discord.gg/ae87a8ygbN)·[🤝 Partner](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

* * *

## Um

**Fincept Terminal vch**ist eine rein native C++20-Desktopanwendung – eine komplette Neufassung des vorherigen Tauri/React/Rust-Stacks. Es nutzt**Qt6**für Benutzeroberfläche und Rendering, eingebettet**Python**für Analysen und liefert eine Leistung der Bloomberg-Terminal-Klasse in einer einzigen nativen Binärdatei.

* * *

## Merkmale

| **Besonderheit**               | **Beschreibung**                                                                                                                                                                                      |
| ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 📊**Analysen auf CFA-Ebene**   | DCF-Modelle, Portfoliooptimierung, Risikometriken (VaR, Sharpe), Derivatpreisgestaltung über eingebettetes Python                                                                                     |
| 🤖**KI-Agenten**               | Über 20 Investorenpersönlichkeiten (Buffett, Dalio, Graham), Hedgefonds-Strategien, lokale LLM-Unterstützung, Multi-Provider (OpenAI, Anthropic, Gemini, Groq, DeepSeek, MiniMax, OpenRouter, Ollama) |
| 🌐**Über 100 Datenanschlüsse** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, IWF, Weltbank, AkShare, Regierungs-APIs                                                                                                               |
| 📈**Echtzeithandel**           | Krypto (Kraken/HyperLiquid WebSocket), Aktien, Algo-Handel, Papierhandelsmaschine                                                                                                                     |
| 🔬**QuantLib Suite**           | 18 quantitative Analysemodule – Preisgestaltung, Risiko, Stochastik, Volatilität, festverzinsliche Wertpapiere                                                                                        |
| 🚢**Globale Intelligenz**      | Maritime Verfolgung, geopolitische Analyse, Beziehungskartierung, Satellitendaten                                                                                                                     |
| 🎨**Visuelle Arbeitsabläufe**  | Knoteneditor für Automatisierungspipelines, MCP-Tool-Integration                                                                                                                                      |
| 🧠**AI Quant Lab**             | ML-Modelle, Faktorerkennung, HFT, Reinforcement Learning Trading                                                                                                                                      |

* * *

## Installation

### Option 1 – Vorgefertigte Binärdatei herunterladen (empfohlen)

Vorgefertigte Binärdateien sind auf der verfügbar[Seite „Veröffentlichungen“.](https://github.com/Fincept-Corporation/FinceptTerminal/releases). Keine Build-Tools erforderlich – einfach extrahieren und ausführen.

| Plattform                 | Herunterladen                            | Laufen                                               |
| ------------------------- | ---------------------------------------- | ---------------------------------------------------- |
| **Windows x64**           | `FinceptTerminal-Windows-x64.zip`        | Extrahieren →`FinceptTerminal.exe`                   |
| **Windows ARM64**         | `FinceptTerminal-Windows-arm64.zip`      | Extrahieren →`FinceptTerminal.exe`                   |
| **Linux x64**             | `FinceptTerminal-Linux-x86_64.AppImage`  | `chmod +x`→`./FinceptTerminal-Linux-x86_64.AppImage` |
| **macOS (Apple Silicon)** | `FinceptTerminal-macOS-arm64.tar.gz`     | Extrahieren →`./FinceptTerminal`                     |
| **macOS (Intel)**         | `FinceptTerminal-macOS-x64.tar.gz`       | Extrahieren →`./FinceptTerminal`                     |
| **macOS (universell)**    | `FinceptTerminal-macOS-universal.tar.gz` | Extrahieren →`./FinceptTerminal`                     |

* * *

### Option 2 – Schnellstart (One-Click Build)

Klonen Sie das Setup-Skript und führen Sie es aus – es installiert alle Abhängigkeiten und erstellt die App automatisch:

```bash
# Linux / macOS
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
chmod +x setup.sh && ./setup.sh
```

```bat
# Windows — run from Developer Command Prompt for VS 2022
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
setup.bat
```

Das Skript übernimmt Folgendes: Compilerprüfung, CMake, Qt6, Python, Build und Start.

* * *

### Option 3 – Docker

```bash
# Pull and run
docker pull ghcr.io/fincept-corporation/fincept-terminal:latest
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
    ghcr.io/fincept-corporation/fincept-terminal:latest

# Or build from source
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
docker build -t fincept-terminal .
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix fincept-terminal
```

> **Notiz:**Docker ist in erster Linie für Linux gedacht. macOS und Windows erfordern eine zusätzliche XServer-Konfiguration.

* * *

### Option 4 – Aus dem Quellcode erstellen (manuell)

#### Voraussetzungen

| Werkzeug         | Version | Windows                                                          | Linux                 | macOS                              |
| ---------------- | ------- | ---------------------------------------------------------------- | --------------------- | ---------------------------------- |
| **Git**          | letzte  | `winget install Git.Git`                                         | `apt install git`     | `brew install git`                 |
| **CMake**        | 3.20+   | `winget install Kitware.CMake`                                   | `apt install cmake`   | `brew install cmake`               |
| **C++-Compiler** | C++20   | MSVC 2022 ([Visual Studio](https://visualstudio.microsoft.com/)) | `apt install g++`     | Xcode-CLT:`xcode-select --install` |
| **Qt6**          | 6.5+    | Siehe unten                                                      | Siehe unten           | Siehe unten                        |
| **Python**       | 3.11+   | [python.org](https://www.python.org/downloads/)                  | `apt install python3` | `brew install python`              |

#### Installieren Sie Qt6

**Windows:**

```powershell
# Via Qt online installer (recommended — includes windeployqt)
# Download from https://www.qt.io/download-qt-installer
# Select: Qt 6.x > MSVC 2022 64-bit

# Or via winget
winget install Qt.QtCreator
```

**Linux (Ubuntu/Debian):**

```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

**macOS:**

```bash
brew install qt
```

#### Bauen

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-qt

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (from Developer Command Prompt for VS 2022)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release --parallel
```

#### Laufen

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

* * *

## Was uns auszeichnet

**Fincept-Terminal**ist eine Open-Source-Finanzplattform, die für diejenigen entwickelt wurde, die sich nicht durch traditionelle Software einschränken lassen. Wir konkurrieren weiter**Analysetiefe**Und**Datenzugänglichkeit**– nicht auf Insiderinformationen oder exklusiven Feeds.

-   **Native Leistung**– C++20 mit Qt6, kein Electron/Web-Overhead
-   **Einzelne Binärdatei**– kein Node.js, keine Browser-Laufzeitumgebung, kein JavaScript-Bundler
-   **Analysen auf CFA-Ebene**— Vollständige Lehrplanabdeckung über Python-Module
-   **Über 100 Datenanschlüsse**– von Yahoo Finance bis hin zu Regierungsdatenbanken
-   **Kostenlos und Open Source**(AGPL-3.0) mit kommerziellen Lizenzen verfügbar

* * *

## Roadmap

| Zeitleiste          | Meilenstein                                                                         |
| ------------------- | ----------------------------------------------------------------------------------- |
| **1. Quartal 2026** | Echtzeit-Streaming, erweitertes Backtesting, Broker-Integrationen                   |
| **Erbrechen 2026**  | Optionsstrategie-Builder, Multi-Portfolio-Management, über 50 KI-Agenten            |
| **Kz 2026**         | Programmatische API, Benutzeroberfläche für ML-Training, institutionelle Funktionen |
| **Zukunft**         | Mobiler Begleiter, Cloud-Synchronisierung, Community-Marktplatz                     |

* * *

## Mitwirken

Wir gestalten die Zukunft der Finanzanalyse – gemeinsam.

**Beitragen:**Neue Datenkonnektoren, KI-Agenten, Analysemodule, C++-Bildschirme, Dokumentation

-   [Mitwirkender Leitfaden](docs/CONTRIBUTING.md)
-   [Leitfaden für C++-Beiträge](fincept-qt/CONTRIBUTING.md)
-   [Leitfaden für Python-Mitwirkende](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [Fehler melden](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [Anforderungsfunktion](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## Für Universitäten und Pädagogen

**Bringen Sie professionelle Finanzanalysen in Ihren Unterricht.**

-   **799 $/Monat**für 20 Konten
-   Voller Zugriff auf Fincept-Daten und APIs
-   Perfekt für Finanz-, Wirtschafts- und Datenwissenschaftskurse
-   Integrierte CFA-Lehrplananalyse

**Interessiert?**E-Mail**[support@fincept.in](mailto:support@fincept.in)**mit dem Namen Ihrer Institution.

[Details zur Universitätslizenzierung](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## Lizenz

**Doppellizenz: AGPL-3.0 (Open Source) + kommerziell**

### Open Source (AGPL-3.0)

-   Kostenlos für den persönlichen, pädagogischen und nichtkommerziellen Gebrauch
-   Erfordert Freigabeänderungen, wenn es verteilt oder als Netzwerkdienst verwendet wird
-   Volle Transparenz des Quellcodes

### Kommerzielle Lizenz

-   Erforderlich für die geschäftliche Nutzung oder den kommerziellen Zugriff auf Fincept-Daten/APIs
-   Kontakt:**[support@fincept.in](mailto:support@fincept.in)**
-   Einzelheiten:[Leitfaden für kommerzielle Lizenzen](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### Marken

„Fincept Terminal“ und „Fincept“ sind Marken der Fincept Corporation.

© 2025-2026 Fincept Corporation. Alle Rechte vorbehalten.

* * *

<div align="center">

### **Ihr Denken ist die einzige Grenze. Die Daten sind es nicht.**

<div align="center">
<a href="https://star-history.com/#Fincept-Corporation/FinceptTerminal&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
 </picture>
</a>
</div>

[![Email](https://img.shields.io/badge/Email-support@fincept.in-blue)](mailto:support@fincept.in)

⭐**Stern**· 🔄**Aktie**· 🤝**Beitragen**

</div>
