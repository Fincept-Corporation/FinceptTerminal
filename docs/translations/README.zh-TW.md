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

> [!IMPORTANT]
> **Fincept Terminal 提供兩種版本。**
>
> 🏢 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** — 為基金、家族辦公室與研究團隊打造的私有閉源版本。41 個模組、專有資料集、多代理研究、即時券商路由、私有資料室、SSO 與 SLA，**每位使用者每月 99 美元**起。團隊的日常開發都在這個版本上；若終端機是你的收入來源，就該用它。
>
> 📖 **本儲存庫** — 採用 AGPL-3.0 的免費開源版本，適合學習、個人使用與學術研究。儲存庫會持續公開、不會刪除，並維持**每月一次發布**。
>
> [**你需要哪一個？ →**](#應該選擇哪個版本) · [比較](https://fincept.in/comparison) · [價格](https://fincept.in/pricing)

---

## 關於

**Fincept Terminal** 是一款用於金融研究的原生 C++20 桌面終端機 —— Qt6 介面、內嵌 Python 3.11 分析引擎、單一執行檔，不依賴 Electron。

它在**同一套資料核心上提供兩個版本**：

| | **開源版** — 本儲存庫 | **Enterprise** — [fincept.in/enterprise](https://fincept.in/enterprise) |
|---|---|---|
| **適用對象** | 學習、個人使用、學術研究 | 基金、家族辦公室、研究團隊 |
| **價格** | 免費 · AGPL-3.0 | **每位使用者每月 99 美元**起 |
| **發布節奏** | 每月一次更新 | 持續更新 |

---

## 應該選擇哪個版本

| 如果你是…… | 選擇 | 原因 |
|---|---|---|
| 學生、愛好者或自學者 | **開源版** | 真正免費，而且會一直免費 |
| 學術研究人員 | **開源版** | 學術用途免費 —— 需要統一管理席次的大專院校請見下方學術方案 |
| 基金、家族辦公室、自營交易台、銀行或金融科技公司 | **Enterprise** | 不適用 AGPL 的傳染性條款，並可取得專有資料、即時路由、SSO 與 SLA |
| 以此為職業、靠它賺錢的人 | **Enterprise** | 實際成本更低，而且是唯一每日持續開發的版本 |

> **老實說。** 開源版是真材實料，不是閹割版的展示品 —— 但它跑在免費公開資料來源上，用的是**你的** API 金鑰與**你的** LLM 金鑰，每一次 AI 呼叫都按 token 計入你的帳單，沒有上限。社群 issue 會盡力處理，但不承諾回覆時間。若終端機是你的謀生工具，Enterprise 才是更便宜也更穩妥的席次。

[**了解 Enterprise →**](https://fincept.in/enterprise) · [完整比較](https://fincept.in/comparison) · [價格](https://fincept.in/pricing) · [常見問題](https://fincept.in/faq)

---

## 開源版與 Enterprise 比較

| | 開源版 | **Enterprise** |
|---|---|---|
| **授權條款** | AGPL-3.0 —— 強傳染性。一旦散布或託管，就必須公開你的修改 | 專有授權 —— 無需處理任何 copyleft 義務 |
| **成本** | 免費 —— 資料與 LLM 帳單自付 | 每位使用者每月 99 / 199 / 299 美元，全部包含 |
| **模組** | 核心終端機 | **6 個業務台、41 個模組** |
| **資料** | 免費公開資料來源，有流量限制，需自備金鑰 | 私有與專有資料集 —— 歷史更長、更新更快 |
| **價格歷史** | 取決於免費額度 | 1 年 / 5 年 / 無限 · 提供**時點資料**，回測更可信 |
| **AI 預算** | 自備 LLM 金鑰，按 token 自付 | 每月含 400 / 2,000 / 5,000 點數 |
| **AI 研究** | 基礎行情助理 | 會**規劃並分派任務**的多代理研究 · Agent Studio · 最多 53 個代理 · 6 種團隊模式 |
| **私有資料室** | — | 你的文件與模型僅由你自己的代理讀取 —— 不與他人混用，也不用於訓練 |
| **交易** | 模擬交易 + 自備金鑰的券商串接 | **即時券商路由 + 即時演算法部署** |
| **量化** | 社群回測 | Quant Lab、Alpha Arena、波動率分析、時點回測 |
| **資安與法遵** | — | SSO/SAML、稽核紀錄、角色權限控管、資料隔離 |
| **支援** | GitHub issue，盡力而為，不承諾回覆 | 有 SLA 保障的優先處理 |
| **文件** | 本儲存庫 | [**700 頁手冊**](https://fincept.in/manual) —— 41 篇指南、472 個章節 |
| **平台** | Windows、macOS、Linux + 託管網頁終端機 | Windows 10/11、macOS 13+（Apple 晶片）、Linux |

Enterprise 使用**獨立帳號** —— 免費 Fincept 帳號無法登入，且在綁定訂閱之前應用程式會保持鎖定。[建立 Enterprise 帳號 →](https://fincept.in/enterprise/signup)

---

## Enterprise —— 六大業務台，41 個模組

| 業務台 | 功能 |
|---|---|
| **Agentic Research** · 6 | 代理規劃工作、分派給專職代理，讀取即時資料與你的資料室，回傳附來源的研究筆記 |
| **Quant Lab & Backtesting** · 4 | 訊號研究、時點回測、波動率曲面 |
| **Deep Fundamental Research** · 8 | 股票分析、估值、衍生性商品、併購分析 |
| **Markets & Execution** · 7 | 股票、加密貨幣與預測市場即時交易，券商路由，演算法部署 |
| **Macro & Global Intelligence** · 7 | 統計資料、政府資料、地緣政治、航運航線 |
| **你的專屬工作區** · 9 | 儀表板、試算表、筆記、檔案、程式碼、報告產生器 |

[瀏覽產品 →](https://fincept.in/products)

### 價格

| | **Exclusive** | **Exclusive+** ★ 最受歡迎 | **Exclusive Pro** |
|---|---|---|---|
| | **99 美元**/使用者/月 | **199 美元**/使用者/月 | **299 美元**/使用者/月 |
| AI 點數 / 月 | 400 | 2,000 | 5,000 |
| 投資組合 · 自選清單 | 1 · 3 | 10 · 25 | 無限 |
| 價格歷史 | 1 年 | 5 年 | 無限 |
| 內含席次 | 1 | 1 | 2 |
| 深度研究 + 代理團隊 | — | ✓ | ✓ |
| 券商帳戶綁定 | — | ✓ | ✓ |
| 即時券商交易 + 演算法部署 | — | — | ✓ |

按月計費 · 無年度綁約 · 無最低席次 · **按季付款享 85 折**。折合**每位使用者每年 1,188–3,588 美元**，而一個彭博終端機席次約為 **27,000 美元** —— [查看完整比較](https://fincept.in/comparison)。

**大專院校與學術機構：** 單一方案 —— **5 個 Exclusive Pro 席次每月 699 美元**（原價 1,495 美元）。請來信 [support@fincept.in](mailto:support@fincept.in)。

這三種方案加上學術方案就是完整價目表。沒有客製或議價的企業報價，也不再另外販售商業授權。

[**建立 Enterprise 帳號**](https://fincept.in/enterprise/signup) · [**預約導覽**](https://calendly.com/nikultilak/fincept-terminal-demo) · [閱讀手冊](https://fincept.in/manual)

---

## 安裝開源版

**Windows x64**、**Linux x64**（`.run` / `.deb` / `.rpm`）與 **macOS（Apple 晶片）**的安裝檔請見 [Releases 頁面](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)。

**從原始碼建置** —— Linux/macOS：`git clone … && ./setup.sh`。Windows、手動建置、鎖定的工具鏈（**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**）與疑難排解請見 **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**。版本已鎖定，更新或更舊的版本皆不支援。

> 在找 Enterprise 版本嗎？它有專屬的簽章安裝檔，支援 Windows、macOS 與 Linux，需以 Enterprise 帳號登入取得 —— [由此取得](https://fincept.in/enterprise)。

---

## 開源版包含哪些功能

| | |
|---|---|
| **多資產分析** | DCF、投資組合最佳化、VaR/夏普值、衍生性商品定價、固定收益、另類資產 —— 透過內嵌 Python 實作 |
| **QuantLib 套件** | 18 個量化模組 —— 定價、風險、隨機過程、波動率、固定收益 |
| **AI 代理** | 涵蓋交易員/投資人、經濟與地緣政治框架的 37 個代理；需自備金鑰（OpenAI、Anthropic、Gemini、Groq、DeepSeek、OpenRouter、Ollama） |
| **100+ 資料連接器** | DBnomics、FRED、IMF、世界銀行、AkShare、Polygon、Kraken、Yahoo Finance、政府 API |
| **交易** | 加密貨幣與股票行情、模擬交易引擎、16 家券商串接 |
| **自動化** | 視覺化節點編輯器、MCP 工具整合、AI Quant Lab（機器學習、因子挖掘、強化學習） |
| **全球情報** | 海運追蹤、地緣政治分析、關係圖譜 |

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

對於本儲存庫，Fincept **不再販售單獨的商業或學術授權**。商業、機構與大專院校需求由 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** 依上述公開價格提供。

**商標。** 「Fincept」、「Fincept Terminal」及 Fincept 標誌均為 Fincept Corporation 的商標。在任何分支、衍生、改名或商業產品中使用，皆須事先取得書面許可。

洽詢：[support@fincept.in](mailto:support@fincept.in) · [服務條款](https://fincept.in/terms) · [隱私權政策](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. 保留一切權利。

---

<div align="center">

### **唯一的上限是你的思考，而不是資料。**

⭐ **加星** · 🔄 **分享** · 🤝 **貢獻**

<sub>英文原版：<a href="../../README.md">README.md</a></sub>

</div>
