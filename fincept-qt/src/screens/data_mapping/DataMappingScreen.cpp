#include "screens/data_mapping/DataMappingScreen.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "services/data_normalization/DataNormalizationService.h"
#include "storage/repositories/DataMappingRepository.h"
#include "ui/theme/Theme.h"

#include <QShowEvent>
#include <QUuid>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

// ── Stylesheet ──────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;
static const QString kStyle =
    QStringLiteral("#dmScreen { background: %1; }"

                   "#dmHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#dmHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#dmHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
                   "#dmHeaderBadge { color: %6; font-size: 8px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

                   "#dmViewBtn { background: transparent; color: %5; border: 1px solid %8; "
                   "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
                   "#dmViewBtn:hover { color: %4; }"
                   "#dmViewBtn[active=\"true\"] { background: %3; color: %1; border-color: %3; }"

                   "#dmStepBar { background: %7; border-bottom: 1px solid %8; }"
                   "#dmStepBtn { background: transparent; color: %5; border: none; "
                   "  font-size: 9px; font-weight: 700; padding: 6px 14px; border-bottom: 2px solid transparent; }"
                   "#dmStepBtn:hover { color: %4; }"
                   "#dmStepBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"
                   "#dmStepBtn[complete=\"true\"] { color: %6; }"

                   "#dmLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#dmRightPanel { background: %7; border-left: 1px solid %8; }"

                   "#dmPanelTitle { color: %5; font-size: 10px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; padding: 8px; "
                   "  border-bottom: 1px solid %8; }"

                   "#dmPanel { background: %7; border: 1px solid %8; }"
                   "#dmPanelHeader { background: %2; border-bottom: 1px solid %8; }"
                   "#dmPanelHeaderTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#dmPanelHeaderIcon { color: %3; font-size: 13px; background: transparent; }"

                   "#dmLabel { color: %5; font-size: 9px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QLineEdit:focus { border-color: %9; }"
                   "QPlainTextEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                   "  selection-background-color: %3; }"
                   "QSpinBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QSpinBox::up-button, QSpinBox::down-button { width: 0; }"

                   "#dmCalcBtn { background: %3; color: %1; border: none; padding: 5px 16px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#dmCalcBtn:hover { background: #FF8800; }"
                   "#dmCalcBtn:disabled { background: %10; color: %11; }"
                   "#dmSecondaryBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 5px 16px; font-size: 9px; font-weight: 700; }"
                   "#dmSecondaryBtn:hover { color: %4; }"
                   "#dmSaveBtn { background: %6; color: %1; border: none; padding: 6px 20px; "
                   "  font-size: 10px; font-weight: 700; }"
                   "#dmSaveBtn:hover { background: #22c55e; }"
                   "#dmSaveBtn:disabled { background: %10; color: %11; }"

                   "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: 11px; }"
                   "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

                   "QTreeWidget { background: %1; color: %4; border: none; font-size: 11px; }"
                   "QTreeWidget::item { padding: 2px 4px; }"
                   "QTreeWidget::item:selected { background: rgba(217,119,6,0.1); color: %3; }"

                   "QListWidget { background: %1; border: none; font-size: 11px; }"
                   "QListWidget::item { color: %5; padding: 6px 10px; border-bottom: 1px solid %8; }"
                   "QListWidget::item:hover { color: %4; background: %12; }"
                   "QListWidget::item:selected { color: %3; background: rgba(217,119,6,0.1); }"

                   "QTextEdit { background: %1; color: %13; border: none; font-size: 11px; }"

                   "#dmInfoLabel { color: %5; font-size: 9px; background: transparent; }"
                   "#dmInfoValue { color: %13; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#dmSuccessBadge { color: %6; font-size: 9px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.15); padding: 2px 6px; }"
                   "#dmErrorBadge { color: %14; font-size: 9px; font-weight: 700; "
                   "  background: rgba(220,38,38,0.15); padding: 2px 6px; }"

                   "#dmNavFooter { background: %2; border-top: 1px solid %8; }"
                   "#dmStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#dmStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#dmStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

                   "#dmEmptyState { color: %11; font-size: 13px; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollArea { background: %1; border: none; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(colors::BG_BASE)        // %1
        .arg(colors::BG_RAISED)      // %2
        .arg(colors::AMBER)          // %3
        .arg(colors::TEXT_PRIMARY)   // %4
        .arg(colors::TEXT_SECONDARY) // %5
        .arg(colors::POSITIVE)       // %6
        .arg(colors::BG_SURFACE)     // %7
        .arg(colors::BORDER_DIM)     // %8
        .arg(colors::BORDER_BRIGHT)  // %9
        .arg(colors::AMBER_DIM)      // %10
        .arg(colors::TEXT_DIM)       // %11
        .arg(colors::BG_HOVER)       // %12
        .arg(colors::CYAN)           // %13
        .arg(colors::NEGATIVE)       // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Schema definitions ──────────────────────────────────────────────────────

struct SchemaField {
    QString name;
    QString type;
    bool required;
    QString description;
};

struct SchemaInfo {
    QString name;
    QString description;
    QList<SchemaField> fields;
};

static QList<SchemaInfo> build_schemas() {
    return {
        {"OHLCV",
         "Open/High/Low/Close/Volume candlestick data",
         {
             {"symbol", "string", true, "Trading symbol"},
             {"timestamp", "datetime", true, "Bar timestamp"},
             {"open", "number", true, "Opening price"},
             {"high", "number", true, "Highest price"},
             {"low", "number", true, "Lowest price"},
             {"close", "number", true, "Closing price"},
             {"volume", "number", true, "Trading volume"},
             {"vwap", "number", false, "Volume weighted avg price"},
             {"trades", "number", false, "Number of trades"},
         }},
        {"QUOTE",
         "Real-time or delayed quote with bid/ask",
         {
             {"symbol", "string", true, "Trading symbol"},
             {"timestamp", "datetime", true, "Quote timestamp"},
             {"price", "number", true, "Last traded price"},
             {"bid", "number", false, "Best bid price"},
             {"ask", "number", false, "Best ask price"},
             {"bidSize", "number", false, "Bid quantity"},
             {"askSize", "number", false, "Ask quantity"},
             {"volume", "number", false, "Total volume"},
             {"change", "number", false, "Change from prev close"},
             {"changePercent", "number", false, "Percentage change"},
             {"open", "number", false, "Day open price"},
             {"high", "number", false, "Day high price"},
             {"low", "number", false, "Day low price"},
             {"previousClose", "number", false, "Previous close"},
         }},
        {"TICK",
         "Individual trade ticks",
         {
             {"symbol", "string", true, "Trading symbol"},
             {"timestamp", "datetime", true, "Trade timestamp"},
             {"price", "number", true, "Trade price"},
             {"quantity", "number", true, "Trade quantity"},
             {"side", "enum", false, "BUY/SELL/UNKNOWN"},
             {"tradeId", "string", false, "Unique trade ID"},
         }},
        {"ORDER",
         "Trading order",
         {
             {"orderId", "string", true, "Unique order ID"},
             {"symbol", "string", true, "Trading symbol"},
             {"side", "enum", true, "BUY/SELL"},
             {"type", "enum", true, "MARKET/LIMIT/STOP/STOP_LIMIT"},
             {"quantity", "number", true, "Order quantity"},
             {"filledQuantity", "number", false, "Filled quantity"},
             {"price", "number", false, "Limit price"},
             {"averagePrice", "number", false, "Average fill price"},
             {"status", "enum", true, "PENDING/OPEN/FILLED/CANCELLED"},
             {"timestamp", "datetime", true, "Placement timestamp"},
             {"exchange", "string", false, "Exchange name"},
         }},
        {"POSITION",
         "Open position in portfolio",
         {
             {"symbol", "string", true, "Trading symbol"},
             {"quantity", "number", true, "Position quantity"},
             {"averagePrice", "number", true, "Average entry price"},
             {"currentPrice", "number", true, "Current market price"},
             {"marketValue", "number", true, "Current market value"},
             {"costBasis", "number", true, "Total cost basis"},
             {"pnl", "number", true, "Unrealized P&L"},
             {"pnlPercent", "number", true, "P&L percentage"},
             {"exchange", "string", false, "Exchange name"},
         }},
        {"PORTFOLIO",
         "Portfolio summary",
         {
             {"totalValue", "number", true, "Total portfolio value"},
             {"cash", "number", true, "Available cash"},
             {"invested", "number", true, "Total invested"},
             {"marketValue", "number", true, "Market value of holdings"},
             {"pnl", "number", true, "Total unrealized P&L"},
             {"pnlPercent", "number", true, "Total P&L percentage"},
             {"dayPnl", "number", false, "Today's P&L"},
             {"timestamp", "datetime", true, "Snapshot timestamp"},
         }},
        {"INSTRUMENT",
         "Security/instrument master data",
         {
             {"symbol", "string", true, "Trading symbol"},
             {"name", "string", true, "Instrument name"},
             {"exchange", "string", true, "Exchange"},
             {"instrumentType", "enum", true, "EQUITY/FUTURES/OPTIONS/CURRENCY"},
             {"isin", "string", false, "ISIN code"},
             {"lotSize", "number", false, "Lot size"},
             {"tickSize", "number", false, "Min price movement"},
         }},
    };
}

