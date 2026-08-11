> [!IMPORTANT]
> **Dos ediciones.** **[Enterprise](https://fincept.in/enterprise)** es la versión privada y de código cerrado para fondos y mesas de análisis — 41 módulos, datos privados, enrutamiento en vivo a brókers, SSO, desde **99 $/usuario/mes**. **Este repositorio** es la edición libre AGPL-3.0 para aprendizaje y uso académico, con una versión al mes.
> [Comparativa](https://fincept.in/comparison) · [Precios](https://fincept.in/pricing)

# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **Tu pensamiento es el único límite. Los datos no lo son.**

Plataforma de inteligencia financiera de última generación, con analítica de nivel institucional, automatización con IA y conectividad de datos sin límites.

[📥 Descargar](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 Precios](https://fincept.in/pricing) · [📖 Manual](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

---

## Acerca de

**Fincept Terminal** es un terminal de escritorio nativo en C++20 para investigación financiera — interfaz Qt6, analítica con Python 3.11 embebido, un solo binario, sin Electron.

Dos ediciones funcionan sobre un mismo núcleo de datos. **[Enterprise](https://fincept.in/enterprise)** es la versión privada y de código cerrado en la que el equipo trabaja a diario, para fondos, family offices y mesas de análisis. **Este repositorio** es la edición libre AGPL-3.0 — aprendizaje, uso personal, investigación académica — con una versión al mes.

Usa la edición abierta si eres estudiante, aficionado o académico. Usa Enterprise si eres una firma, o si el terminal es tu medio de vida: el copyleft de AGPL no aplica, y el coste real de la edición abierta son tus propias facturas de datos y de LLM, cobradas por token y sin techo.

| | Código abierto | **Enterprise** |
|---|---|---|
| **Licencia** | AGPL-3.0 — copyleft fuerte | Propietaria — sin obligaciones de copyleft |
| **Coste** | Gratis, más tus facturas de datos y LLM | 99 $ / 199 $ / 299 $ por usuario/mes |
| **Datos** | Fuentes públicas gratuitas, tus propias claves | Conjuntos de datos privados, más histórico, point-in-time |
| **IA** | Tu propia clave de LLM | 400–5.000 créditos incluidos · investigación multiagente · sala de datos privada |
| **Trading** | Papel + integraciones de bróker | Enrutamiento en vivo a brókers + despliegue de algos en vivo |
| **Controles** | — | SSO/SAML, registros de auditoría, RBAC, soporte con SLA |

[**Ver Enterprise →**](https://fincept.in/enterprise) · [Comparativa completa](https://fincept.in/comparison) · [Precios](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Enterprise

41 módulos repartidos en seis mesas — investigación agéntica, quant lab y backtesting, análisis fundamental profundo, mercados y ejecución, macro e inteligencia global, y tu propio espacio de trabajo. Todo ello en un [manual de 700 páginas](https://fincept.in/manual).

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **99 $**/usuario/mes | **199 $**/usuario/mes | **299 $**/usuario/mes |
| Créditos de IA / mes | 400 | 2.000 | 5.000 |
| Deep research + equipos de agentes | — | ✓ | ✓ |
| Trading en vivo + algos | — | — | ✓ |

Facturación mensual, sin permanencia, sin mínimo de licencias, 15 % de descuento trimestral — **1.188–3.588 $ por usuario y año**, frente a unos 27.000 $ de una licencia Bloomberg. **Universidades:** 5 licencias Exclusive Pro por **699 $/mes**. Estos planes son la lista de precios completa: sin precios negociados y sin licencia comercial aparte.

Enterprise necesita su propia cuenta — las cuentas gratuitas de Fincept no sirven para entrar.

[**Crear cuenta**](https://fincept.in/enterprise/signup) · [**Reservar una demo**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## Instalación

Los instaladores para **Windows x64**, **Linux x64** (`.run` / `.deb` / `.rpm`) y **macOS (Apple silicon)** están en la [página de releases](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest).

**Compilar desde el código** — Linux/macOS: `git clone … && ./setup.sh`. Windows, compilaciones manuales, el conjunto de herramientas fijado (**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**) y la resolución de problemas están en **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**. Las versiones están fijadas — otras no están soportadas.

> ¿Buscas la compilación Enterprise? Tiene sus propios instaladores firmados para Windows, macOS y Linux, detrás de un inicio de sesión Enterprise — [consíguelos aquí](https://fincept.in/enterprise).

---

## Qué incluye la edición abierta

- **Analítica** — DCF, optimización de carteras, VaR/Sharpe, valoración de derivados, renta fija, alternativos, más una suite QuantLib de 18 módulos
- **IA** — 37 agentes de trader/inversor, economía y geopolítica; con tu propia clave (OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama)
- **Datos** — más de 100 conectores: FRED, FMI, Banco Mundial, DBnomics, AkShare, Polygon, Kraken, Yahoo Finance, APIs gubernamentales
- **Trading** — feeds de cripto y renta variable, motor de paper trading, 16 integraciones de bróker
- **Automatización** — editor de nodos visual, herramientas MCP, AI Quant Lab (ML, descubrimiento de factores, RL)
- **Inteligencia global** — seguimiento marítimo, análisis geopolítico, mapa de relaciones

C++20 nativo · Qt6 · Python 3.11 embebido · un solo binario · sin Node.js ni navegador.

---

## Cómo se mantiene este repositorio

Este repositorio **sigue siendo público y no se va a eliminar**. Todo lo ya publicado permanece publicado.

Ahora publica **una versión al mes** en lugar de desarrollo continuo, porque el trabajo diario del equipo está en Enterprise. Las issues y los pull requests se siguen revisando, y las correcciones llegan en el ciclo mensual. Los informes de seguridad, a [support@fincept.in](mailto:support@fincept.in).

---

## Contribuir

Nuevos conectores de datos, agentes de IA, módulos de analítica, pantallas C++ y documentación: todo es bienvenido.

[Guía de contribución](../CONTRIBUTING.md) · [Guía C++](../CPP_CONTRIBUTOR_GUIDE.md) · [Guía Python](../PYTHON_CONTRIBUTOR_GUIDE.md) · [Arquitectura](../ARCHITECTURE.md) · [Reportar un fallo](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [Proponer una función](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## También de Fincept

- **[Fincept Data API](https://docs.fincept.in)** — más de 500 endpoints REST, más de 423.000 instrumentos, más de 2.000 fuentes. Capa gratuita incluida con cualquier cuenta.
- **[Quantcept](https://quantcept.io)** — terminal financiero de línea de comandos, de código abierto y con IA (Apache-2.0).

---

## Licencia

**AGPL-3.0-or-later** — texto completo en [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE).

Gratis para uso personal, aprendizaje e investigación académica. AGPL-3.0 es **copyleft fuerte, no permisiva**: si distribuyes una compilación modificada, o la ejecutas como servicio al que acceden otras personas, debes publicar tus cambios bajo la misma licencia. Para la mayoría de los departamentos legales, ahí se acaba la discusión — y por eso las firmas eligen **[Enterprise](https://fincept.in/enterprise)**, que es propietaria y no arrastra obligaciones de copyleft. El uso personal, sin distribución, no conlleva ninguna obligación.

Fincept ya no vende una licencia comercial o académica aparte para este repositorio. Las necesidades comerciales, empresariales y universitarias se cubren con **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** a los precios publicados arriba.

**Marcas.** «Fincept», «Fincept Terminal» y el logotipo de Fincept son marcas de Fincept Corporation. Su uso en cualquier producto bifurcado, derivado, renombrado o comercial requiere autorización previa por escrito.

Consultas: [support@fincept.in](mailto:support@fincept.in) · [Términos](https://fincept.in/terms) · [Privacidad](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. Todos los derechos reservados.

---

<div align="center">

### **Tu pensamiento es el único límite. Los datos no lo son.**

⭐ **Estrella** · 🔄 **Comparte** · 🤝 **Contribuye**

<sub>Original en inglés: <a href="../../README.md">README.md</a></sub>

</div>
