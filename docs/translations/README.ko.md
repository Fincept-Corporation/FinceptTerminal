# 핀셉트 터미널 ✨

<div align="center">

[![License: MIT](https://img.shields.io/badge/license-MIT-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE.txt)[![Tauri](https://img.shields.io/badge/Tauri-2.0-FFC131?logo=tauri)](https://tauri.app/)[![React](https://img.shields.io/badge/React-19-61DAFB?logo=react)](https://react.dev/)[![TypeScript](https://img.shields.io/badge/TypeScript-5.6-3178C6?logo=typescript)](https://www.typescriptlang.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

[영어](README.md)\|[스페인 사람](README.es.md)\|[중국인](README.zh-CN.md)\|[일본어](README.ja.md)\|[프랑스 국민](README.fr.md)\|[독일 사람](README.de.md)\|[한국어](README.ko.md)\|[힌디 어](README.hi.md)

### _전문 재무 분석 플랫폼_

**모두를 위한 Bloomberg 수준의 통찰력. 오픈 소스. 크로스 플랫폼.**

[📥 다운로드](#-getting-started)•[✨ 특징](#-features)•[📸 스크린샷](#-platform-preview)•[🤝 기여](#-contributing)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Geopolitics.png)

</div>

* * *

## 🎯 핀셉트 터미널이란?

**핀셉트 터미널**현대적인 크로스 플랫폼 금융 터미널입니다.**고난**,**반응하다**, 그리고**타입스크립트**. 개인 투자자와 거래자에게 기관 수준의 재무 분석 도구를 완전 무료 오픈 소스로 제공합니다.

Bloomberg 및 Refinitiv에서 영감을 받은 Fincept Terminal은 기업 가격표 없이 실시간 시장 데이터, 고급 분석, AI 기반 통찰력 및 전문 인터페이스를 모두 제공합니다.

### 🌟 Fincept 터미널을 선택하는 이유는 무엇입니까?

| 전통적인 플랫폼       | 핀셉트 터미널             |
| -------------- | ------------------- |
| 💸 연간 $20,000+ | 🆓**무료 및 오픈 소스**    |
| 🏢 기업 전용       | 👤**누구나 이용 가능**     |
| 🔒 공급업체 잠금     | 💻**크로스 플랫폼 데스크탑**  |
| ⚙️ 제한된 사용자 정의  | 🎨**완전히 사용자 정의 가능** |

**기술 스택:**Tauri(Rust) • React 19 • TypeScript • TailwindCSS

* * *

## 🚀 시작하기

### **옵션 1: Microsoft Store에서 다운로드**🎉

<div align="center">

[![Get it from Microsoft Store](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/detail/XPDDZR13CXS466?hl=en-US&gl=IN&ocid=pdpshare)

**가장 쉬운 설치 • 자동 업데이트 • Windows 10/11**

</div>

### **옵션 2: 설치 프로그램 다운로드**

**윈도우:**

-   📦[MSI 설치 프로그램 다운로드](http://product.fincept.in/FinceptTerminalV2Alpha.msi)(윈도우 10/11)

**macOS 및 Linux:**

-   사전 구축된 설치 프로그램이 곧 출시될 예정입니다.

### **옵션 3: 소스에서 빌드**

#### 🚀**빠른 설정(자동)**

**Windows의 경우:**

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal

# 2. Run setup script (as Administrator)
setup.bat
```

**Linux/macOS의 경우:**

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal

# 2. Make script executable and run
chmod +x setup.sh
./setup.sh
```

자동화된 설정 스크립트는 다음을 수행합니다.

-   ✅ Node.js LTS(v22.14.0) 설치
-   ✅ Rust 설치(최신 안정 버전)
-   ✅ 프로젝트 종속성 설치
-   ✅ 모든 것을 자동으로 설정하세요

#### ⚙️**수동 설정**

**전제 조건:**Node.js 18+, Rust 1.70+, Git

```bash
# Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-terminal-desktop

# Install dependencies
npm install

# Run in development
npm run tauri dev

# Build for production
npm run tauri build
```

* * *

## ✨ 특징

<table>
<tr>
<td width="50%">

### 📊 시장 정보

🌍 글로벌 범위(주식, 외환, 암호화폐, 원자재)<br>📈 실시간 데이터 및 스트리밍 업데이트<br>📰 다중 소스 뉴스 통합<br>📉 맞춤 관심 목록

### 🧠 AI 기반 분석

🤖 GenAI 채팅 인터페이스<br>📊 실시간 감정 분석<br>💡 AI 기반 통찰력 및 권장 사항<br>🎯 자동 패턴 인식

</td>
<td width="50%">

### 📈 전문 도구

📊 고급 차트(50개 이상의 지표)<br>💼 회사 재무 및 연구<br>📋 다중 계정 포트폴리오 추적<br>⚡ 백테스팅 전략<br>🔔 맞춤형 가격 및 기술 알림

### 🌐 글로벌 인사이트

🏛️ 경제 데이터(이자율, GDP, 인플레이션)<br>🗺️ 무역로 및 해상 물류<br>🌍 지정학적 위험 평가

</td>
</tr>
</table>

**사용자 경험:**Bloomberg 스타일 UI • 기능 키 단축키(F1~F12) • 다크 모드 • 탭 기반 워크플로

* * *

## 🎬 플랫폼 미리보기

<div align="center">

|                                                채팅 모듈                                                |                                                      계기반                                                      |
| :-------------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------------: |
| ![Chat](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Chat.png) | ![Dashboard](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png) |
|                                             AI 기반 금융 보조원                                            |                                                   실시간 시장 개요                                                   |

|                                                     경제                                                    |                                                  주식 조사                                                  |
| :-------------------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------: |
| ![Economy](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Economy.png) | ![Equity](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png) |
|                                                 글로벌 경제 지표                                                 |                                                종합적인 주식 분석                                               |

|                                                   법정                                                  |                                                        지정학                                                        |
| :---------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------------------------------: |
| ![Forum](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Forum.png) | ![Geopolitics](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Geopolitics.png) |
|                                                커뮤니티 토론                                                |                                                    글로벌 위험 모니터링                                                    |

|                                                       글로벌 무역                                                      |                                                     시장                                                    |
| :---------------------------------------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------------------------------: |
| ![GlobalTrade](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/GlobalTrade.png) | ![Markets](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Markets.png) |
|                                                      국제 무역 흐름                                                     |                                                실시간 다중 자산 시장                                               |

|                                                          무역분석                                                         |
| :-------------------------------------------------------------------------------------------------------------------: |
| ![TradeAnalysis](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/TradeAnalysis.png) |
|                                                      고급 분석 및 백테스트                                                     |

</div>

* * *

## 🛣️ 로드맵

### **현황**

-   ✅ Tauri 애플리케이션 프레임워크
-   ✅ 인증 시스템(게스트+등록자)
-   ✅ Bloomberg 스타일 UI
-   ✅ 결제 통합
-   ✅ 포럼 기능
-   🚧 실시간 시장 데이터
-   🚧 고급 차트 작성
-   🚧 AI 비서

### **출시 예정(2025년 2분기)**

-   📊 완전한 시장 데이터 스트리밍
-   📈 50개 이상의 지표가 포함된 대화형 차트
-   🤖 프로덕션 AI 기능
-   💼 포트폴리오 관리
-   🔔 경고 시스템

### **미래**

-   🌍 다국어 지원
-   🏢 브로커 통합
-   📱 모바일 컴패니언 앱
-   🔌 플러그인 시스템
-   🎨 테마 마켓플레이스

* * *

## 🤝 기여하기

우리는 개발자, 거래자, 금융 전문가의 기여를 환영합니다!

**기여 방법:**

-   🐛 버그 및 문제 신고
-   💡 새로운 기능 제안
-   🔧 코드 제출(React, Rust, TypeScript)
-   📚 문서 개선
-   🎨 디자인 기여

**빠른 링크:**

-   [기여 지침](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/CONTRIBUTING.md)
-   [버그 신고](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new?template=bug_report.md)
-   [기능 요청](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new?template=feature_request.md)
-   [토론](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

**개발 설정:**

```bash
# Fork and clone
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal

# Automated setup (recommended for first-time contributors)
# Windows: run setup.bat as Administrator
# Linux/macOS: chmod +x setup.sh && ./setup.sh

# Or manual setup
cd fincept-terminal-desktop
npm install
npm run dev          # Start Vite dev server
npm run tauri dev    # Start Tauri app
```

* * *

## 🏗️ 프로젝트 구조

    fincept-terminal-desktop/
    ├── src/
    │   ├── components/      # React components (auth, dashboard, tabs, ui)
    │   ├── contexts/        # React Context (Auth, Theme)
    │   ├── services/        # API service layers
    │   └── hooks/           # Custom React hooks
    ├── src-tauri/
    │   ├── src/             # Rust backend
    │   ├── Cargo.toml       # Rust dependencies
    │   └── tauri.conf.json  # Tauri config
    └── package.json         # Node dependencies

* * *

## 📊 기술 세부정보

**성능:**

-   바이너리 크기: ~15MB
-   메모리: ~150MB(Electron의 경우 500MB 이상)
-   시작: &lt;2초

**플랫폼 지원:**

-   ✅ 윈도우 10/11(x64, ARM64)
-   ✅ macOS 11+(Intel, Apple Silicon)
-   ✅ 리눅스(우분투, 데비안, 페도라)

**보안:**

-   Tauri 샌드박스 환경
-   Node.js 런타임 없음
-   암호화된 자격 증명 저장소
-   HTTPS 전용 API 호출

* * *

## 📈 스타 히스토리

**⭐ 저장소에 별표를 표시하고 프로젝트를 공유하세요 ❤️**

<div align="center">
<a href="https://star-history.com/#Fincept-Corporation/FinceptTerminal&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
 </picture>
</a>
</div>

* * *

## 🌐 우리와 연결하세요

<div align="center">

[![GitHub Discussions](https://img.shields.io/badge/GitHub-Discussions-181717?style=for-the-badge&logo=github)](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)[![Email Support](https://img.shields.io/badge/Email-dev@fincept.in-D14836?style=for-the-badge&logo=gmail&logoColor=white)](mailto:dev@fincept.in)[![Contact Form](https://img.shields.io/badge/Contact-Form-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://forms.gle/DUsDHwxBNRVstYMi6)

**커뮤니티에 의해, 커뮤니티를 위해 구축됨**_모든 사람이 전문적인 재무 분석에 접근할 수 있도록 만들기_

⭐**우리를 스타**• 🔄**공유하다**• 🤝**기여하다**

</div>

* * *

## 📜 라이선스

MIT 라이센스 - 참조[라이센스.txt](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE.txt)

* * *

## 🙏 감사의 말씀

다음으로 제작:[고난](https://tauri.app/)•[반응하다](https://react.dev/)•[녹](https://www.rust-lang.org/)•[순풍CSS](https://tailwindcss.com/)•[기수 UI](https://www.radix-ui.com/)

* * *

**메모:**이전 버전은 Python/DearPyGUI로 빌드되었으며 다음 위치에 보관되어 있습니다.`legacy-python-depreciated/`. 현재 Tauri 기반 애플리케이션은 현대 기술로 완전히 재작성되었습니다.

* * *

<div align="center">

**© 2024-2025 Fincept Corporation • 오픈 소스 • MIT 라이센스**

</div>