static QList<SchemaInfo> g_schemas = build_schemas();

// ── Template definitions ────────────────────────────────────────────────────

struct FieldMapping {
    QString target;
    QString expression;
    QString transform;
    QString default_val;
};

struct MappingTemplate {
    QString id;
    QString name;
    QString description;
    QString broker;
    QString schema;
    QStringList tags;
    bool verified;
    // Real API details
    QString base_url;
    QString endpoint;
    QString method;
    QString auth_type;
    QString headers;
    QString body;
    QString parser;
    QList<FieldMapping> field_mappings;
};

static QList<MappingTemplate> build_templates() {
    return {
        // ── Free Public APIs (no auth) ─────────────────────────────────────

        {"coingecko_quote",
         "CoinGecko Coin Price → QUOTE",
         "Fetch real-time crypto prices from CoinGecko. Free, no API key required. "
         "Returns price, market cap, 24h volume, and change data.",
         "CoinGecko",
         "QUOTE",
         {"crypto", "coingecko", "free", "public"},
         true,
         "https://api.coingecko.com",
         "/api/v3/simple/price?ids=bitcoin&vs_currencies=usd&include_24hr_vol=true&include_24hr_change=true&include_last_updated_at=true",
         "GET",
         "None",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"symbol", "$.bitcoin", "", "bitcoin"},
          {"price", "$.bitcoin.usd", "", "0"},
          {"volume", "$.bitcoin.usd_24h_vol", "", "0"},
          {"changePercent", "$.bitcoin.usd_24h_change", "", "0"},
          {"timestamp", "$.bitcoin.last_updated_at", "unix_to_iso", ""}}},

        {"coingecko_ohlcv",
         "CoinGecko OHLC → OHLCV",
         "Fetch OHLC candlestick data for any crypto from CoinGecko. Free API. "
         "Returns 1/7/14/30/90/180/365 day candles. Array format: [timestamp, O, H, L, C].",
         "CoinGecko",
         "OHLCV",
         {"crypto", "coingecko", "ohlc", "candles", "free"},
         true,
         "https://api.coingecko.com",
         "/api/v3/coins/bitcoin/ohlc?vs_currency=usd&days=30",
         "GET",
         "None",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"timestamp", "$[*][0]", "unix_ms_to_iso", ""},
          {"open", "$[*][1]", "", "0"},
          {"high", "$[*][2]", "", "0"},
          {"low", "$[*][3]", "", "0"},
          {"close", "$[*][4]", "", "0"},
          {"symbol", "", "", "bitcoin"}}},

        {"yahoo_quote",
         "Yahoo Finance Spark → QUOTE",
         "Fetch real-time stock quotes from Yahoo Finance. Free, no API key. "
         "Returns last price, volume, and market state for any symbol.",
         "Yahoo Finance",
         "QUOTE",
         {"stocks", "yahoo", "free", "public", "us"},
         true,
         "https://query1.finance.yahoo.com",
         "/v8/finance/spark?symbols=AAPL&range=1d&interval=1d",
         "GET",
         "None",
         "Accept: application/json\nUser-Agent: Mozilla/5.0",
         "",
         "JSONPath",
         {{"symbol", "$.spark.result[0].symbol", "", ""},
          {"price", "$.spark.result[0].response[0].meta.regularMarketPrice", "", "0"},
          {"previousClose", "$.spark.result[0].response[0].meta.previousClose", "", "0"},
          {"volume", "$.spark.result[0].response[0].meta.regularMarketVolume", "", "0"},
          {"timestamp", "$.spark.result[0].response[0].meta.regularMarketTime", "unix_to_iso", ""}}},

        {"binance_ohlcv",
         "Binance Klines → OHLCV",
         "Fetch candlestick/kline data from Binance. Free, no API key required for market data. "
         "Supports intervals: 1m, 3m, 5m, 15m, 30m, 1h, 4h, 1d, 1w, 1M.",
         "Binance",
         "OHLCV",
         {"crypto", "binance", "klines", "candles", "free"},
         true,
         "https://api.binance.com",
         "/api/v3/klines?symbol=BTCUSDT&interval=1h&limit=100",
         "GET",
         "None",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"timestamp", "$[*][0]", "unix_ms_to_iso", ""},
          {"open", "$[*][1]", "to_number", "0"},
          {"high", "$[*][2]", "to_number", "0"},
          {"low", "$[*][3]", "to_number", "0"},
          {"close", "$[*][4]", "to_number", "0"},
          {"volume", "$[*][5]", "to_number", "0"},
          {"trades", "$[*][8]", "to_number", "0"},
          {"symbol", "", "", "BTCUSDT"}}},

        {"binance_quote",
         "Binance 24hr Ticker → QUOTE",
         "Fetch 24-hour price statistics from Binance. Free, no API key. "
         "Returns price, bid/ask, volume, and 24h change for any trading pair.",
         "Binance",
         "QUOTE",
         {"crypto", "binance", "ticker", "free"},
         true,
         "https://api.binance.com",
         "/api/v3/ticker/24hr?symbol=BTCUSDT",
         "GET",
         "None",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"symbol", "$.symbol", "", ""},
          {"price", "$.lastPrice", "to_number", "0"},
          {"bid", "$.bidPrice", "to_number", "0"},
          {"ask", "$.askPrice", "to_number", "0"},
          {"bidSize", "$.bidQty", "to_number", "0"},
          {"askSize", "$.askQty", "to_number", "0"},
          {"volume", "$.volume", "to_number", "0"},
          {"change", "$.priceChange", "to_number", "0"},
          {"changePercent", "$.priceChangePercent", "to_number", "0"},
          {"open", "$.openPrice", "to_number", "0"},
          {"high", "$.highPrice", "to_number", "0"},
          {"low", "$.lowPrice", "to_number", "0"},
          {"previousClose", "$.prevClosePrice", "to_number", "0"}}},

        {"kraken_ohlcv",
         "Kraken OHLC → OHLCV",
         "Fetch OHLC data from Kraken. Free public endpoint, no API key required. "
         "Intervals: 1, 5, 15, 30, 60, 240, 1440, 10080, 21600 minutes.",
         "Kraken",
         "OHLCV",
         {"crypto", "kraken", "ohlc", "free"},
         true,
         "https://api.kraken.com",
         "/0/public/OHLC?pair=XBTUSD&interval=60",
         "GET",
         "None",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"timestamp", "$.result.XXBTZUSD[*][0]", "unix_to_iso", ""},
          {"open", "$.result.XXBTZUSD[*][1]", "to_number", "0"},
          {"high", "$.result.XXBTZUSD[*][2]", "to_number", "0"},
          {"low", "$.result.XXBTZUSD[*][3]", "to_number", "0"},
          {"close", "$.result.XXBTZUSD[*][4]", "to_number", "0"},
          {"vwap", "$.result.XXBTZUSD[*][5]", "to_number", "0"},
          {"volume", "$.result.XXBTZUSD[*][6]", "to_number", "0"},
          {"trades", "$.result.XXBTZUSD[*][7]", "to_number", "0"},
          {"symbol", "", "", "XBTUSD"}}},

        // ── API Key Required (Free Tier) ───────────────────────────────────

        {"alphavantage_ohlcv",
         "Alpha Vantage Intraday → OHLCV",
         "Fetch intraday OHLCV from Alpha Vantage. Free tier: 25 requests/day. "
         "Intervals: 1min, 5min, 15min, 30min, 60min. Set your API key in AUTH VALUE.",
         "Alpha Vantage",
         "OHLCV",
         {"stocks", "alphavantage", "intraday", "free-tier"},
         true,
         "https://www.alphavantage.co",
         "/query?function=TIME_SERIES_INTRADAY&symbol=IBM&interval=5min&apikey=YOUR_KEY",
         "GET",
         "API Key",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"timestamp", "$.['Time Series (5min)'].*~", "", ""},
          {"open", "$.['Time Series (5min)'].*.['1. open']", "to_number", "0"},
          {"high", "$.['Time Series (5min)'].*.['2. high']", "to_number", "0"},
          {"low", "$.['Time Series (5min)'].*.['3. low']", "to_number", "0"},
          {"close", "$.['Time Series (5min)'].*.['4. close']", "to_number", "0"},
          {"volume", "$.['Time Series (5min)'].*.['5. volume']", "to_number", "0"},
          {"symbol", "", "", "IBM"}}},

        {"twelvedata_ohlcv",
         "Twelve Data Time Series → OHLCV",
         "Fetch OHLCV time series from Twelve Data. Free tier: 800 requests/day. "
         "Supports stocks, forex, crypto. Set API key in AUTH VALUE field.",
         "Twelve Data",
         "OHLCV",
         {"stocks", "forex", "crypto", "twelvedata", "free-tier"},
         true,
         "https://api.twelvedata.com",
         "/time_series?symbol=AAPL&interval=1h&outputsize=30&apikey=YOUR_KEY",
         "GET",
         "API Key",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"timestamp", "$.values[*].datetime", "", ""},
          {"open", "$.values[*].open", "to_number", "0"},
          {"high", "$.values[*].high", "to_number", "0"},
          {"low", "$.values[*].low", "to_number", "0"},
          {"close", "$.values[*].close", "to_number", "0"},
          {"volume", "$.values[*].volume", "to_number", "0"},
          {"symbol", "$.meta.symbol", "", ""}}},

        {"finnhub_quote",
         "Finnhub Quote → QUOTE",
         "Fetch real-time stock quote from Finnhub. Free tier: 60 calls/min. "
         "Returns current price, change, high/low, open, previous close. Set API key in AUTH VALUE.",
         "Finnhub",
         "QUOTE",
         {"stocks", "finnhub", "free-tier", "us"},
         true,
         "https://finnhub.io",
         "/api/v1/quote?symbol=AAPL&token=YOUR_KEY",
         "GET",
         "API Key",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"price", "$.c", "", "0"},
          {"change", "$.d", "", "0"},
          {"changePercent", "$.dp", "", "0"},
          {"high", "$.h", "", "0"},
          {"low", "$.l", "", "0"},
          {"open", "$.o", "", "0"},
          {"previousClose", "$.pc", "", "0"},
          {"timestamp", "$.t", "unix_to_iso", ""},
          {"symbol", "", "", "AAPL"}}},

        {"coinmarketcap_quote",
         "CoinMarketCap Latest → QUOTE",
         "Fetch latest crypto quotes from CoinMarketCap. Free tier: 10,000 calls/month. "
         "Returns price, volume, market cap, and 24h change. Set API key in AUTH VALUE.",
         "CoinMarketCap",
         "QUOTE",
         {"crypto", "coinmarketcap", "free-tier"},
         true,
         "https://pro-api.coinmarketcap.com",
         "/v1/cryptocurrency/quotes/latest?symbol=BTC&convert=USD",
         "GET",
         "API Key",
         "Accept: application/json\nX-CMC_PRO_API_KEY: YOUR_KEY",
         "",
         "JSONPath",
         {{"symbol", "$.data.BTC.symbol", "", ""},
          {"price", "$.data.BTC.quote.USD.price", "", "0"},
          {"volume", "$.data.BTC.quote.USD.volume_24h", "", "0"},
          {"change", "$.data.BTC.quote.USD.percent_change_24h", "", "0"},
          {"changePercent", "$.data.BTC.quote.USD.percent_change_24h", "", "0"},
          {"timestamp", "$.data.BTC.quote.USD.last_updated", "", ""}}},

        // ── Global Broker APIs (Auth Required) ─────────────────────────────

        {"alpaca_ohlcv",
         "Alpaca Bars → OHLCV",
         "Fetch historical bar data from Alpaca Markets. Free paper trading account available. "
         "Set API key as 'APCA-API-KEY-ID' and secret as 'APCA-API-SECRET-KEY' in headers.",
         "Alpaca",
         "OHLCV",
         {"stocks", "alpaca", "bars", "us"},
         true,
         "https://data.alpaca.markets",
         "/v2/stocks/AAPL/bars?timeframe=1Hour&start=2024-03-01&limit=100",
         "GET",
         "API Key",
         "Accept: application/json\nAPCA-API-KEY-ID: YOUR_KEY\nAPCA-API-SECRET-KEY: YOUR_SECRET",
         "",
         "JSONPath",
         {{"timestamp", "$.bars[*].t", "", ""},
          {"open", "$.bars[*].o", "", "0"},
          {"high", "$.bars[*].h", "", "0"},
          {"low", "$.bars[*].l", "", "0"},
          {"close", "$.bars[*].c", "", "0"},
          {"volume", "$.bars[*].v", "", "0"},
          {"vwap", "$.bars[*].vw", "", "0"},
          {"trades", "$.bars[*].n", "", "0"},
          {"symbol", "$.symbol", "", "AAPL"}}},

        {"alpaca_positions",
         "Alpaca Positions → POSITION",
         "Fetch open positions from Alpaca. Works with paper or live accounts. "
         "Set API credentials in headers.",
         "Alpaca",
         "POSITION",
         {"stocks", "alpaca", "positions", "us"},
         true,
         "https://paper-api.alpaca.markets",
         "/v2/positions",
         "GET",
         "API Key",
         "Accept: application/json\nAPCA-API-KEY-ID: YOUR_KEY\nAPCA-API-SECRET-KEY: YOUR_SECRET",
         "",
         "JSONPath",
         {{"symbol", "$[*].symbol", "", ""},
          {"quantity", "$[*].qty", "to_number", "0"},
          {"averagePrice", "$[*].avg_entry_price", "to_number", "0"},
          {"currentPrice", "$[*].current_price", "to_number", "0"},
          {"marketValue", "$[*].market_value", "to_number", "0"},
          {"costBasis", "$[*].cost_basis", "to_number", "0"},
          {"pnl", "$[*].unrealized_pl", "to_number", "0"},
          {"pnlPercent", "$[*].unrealized_plpc", "to_number", "0"},
          {"exchange", "$[*].exchange", "", ""}}},

        {"tradier_quote",
         "Tradier Quote → QUOTE",
         "Fetch stock quotes from Tradier. Free sandbox available. "
         "Set Bearer token in AUTH VALUE. Use sandbox URL for testing.",
         "Tradier",
         "QUOTE",
         {"stocks", "tradier", "us", "options"},
         true,
         "https://sandbox.tradier.com",
         "/v1/markets/quotes?symbols=AAPL",
         "GET",
         "Bearer Token",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"symbol", "$.quotes.quote.symbol", "", ""},
          {"price", "$.quotes.quote.last", "", "0"},
          {"bid", "$.quotes.quote.bid", "", "0"},
          {"ask", "$.quotes.quote.ask", "", "0"},
          {"volume", "$.quotes.quote.volume", "", "0"},
          {"change", "$.quotes.quote.change", "", "0"},
          {"changePercent", "$.quotes.quote.change_percentage", "", "0"},
          {"open", "$.quotes.quote.open", "", "0"},
          {"high", "$.quotes.quote.high", "", "0"},
          {"low", "$.quotes.quote.low", "", "0"},
          {"previousClose", "$.quotes.quote.prevclose", "", "0"}}},

        // ── Macro / Economic Data ──────────────────────────────────────────

        {"dbnomics_ohlcv",
         "DBnomics Series → OHLCV",
         "Fetch economic time series from DBnomics. Free, no API key. "
         "Access 100M+ series from ECB, Fed, World Bank, IMF. Replace provider/dataset/series in endpoint.",
         "DBnomics",
         "OHLCV",
         {"economics", "macro", "dbnomics", "free"},
         true,
         "https://api.db.nomics.world",
         "/v22/series/IMF/WEO:2024-04/USA.NGDP_RPCH.pcent_change?observations=1",
         "GET",
         "None",
         "Accept: application/json",
         "",
         "JSONPath",
         {{"timestamp", "$.series.docs[0].period", "", ""},
          {"close", "$.series.docs[0].value", "", "0"},
          {"symbol", "$.series.docs[0].series_code", "", ""}}},
    };
}

