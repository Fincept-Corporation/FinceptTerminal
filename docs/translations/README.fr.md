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

> [!IMPORTANT]
> **Fincept Terminal existe en deux éditions.**
>
> 🏢 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** — la version privée, à code fermé, destinée aux fonds, family offices et desks de recherche. 41 modules, jeux de données propriétaires, recherche multi-agents, routage courtier en direct, data room privée, SSO et SLA, à partir de **99 $/utilisateur/mois**. C'est là que l'équipe développe au quotidien, et c'est l'édition à prendre si le terminal est votre gagne-pain.
>
> 📖 **Ce dépôt** — l'édition open source gratuite sous AGPL-3.0, pour l'apprentissage, l'usage personnel et la recherche académique. Il reste public, ne sera pas supprimé, et reçoit **une version par mois**.
>
> [**Laquelle vous faut-il ? →**](#choisir-votre-édition) · [Comparatif](https://fincept.in/comparison) · [Tarifs](https://fincept.in/pricing)

---

## À propos

**Fincept Terminal** est un terminal de bureau natif en C++20 dédié à la recherche financière — interface Qt6, analyse Python 3.11 embarquée, un seul binaire, sans Electron.

Il existe en **deux éditions sur un socle de données commun** :

| | **Open source** — ce dépôt | **Enterprise** — [fincept.in/enterprise](https://fincept.in/enterprise) |
|---|---|---|
| **Conçu pour** | L'apprentissage, l'usage personnel, la recherche académique | Fonds, family offices, desks de recherche |
| **Prix** | Gratuit · AGPL-3.0 | À partir de **99 $**/utilisateur/mois |
| **Rythme des versions** | Une mise à jour par mois | En continu |

---

## Choisir votre édition

| Si vous êtes… | Prenez | Pourquoi |
|---|---|---|
| Étudiant, amateur ou autodidacte | **Open source** | Vraiment gratuit, et ça ne changera pas |
| Chercheur académique | **Open source** | Gratuit pour l'usage académique — les universités souhaitant des postes gérés : voir l'offre académique ci-dessous |
| Un fonds, family office, desk propriétaire, une banque ou une fintech | **Enterprise** | Le copyleft AGPL ne s'applique pas, et vous obtenez données privées, routage en direct, SSO et SLA |
| Quelqu'un qui en vit professionnellement | **Enterprise** | Moins cher en pratique, et la seule édition en développement quotidien |

> **Pour être franc.** L'édition ouverte est un vrai produit, pas une démo bridée — mais elle fonctionne avec des flux publics gratuits, **vos** clés d'API et **votre** clé LLM, et chaque appel d'IA vous est facturé au token, sans plafond. Les tickets communautaires sont traités au mieux, sans engagement de délai. Si le terminal est votre outil de travail, Enterprise est le poste le moins cher et le plus sûr.

[**Découvrir Enterprise →**](https://fincept.in/enterprise) · [Comparatif complet](https://fincept.in/comparison) · [Tarifs](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Open source vs Enterprise

| | Open source | **Enterprise** |
|---|---|---|
| **Licence** | AGPL-3.0 — copyleft fort. Si vous distribuez ou hébergez, vous publiez vos modifications | Propriétaire — aucune obligation de copyleft à gérer |
| **Coût** | Gratuit — vous payez vos propres factures de données et de LLM | 99 $ / 199 $ / 299 $ par utilisateur et par mois, tout compris |
| **Modules** | Terminal de base | **41 modules répartis sur 6 desks** |
| **Données** | Flux publics gratuits, limités en débit, clés à fournir | Jeux de données privés et propriétaires — historique plus profond, rafraîchissement plus rapide |
| **Historique de prix** | Ce que permettent les offres gratuites | 1 an / 5 ans / illimité · **point-in-time** pour des backtests honnêtes |
| **Budget IA** | Votre clé LLM, facturée au token | 400 / 2 000 / 5 000 crédits inclus par mois |
| **Recherche IA** | Assistant de marché basique | Recherche multi-agents qui **planifie et délègue** · Agent Studio · jusqu'à 53 agents · 6 modes d'équipe |
| **Data room privée** | — | Vos documents et modèles, lus uniquement par vos agents — jamais mutualisés, jamais utilisés pour l'entraînement |
| **Trading** | Papier + intégrations courtier avec vos clés | **Routage courtier en direct + déploiement d'algos en direct** |
| **Quant** | Backtesting communautaire | Quant Lab, Alpha Arena, analyse de volatilité, backtests point-in-time |
| **Sécurité et conformité** | — | SSO/SAML, journaux d'audit, contrôle d'accès par rôle, isolation des données |
| **Support** | Issues GitHub, au mieux, sans engagement de réponse | Traitement prioritaire adossé à un SLA |
| **Documentation** | Ce dépôt | [**Manuel de 700 pages**](https://fincept.in/manual) — 41 guides, 472 sections |
| **Plateformes** | Windows, macOS, Linux + terminal web hébergé | Windows 10/11, macOS 13+ (Apple silicon), Linux |

Enterprise fonctionne avec un **compte distinct** — les identifiants Fincept gratuits ne permettent pas de s'y connecter, et l'application reste verrouillée tant qu'aucun abonnement n'y est rattaché. [Créer un compte Enterprise →](https://fincept.in/enterprise/signup)

---

## Enterprise — six desks, 41 modules

| Desk | Rôle |
|---|---|
| **Agentic Research** · 6 | Les agents planifient le travail, délèguent aux spécialistes, lisent les données en direct et votre data room, et rendent des notes sourcées |
| **Quant Lab & Backtesting** · 4 | Recherche de signaux, backtests point-in-time, surfaces de volatilité |
| **Deep Fundamental Research** · 8 | Analyse actions, valorisation, dérivés, analyse M&A |
| **Markets & Execution** · 7 | Actions, crypto et marchés prédictifs en direct, routage courtier, déploiement d'algos |
| **Macro & Global Intelligence** · 7 | Statistiques, données publiques, géopolitique, routes maritimes |
| **Votre espace de travail** · 9 | Tableau de bord, tableur, notes, fichiers, code, générateur de rapports |

[Voir les produits →](https://fincept.in/products)

### Tarifs

| | **Exclusive** | **Exclusive+** ★ le plus choisi | **Exclusive Pro** |
|---|---|---|---|
| | **99 $**/utilisateur/mois | **199 $**/utilisateur/mois | **299 $**/utilisateur/mois |
| Crédits IA / mois | 400 | 2 000 | 5 000 |
| Portefeuilles · listes de suivi | 1 · 3 | 10 · 25 | Illimités |
| Historique de prix | 1 an | 5 ans | Illimité |
| Postes inclus | 1 | 1 | 2 |
| Deep research + équipes d'agents | — | ✓ | ✓ |
| Liaison compte courtier | — | ✓ | ✓ |
| Trading en direct + déploiement d'algos | — | — | ✓ |

Facturation mensuelle · sans engagement annuel · sans minimum de postes · **15 % de remise au trimestre**. Soit **1 188 à 3 588 $ par utilisateur et par an**, contre environ **27 000 $** pour un poste Bloomberg — [voir le comparatif complet](https://fincept.in/comparison).

**Universités et établissements académiques :** une offre forfaitaire — **5 postes Exclusive Pro pour 699 $/mois** (prix catalogue 1 495 $). Écrivez à [support@fincept.in](mailto:support@fincept.in).

Ces trois formules et l'offre académique constituent la totalité de la grille tarifaire. Il n'existe ni tarif enterprise sur mesure ou négocié, ni licence commerciale distincte à acheter.

[**Créer un compte Enterprise**](https://fincept.in/enterprise/signup) · [**Réserver une démo**](https://calendly.com/nikultilak/fincept-terminal-demo) · [Lire le manuel](https://fincept.in/manual)

---

## Installer l'édition open source

Les installateurs pour **Windows x64**, **Linux x64** (`.run` / `.deb` / `.rpm`) et **macOS (Apple silicon)** se trouvent sur la [page des releases](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest).

**Compiler depuis les sources** — Linux/macOS : `git clone … && ./setup.sh`. Windows, compilation manuelle, chaîne d'outils figée (**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**) et dépannage : voir **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**. Les versions sont figées — les autres ne sont pas prises en charge.

> Vous cherchez la version Enterprise ? Elle dispose de ses propres installateurs signés pour Windows, macOS et Linux, derrière une authentification Enterprise — [à récupérer ici](https://fincept.in/enterprise).

---

## Ce que contient l'édition ouverte

| | |
|---|---|
| **Analyse multi-actifs** | DCF, optimisation de portefeuille, VaR/Sharpe, valorisation de dérivés, taux, actifs alternatifs — via Python embarqué |
| **Suite QuantLib** | 18 modules quantitatifs — pricing, risque, stochastique, volatilité, taux |
| **Agents IA** | 37 agents trader/investisseur, économie et géopolitique ; avec votre propre clé (OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama) |
| **Plus de 100 connecteurs** | DBnomics, FRED, FMI, Banque mondiale, AkShare, Polygon, Kraken, Yahoo Finance, API publiques |
| **Trading** | Flux crypto et actions, moteur de paper trading, 16 intégrations courtier |
| **Automatisation** | Éditeur de nœuds visuel, intégration d'outils MCP, AI Quant Lab (ML, découverte de facteurs, RL) |
| **Renseignement global** | Suivi maritime, analyse géopolitique, cartographie de relations |

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

Fincept **ne vend plus de licence commerciale ou académique distincte** pour ce dépôt. Les besoins commerciaux, institutionnels et universitaires sont couverts par **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** aux tarifs publiés ci-dessus.

**Marques.** « Fincept », « Fincept Terminal » et le logo Fincept sont des marques de Fincept Corporation. Leur usage dans tout produit forké, dérivé, renommé ou commercial requiert une autorisation écrite préalable.

Questions : [support@fincept.in](mailto:support@fincept.in) · [Conditions](https://fincept.in/terms) · [Confidentialité](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. Tous droits réservés.

---

<div align="center">

### **Votre réflexion est la seule limite. Les données ne le sont pas.**

⭐ **Star** · 🔄 **Partager** · 🤝 **Contribuer**

<sub>Version originale anglaise : <a href="../../README.md">README.md</a></sub>

</div>
