> [!IMPORTANT]
> **兩種版本。** **[Enterprise](https://fincept.in/enterprise)** 是為基金與研究團隊打造的私有閉源版本 —— 41 個模組、專有資料、即時券商路由、SSO，**每位使用者每月 99 美元**起。**本儲存庫**是供學習與學術用途的免費 AGPL-3.0 版本，每月發布一次。
> [比較](https://fincept.in/comparison) · [價格](https://fincept.in/pricing)

# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **唯一的上限是你的思考，而不是資料。**

面向機構級金融分析、AI 自動化與無限資料連接的前沿金融智慧平台。

[📥 下載](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 價格](https://fincept.in/pricing) · [📖 手冊](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

---

## 關於

**Fincept Terminal** 是一款用於金融研究的原生 C++20 桌面終端機 —— Qt6 介面、內嵌 Python 3.11 分析引擎、單一執行檔，不依賴 Electron。

兩個版本執行在同一套資料核心之上。**[Enterprise](https://fincept.in/enterprise)** 是團隊日常開發的私有閉源版本，面向基金、家族辦公室與研究團隊。**本儲存庫**是免費的 AGPL-3.0 版本 —— 學習、個人使用、學術研究 —— 每月發布一次。

如果你是學生、愛好者或學術研究者，用開源版。如果你是機構，或者靠終端機賺錢，用 Enterprise：AGPL 的傳染性條款不適用，而開源版的真實成本是你自己的資料與 LLM 帳單，按 token 計費且沒有上限。

| | 開源版 | **Enterprise** |
|---|---|---|
| **授權條款** | AGPL-3.0 —— 強傳染性 | 專有 —— 無 copyleft 義務 |
| **成本** | 免費，另加自付的資料與 LLM 帳單 | 每位使用者每月 99 / 199 / 299 美元 |
| **資料** | 免費公開資料來源，需自備金鑰 | 專有資料集、更長歷史、時點資料 |
| **AI** | 自備 LLM 金鑰 | 含 400–5,000 點數 · 多代理研究 · 私有資料室 |
| **交易** | 模擬交易 + 券商串接 | 即時券商路由 + 即時演算法部署 |
| **管控** | — | SSO/SAML、稽核紀錄、RBAC、SLA 保障支援 |

[**了解 Enterprise →**](https://fincept.in/enterprise) · [完整比較](https://fincept.in/comparison) · [價格](https://fincept.in/pricing) · [常見問題](https://fincept.in/faq)

---

## Enterprise

六大業務台、41 個模組 —— 代理研究、量化實驗室與回測、深度基本面研究、市場與執行、總體與全球情報，以及你的專屬工作區。全部收錄於一本 [700 頁手冊](https://fincept.in/manual)。

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **99 美元**/使用者/月 | **199 美元**/使用者/月 | **299 美元**/使用者/月 |
| AI 點數 / 月 | 400 | 2,000 | 5,000 |
| 深度研究 + 代理團隊 | — | ✓ | ✓ |
| 即時交易 + 演算法 | — | — | ✓ |

按月計費、無綁約、無最低席次、按季付款享 85 折 —— 折合**每位使用者每年 1,188–3,588 美元**，而一個彭博終端機席次約為 27,000 美元。**大專院校：** 5 個 Exclusive Pro 席次每月 **699 美元**。這就是完整價目表：沒有議價報價，也沒有另外販售的商業授權。

Enterprise 需要獨立帳號 —— 免費 Fincept 帳號無法登入。

[**建立帳號**](https://fincept.in/enterprise/signup) · [**預約導覽**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## 安裝

**Windows x64**、**Linux x64**（`.run` / `.deb` / `.rpm`）與 **macOS（Apple 晶片）**的安裝檔請見 [Releases 頁面](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)。

**從原始碼建置** —— Linux/macOS：`git clone … && ./setup.sh`。Windows、手動建置、鎖定的工具鏈（**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**）與疑難排解請見 **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**。版本已鎖定，更新或更舊的版本皆不支援。

> 在找 Enterprise 版本嗎？它有專屬的簽章安裝檔，支援 Windows、macOS 與 Linux，需以 Enterprise 帳號登入取得 —— [由此取得](https://fincept.in/enterprise)。

---

## 開源版包含哪些功能

- **分析** —— DCF、投資組合最佳化、VaR/夏普值、衍生性商品定價、固定收益、另類資產，外加 18 個模組的 QuantLib 套件
- **AI** —— 涵蓋交易員/投資人、經濟與地緣政治的 37 個代理；需自備金鑰（OpenAI、Anthropic、Gemini、Groq、DeepSeek、OpenRouter、Ollama）
- **資料** —— 100 多個連接器：FRED、IMF、世界銀行、DBnomics、AkShare、Polygon、Kraken、Yahoo Finance、政府 API
- **交易** —— 加密貨幣與股票行情、模擬交易引擎、16 家券商串接
- **自動化** —— 視覺化節點編輯器、MCP 工具、AI Quant Lab（機器學習、因子挖掘、強化學習）
- **全球情報** —— 海運追蹤、地緣政治分析、關係圖譜

原生 C++20 · Qt6 · 內嵌 Python 3.11 · 單一執行檔 · 不需 Node.js、不需瀏覽器執行環境。

---

## 本儲存庫的維護方式

本儲存庫**會持續公開，不會被刪除**。已經發布的內容都會保留。

現在改為**每月發布一次**，而非持續開發，因為團隊的日常工作在 Enterprise 上。Issue 與 Pull Request 仍會審閱，修正會依月度週期釋出。資安問題請回報至 [support@fincept.in](mailto:support@fincept.in)。

---

## 參與貢獻

歡迎提交新的資料連接器、AI 代理、分析模組、C++ 畫面與文件。

[貢獻指南](../CONTRIBUTING.md) · [C++ 指南](../CPP_CONTRIBUTOR_GUIDE.md) · [Python 指南](../PYTHON_CONTRIBUTOR_GUIDE.md) · [架構說明](../ARCHITECTURE.md) · [回報錯誤](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [提出需求](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## Fincept 的其他產品

- **[Fincept Data API](https://docs.fincept.in)** —— 500 多個 REST 端點、423,000 多檔標的、2,000 多個資料來源。任何帳號皆含免費額度。
- **[Quantcept](https://quantcept.io)** —— 開源的 AI 驅動命令列金融終端機（Apache-2.0）。

---

## 授權條款

**AGPL-3.0-or-later** —— 全文見 [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)。

個人使用、學習與學術研究免費。AGPL-3.0 是**強傳染性授權，而非寬鬆授權**：若你散布修改過的版本，或將其作為他人可存取的服務執行，就必須以相同授權公開你的修改。對多數法務團隊而言，討論到這一條就結束了 —— 這也是企業選擇 **[Enterprise](https://fincept.in/enterprise)** 的原因：它是專有軟體，沒有任何 copyleft 義務需要處理。不涉及散布的個人使用則沒有任何義務。

對於本儲存庫，Fincept 不再販售單獨的商業或學術授權。商業、企業與大專院校需求由 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** 依上述公開價格提供。

**商標。** 「Fincept」、「Fincept Terminal」及 Fincept 標誌均為 Fincept Corporation 的商標。在任何分支、衍生、改名或商業產品中使用，皆須事先取得書面許可。

洽詢：[support@fincept.in](mailto:support@fincept.in) · [服務條款](https://fincept.in/terms) · [隱私權政策](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. 保留一切權利。

---

<div align="center">

### **唯一的上限是你的思考，而不是資料。**

⭐ **加星** · 🔄 **分享** · 🤝 **貢獻**

<sub>英文原版：<a href="../../README.md">README.md</a></sub>

</div>
