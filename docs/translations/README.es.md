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

> [!IMPORTANT]
> **Fincept Terminal se distribuye en dos ediciones.**
>
> 🏢 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** — la versión privada y de código cerrado para fondos, family offices y mesas de análisis. 41 módulos, conjuntos de datos propietarios, investigación multiagente, enrutamiento en vivo a brókers, sala de datos privada, SSO y SLA, desde **99 $/usuario/mes**. Es donde el equipo desarrolla a diario, y es la edición que necesitas si el terminal es tu medio de vida.
>
> 📖 **Este repositorio** — la edición libre de código abierto bajo AGPL-3.0, para aprendizaje, uso personal e investigación académica. Sigue siendo público, no se eliminará y recibe **una versión al mes**.
>
> [**¿Cuál necesitas? →**](#qué-edición-deberías-usar) · [Comparativa](https://fincept.in/comparison) · [Precios](https://fincept.in/pricing)

---

## Acerca de

**Fincept Terminal** es un terminal de escritorio nativo en C++20 para investigación financiera — interfaz Qt6, analítica con Python 3.11 embebido, un solo binario, sin Electron.

Se distribuye en **dos ediciones sobre un mismo núcleo de datos**:

| | **Código abierto** — este repo | **Enterprise** — [fincept.in/enterprise](https://fincept.in/enterprise) |
|---|---|---|
| **Pensado para** | Aprendizaje, uso personal, investigación académica | Fondos, family offices, mesas de análisis |
| **Precio** | Gratis · AGPL-3.0 | Desde **99 $**/usuario/mes |
| **Cadencia de versiones** | Una actualización al mes | Continua |

---

## ¿Qué edición deberías usar?

| Si eres… | Usa | Por qué |
|---|---|---|
| Estudiante, aficionado o autodidacta | **Código abierto** | Es realmente gratis, y va a seguir siéndolo |
| Investigador académico | **Código abierto** | Gratis para uso académico — universidades que quieran licencias gestionadas: ver el paquete académico abajo |
| Un fondo, family office, mesa propietaria, banco o fintech | **Enterprise** | El copyleft de AGPL no aplica, y obtienes datos privados, enrutamiento en vivo, SSO y SLA |
| Alguien que hace esto profesionalmente, por dinero | **Enterprise** | En la práctica sale más barato, y es la única edición en desarrollo diario |

> **La versión honesta.** La edición abierta es trabajo real, no una demo capada — pero funciona con fuentes públicas gratuitas, **tus** claves de API y **tu** clave de LLM, y cada llamada de IA se te factura por token, sin techo. Las incidencias de la comunidad se atienden según disponibilidad, sin compromiso de respuesta. Si el terminal es tu forma de ganarte la vida, Enterprise es la licencia más barata y más segura.

[**Ver Enterprise →**](https://fincept.in/enterprise) · [Comparativa completa](https://fincept.in/comparison) · [Precios](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Código abierto vs. Enterprise

| | Código abierto | **Enterprise** |
|---|---|---|
| **Licencia** | AGPL-3.0 — copyleft fuerte. Si lo distribuyes o lo alojas, publicas tus cambios | Propietaria — sin obligaciones de copyleft que gestionar |
| **Coste** | Gratis — pagas tus propias facturas de datos y de LLM | 99 $ / 199 $ / 299 $ por usuario y mes, todo incluido |
| **Módulos** | Terminal base | **41 módulos en 6 mesas** |
| **Datos** | Fuentes públicas gratuitas, con límite de peticiones, claves propias | Conjuntos de datos privados y propietarios — más histórico, refresco más rápido |
| **Histórico de precios** | Lo que permitan las capas gratuitas | 1 año / 5 años / ilimitado · **point-in-time** para backtests honestos |
| **Presupuesto de IA** | Tu propia clave de LLM, facturada por token | 400 / 2.000 / 5.000 créditos incluidos al mes |
| **Investigación con IA** | Asistente de mercado básico | Investigación multiagente que **planifica y delega** · Agent Studio · hasta 53 agentes · 6 modos de equipo |
| **Sala de datos privada** | — | Tus informes y modelos, leídos solo por tus agentes — nunca agrupados, nunca usados para entrenar |
| **Trading** | Papel + integraciones de bróker con claves propias | **Enrutamiento en vivo a brókers + despliegue de algos en vivo** |
| **Quant** | Backtesting comunitario | Quant Lab, Alpha Arena, analítica de volatilidad, backtests point-in-time |
| **Seguridad y cumplimiento** | — | SSO/SAML, registros de auditoría, control de acceso por roles, aislamiento de datos |
| **Soporte** | Issues de GitHub, según disponibilidad, sin compromiso | Atención prioritaria con SLA |
| **Documentación** | Este repo | [**Manual de 700 páginas**](https://fincept.in/manual) — 41 guías, 472 secciones |
| **Plataformas** | Windows, macOS, Linux + terminal web alojado | Windows 10/11, macOS 13+ (Apple silicon), Linux |

Enterprise funciona con una **cuenta independiente** — las cuentas gratuitas de Fincept no sirven para entrar, y la aplicación permanece bloqueada hasta que se asocia una suscripción. [Crear cuenta Enterprise →](https://fincept.in/enterprise/signup)

---

## Enterprise — seis mesas, 41 módulos

| Mesa | Qué hace |
|---|---|
| **Agentic Research** · 6 | Los agentes planifican el trabajo, delegan en especialistas, leen datos en vivo y tu sala de datos, y devuelven notas con fuentes |
| **Quant Lab & Backtesting** · 4 | Investigación de señales, backtests point-in-time, superficies de volatilidad |
| **Deep Fundamental Research** · 8 | Análisis de renta variable, valoración, derivados, analítica de M&A |
| **Markets & Execution** · 7 | Renta variable, cripto y mercados de predicción en vivo, enrutamiento a brókers, despliegue de algos |
| **Macro & Global Intelligence** · 7 | Estadística, datos gubernamentales, geopolítica, rutas marítimas |
| **Tu propio espacio de trabajo** · 9 | Panel, hoja de cálculo, notas, archivos, código, generador de informes |

[Ver los productos →](https://fincept.in/products)

### Precios

| | **Exclusive** | **Exclusive+** ★ el más elegido | **Exclusive Pro** |
|---|---|---|---|
| | **99 $**/usuario/mes | **199 $**/usuario/mes | **299 $**/usuario/mes |
| Créditos de IA / mes | 400 | 2.000 | 5.000 |
| Carteras · listas de seguimiento | 1 · 3 | 10 · 25 | Ilimitadas |
| Histórico de precios | 1 año | 5 años | Ilimitado |
| Licencias incluidas | 1 | 1 | 2 |
| Deep research + equipos de agentes | — | ✓ | ✓ |
| Vinculación de cuenta de bróker | — | ✓ | ✓ |
| Trading en vivo + despliegue de algos | — | — | ✓ |

Facturación mensual · sin permanencia anual · sin mínimo de licencias · **15 % de descuento trimestral**. Son **1.188–3.588 $ por usuario y año**, frente a unos **27.000 $** de una licencia Bloomberg — [ver la comparativa completa](https://fincept.in/comparison).

**Universidades e instituciones académicas:** un paquete único — **5 licencias Exclusive Pro por 699 $/mes** (precio de lista 1.495 $). Escribe a [support@fincept.in](mailto:support@fincept.in).

Estos tres planes más el paquete académico son la lista de precios completa. No hay precios enterprise negociados ni a medida, ni una licencia comercial aparte que comprar.

[**Crear cuenta Enterprise**](https://fincept.in/enterprise/signup) · [**Reservar una demo**](https://calendly.com/nikultilak/fincept-terminal-demo) · [Leer el manual](https://fincept.in/manual)

---

## Instalar la edición de código abierto

Los instaladores para **Windows x64**, **Linux x64** (`.run` / `.deb` / `.rpm`) y **macOS (Apple silicon)** están en la [página de releases](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest).

**Compilar desde el código** — Linux/macOS: `git clone … && ./setup.sh`. Windows, compilaciones manuales, el conjunto de herramientas fijado (**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**) y la resolución de problemas están en **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**. Las versiones están fijadas — otras no están soportadas.

> ¿Buscas la compilación Enterprise? Tiene sus propios instaladores firmados para Windows, macOS y Linux, detrás de un inicio de sesión Enterprise — [consíguelos aquí](https://fincept.in/enterprise).

---

## Qué incluye la edición abierta

| | |
|---|---|
| **Analítica multiactivo** | DCF, optimización de carteras, VaR/Sharpe, valoración de derivados, renta fija, alternativos — vía Python embebido |
| **Suite QuantLib** | 18 módulos cuantitativos — pricing, riesgo, estocástico, volatilidad, renta fija |
| **Agentes de IA** | 37 agentes de trader/inversor, economía y geopolítica; con tu propia clave (OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama) |
| **Más de 100 conectores** | DBnomics, FRED, FMI, Banco Mundial, AkShare, Polygon, Kraken, Yahoo Finance, APIs gubernamentales |
| **Trading** | Feeds de cripto y renta variable, motor de paper trading, 16 integraciones de bróker |
| **Automatización** | Editor de nodos visual, integración de herramientas MCP, AI Quant Lab (ML, descubrimiento de factores, RL) |
| **Inteligencia global** | Seguimiento marítimo, análisis geopolítico, mapa de relaciones |

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

Fincept **ya no vende una licencia comercial o académica aparte** para este repositorio. Las necesidades comerciales, institucionales y universitarias se cubren con **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** a los precios publicados arriba.

**Marcas.** «Fincept», «Fincept Terminal» y el logotipo de Fincept son marcas de Fincept Corporation. Su uso en cualquier producto bifurcado, derivado, renombrado o comercial requiere autorización previa por escrito.

Consultas: [support@fincept.in](mailto:support@fincept.in) · [Términos](https://fincept.in/terms) · [Privacidad](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. Todos los derechos reservados.

---

<div align="center">

### **Tu pensamiento es el único límite. Los datos no lo son.**

⭐ **Estrella** · 🔄 **Comparte** · 🤝 **Contribuye**

<sub>Original en inglés: <a href="../../README.md">README.md</a></sub>

</div>
