# Terminal Fincept

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![ImGui](https://img.shields.io/badge/ImGui-Docking-blue)](https://github.com/ocornut/imgui)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **Votre réflexion est la seule limite. Les données ne le sont pas.**

Plateforme de renseignement financier de pointe avec analyses de niveau CFA, automatisation de l'IA et connectivité de données illimitée.

[📥 Télécharger](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 Documents](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 Discorde](https://discord.gg/ae87a8ygbN)·[🤝 Partenaire](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png)

</div>

* * *

## À propos

**Fincept Terminal vch**est une application de bureau C++20 purement native – une réécriture complète de la précédente pile Tauri/React/Rust. Il utilise**Cher ImGui**pour l'interface utilisateur accélérée par GPU,**GLFW + OpenGL**pour le rendu, intégré**Python**pour l'analyse et offre des performances de classe terminal Bloomberg dans un seul binaire natif.

* * *

## Pile technologique

| Couche                    | Technologies                               |
| ------------------------- | ------------------------------------------ |
| **Langue**                | C++20 (MSVC/GCC/Clang)                     |
| **Interface utilisateur** | Cher ImGui (branche d'accueil) + ImPlot    |
| **Mise en page**          | Yoga (moteur Flexbox)                      |
| **Rendu**                 | GLFW 3 + OpenGL 3.3+                       |
| **Réseautage**            | libcurl + OpenSSL                          |
| **Base de données**       | SQLite 3                                   |
| **Sérialisation**         | nlohmann/json                              |
| **Enregistrement**        | journal spd                                |
| **Audio**                 | mini-audio                                 |
| **Vidéo**                 | libmpv (facultatif)                        |
| **Analytique**            | Python intégré 3.11+ (plus de 100 scripts) |
| **Construire**            | CMake 3.20+ / vcpkg                        |

* * *

## Caractéristiques

| **Fonctionnalité**                       | **Description**                                                                                                                  |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| 📊**Analyses de niveau CFA**             | Modèles DCF, optimisation de portefeuille, mesures de risque (VaR, Sharpe), tarification des produits dérivés via Python intégré |
| 🤖**AI Agents**                          | Plus de 20 personnalités d'investisseurs (Buffett, Dalio, Graham), stratégies de hedge funds, support LLM local                  |
| 🌐**Plus de 100 connecteurs de données** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, FMI, Banque mondiale, AkShare, API gouvernementales                              |
| 📈**Trading en temps réel**              | Crypto (Kraken/HyperLiquid WebSocket), actions, trading algo, moteur de trading papier                                           |
| 🔬**QuantLib Suite**                     | 18 modules d'analyse quantitative — tarification, risque, stochastique, volatilité, titres à revenu fixe                         |
| 🚢**Renseignement mondial**              | Suivi maritime, analyse géopolitique, cartographie des relations, données satellite                                              |
| 🎨**Flux de travail visuels**            | Éditeur de nœuds pour les pipelines d'automatisation, intégration de l'outil MCP                                                 |
| 🧠**AI Quant Lab**                       | Modèles ML, découverte de facteurs, HFT, trading d'apprentissage par renforcement                                                |

* * *

## 40+ écrans

| Catégorie        | Écrans                                                                                                                                                                                               |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Cœur**         | Tableau de bord, marchés, actualités, liste de surveillance                                                                                                                                          |
| **Commerce**     | Trading de crypto, trading d'actions, trading d'algorithmes, backtesting, visualisation commerciale                                                                                                  |
| **Recherche**    | Recherche sur les actions, filtre, portefeuille, analyses de surface, analyses de fusions et acquisitions, produits dérivés, investissements alternatifs                                             |
| **QuantLib**     | Noyau, Analyse, Courbes, Économie, Instruments, ML, Modèles, Numérique, Physique, Portefeuille, Tarification, Réglementation, Risque, Planification, Solveur, Statistiques, Stochastique, Volatilité |
| **AI/ML**        | AI Quant Lab, Agent Studio, AI Chat, Alpha Arena                                                                                                                                                     |
| **Économie**     | Économie, DBnomics, AkShare, marchés asiatiques                                                                                                                                                      |
| **Géopolitique** | Géopolitique, données gouvernementales, carte des relations, maritime, polymarché                                                                                                                    |
| **Outils**       | Éditeur de code, éditeur de nœuds, Excel, générateur de rapports, notes, sources de données, mappage de données, serveurs MCP                                                                        |
| **Communauté**   | Forum, Profil, Paramètres, Assistance, Docs, À propos                                                                                                                                                |

* * *

## Construire à partir de la source

### Conditions préalables

-   **CMake**3.20+
-   **vcpkg** (for dependency management)
-   **Compilateur C++20**(MSVC 2022, GCC 12+ ou Clang 15+)
-   **Python**3.11+ (pour les scripts d'analyse)

### Construire

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-cpp

# Windows (MSVC)
cmake --preset=default
cmake --build build --config Release

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Courir

```bash
./build/FinceptTerminal          # Linux / macOS
.\build\Release\FinceptTerminal  # Windows
```

### Dépendances vcpkg

glfw3, curl, nlohmann-json, sqlite3, openssl, imgui (docking + freetype), yoga, stb, implot, spdlog, miniaudio

* * *

## Ce qui nous distingue

**Terminal Fincept**est une plateforme financière open source conçue pour ceux qui refusent d'être limités par les logiciels traditionnels. Nous sommes en compétition sur**profondeur d'analyse**et**accessibilité des données**– pas sur les informations privilégiées ou les flux exclusifs.

-   **Performances natives**— C++ avec ImGui accéléré par GPU, pas de surcharge Electron/Web
-   **Binaire simple**- pas de Node.js, pas d'exécution de navigateur, pas de bundle JavaScript
-   **Analyses de niveau CFA**— couverture complète du programme via les modules Python
-   **Plus de 100 connecteurs de données**— de Yahoo Finance aux bases de données gouvernementales
-   **Gratuit et open source**(AGPL-3.0) avec licences commerciales disponibles

* * *

## Feuille de route

**T1 2026 :**Migration C++ terminée, streaming en temps réel amélioré, backtesting avancé**2026 :**Constructeur de stratégie d'options, gestion multi-portefeuilles, plus de 50 agents IA**Avenir:**Compagnon mobile, fonctionnalités institutionnelles, API programmatique, interface utilisateur de formation ML

* * *

## Contribuer

Nous construisons l’avenir de l’analyse financière – ensemble.

**Contribuer:**Nouveaux connecteurs de données, agents IA, modules d'analyse, écrans C++, documentation

-   [Guide de contribution](docs/CONTRIBUTING.md)
-   [Guide de contribution C++](fincept-cpp/CONTRIBUTING.md)
-   [Guide du contributeur Python](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [Signaler un bug](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [Fonctionnalité de demande](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## Pour les universités et les enseignants

**Apportez des analyses financières de qualité professionnelle à votre classe.**

-   **799 $/mois**pour 20 comptes
-   Accès complet aux données et API Fincept
-   Parfait pour les cours de finance, d'économie et de science des données
-   Analyse du programme CFA intégrée

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
