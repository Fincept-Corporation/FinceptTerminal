#pragma once
// Economics Tab — Data types, data source configs, indicators, countries
// Port of economics/constants.ts + economics/types.ts

#include <string>
#include <vector>
#include <optional>
#include <imgui.h>

namespace fincept::economics {

// ============================================================================
// Core types
// ============================================================================

struct DataPoint {
    std::string date;
    double value = 0.0;
};

struct IndicatorData {
    std::string indicator;
    std::string country;
    std::vector<DataPoint> data;
    std::string indicator_name;
    std::string country_name;
    std::string source;
};

struct ChartStats {
    double latest = 0.0;
    double change = 0.0; // percent
    double min = 0.0;
    double max = 0.0;
    double avg = 0.0;
    int count = 0;
};

// ============================================================================
// Data source configuration
// ============================================================================

enum class DataSource {
    WorldBank = 0, BIS, IMF, FRED, OECD, WTO, CFTC, EIA, ADB,
    Fed, BLS, UNESCO, FiscalData, BEA, Fincept,
    COUNT
};

struct DataSourceConfig {
    DataSource id;
    const char* name;       // Short name for buttons
    const char* full_name;  // Full name for display
    const char* description;
    ImVec4 color;
    const char* script_name;
    bool requires_api_key = false;
    const char* api_key_name = nullptr; // e.g. "wto_api_key" (matches settings credentials)
};

struct Indicator {
    const char* id;
    const char* name;
    const char* category;
    const char* code = nullptr; // For CFTC markets
};

struct Country {
    const char* code;       // ISO 3-letter
    const char* name;
    const char* bis = nullptr;
    const char* imf = nullptr;
    const char* fred = nullptr;
    const char* oecd = nullptr;
    const char* wto = nullptr;
    const char* adb = nullptr;
};

// ============================================================================
// Data source configs
// ============================================================================

inline const DataSourceConfig DATA_SOURCES[] = {
    {DataSource::WorldBank,  "World Bank", "World Bank",                          "Global development indicators",                {0.0f,0.9f,1.0f,1.0f}, "worldbank_data.py"},
    {DataSource::BIS,        "BIS",        "Bank for International Settlements",  "Central bank statistics, exchange rates",      {0.616f,0.306f,0.867f,1.0f}, "bis_data.py"},
    {DataSource::IMF,        "IMF",        "International Monetary Fund",         "World Economic Outlook, fiscal & monetary",    {0.0f,0.533f,1.0f,1.0f}, "imf_data.py"},
    {DataSource::FRED,       "FRED",       "Federal Reserve Economic Data",       "U.S. economic data from the Federal Reserve",  {0.0f,0.84f,0.44f,1.0f}, "fred_data.py"},
    {DataSource::OECD,       "OECD",       "Organisation for Economic Co-operation","Economic indicators for developed countries",{1.0f,0.84f,0.0f,1.0f}, "oecd_data.py"},
    {DataSource::WTO,        "WTO",        "World Trade Organization",            "Trade statistics, tariffs & trade policy",     {0.91f,0.12f,0.39f,1.0f}, "wto_data.py", true, "wto_api_key"},
    {DataSource::CFTC,       "CFTC",       "Commodity Futures Trading Commission","Commitment of Traders (COT) Reports",         {1.0f,0.34f,0.13f,1.0f}, "cftc_data.py"},
    {DataSource::EIA,        "EIA",        "Energy Information Administration",   "U.S. energy production & prices",              {0.3f,0.69f,0.31f,1.0f}, "eia_data.py", true, "eia_api_key"},
    {DataSource::ADB,        "ADB",        "Asian Development Bank",              "Asia-Pacific economic indicators",             {0.0f,0.45f,0.74f,1.0f}, "adb_data.py"},
    {DataSource::Fed,        "Fed",        "Federal Reserve",                     "US monetary policy, rates & money supply",     {0.1f,0.37f,0.48f,1.0f}, "federal_reserve_data.py"},
    {DataSource::BLS,        "BLS",        "Bureau of Labor Statistics",           "US labor, prices, employment & productivity",  {0.67f,0.28f,0.74f,1.0f}, "bls_data.py", true, "bls_api_key"},
    {DataSource::UNESCO,     "UNESCO",     "UNESCO Institute for Statistics",     "Education, science, culture & demographic",    {0.0f,0.67f,0.76f,1.0f}, "unesco_data.py"},
    {DataSource::FiscalData, "FiscalData", "U.S. Treasury FiscalData",            "US debt, interest rates, spending & gold",     {1.0f,0.65f,0.15f,1.0f}, "fiscal_data.py"},
    {DataSource::BEA,        "BEA",        "Bureau of Economic Analysis",         "US GDP, income, spending & national accounts", {0.9f,0.32f,0.0f,1.0f}, "bea_data.py", true, "bea_api_key"},
    {DataSource::Fincept,    "Fincept",    "Fincept Macro Data",                  "CEIC series, economic calendar, WGB sovereign",{1.0f,0.53f,0.0f,1.0f}, ""},
};

constexpr int DATA_SOURCE_COUNT = static_cast<int>(DataSource::COUNT);

inline const DataSourceConfig& get_source_config(DataSource src) {
    return DATA_SOURCES[static_cast<int>(src)];
}

// ============================================================================
// Indicators per data source
// ============================================================================

inline const Indicator WORLDBANK_INDICATORS[] = {
    {"NY.GDP.MKTP.CD",      "GDP (current US$)",                         "GDP"},
    {"NY.GDP.MKTP.KD.ZG",   "GDP growth (annual %)",                     "GDP"},
    {"NY.GDP.PCAP.CD",      "GDP per capita (current US$)",              "GDP"},
    {"FP.CPI.TOTL.ZG",      "Inflation, consumer prices (annual %)",     "Prices"},
    {"SL.UEM.TOTL.ZS",      "Unemployment (% of labor force)",           "Labor"},
    {"NE.EXP.GNFS.ZS",      "Exports of goods and services (% of GDP)",  "Trade"},
    {"NE.IMP.GNFS.ZS",      "Imports of goods and services (% of GDP)",  "Trade"},
    {"BN.CAB.XOKA.GD.ZS",   "Current account balance (% of GDP)",        "Trade"},
    {"GC.DOD.TOTL.GD.ZS",   "Central government debt (% of GDP)",        "Government"},
    {"SP.POP.TOTL",          "Population, total",                         "Demographics"},
    {"SP.POP.GROW",          "Population growth (annual %)",              "Demographics"},
    {"FR.INR.RINR",          "Real interest rate (%)",                    "Financial"},
    {"PA.NUS.FCRF",          "Official exchange rate (LCU per US$)",      "Financial"},
    {"FI.RES.TOTL.CD",       "Total reserves (includes gold, US$)",       "Financial"},
    {"BX.KLT.DINV.WD.GD.ZS","FDI net inflows (% of GDP)",               "Investment"},
};

inline const Indicator BIS_INDICATORS[] = {
    {"WS_CBPOL",      "Central Bank Policy Rates",              "Rates"},
    {"WS_CBTA",        "Central Bank Total Assets",              "Rates"},
    {"WS_EER",         "Effective Exchange Rates (Nominal)",     "Exchange Rates"},
    {"WS_XRU",         "US Dollar Exchange Rates",               "Exchange Rates"},
    {"WS_LONG_CPI",    "Consumer Prices (CPI)",                  "Prices"},
    {"WS_SPP",         "Selected Residential Property Prices",   "Property"},
    {"WS_DPP",         "Detailed Residential Property Prices",   "Property"},
    {"WS_CPP",         "Commercial Property Prices",             "Property"},
    {"WS_TC",          "Total Credit to Non-Financial Sector",   "Credit"},
    {"WS_CREDIT_GAP",  "Credit-to-GDP Gaps",                     "Credit"},
    {"WS_DSR",         "Debt Service Ratios",                    "Credit"},
    {"WS_GLI",         "Global Liquidity Indicators",            "Credit"},
    {"WS_DEBT_SEC2_PUB","International Debt Securities",         "Debt Securities"},
    {"WS_LBS_D_PUB",   "Locational Banking Statistics",          "Banking"},
    {"WS_CBS_PUB",     "Consolidated Banking Statistics",        "Banking"},
    {"WS_OTC_DERIV2",  "OTC Derivatives Outstanding",            "Derivatives"},
};

inline const Indicator IMF_INDICATORS[] = {
    {"NGDP_RPCH",    "Real GDP growth (Annual %)",                "GDP"},
    {"NGDPD",        "GDP, current prices (Billions USD)",        "GDP"},
    {"NGDPDPC",      "GDP per capita, current prices (USD)",      "GDP"},
    {"PCPIPCH",      "Inflation, avg consumer prices (Annual %)", "Prices"},
    {"PCPIEPCH",     "Inflation, end of period (Annual %)",       "Prices"},
    {"LUR",          "Unemployment rate (%)",                     "Labor"},
    {"BCA_NGDPD",    "Current account balance (% of GDP)",        "Trade"},
    {"GGXWDG_NGDP",  "General govt gross debt (% of GDP)",        "Government"},
    {"GGXCNL_NGDP",  "General govt net lending (% of GDP)",       "Government"},
    {"GGR_NGDP",     "General govt revenue (% of GDP)",           "Government"},
};

inline const Indicator FRED_INDICATORS[] = {
    {"GDP",       "Gross Domestic Product",             "GDP"},
    {"GDPC1",     "Real Gross Domestic Product",        "GDP"},
    {"CPIAUCSL",  "CPI for All Urban Consumers",        "Prices"},
    {"CPILFESL",  "Core CPI (Less Food and Energy)",    "Prices"},
    {"UNRATE",    "Unemployment Rate",                  "Labor"},
    {"PAYEMS",    "All Employees, Total Nonfarm",       "Labor"},
    {"FEDFUNDS",  "Federal Funds Effective Rate",       "Rates"},
    {"DGS10",     "10-Year Treasury Rate",              "Rates"},
    {"DGS2",      "2-Year Treasury Rate",               "Rates"},
    {"DEXUSEU",   "US / Euro Foreign Exchange Rate",    "Exchange Rates"},
    {"DTWEXBGS",  "Trade Weighted US Dollar Index",     "Exchange Rates"},
    {"M2SL",      "M2 Money Stock",                     "Money Supply"},
    {"WALCL",     "Federal Reserve Total Assets",       "Fed Balance Sheet"},
    {"HOUST",     "Housing Starts",                     "Housing"},
    {"INDPRO",    "Industrial Production Index",        "Production"},
};

inline const Indicator OECD_INDICATORS[] = {
    {"GDP",     "Gross Domestic Product",        "GDP"},
    {"RGDP",    "Real GDP",                      "GDP"},
    {"CPI",     "Consumer Price Index",           "Prices"},
    {"UNR",     "Unemployment Rate",              "Labor"},
    {"LF",      "Labour Force",                   "Labor"},
    {"IRSTCI",  "Short-term Interest Rates",      "Rates"},
    {"IRLTLT",  "Long-term Interest Rates",       "Rates"},
    {"EXCH",    "Exchange Rates",                 "Exchange Rates"},
    {"TBAL",    "Trade Balance",                  "Trade"},
    {"CLI",     "Composite Leading Indicators",   "Leading Indicators"},
    {"BCI",     "Business Confidence Index",      "Confidence"},
    {"CCI",     "Consumer Confidence Index",      "Confidence"},
};

inline const Indicator WTO_INDICATORS[] = {
    {"TP_A_0010", "Simple Avg MFN Tariff - All Products",     "Tariffs"},
    {"TP_A_0160", "Simple Avg MFN Tariff - Agricultural",     "Tariffs"},
    {"TP_A_0430", "Simple Avg MFN Tariff - Non-Agricultural", "Tariffs"},
    {"TP_A_0030", "Trade-Weighted MFN Tariff - All Products", "Tariffs"},
    {"TP_A_0020", "Simple Avg Bound Tariff - All Products",   "Bound Tariffs"},
    {"TP_A_0100", "Share of Duty-Free Tariff Lines - All",    "Tariff Structure"},
    {"TP_A_0090", "Binding Coverage - All Products",          "Tariff Structure"},
    {"TP_A_0070", "Maximum MFN Applied Tariff - All",         "Tariff Peaks"},
};

inline const Indicator CFTC_INDICATORS[] = {
    {"corn",             "Corn",               "Agricultural", "002602"},
    {"wheat",            "Wheat (SRW)",         "Agricultural", "001602"},
    {"soybeans",         "Soybeans",            "Agricultural", "005602"},
    {"cotton",           "Cotton",              "Agricultural", "033661"},
    {"crude_oil",        "Crude Oil (WTI)",      "Energy",       "067651"},
    {"natural_gas",      "Natural Gas",          "Energy",       "02365B"},
    {"gold",             "Gold",                 "Metals",       "088691"},
    {"silver",           "Silver",               "Metals",       "084691"},
    {"copper",           "Copper",               "Metals",       "085692"},
    {"euro",             "Euro FX",              "Currencies",   "099741"},
    {"jpy",              "Japanese Yen",         "Currencies",   "097741"},
    {"s&p_500",          "E-mini S&P 500",       "Indices",      "13874A"},
    {"nasdaq_100",       "Nasdaq 100 E-mini",    "Indices",      "209742"},
    {"treasury_bonds",   "US Treasury Bonds",    "Interest Rates","020601"},
    {"bitcoin",          "Bitcoin",              "Crypto",       "133741"},
};

inline const Indicator EIA_INDICATORS[] = {
    {"balance_sheet",                   "Petroleum Balance Sheet",          "Petroleum"},
    {"inputs_and_production",           "Refinery Inputs & Production",    "Petroleum"},
    {"crude_petroleum_stocks",          "Crude Oil Stocks",                 "Petroleum"},
    {"gasoline_fuel_stocks",            "Gasoline Stocks",                  "Petroleum"},
    {"spot_prices_crude_gas_heating",   "Spot Prices (Crude, Gas, Heating)","Prices"},
    {"retail_prices",                   "Retail Fuel Prices",               "Prices"},
    {"steo_01",                         "US Energy Markets Summary",        "STEO"},
    {"steo_02",                         "Nominal Energy Prices",            "STEO"},
    {"steo_04a",                        "US Petroleum Supply & Consumption","STEO"},
};

inline const Indicator ADB_INDICATORS[] = {
    {"NGDP_XDC",                 "GDP at Current Prices",               "Economy & Output"},
    {"NGDPPC_XDC",               "GDP per Capita",                      "Economy & Output"},
    {"NYG_USD",                  "GNI (USD millions)",                  "Economy & Output"},
    {"NGDPSO_AGR_XGDP_PS",      "Agriculture (% of GDP)",              "GDP Composition"},
    {"NGDPSO_IND_XGDP_PS",      "Industry (% of GDP)",                 "GDP Composition"},
    {"NGDPSO_SER_XGDP_PS",      "Services (% of GDP)",                 "GDP Composition"},
    {"NC_HFC_XGDP_PS",          "Household Consumption (% of GDP)",    "Expenditure"},
    {"NCGG_XGDP_PS",            "Government Consumption (% of GDP)",   "Expenditure"},
    {"NEGS_XGDP_PS",            "Exports of Goods & Services (% GDP)", "Trade"},
    {"LP_PE_NUM_MOP",            "Total Population",                    "Population"},
    {"LP_MOP_PTX_PS",            "Population Growth Rate (%)",          "Population"},
};

inline const Indicator FED_INDICATORS[] = {
    {"federal_funds_rate",       "Federal Funds Rate (EFFR)",     "Interest Rates"},
    {"sofr_rate",                "SOFR Rate",                     "Interest Rates"},
    {"treasury_rates",           "Treasury Rates (All Maturities)","Treasury"},
    {"yield_curve",              "Yield Curve (Latest)",          "Treasury"},
    {"money_measures",           "Money Supply (M1, M2)",         "Money Supply"},
    {"money_measures_adjusted",  "Money Supply (Seasonally Adj)", "Money Supply"},
    {"central_bank_holdings",    "Fed Holdings (SOMA)",           "Central Bank"},
    {"market_overview",          "Market Overview (Recent)",      "Overview"},
};

inline const Indicator BLS_INDICATORS[] = {
    {"CUSR0000SA0",       "CPI All Urban Consumers: All Items (SA)", "CPI"},
    {"CUSR0000SA0L1E",    "CPI All Items Less Food & Energy",        "CPI"},
    {"CES0000000001",     "Total Nonfarm Payrolls",                  "Employment"},
    {"CES0500000003",     "Average Hourly Earnings (Private)",       "Employment"},
    {"LNS14000000",       "Unemployment Rate",                       "Unemployment"},
    {"LNS13000000",       "Labor Force Participation Rate",          "Unemployment"},
    {"WPUFD49104",        "PPI Final Demand",                        "PPI"},
    {"JTS000000000000000JOL","JOLTS Job Openings",                   "JOLTS"},
    {"PRS85006092",       "Nonfarm Business Labor Productivity",     "Productivity"},
};

inline const Indicator UNESCO_INDICATORS[] = {
    {"GER.1",        "Gross Enrolment Ratio, Primary (%)",     "Education"},
    {"GER.2T3",      "Gross Enrolment Ratio, Secondary (%)",   "Education"},
    {"GER.5T8",      "Gross Enrolment Ratio, Tertiary (%)",    "Education"},
    {"LR.AG15T99",   "Adult Literacy Rate, 15+ (%)",           "Literacy"},
    {"XGDP.FSGOV",   "Govt Education Expenditure (% GDP)",     "Education Spending"},
    {"EXPGDP.TOT",   "R&D Expenditure (% of GDP)",             "Science & Tech"},
    {"200101",        "Total Population (thousands)",           "Demographics"},
};

inline const Indicator FISCALDATA_INDICATORS[] = {
    {"total_public_debt",       "Total Public Debt Outstanding",      "Debt"},
    {"debt_held_public",        "Debt Held by the Public",            "Debt"},
    {"intragov_holdings",       "Intragovernmental Holdings",         "Debt"},
    {"avg_rate_tbills",         "Avg Interest Rate - T-Bills",        "Interest Rates"},
    {"avg_rate_tnotes",         "Avg Interest Rate - T-Notes",        "Interest Rates"},
    {"avg_rate_tbonds",         "Avg Interest Rate - T-Bonds",        "Interest Rates"},
    {"interest_expense_total",  "Total Interest Expense (FYTD)",      "Interest Expense"},
    {"outlays_defense",         "Federal Outlays - Defense (FYTD)",   "Outlays"},
    {"outlays_social_security", "Federal Outlays - Social Security",  "Outlays"},
    {"gold_reserve_total",      "U.S. Gold Reserve (Book Value)",     "Gold Reserve"},
};

inline const Indicator BEA_INDICATORS[] = {
    {"gdp_growth",          "Real GDP Growth (% Change)",                "GDP"},
    {"nominal_gdp",         "Nominal GDP (Billions $)",                  "GDP"},
    {"real_gdp",            "Real GDP (Chained 2017 $, Billions)",       "GDP"},
    {"gdp_per_capita",      "GDP per Capita (Current $)",                "GDP"},
    {"pce",                 "Personal Consumption Expenditures",         "Consumption"},
    {"gross_investment",    "Gross Private Domestic Investment",          "Investment"},
    {"net_exports",         "Net Exports (Billions $)",                  "Trade"},
    {"exports",             "Exports of Goods & Services",               "Trade"},
    {"personal_income",     "Personal Income (Billions $)",              "Income"},
    {"saving_rate",         "Personal Saving Rate (%)",                  "Saving"},
    {"pce_inflation",       "PCE Price Index (% Change)",                "Inflation"},
    {"core_pce_inflation",  "Core PCE Price Index (% Change)",           "Inflation"},
    {"govt_spending",       "Government Spending (Billions $)",          "Government"},
};

inline const Indicator FINCEPT_INDICATORS[] = {
    {"ceic_series_countries", "CEIC: Available Countries",         "CEIC Series"},
    {"ceic_series_indicators","CEIC: Indicators by Country",       "CEIC Series"},
    {"ceic_series",           "CEIC: Economic Series Data",        "CEIC Series"},
    {"economic_calendar",     "Economic Calendar Events",          "Economic Calendar"},
    {"upcoming_events",       "Upcoming Economic Events (TE)",     "Economic Calendar"},
    {"wgb_central_bank_rates","WGB: Central Bank Rates",           "World Gov. Bonds"},
    {"wgb_credit_ratings",    "WGB: Sovereign Credit Ratings",     "World Gov. Bonds"},
    {"wgb_sovereign_cds",     "WGB: Sovereign CDS Spreads",        "World Gov. Bonds"},
    {"wgb_bond_spreads",      "WGB: Government Bond Spreads",      "World Gov. Bonds"},
    {"wgb_inverted_yields",   "WGB: Inverted Yield Curves",        "World Gov. Bonds"},
};

// Helper: get indicator list + count for a data source
struct IndicatorList {
    const Indicator* items;
    int count;
};

inline IndicatorList get_indicators(DataSource src) {
    switch (src) {
        case DataSource::WorldBank:  return {WORLDBANK_INDICATORS, (int)(sizeof(WORLDBANK_INDICATORS)/sizeof(WORLDBANK_INDICATORS[0]))};
        case DataSource::BIS:        return {BIS_INDICATORS,       (int)(sizeof(BIS_INDICATORS)/sizeof(BIS_INDICATORS[0]))};
        case DataSource::IMF:        return {IMF_INDICATORS,       (int)(sizeof(IMF_INDICATORS)/sizeof(IMF_INDICATORS[0]))};
        case DataSource::FRED:       return {FRED_INDICATORS,      (int)(sizeof(FRED_INDICATORS)/sizeof(FRED_INDICATORS[0]))};
        case DataSource::OECD:       return {OECD_INDICATORS,      (int)(sizeof(OECD_INDICATORS)/sizeof(OECD_INDICATORS[0]))};
        case DataSource::WTO:        return {WTO_INDICATORS,       (int)(sizeof(WTO_INDICATORS)/sizeof(WTO_INDICATORS[0]))};
        case DataSource::CFTC:       return {CFTC_INDICATORS,      (int)(sizeof(CFTC_INDICATORS)/sizeof(CFTC_INDICATORS[0]))};
        case DataSource::EIA:        return {EIA_INDICATORS,       (int)(sizeof(EIA_INDICATORS)/sizeof(EIA_INDICATORS[0]))};
        case DataSource::ADB:        return {ADB_INDICATORS,       (int)(sizeof(ADB_INDICATORS)/sizeof(ADB_INDICATORS[0]))};
        case DataSource::Fed:        return {FED_INDICATORS,       (int)(sizeof(FED_INDICATORS)/sizeof(FED_INDICATORS[0]))};
        case DataSource::BLS:        return {BLS_INDICATORS,       (int)(sizeof(BLS_INDICATORS)/sizeof(BLS_INDICATORS[0]))};
        case DataSource::UNESCO:     return {UNESCO_INDICATORS,    (int)(sizeof(UNESCO_INDICATORS)/sizeof(UNESCO_INDICATORS[0]))};
        case DataSource::FiscalData: return {FISCALDATA_INDICATORS,(int)(sizeof(FISCALDATA_INDICATORS)/sizeof(FISCALDATA_INDICATORS[0]))};
        case DataSource::BEA:        return {BEA_INDICATORS,       (int)(sizeof(BEA_INDICATORS)/sizeof(BEA_INDICATORS[0]))};
        case DataSource::Fincept:    return {FINCEPT_INDICATORS,   (int)(sizeof(FINCEPT_INDICATORS)/sizeof(FINCEPT_INDICATORS[0]))};
        default:                     return {WORLDBANK_INDICATORS, (int)(sizeof(WORLDBANK_INDICATORS)/sizeof(WORLDBANK_INDICATORS[0]))};
    }
}

inline const char* get_default_indicator(DataSource src) {
    switch (src) {
        case DataSource::WorldBank:  return "NY.GDP.MKTP.CD";
        case DataSource::BIS:        return "WS_CBPOL";
        case DataSource::IMF:        return "NGDP_RPCH";
        case DataSource::FRED:       return "GDP";
        case DataSource::OECD:       return "GDP";
        case DataSource::WTO:        return "TP_A_0010";
        case DataSource::CFTC:       return "gold";
        case DataSource::EIA:        return "crude_petroleum_stocks";
        case DataSource::ADB:        return "NGDP_XDC";
        case DataSource::Fed:        return "federal_funds_rate";
        case DataSource::BLS:        return "CUSR0000SA0";
        case DataSource::UNESCO:     return "GER.1";
        case DataSource::FiscalData: return "total_public_debt";
        case DataSource::BEA:        return "gdp_growth";
        case DataSource::Fincept:    return "wgb_central_bank_rates";
        default:                     return "NY.GDP.MKTP.CD";
    }
}

// ============================================================================
// Countries (50+ with per-source code mapping)
// ============================================================================

inline const Country COUNTRIES[] = {
    {"USA", "United States",      "US", "USA", "US",  "USA", "840", nullptr},
    {"CHN", "China",              "CN", "CHN", nullptr,"CHN","156", "CHN"},
    {"JPN", "Japan",              "JP", "JPN", nullptr,"JPN","392", "JPN"},
    {"DEU", "Germany",            "DE", "DEU", nullptr,"DEU","276", nullptr},
    {"GBR", "United Kingdom",     "GB", "GBR", nullptr,"GBR","826", nullptr},
    {"FRA", "France",             "FR", "FRA", nullptr,"FRA","251", nullptr},
    {"IND", "India",              "IN", "IND", nullptr,"IND","699", "IND"},
    {"ITA", "Italy",              "IT", "ITA", nullptr,"ITA","381", nullptr},
    {"BRA", "Brazil",             "BR", "BRA", nullptr,"BRA","076", nullptr},
    {"CAN", "Canada",             "CA", "CAN", nullptr,"CAN","124", nullptr},
    {"RUS", "Russian Federation", "RU", "RUS", nullptr,"RUS","643", nullptr},
    {"KOR", "Korea, Rep.",        "KR", "KOR", nullptr,"KOR","410", "KOR"},
    {"AUS", "Australia",          "AU", "AUS", nullptr,"AUS","036", "AUS"},
    {"ESP", "Spain",              "ES", "ESP", nullptr,"ESP","724", nullptr},
    {"MEX", "Mexico",             "MX", "MEX", nullptr,"MEX","484", nullptr},
    {"IDN", "Indonesia",          "ID", "IDN", nullptr,"IDN","360", "IDN"},
    {"NLD", "Netherlands",        "NL", "NLD", nullptr,"NLD","528", nullptr},
    {"SAU", "Saudi Arabia",       "SA", "SAU", nullptr,nullptr,"682",nullptr},
    {"TUR", "Turkey",             "TR", "TUR", nullptr,"TUR","792", nullptr},
    {"CHE", "Switzerland",        "CH", "CHE", nullptr,"CHE","757", nullptr},
    {"POL", "Poland",             "PL", "POL", nullptr,"POL","616", nullptr},
    {"SWE", "Sweden",             "SE", "SWE", nullptr,"SWE","752", nullptr},
    {"BEL", "Belgium",            "BE", "BEL", nullptr,"BEL","056", nullptr},
    {"ARG", "Argentina",          "AR", "ARG", nullptr,"ARG","032", nullptr},
    {"NOR", "Norway",             "NO", "NOR", nullptr,"NOR","579", nullptr},
    {"AUT", "Austria",            "AT", "AUT", nullptr,"AUT","040", nullptr},
    {"ARE", "United Arab Emirates","AE","ARE", nullptr,nullptr,"784",nullptr},
    {"NGA", "Nigeria",            "NG", "NGA", nullptr,nullptr,"566",nullptr},
    {"ISR", "Israel",             "IL", "ISR", nullptr,"ISR","376", nullptr},
    {"ZAF", "South Africa",       "ZA", "ZAF", nullptr,"ZAF","710", nullptr},
    {"SGP", "Singapore",          "SG", "SGP", nullptr,nullptr,"702","SGP"},
    {"HKG", "Hong Kong",          "HK", "HKG", nullptr,nullptr,"344","HKG"},
    {"DNK", "Denmark",            "DK", "DNK", nullptr,"DNK","208", nullptr},
    {"MYS", "Malaysia",           "MY", "MYS", nullptr,nullptr,"458","MYS"},
    {"PHL", "Philippines",        "PH", "PHL", nullptr,nullptr,"608","PHI"},
    {"CHL", "Chile",              "CL", "CHL", nullptr,"CHL","152", nullptr},
    {"FIN", "Finland",            "FI", "FIN", nullptr,"FIN","246", nullptr},
    {"COL", "Colombia",           "CO", "COL", nullptr,"COL","170", nullptr},
    {"CZE", "Czech Republic",     "CZ", "CZE", nullptr,"CZE","203", nullptr},
    {"PRT", "Portugal",           "PT", "PRT", nullptr,"PRT","620", nullptr},
    {"NZL", "New Zealand",        "NZ", "NZL", nullptr,"NZL","554", "NZL"},
    {"GRC", "Greece",             "GR", "GRC", nullptr,"GRC","300", nullptr},
    {"HUN", "Hungary",            "HU", "HUN", nullptr,"HUN","348", nullptr},
    {"IRL", "Ireland",            "IE", "IRL", nullptr,"IRL","372", nullptr},
    {"THA", "Thailand",           "TH", "THA", nullptr,nullptr,"764","THA"},
    {"VNM", "Vietnam",            "VN", "VNM", nullptr,nullptr,"704","VNM"},
    {"TWN", "Taiwan",             "TW", nullptr,nullptr,nullptr,nullptr,"TWN"},
    {"PAK", "Pakistan",           "PK", "PAK", nullptr,nullptr,"586","PAK"},
    {"BGD", "Bangladesh",         "BD", "BGD", nullptr,nullptr,"050","BGD"},
};

constexpr int COUNTRY_COUNT = sizeof(COUNTRIES) / sizeof(COUNTRIES[0]);

} // namespace fincept::economics
