# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **한계는 당신의 사고뿐입니다. 데이터가 아니라.**

기관 수준의 금융 분석, AI 자동화, 무제한 데이터 연결을 갖춘 최첨단 금융 인텔리전스 플랫폼.

[📥 다운로드](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 요금제](https://fincept.in/pricing) · [📖 매뉴얼](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

> [!IMPORTANT]
> **Fincept Terminal은 두 가지 에디션으로 제공됩니다.**
>
> 🏢 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** — 펀드, 패밀리 오피스, 리서치 데스크를 위한 비공개 클로즈드소스 빌드. 41개 모듈, 독점 데이터셋, 멀티 에이전트 리서치, 실시간 브로커 라우팅, 프라이빗 데이터룸, SSO, SLA를 제공하며 **사용자당 월 99달러**부터. 팀이 매일 개발하는 쪽이 이 에디션이며, 터미널로 수익을 낸다면 이쪽을 쓰셔야 합니다.
>
> 📖 **이 저장소** — AGPL-3.0 기반의 무료 오픈소스 에디션. 학습, 개인 사용, 학술 연구용입니다. 계속 공개로 유지되고 삭제되지 않으며, **월 1회 릴리스**됩니다.
>
> [**어느 쪽이 필요한가요? →**](#어떤-에디션을-선택해야-할까) · [비교](https://fincept.in/comparison) · [요금제](https://fincept.in/pricing)

---

## 소개

**Fincept Terminal**은 금융 리서치를 위한 네이티브 C++20 데스크톱 터미널입니다. Qt6 UI, 내장 Python 3.11 분석 엔진, 단일 바이너리로 동작하며 Electron을 쓰지 않습니다.

**하나의 데이터 코어 위에 두 가지 에디션**으로 제공됩니다.

| | **오픈소스** — 이 저장소 | **Enterprise** — [fincept.in/enterprise](https://fincept.in/enterprise) |
|---|---|---|
| **대상** | 학습, 개인 사용, 학술 연구 | 펀드, 패밀리 오피스, 리서치 데스크 |
| **가격** | 무료 · AGPL-3.0 | **월 99달러**/사용자부터 |
| **릴리스 주기** | 월 1회 업데이트 | 상시 |

---

## 어떤 에디션을 선택해야 할까

| 당신이… | 선택 | 이유 |
|---|---|---|
| 학생, 취미 사용자, 독학하는 분 | **오픈소스** | 정말로 무료이며, 앞으로도 그렇습니다 |
| 학술 연구자 | **오픈소스** | 학술 용도는 무료 — 관리형 좌석이 필요한 대학은 아래 학술 번들 참고 |
| 펀드, 패밀리 오피스, 자기자본 데스크, 은행, 핀테크 | **Enterprise** | AGPL 카피레프트가 적용되지 않고, 독점 데이터·실시간 라우팅·SSO·SLA를 받습니다 |
| 이 일을 직업으로, 돈을 벌기 위해 하는 분 | **Enterprise** | 실질적으로 더 저렴하고, 매일 개발되는 유일한 에디션입니다 |

> **솔직히 말하면.** 오픈 빌드는 기능을 깎아낸 데모가 아니라 실제로 쓸 만한 제품입니다. 다만 무료 공개 피드와 **당신의** API 키, **당신의** LLM 키로 동작하며, 모든 AI 호출은 상한 없이 토큰 단위로 당신에게 청구됩니다. 커뮤니티 이슈는 최선을 다해 처리되지만 응답을 보장하지 않습니다. 터미널이 밥벌이 수단이라면 Enterprise가 더 싸고 안전한 선택입니다.

[**Enterprise 보기 →**](https://fincept.in/enterprise) · [전체 비교](https://fincept.in/comparison) · [요금제](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## 오픈소스 vs Enterprise

| | 오픈소스 | **Enterprise** |
|---|---|---|
| **라이선스** | AGPL-3.0 — 강한 카피레프트. 배포하거나 호스팅하면 변경 사항을 공개해야 합니다 | 독점 — 관리할 카피레프트 의무 없음 |
| **비용** | 무료 — 데이터와 LLM 비용은 본인 부담 | 사용자당 월 99 / 199 / 299달러, 전부 포함 |
| **모듈** | 코어 터미널 | **6개 데스크, 41개 모듈** |
| **데이터** | 무료 공개 피드, 요청 제한 있음, 키는 직접 준비 | 독점 데이터셋 — 더 긴 이력, 더 빠른 갱신 |
| **가격 이력** | 무료 등급이 허용하는 범위 | 1년 / 5년 / 무제한 · 정직한 백테스트를 위한 **포인트 인 타임** |
| **AI 예산** | 본인 LLM 키, 토큰 단위로 본인 청구 | 매월 400 / 2,000 / 5,000 크레딧 포함 |
| **AI 리서치** | 기본 마켓 어시스턴트 | **계획하고 위임하는** 멀티 에이전트 리서치 · Agent Studio · 최대 53개 에이전트 · 6가지 팀 모드 |
| **프라이빗 데이터룸** | — | 공시 자료와 모델을 본인 에이전트만 읽습니다. 타사와 섞이지 않고 학습에도 쓰이지 않습니다 |
| **트레이딩** | 모의 매매 + 본인 키로 브로커 연동 | **실시간 브로커 라우팅 + 실시간 알고 배포** |
| **퀀트** | 커뮤니티 백테스팅 | Quant Lab, Alpha Arena, 변동성 분석, 포인트 인 타임 백테스트 |
| **보안·컴플라이언스** | — | SSO/SAML, 감사 로그, 역할 기반 접근 제어, 데이터 격리 |
| **지원** | GitHub 이슈, 최선 노력, 응답 보장 없음 | SLA 기반 우선 대응 |
| **문서** | 이 저장소 | [**700쪽 매뉴얼**](https://fincept.in/manual) — 41개 가이드, 472개 섹션 |
| **플랫폼** | Windows, macOS, Linux + 호스팅 웹 터미널 | Windows 10/11, macOS 13+ (애플 실리콘), Linux |

Enterprise는 **별도 계정**으로 동작합니다. 무료 Fincept 계정으로는 로그인되지 않으며, 구독이 연결될 때까지 앱은 잠긴 상태로 유지됩니다. [Enterprise 계정 만들기 →](https://fincept.in/enterprise/signup)

---

## Enterprise — 6개 데스크, 41개 모듈

| 데스크 | 하는 일 |
|---|---|
| **Agentic Research** · 6 | 에이전트가 작업을 계획하고 전문 에이전트에 위임하며, 실시간 데이터와 데이터룸을 읽어 출처가 달린 노트를 작성합니다 |
| **Quant Lab & Backtesting** · 4 | 시그널 리서치, 포인트 인 타임 백테스트, 변동성 곡면 |
| **Deep Fundamental Research** · 8 | 주식 분석, 밸류에이션, 파생상품, M&A 분석 |
| **Markets & Execution** · 7 | 주식·암호화폐·예측시장 실시간 거래, 브로커 라우팅, 알고 배포 |
| **Macro & Global Intelligence** · 7 | 통계, 정부 데이터, 지정학, 해상 항로 |
| **나만의 워크스페이스** · 9 | 대시보드, 스프레드시트, 노트, 파일, 코드, 리포트 빌더 |

[제품 살펴보기 →](https://fincept.in/products)

### 요금제

| | **Exclusive** | **Exclusive+** ★ 가장 인기 | **Exclusive Pro** |
|---|---|---|---|
| | **월 99달러**/사용자 | **월 199달러**/사용자 | **월 299달러**/사용자 |
| AI 크레딧 / 월 | 400 | 2,000 | 5,000 |
| 포트폴리오 · 관심목록 | 1 · 3 | 10 · 25 | 무제한 |
| 가격 이력 | 1년 | 5년 | 무제한 |
| 포함 좌석 | 1 | 1 | 2 |
| 딥 리서치 + 에이전트 팀 | — | ✓ | ✓ |
| 브로커 계좌 연동 | — | ✓ | ✓ |
| 실시간 브로커 거래 + 알고 배포 | — | — | ✓ |

월 단위 결제 · 연간 약정 없음 · 최소 좌석 수 없음 · **분기 결제 시 15% 할인**. 연간 **사용자당 1,188~3,588달러**로, 블룸버그 단말 한 좌석의 약 **27,000달러**와 비교됩니다 — [전체 비교 보기](https://fincept.in/comparison).

**대학 및 학술 기관:** 단일 번들 — **Exclusive Pro 5좌석을 월 699달러**(정가 1,495달러). [support@fincept.in](mailto:support@fincept.in)으로 문의하세요.

이 세 가지 요금제와 학술 번들이 전체 가격표입니다. 개별 협상형 엔터프라이즈 가격은 없으며, 따로 구매하는 상용 라이선스도 없습니다.

[**Enterprise 계정 만들기**](https://fincept.in/enterprise/signup) · [**데모 예약**](https://calendly.com/nikultilak/fincept-terminal-demo) · [매뉴얼 읽기](https://fincept.in/manual)

---

## 오픈소스 에디션 설치

**Windows x64**, **Linux x64**(`.run` / `.deb` / `.rpm`), **macOS(애플 실리콘)** 설치 파일은 [릴리스 페이지](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)에 있습니다.

**소스에서 빌드** — Linux/macOS: `git clone … && ./setup.sh`. Windows, 수동 빌드, 고정된 툴체인(**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**), 문제 해결은 **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)** 를 참고하세요. 버전은 고정되어 있으며 그보다 높거나 낮은 버전은 지원되지 않습니다.

> Enterprise 빌드를 찾으시나요? Windows, macOS, Linux용 서명된 설치 파일이 Enterprise 로그인 뒤에 준비되어 있습니다 — [여기서 받으세요](https://fincept.in/enterprise).

---

## 오픈 빌드에 포함된 것

| | |
|---|---|
| **멀티에셋 분석** | DCF, 포트폴리오 최적화, VaR/샤프, 파생상품 가격 산정, 채권, 대체투자 — 내장 Python 기반 |
| **QuantLib 스위트** | 18개 퀀트 모듈 — 가격 산정, 리스크, 확률과정, 변동성, 채권 |
| **AI 에이전트** | 트레이더/투자자, 경제, 지정학 프레임워크에 걸친 37개 에이전트. 키는 직접 준비(OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama) |
| **100개 이상 데이터 커넥터** | DBnomics, FRED, IMF, 세계은행, AkShare, Polygon, Kraken, Yahoo Finance, 정부 API |
| **트레이딩** | 암호화폐·주식 피드, 모의 매매 엔진, 16개 브로커 연동 |
| **자동화** | 비주얼 노드 에디터, MCP 도구 연동, AI Quant Lab(ML, 팩터 발굴, 강화학습) |
| **글로벌 인텔리전스** | 해상 물류 추적, 지정학 분석, 관계 매핑 |

네이티브 C++20 · Qt6 · 내장 Python 3.11 · 단일 바이너리 · Node.js도 브라우저 런타임도 없음.

---

## 이 저장소의 유지보수 방식

이 저장소는 **계속 공개되며 삭제되지 않습니다**. 이미 릴리스된 것은 그대로 남습니다.

이제 상시 개발이 아니라 **월 1회 릴리스**로 운영됩니다. 팀의 일상 작업이 Enterprise에 있기 때문입니다. 이슈와 풀 리퀘스트는 계속 검토되며, 수정 사항은 월간 주기로 반영됩니다. 보안 제보는 [support@fincept.in](mailto:support@fincept.in)으로 보내주세요.

---

## 기여하기

새로운 데이터 커넥터, AI 에이전트, 분석 모듈, C++ 화면, 문서 모두 환영합니다.

[기여 가이드](../CONTRIBUTING.md) · [C++ 가이드](../CPP_CONTRIBUTOR_GUIDE.md) · [Python 가이드](../PYTHON_CONTRIBUTOR_GUIDE.md) · [아키텍처](../ARCHITECTURE.md) · [버그 신고](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [기능 제안](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## Fincept의 다른 제품

- **[Fincept Data API](https://docs.fincept.in)** — 500개 이상 REST 엔드포인트, 423,000개 이상 종목, 2,000개 이상 소스. 모든 계정에 무료 등급 포함.
- **[Quantcept](https://quantcept.io)** — 오픈소스 AI 기반 커맨드라인 금융 터미널(Apache-2.0).

---

## 라이선스

**AGPL-3.0-or-later** — 전문은 [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)에 있습니다.

개인 사용, 학습, 학술 연구에는 무료입니다. AGPL-3.0은 **관대한 라이선스가 아니라 강한 카피레프트**입니다. 수정한 빌드를 배포하거나 다른 사람이 접근하는 서비스로 운영하면, 변경 사항을 동일한 라이선스로 공개해야 합니다. 대부분의 법무팀에게는 이 한 줄에서 논의가 끝납니다. 그래서 기업들은 독점 라이선스이며 관리할 카피레프트 의무가 없는 **[Enterprise](https://fincept.in/enterprise)** 를 선택합니다. 배포하지 않는 개인 사용에는 아무 의무도 없습니다.

Fincept는 이 저장소에 대해 **더 이상 별도의 상용 또는 학술 라이선스를 판매하지 않습니다**. 상업적·기관·대학 용도는 위에 공개된 가격의 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** 가 담당합니다.

**상표.** "Fincept", "Fincept Terminal" 및 Fincept 로고는 Fincept Corporation의 상표입니다. 포크, 파생물, 리브랜딩 제품, 상용 제품에서 사용하려면 사전 서면 허가가 필요합니다.

문의: [support@fincept.in](mailto:support@fincept.in) · [이용약관](https://fincept.in/terms) · [개인정보처리방침](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. All rights reserved.

---

<div align="center">

### **한계는 당신의 사고뿐입니다. 데이터가 아니라.**

⭐ **스타** · 🔄 **공유** · 🤝 **기여**

<sub>영문 원본: <a href="../../README.md">README.md</a></sub>

</div>
