#pragma once
// Markets tab — data types

#include <string>
#include <vector>
#include <map>

namespace fincept::markets {

struct MarketQuote {
    std::string symbol;
    std::string name;       // optional display name
    double price          = 0;
    double change         = 0;
    double change_percent = 0;
    double high           = 0;
    double low            = 0;
    double open           = 0;
    double previous_close = 0;
    double volume         = 0;
};

struct MarketCategory {
    std::string title;
    std::vector<std::string> symbols;
    std::map<std::string, std::string> names;  // symbol -> display name (for regional)
    bool show_name = false;                     // show name column
};

// Default ticker categories
inline std::vector<MarketCategory> default_global_markets() {
    return {
        {"Stock Indices", {
            "^GSPC", "^IXIC", "^DJI", "^RUT", "^VIX",
            "^FTSE", "^GDAXI", "^N225", "^FCHI", "^HSI",
            "^AXJO", "^BSESN", "^NSEI", "^STOXX50E"
        }, {}, false},
        {"Forex", {
            "EURUSD=X", "GBPUSD=X", "USDJPY=X", "USDCHF=X",
            "USDCAD=X", "AUDUSD=X", "NZDUSD=X", "EURGBP=X",
            "EURJPY=X", "GBPJPY=X", "USDCNY=X", "USDINR=X"
        }, {}, false},
        {"Commodities", {
            "GC=F", "SI=F", "PL=F", "HG=F",
            "CL=F", "BZ=F", "NG=F",
            "ZC=F", "ZW=F", "ZS=F",
            "KC=F", "CT=F", "SB=F", "CC=F"
        }, {}, false},
        {"Bonds & Fixed Income", {
            "^TNX", "^TYX", "^IRX", "^FVX",
            "TLT", "IEF", "SHY", "BND", "AGG", "LQD", "HYG", "JNK"
        }, {}, false},
        {"ETFs", {
            "SPY", "QQQ", "DIA", "EEM", "GLD",
            "XLK", "XLE", "XLF", "XLV", "VNQ", "IWM", "VTI"
        }, {}, false},
        {"Cryptocurrencies", {
            "BTC-USD", "ETH-USD", "BNB-USD", "SOL-USD",
            "XRP-USD", "ADA-USD", "DOGE-USD", "LINK-USD",
            "DOT-USD", "AVAX-USD", "UNI-USD", "ATOM-USD"
        }, {}, false}
    };
}

inline std::vector<MarketCategory> default_regional_markets() {
    return {
        {"India", {
            "RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS",
            "HINDUNILVR.NS", "ICICIBANK.NS", "SBIN.NS", "BHARTIARTL.NS",
            "ITC.NS", "KOTAKBANK.NS", "LT.NS", "WIPRO.NS"
        }, {
            {"RELIANCE.NS", "Reliance Industries"},
            {"TCS.NS", "Tata Consultancy"},
            {"HDFCBANK.NS", "HDFC Bank"},
            {"INFY.NS", "Infosys"},
            {"HINDUNILVR.NS", "Hindustan Unilever"},
            {"ICICIBANK.NS", "ICICI Bank"},
            {"SBIN.NS", "State Bank of India"},
            {"BHARTIARTL.NS", "Bharti Airtel"},
            {"ITC.NS", "ITC Limited"},
            {"KOTAKBANK.NS", "Kotak Mahindra Bank"},
            {"LT.NS", "Larsen & Toubro"},
            {"WIPRO.NS", "Wipro Limited"}
        }, true},
        {"China", {
            "BABA", "PDD", "JD", "BIDU",
            "NIO", "LI", "XPEV", "BILI",
            "NTES", "ZTO", "VNET", "TAL"
        }, {
            {"BABA", "Alibaba Group"},
            {"PDD", "PDD Holdings"},
            {"JD", "JD.com"},
            {"BIDU", "Baidu"},
            {"NIO", "NIO Inc"},
            {"LI", "Li Auto"},
            {"XPEV", "XPeng"},
            {"BILI", "Bilibili"},
            {"NTES", "NetEase"},
            {"ZTO", "ZTO Express"},
            {"VNET", "VNET Group"},
            {"TAL", "TAL Education"}
        }, true},
        {"United States", {
            "AAPL", "MSFT", "GOOGL", "AMZN",
            "NVDA", "META", "TSLA", "JPM",
            "V", "WMT", "UNH", "MA"
        }, {
            {"AAPL", "Apple Inc"},
            {"MSFT", "Microsoft Corp"},
            {"GOOGL", "Alphabet Inc"},
            {"AMZN", "Amazon.com"},
            {"NVDA", "NVIDIA Corp"},
            {"META", "Meta Platforms"},
            {"TSLA", "Tesla Inc"},
            {"JPM", "JPMorgan Chase"},
            {"V", "Visa Inc"},
            {"WMT", "Walmart Inc"},
            {"UNH", "UnitedHealth Group"},
            {"MA", "Mastercard Inc"}
        }, true}
    };
}

} // namespace fincept::markets