static QList<MappingTemplate> g_templates = build_templates();

// ── Constructor ─────────────────────────────────────────────────────────────

DataMappingScreen::DataMappingScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("dmScreen");
    setStyleSheet(kStyle);
    setup_ui();
    LOG_INFO("DataMapping", "Screen constructed");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void DataMappingScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());
    root->addWidget(create_step_bar());
    root->addWidget(create_main_area(), 1);

    nav_footer_ = create_nav_footer();
    nav_footer_->hide();
    root->addWidget(nav_footer_);

    root->addWidget(create_status_bar());

    // Default to list view
    on_view_changed(0);
}

QWidget* DataMappingScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("dmHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    auto* title = new QLabel("DATA MAPPING ENGINE");
    title->setObjectName("dmHeaderTitle");
    auto* sub = new QLabel("API CONFIGURATION & SCHEMA TRANSFORMATION");
    sub->setObjectName("dmHeaderSub");
    title_col->addWidget(title);
    title_col->addWidget(sub);
    hl->addLayout(title_col);
    hl->addSpacing(24);

    // View buttons
    const QStringList views = {"MAPPINGS", "TEMPLATES", "CREATE"};
    for (int i = 0; i < views.size(); ++i) {
        auto* btn = new QPushButton(views[i]);
        btn->setObjectName("dmViewBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hl->addWidget(btn);
        view_btns_.append(btn);
    }

    hl->addStretch(1);

    auto* badge = new QLabel("7 SCHEMAS");
    badge->setObjectName("dmHeaderBadge");
    hl->addWidget(badge);

    return bar;
}

QWidget* DataMappingScreen::create_step_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("dmStepBar");
    bar->setFixedHeight(32);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(0);

    const QStringList steps = {"API CONFIG", "SCHEMA", "FIELD MAPPING", "CACHE", "TEST & SAVE"};
    for (int i = 0; i < steps.size(); ++i) {
        auto* btn = new QPushButton(QString::number(i + 1) + ". " + steps[i]);
        btn->setObjectName("dmStepBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        btn->setProperty("complete", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_step_changed(i); });
        hl->addWidget(btn);
        step_btns_.append(btn);
    }

    hl->addStretch(1);
    return bar;
}

QWidget* DataMappingScreen::create_main_area() {
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // Left panel
    auto* left = create_left_panel();
    left->setMinimumWidth(180);
    left->setMaximumWidth(220);

    // Center: view stack
    auto* center = new QWidget;
    auto* cvl = new QVBoxLayout(center);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);

    view_stack_ = new QStackedWidget;
    // Stack order must match header button order: MAPPINGS=0, TEMPLATES=1, CREATE=2
    view_stack_->addWidget(create_list_view());

    view_stack_->addWidget(create_template_view());

    // Create view contains step stack
    auto* create_scroll = new QScrollArea;
    create_scroll->setWidgetResizable(true);
    auto* create_content = new QWidget;
    auto* ccl = new QVBoxLayout(create_content);
    ccl->setContentsMargins(16, 16, 16, 16);
    ccl->setSpacing(16);

    step_stack_ = new QStackedWidget;
    step_stack_->addWidget(create_api_config_panel());
    step_stack_->addWidget(create_schema_panel());
    step_stack_->addWidget(create_field_mapping_panel());
    step_stack_->addWidget(create_cache_panel());
    step_stack_->addWidget(create_test_save_panel());
    ccl->addWidget(step_stack_);
    ccl->addStretch(1);
    create_scroll->setWidget(create_content);
    view_stack_->addWidget(create_scroll);
    cvl->addWidget(view_stack_, 1);

    // Right panel
    auto* right = create_right_panel();
    right->setMinimumWidth(190);
    right->setMaximumWidth(220);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({200, 700, 200});

    return splitter;
}

