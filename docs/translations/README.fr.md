> [!IMPORTANT]
> **Deux éditions.** **[Enterprise](https://fincept.in/enterprise)** est la version privée, à code fermé, destinée aux fonds et desks de recherche — 41 modules, données privées, routage courtier en direct, SSO, à partir de **99 $/utilisateur/mois**. **Ce dépôt** est l'édition libre AGPL-3.0 pour l'apprentissage et l'usage académique, avec une version par mois.
> [Comparatif](https://fincept.in/comparison) · [Tarifs](https://fincept.in/pricing)

# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **Votre réflexion est la seule limite. Les données ne le sont pas.**

Plateforme d'intelligence financière de pointe : analyse de niveau institutionnel, automatisation par IA et connectivité de données sans limite.

[📥 Télécharger](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 Tarifs](https://fincept.in/pricing) · [📖 Manuel](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

---

## À propos

**Fincept Terminal** est un terminal de bureau natif en C++20 dédié à la recherche financière — interface Qt6, analyse Python 3.11 embarquée, un seul binaire, sans Electron.

Deux éditions reposent sur un socle de données commun. **[Enterprise](https://fincept.in/enterprise)** est la version privée, à code fermé, sur laquelle l'équipe travaille au quotidien, destinée aux fonds, family offices et desks de recherche. **Ce dépôt** est l'édition libre AGPL-3.0 — apprentissage, usage personnel, recherche académique — avec une version par mois.

Prenez l'édition ouverte si vous êtes étudiant, amateur ou universitaire. Prenez Enterprise si vous êtes une société, ou si le terminal est votre gagne-pain : le copyleft AGPL ne s'applique pas, et le vrai coût de l'édition ouverte, ce sont vos propres factures de données et de LLM, facturées au token et sans plafond.

| | Open source | **Enterprise** |
|---|---|---|
| **Licence** | AGPL-3.0 — copyleft fort | Propriétaire — aucune obligation de copyleft |
| **Coût** | Gratuit, plus vos factures de données et de LLM | 99 $ / 199 $ / 299 $ par utilisateur/mois |
| **Données** | Flux publics gratuits, vos propres clés | Jeux de données privés, historique plus profond, point-in-time |
| **IA** | Votre clé LLM | 400–5 000 crédits inclus · recherche multi-agents · data room privée |
| **Trading** | Papier + intégrations courtier | Routage courtier en direct + déploiement d'algos en direct |
| **Contrôles** | — | SSO/SAML, journaux d'audit, RBAC, support adossé à un SLA |

[**Découvrir Enterprise →**](https://fincept.in/enterprise) · [Comparatif complet](https://fincept.in/comparison) · [Tarifs](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Enterprise

41 modules répartis sur six desks — recherche agentique, quant lab et backtesting, analyse fondamentale approfondie, marchés et exécution, macro et intelligence globale, et votre propre espace de travail. Le tout dans un [manuel de 700 pages](https://fincept.in/manual).

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **99 $**/utilisateur/mois | **199 $**/utilisateur/mois | **299 $**/utilisateur/mois |
| Crédits IA / mois | 400 | 2 000 | 5 000 |
| Deep research + équipes d'agents | — | ✓ | ✓ |
| Trading en direct + algos | — | — | ✓ |

Facturation mensuelle, sans engagement, sans minimum de postes, 15 % de remise au trimestre — **1 188 à 3 588 $ par utilisateur et par an**, contre environ 27 000 $ pour un poste Bloomberg. **Universités :** 5 postes Exclusive Pro pour **699 $/mois**. Ces formules constituent toute la grille tarifaire : aucun tarif négocié, aucune licence commerciale distincte.

Enterprise exige son propre compte — les identifiants Fincept gratuits n'y donnent pas accès.

[**Créer un compte**](https://fincept.in/enterprise/signup) · [**Réserver une démo**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## Installation

Les installateurs pour **Windows x64**, **Linux x64** (`.run` / `.deb` / `.rpm`) et **macOS (Apple silicon)** se trouvent sur la [page des releases](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest).

**Compiler depuis les sources** — Linux/macOS : `git clone … && ./setup.sh`. Windows, compilation manuelle, chaîne d'outils figée (**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**) et dépannage : voir **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**. Les versions sont figées — les autres ne sont pas prises en charge.

> Vous cherchez la version Enterprise ? Elle dispose de ses propres installateurs signés pour Windows, macOS et Linux, derrière une authentification Enterprise — [à récupérer ici](https://fincept.in/enterprise).

---

## Ce que contient l'édition ouverte

- **Analyse** — DCF, optimisation de portefeuille, VaR/Sharpe, valorisation de dérivés, taux, actifs alternatifs, plus une suite QuantLib de 18 modules
- **IA** — 37 agents trader/investisseur, économie et géopolitique ; avec votre propre clé (OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama)
- **Données** — plus de 100 connecteurs : FRED, FMI, Banque mondiale, DBnomics, AkShare, Polygon, Kraken, Yahoo Finance, API publiques
- **Trading** — flux crypto et actions, moteur de paper trading, 16 intégrations courtier
- **Automatisation** — éditeur de nœuds visuel, outils MCP, AI Quant Lab (ML, découverte de facteurs, RL)
- **Renseignement global** — suivi maritime, analyse géopolitique, cartographie de relations

C++20 natif · Qt6 · Python 3.11 embarqué · un seul binaire · sans Node.js ni navigateur.

---

## Comment ce dépôt est maintenu

Ce dépôt **reste public et ne sera pas supprimé**. Tout ce qui a été publié le reste.

Il passe à **une version par mois** plutôt qu'un développement continu, l'équipe travaillant au quotidien sur Enterprise. Les issues et pull requests sont toujours examinées, et les correctifs arrivent au rythme mensuel. Signalements de sécurité : [support@fincept.in](mailto:support@fincept.in).

---

## Contribuer

Nouveaux connecteurs de données, agents IA, modules d'analyse, écrans C++ et documentation : tout est bienvenu.

[Guide de contribution](../CONTRIBUTING.md) · [Guide C++](../CPP_CONTRIBUTOR_GUIDE.md) · [Guide Python](../PYTHON_CONTRIBUTOR_GUIDE.md) · [Architecture](../ARCHITECTURE.md) · [Signaler un bug](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [Proposer une fonctionnalité](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## Également chez Fincept

- **[Fincept Data API](https://docs.fincept.in)** — plus de 500 endpoints REST, plus de 423 000 instruments, plus de 2 000 sources. Palier gratuit inclus avec tout compte.
- **[Quantcept](https://quantcept.io)** — terminal financier en ligne de commande, open source et propulsé par l'IA (Apache-2.0).

---

## Licence

**AGPL-3.0-or-later** — texte intégral dans [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE).

Gratuit pour l'usage personnel, l'apprentissage et la recherche académique. AGPL-3.0 est une licence **copyleft forte, non permissive** : si vous distribuez une version modifiée, ou si vous l'exploitez comme un service accessible à d'autres, vous devez publier vos modifications sous la même licence. Pour la plupart des directions juridiques, c'est la ligne qui clôt le débat — d'où le choix d'**[Enterprise](https://fincept.in/enterprise)** par les sociétés : propriétaire, sans aucune obligation de copyleft à gérer. L'usage personnel non distribué n'entraîne aucune obligation.

Fincept ne vend plus de licence commerciale ou académique distincte pour ce dépôt. Les besoins commerciaux, d'entreprise et universitaires sont couverts par **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** aux tarifs publiés ci-dessus.

**Marques.** « Fincept », « Fincept Terminal » et le logo Fincept sont des marques de Fincept Corporation. Leur usage dans tout produit forké, dérivé, renommé ou commercial requiert une autorisation écrite préalable.

Questions : [support@fincept.in](mailto:support@fincept.in) · [Conditions](https://fincept.in/terms) · [Confidentialité](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. Tous droits réservés.

---

<div align="center">

### **Votre réflexion est la seule limite. Les données ne le sont pas.**

⭐ **Star** · 🔄 **Partager** · 🤝 **Contribuer**

<sub>Version originale anglaise : <a href="../../README.md">README.md</a></sub>

</div>
