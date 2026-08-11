> [!IMPORTANT]
> **에디션은 두 가지.** **[Enterprise](https://fincept.in/enterprise)** 는 펀드와 리서치 데스크를 위한 비공개 클로즈드소스 빌드로, 41개 모듈·독점 데이터·실시간 브로커 라우팅·SSO를 갖추고 **사용자당 월 99달러**부터 시작합니다. **이 저장소**는 학습과 학술 용도의 무료 AGPL-3.0 에디션이며 월 1회 릴리스됩니다.
> [비교](https://fincept.in/comparison) · [요금제](https://fincept.in/pricing)

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

---

## 소개

**Fincept Terminal**은 금융 리서치를 위한 네이티브 C++20 데스크톱 터미널입니다. Qt6 UI, 내장 Python 3.11 분석 엔진, 단일 바이너리로 동작하며 Electron을 쓰지 않습니다.

하나의 데이터 코어 위에 두 에디션이 있습니다. **[Enterprise](https://fincept.in/enterprise)** 는 팀이 매일 개발하는 비공개 클로즈드소스 빌드로, 펀드·패밀리 오피스·리서치 데스크를 위한 것입니다. **이 저장소**는 학습·개인 사용·학술 연구를 위한 무료 AGPL-3.0 에디션이며 월 1회 릴리스됩니다.

학생·취미 사용자·연구자라면 오픈 빌드를 쓰세요. 회사이거나 터미널로 수익을 낸다면 Enterprise를 쓰세요. AGPL 카피레프트가 적용되지 않고, 오픈 빌드의 실제 비용은 상한 없이 토큰 단위로 청구되는 본인의 데이터·LLM 요금이기 때문입니다.

| | 오픈소스 | **Enterprise** |
|---|---|---|
| **라이선스** | AGPL-3.0 — 강한 카피레프트 | 독점 — 카피레프트 의무 없음 |
| **비용** | 무료, 단 데이터·LLM 요금은 본인 부담 | 사용자당 월 99 / 199 / 299달러 |
| **데이터** | 무료 공개 피드, 본인 API 키 | 독점 데이터셋, 더 긴 이력, 포인트 인 타임 |
| **AI** | 본인 LLM 키 | 400~5,000 크레딧 포함 · 멀티 에이전트 리서치 · 프라이빗 데이터룸 |
| **트레이딩** | 모의 매매 + 브로커 연동 | 실시간 브로커 라우팅 + 실시간 알고 배포 |
| **통제** | — | SSO/SAML, 감사 로그, RBAC, SLA 기반 지원 |

[**Enterprise 보기 →**](https://fincept.in/enterprise) · [전체 비교](https://fincept.in/comparison) · [요금제](https://fincept.in/pricing) · [FAQ](https://fincept.in/faq)

---

## Enterprise

여섯 개 데스크에 걸친 41개 모듈 — 에이전트 리서치, 퀀트 랩과 백테스팅, 심층 펀더멘털 리서치, 마켓과 실행, 매크로와 글로벌 인텔리전스, 그리고 나만의 워크스페이스. 모두 [700쪽 매뉴얼](https://fincept.in/manual)에 정리되어 있습니다.

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **월 99달러**/사용자 | **월 199달러**/사용자 | **월 299달러**/사용자 |
| AI 크레딧 / 월 | 400 | 2,000 | 5,000 |
| 딥 리서치 + 에이전트 팀 | — | ✓ | ✓ |
| 실시간 거래 + 알고 | — | — | ✓ |

월 단위 결제, 약정 없음, 최소 좌석 수 없음, 분기 결제 시 15% 할인 — 연간 **사용자당 1,188~3,588달러**로, 블룸버그 단말 한 좌석 약 27,000달러와 비교됩니다. **대학:** Exclusive Pro 5좌석 **월 699달러**. 이것이 전체 가격표이며, 협상 가격도 별도 상용 라이선스도 없습니다.

Enterprise는 별도 계정이 필요합니다 — 무료 Fincept 계정으로는 로그인되지 않습니다.

[**계정 만들기**](https://fincept.in/enterprise/signup) · [**데모 예약**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## 설치

**Windows x64**, **Linux x64**(`.run` / `.deb` / `.rpm`), **macOS(애플 실리콘)** 설치 파일은 [릴리스 페이지](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)에 있습니다.

**소스에서 빌드** — Linux/macOS: `git clone … && ./setup.sh`. Windows, 수동 빌드, 고정된 툴체인(**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**), 문제 해결은 **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)** 를 참고하세요. 버전은 고정되어 있으며 그보다 높거나 낮은 버전은 지원되지 않습니다.

> Enterprise 빌드를 찾으시나요? Windows, macOS, Linux용 서명된 설치 파일이 Enterprise 로그인 뒤에 준비되어 있습니다 — [여기서 받으세요](https://fincept.in/enterprise).

---

## 오픈 빌드에 포함된 것

- **분석** — DCF, 포트폴리오 최적화, VaR/샤프, 파생상품 가격 산정, 채권, 대체투자, 그리고 18개 모듈의 QuantLib 스위트
- **AI** — 트레이더/투자자, 경제, 지정학에 걸친 37개 에이전트. 키는 직접 준비(OpenAI, Anthropic, Gemini, Groq, DeepSeek, OpenRouter, Ollama)
- **데이터** — 100개 이상 커넥터: FRED, IMF, 세계은행, DBnomics, AkShare, Polygon, Kraken, Yahoo Finance, 정부 API
- **트레이딩** — 암호화폐·주식 피드, 모의 매매 엔진, 16개 브로커 연동
- **자동화** — 비주얼 노드 에디터, MCP 도구, AI Quant Lab(ML, 팩터 발굴, 강화학습)
- **글로벌 인텔리전스** — 해상 물류 추적, 지정학 분석, 관계 매핑

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

Fincept는 이 저장소에 대해 더 이상 별도의 상용 또는 학술 라이선스를 판매하지 않습니다. 상업적·기업·대학 용도는 위에 공개된 가격의 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** 가 담당합니다.

**상표.** "Fincept", "Fincept Terminal" 및 Fincept 로고는 Fincept Corporation의 상표입니다. 포크, 파생물, 리브랜딩 제품, 상용 제품에서 사용하려면 사전 서면 허가가 필요합니다.

문의: [support@fincept.in](mailto:support@fincept.in) · [이용약관](https://fincept.in/terms) · [개인정보처리방침](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. All rights reserved.

---

<div align="center">

### **한계는 당신의 사고뿐입니다. 데이터가 아니라.**

⭐ **스타** · 🔄 **공유** · 🤝 **기여**

<sub>영문 원본: <a href="../../README.md">README.md</a></sub>

</div>