QWidget* DataMappingScreen::create_left_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmLeftPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("WIZARD STEPS");
    title->setObjectName("dmPanelTitle");
    vl->addWidget(title);

    // Step navigation
    const QStringList step_labels = {"API Configuration", "Schema Selection", "Field Mapping", "Cache Settings",
                                     "Test & Save"};
    const QStringList step_icons = {"1", "2", "3", "4", "5"};
    for (int i = 0; i < step_labels.size(); ++i) {
        auto* btn = new QPushButton(step_icons[i] + "  " + step_labels[i]);
        btn->setObjectName("dmSecondaryBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("text-align: left; padding: 6px 10px; border: none; border-bottom: 1px solid " +
                           QLatin1String(colors::BORDER_DIM()) + ";");
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            on_view_changed(2); // switch to create view
            on_step_changed(i);
        });
        vl->addWidget(btn);
    }

    vl->addStretch(1);

    // Quick stats
    auto* stats = new QWidget;
    auto* sl = new QVBoxLayout(stats);
    sl->setContentsMargins(10, 8, 10, 8);
    sl->setSpacing(4);

    auto* stat_title = new QLabel("QUICK STATS");
    stat_title->setObjectName("dmLabel");
    sl->addWidget(stat_title);

    status_mappings_ = new QLabel("Saved: 0");
    status_mappings_->setObjectName("dmInfoValue");
    sl->addWidget(status_mappings_);

    auto* schemas_lbl = new QLabel("Schemas: 7");
    schemas_lbl->setObjectName("dmInfoLabel");
    sl->addWidget(schemas_lbl);

    auto* parsers_lbl = new QLabel("Parsers: 6");
    parsers_lbl->setObjectName("dmInfoLabel");
    sl->addWidget(parsers_lbl);

    vl->addWidget(stats);
    return panel;
}

QWidget* DataMappingScreen::create_right_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmRightPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("SYSTEM");
    title->setObjectName("dmPanelTitle");
    vl->addWidget(title);

    auto* info = new QWidget;
    auto* il = new QVBoxLayout(info);
    il->setContentsMargins(10, 8, 10, 8);
    il->setSpacing(6);

    // Engine status
    auto* eng_title = new QLabel("MAPPING ENGINE");
    eng_title->setObjectName("dmLabel");
    il->addWidget(eng_title);

    right_engine_status_ = new QLabel("ONLINE");
    right_engine_status_->setObjectName("dmSuccessBadge");
    il->addWidget(right_engine_status_);

    il->addSpacing(8);

    // Parser list
    auto* par_title = new QLabel("PARSER ENGINES");
    par_title->setObjectName("dmLabel");
    il->addWidget(par_title);

    const QStringList parsers = {"JSONPath", "JSONata", "JMESPath", "Direct", "JavaScript", "Regex"};
    for (const auto& p : parsers) {
        auto* lbl = new QLabel(p + " — READY");
        lbl->setObjectName("dmInfoLabel");
        il->addWidget(lbl);
    }

    il->addSpacing(8);

    // Security
    auto* sec_title = new QLabel("SECURITY");
    sec_title->setObjectName("dmLabel");
    il->addWidget(sec_title);
    auto* sec_val = new QLabel("AES-256-GCM");
    sec_val->setObjectName("dmInfoValue");
    il->addWidget(sec_val);

    il->addSpacing(8);

    // Current info
    auto* cur_title = new QLabel("CURRENT MAPPING");
    cur_title->setObjectName("dmLabel");
    il->addWidget(cur_title);

    right_schema_info_ = new QLabel("Schema: --");
    right_schema_info_->setObjectName("dmInfoLabel");
    il->addWidget(right_schema_info_);

    right_fields_info_ = new QLabel("Fields: --");
    right_fields_info_->setObjectName("dmInfoLabel");
    il->addWidget(right_fields_info_);

    right_test_info_ = new QLabel("Test: --");
    right_test_info_->setObjectName("dmInfoLabel");
    il->addWidget(right_test_info_);

    il->addStretch(1);
    vl->addWidget(info, 1);
    return panel;
}

