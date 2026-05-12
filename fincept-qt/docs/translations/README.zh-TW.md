# FinceptTerminal — 台灣股市資料連接器

> **本專案是 [FinceptTerminal](https://github.com/Fincept-Corporation/FinceptTerminal) 的開源社群貢獻分支。**
> 原始專案版權歸 Fincept Corporation 所有，本修改依照 AGPL-3.0 授權條款釋出。
> 本分支無任何商業目的，僅供學習與研究使用。

---

## 📋 目錄

1. [這個分支做了什麼？](#這個分支做了什麼)
2. [授權說明（重要，請先閱讀）](#授權說明重要請先閱讀)
3. [安裝方式](#安裝方式)
4. [啟用台灣股市連接器](#啟用台灣股市連接器)
5. [功能說明與程式碼範例](#功能說明與程式碼範例)
6. [支援的股票代碼](#支援的股票代碼)
7. [資料來源說明](#資料來源說明)
8. [常見問題 FAQ](#常見問題-faq)
9. [如何貢獻](#如何貢獻)
10. [致謝](#致謝)

---

## 這個分支做了什麼？

原版 FinceptTerminal 的股市資料連接器主要以美股（NYSE / NASDAQ）為預設市場。

本分支在 `fincept-qt/src/python/` 目錄下新增了一個 Python 檔案：

```
fincept-qt/src/python/taiwan_market_connector.py
```

這個連接器讓你可以在 FinceptTerminal 中直接查詢：

| 功能 | 說明 |
|---|---|
| 個股歷史價格 | OHLCV 日/週/月K，支援上市（TWSE）與上櫃（TPEX） |
| 個股公司資訊 | 名稱、產業、本益比、股價淨值比、殖利率、52週高低點 |
| 大盤指數 | 台灣加權指數（TAIEX）、櫃買指數 |
| 市場概況 | 大盤成交量、成交值、漲跌家數（來自 TWSE OpenAPI） |
| 強勢股排行 | 當日漲幅最大個股列表 |
| 股票搜尋 | 依代碼或名稱搜尋台灣主要股票 |

**重要：本修改僅涉及 Python 層，不需要重新編譯 C++ 程式碼。**

---

## 授權說明（重要，請先閱讀）

### 原始專案授權

FinceptTerminal 採用雙授權制：

- **AGPL-3.0**：適用於個人、學術與開源用途（免費）
- **Fincept 商業授權**：任何商業用途皆需取得此授權

### 本分支的授權立場

本分支依照 **AGPL-3.0** 條款釋出，這表示：

✅ **你可以**：
- 免費下載、使用、修改本程式碼
- 將本修改再分享給其他人（須保持相同授權）
- 用於個人學習、學術研究、非商業專案

❌ **你不可以**：
- 將本程式碼或衍生作品用於任何商業目的（需另行取得 Fincept 商業授權）
- 移除原始版權聲明
- 以「Fincept」、「Fincept Terminal」等商標命名你的衍生產品

### 我們如何確保合規？

1. 本分支的 `LICENSE` 檔案完整保留 AGPL-3.0 全文
2. 每個修改的檔案頭部都有明確的原始來源聲明
3. README 頂部清楚標示這是社群貢獻分支，非官方版本
4. 若你有商業需求，請聯絡 Fincept Corporation：support@fincept.in

---

## 安裝方式

### 前置需求

本分支與原版相同，需要：
- Python 3.11.9（由 CMake 自動管理）
- Qt 6.8.3
- CMake 3.27+
- C++20 編譯器（MSVC 2022 / GCC 12+ / Apple Clang 15+）

### 方法一：直接使用預編譯版本（推薦新手）

本分支的 Python 連接器**不需要重新編譯** C++ 程式碼。
你可以先從[官方發布頁面](https://github.com/Fincept-Corporation/FinceptTerminal/releases)下載預編譯版本安裝後，再手動加入本連接器檔案。

```bash
# 1. 下載並安裝官方預編譯版本（Windows / macOS / Linux）
# 從 https://github.com/Fincept-Corporation/FinceptTerminal/releases 下載

# 2. 複製台灣股市連接器到 Python 層目錄
cp taiwan_market_connector.py 路徑/到/fincept-qt/src/python/
```

### 方法二：從原始碼建置（開發者）

```bash
# Clone 本分支
git clone https://github.com/你的帳號/FinceptTerminal.git
cd FinceptTerminal

# Linux / macOS — 一鍵建置
chmod +x setup.sh && ./setup.sh

# Windows（在 VS 2022 Developer Command Prompt 執行）
setup.bat
```

### 安裝 Python 相依套件

連接器使用以下套件，通常 FinceptTerminal 環境已內建：

```bash
pip install yfinance requests pandas
```

若要使用 `twstock` 進行更完整的即時分析：

```bash
pip install twstock
```

---

## 啟用台灣股市連接器

### 步驟 1：複製連接器檔案

將 `taiwan_market_connector.py` 放入：

```
FinceptTerminal/
└── fincept-qt/
    └── src/
        └── python/
            └── taiwan_market_connector.py   ← 放在這裡
```

### 步驟 2：在連接器登錄中註冊

找到 `fincept-qt/src/python/` 目錄中的連接器登錄檔案（通常是 `data_sources.py` 或 `connectors/__init__.py`），加入以下程式碼：

```python
from .taiwan_market_connector import CONNECTOR_META as TW_META

# 在現有連接器字典中加入
AVAILABLE_CONNECTORS["taiwan_twse"] = TW_META
```

### 步驟 3：在應用程式介面中啟用

啟動 FinceptTerminal 後：
1. 前往 **Data Sources（資料來源）** 設定頁面
2. 找到 **Taiwan Stock Exchange (TWSE / TPEX)**
3. 點擊啟用
4. 回到主畫面即可使用台灣股市功能

---

## 功能說明與程式碼範例

以下範例可在 Python 直譯器中直接執行，用於驗證連接器是否正常運作。

### 查詢個股歷史價格

```python
from taiwan_market_connector import get_tw_stock_history

# 查詢台積電（上市股票，加 .TW 後綴）
df = get_tw_stock_history("2330", start="2024-01-01", market="TWSE")
print(df.tail(10))

# 查詢上櫃股票（加 .TWO 後綴）
df_otc = get_tw_stock_history("6669", start="2024-01-01", market="TPEX")
print(df_otc.tail(5))

# 查詢週K資料
df_weekly = get_tw_stock_history("2330", start="2023-01-01", interval="1wk")
print(df_weekly)
```

**輸出範例：**
```
            Open    High     Low   Close    Volume
Date
2025-04-28  780.0  790.0  778.0  788.0  28500000
2025-04-29  789.0  795.0  785.0  792.0  31200000
...
```

---

### 查詢公司基本資訊

```python
from taiwan_market_connector import get_tw_stock_info

info = get_tw_stock_info("2330")
for key, value in info.items():
    print(f"{key:<15}: {value}")
```

**輸出範例：**
```
name           : Taiwan Semiconductor Manufacturing Company Limited
symbol         : 2330
exchange       : TWSE
sector         : Technology
market_cap     : 15280000000000
currency       : TWD
pe_ratio       : 22.4
pb_ratio       : 5.8
div_yield      : 0.018
52w_high       : 1080.0
52w_low        : 688.0
current_price  : 788.0
```

---

### 查詢大盤指數

```python
from taiwan_market_connector import get_tw_index

# 台灣加權指數（TAIEX）
taiex = get_tw_index("^TWII", period="3mo")
print(taiex.tail(5))

# 上櫃指數
tpex_idx = get_tw_index("^TWOII", period="1mo")
print(tpex_idx.tail(5))
```

---

### 查詢市場概況

```python
from taiwan_market_connector import get_tw_market_overview

overview = get_tw_market_overview()
print(f"日期：{overview['date']}")
print(f"成交量：{overview['trade_volume']}")
print(f"成交值：{overview['trade_value']}")
print(f"加權指數收盤：{overview['taiex_close']}")
print(f"漲跌：{overview['taiex_change']}")
```

---

### 查詢當日強勢股

```python
from taiwan_market_connector import get_tw_top_movers

# 取得漲幅前 10 名
movers = get_tw_top_movers(top_n=10)
print(movers)
```

---

### 搜尋股票

```python
from taiwan_market_connector import search_tw_stocks

# 依中文名稱搜尋
results = search_tw_stocks("金融")
for r in results:
    print(f"{r['code']}  {r['name']:6}  {r['english']}")

# 依產業搜尋
results = search_tw_stocks("Semiconductors")
for r in results:
    print(r)
```

---

### 執行完整自我測試

```bash
# 在專案根目錄執行
python fincept-qt/src/python/taiwan_market_connector.py
```

若所有功能正常，將看到：
```
=== Taiwan Market Connector — Self Test ===
1. TSMC (2330) last 5 trading days: ...
2. TSMC company info: ...
3. TAIEX index (last 5 days): ...
4. Search 'semi': ...
✓ All tests passed.
```

---

## 支援的股票代碼

### 上市股票（TWSE 台灣證券交易所）

使用格式：`代碼.TW`（在 Yahoo Finance 中）

| 代碼 | 中文名稱 | 英文名稱 | 產業 |
|------|----------|----------|------|
| 2330 | 台積電 | TSMC | 半導體 |
| 2454 | 聯發科 | MediaTek | 半導體 |
| 3711 | 日月光 | ASE Technology | 半導體 |
| 2303 | 聯電 | UMC | 半導體 |
| 2317 | 鴻海 | Hon Hai / Foxconn | 電子 |
| 2308 | 台達電 | Delta Electronics | 電子 |
| 2382 | 廣達 | Quanta Computer | 電子 |
| 2357 | 華碩 | ASUS | 電子 |
| 2353 | 宏碁 | Acer | 電子 |
| 2881 | 富邦金 | Fubon Financial | 金融 |
| 2882 | 國泰金 | Cathay Financial | 金融 |
| 2891 | 中信金 | CTBC Financial | 金融 |
| 2886 | 兆豐金 | Mega Financial | 金融 |
| 2892 | 第一金 | First Financial | 金融 |
| 2412 | 中華電 | Chunghwa Telecom | 電信 |
| 3045 | 台灣大 | Taiwan Mobile | 電信 |
| 1301 | 台塑 | Formosa Plastics | 材料 |
| 1303 | 南亞 | Nan Ya Plastics | 材料 |
| 6505 | 台塑化 | FPCC | 材料 |

### 上櫃股票（TPEX 證券櫃檯買賣中心）

使用格式：`代碼.TWO`（在 Yahoo Finance 中）

上櫃股票請傳入 `market="TPEX"` 參數。常見上櫃股：

| 代碼 | 名稱 | 說明 |
|------|------|------|
| 6669 | 緯穎 | 伺服器 ODM |
| 6488 | 環球晶 | 矽晶圓 |
| 3443 | 創意 | IC 設計服務 |

---

## 資料來源說明

| 資料來源 | 用途 | API 金鑰 | 備註 |
|----------|------|----------|------|
| [Yahoo Finance（yfinance）](https://github.com/ranaroussi/yfinance) | 歷史 OHLCV、公司資訊 | 不需要 | 免費，有速率限制 |
| [TWSE OpenAPI](https://openapi.twse.com.tw/) | 市場概況、強勢股、完整上市清單 | 不需要 | 官方資料，每 5 秒 3 次請求限制 |
| [twstock](https://github.com/mlouielu/twstock)（選用） | 即時報價、技術分析輔助 | 不需要 | 需另行 `pip install twstock` |

> **注意事項：**
> - TWSE OpenAPI 有請求速率限制（每 5 秒 3 次），本連接器已內建等待邏輯。
> - Yahoo Finance 的資料可能有 15–20 分鐘延遲（非 Premium 用戶）。
> - 盤中即時資料建議透過 TWSE OpenAPI 取得。

---

## 常見問題 FAQ

### Q1：執行時出現 `No data returned for 2330.TW`，怎麼辦？

**可能原因：**
- 股票代碼或市場類型（TWSE/TPEX）錯誤
- 查詢日期範圍包含假日或超出資料範圍
- Yahoo Finance 暫時性網路問題

**解決方式：**
```python
# 確認代碼正確（上市用 .TW，上櫃用 .TWO）
import yfinance as yf
ticker = yf.Ticker("2330.TW")
print(ticker.history(period="5d"))  # 直接測試
```

---

### Q2：TWSE OpenAPI 回傳空值？

TWSE OpenAPI 僅在**台灣股市交易時間**（週一至週五 09:00–13:30 台灣時間）有即時資料。非交易時段請求會回傳最新收盤資料或空陣列。

---

### Q3：為什麼不直接支援 Polygon 或 Bloomberg 的台股資料？

Polygon 的台股資料需要付費方案。TWSE OpenAPI + Yahoo Finance 的組合對於個人研究與學習已足夠，且完全免費。

---

### Q4：可以查詢 ETF 嗎？

可以！台灣 ETF 使用相同格式：

```python
# 元大台灣50（0050）
df = get_tw_stock_history("0050", market="TWSE")

# 元大高股息（0056）
df = get_tw_stock_history("0056", market="TWSE")
```

---

### Q5：如何取得完整的上市股票清單？

```python
from taiwan_market_connector import get_tw_all_listed_stocks

all_stocks = get_tw_all_listed_stocks()
print(f"共 {len(all_stocks)} 檔上市股票")
print(all_stocks.head(10))
```

---

## 如何貢獻

歡迎任何形式的貢獻！

### 貢獻流程

```bash
# 1. Fork 本 Repository（點擊 GitHub 右上角 Fork 按鈕）

# 2. Clone 你的 Fork
git clone https://github.com/你的帳號/FinceptTerminal.git
cd FinceptTerminal

# 3. 建立新的功能分支
git checkout -b feature/你的功能名稱

# 4. 進行修改...

# 5. 提交變更
git add .
git commit -m "feat: 加入某某功能的說明"

# 6. 推送到你的 Fork
git push origin feature/你的功能名稱

# 7. 在 GitHub 上開啟 Pull Request
```

### 目前期待的貢獻項目

- [ ] 新增更多上櫃股票到參考清單
- [ ] 加入台灣 ETF 完整清單
- [ ] 支援期貨/選擇權資料（TAIFEX 臺灣期貨交易所）
- [ ] 加入英文版 README（`README.md`）
- [ ] 補充更多技術指標範例

### 程式碼風格

- 所有函式需有 docstring（中英文均可）
- 新增功能請附上測試範例
- 檔案頂部須保留原始授權聲明

---

## 致謝

- **[Fincept Corporation](https://github.com/Fincept-Corporation/FinceptTerminal)**：感謝原始專案的開源貢獻，提供了這個優秀的金融分析框架。
- **[Yahoo Finance / yfinance](https://github.com/ranaroussi/yfinance)**：提供免費的台股歷史資料介面。
- **[TWSE 臺灣證券交易所](https://openapi.twse.com.tw/)**：提供官方免費 OpenAPI。
- **[twstock](https://github.com/mlouielu/twstock)**：提供 Python 台股分析套件。

---

## 聯絡方式

若有問題或建議，歡迎在本 Repository 開啟 [Issue](../../issues)。

若涉及原始 FinceptTerminal 的功能問題，請至[官方 Repository](https://github.com/Fincept-Corporation/FinceptTerminal/issues) 回報。

---

*本文件最後更新：2026 年 5 月*
*授權：AGPL-3.0（同上游專案）*
