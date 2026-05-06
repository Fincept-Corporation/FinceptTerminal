# Terminal Fincept

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **Votre réflexion est la seule limite. Les données ne le sont pas.**

Plateforme de renseignement financier de pointe avec analyses financières de niveau institutionnel, automatisation de l'IA et connectivité de données illimitée.

[📥 Télécharger](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 Documents](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 Discorde](https://discord.gg/ae87a8ygbN)·[🤝 Partenaire](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

* * *

## À propos

**Fincept Terminal vch**est une application de bureau C++20 purement native. Il utilise**Qt6**pour l'interface utilisateur et le rendu, intégré**Python**pour l'analyse et offre des performances de classe terminal professionnel dans un seul binaire natif.

* * *

## Caractéristiques

| **Fonctionnalité**                       | **Description**                                                                                                                                                                                                                                    |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 📊**Analyses multi-actifs**              | Modèles DCF, optimisation de portefeuille, mesures de risque (VaR, Sharpe), tarification des produits dérivés sur actions, taux, dérivés, portefeuille et alternatifs via Python intégré                                                          |
| 🤖**AI Agents**                          | Plus de 20 personnalités d'investisseurs (Buffett, Dalio, Graham), stratégies de hedge funds, support LLM local, multi-fournisseurs (OpenAI, Anthropic, Gemini, Groq, DeepSeek, MiniMax, OpenRouter, Ollama)                                       |
| 🌐**Plus de 100 connecteurs de données** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, FMI, Banque mondiale, AkShare, API gouvernementales, ainsi que des superpositions de données alternatives facultatives telles que le sentiment du marché Adanos pour la recherche sur les actions. |
| 📈**Trading en temps réel**              | Crypto (Kraken/HyperLiquid WebSocket), actions, trading algo, moteur de trading papier                                                                                                                                                             |
| 🔬**QuantLib Suite**                     | 18 modules d'analyse quantitative — tarification, risque, stochastique, volatilité, titres à revenu fixe                                                                                                                                           |
| 🚢**Renseignement mondial**              | Suivi maritime, analyse géopolitique, cartographie des relations, données satellite                                                                                                                                                                |
| 🎨**Flux de travail visuels**            | Éditeur de nœuds pour les pipelines d'automatisation, intégration de l'outil MCP                                                                                                                                                                   |
| 🧠**AI Quant Lab**                       | Modèles ML, découverte de facteurs, HFT, trading d'apprentissage par renforcement                                                                                                                                                                  |

* * *

## Installation

### Option 1 — Télécharger le binaire prédéfini (recommandé)

Des binaires prédéfinis sont disponibles sur le[Page des versions](https://github.com/Fincept-Corporation/FinceptTerminal/releases). Aucun outil de construction requis : il suffit d'extraire et d'exécuter.

| Plate-forme                | Télécharger                              | Courir                                               |
| -------------------------- | ---------------------------------------- | ---------------------------------------------------- |
| **Windows x64**            | `FinceptTerminal-Windows-x64.zip`        | Extraire →`FinceptTerminal.exe`                      |
| **WindowsARM64**           | `FinceptTerminal-Windows-arm64.zip`      | Extraire →`FinceptTerminal.exe`                      |
| **Linuxx64**               | `FinceptTerminal-Linux-x86_64.AppImage`  | `chmod +x`→`./FinceptTerminal-Linux-x86_64.AppImage` |
| **macOS (Apple Silicium)** | `FinceptTerminal-macOS-arm64.tar.gz`     | Extraire →`./FinceptTerminal`                        |
| **macOS (Intel)**          | `FinceptTerminal-macOS-x64.tar.gz`       | Extraire →`./FinceptTerminal`                        |
| **macOS (universel)**      | `FinceptTerminal-macOS-universal.tar.gz` | Extraire →`./FinceptTerminal`                        |

* * *

### Option 2 — Démarrage rapide (construction en un clic)

Clonez et exécutez le script d'installation : il installe toutes les dépendances et crée automatiquement l'application :

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

Le script gère : la vérification du compilateur, CMake, Qt6, Python, la construction et le lancement.

* * *

### Option 3 — Docker

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

> **Note:**Docker est principalement destiné à Linux. macOS et Windows nécessitent une configuration XServer supplémentaire.

* * *

### Option 4 — Construire à partir de la source (manuel)

> **Les versions sont épinglées** (Qt 6.7.2, CMake 3.27.7, MSVC 19.38 / GCC 12.3 / Apple Clang 15.0, Python 3.11.9). Pour éviter la dérive de traduction, suivez les instructions officielles en anglais :
>
> 👉 **[README.md (English) — Build from Source](../../README.md#option-4--build-from-source-manual)**
>
> Démarrage rapide avec les presets CMake :
> ```bash
> ./setup.sh                                            # Linux / macOS — installation automatisée
> setup.bat                                             # Windows (VS 2022 Developer Cmd)
>
> # Ou manuellement :
> cd FinceptTerminal/fincept-qt
> cmake --preset linux-release   && cmake --build --preset linux-release
> cmake --preset macos-release   && cmake --build --preset macos-release
> cmake --preset win-release     && cmake --build --preset win-release
> ```

<details>
<summary>Instructions originales (obsolètes — conservées pour référence)</summary>

#### Conditions préalables

| Outil               | Version | Fenêtres                                                        | Linux                 | macOS                              |
| ------------------- | ------- | --------------------------------------------------------------- | --------------------- | ---------------------------------- |
| **Git**             | dernier | `winget install Git.Git`                                        | `apt install git`     | `brew install git`                 |
| **CMake**           | 3.20+   | `winget install Kitware.CMake`                                  | `apt install cmake`   | `brew install cmake`               |
| **Compilateur C++** | C++20   | MSVC2022 ([Studio visuel](https://visualstudio.microsoft.com/)) | `apt install g++`     | XcodeCLT :`xcode-select --install` |
| **Qt6**             | 6.5+    | Voir ci-dessous                                                 | Voir ci-dessous       | Voir ci-dessous                    |
| **Python**          | 3.11+   | [python.org](https://www.python.org/downloads/)                 | `apt install python3` | `brew install python`              |

#### Installer Qt6

**Fenêtres :**

```powershell
# Via Qt online installer (recommended — includes windeployqt)
# Download from https://www.qt.io/download-qt-installer
# Select: Qt 6.x > MSVC 2022 64-bit

# Or via winget
winget install Qt.QtCreator
```

**Linux (Ubuntu/Debian) :**

```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

**macOS :**

```bash
brew install qt
```

#### Construire

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

#### Courir

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

</details>

* * *

## Ce qui nous distingue

**Terminal Fincept**est une plateforme financière open source conçue pour ceux qui refusent d'être limités par les logiciels traditionnels. Nous sommes en compétition sur**profondeur d'analyse**et**accessibilité des données**– pas sur les informations privilégiées ou les flux exclusifs.

Les versions récentes prennent également en charge les options facultatives**Adanos Market Sentiment**connectivité dans**Sources de données → Données alternatives**. Une fois configuré, Equity Research peut générer des instantanés du sentiment des détaillants multi-sources sur Reddit, X, Finance News et Polymarket. Sans connexion Adanos active, la fonctionnalité reste inactive et le reste de l'application se comporte exactement comme avant.

-   **Performances natives**— C++20 avec Qt6, pas de surcharge Electron/Web
-   **Binaire simple**- pas de Node.js, pas d'exécution de navigateur, pas de bundle JavaScript
-   **Boîte à outils complète d'analyste buy-side**— actions, portefeuille, dérivés, taux, finance d'entreprise, alternatifs
-   **Plus de 100 connecteurs de données**— de Yahoo Finance aux bases de données gouvernementales
-   **Gratuit et open source**(AGPL-3.0) avec licences commerciales disponibles

* * *

## Feuille de route

| Chronologie | Jalon                                                                                        |
| ----------- | -------------------------------------------------------------------------------------------- |
| **T1 2026** | Streaming en temps réel, backtesting avancé, intégrations de courtiers                       |
| **Q2 2026** | Constructeur de stratégie d'options, gestion multi-portefeuilles, plus de 50 agents IA       |
| **KZ 2026** | API programmatique, interface utilisateur de formation ML, fonctionnalités institutionnelles |
| **Avenir**  | Compagnon mobile, synchronisation cloud, marché communautaire                                |

* * *

## Contribuer

Nous construisons l’avenir de l’analyse financière – ensemble.

**Contribuer:**Nouveaux connecteurs de données, agents IA, modules d'analyse, écrans C++, documentation

-   [Guide de contribution](docs/CONTRIBUTING.md)
-   [Guide de contribution C++](fincept-qt/CONTRIBUTING.md)
-   [Guide du contributeur Python](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [Signaler un bug](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [Fonctionnalité de demande](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## Pour les universités et les enseignants

**Apportez des analyses financières de qualité professionnelle à votre classe.**

-   **799 $/mois**pour 20 comptes
-   Accès complet aux données et API Fincept
-   Parfait pour les cours de finance, d'économie et de science des données
-   Analyses intégrées actions, portefeuille, dérivés, taux et économie

**Intéressé?**E-mail**[support@fincept.in](mailto:support@fincept.in)**avec le nom de votre établissement.

[Détails de la licence universitaire](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## Licence

**Double licence : AGPL-3.0 (Open Source) + Commerciale**

### Source ouverte (AGPL-3.0)

-   Gratuit pour un usage personnel, éducatif et non commercial
-   Nécessite des modifications de partage lorsqu'il est distribué ou utilisé comme service réseau
-   Transparence totale du code source

### Licence commerciale

-   Requis pour un usage professionnel ou pour accéder commercialement aux données/API Fincept
-   Contact:**[support@fincept.in](mailto:support@fincept.in)**
-   Détails:[Guide des licences commerciales](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### Marques déposées

« Fincept Terminal » et « Fincept » sont des marques commerciales de Fincept Corporation.

© 2025-2026 Fincept Corporation. Tous droits réservés.

* * *

<div align="center">

### **Votre réflexion est la seule limite. Les données ne le sont pas.**

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

⭐**Étoile**· 🔄**Partager**· 🤝**Contribuer**

</div>
