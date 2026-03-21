# Terminal Fincept

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![ImGui](https://img.shields.io/badge/ImGui-Docking-blue)](https://github.com/ocornut/imgui)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **Tu pensamiento es el único límite. Los datos no lo son.**

Plataforma de inteligencia financiera de última generación con análisis de nivel CFA, automatización de IA y conectividad de datos ilimitada.

[📥 Descargar](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 Documentos](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 Discusiones](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 Discordia](https://discord.gg/ae87a8ygbN)·[🤝 Socio](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png)

</div>

* * *

## Acerca de

**Terminal fincept vch**es una aplicación de escritorio nativa pura de C++20: una reescritura completa de la pila anterior de Tauri/React/Rust. se utiliza**Estimado ImGui**para interfaz de usuario acelerada por GPU,**GLFW+OpenGL**para renderizado, incrustado**Pitón**para análisis y ofrece rendimiento de clase terminal Bloomberg en un único binario nativo.

* * *

## Pila de tecnología

| Capa                         | Tecnologías                                    |
| ---------------------------- | ---------------------------------------------- |
| **Idioma**                   | C++20 (MSVC/GCC/Clang)                         |
| **interfaz de usuario**      | Estimado ImGui (rama de acoplamiento) + ImPlot |
| **Disposición**              | Yoga (motor Flexbox)                           |
| **Representación**           | GLFW 3 + OpenGL 3.3+                           |
| **Redes**                    | libcurl+OpenSSL                                |
| **Base de datos**            | SQLite 3                                       |
| **Publicación por entregas** | nlohmann/json                                  |
| **Explotación florestal**    | spdlog                                         |
| **Audio**                    | miniaudio                                      |
| **Video**                    | libmpv (opcional)                              |
| **Analítica**                | Python integrado 3.11+ (más de 100 scripts)    |
| **Construir**                | CMake 3.20+ / vcpkg                            |

* * *

## Características

| **Característica**                   | **Descripción**                                                                                                           |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------- |
| 📊**Análisis a nivel CFA**           | Modelos DCF, optimización de cartera, métricas de riesgo (VaR, Sharpe), precios de derivados a través de Python integrado |
| 🤖**Agentes de IA**                  | Más de 20 personas de inversores (Buffett, Dalio, Graham), estrategias de fondos de cobertura, soporte local de LLM       |
| 🌐**Más de 100 conectores de datos** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, FMI, Banco Mundial, AkShare, API gubernamentales                          |
| 📈**Comercio en tiempo real**        | Cripto (Kraken/HyperLiquid WebSocket), acciones, comercio algorítmico, motor de comercio de papel                         |
| 🔬**Suite QuantLib**                 | 18 módulos de análisis cuantitativo: fijación de precios, riesgo, estocástico, volatilidad, renta fija                    |
| 🚢**Inteligencia global**            | Seguimiento marítimo, análisis geopolítico, mapeo de relaciones, datos satelitales.                                       |
| 🎨**Flujos de trabajo visuales**     | Editor de nodos para canales de automatización, integración de herramientas MCP                                           |
| 🧠**Laboratorio cuantitativo de IA** | Modelos de aprendizaje automático, descubrimiento de factores, HFT, comercio de aprendizaje por refuerzo                  |

* * *

## Más de 40 pantallas

| Categoría               | Pantallas                                                                                                                                                                               |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Centro**              | Panel de control, Mercados, Noticias, Lista de seguimiento                                                                                                                              |
| **Comercio**            | Comercio de criptomonedas, comercio de acciones, comercio de algoritmos, pruebas retrospectivas, visualización comercial                                                                |
| **Investigación**       | Investigación de acciones, Screener, Cartera, Análisis de superficie, Análisis de fusiones y adquisiciones, Derivados, Inversiones alternativas                                         |
| **QuantLib**            | Núcleo, Análisis, Curvas, Economía, Instrumentos, ML, Modelos, Numérico, Física, Portafolio, Precios, Regulatorio, Riesgo, Programación, Solver, Estadísticas, Estocástico, Volatilidad |
| **IA/ML**               | AI Quant Lab, Agent Studio, AI Chat, Alpha Arena                                                                                                                                        |
| **Ciencias económicas** | Economía, DBnomics, AkShare, Mercados de Asia                                                                                                                                           |
| **Geopolítica**         | Geopolítica, datos gubernamentales, mapa de relaciones, marítimo, polimercado                                                                                                           |
| **Herramientas**        | Editor de código, Editor de nodos, Excel, Generador de informes, Notas, Fuentes de datos, Mapeo de datos, Servidores MCP                                                                |
| **Comunidad**           | Foro, Perfil, Configuración, Soporte, Documentos, Acerca de                                                                                                                             |

* * *

## Inicio rápido (configuración con un clic)

Clona y ejecuta el script de instalación: instala todas las dependencias y crea la aplicación automáticamente:

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

El script maneja: verificación del compilador, CMake, Ninja, Python, vcpkg, todas las dependencias de C++, compilación y lanzamiento.

* * *

## Descargar y ejecutar (no se requiere compilación)

Los archivos binarios prediseñados están disponibles en[Página de lanzamientos](https://github.com/Fincept-Corporation/FinceptTerminal/releases).

| Plataforma                | Descargar                                | Correr                         |
| ------------------------- | ---------------------------------------- | ------------------------------ |
| **Ventanas x64**          | `FinceptTerminal-Windows-x64.zip`        | Extraer →`FinceptTerminal.exe` |
| **Linuxx64**              | `FinceptTerminal-Linux-x64.tar.gz`       | Extraer →`./FinceptTerminal`   |
| **macOS (Apple Silicio)** | `FinceptTerminal-macOS-arm64.tar.gz`     | Extraer →`./FinceptTerminal`   |
| **MacOS (Intel)**         | `FinceptTerminal-macOS-x64.tar.gz`       | Extraer →`./FinceptTerminal`   |
| **macOS (universal)**     | `FinceptTerminal-macOS-universal.tar.gz` | Extraer →`./FinceptTerminal`   |

No requiere instalación: simplemente extraiga y ejecute.

* * *

## Construir desde la fuente

### Requisitos previos

| Herramienta            | Versión   | ventanas                                                          | linux                     | macos                              |
| ---------------------- | --------- | ----------------------------------------------------------------- | ------------------------- | ---------------------------------- |
| **git**                | el último | `winget install Git.Git`                                          | `apt install git`         | `brew install git`                 |
| **Chacer**             | 3.20+     | `winget install Kitware.CMake`                                    | `apt install cmake`       | `brew install cmake`               |
| **ninja**              | el último | `winget install Ninja-build.Ninja`                                | `apt install ninja-build` | `brew install ninja`               |
| **compilador de C ++** | C++20     | MSVC 2022 ([estudio visual](https://visualstudio.microsoft.com/)) | `apt install g++`         | Xcode CLT:`xcode-select --install` |
| **vcpkg**              | el último | Vea abajo                                                         | Vea abajo                 | Vea abajo                          |
| **Pitón**              | 3.11+     | [python.org](https://www.python.org/downloads/)                   | `apt install python3`     | `brew install python`              |

#### Instalar vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh       # Linux / macOS
# or
git clone https://github.com/microsoft/vcpkg.git %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat   # Windows
```

Luego establezca`VCPKG_ROOT`permanentemente:

```bash
# Linux / macOS — add to ~/.bashrc or ~/.zshrc
export VCPKG_ROOT=~/vcpkg

# Windows (PowerShell — run once)
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT","$env:USERPROFILE\vcpkg","User")
```

#### Dependencias del sistema Linux

```bash
sudo apt install -y \
  libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev \
  libxrandr-dev libxi-dev libxext-dev libxfixes-dev \
  libwayland-dev libxkbcommon-dev \
  pkg-config
```

### Construir

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-cpp

# All platforms (requires VCPKG_ROOT set)
cmake --preset=default
cmake --build build --config Release
```

### Correr

```bash
./build/FinceptTerminal          # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

### Dependencias de vcpkg

Todas las dependencias se instalan automáticamente mediante vcpkg:
glfw3, curl, nlohmann-json, sqlite3, openssl, imgui (acoplamiento + tipo libre), yoga, stb, implot, spdlog, miniaudio

* * *

## Lo que nos diferencia

**Terminal Fincept**es una plataforma financiera de código abierto creada para aquellos que se niegan a verse limitados por el software tradicional. Competimos en**profundidad analítica**y**accesibilidad a los datos**– no en información privilegiada ni en feeds exclusivos.

-   **Rendimiento nativo**— C++ con ImGui acelerado por GPU, sin sobrecarga de Electron/web
-   **binario único**— sin Node.js, sin tiempo de ejecución del navegador, sin paquete de JavaScript
-   **Análisis a nivel CFA**— cobertura curricular completa a través de módulos de Python
-   **Más de 100 conectores de datos**— desde Yahoo Finance hasta las bases de datos gubernamentales
-   **Gratis y de código abierto**(AGPL-3.0) con licencias comerciales disponibles

* * *

## Hoja de ruta

**Primer trimestre de 2026:**Migración a C++ completa, transmisión en tiempo real mejorada, backtesting avanzado**2026:**Creador de estrategias de opciones, gestión de carteras múltiples, más de 50 agentes de IA**Futuro:**Complemento móvil, funciones institucionales, API programática, interfaz de usuario de capacitación de aprendizaje automático

* * *

## Contribuyendo

Estamos construyendo juntos el futuro del análisis financiero.

**Contribuir:**Nuevos conectores de datos, agentes de IA, módulos de análisis, pantallas C++, documentación

-   [Guía contribuyente](docs/CONTRIBUTING.md)
-   [Guía de contribución de C++](fincept-cpp/CONTRIBUTING.md)
-   [Guía para contribuyentes de Python](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [Informar error](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [Solicitar función](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## Para universidades y educadores

**Lleve análisis financieros de nivel profesional a su salón de clases.**

-   **$799/mes**por 20 cuentas
-   Acceso completo a los datos y API de Fincept
-   Perfecto para cursos de finanzas, economía y ciencia de datos
-   Análisis curricular CFA integrado

**¿Interesado?**Correo electrónico**[support@fincept.in](mailto:support@fincept.in)**con el nombre de su institución.

[University Licensing Details](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## Licencia

**Licencia dual: AGPL-3.0 (código abierto) + Comercial**

### Código abierto (AGPL-3.0)

-   Gratis para uso personal, educativo y no comercial
-   Requiere modificaciones para compartir cuando se distribuye o se utiliza como servicio de red
-   Transparencia total del código fuente

### Licencia Comercial

-   Requerido para uso comercial o para acceder comercialmente a Fincept Data/API
-   Contacto:**[support@fincept.in](mailto:support@fincept.in)**
-   Detalles:[Guía de licencia comercial](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### Marcas registradas

"Fincept Terminal" y "Fincept" son marcas comerciales de Fincept Corporation.

© 2025-2026 Corporación Fincept. Reservados todos los derechos.

* * *

<div align="center">

### **Tu pensamiento es el único límite. Los datos no lo son.**

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

⭐**Estrella**· 🔄**Compartir**· 🤝**Contribuir**

</div>
