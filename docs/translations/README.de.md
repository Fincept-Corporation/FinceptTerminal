> [!IMPORTANT]
> **Zwei Editionen.** **[Enterprise](https://fincept.in/enterprise)** ist der private, quelloffen nicht verfügbare Build für Fonds und Research-Desks — 41 Module, private Daten, Live-Broker-Routing, SSO, ab **99 $/Nutzer/Monat**. **Dieses Repo** ist die kostenlose AGPL-3.0-Edition für Lernzwecke und akademische Nutzung, mit einem Release pro Monat.
> [Vergleich](https://fincept.in/comparison) · [Preise](https://fincept.in/pricing)

# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **Ihr Denken ist die einzige Grenze. Die Daten sind es nicht.**

Hochmoderne Financial-Intelligence-Plattform mit Finanzanalysen auf institutionellem Niveau, KI-Automatisierung und unbegrenzter Datenkonnektivität.

[📥 Download](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 Preise](https://fincept.in/pricing) · [📖 Handbuch](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

---

## Über

**Fincept Terminal** ist ein natives C++20-Desktop-Terminal für Finanzresearch — Qt6-Oberfläche, eingebettete Python-3.11-Analytik, eine einzige Binärdatei, kein Electron.

Zwei Editionen laufen auf einem gemeinsamen Datenkern. **[Enterprise](https://fincept.in/enterprise)** ist der private, quelloffen nicht verfügbare Build, an dem das Team täglich arbeitet — für Fonds, Family Offices und Research-Desks. **Dieses Repo** ist die kostenlose AGPL-3.0-Edition für Lernzwecke, private Nutzung und akademische Forschung, mit einem Release pro Monat.

Nutzen Sie den offenen Build, wenn Sie Studierende, Hobbyist oder akademisch tätig sind. Nutzen Sie Enterprise, wenn Sie ein Unternehmen sind oder mit dem Terminal Geld verdienen: Das AGPL-Copyleft entfällt, und die tatsächlichen Kosten des offenen Builds sind Ihre eigenen Daten- und LLM-Rechnungen, abgerechnet pro Token ohne Obergrenze.

| | Open Source | **Enterprise** |
|---|---|---|
| **Lizenz** | AGPL-3.0 — starkes Copyleft | Proprietär — keine Copyleft-Pflichten |
| **Kosten** | Kostenlos, plus eigene Daten- und LLM-Rechnungen | 99 $ / 199 $ / 299 $ pro Nutzer/Monat |
| **Daten** | Kostenlose öffentliche Feeds, eigene API-Schlüssel | Private Datensätze, längere Historie, Point-in-Time |
| **KI** | Eigener LLM-Schlüssel | 400–5.000 Credits inklusive · Multi-Agenten-Research · privater Datenraum |
| **Handel** | Papierhandel + Broker-Anbindungen | Live-Broker-Routing + Live-Algo-Deployment |
| **Kontrollen** | — | SSO/SAML, Audit-Logs, RBAC, SLA-gestützter Support |

[**Enterprise ansehen →**](https://fincept.in/enterprise) · [Vollständiger Vergleich](https://fincept.in/comparison) · [Preise](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Enterprise

41 Module über sechs Desks — Agentic Research, Quant Lab und Backtesting, Deep Fundamental Research, Markets and Execution, Macro and Global Intelligence sowie Ihr eigener Workspace. Alles dokumentiert in einem [700-seitigen Handbuch](https://fincept.in/manual).

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **99 $**/Nutzer/Monat | **199 $**/Nutzer/Monat | **299 $**/Nutzer/Monat |
| KI-Credits / Monat | 400 | 2.000 | 5.000 |
| Deep Research + Agenten-Teams | — | ✓ | ✓ |
| Live-Broker-Handel + Algo | — | — | ✓ |

Monatliche Abrechnung, keine Bindung, kein Platz-Minimum, 15 % Rabatt bei quartalsweiser Zahlung — **1.188–3.588 $ pro Nutzer und Jahr**, gegenüber rund 27.000 $ für einen Bloomberg-Platz. **Hochschulen:** 5 Exclusive-Pro-Plätze für **699 $/Monat**. Diese Pläne sind die vollständige Preisliste: keine Verhandlungspreise, keine separate kommerzielle Lizenz.

Enterprise benötigt ein eigenes Konto — kostenlose Fincept-Logins funktionieren dort nicht.

[**Konto anlegen**](https://fincept.in/enterprise/signup) · [**Demo buchen**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## Installation

Fertige Installationsprogramme für **Windows x64**, **Linux x64** (`.run` / `.deb` / `.rpm`) und **macOS (Apple Silicon)** finden Sie auf der [Releases-Seite](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest).

**Aus dem Quellcode bauen** — Linux/macOS: `git clone … && ./setup.sh`. Windows, manuelle Builds, die fixierte Toolchain (**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**) und Fehlerbehebung stehen in **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**. Die Versionen sind fixiert — neuere oder ältere werden nicht unterstützt.

> Sie suchen den Enterprise-Build? Er hat eigene signierte Installer für Windows, macOS und Linux hinter einem Enterprise-Login — [hier erhältlich](https://fincept.in/enterprise).

---

## Was im offenen Build steckt

- **Analytik** — DCF, Portfoliooptimierung, VaR/Sharpe, Derivatebewertung, Anleihen, Alternatives, dazu eine QuantLib-Suite mit 18 Modulen
- **KI** — 37 Agenten aus Trader/Investor, Ökonomie und Geopolitik; eigener Schlüssel nötig (OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama)
- **Daten** — über 100 Konnektoren: FRED, IWF, Weltbank, DBnomics, AkShare, Polygon, Kraken, Yahoo Finance, Behörden-APIs
- **Handel** — Krypto- und Aktien-Feeds, Papierhandels-Engine, 16 Broker-Anbindungen
- **Automatisierung** — visueller Node-Editor, MCP-Tools, AI Quant Lab (ML, Faktorforschung, RL)
- **Globale Aufklärung** — maritimes Tracking, geopolitische Analyse, Beziehungskartierung

Natives C++20 · Qt6 · eingebettetes Python 3.11 · eine Binärdatei · kein Node.js, keine Browser-Runtime.

---

## Wie dieses Repo gepflegt wird

Dieses Repository **bleibt öffentlich und wird nicht gelöscht**. Alles bereits Veröffentlichte bleibt veröffentlicht.

Es erscheint jetzt **ein Release pro Monat** statt fortlaufender Entwicklung, weil das Team täglich an Enterprise arbeitet. Issues und Pull Requests werden weiterhin geprüft, Fixes kommen im Monatstakt. Sicherheitsmeldungen an [support@fincept.in](mailto:support@fincept.in).

---

## Mitwirken

Neue Datenkonnektoren, KI-Agenten, Analysemodule, C++-Screens und Dokumentation sind alle willkommen.

[Contributing-Leitfaden](../CONTRIBUTING.md) · [C++-Leitfaden](../CPP_CONTRIBUTOR_GUIDE.md) · [Python-Leitfaden](../PYTHON_CONTRIBUTOR_GUIDE.md) · [Architektur](../ARCHITECTURE.md) · [Fehler melden](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [Feature vorschlagen](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## Ebenfalls von Fincept

- **[Fincept Data API](https://docs.fincept.in)** — über 500 REST-Endpunkte, mehr als 423.000 Instrumente, über 2.000 Quellen. Kostenlose Stufe zu jedem Konto.
- **[Quantcept](https://quantcept.io)** — quelloffenes, KI-gestütztes Finanz-Terminal für die Kommandozeile (Apache-2.0).

---

## Lizenz

**AGPL-3.0-or-later** — vollständiger Text in [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE).

Kostenlos für private Nutzung, Lernzwecke und akademische Forschung. AGPL-3.0 ist **starkes Copyleft, nicht permissiv**: Wer einen modifizierten Build verbreitet oder als Dienst betreibt, den andere erreichen, muss seine Änderungen unter derselben Lizenz veröffentlichen. Für die meisten Rechtsabteilungen endet die Diskussion genau hier — und deshalb nehmen Firmen **[Enterprise](https://fincept.in/enterprise)**, das proprietär ist und keine Copyleft-Pflichten mit sich bringt. Private, nicht verbreitete Nutzung bringt keinerlei Pflichten mit sich.

Fincept verkauft für dieses Repository keine separate kommerzielle oder akademische Lizenz mehr. Kommerzieller, unternehmerischer und universitärer Bedarf wird über **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** zu den oben genannten Preisen abgedeckt.

**Marken.** „Fincept", „Fincept Terminal" und das Fincept-Logo sind Marken der Fincept Corporation. Die Verwendung in geforkten, abgeleiteten, umbenannten oder kommerziellen Produkten bedarf der vorherigen schriftlichen Genehmigung.

Fragen: [support@fincept.in](mailto:support@fincept.in) · [AGB](https://fincept.in/terms) · [Datenschutz](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. Alle Rechte vorbehalten.

---

<div align="center">

### **Ihr Denken ist die einzige Grenze. Die Daten sind es nicht.**

⭐ **Stern vergeben** · 🔄 **Teilen** · 🤝 **Mitwirken**

<sub>Englisches Original: <a href="../../README.md">README.md</a></sub>

</div>