// ── Step panels ─────────────────────────────────────────────────────────────

QWidget* DataMappingScreen::create_api_config_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget;
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("1");
    icon->setObjectName("dmPanelHeaderIcon");
    auto* t = new QLabel("API CONFIGURATION");
    t->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(t);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget;
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    api_name_ = new QLineEdit;
    api_name_->setPlaceholderText("e.g. Upstox OHLCV");
    bl->addWidget(create_form_row("MAPPING NAME", api_name_));

    api_base_url_ = new QLineEdit;
    api_base_url_->setPlaceholderText("https://api.example.com");
    api_endpoint_ = new QLineEdit;
    api_endpoint_->setPlaceholderText("/v2/historical-candle/{symbol}/{interval}");
    bl->addWidget(
        create_form_two_col(create_form_row("BASE URL", api_base_url_), create_form_row("ENDPOINT", api_endpoint_)));

    api_method_ = new QComboBox;
    api_method_->addItems({"GET", "POST", "PUT", "DELETE", "PATCH"});
    api_auth_type_ = new QComboBox;
    api_auth_type_->addItems({"None", "API Key", "Bearer Token", "Basic Auth", "OAuth2"});
    bl->addWidget(create_form_two_col(create_form_row("HTTP METHOD", api_method_),
                                      create_form_row("AUTHENTICATION", api_auth_type_)));

    api_auth_value_ = new QLineEdit;
    api_auth_value_->setPlaceholderText("Token / API Key value");
    api_auth_value_->setEchoMode(QLineEdit::Password);
    bl->addWidget(create_form_row("AUTH VALUE", api_auth_value_));

    api_headers_ = new QPlainTextEdit;
    api_headers_->setPlaceholderText("Content-Type: application/json\nAccept: application/json");
    api_headers_->setMaximumHeight(60);
    bl->addWidget(create_form_row("HEADERS (one per line)", api_headers_));

    api_body_ = new QPlainTextEdit;
    api_body_->setPlaceholderText("{\"symbol\": \"AAPL\"}");
    api_body_->setMaximumHeight(60);
    bl->addWidget(create_form_row("REQUEST BODY (JSON)", api_body_));

    api_timeout_ = new QSpinBox;
    api_timeout_->setRange(1, 120);
    api_timeout_->setValue(30);
    api_timeout_->setSuffix(" sec");
    bl->addWidget(create_form_row("TIMEOUT", api_timeout_));

    // Test button + status
    auto* test_row = new QWidget;
    auto* trl = new QHBoxLayout(test_row);
    trl->setContentsMargins(0, 0, 0, 0);
    trl->setSpacing(8);
    api_test_btn_ = new QPushButton("TEST API REQUEST");
    api_test_btn_->setObjectName("dmCalcBtn");
    api_test_btn_->setCursor(Qt::PointingHandCursor);
    connect(api_test_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_test_api);
    api_test_status_ = new QLabel;
    api_test_status_->setObjectName("dmInfoLabel");
    trl->addWidget(api_test_btn_);
    trl->addWidget(api_test_status_);
    trl->addStretch(1);
    bl->addWidget(test_row);

    vl->addWidget(body);
    return panel;
}

QWidget* DataMappingScreen::create_schema_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("2");
    icon->setObjectName("dmPanelHeaderIcon");
    auto* t = new QLabel("SCHEMA SELECTION");
    t->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(t);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget;
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    schema_type_ = new QComboBox;
    schema_type_->addItems({"Predefined Schema", "Custom Schema"});
    bl->addWidget(create_form_row("SCHEMA TYPE", schema_type_));

    schema_select_ = new QComboBox;
    for (const auto& s : g_schemas) {
        schema_select_->addItem(s.name);
    }
    connect(schema_select_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < g_schemas.size()) {
            schema_desc_->setText(g_schemas[idx].description);
            // Populate fields table
            const auto& fields = g_schemas[idx].fields;
            schema_fields_table_->setRowCount(fields.size());
            for (int i = 0; i < fields.size(); ++i) {
                schema_fields_table_->setItem(i, 0, new QTableWidgetItem(fields[i].name));
                schema_fields_table_->setItem(i, 1, new QTableWidgetItem(fields[i].type));
                schema_fields_table_->setItem(i, 2, new QTableWidgetItem(fields[i].required ? "Yes" : "No"));
                schema_fields_table_->setItem(i, 3, new QTableWidgetItem(fields[i].description));
            }
            if (right_schema_info_)
                right_schema_info_->setText("Schema: " + g_schemas[idx].name);
            if (right_fields_info_)
                right_fields_info_->setText("Fields: " + QString::number(fields.size()));
        }
    });
    bl->addWidget(create_form_row("SELECT SCHEMA", schema_select_));

    schema_desc_ = new QLabel(g_schemas.isEmpty() ? "" : g_schemas[0].description);
    schema_desc_->setObjectName("dmInfoLabel");
    schema_desc_->setWordWrap(true);
    bl->addWidget(schema_desc_);

    // Fields table
    schema_fields_table_ = new QTableWidget;
    schema_fields_table_->setColumnCount(4);
    schema_fields_table_->setHorizontalHeaderLabels({"Field", "Type", "Required", "Description"});
    schema_fields_table_->horizontalHeader()->setStretchLastSection(true);
    schema_fields_table_->verticalHeader()->setVisible(false);
    schema_fields_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    schema_fields_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    schema_fields_table_->setMinimumHeight(200);

    // Populate with first schema
    if (!g_schemas.isEmpty()) {
        schema_select_->setCurrentIndex(0);
        emit schema_select_->currentIndexChanged(0);
    }

    bl->addWidget(schema_fields_table_);
    vl->addWidget(body);
    return panel;
}

QWidget* DataMappingScreen::create_field_mapping_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("3");
    icon->setObjectName("dmPanelHeaderIcon");
    auto* t = new QLabel("FIELD MAPPING");
    t->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(t);
    hl->addStretch(1);

    // Parser engine selector
    parser_engine_ = new QComboBox;
    parser_engine_->addItems({"JSONPath", "JSONata", "JMESPath", "Direct", "JavaScript", "Regex"});
    hl->addWidget(new QLabel("Parser:"));
    hl->addWidget(parser_engine_);
    vl->addWidget(hdr);

    // Split: JSON tree | mapping table
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    // JSON explorer
    json_tree_ = new QTreeWidget;
    json_tree_->setHeaderLabels({"Key", "Value", "Type"});
    json_tree_->setColumnWidth(0, 160);
    json_tree_->setColumnWidth(1, 200);
    json_tree_->setMinimumWidth(300);
    split->addWidget(json_tree_);

    // Mapping table
    mapping_table_ = new QTableWidget;
    mapping_table_->setColumnCount(4);
    mapping_table_->setHorizontalHeaderLabels({"Target Field", "Expression", "Transform", "Default"});
    mapping_table_->horizontalHeader()->setStretchLastSection(true);
    mapping_table_->verticalHeader()->setVisible(false);
    mapping_table_->setMinimumWidth(400);
    mapping_table_->setMinimumHeight(250);
    split->addWidget(mapping_table_);

    split->setSizes({350, 500});
    vl->addWidget(split, 1);

    return panel;
}

QWidget* DataMappingScreen::create_cache_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("4");
    icon->setObjectName("dmPanelHeaderIcon");
    auto* t = new QLabel("CACHE & SECURITY SETTINGS");
    t->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(t);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget;
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    cache_enabled_ = new QComboBox;
    cache_enabled_->addItems({"Enabled", "Disabled"});
    bl->addWidget(create_form_row("RESPONSE CACHING", cache_enabled_));

    cache_ttl_ = new QSpinBox;
    cache_ttl_->setRange(0, 86400);
    cache_ttl_->setValue(300);
    cache_ttl_->setSuffix(" sec");
    bl->addWidget(create_form_row("CACHE TTL", cache_ttl_));

    // Security info
    auto* sec_box = new QWidget;
    sec_box->setStyleSheet(
        QString("background: rgba(22,163,74,0.05); border: 1px solid %1; padding: 8px;").arg(colors::BORDER_DIM));
    auto* sbl = new QVBoxLayout(sec_box);
    sbl->setSpacing(4);
    auto* sec_title = new QLabel("ENCRYPTION");
    sec_title->setObjectName("dmLabel");
    sbl->addWidget(sec_title);
    auto* sec_detail = new QLabel("API credentials are encrypted with AES-256-GCM before storage.\n"
                                  "Sensitive data never stored in plaintext.");
    sec_detail->setObjectName("dmInfoLabel");
    sec_detail->setWordWrap(true);
    sbl->addWidget(sec_detail);
    bl->addWidget(sec_box);

    bl->addStretch(1);
    vl->addWidget(body);
    return panel;
}

