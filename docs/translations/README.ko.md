# 핀셉트 터미널

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![ImGui](https://img.shields.io/badge/ImGui-Docking-blue)](https://github.com/ocornut/imgui)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **당신의 생각이 유일한 한계입니다. 데이터는 그렇지 않습니다.**

CFA 수준 분석, AI 자동화 및 무제한 데이터 연결을 갖춘 최첨단 금융 인텔리전스 플랫폼입니다.

[📥 다운로드](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 문서](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 토론](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 불화](https://discord.gg/ae87a8ygbN)·[🤝 파트너](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png)

</div>

* * *

## 에 대한

**Fincept 터미널 vch**순수 네이티브 C++20 데스크톱 애플리케이션으로, 이전 Tauri/React/Rust 스택을 완전히 다시 작성했습니다. 그것은 사용한다**임귀님께**GPU 가속 UI의 경우,**GLFW + OpenGL**렌더링용, 임베디드**파이썬**분석을 위해 단일 네이티브 바이너리로 Bloomberg 터미널 수준의 성능을 제공합니다.

* * *

## 기술 스택

| 층             | 기술                               |
| ------------- | -------------------------------- |
| **언어**        | C++20(MSVC/GCC/클랭)               |
| **UI**        | 친애하는 ImGui(도킹 브랜치) + ImPlot      |
| **공들여 나열한 것** | 요가(Flexbox 엔진)                   |
| **표현**        | GLFW 3 + OpenGL 3.3+             |
| **네트워킹**      | libcurl + OpenSSL                |
| **데이터 베이스**   | SQLite 3                         |
| **직렬화**       | nlohmann/json                    |
| **벌채 반출**     | spdlog                           |
| **오디오**       | 미니오디오                            |
| **동영상**       | libmpv(선택사항)                     |
| **해석학**       | 임베디드 Python 3.11+(100개 이상의 스크립트) |
| **짓다**        | CMake 3.20+ / vcpkg              |

* * *

## 특징

| **특징**                 | **설명**                                                                           |
| ---------------------- | -------------------------------------------------------------------------------- |
| 📊**CFA 수준 분석**        | DCF 모델, 포트폴리오 최적화, 위험 지표(VaR, Sharpe), 내장된 Python을 통한 파생 상품 가격 책정                |
| 🤖**AI 에이전트**          | 20명 이상의 투자자 페르소나(Buffett, Dalio, Graham), 헤지펀드 전략, 현지 LLM 지원                     |
| 🌐**100개 이상의 데이터 커넥터** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, IMF, World Bank, AkShare, 정부 API |
| 📈**실시간 거래**           | 암호화폐(Kraken/HyperLiquid WebSocket), 주식, 알고 트레이딩, 종이 트레이딩 엔진                      |
| 🔬**QuantLib 스위트**     | 18개의 정량 분석 ​​모듈 — 가격 책정, 리스크, 확률론적, 변동성, 채권                                      |
| 🚢**글로벌 인텔리전스**        | 해양 추적, 지정학적 분석, 관계 매핑, 위성 데이터                                                    |
| 🎨**시각적 워크플로우**        | 자동화 파이프라인을 위한 노드 편집기, MCP 도구 통합                                                  |
| 🧠**AI 퀀트 연구실**        | ML 모델, 요인 발견, HFT, 강화 학습 거래                                                      |

* * *

## 40개 이상의 화면

| 범주           | 스크린                                                                             |
| ------------ | ------------------------------------------------------------------------------- |
| **핵심**       | 대시보드, 시장, 뉴스, 관심 목록                                                             |
| **거래**       | 암호화폐 거래, 주식 거래, 알고 거래, 백테스팅, 거래 시각화                                             |
| **연구**       | 주식 조사, 스크리너, 포트폴리오, 표면 분석, M&A 분석, 파생 상품, Alt 투자                                |
| **QuantLib** | 코어, 분석, 곡선, 경제학, 도구, ML, 모델, 수치, 물리학, 포트폴리오, 가격, 규제, 위험, 일정, 해결사, 통계, 확률론적, 변동성 |
| **AI/ML**    | AI Quant Lab, Agent Studio, AI Chat, 알파아레나                                      |
| **경제학**      | 경제, DBnomics, AkShare, 아시아 시장                                                   |
| **지정학**      | 지정학, 정부 데이터, 관계 지도, 해양, 폴리마켓                                                    |
| **도구**       | 코드 편집기, 노드 편집기, Excel, 보고서 작성기, 메모, 데이터 소스, 데이터 매핑, MCP 서버                      |
| **지역 사회**    | 포럼, 프로필, 설정, 지원, 문서, 정보                                                         |

* * *

## 소스에서 빌드

### 전제조건

-   **CMake**3.20+
-   **vcpkg**(의존성 관리용)
-   **C++20 컴파일러**(MSVC 2022, GCC 12+ 또는 Clang 15+)
-   **파이썬**3.11+(분석 스크립트용)

### 짓다

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

### 달리다

```bash
./build/FinceptTerminal          # Linux / macOS
.\build\Release\FinceptTerminal  # Windows
```

### vcpkg 종속성

glfw3, 컬, nlohmann-json, sqlite3, openssl, imgui(도킹 + 프리타입), 요가, stb, implot, spdlog, miniaudio

* * *

## 우리를 차별화하는 요소

**핀셉트 터미널**전통적인 소프트웨어의 제한을 거부하는 사람들을 위해 구축된 오픈 소스 금융 플랫폼입니다. 우리는 경쟁합니다**분석 깊이**그리고**데이터 접근성**— 내부자 정보나 독점 피드가 아닙니다.

-   **기본 성능**— GPU 가속 ImGui가 포함된 C++, 전자/웹 오버헤드 없음
-   **단일 바이너리**— Node.js 없음, 브라우저 런타임 없음, JavaScript 번들러 없음
-   **CFA 수준 분석**— Python 모듈을 통한 완전한 커리큘럼 적용
-   **100개 이상의 데이터 커넥터**— Yahoo Finance에서 정부 데이터베이스까지
-   **무료 및 오픈 소스**(AGPL-3.0) 상용 라이센스 사용 가능

* * *

## 로드맵

**2026년 1분기:**C++ 마이그레이션 완료, 향상된 실시간 스트리밍, 고급 백테스팅**2026년:**옵션 전략 빌더, 다중 포트폴리오 관리, 50개 이상의 AI 에이전트**미래:**모바일 컴패니언, 기관 기능, 프로그래밍 방식 API, ML 교육 UI

* * *

## 기여

우리는 재무 분석의 미래를 함께 만들어가고 있습니다.

**기여하다:**새로운 데이터 커넥터, AI 에이전트, 분석 모듈, C++ 화면, 문서

-   [기여 가이드](docs/CONTRIBUTING.md)
-   [C++ 기여 가이드](fincept-cpp/CONTRIBUTING.md)
-   [Python 기여자 가이드](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [버그 신고](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [기능 요청](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## 대학 및 교육자용

**전문가 수준의 재무 분석을 교실에 도입하세요.**

-   **$799/월**20개 계정용
-   Fincept 데이터 및 API에 대한 전체 액세스
-   금융, 경제, 데이터 과학 과정에 적합
-   CFA 커리큘럼 분석 내장

**관심 있는?**이메일**[support@fincept.in](mailto:support@fincept.in)**귀하의 기관 이름으로.

[대학 라이센스 세부정보](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## 특허

**이중 라이센스: AGPL-3.0(오픈 소스) + 상업용**

### 오픈 소스(AGPL-3.0)

-   개인적, 교육적, 비상업적 용도로는 무료입니다.
-   네트워크 서비스로 배포 또는 사용 시 공유 수정 필요
-   전체 소스 코드 투명성

### 상업용 라이센스

-   업무용으로 사용하거나 Fincept 데이터/API에 상업적으로 액세스하는 데 필요합니다.
-   연락하다:**[support@fincept.in](mailto:support@fincept.in)**
-   세부:[상용 라이센스 가이드](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### 상표

"Fincept Terminal" 및 "Fincept"는 Fincept Corporation의 상표입니다.

© 2025-2026 핀셉트 코퍼레이션. 모든 권리 보유.

* * *

<div align="center">

### **당신의 생각이 유일한 한계입니다. 데이터는 그렇지 않습니다.**

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

⭐**별**· 🔄**공유하다**· 🤝**기여하다**

</div>
