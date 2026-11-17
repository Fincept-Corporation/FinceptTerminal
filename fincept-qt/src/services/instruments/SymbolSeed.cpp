// SymbolSeed.cpp — see SymbolSeed.h. File-scope names carry an ss_ / kSs prefix
// (unity builds). Non-ASCII text is written as UTF-8 escapes (MSVC without /utf-8);
// an escape followed by a hex digit is split into two literals ("L'Or\xc3\xa9" "al").

#include "services/instruments/SymbolSeed.h"

#include "core/instrument/VenueSymbols.h"

#include <QStringList>

namespace fincept::services {

namespace {

using SsInstrument = core::instrument::Instrument;
using SsClass = core::instrument::AssetClass;

enum : unsigned {
    kSsPrimary = 1u,  ///< the issuer's primary / home listing
    kSsEtf = 2u,      ///< exchange-traded fund
    kSsBseTwin = 4u,  ///< also seed the BSE listing (<code>.BO, same ISIN, not primary)
};

struct SsCompany {
    const char* symbol;  ///< Yahoo form
    const char* name;
    const char* aliases; ///< '|'-separated, may be empty
    int weight;          ///< rank_weight: 10 mega cap, 9 large cap, 8 / 7 smaller
    unsigned flags;
    const char* isin;    ///< empty when not seeded (only ISINs the unit test Luhn-checks)
    const char* former;  ///< '|'-separated former tickers of THIS listing, may be empty
};

// Companies of the news directory's built-in list (NewsTickerTagger.cpp kNtBuiltins), with
// home listings for its ADRs, plus a few major ETFs. US ADRs are not primary listings.
const SsCompany kSsCompanies[] = {
    // ── United States ──
    {"AAPL", "Apple Inc.", "Apple", 10, kSsPrimary, "US0378331005", ""},
    {"MSFT", "Microsoft Corporation", "Microsoft", 10, kSsPrimary, "US5949181045", ""},
    {"NVDA", "NVIDIA Corporation", "Nvidia", 10, kSsPrimary, "US67066G1040", ""},
    {"AMZN", "Amazon.com, Inc.", "Amazon", 10, kSsPrimary, "US0231351067", ""},
    {"GOOGL", "Alphabet Inc. Class A", "Alphabet|Google", 10, kSsPrimary, "US02079K3059", ""},
    {"GOOG", "Alphabet Inc. Class C", "Alphabet|Google", 9, kSsPrimary, "", ""}, // GOOGL is the line users mean
    {"META", "Meta Platforms, Inc.", "Meta|Facebook", 10, kSsPrimary, "US30303M1027", "FB"},
    {"TSLA", "Tesla, Inc.", "Tesla", 10, kSsPrimary, "US88160R1014", ""},
    {"BRK-B", "Berkshire Hathaway Inc. Class B", "Berkshire Hathaway|Berkshire", 10, kSsPrimary, "", ""},
    {"JPM", "JPMorgan Chase & Co.", "JPMorgan|JP Morgan", 10, kSsPrimary, "", ""},
    {"V", "Visa Inc.", "Visa", 10, kSsPrimary, "", ""},
    {"MA", "Mastercard Incorporated", "Mastercard", 10, kSsPrimary, "", ""},
    {"UNH", "UnitedHealth Group Incorporated", "UnitedHealth", 9, kSsPrimary, "", ""},
    {"XOM", "Exxon Mobil Corporation", "Exxon|ExxonMobil", 9, kSsPrimary, "", ""},
    {"JNJ", "Johnson & Johnson", "", 9, kSsPrimary, "", ""},
    {"WMT", "Walmart Inc.", "Walmart", 10, kSsPrimary, "", ""},
    {"PG", "The Procter & Gamble Company", "Procter & Gamble|P&G", 9, kSsPrimary, "", ""},
    {"HD", "The Home Depot, Inc.", "Home Depot", 9, kSsPrimary, "", ""},
    {"CVX", "Chevron Corporation", "Chevron", 9, kSsPrimary, "", ""},
    {"LLY", "Eli Lilly and Company", "Eli Lilly|Lilly", 10, kSsPrimary, "", ""},
    {"AVGO", "Broadcom Inc.", "Broadcom", 10, kSsPrimary, "", ""},
    {"ORCL", "Oracle Corporation", "Oracle", 10, kSsPrimary, "", ""},
    {"COST", "Costco Wholesale Corporation", "Costco", 9, kSsPrimary, "", ""},
    {"KO", "The Coca-Cola Company", "Coca-Cola|Coke", 9, kSsPrimary, "", ""},
    {"PEP", "PepsiCo, Inc.", "PepsiCo|Pepsi", 9, kSsPrimary, "", ""},
    {"BAC", "Bank of America Corporation", "Bank of America", 9, kSsPrimary, "", ""},
    {"WFC", "Wells Fargo & Company", "Wells Fargo", 9, kSsPrimary, "", ""},
    {"C", "Citigroup Inc.", "Citigroup|Citi", 9, kSsPrimary, "", ""},
    {"GS", "The Goldman Sachs Group, Inc.", "Goldman Sachs|Goldman", 9, kSsPrimary, "", ""},
    {"MS", "Morgan Stanley", "", 9, kSsPrimary, "", ""},
    {"BLK", "BlackRock, Inc.", "BlackRock", 9, kSsPrimary, "", ""},
    {"AMD", "Advanced Micro Devices, Inc.", "AMD", 9, kSsPrimary, "", ""},
    {"INTC", "Intel Corporation", "Intel", 8, kSsPrimary, "", ""},
    {"CSCO", "Cisco Systems, Inc.", "Cisco", 9, kSsPrimary, "", ""},
    {"ADBE", "Adobe Inc.", "Adobe", 9, kSsPrimary, "", ""},
    {"CRM", "Salesforce, Inc.", "Salesforce", 9, kSsPrimary, "", ""},
    {"NFLX", "Netflix, Inc.", "Netflix", 9, kSsPrimary, "", ""},
    {"DIS", "The Walt Disney Company", "Disney|Walt Disney", 9, kSsPrimary, "", ""},
    {"NKE", "NIKE, Inc.", "Nike", 8, kSsPrimary, "", ""},
    {"MCD", "McDonald's Corporation", "McDonald's|McDonalds", 9, kSsPrimary, "", ""},
    {"BA", "The Boeing Company", "Boeing", 9, kSsPrimary, "", ""},
    {"CAT", "Caterpillar Inc.", "Caterpillar", 9, kSsPrimary, "", ""},
    {"GE", "GE Aerospace", "General Electric", 9, kSsPrimary, "", ""},
    {"GEV", "GE Vernova Inc.", "GE Vernova", 8, kSsPrimary, "", ""},
    {"IBM", "International Business Machines Corporation", "IBM", 9, kSsPrimary, "", ""},
    {"QCOM", "QUALCOMM Incorporated", "Qualcomm", 9, kSsPrimary, "", ""},
    {"TXN", "Texas Instruments Incorporated", "Texas Instruments", 9, kSsPrimary, "", ""},
    {"MU", "Micron Technology, Inc.", "Micron", 9, kSsPrimary, "", ""},
    {"PFE", "Pfizer Inc.", "Pfizer", 8, kSsPrimary, "", ""},
    {"MRK", "Merck & Co., Inc.", "Merck", 9, kSsPrimary, "", ""},
    {"ABBV", "AbbVie Inc.", "AbbVie", 9, kSsPrimary, "", ""},
    {"T", "AT&T Inc.", "AT&T", 9, kSsPrimary, "", ""},
    {"VZ", "Verizon Communications Inc.", "Verizon", 9, kSsPrimary, "", ""},
    {"UBER", "Uber Technologies, Inc.", "Uber", 9, kSsPrimary, "", ""},
    {"ABNB", "Airbnb, Inc.", "Airbnb", 8, kSsPrimary, "", ""},
    {"PYPL", "PayPal Holdings, Inc.", "PayPal", 8, kSsPrimary, "", ""},
    {"SBUX", "Starbucks Corporation", "Starbucks", 8, kSsPrimary, "", ""},
    {"F", "Ford Motor Company", "Ford", 8, kSsPrimary, "", ""},
    {"GM", "General Motors Company", "General Motors", 8, kSsPrimary, "", ""},
    {"COIN", "Coinbase Global, Inc.", "Coinbase", 8, kSsPrimary, "", ""},
    {"PLTR", "Palantir Technologies Inc.", "Palantir", 9, kSsPrimary, "", ""},
    {"SMCI", "Super Micro Computer, Inc.", "Supermicro|Super Micro", 7, kSsPrimary, "", ""},
    {"ARM", "Arm Holdings plc", "Arm", 8, kSsPrimary, "", ""},
    {"SNOW", "Snowflake Inc.", "Snowflake", 8, kSsPrimary, "", ""},
    {"SHOP", "Shopify Inc.", "Shopify", 9, 0, "", ""},
    {"SHOP.TO", "Shopify Inc.", "Shopify", 9, kSsPrimary, "", ""},
    // ── China / Hong Kong / Taiwan ──
    {"BABA", "Alibaba Group Holding Limited", "Alibaba|\xe9\x98\xbf\xe9\x87\x8c\xe5\xb7\xb4\xe5\xb7\xb4", 9, kSsPrimary,
     "", ""},
    {"9988.HK", "Alibaba Group Holding Limited", "Alibaba|\xe9\x98\xbf\xe9\x87\x8c\xe5\xb7\xb4\xe5\xb7\xb4", 9, 0, "",
     ""},
    {"PDD", "PDD Holdings Inc.", "Pinduoduo|Temu|\xe6\x8b\xbc\xe5\xa4\x9a\xe5\xa4\x9a", 9, kSsPrimary, "", ""},
    {"JD", "JD.com, Inc.", "JD|\xe4\xba\xac\xe4\xb8\x9c", 8, kSsPrimary, "", ""},
    {"BIDU", "Baidu, Inc.", "Baidu|\xe7\x99\xbe\xe5\xba\xa6", 8, kSsPrimary, "", ""},
    {"NIO", "NIO Inc.", "Nio|\xe8\x94\x9a\xe6\x9d\xa5", 7, kSsPrimary, "", ""},
    {"TSM", "Taiwan Semiconductor Manufacturing Company Limited",
     "TSMC|\xe5\x8f\xb0\xe7\xa7\xaf\xe7\x94\xb5|\xe5\x8f\xb0\xe7\xa9\x8d\xe9\x9b\xbb", 10, 0, "", ""},
    {"2330.TW", "Taiwan Semiconductor Manufacturing Company Limited",
     "TSMC|\xe5\x8f\xb0\xe7\xa7\xaf\xe7\x94\xb5|\xe5\x8f\xb0\xe7\xa9\x8d\xe9\x9b\xbb", 10, kSsPrimary, "", ""},
    {"0700.HK", "Tencent Holdings Limited",
     "Tencent|\xe8\x85\xbe\xe8\xae\xaf|\xe9\xa8\xb0\xe8\xa8\x8a|\xe8\x85\xbe\xe8\xae\xaf\xe6\x8e\xa7\xe8\x82\xa1", 10,
     kSsPrimary, "", ""},
    {"3690.HK", "Meituan", "\xe7\xbe\x8e\xe5\x9b\xa2", 9, kSsPrimary, "", ""},
    {"1211.HK", "BYD Company Limited", "BYD|\xe6\xaf\x94\xe4\xba\x9a\xe8\xbf\xaa", 9, kSsPrimary, "", ""},
    {"1810.HK", "Xiaomi Corporation", "Xiaomi|\xe5\xb0\x8f\xe7\xb1\xb3", 9, kSsPrimary, "", ""},
    {"0939.HK", "China Construction Bank Corporation", "\xe5\xbb\xba\xe8\xae\xbe\xe9\x93\xb6\xe8\xa1\x8c", 9,
     kSsPrimary, "", ""},
    {"1398.HK", "Industrial and Commercial Bank of China Limited", "ICBC|\xe5\xb7\xa5\xe5\x95\x86\xe9\x93\xb6\xe8\xa1\x8c",
     9, kSsPrimary, "", ""},
    {"0941.HK", "China Mobile Limited", "\xe4\xb8\xad\xe5\x9b\xbd\xe7\xa7\xbb\xe5\x8a\xa8", 9, kSsPrimary, "", ""},
    {"2318.HK", "Ping An Insurance (Group) Company of China, Ltd.", "Ping An|\xe4\xb8\xad\xe5\x9b\xbd\xe5\xb9\xb3\xe5\xae\x89",
     9, kSsPrimary, "", ""},
    {"0388.HK", "Hong Kong Exchanges and Clearing Limited", "HKEX", 8, kSsPrimary, "", ""},
    {"1024.HK", "Kuaishou Technology", "Kuaishou|\xe5\xbf\xab\xe6\x89\x8b", 8, kSsPrimary, "", ""},
    {"0883.HK", "CNOOC Limited", "CNOOC", 8, kSsPrimary, "", ""},
    {"0857.HK", "PetroChina Company Limited", "PetroChina|\xe4\xb8\xad\xe5\x9b\xbd\xe7\x9f\xb3\xe6\xb2\xb9", 8, kSsPrimary,
     "", ""},
    {"600519.SS", "Kweichow Moutai Co., Ltd.",
     "Moutai|\xe8\xb4\xb5\xe5\xb7\x9e\xe8\x8c\x85\xe5\x8f\xb0|\xe8\x8c\x85\xe5\x8f\xb0", 10, kSsPrimary, "", ""},
    {"300750.SZ", "Contemporary Amperex Technology Co., Limited", "CATL|\xe5\xae\x81\xe5\xbe\xb7\xe6\x97\xb6\xe4\xbb\xa3", 9,
     kSsPrimary, "", ""},
    {"2317.TW", "Hon Hai Precision Industry Co., Ltd.", "Foxconn|Hon Hai|\xe9\xb4\xbb\xe6\xb5\xb7", 9, kSsPrimary, "", ""},
    {"2454.TW", "MediaTek Inc.", "MediaTek|\xe8\x81\xaf\xe7\x99\xbc\xe7\xa7\x91", 9, kSsPrimary, "", ""},
    // ── Japan ──
    {"7203.T", "Toyota Motor Corporation", "Toyota|\xe3\x83\x88\xe3\x83\xa8\xe3\x82\xbf", 10, kSsPrimary, "", ""},
    {"6758.T", "Sony Group Corporation", "Sony|\xe3\x82\xbd\xe3\x83\x8b\xe3\x83\xbc", 9, kSsPrimary, "", ""},
    {"9984.T", "SoftBank Group Corp.",
     "SoftBank|\xe3\x82\xbd\xe3\x83\x95\xe3\x83\x88\xe3\x83\x90\xe3\x83\xb3\xe3\x82\xaf", 9, kSsPrimary, "", ""},
    {"6861.T", "Keyence Corporation", "Keyence|\xe3\x82\xad\xe3\x83\xbc\xe3\x82\xa8\xe3\x83\xb3\xe3\x82\xb9", 9,
     kSsPrimary, "", ""},
    {"8306.T", "Mitsubishi UFJ Financial Group, Inc.", "MUFG|Mitsubishi UFJ|\xe4\xb8\x89\xe8\x8f\xb1UFJ", 9, kSsPrimary,
     "", ""},
    {"7974.T", "Nintendo Co., Ltd.", "Nintendo|\xe4\xbb\xbb\xe5\xa4\xa9\xe5\xa0\x82", 9, kSsPrimary, "", ""},
    {"9983.T", "Fast Retailing Co., Ltd.", "Uniqlo|\xe3\x83\xa6\xe3\x83\x8b\xe3\x82\xaf\xe3\x83\xad", 9, kSsPrimary, "",
     ""},
    {"6501.T", "Hitachi, Ltd.", "Hitachi|\xe6\x97\xa5\xe7\xab\x8b\xe8\xa3\xbd\xe4\xbd\x9c\xe6\x89\x80", 9, kSsPrimary, "",
     ""},
    {"7267.T", "Honda Motor Co., Ltd.", "Honda|\xe3\x83\x9b\xe3\x83\xb3\xe3\x83\x80", 9, kSsPrimary, "", ""},
    {"7201.T", "Nissan Motor Co., Ltd.", "Nissan|\xe6\x97\xa5\xe7\x94\xa3\xe8\x87\xaa\xe5\x8b\x95\xe8\xbb\x8a", 8,
     kSsPrimary, "", ""},
    {"8035.T", "Tokyo Electron Limited",
     "Tokyo Electron|\xe6\x9d\xb1\xe4\xba\xac\xe3\x82\xa8\xe3\x83\xac\xe3\x82\xaf\xe3\x83\x88\xe3\x83\xad\xe3\x83\xb3", 9,
     kSsPrimary, "", ""},
    {"4063.T", "Shin-Etsu Chemical Co., Ltd.", "Shin-Etsu", 9, kSsPrimary, "", ""},
    {"8058.T", "Mitsubishi Corporation", "Mitsubishi Corp|\xe4\xb8\x89\xe8\x8f\xb1\xe5\x95\x86\xe4\xba\x8b", 9, kSsPrimary,
     "", ""},
    {"9432.T", "Nippon Telegraph and Telephone Corporation", "NTT", 9, kSsPrimary, "", ""},
    {"6098.T", "Recruit Holdings Co., Ltd.", "Recruit Holdings", 9, kSsPrimary, "", ""},
    // ── Korea ──
    {"005930.KS", "Samsung Electronics Co., Ltd.", "Samsung|\xec\x82\xbc\xec\x84\xb1\xec\xa0\x84\xec\x9e\x90", 10,
     kSsPrimary, "", ""},
    {"000660.KS", "SK hynix Inc.", "SK Hynix|SK\xed\x95\x98\xec\x9d\xb4\xeb\x8b\x89\xec\x8a\xa4", 9, kSsPrimary, "", ""},
    {"005380.KS", "Hyundai Motor Company", "Hyundai Motor|\xed\x98\x84\xeb\x8c\x80\xec\xb0\xa8", 9, kSsPrimary, "", ""},
    {"035420.KS", "NAVER Corporation", "Naver|\xeb\x84\xa4\xec\x9d\xb4\xeb\xb2\x84", 8, kSsPrimary, "", ""},
    {"035720.KS", "Kakao Corp.", "Kakao|\xec\xb9\xb4\xec\xb9\xb4\xec\x98\xa4", 8, kSsPrimary, "", ""},
    {"373220.KS", "LG Energy Solution, Ltd.", "LG Energy Solution", 9, kSsPrimary, "", ""},
    {"005490.KS", "POSCO Holdings Inc.", "POSCO", 8, kSsPrimary, "", ""},
    // ── India (NSE primary; BSE twin) ──
    {"RELIANCE.NS", "Reliance Industries Limited", "Reliance|RIL", 10, kSsPrimary | kSsBseTwin, "INE002A01018", ""},
    {"TCS.NS", "Tata Consultancy Services Limited", "Tata Consultancy", 10, kSsPrimary | kSsBseTwin, "INE467B01029", ""},
    {"INFY.NS", "Infosys Limited", "Infosys", 10, kSsPrimary | kSsBseTwin, "INE009A01021", ""},
    {"HDFCBANK.NS", "HDFC Bank Limited", "HDFC Bank", 10, kSsPrimary | kSsBseTwin, "INE040A01034", ""},
    {"ICICIBANK.NS", "ICICI Bank Limited", "ICICI Bank", 10, kSsPrimary | kSsBseTwin, "", ""},
    {"SBIN.NS", "State Bank of India", "SBI", 10, kSsPrimary | kSsBseTwin, "", ""},
    {"BHARTIARTL.NS", "Bharti Airtel Limited", "Airtel|Bharti Airtel", 10, kSsPrimary | kSsBseTwin, "", ""},
    {"ITC.NS", "ITC Limited", "", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"LT.NS", "Larsen & Toubro Limited", "L&T|Larsen", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"HINDUNILVR.NS", "Hindustan Unilever Limited", "HUL", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"ADANIENT.NS", "Adani Enterprises Limited", "Adani", 8, kSsPrimary | kSsBseTwin, "", ""},
    {"ADANIPORTS.NS", "Adani Ports and Special Economic Zone Limited", "Adani Ports", 8, kSsPrimary | kSsBseTwin, "", ""},
    {"WIPRO.NS", "Wipro Limited", "", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"MARUTI.NS", "Maruti Suzuki India Limited", "Maruti|Maruti Suzuki", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"BAJFINANCE.NS", "Bajaj Finance Limited", "", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"SUNPHARMA.NS", "Sun Pharmaceutical Industries Limited", "Sun Pharma", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"KOTAKBANK.NS", "Kotak Mahindra Bank Limited", "Kotak|Kotak Bank", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"AXISBANK.NS", "Axis Bank Limited", "", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"HCLTECH.NS", "HCL Technologies Limited", "HCLTech|HCL Tech", 9, kSsPrimary | kSsBseTwin, "", ""},
    {"M&M.NS", "Mahindra & Mahindra Limited", "Mahindra", 9, kSsPrimary | kSsBseTwin, "", ""},
    // Tata Motors demerger: the listed company was renamed and its symbol changed
    // TATAMOTORS -> TMPV (NSE symbolchange.csv: TATAMOTORS,TMPV,24-OCT-2025); the commercial
    // vehicles business listed separately as TMCV. The old record is added below as Renamed.
    {"TMPV.NS", "Tata Motors Passenger Vehicles Limited", "Tata Motors PV", 9, kSsPrimary, "", "TATAMOTORS"},
    {"TMCV.NS", "Tata Motors Limited", "Tata Motors CV", 8, kSsPrimary, "", ""},
    {"INFY", "Infosys Limited", "Infosys", 9, 0, "", ""},
    {"HDB", "HDFC Bank Limited", "HDFC Bank", 9, 0, "", ""},
    {"IBN", "ICICI Bank Limited", "ICICI Bank", 9, 0, "", ""},
    {"WIT", "Wipro Limited", "Wipro", 8, 0, "", ""},
    // ── Europe (home listing primary; US ADR / NY shares not) ──
    {"SAP.DE", "SAP SE", "SAP", 10, kSsPrimary, "DE0007164600", ""},
    {"SAP", "SAP SE", "", 9, 0, "", ""},
    {"ASML.AS", "ASML Holding N.V.", "ASML", 10, kSsPrimary, "", ""},
    {"ASML", "ASML Holding N.V.", "", 9, 0, "", ""},
    {"NOVO-B.CO", "Novo Nordisk A/S", "Novo Nordisk|Novo", 9, kSsPrimary, "", ""},
    {"NVO", "Novo Nordisk A/S", "Novo Nordisk", 9, 0, "", ""},
    {"AZN.L", "AstraZeneca PLC", "AstraZeneca", 10, kSsPrimary, "", ""},
    {"AZN", "AstraZeneca PLC", "AstraZeneca", 9, 0, "", ""},
    {"SHEL.L", "Shell plc", "Shell", 10, kSsPrimary, "", ""},
    {"SHEL", "Shell plc", "Shell", 9, 0, "", "RDS-A|RDS-B"},
    {"BP.L", "BP p.l.c.", "BP", 9, kSsPrimary, "", ""},
    {"BP", "BP p.l.c.", "", 8, 0, "", ""},
    {"HSBA.L", "HSBC Holdings plc", "HSBC", 10, kSsPrimary, "", ""},
    {"0005.HK", "HSBC Holdings plc", "HSBC", 9, 0, "", ""},
    {"HSBC", "HSBC Holdings plc", "", 9, 0, "", ""},
    {"ULVR.L", "Unilever PLC", "Unilever", 9, kSsPrimary, "", ""},
    {"UL", "Unilever PLC", "Unilever", 9, 0, "", ""},
    {"RIO.L", "Rio Tinto plc", "Rio Tinto", 9, kSsPrimary, "", ""},
    {"RIO", "Rio Tinto plc", "Rio Tinto", 8, 0, "", ""},
    {"BHP.AX", "BHP Group Limited", "BHP", 10, kSsPrimary, "", ""},
    {"BHP", "BHP Group Limited", "", 9, 0, "", ""},
    {"GSK.L", "GSK plc", "GSK|GlaxoSmithKline", 9, kSsPrimary, "", ""},
    {"GSK", "GSK plc", "GlaxoSmithKline", 8, 0, "", ""},
    {"BARC.L", "Barclays PLC", "Barclays", 9, kSsPrimary, "", ""},
    {"BCS", "Barclays PLC", "Barclays", 8, 0, "", ""},
    {"LLOY.L", "Lloyds Banking Group plc", "Lloyds|Lloyds Banking", 9, kSsPrimary, "", ""},
    {"LYG", "Lloyds Banking Group plc", "Lloyds", 8, 0, "", ""},
    {"VOD.L", "Vodafone Group Plc", "Vodafone", 9, kSsPrimary, "", ""},
    {"VOD", "Vodafone Group Plc", "Vodafone", 8, 0, "", ""},
    {"RR.L", "Rolls-Royce Holdings plc", "Rolls-Royce", 9, kSsPrimary, "", ""},
    {"BATS.L", "British American Tobacco p.l.c.", "British American Tobacco|BAT", 9, kSsPrimary, "", ""},
    {"BTI", "British American Tobacco p.l.c.", "British American Tobacco", 8, 0, "", ""},
    {"NESN.SW", "Nestl\xc3\xa9 S.A.", "Nestle|Nestl\xc3\xa9", 10, kSsPrimary, "", ""},
    {"ROG.SW", "Roche Holding AG", "Roche", 10, kSsPrimary, "", ""},
    {"NOVN.SW", "Novartis AG", "Novartis", 10, kSsPrimary, "", ""},
    {"UBSG.SW", "UBS Group AG", "UBS", 9, kSsPrimary, "", ""},
    {"UBS", "UBS Group AG", "", 9, 0, "", ""},
    {"MC.PA", "LVMH Mo\xc3\xabt Hennessy Louis Vuitton SE", "LVMH", 10, kSsPrimary, "", ""},
    {"OR.PA", "L'Or\xc3\xa9" "al S.A.", "L'Oreal|L'Or\xc3\xa9" "al", 10, kSsPrimary, "", ""},
    {"TTE.PA", "TotalEnergies SE", "TotalEnergies|Total", 10, kSsPrimary, "", ""},
    {"TTE", "TotalEnergies SE", "TotalEnergies", 9, 0, "", ""},
    {"AIR.PA", "Airbus SE", "Airbus", 10, kSsPrimary, "", ""},
    {"SAN.PA", "Sanofi", "", 9, kSsPrimary, "", ""},
    {"BNP.PA", "BNP Paribas SA", "BNP Paribas|BNP", 9, kSsPrimary, "", ""},
    {"RMS.PA", "Herm\xc3\xa8s International", "Hermes|Herm\xc3\xa8s", 10, kSsPrimary, "", ""},
    {"SIE.DE", "Siemens AG", "Siemens", 10, kSsPrimary, "", ""},
    {"ENR.DE", "Siemens Energy AG", "Siemens Energy", 9, kSsPrimary, "", ""},
    {"SHL.DE", "Siemens Healthineers AG", "Siemens Healthineers", 9, kSsPrimary, "", ""},
    {"ABN.AS", "ABN AMRO Bank N.V.", "ABN Amro", 8, kSsPrimary, "", ""},
    {"ALV.DE", "Allianz SE", "Allianz", 10, kSsPrimary, "", ""},
    {"DTE.DE", "Deutsche Telekom AG", "Deutsche Telekom", 10, kSsPrimary, "", ""},
    {"VOW3.DE", "Volkswagen AG", "Volkswagen|VW", 9, kSsPrimary, "", ""},
    {"MBG.DE", "Mercedes-Benz Group AG", "Mercedes-Benz|Mercedes", 9, kSsPrimary, "", ""},
    {"BMW.DE", "Bayerische Motoren Werke AG", "BMW", 9, kSsPrimary, "", ""},
    {"BAS.DE", "BASF SE", "BASF", 9, kSsPrimary, "", ""},
    {"DBK.DE", "Deutsche Bank AG", "Deutsche Bank", 9, kSsPrimary, "", ""},
    {"DB", "Deutsche Bank AG", "Deutsche Bank", 8, 0, "", ""},
    {"ADS.DE", "adidas AG", "Adidas", 9, kSsPrimary, "", ""},
    {"IFX.DE", "Infineon Technologies AG", "Infineon", 9, kSsPrimary, "", ""},
    {"ENEL.MI", "Enel S.p.A.", "Enel", 9, kSsPrimary, "", ""},
    {"ISP.MI", "Intesa Sanpaolo S.p.A.", "Intesa Sanpaolo|Intesa", 9, kSsPrimary, "", ""},
    {"UCG.MI", "UniCredit S.p.A.", "UniCredit", 9, kSsPrimary, "", ""},
    {"STLAM.MI", "Stellantis N.V.", "Stellantis", 9, kSsPrimary, "", ""},
    {"STLA", "Stellantis N.V.", "Stellantis", 8, 0, "", ""},
    {"SAN.MC", "Banco Santander, S.A.", "Santander|Banco Santander", 9, kSsPrimary, "", ""},
    {"SAN", "Banco Santander, S.A.", "Santander", 8, 0, "", ""},
    {"ITX.MC", "Industria de Dise\xc3\xb1o Textil, S.A.", "Inditex", 10, kSsPrimary, "", ""},
    {"IBE.MC", "Iberdrola, S.A.", "Iberdrola", 9, kSsPrimary, "", ""},
    {"BBVA.MC", "Banco Bilbao Vizcaya Argentaria, S.A.", "BBVA", 9, kSsPrimary, "", ""},
    {"BBVA", "Banco Bilbao Vizcaya Argentaria, S.A.", "", 8, 0, "", ""},
    {"INGA.AS", "ING Groep N.V.", "ING", 9, kSsPrimary, "", ""},
    {"ING", "ING Groep N.V.", "", 8, 0, "", ""},
    {"PRX.AS", "Prosus N.V.", "Prosus", 9, kSsPrimary, "", ""},
    {"ADYEN.AS", "Adyen N.V.", "Adyen", 9, kSsPrimary, "", ""},
    {"EQNR.OL", "Equinor ASA", "Equinor", 9, kSsPrimary, "", ""},
    {"EQNR", "Equinor ASA", "Equinor", 8, 0, "", ""},
    {"ERIC-B.ST", "Telefonaktiebolaget LM Ericsson", "Ericsson", 8, kSsPrimary, "", ""},
    {"ERIC", "Telefonaktiebolaget LM Ericsson", "Ericsson", 8, 0, "", ""},
    {"NOKIA.HE", "Nokia Oyj", "Nokia", 8, kSsPrimary, "", ""},
    {"NOK", "Nokia Oyj", "Nokia", 8, 0, "", ""},
    // ── Australia / Canada ──
    {"CBA.AX", "Commonwealth Bank of Australia", "Commonwealth Bank|CommBank", 10, kSsPrimary, "", ""},
    {"CSL.AX", "CSL Limited", "CSL", 9, kSsPrimary, "", ""},
    {"WBC.AX", "Westpac Banking Corporation", "Westpac", 9, kSsPrimary, "", ""},
    {"NAB.AX", "National Australia Bank Limited", "National Australia Bank|NAB", 9, kSsPrimary, "", ""},
    {"ANZ.AX", "ANZ Group Holdings Limited", "ANZ", 9, kSsPrimary, "", ""},
    {"FMG.AX", "Fortescue Ltd", "Fortescue", 8, kSsPrimary, "", ""},
    {"WDS.AX", "Woodside Energy Group Ltd", "Woodside", 8, kSsPrimary, "", ""},
    {"RY.TO", "Royal Bank of Canada", "RBC|Royal Bank", 10, kSsPrimary, "", ""},
    {"RY", "Royal Bank of Canada", "RBC", 9, 0, "", ""},
    {"TD.TO", "The Toronto-Dominion Bank", "TD Bank|Toronto-Dominion", 9, kSsPrimary, "", ""},
    {"TD", "The Toronto-Dominion Bank", "TD Bank", 9, 0, "", ""},
    {"ENB.TO", "Enbridge Inc.", "Enbridge", 9, kSsPrimary, "", ""},
    {"ENB", "Enbridge Inc.", "Enbridge", 8, 0, "", ""},
    {"CNQ.TO", "Canadian Natural Resources Limited", "Canadian Natural", 9, kSsPrimary, "", ""},
    {"CNQ", "Canadian Natural Resources Limited", "Canadian Natural", 8, 0, "", ""},
    {"CNR.TO", "Canadian National Railway Company", "CN Rail|Canadian National", 9, kSsPrimary, "", ""},
    {"CNI", "Canadian National Railway Company", "CN Rail", 8, 0, "", ""},
    {"BN.TO", "Brookfield Corporation", "Brookfield", 9, kSsPrimary, "", ""},
    {"BN", "Brookfield Corporation", "Brookfield", 8, 0, "", ""},
    // ── Latin America ──
    {"VALE3.SA", "Vale S.A.", "Vale", 9, kSsPrimary, "", ""},
    {"VALE", "Vale S.A.", "Vale", 9, 0, "", ""},
    {"PETR4.SA", "Petr\xc3\xb3leo Brasileiro S.A. - Petrobras", "Petrobras", 9, kSsPrimary, "", ""},
    {"PBR", "Petr\xc3\xb3leo Brasileiro S.A. - Petrobras", "Petrobras", 9, 0, "", ""},
    {"ITUB4.SA", "Ita\xc3\xba Unibanco Holding S.A.", "Itau Unibanco|Ita\xc3\xba|Itau", 9, kSsPrimary, "", ""},
    {"ITUB", "Ita\xc3\xba Unibanco Holding S.A.", "Itau Unibanco", 8, 0, "", ""},
    {"NU", "Nu Holdings Ltd.", "Nubank", 9, kSsPrimary, "", ""},
    {"MELI", "MercadoLibre, Inc.", "MercadoLibre|Mercado Libre", 9, kSsPrimary, "", ""},
    {"AMX", "Am\xc3\xa9rica M\xc3\xb3vil, S.A.B. de C.V.", "America Movil", 8, 0, "", ""},
    {"CX", "CEMEX, S.A.B. de C.V.", "Cemex", 7, 0, "", ""},
    {"BBD", "Banco Bradesco S.A.", "Bradesco", 8, 0, "", ""},
    {"WALMEX.MX", "Wal-Mart de M\xc3\xa9xico, S.A.B. de C.V.", "Walmex", 9, kSsPrimary, "", ""},
    {"GGAL.BA", "Grupo Financiero Galicia S.A.", "Grupo Galicia|Galicia", 7, kSsPrimary, "", ""},
    {"GGAL", "Grupo Financiero Galicia S.A.", "Grupo Galicia", 7, 0, "", ""},
    {"YPFD.BA", "YPF Sociedad An\xc3\xb3nima", "YPF", 7, kSsPrimary, "", ""},
    {"YPF", "YPF Sociedad An\xc3\xb3nima", "", 7, 0, "", ""},
    // ── Middle East / Africa ──
    {"2222.SR", "Saudi Arabian Oil Company", "Saudi Aramco|Aramco|\xd8\xa3\xd8\xb1\xd8\xa7\xd9\x85\xd9\x83\xd9\x88", 10,
     kSsPrimary, "", ""},
    {"1120.SR", "Al Rajhi Banking and Investment Corporation", "Al Rajhi Bank|Al Rajhi", 9, kSsPrimary, "", ""},
    {"TEVA", "Teva Pharmaceutical Industries Limited", "Teva", 8, kSsPrimary, "", ""},
    {"CHKP", "Check Point Software Technologies Ltd.", "Check Point", 8, kSsPrimary, "", ""},
    {"NPN.JO", "Naspers Limited", "Naspers", 9, kSsPrimary, "", ""},
    {"MTN.JO", "MTN Group Limited", "MTN Group|MTN", 8, kSsPrimary, "", ""},
    {"SBK.JO", "Standard Bank Group Limited", "Standard Bank", 8, kSsPrimary, "", ""},
    {"FSR.JO", "FirstRand Limited", "FirstRand", 8, kSsPrimary, "", ""},
    // ── ETFs ──
    {"SPY", "SPDR S&P 500 ETF Trust", "", 10, kSsPrimary | kSsEtf, "", ""},
    {"VOO", "Vanguard S&P 500 ETF", "", 9, kSsPrimary | kSsEtf, "", ""},
    {"IVV", "iShares Core S&P 500 ETF", "", 9, kSsPrimary | kSsEtf, "", ""},
    {"QQQ", "Invesco QQQ Trust", "", 10, kSsPrimary | kSsEtf, "", ""},
    {"VTI", "Vanguard Total Stock Market ETF", "", 9, kSsPrimary | kSsEtf, "", ""},
    {"IWM", "iShares Russell 2000 ETF", "", 9, kSsPrimary | kSsEtf, "", ""},
    {"EEM", "iShares MSCI Emerging Markets ETF", "", 8, kSsPrimary | kSsEtf, "", ""},
    {"TLT", "iShares 20+ Year Treasury Bond ETF", "", 8, kSsPrimary | kSsEtf, "", ""},
    {"GLD", "SPDR Gold Shares", "", 9, kSsPrimary | kSsEtf, "", ""},
    {"SLV", "iShares Silver Trust", "", 8, kSsPrimary | kSsEtf, "", ""},
    {"NIFTYBEES.NS", "Nippon India ETF Nifty 50 BeES", "", 8, kSsPrimary | kSsEtf, "", ""},
};

/// The record of the old TATAMOTORS listing: renamed, successor TMPV.
struct SsRenamed {
    const char* symbol;
    const char* name;
    const char* successor_symbol;
    int weight;
};
const SsRenamed kSsRenamed[] = {
    {"TATAMOTORS.NS", "Tata Motors Limited", "TMPV.NS", 9},
};

/// "<successor yahoo>|<former code>" -> last day the former code was used.
struct SsFormerDate {
    const char* successor_symbol;
    const char* former;
    const char* valid_to;
};
const SsFormerDate kSsFormerDates[] = {
    {"TMPV.NS", "TATAMOTORS", "2025-10-24"}, // NSE symbolchange.csv
};

struct SsCurrency {
    const char* code;
    const char* name;
};
const SsCurrency kSsCurrencies[] = {
    {"USD", "US Dollar"},          {"EUR", "Euro"},
    {"GBP", "British Pound"},      {"JPY", "Japanese Yen"},
    {"CHF", "Swiss Franc"},        {"AUD", "Australian Dollar"},
    {"NZD", "New Zealand Dollar"}, {"CAD", "Canadian Dollar"},
    {"CNY", "Chinese Yuan"},       {"CNH", "Chinese Yuan (Offshore)"},
    {"HKD", "Hong Kong Dollar"},   {"SGD", "Singapore Dollar"},
    {"INR", "Indian Rupee"},       {"KRW", "South Korean Won"},
    {"MXN", "Mexican Peso"},       {"BRL", "Brazilian Real"},
    {"ZAR", "South African Rand"}, {"TRY", "Turkish Lira"},
    {"SEK", "Swedish Krona"},      {"NOK", "Norwegian Krone"},
    {"DKK", "Danish Krone"},       {"PLN", "Polish Zloty"},
    {"IDR", "Indonesian Rupiah"},  {"THB", "Thai Baht"},
    {"PHP", "Philippine Peso"},    {"MYR", "Malaysian Ringgit"},
    {"TWD", "New Taiwan Dollar"},  {"ILS", "Israeli New Shekel"},
    {"SAR", "Saudi Riyal"},        {"AED", "UAE Dirham"},
};

struct SsPair {
    const char* pair;
    int weight;
};
const SsPair kSsFxPairs[] = {
    // Majors
    {"EURUSD", 10}, {"GBPUSD", 10}, {"USDJPY", 10}, {"USDCHF", 10}, {"AUDUSD", 10}, {"NZDUSD", 10}, {"USDCAD", 10},
    // USD against other currencies
    {"USDCNY", 8}, {"USDCNH", 8}, {"USDHKD", 8}, {"USDSGD", 8}, {"USDINR", 9}, {"USDKRW", 8}, {"USDMXN", 8},
    {"USDBRL", 8}, {"USDZAR", 8}, {"USDTRY", 8}, {"USDSEK", 8}, {"USDNOK", 8}, {"USDDKK", 8}, {"USDPLN", 8},
    {"USDIDR", 8}, {"USDTHB", 8}, {"USDPHP", 8}, {"USDMYR", 8}, {"USDTWD", 8}, {"USDILS", 8}, {"USDSAR", 8},
    {"USDAED", 8},
    // Crosses
    {"EURGBP", 9}, {"EURJPY", 9}, {"EURCHF", 9}, {"GBPJPY", 9}, {"AUDJPY", 8}, {"EURAUD", 8}, {"EURCAD", 8},
    {"GBPCHF", 8}, {"CHFJPY", 8}, {"EURINR", 8}, {"GBPINR", 8},
};

QStringList ss_split(const char* text) {
    QStringList out;
    const QString s = QString::fromUtf8(text);
    for (const QString& part : s.split(QLatin1Char('|'), Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (!t.isEmpty())
            out.push_back(t);
    }
    return out;
}

void ss_add_unique(QStringList& list, const QString& value) {
    const QString key = core::instrument::alias_key(value);
    if (key.isEmpty())
        return;
    for (const QString& x : list)
        if (core::instrument::alias_key(x) == key)
            return;
    list.push_back(value);
}

QString ss_currency_name(const QString& code) {
    for (const SsCurrency& c : kSsCurrencies)
        if (code == QLatin1String(c.code))
            return QString::fromLatin1(c.name);
    return code;
}

SsInstrument ss_company(const QString& symbol, const SsCompany& c, bool primary) {
    namespace ci = core::instrument;
    SsInstrument inst = ci::instrument_skeleton(symbol);
    if (inst.id.isEmpty())
        return {};
    inst.name = QString::fromUtf8(c.name);
    inst.aliases = ss_split(c.aliases);
    inst.former_symbols = ss_split(c.former);
    inst.rank_weight = c.weight;
    inst.primary_listing = primary;
    if (c.flags & kSsEtf) {
        inst.asset_class = SsClass::Etf;
        inst.quote_type = QStringLiteral("ETF");
    } else {
        inst.quote_type = QStringLiteral("EQUITY");
    }
    inst.isin = QString::fromLatin1(c.isin);
    return inst;
}

QVector<SsInstrument> ss_build(SymbolSeedStats& st) {
    namespace ci = core::instrument;
    QVector<SsInstrument> out;
    out.reserve(900);

    // Indices with a Yahoo symbol: the skeleton carries the table name and aliases.
    for (const ci::IndexInfo& x : ci::all_indices()) {
        if (x.yahoo.isEmpty())
            continue;
        SsInstrument inst = ci::instrument_skeleton(x.yahoo);
        if (inst.id.isEmpty()) {
            ++st.skipped;
            continue;
        }
        inst.name = x.name;
        inst.asset_class = SsClass::Index;
        inst.quote_type = QStringLiteral("INDEX");
        if (!x.venue.isEmpty())
            inst.venue = x.venue;
        if (!x.country.isEmpty())
            inst.country = x.country;
        for (const QString& a : x.aliases)
            ss_add_unique(inst.aliases, a);
        for (const QString& a : x.text_only)
            ss_add_unique(inst.aliases, a);
        if (!x.broker_symbol.isEmpty())
            ss_add_unique(inst.aliases, x.broker_symbol);
        inst.rank_weight = 10;
        out.push_back(inst);
        ++st.indices;
    }

    // Continuous futures.
    for (const ci::FutureInfo& f : ci::all_futures()) {
        if (f.yahoo.isEmpty())
            continue;
        SsInstrument inst = ci::instrument_skeleton(f.yahoo);
        if (inst.id.isEmpty()) {
            ++st.skipped;
            continue;
        }
        inst.name = f.name;
        inst.asset_class = SsClass::Future;
        inst.quote_type = QStringLiteral("FUTURE");
        if (!f.currency.isEmpty())
            inst.currency = f.currency;
        for (const QString& a : f.aliases)
            ss_add_unique(inst.aliases, a);
        inst.rank_weight = f.commodity ? 10 : 9;
        out.push_back(inst);
        ++st.futures;
    }

    // Crypto assets against USD; the table order is by size.
    int crypto_rank = 0;
    for (const ci::CryptoInfo& c : ci::all_cryptos()) {
        const QString base = c.yahoo_base.isEmpty() ? c.base : c.yahoo_base;
        SsInstrument inst = ci::instrument_skeleton(base + QStringLiteral("-USD"));
        if (inst.id.isEmpty() || inst.asset_class != SsClass::Crypto) {
            ++st.skipped;
            continue;
        }
        if (inst.name.isEmpty())
            inst.name = c.name;
        for (const QString& a : c.aliases)
            ss_add_unique(inst.aliases, a);
        if (c.base != base)
            ss_add_unique(inst.aliases, c.base);
        inst.quote_type = QStringLiteral("CRYPTOCURRENCY");
        inst.rank_weight = crypto_rank < 10 ? 10 : (crypto_rank < 30 ? 8 : 6);
        ++crypto_rank;
        out.push_back(inst);
        ++st.crypto;
    }

    // FX pairs.
    for (const SsPair& p : kSsFxPairs) {
        const QString pair = QString::fromLatin1(p.pair);
        const QString yahoo = ci::to_yahoo(pair, QStringLiteral("FX"), SsClass::Fx);
        SsInstrument inst = ci::instrument_skeleton(yahoo);
        if (inst.id.isEmpty() || inst.asset_class != SsClass::Fx) {
            ++st.skipped;
            continue;
        }
        inst.name = ss_currency_name(pair.left(3)) + QStringLiteral(" / ") + ss_currency_name(pair.mid(3));
        inst.quote_type = QStringLiteral("CURRENCY");
        inst.rank_weight = p.weight;
        out.push_back(inst);
        ++st.fx;
    }

    // Companies and ETFs (+ BSE twins of the Indian names).
    for (const SsCompany& c : kSsCompanies) {
        const QString symbol = QString::fromUtf8(c.symbol);
        SsInstrument inst = ss_company(symbol, c, (c.flags & kSsPrimary) != 0);
        if (inst.id.isEmpty()) {
            ++st.skipped;
            continue;
        }
        out.push_back(inst);
        ++st.companies;
        if ((c.flags & kSsBseTwin) && symbol.endsWith(QLatin1String(".NS"))) {
            SsInstrument twin = ss_company(symbol.chopped(3) + QStringLiteral(".BO"), c, false);
            if (twin.id.isEmpty()) {
                ++st.skipped;
                continue;
            }
            twin.former_symbols.clear();
            out.push_back(twin);
            ++st.companies;
        }
    }

    // Renamed listings: kept so stored rows still resolve, pointing at the successor.
    for (const SsRenamed& r : kSsRenamed) {
        SsInstrument inst = ci::instrument_skeleton(QString::fromLatin1(r.symbol));
        const QString successor = ci::instrument_id_for(QString::fromLatin1(r.successor_symbol));
        if (inst.id.isEmpty() || successor.isEmpty()) {
            ++st.skipped;
            continue;
        }
        inst.name = QString::fromUtf8(r.name);
        inst.quote_type = QStringLiteral("EQUITY");
        inst.status = core::instrument::InstrumentStatus::Renamed;
        inst.successor_id = successor;
        inst.primary_listing = true;
        inst.rank_weight = r.weight;
        out.push_back(inst);
        ++st.companies;
    }
    return out;
}

} // namespace

QVector<core::instrument::Instrument> builtin_symbol_seed(SymbolSeedStats* stats) {
    struct Built {
        QVector<SsInstrument> rows;
        SymbolSeedStats stats;
    };
    static const Built built = []() {
        Built b;
        b.rows = ss_build(b.stats);
        return b;
    }();
    if (stats)
        *stats = built.stats;
    return built.rows;
}

QHash<QString, QString> builtin_symbol_seed_former_dates() {
    QHash<QString, QString> out;
    for (const SsFormerDate& d : kSsFormerDates) {
        const QString id = core::instrument::instrument_id_for(QString::fromLatin1(d.successor_symbol));
        if (id.isEmpty())
            continue;
        out.insert(id + QLatin1Char('|') + core::instrument::alias_key(QString::fromLatin1(d.former)),
                   QString::fromLatin1(d.valid_to));
    }
    return out;
}

} // namespace fincept::services