QWidget* DataMappingScreen::create_test_save_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dmPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget;
    hdr->setObjectName("dmPanelHeader");
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* icon = new QLabel("5");
    icon->setObjectName("dmPanelHeaderIcon");
    auto* t = new QLabel("TEST & SAVE");
    t->setObjectName("dmPanelHeaderTitle");
    hl->addWidget(icon);
    hl->addSpacing(8);
    hl->addWidget(t);
    hl->addStretch(1);
    vl->addWidget(hdr);

    auto* body = new QWidget;
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->setSpacing(10);

    // Test controls
    auto* test_row = new QWidget;
    auto* trl = new QHBoxLayout(test_row);
    trl->setContentsMargins(0, 0, 0, 0);
    trl->setSpacing(8);

    test_btn_ = new QPushButton("RUN TEST");
    test_btn_->setObjectName("dmCalcBtn");
    test_btn_->setCursor(Qt::PointingHandCursor);
    connect(test_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_test_mapping);

    test_status_ = new QLabel("Not yet tested");
    test_status_->setObjectName("dmInfoLabel");

    trl->addWidget(test_btn_);
    trl->addWidget(test_status_);
    trl->addStretch(1);
    bl->addWidget(test_row);

    // Test output
    test_output_ = new QTextEdit;
    test_output_->setObjectName("dmTestOutput");
    test_output_->setReadOnly(true);
    test_output_->setMinimumHeight(200);
    test_output_->setPlaceholderText("Test results will appear here...");
    bl->addWidget(test_output_);

    // Save button
    save_btn_ = new QPushButton("SAVE MAPPING CONFIGURATION");
    save_btn_->setObjectName("dmSaveBtn");
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setFixedHeight(36);
    save_btn_->setEnabled(false);
    connect(save_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_save_mapping);
    bl->addWidget(save_btn_);

    vl->addWidget(body);
    return panel;
}

// ── View panels ─────────────────────────────────────────────────────────────

QWidget* DataMappingScreen::create_list_view() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Toolbar
    auto* toolbar = new QWidget;
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(0, 0, 0, 0);
    auto* list_title = new QLabel("SAVED MAPPINGS");
    list_title->setObjectName("dmPanelHeaderTitle");
    tbl->addWidget(list_title);
    tbl->addStretch(1);

    auto* run_btn = new QPushButton("▶ RUN");
    run_btn->setObjectName("dmCalcBtn");
    run_btn->setCursor(Qt::PointingHandCursor);
    connect(run_btn, &QPushButton::clicked, this, &DataMappingScreen::on_run_mapping);
    tbl->addWidget(run_btn);

    auto* del_btn = new QPushButton("DELETE");
    del_btn->setObjectName("dmDestructiveBtn");
    del_btn->setCursor(Qt::PointingHandCursor);
    connect(del_btn, &QPushButton::clicked, this, &DataMappingScreen::on_delete_mapping);
    tbl->addWidget(del_btn);

    auto* new_btn = new QPushButton("+ NEW MAPPING");
    new_btn->setObjectName("dmCalcBtn");
    new_btn->setCursor(Qt::PointingHandCursor);
    connect(new_btn, &QPushButton::clicked, this, &DataMappingScreen::on_new_mapping);
    tbl->addWidget(new_btn);
    vl->addWidget(toolbar);

    // Mapping list
    mapping_list_ = new QListWidget;
    vl->addWidget(mapping_list_, 1);

    // Empty state
    auto* empty = new QLabel("No mappings saved yet.\nClick CREATE to build your first data mapping.");
    empty->setObjectName("dmEmptyState");
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    vl->addWidget(empty);

    scroll->setWidget(content);
    return scroll;
}

QWidget* DataMappingScreen::create_template_view() {
    auto* outer = new QWidget;
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);

    // Template list
    template_list_ = new QListWidget;
    for (int i = 0; i < g_templates.size(); ++i) {
        auto* item = new QListWidgetItem(g_templates[i].name);
        item->setData(Qt::UserRole, i);
        item->setToolTip(g_templates[i].description);
        template_list_->addItem(item);
    }
    connect(template_list_, &QListWidget::currentRowChanged, this, &DataMappingScreen::on_template_selected);
    split->addWidget(template_list_);

    // Template detail
    auto* detail_scroll = new QScrollArea;
    detail_scroll->setWidgetResizable(true);
    auto* detail_content = new QWidget;
    auto* dvl = new QVBoxLayout(detail_content);
    dvl->setContentsMargins(16, 16, 16, 16);
    dvl->setSpacing(8);

    template_detail_ = new QLabel("Select a template to view details");
    template_detail_->setObjectName("dmInfoLabel");
    template_detail_->setWordWrap(true);
    template_detail_->setAlignment(Qt::AlignTop);
    dvl->addWidget(template_detail_);
    dvl->addStretch(1);

    auto* use_btn = new QPushButton("USE THIS TEMPLATE");
    use_btn->setObjectName("dmCalcBtn");
    use_btn->setCursor(Qt::PointingHandCursor);
    connect(use_btn, &QPushButton::clicked, this, [this]() {
        int row = template_list_->currentRow();
        if (row >= 0 && row < g_templates.size()) {
            const auto& tmpl = g_templates[row];
            // Pre-populate all form fields from template
            api_name_->setText(tmpl.name);
            api_base_url_->setText(tmpl.base_url);
            api_endpoint_->setText(tmpl.endpoint);
            api_method_->setCurrentText(tmpl.method);
            api_auth_type_->setCurrentText(tmpl.auth_type);
            api_headers_->setPlainText(tmpl.headers);
            api_body_->setPlainText(tmpl.body);
            schema_select_->setCurrentText(tmpl.schema);
            parser_engine_->setCurrentText(tmpl.parser);

            // Pre-populate field mapping table
            mapping_table_->setRowCount(tmpl.field_mappings.size());
            for (int i = 0; i < tmpl.field_mappings.size(); ++i) {
                const auto& fm = tmpl.field_mappings[i];
                auto* name_item = new QTableWidgetItem(fm.target);
                name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
                mapping_table_->setItem(i, 0, name_item);
                mapping_table_->setItem(i, 1, new QTableWidgetItem(fm.expression));
                mapping_table_->setItem(i, 2, new QTableWidgetItem(fm.transform));
                mapping_table_->setItem(i, 3, new QTableWidgetItem(fm.default_val));
            }

            on_view_changed(2); // switch to create
            on_step_changed(0);
            LOG_INFO("DataMapping", "Template applied: " + tmpl.name);
        }
    });
    dvl->addWidget(use_btn);

    detail_scroll->setWidget(detail_content);
    split->addWidget(detail_scroll);
    split->setSizes({350, 500});

    auto* vl = new QVBoxLayout(outer);
    vl->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    auto* toolbar = new QWidget;
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(16, 8, 16, 8);
    auto* tt = new QLabel("BROKER TEMPLATES");
    tt->setObjectName("dmPanelHeaderTitle");
    tbl->addWidget(tt);
    tbl->addStretch(1);
    auto* count = new QLabel(QString::number(g_templates.size()) + " templates");
    count->setObjectName("dmInfoLabel");
    tbl->addWidget(count);
    vl->addWidget(toolbar);
    vl->addWidget(split, 1);

    return outer;
}

// ── Footer / Status ─────────────────────────────────────────────────────────

QWidget* DataMappingScreen::create_nav_footer() {
    auto* bar = new QWidget;
    bar->setObjectName("dmNavFooter");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    prev_btn_ = new QPushButton("PREVIOUS");
    prev_btn_->setObjectName("dmSecondaryBtn");
    prev_btn_->setCursor(Qt::PointingHandCursor);
    prev_btn_->setEnabled(false);
    connect(prev_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_prev_step);

    step_label_ = new QLabel("Step 1 of 5 — API CONFIG");
    step_label_->setObjectName("dmStatusText");
    step_label_->setAlignment(Qt::AlignCenter);

    next_btn_ = new QPushButton("NEXT");
    next_btn_->setObjectName("dmCalcBtn");
    next_btn_->setCursor(Qt::PointingHandCursor);
    connect(next_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_next_step);

    hl->addWidget(prev_btn_);
    hl->addStretch(1);
    hl->addWidget(step_label_);
    hl->addStretch(1);
    hl->addWidget(next_btn_);

    return bar;
}

QWidget* DataMappingScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("dmStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* ver = new QLabel("DATA MAPPING v1.0");
    ver->setObjectName("dmStatusText");
    hl->addWidget(ver);
    hl->addStretch(1);

    status_view_ = new QLabel("VIEW: MAPPINGS");
    status_view_->setObjectName("dmStatusText");
    hl->addWidget(status_view_);

    hl->addSpacing(16);
    status_step_ = new QLabel;
    status_step_->setObjectName("dmStatusHighlight");
    hl->addWidget(status_step_);

    return bar;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

QWidget* DataMappingScreen::create_form_row(const QString& label_text, QWidget* input) {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    auto* lbl = new QLabel(label_text);
    lbl->setObjectName("dmLabel");
    vl->addWidget(lbl);
    vl->addWidget(input);
    return w;
}

QWidget* DataMappingScreen::create_form_two_col(QWidget* left, QWidget* right) {
    auto* w = new QWidget;
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(left, 1);
    hl->addWidget(right, 1);
    return w;
}

void DataMappingScreen::update_step_indicators() {
    const QStringList labels = {"API CONFIG", "SCHEMA", "FIELD MAPPING", "CACHE", "TEST & SAVE"};
    for (int i = 0; i < step_btns_.size(); ++i) {
        step_btns_[i]->setProperty("active", i == current_step_);
        step_btns_[i]->setProperty("complete", i < current_step_);
        step_btns_[i]->style()->unpolish(step_btns_[i]);
        step_btns_[i]->style()->polish(step_btns_[i]);
    }
    step_label_->setText(QString("Step %1 of 5 — %2").arg(current_step_ + 1).arg(labels[current_step_]));
    prev_btn_->setEnabled(current_step_ > 0);
    next_btn_->setEnabled(current_step_ < 4);
    status_step_->setText(labels[current_step_]);
}

void DataMappingScreen::populate_json_tree(const QJsonValue& val, QTreeWidgetItem* parent, const QString& key) {
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(json_tree_);
    item->setText(0, key);

    if (val.isObject()) {
        item->setText(2, "object");
        auto obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            populate_json_tree(it.value(), item, it.key());
        }
    } else if (val.isArray()) {
        auto arr = val.toArray();
        item->setText(2, QString("array[%1]").arg(arr.size()));
        int limit = qMin(arr.size(), 10); // limit display
        for (int i = 0; i < limit; ++i) {
            populate_json_tree(arr[i], item, QString("[%1]").arg(i));
        }
    } else if (val.isDouble()) {
        item->setText(1, QString::number(val.toDouble(), 'g', 10));
        item->setText(2, "number");
        item->setForeground(1, QColor(colors::CYAN()));
    } else if (val.isBool()) {
        item->setText(1, val.toBool() ? "true" : "false");
        item->setText(2, "boolean");
        item->setForeground(1, QColor(colors::WARNING()));
    } else if (val.isNull()) {
        item->setText(1, "null");
        item->setText(2, "null");
        item->setForeground(1, QColor(colors::TEXT_DIM()));
    } else {
        item->setText(1, val.toString());
        item->setText(2, "string");
    }
}

void DataMappingScreen::populate_mapping_list() {
    // Populate the mapping table with schema fields
    int schema_idx = schema_select_->currentIndex();
    if (schema_idx < 0 || schema_idx >= g_schemas.size())
        return;

    const auto& fields = g_schemas[schema_idx].fields;
    mapping_table_->setRowCount(fields.size());
    for (int i = 0; i < fields.size(); ++i) {
        auto* name_item = new QTableWidgetItem(fields[i].name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        mapping_table_->setItem(i, 0, name_item);
        mapping_table_->setItem(i, 1, new QTableWidgetItem("")); // expression
        mapping_table_->setItem(i, 2, new QTableWidgetItem("")); // transform
        mapping_table_->setItem(i, 3, new QTableWidgetItem("")); // default
    }
}

// ── Slots ───────────────────────────────────────────────────────────────────

void DataMappingScreen::on_view_changed(int view) {
    current_view_ = view;
    view_stack_->setCurrentIndex(view);

    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }

    const QStringList names = {"MAPPINGS", "TEMPLATES", "CREATE"};
    status_view_->setText("VIEW: " + names[view]);

    // Show/hide step bar and nav footer — CREATE is index 2
    nav_footer_->setVisible(view == 2); // only in create mode

    if (view == 2) {
        update_step_indicators();
    }

    LOG_INFO("DataMapping", "View: " + names[view]);
}

void DataMappingScreen::on_step_changed(int step) {
    if (step < 0 || step > 4)
        return;
    current_step_ = step;
    step_stack_->setCurrentIndex(step);
    update_step_indicators();

    // Populate field mapping table when entering step 2
    if (step == 2) {
        populate_mapping_list();
        // Populate JSON tree if we have sample data
        if (!sample_data_.isNull()) {
            json_tree_->clear();
            populate_json_tree(sample_data_.isObject() ? QJsonValue(sample_data_.object())
                                                       : QJsonValue(sample_data_.array()),
                               nullptr, "root");
            json_tree_->expandToDepth(1);
        }
    }

    LOG_INFO("DataMapping", "Step: " + QString::number(step + 1));
}

void DataMappingScreen::on_next_step() {
    if (current_step_ < 4) {
        on_step_changed(current_step_ + 1);
    }
}

void DataMappingScreen::on_prev_step() {
    if (current_step_ > 0) {
        on_step_changed(current_step_ - 1);
    }
}

void DataMappingScreen::on_test_api() {
    QString url = api_base_url_->text().trimmed() + api_endpoint_->text().trimmed();
    if (url.isEmpty()) {
        api_test_status_->setText("Enter a URL first");
        return;
    }

    api_test_btn_->setEnabled(false);
    api_test_status_->setText("Testing...");

    // Use HttpClient for the test request
    auto callback = [this](Result<QJsonDocument> result) {
        api_test_btn_->setEnabled(true);
        if (result.is_ok()) {
            sample_data_ = result.value();
            api_test_status_->setText("SUCCESS — Sample data received");
            api_test_status_->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(colors::POSITIVE));
            LOG_INFO("DataMapping", "API test success");
        } else {
            api_test_status_->setText("FAILED — " + QString::fromStdString(result.error()));
            api_test_status_->setStyleSheet(
                QString("color: %1; font-size: 9px; background: transparent;").arg(colors::NEGATIVE));
            LOG_ERROR("DataMapping", "API test failed: " + QString::fromStdString(result.error()));
        }
    };

    QString method = api_method_->currentText();
    if (method == "POST" || method == "PUT" || method == "PATCH") {
        QJsonObject body;
        if (!api_body_->toPlainText().trimmed().isEmpty()) {
            auto doc = QJsonDocument::fromJson(api_body_->toPlainText().toUtf8());
            if (doc.isObject())
                body = doc.object();
        }
        if (method == "POST")
            HttpClient::instance().post(url, body, callback);
        else
            HttpClient::instance().put(url, body, callback);
    } else {
        HttpClient::instance().get(url, callback);
    }
}

void DataMappingScreen::on_test_mapping() {
    if (sample_data_.isNull()) {
        test_status_->setText("No sample data — test API first (Step 1)");
        return;
    }

    test_btn_->setEnabled(false);
    test_status_->setText("Running test...");

    // Build a summary of the mapping configuration
    QJsonObject summary;
    summary["name"] = api_name_->text();
    summary["schema"] = schema_select_->currentText();
    summary["parser"] = parser_engine_->currentText();
    summary["cache_enabled"] = cache_enabled_->currentIndex() == 0;
    summary["cache_ttl"] = cache_ttl_->value();

    // Collect field mappings from the table
    QJsonArray mappings;
    for (int r = 0; r < mapping_table_->rowCount(); ++r) {
        auto* expr_item = mapping_table_->item(r, 1);
        if (expr_item && !expr_item->text().trimmed().isEmpty()) {
            QJsonObject m;
            m["target"] = mapping_table_->item(r, 0)->text();
            m["expression"] = expr_item->text();
            auto* trans = mapping_table_->item(r, 2);
            if (trans && !trans->text().isEmpty())
                m["transform"] = trans->text();
            auto* def = mapping_table_->item(r, 3);
            if (def && !def->text().isEmpty())
                m["default"] = def->text();
            mappings.append(m);
        }
    }
    summary["field_mappings"] = mappings;
    summary["mapping_count"] = mappings.size();

    // Display test result
    QJsonObject result;
    result["success"] = mappings.size() > 0;
    result["config"] = summary;
    result["sample_data_keys"] = QJsonArray();
    if (sample_data_.isObject()) {
        for (auto it = sample_data_.object().begin(); it != sample_data_.object().end(); ++it) {
            QJsonArray keys = result["sample_data_keys"].toArray();
            keys.append(it.key());
            result["sample_data_keys"] = keys;
        }
    }

    test_output_->setPlainText(QJsonDocument(result).toJson(QJsonDocument::Indented));
    test_result_ = result;

    bool success = mappings.size() > 0;
    if (success) {
        test_status_->setText("TEST PASSED");
        test_status_->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(colors::POSITIVE));
        save_btn_->setEnabled(true);
        right_test_info_->setText("Test: PASSED");
    } else {
        test_status_->setText("TEST FAILED — No field mappings configured");
        test_status_->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(colors::NEGATIVE));
        save_btn_->setEnabled(false);
        right_test_info_->setText("Test: FAILED");
    }

    test_btn_->setEnabled(true);
    LOG_INFO("DataMapping", "Test: " + QString(success ? "PASSED" : "FAILED"));
}

void DataMappingScreen::on_save_mapping() {
    const QString name = api_name_->text().trimmed();
    if (name.isEmpty()) {
        test_status_->setText("Enter a mapping name first");
        return;
    }

    QJsonObject config;
    build_mapping_config(config);

    DataMapping dm;
    dm.id                  = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dm.name                = name;
    dm.schema_name         = schema_select_->currentText();
    dm.base_url            = api_base_url_->text().trimmed();
    dm.endpoint            = api_endpoint_->text().trimmed();
    dm.method              = api_method_->currentText();
    dm.auth_type           = api_auth_type_->currentText();
    dm.auth_token          = api_auth_value_->text().trimmed();
    dm.headers             = api_headers_->toPlainText().trimmed();
    dm.body                = api_body_->toPlainText().trimmed();
    dm.parser              = parser_engine_->currentText();
    dm.cache_enabled       = cache_enabled_->currentIndex() == 0;
    dm.cache_ttl           = cache_ttl_->value();
    dm.field_mappings_json =
        QJsonDocument(config["field_mappings"].toArray()).toJson(QJsonDocument::Compact);

    auto r = DataMappingRepository::instance().save(dm);
    if (r.is_err()) {
        LOG_ERROR("DataMapping", "Failed to save: " + QString::fromStdString(r.error()));
        test_status_->setText("Save failed — database error");
        return;
    }

    load_mappings_from_db();
    on_view_changed(0);
    LOG_INFO("DataMapping", "Mapping saved: " + name);
}

void DataMappingScreen::on_run_mapping() {
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size()) return;

    const DataMapping& dm = saved_mappings_[row];
    LOG_INFO("DataMapping", "Running mapping: " + dm.name);

    QPointer<DataMappingScreen> self = this;
    fincept::services::DataNormalizationService::instance().fetch_and_normalize(
        dm, [self, dm](bool ok, fincept::services::NormalizedRecord rec) {
            if (!self) return;
            if (ok) {
                const QString out =
                    QJsonDocument(rec.normalized).toJson(QJsonDocument::Indented);
                self->test_output_->setPlainText(out);
                self->test_status_->setText(
                    QString("RUN OK — %1 fields extracted").arg(rec.normalized.size()));
                LOG_INFO("DataMapping", "Run complete: " + dm.name);
            } else {
                const QString errs = rec.errors.join(", ");
                self->test_status_->setText("RUN FAILED — " + errs);
                LOG_WARN("DataMapping", "Run failed: " + errs);
            }
        });
}

void DataMappingScreen::on_template_selected(int index) {
    if (index < 0 || index >= g_templates.size())
        return;

    const auto& tmpl = g_templates[index];
    QString detail = QString("NAME: %1\n\n"
                             "PROVIDER: %2\n"
                             "SCHEMA: %3\n"
                             "VERIFIED: %4\n\n"
                             "DESCRIPTION:\n%5\n\n"
                             "API DETAILS:\n"
                             "  Base URL: %6\n"
                             "  Endpoint: %7\n"
                             "  Method: %8\n"
                             "  Auth: %9\n\n"
                             "FIELD MAPPINGS: %10 fields configured\n"
                             "PARSER: %11\n\n"
                             "TAGS: %12")
                         .arg(tmpl.name, tmpl.broker, tmpl.schema,
                              tmpl.verified ? "Yes" : "No", tmpl.description,
                              tmpl.base_url, tmpl.endpoint, tmpl.method,
                              tmpl.auth_type, QString::number(tmpl.field_mappings.size()),
                              tmpl.parser, tmpl.tags.join(", "));

    template_detail_->setText(detail);
}

void DataMappingScreen::on_new_mapping() {
    // Reset form
    api_name_->clear();
    api_base_url_->clear();
    api_endpoint_->clear();
    api_auth_value_->clear();
    api_headers_->clear();
    api_body_->clear();
    api_test_status_->clear();
    sample_data_ = QJsonDocument();
    test_result_ = QJsonObject();
    test_output_->clear();
    test_status_->setText("Not yet tested");
    save_btn_->setEnabled(false);
    right_test_info_->setText("Test: --");

    on_view_changed(2); // create
    on_step_changed(0);
}

void DataMappingScreen::on_delete_mapping() {
    const int row = mapping_list_->currentRow();
    if (row < 0 || row >= saved_mappings_.size()) return;

    const QString id = saved_mappings_[row].id;
    auto r = DataMappingRepository::instance().remove(id);
    if (r.is_err()) {
        LOG_ERROR("DataMapping", "Failed to delete: " + QString::fromStdString(r.error()));
        return;
    }

    load_mappings_from_db();
    LOG_INFO("DataMapping", "Mapping deleted");
}

void DataMappingScreen::load_mappings_from_db() {
    saved_mappings_.clear();
    mapping_list_->clear();

    auto r = DataMappingRepository::instance().list_all();
    if (r.is_err()) {
        LOG_WARN("DataMapping", "Could not load mappings: " + QString::fromStdString(r.error()));
        return;
    }

    saved_mappings_ = r.value();
    for (const auto& dm : saved_mappings_) {
        mapping_list_->addItem(dm.name + " — " + dm.schema_name);
    }

    if (status_mappings_) {
        status_mappings_->setText("Saved: " + QString::number(saved_mappings_.size()));
    }
}

void DataMappingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load_mappings_from_db();
}

void DataMappingScreen::build_mapping_config(QJsonObject& config) {
    config["name"] = api_name_->text();
    config["base_url"] = api_base_url_->text();
    config["endpoint"] = api_endpoint_->text();
    config["method"] = api_method_->currentText();
    config["auth_type"] = api_auth_type_->currentText();
    config["schema"] = schema_select_->currentText();
    config["parser"] = parser_engine_->currentText();
    config["cache_enabled"] = cache_enabled_->currentIndex() == 0;
    config["cache_ttl"] = cache_ttl_->value();

    QJsonArray mappings;
    for (int r = 0; r < mapping_table_->rowCount(); ++r) {
        auto* expr_item = mapping_table_->item(r, 1);
        if (expr_item && !expr_item->text().trimmed().isEmpty()) {
            QJsonObject m;
            m["target"] = mapping_table_->item(r, 0)->text();
            m["expression"] = expr_item->text();
            mappings.append(m);
        }
    }
    config["field_mappings"] = mappings;
}

} // namespace fincept::screens
