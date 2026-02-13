// Economics Tab Constants

import type { DataSourceConfig, Indicator, Country, DataSource } from './types';

// Fincept Design System Colors
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

// Data source configurations
export const DATA_SOURCES: DataSourceConfig[] = [
  { id: 'worldbank', name: 'World Bank', fullName: 'World Bank', description: 'Global development indicators', color: FINCEPT.CYAN, scriptName: 'worldbank_data.py' },
  { id: 'bis', name: 'BIS', fullName: 'Bank for International Settlements', description: 'Central bank statistics, exchange rates, credit', color: FINCEPT.PURPLE, scriptName: 'bis_data.py' },
  { id: 'imf', name: 'IMF', fullName: 'International Monetary Fund', description: 'World Economic Outlook, fiscal & monetary data', color: FINCEPT.BLUE, scriptName: 'imf_data.py' },
  { id: 'fred', name: 'FRED', fullName: 'Federal Reserve Economic Data', description: 'U.S. economic data from the Federal Reserve', color: FINCEPT.GREEN, scriptName: 'fred_data.py' },
  { id: 'oecd', name: 'OECD', fullName: 'Organisation for Economic Co-operation', description: 'Economic indicators for developed countries', color: FINCEPT.YELLOW, scriptName: 'oecd_data.py' },
  { id: 'wto', name: 'WTO', fullName: 'World Trade Organization', description: 'Trade statistics, tariffs & trade policy', color: '#E91E63', scriptName: 'wto_data.py', requiresApiKey: true, apiKeyName: 'WTO_API_KEY' },
  { id: 'cftc', name: 'CFTC', fullName: 'Commodity Futures Trading Commission', description: 'Commitment of Traders (COT) Reports', color: '#FF5722', scriptName: 'cftc_data.py' },
  { id: 'eia', name: 'EIA', fullName: 'Energy Information Administration', description: 'U.S. energy production & prices', color: '#4CAF50', scriptName: 'eia_data.py', requiresApiKey: true, apiKeyName: 'EIA_API_KEY' },
  { id: 'adb', name: 'ADB', fullName: 'Asian Development Bank', description: 'Asia-Pacific economic indicators', color: '#0072BC', scriptName: 'adb_data.py' },
  { id: 'fed', name: 'Fed', fullName: 'Federal Reserve', description: 'US monetary policy, rates & money supply', color: '#1A5F7A', scriptName: 'federal_reserve_data.py' },
  { id: 'bls', name: 'BLS', fullName: 'Bureau of Labor Statistics', description: 'US labor, prices, employment & productivity', color: '#AB47BC', scriptName: 'bls_data.py', requiresApiKey: true, apiKeyName: 'BLS_API_KEY' },
  { id: 'unesco', name: 'UNESCO', fullName: 'UNESCO Institute for Statistics', description: 'Education, science, culture & demographic data', color: '#00ACC1', scriptName: 'unesco_data.py' },
  { id: 'fiscaldata', name: 'FiscalData', fullName: 'U.S. Treasury FiscalData', description: 'US debt, interest rates, spending & gold reserves', color: '#FFA726', scriptName: 'fiscal_data.py' },
  { id: 'bea', name: 'BEA', fullName: 'Bureau of Economic Analysis', description: 'US GDP, income, spending & national accounts (NIPA)', color: '#E65100', scriptName: 'bea_data.py', requiresApiKey: true, apiKeyName: 'BEA_API_KEY' },
];

// World Bank indicators
export const WORLDBANK_INDICATORS: Indicator[] = [
  { id: 'NY.GDP.MKTP.CD', name: 'GDP (current US$)', category: 'GDP' },
  { id: 'NY.GDP.MKTP.KD.ZG', name: 'GDP growth (annual %)', category: 'GDP' },
  { id: 'NY.GDP.PCAP.CD', name: 'GDP per capita (current US$)', category: 'GDP' },
  { id: 'FP.CPI.TOTL.ZG', name: 'Inflation, consumer prices (annual %)', category: 'Prices' },
  { id: 'SL.UEM.TOTL.ZS', name: 'Unemployment, total (% of labor force)', category: 'Labor' },
  { id: 'NE.EXP.GNFS.ZS', name: 'Exports of goods and services (% of GDP)', category: 'Trade' },
  { id: 'NE.IMP.GNFS.ZS', name: 'Imports of goods and services (% of GDP)', category: 'Trade' },
  { id: 'BN.CAB.XOKA.GD.ZS', name: 'Current account balance (% of GDP)', category: 'Trade' },
  { id: 'GC.DOD.TOTL.GD.ZS', name: 'Central government debt (% of GDP)', category: 'Government' },
  { id: 'SP.POP.TOTL', name: 'Population, total', category: 'Demographics' },
  { id: 'SP.POP.GROW', name: 'Population growth (annual %)', category: 'Demographics' },
  { id: 'FR.INR.RINR', name: 'Real interest rate (%)', category: 'Financial' },
  { id: 'PA.NUS.FCRF', name: 'Official exchange rate (LCU per US$)', category: 'Financial' },
  { id: 'FI.RES.TOTL.CD', name: 'Total reserves (includes gold, current US$)', category: 'Financial' },
  { id: 'BX.KLT.DINV.WD.GD.ZS', name: 'Foreign direct investment, net inflows (% of GDP)', category: 'Investment' },
];

// BIS indicators — mapped to real BIS SDMX dataflow IDs
// Full coverage of all 28 BIS statistical domains (excluding release calendar)
export const BIS_INDICATORS: Indicator[] = [
  // Monetary Policy & Interest Rates
  { id: 'WS_CBPOL', name: 'Central Bank Policy Rates', category: 'Rates' },
  { id: 'WS_CBTA', name: 'Central Bank Total Assets', category: 'Rates' },
  // Exchange Rates
  { id: 'WS_EER', name: 'Effective Exchange Rates (Nominal, Broad)', category: 'Exchange Rates' },
  { id: 'WS_XRU', name: 'US Dollar Exchange Rates', category: 'Exchange Rates' },
  // Prices
  { id: 'WS_LONG_CPI', name: 'Consumer Prices (CPI)', category: 'Prices' },
  // Property
  { id: 'WS_SPP', name: 'Selected Residential Property Prices', category: 'Property' },
  { id: 'WS_DPP', name: 'Detailed Residential Property Prices', category: 'Property' },
  { id: 'WS_CPP', name: 'Commercial Property Prices', category: 'Property' },
  // Credit & Debt
  { id: 'WS_TC', name: 'Total Credit to Non-Financial Sector', category: 'Credit' },
  { id: 'WS_CREDIT_GAP', name: 'Credit-to-GDP Gaps', category: 'Credit' },
  { id: 'WS_DSR', name: 'Debt Service Ratios', category: 'Credit' },
  { id: 'WS_GLI', name: 'Global Liquidity Indicators', category: 'Credit' },
  // Debt Securities
  { id: 'WS_DEBT_SEC2_PUB', name: 'International Debt Securities', category: 'Debt Securities' },
  { id: 'WS_NA_SEC_DSS', name: 'Debt Securities Statistics', category: 'Debt Securities' },
  // Banking
  { id: 'WS_LBS_D_PUB', name: 'Locational Banking Statistics', category: 'Banking' },
  { id: 'WS_CBS_PUB', name: 'Consolidated Banking Statistics', category: 'Banking' },
  // Derivatives
  { id: 'WS_OTC_DERIV2', name: 'OTC Derivatives Outstanding', category: 'Derivatives' },
  { id: 'WS_DER_OTC_TOV', name: 'OTC Derivatives Turnover', category: 'Derivatives' },
  { id: 'WS_XTD_DERIV', name: 'Exchange-Traded Derivatives', category: 'Derivatives' },
  // CPMI Payment Statistics
  { id: 'WS_CPMI_MACRO', name: 'CPMI Macro Statistics', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_CASHLESS', name: 'CPMI Cashless Payments', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_CT1', name: 'CPMI Comparative Tables Type 1', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_CT2', name: 'CPMI Comparative Tables Type 2', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_DEVICES', name: 'CPMI Payment Devices', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_INSTITUT', name: 'CPMI Institutions', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_PARTICIP', name: 'CPMI Participants', category: 'Payments (CPMI)' },
  { id: 'WS_CPMI_SYSTEMS', name: 'CPMI Systems', category: 'Payments (CPMI)' },
];

// IMF indicators
export const IMF_INDICATORS: Indicator[] = [
  { id: 'NGDP_RPCH', name: 'Real GDP growth (Annual %)', category: 'GDP' },
  { id: 'NGDPD', name: 'GDP, current prices (Billions USD)', category: 'GDP' },
  { id: 'NGDPDPC', name: 'GDP per capita, current prices (USD)', category: 'GDP' },
  { id: 'PCPIPCH', name: 'Inflation, average consumer prices (Annual %)', category: 'Prices' },
  { id: 'PCPIEPCH', name: 'Inflation, end of period (Annual %)', category: 'Prices' },
  { id: 'LUR', name: 'Unemployment rate (%)', category: 'Labor' },
  { id: 'BCA_NGDPD', name: 'Current account balance (% of GDP)', category: 'Trade' },
  { id: 'GGXWDG_NGDP', name: 'General government gross debt (% of GDP)', category: 'Government' },
  { id: 'GGXCNL_NGDP', name: 'General government net lending (% of GDP)', category: 'Government' },
  { id: 'GGR_NGDP', name: 'General government revenue (% of GDP)', category: 'Government' },
];

// FRED indicators (US-focused)
export const FRED_INDICATORS: Indicator[] = [
  { id: 'GDP', name: 'Gross Domestic Product', category: 'GDP' },
  { id: 'GDPC1', name: 'Real Gross Domestic Product', category: 'GDP' },
  { id: 'CPIAUCSL', name: 'Consumer Price Index for All Urban Consumers', category: 'Prices' },
  { id: 'CPILFESL', name: 'Core CPI (Less Food and Energy)', category: 'Prices' },
  { id: 'UNRATE', name: 'Unemployment Rate', category: 'Labor' },
  { id: 'PAYEMS', name: 'All Employees, Total Nonfarm', category: 'Labor' },
  { id: 'FEDFUNDS', name: 'Federal Funds Effective Rate', category: 'Rates' },
  { id: 'DGS10', name: '10-Year Treasury Constant Maturity Rate', category: 'Rates' },
  { id: 'DGS2', name: '2-Year Treasury Constant Maturity Rate', category: 'Rates' },
  { id: 'DEXUSEU', name: 'US / Euro Foreign Exchange Rate', category: 'Exchange Rates' },
  { id: 'DTWEXBGS', name: 'Trade Weighted US Dollar Index', category: 'Exchange Rates' },
  { id: 'M2SL', name: 'M2 Money Stock', category: 'Money Supply' },
  { id: 'WALCL', name: 'Federal Reserve Total Assets', category: 'Fed Balance Sheet' },
  { id: 'HOUST', name: 'Housing Starts', category: 'Housing' },
  { id: 'INDPRO', name: 'Industrial Production Index', category: 'Production' },
];

// OECD indicators
export const OECD_INDICATORS: Indicator[] = [
  { id: 'GDP', name: 'Gross Domestic Product', category: 'GDP' },
  { id: 'RGDP', name: 'Real GDP', category: 'GDP' },
  { id: 'CPI', name: 'Consumer Price Index', category: 'Prices' },
  { id: 'UNR', name: 'Unemployment Rate', category: 'Labor' },
  { id: 'LF', name: 'Labour Force', category: 'Labor' },
  { id: 'IRSTCI', name: 'Short-term Interest Rates', category: 'Rates' },
  { id: 'IRLTLT', name: 'Long-term Interest Rates', category: 'Rates' },
  { id: 'EXCH', name: 'Exchange Rates', category: 'Exchange Rates' },
  { id: 'TBAL', name: 'Trade Balance', category: 'Trade' },
  { id: 'CLI', name: 'Composite Leading Indicators', category: 'Leading Indicators' },
  { id: 'BCI', name: 'Business Confidence Index', category: 'Confidence' },
  { id: 'CCI', name: 'Consumer Confidence Index', category: 'Confidence' },
];

// WTO indicators (Timeseries API)
export const WTO_INDICATORS: Indicator[] = [
  // MFN Applied Tariffs
  { id: 'TP_A_0010', name: 'Simple Average MFN Tariff - All Products', category: 'Tariffs' },
  { id: 'TP_A_0160', name: 'Simple Average MFN Tariff - Agricultural', category: 'Tariffs' },
  { id: 'TP_A_0430', name: 'Simple Average MFN Tariff - Non-Agricultural', category: 'Tariffs' },
  { id: 'TP_A_0030', name: 'Trade-Weighted MFN Tariff - All Products', category: 'Tariffs' },
  { id: 'TP_A_0170', name: 'Trade-Weighted MFN Tariff - Agricultural', category: 'Tariffs' },
  { id: 'TP_A_0440', name: 'Trade-Weighted MFN Tariff - Non-Agricultural', category: 'Tariffs' },
  // Bound Tariffs
  { id: 'TP_A_0020', name: 'Simple Average Bound Tariff - All Products', category: 'Bound Tariffs' },
  { id: 'TP_A_0310', name: 'Simple Average Bound Tariff - Agricultural', category: 'Bound Tariffs' },
  { id: 'TP_A_0560', name: 'Simple Average Bound Tariff - Non-Agricultural', category: 'Bound Tariffs' },
  // Tariff Peaks and Bindings
  { id: 'TP_A_0100', name: 'Share of Duty-Free Tariff Lines - All', category: 'Tariff Structure' },
  { id: 'TP_A_0090', name: 'Binding Coverage - All Products', category: 'Tariff Structure' },
  { id: 'TP_A_0130', name: 'Share of Domestic Peaks - All Products', category: 'Tariff Structure' },
  { id: 'TP_A_0140', name: 'Share of International Peaks - All Products', category: 'Tariff Structure' },
  // Maximum Tariffs
  { id: 'TP_A_0070', name: 'Maximum MFN Applied Tariff - All Products', category: 'Tariff Peaks' },
  { id: 'TP_A_0080', name: 'Maximum Bound Tariff - All Products', category: 'Tariff Peaks' },
  // Non-Ad Valorem Tariffs
  { id: 'TP_A_0110', name: 'Share of Non-Ad Valorem Tariffs - All', category: 'Tariff Types' },
  { id: 'TP_A_0250', name: 'Share of Non-Ad Valorem Tariffs - Agricultural', category: 'Tariff Types' },
  { id: 'TP_A_0520', name: 'Share of Non-Ad Valorem Tariffs - Non-Agricultural', category: 'Tariff Types' },
];

// CFTC COT Markets (Commitment of Traders)
export const CFTC_MARKETS: Indicator[] = [
  // Agricultural Commodities
  { id: 'corn', name: 'Corn', category: 'Agricultural', code: '002602' },
  { id: 'wheat', name: 'Wheat (SRW)', category: 'Agricultural', code: '001602' },
  { id: 'soybeans', name: 'Soybeans', category: 'Agricultural', code: '005602' },
  { id: 'cotton', name: 'Cotton', category: 'Agricultural', code: '033661' },
  { id: 'cocoa', name: 'Cocoa', category: 'Agricultural', code: '073732' },
  { id: 'coffee', name: 'Coffee', category: 'Agricultural', code: '083731' },
  { id: 'sugar', name: 'Sugar', category: 'Agricultural', code: '080732' },
  { id: 'live_cattle', name: 'Live Cattle', category: 'Agricultural', code: '057642' },
  { id: 'lean_hogs', name: 'Lean Hogs', category: 'Agricultural', code: '054642' },
  // Energy
  { id: 'crude_oil', name: 'Crude Oil (WTI)', category: 'Energy', code: '067651' },
  { id: 'natural_gas', name: 'Natural Gas (Henry Hub)', category: 'Energy', code: '02365B' },
  { id: 'gasoline', name: 'RBOB Gasoline', category: 'Energy', code: '111659' },
  { id: 'heating_oil', name: 'Heating Oil', category: 'Energy', code: '022651' },
  // Precious Metals
  { id: 'gold', name: 'Gold', category: 'Metals', code: '088691' },
  { id: 'silver', name: 'Silver', category: 'Metals', code: '084691' },
  { id: 'copper', name: 'Copper', category: 'Metals', code: '085692' },
  { id: 'platinum', name: 'Platinum', category: 'Metals', code: '076651' },
  { id: 'palladium', name: 'Palladium', category: 'Metals', code: '075651' },
  // Currencies
  { id: 'euro', name: 'Euro FX', category: 'Currencies', code: '099741' },
  { id: 'jpy', name: 'Japanese Yen', category: 'Currencies', code: '097741' },
  { id: 'british_pound', name: 'British Pound', category: 'Currencies', code: '096742' },
  { id: 'swiss_franc', name: 'Swiss Franc', category: 'Currencies', code: '092741' },
  { id: 'canadian_dollar', name: 'Canadian Dollar', category: 'Currencies', code: '090741' },
  { id: 'australian_dollar', name: 'Australian Dollar', category: 'Currencies', code: '232741' },
  { id: 'us_dollar_index', name: 'US Dollar Index', category: 'Currencies', code: '098662' },
  // Stock Index Futures
  { id: 's&p_500', name: 'E-mini S&P 500', category: 'Indices', code: '13874A' },
  { id: 'nasdaq_100', name: 'Nasdaq 100 E-mini', category: 'Indices', code: '209742' },
  { id: 'dow_jones', name: 'Dow Jones E-mini', category: 'Indices', code: '124603' },
  { id: 'nikkei', name: 'Nikkei 225', category: 'Indices', code: '240741' },
  { id: 'vix', name: 'VIX Futures', category: 'Indices', code: '1170E1' },
  // Interest Rates
  { id: 'treasury_bonds', name: 'US Treasury Bonds', category: 'Interest Rates', code: '020601' },
  { id: 'treasury_notes_2y', name: '2-Year Treasury Notes', category: 'Interest Rates', code: '042601' },
  { id: 'treasury_notes_5y', name: '5-Year Treasury Notes', category: 'Interest Rates', code: '044601' },
  { id: 'treasury_notes_10y', name: '10-Year Treasury Notes', category: 'Interest Rates', code: '043602' },
  { id: 'fed_funds', name: '30-Day Fed Funds', category: 'Interest Rates', code: '045601' },
  // Cryptocurrencies
  { id: 'bitcoin', name: 'Bitcoin', category: 'Crypto', code: '133741' },
  { id: 'ether', name: 'Ether', category: 'Crypto', code: '146021' },
];

// ADB Indicators (Asian Development Bank - Key Indicators Database)
// API: https://kidb.adb.org/api - Uses SDMX format with dataflows: EO_NA, PPL_POP
// These are the actual verified indicator codes from the ADB API
export const ADB_INDICATORS: Indicator[] = [
  // === ECONOMY AND OUTPUT (Dataflow: EO_NA) ===
  { id: 'NGDP_XDC', name: 'GDP at Current Prices', category: 'Economy & Output' },
  { id: 'NGDPPC_XDC', name: 'GDP per Capita', category: 'Economy & Output' },
  { id: 'NYG_XDC', name: 'GNI at Current Prices', category: 'Economy & Output' },
  { id: 'NYGPC_XDC', name: 'GNI per Capita', category: 'Economy & Output' },
  { id: 'NYG_USD', name: 'GNI (USD millions)', category: 'Economy & Output' },
  // GDP Composition
  { id: 'NGDPSO_AGR_XGDP_PS', name: 'Agriculture (% of GDP)', category: 'GDP Composition' },
  { id: 'NGDPSO_IND_XGDP_PS', name: 'Industry (% of GDP)', category: 'GDP Composition' },
  { id: 'NGDPSO_SER_XGDP_PS', name: 'Services (% of GDP)', category: 'GDP Composition' },
  // Expenditure
  { id: 'NC_HFC_XGDP_PS', name: 'Household Consumption (% of GDP)', category: 'Expenditure' },
  { id: 'NCGG_XGDP_PS', name: 'Government Consumption (% of GDP)', category: 'Expenditure' },
  { id: 'NI_XGDP_PS', name: 'Gross Capital Formation (% of GDP)', category: 'Expenditure' },
  { id: 'NEGS_XGDP_PS', name: 'Exports of Goods & Services (% of GDP)', category: 'Trade' },
  { id: 'NIGS_XGDP_PS', name: 'Imports of Goods & Services (% of GDP)', category: 'Trade' },
  // Sector Value Added
  { id: 'NGDPVA_ISIC4_A_XDC', name: 'Agriculture, Forestry, Fishing', category: 'Sector Output' },
  { id: 'NGDPVA_ISIC4_C_XDC', name: 'Manufacturing', category: 'Sector Output' },
  { id: 'NGDPVA_ISIC4_F_XDC', name: 'Construction', category: 'Sector Output' },
  { id: 'NGDPVA_ISIC4_G_XDC', name: 'Wholesale & Retail Trade', category: 'Sector Output' },
  { id: 'NGDPVA_ISIC4_K_XDC', name: 'Financial & Insurance', category: 'Sector Output' },
  { id: 'NGDPVA_ISIC4_L_XDC', name: 'Real Estate', category: 'Sector Output' },
  { id: 'NGDPVA_ISIC4_J_XDC', name: 'Information & Communication', category: 'Sector Output' },

  // === POPULATION (Dataflow: PPL_POP) ===
  { id: 'LP_PE_NUM_MOP', name: 'Total Population', category: 'Population' },
  { id: 'LP_MOP_PTX_PS', name: 'Population Growth Rate (%)', category: 'Population' },
  { id: 'LP_PE_PD_PS', name: 'Population Density (per sq km)', category: 'Population' },
  { id: 'LP_UP_PTX_PS', name: 'Urban Population (%)', category: 'Population' },
  { id: 'SP_POP_0014_TO_ZS', name: 'Population Age 0-14 (%)', category: 'Demographics' },
  { id: 'SP_POP_1564_TO_ZS', name: 'Population Age 15-64 (%)', category: 'Demographics' },
  { id: 'SP_POP_GTE_65_TO_ZS_RT_PS', name: 'Population Age 65+ (%)', category: 'Demographics' },
  { id: 'SP_POP_DPND', name: 'Age Dependency Ratio', category: 'Demographics' },
  { id: 'SM_POP_NETM_RT_PS', name: 'Net Migration Rate', category: 'Demographics' },
];

// Federal Reserve Indicators (NY Fed & Federal Reserve Board)
// Note: Daily data sources use date range to limit data volume
export const FED_INDICATORS: Indicator[] = [
  // Key Interest Rates (Daily data - last 1 year by default)
  { id: 'federal_funds_rate', name: 'Federal Funds Rate (EFFR)', category: 'Interest Rates' },
  { id: 'sofr_rate', name: 'SOFR (Secured Overnight Financing Rate)', category: 'Interest Rates' },
  // Treasury Rates (Daily data - last 1 year by default)
  { id: 'treasury_rates', name: 'Treasury Rates (All Maturities)', category: 'Treasury' },
  { id: 'yield_curve', name: 'Yield Curve (Latest)', category: 'Treasury' },
  // Money Supply (Monthly data)
  { id: 'money_measures', name: 'Money Supply (M1, M2)', category: 'Money Supply' },
  { id: 'money_measures_adjusted', name: 'Money Supply (Seasonally Adjusted)', category: 'Money Supply' },
  // Central Bank Operations
  { id: 'central_bank_holdings', name: 'Fed Holdings (SOMA)', category: 'Central Bank' },
  // Composite Data (Recent 7 days)
  { id: 'market_overview', name: 'Market Overview (Recent)', category: 'Overview' },
];

// BLS Indicators (Bureau of Labor Statistics)
// NOTE: Use native BLS series IDs, NOT FRED aliases (e.g. CUSR0000SA0, not CPIAUCSL)
export const BLS_INDICATORS: Indicator[] = [
  // Consumer Price Index
  { id: 'CUSR0000SA0', name: 'CPI All Urban Consumers: All Items (SA)', category: 'CPI' },
  { id: 'CUUR0000SA0', name: 'CPI All Urban Consumers: All Items (NSA)', category: 'CPI' },
  { id: 'CUSR0000SA0L1E', name: 'CPI All Items Less Food & Energy', category: 'CPI' },
  { id: 'CUSR0000SA0E', name: 'CPI Energy', category: 'CPI' },
  { id: 'CUSR0000SAF1', name: 'CPI Food', category: 'CPI' },
  // Employment
  { id: 'CES0000000001', name: 'Total Nonfarm Payrolls', category: 'Employment' },
  { id: 'CES0500000003', name: 'Average Hourly Earnings (Private)', category: 'Employment' },
  { id: 'CES0500000002', name: 'Average Weekly Hours (Private)', category: 'Employment' },
  // Unemployment
  { id: 'LNS14000000', name: 'Unemployment Rate', category: 'Unemployment' },
  { id: 'LNS13000000', name: 'Labor Force Participation Rate', category: 'Unemployment' },
  { id: 'LNS11300000', name: 'Employment-Population Ratio', category: 'Unemployment' },
  // Producer Price Index
  { id: 'WPUFD49104', name: 'PPI Final Demand', category: 'PPI' },
  { id: 'WPUFD49116', name: 'PPI Final Demand Less Foods & Energy', category: 'PPI' },
  // JOLTS
  { id: 'JTS000000000000000JOL', name: 'JOLTS Job Openings', category: 'JOLTS' },
  { id: 'JTS000000000000000HIL', name: 'JOLTS Hires', category: 'JOLTS' },
  { id: 'JTS000000000000000TSL', name: 'JOLTS Total Separations', category: 'JOLTS' },
  { id: 'JTS000000000000000QUL', name: 'JOLTS Quits', category: 'JOLTS' },
  // Productivity
  { id: 'PRS85006092', name: 'Nonfarm Business Labor Productivity', category: 'Productivity' },
  { id: 'PRS85006112', name: 'Nonfarm Business Unit Labor Costs', category: 'Productivity' },
  // Employment Cost Index
  { id: 'CIU1010000000000A', name: 'ECI Total Compensation (Private)', category: 'Wages & Costs' },
  { id: 'CIU2020000000000A', name: 'ECI Wages & Salaries (Private)', category: 'Wages & Costs' },
];

// UNESCO UIS Indicators (UNESCO Institute for Statistics)
// Verified indicator codes from live API: https://api.uis.unesco.org/api/public
// Uses 3-letter ISO country codes (USA, GBR, IND, etc.)
export const UNESCO_INDICATORS: Indicator[] = [
  // Education - Enrollment
  { id: 'GER.1', name: 'Gross Enrolment Ratio, Primary (%)', category: 'Education' },
  { id: 'GER.2T3', name: 'Gross Enrolment Ratio, Secondary (%)', category: 'Education' },
  { id: 'GER.5T8', name: 'Gross Enrolment Ratio, Tertiary (%)', category: 'Education' },
  { id: 'NER.02.CP', name: 'Net Enrolment Rate, Pre-Primary (%)', category: 'Education' },
  // Education - Completion & Survival
  { id: 'CR.MOD.1', name: 'Completion Rate, Primary (%)', category: 'Education' },
  { id: 'CR.MOD.2', name: 'Completion Rate, Lower Secondary (%)', category: 'Education' },
  { id: 'CR.MOD.3', name: 'Completion Rate, Upper Secondary (%)', category: 'Education' },
  { id: 'SR.1.GLAST.CP', name: 'Survival Rate to Last Grade, Primary (%)', category: 'Education' },
  // Education - Out of School
  { id: 'ROFST.1.CP', name: 'Out-of-School Rate, Primary (%)', category: 'Education' },
  // Literacy
  { id: 'LR.AG15T99', name: 'Adult Literacy Rate, 15+ (%)', category: 'Literacy' },
  { id: 'LR.AG15T24', name: 'Youth Literacy Rate, 15-24 (%)', category: 'Literacy' },
  { id: 'LR.AG25T64', name: 'Literacy Rate, 25-64 (%)', category: 'Literacy' },
  // Education Expenditure
  { id: 'XGDP.FSGOV', name: 'Government Education Expenditure (% of GDP)', category: 'Education Spending' },
  { id: 'XGDP.1.FSGOV', name: 'Primary Education Expenditure (% of GDP)', category: 'Education Spending' },
  { id: 'XGDP.5T8.FSGOV', name: 'Tertiary Education Expenditure (% of GDP)', category: 'Education Spending' },
  // Pupil-Teacher Ratio
  { id: 'PTRHC.1.TRAINED', name: 'Pupil-Teacher Ratio, Primary', category: 'Teaching' },
  { id: 'PTRHC.2T3.TRAINED', name: 'Pupil-Teacher Ratio, Secondary', category: 'Teaching' },
  // Science, Technology & Innovation
  { id: 'EXPGDP.TOT', name: 'R&D Expenditure (% of GDP)', category: 'Science & Tech' },
  { id: 'RESDEN.INHAB.TFTE', name: 'Researchers per Million (FTE)', category: 'Science & Tech' },
  { id: 'FRESP.THC', name: 'Female Researchers (% of Total)', category: 'Science & Tech' },
  // Demographics & Economy
  { id: '200101', name: 'Total Population (thousands)', category: 'Demographics' },
  { id: 'NY.GDP.MKTP.CD', name: 'GDP (current US$)', category: 'Economy' },
  { id: 'NY.GDP.MKTP.KD.ZG', name: 'GDP Growth (annual %)', category: 'Economy' },
  { id: 'NY.GDP.PCAP.CD', name: 'GDP per Capita (current US$)', category: 'Economy' },
];

// FiscalData Indicators (U.S. Treasury FiscalData)
// Public API: https://api.fiscaldata.treasury.gov — no API key required
// US-only data source
export const FISCALDATA_INDICATORS: Indicator[] = [
  // Public Debt
  { id: 'total_public_debt', name: 'Total Public Debt Outstanding', category: 'Debt' },
  { id: 'debt_held_public', name: 'Debt Held by the Public', category: 'Debt' },
  { id: 'intragov_holdings', name: 'Intragovernmental Holdings', category: 'Debt' },
  // Average Interest Rates on Treasury Securities
  { id: 'avg_rate_tbills', name: 'Avg Interest Rate - Treasury Bills', category: 'Interest Rates' },
  { id: 'avg_rate_tnotes', name: 'Avg Interest Rate - Treasury Notes', category: 'Interest Rates' },
  { id: 'avg_rate_tbonds', name: 'Avg Interest Rate - Treasury Bonds', category: 'Interest Rates' },
  { id: 'avg_rate_tips', name: 'Avg Interest Rate - TIPS', category: 'Interest Rates' },
  { id: 'avg_rate_frn', name: 'Avg Interest Rate - Floating Rate Notes', category: 'Interest Rates' },
  // Interest Expense
  { id: 'interest_expense_total', name: 'Total Interest Expense (FYTD)', category: 'Interest Expense' },
  { id: 'interest_expense_monthly', name: 'Monthly Interest Expense', category: 'Interest Expense' },
  // Federal Outlays by Function (MTS Table 9)
  { id: 'outlays_health', name: 'Federal Outlays - Health (FYTD)', category: 'Outlays' },
  { id: 'outlays_defense', name: 'Federal Outlays - National Defense (FYTD)', category: 'Outlays' },
  { id: 'outlays_social_security', name: 'Federal Outlays - Social Security (FYTD)', category: 'Outlays' },
  { id: 'outlays_net_interest', name: 'Federal Outlays - Net Interest (FYTD)', category: 'Outlays' },
  { id: 'outlays_education', name: 'Federal Outlays - Education (FYTD)', category: 'Outlays' },
  // Gold Reserve
  { id: 'gold_reserve_total', name: 'U.S. Gold Reserve (Book Value)', category: 'Gold Reserve' },
  { id: 'gold_reserve_ounces', name: 'U.S. Gold Reserve (Troy Ounces)', category: 'Gold Reserve' },
];

// BEA Indicators (Bureau of Economic Analysis - NIPA tables)
// API: https://apps.bea.gov/api/data/ — requires API key (free registration)
// US-only data source, annual frequency
export const BEA_INDICATORS: Indicator[] = [
  // GDP
  { id: 'gdp_growth', name: 'Real GDP Growth (% Change)', category: 'GDP' },
  { id: 'nominal_gdp', name: 'Nominal GDP (Billions $)', category: 'GDP' },
  { id: 'real_gdp', name: 'Real GDP (Chained 2017 $, Billions)', category: 'GDP' },
  { id: 'gdp_deflator', name: 'GDP Price Index', category: 'GDP' },
  { id: 'gdp_price_change', name: 'GDP Price Change (%)', category: 'GDP' },
  { id: 'gdp_per_capita', name: 'GDP per Capita (Current $)', category: 'GDP' },
  // Consumption & Investment
  { id: 'pce', name: 'Personal Consumption Expenditures (Billions $)', category: 'Consumption' },
  { id: 'pce_goods', name: 'PCE Goods (Billions $)', category: 'Consumption' },
  { id: 'pce_services', name: 'PCE Services (Billions $)', category: 'Consumption' },
  { id: 'gross_investment', name: 'Gross Private Domestic Investment (Billions $)', category: 'Investment' },
  { id: 'fixed_investment', name: 'Fixed Investment (Billions $)', category: 'Investment' },
  // Trade
  { id: 'net_exports', name: 'Net Exports (Billions $)', category: 'Trade' },
  { id: 'exports', name: 'Exports of Goods & Services (Billions $)', category: 'Trade' },
  { id: 'imports', name: 'Imports of Goods & Services (Billions $)', category: 'Trade' },
  { id: 'current_account', name: 'Current Account Balance (Billions $)', category: 'Trade' },
  // Income
  { id: 'personal_income', name: 'Personal Income (Billions $)', category: 'Income' },
  { id: 'compensation', name: 'Compensation of Employees (Billions $)', category: 'Income' },
  { id: 'wages_salaries', name: 'Wages and Salaries (Billions $)', category: 'Income' },
  { id: 'disposable_income', name: 'Disposable Personal Income (Billions $)', category: 'Income' },
  { id: 'gdi', name: 'Gross Domestic Income (Billions $)', category: 'Income' },
  // Saving
  { id: 'personal_saving', name: 'Personal Saving (Billions $)', category: 'Saving' },
  { id: 'saving_rate', name: 'Personal Saving Rate (%)', category: 'Saving' },
  { id: 'gross_saving', name: 'Gross Saving (Billions $)', category: 'Saving' },
  { id: 'net_saving', name: 'Net Saving (Billions $)', category: 'Saving' },
  // Inflation
  { id: 'pce_inflation', name: 'PCE Price Index (% Change)', category: 'Inflation' },
  { id: 'core_pce_inflation', name: 'Core PCE Price Index (% Change, ex Food & Energy)', category: 'Inflation' },
  // Government
  { id: 'govt_spending', name: 'Government Spending (Billions $)', category: 'Government' },
  { id: 'federal_spending', name: 'Federal Government Spending (Billions $)', category: 'Government' },
  { id: 'defense_spending', name: 'National Defense Spending (Billions $)', category: 'Government' },
  { id: 'govt_receipts', name: 'Government Current Receipts (Billions $)', category: 'Government' },
  { id: 'personal_taxes', name: 'Personal Current Taxes (Billions $)', category: 'Government' },
  { id: 'corporate_taxes', name: 'Taxes on Corporate Income (Billions $)', category: 'Government' },
];

// EIA Indicators (U.S. Energy Information Administration)
export const EIA_INDICATORS: Indicator[] = [
  // Petroleum Status Report Categories
  { id: 'balance_sheet', name: 'Petroleum Balance Sheet', category: 'Petroleum' },
  { id: 'inputs_and_production', name: 'Refinery Inputs & Production', category: 'Petroleum' },
  { id: 'crude_petroleum_stocks', name: 'Crude Oil Stocks', category: 'Petroleum' },
  { id: 'gasoline_fuel_stocks', name: 'Gasoline Stocks', category: 'Petroleum' },
  { id: 'distillate_fuel_oil_stocks', name: 'Distillate Fuel Oil Stocks', category: 'Petroleum' },
  { id: 'imports', name: 'Petroleum Imports', category: 'Petroleum' },
  { id: 'imports_by_country', name: 'Petroleum Imports by Country', category: 'Petroleum' },
  { id: 'spot_prices_crude_gas_heating', name: 'Spot Prices (Crude, Gas, Heating)', category: 'Prices' },
  { id: 'spot_prices_diesel_jet_fuel_propane', name: 'Spot Prices (Diesel, Jet, Propane)', category: 'Prices' },
  { id: 'retail_prices', name: 'Retail Fuel Prices', category: 'Prices' },
  // STEO Tables (Short Term Energy Outlook) - requires API key
  { id: 'steo_01', name: 'US Energy Markets Summary', category: 'STEO' },
  { id: 'steo_02', name: 'Nominal Energy Prices', category: 'STEO' },
  { id: 'steo_04a', name: 'US Petroleum Supply & Consumption', category: 'STEO' },
  { id: 'steo_05a', name: 'US Natural Gas Supply & Consumption', category: 'STEO' },
  { id: 'steo_07a', name: 'US Electricity Overview', category: 'STEO' },
  { id: 'steo_09a', name: 'US Macro Indicators & CO2 Emissions', category: 'STEO' },
];

// Countries list (with codes for each data source)
export const COUNTRIES: Country[] = [
  { code: 'USA', name: 'United States', bis: 'US', imf: 'USA', fred: 'US', oecd: 'USA', wto: '840' },
  { code: 'CHN', name: 'China', bis: 'CN', imf: 'CHN', oecd: 'CHN', wto: '156', adb: 'CHN' },
  { code: 'JPN', name: 'Japan', bis: 'JP', imf: 'JPN', oecd: 'JPN', wto: '392', adb: 'JPN' },
  { code: 'DEU', name: 'Germany', bis: 'DE', imf: 'DEU', oecd: 'DEU', wto: '276' },
  { code: 'GBR', name: 'United Kingdom', bis: 'GB', imf: 'GBR', oecd: 'GBR', wto: '826' },
  { code: 'FRA', name: 'France', bis: 'FR', imf: 'FRA', oecd: 'FRA', wto: '251' },
  { code: 'IND', name: 'India', bis: 'IN', imf: 'IND', oecd: 'IND', wto: '699', adb: 'IND' },
  { code: 'ITA', name: 'Italy', bis: 'IT', imf: 'ITA', oecd: 'ITA', wto: '381' },
  { code: 'BRA', name: 'Brazil', bis: 'BR', imf: 'BRA', oecd: 'BRA', wto: '076' },
  { code: 'CAN', name: 'Canada', bis: 'CA', imf: 'CAN', oecd: 'CAN', wto: '124' },
  { code: 'RUS', name: 'Russian Federation', bis: 'RU', imf: 'RUS', oecd: 'RUS', wto: '643' },
  { code: 'KOR', name: 'Korea, Rep.', bis: 'KR', imf: 'KOR', oecd: 'KOR', wto: '410', adb: 'KOR' },
  { code: 'AUS', name: 'Australia', bis: 'AU', imf: 'AUS', oecd: 'AUS', wto: '036', adb: 'AUS' },
  { code: 'ESP', name: 'Spain', bis: 'ES', imf: 'ESP', oecd: 'ESP', wto: '724' },
  { code: 'MEX', name: 'Mexico', bis: 'MX', imf: 'MEX', oecd: 'MEX', wto: '484' },
  { code: 'IDN', name: 'Indonesia', bis: 'ID', imf: 'IDN', oecd: 'IDN', wto: '360', adb: 'IDN' },
  { code: 'NLD', name: 'Netherlands', bis: 'NL', imf: 'NLD', oecd: 'NLD', wto: '528' },
  { code: 'SAU', name: 'Saudi Arabia', bis: 'SA', imf: 'SAU', wto: '682' },
  { code: 'TUR', name: 'Turkey', bis: 'TR', imf: 'TUR', oecd: 'TUR', wto: '792' },
  { code: 'CHE', name: 'Switzerland', bis: 'CH', imf: 'CHE', oecd: 'CHE', wto: '757' },
  { code: 'POL', name: 'Poland', bis: 'PL', imf: 'POL', oecd: 'POL', wto: '616' },
  { code: 'SWE', name: 'Sweden', bis: 'SE', imf: 'SWE', oecd: 'SWE', wto: '752' },
  { code: 'BEL', name: 'Belgium', bis: 'BE', imf: 'BEL', oecd: 'BEL', wto: '056' },
  { code: 'ARG', name: 'Argentina', bis: 'AR', imf: 'ARG', oecd: 'ARG', wto: '032' },
  { code: 'NOR', name: 'Norway', bis: 'NO', imf: 'NOR', oecd: 'NOR', wto: '579' },
  { code: 'AUT', name: 'Austria', bis: 'AT', imf: 'AUT', oecd: 'AUT', wto: '040' },
  { code: 'ARE', name: 'United Arab Emirates', bis: 'AE', imf: 'ARE', wto: '784' },
  { code: 'NGA', name: 'Nigeria', bis: 'NG', imf: 'NGA', wto: '566' },
  { code: 'ISR', name: 'Israel', bis: 'IL', imf: 'ISR', oecd: 'ISR', wto: '376' },
  { code: 'ZAF', name: 'South Africa', bis: 'ZA', imf: 'ZAF', oecd: 'ZAF', wto: '710' },
  { code: 'SGP', name: 'Singapore', bis: 'SG', imf: 'SGP', wto: '702', adb: 'SGP' },
  { code: 'HKG', name: 'Hong Kong', bis: 'HK', imf: 'HKG', wto: '344', adb: 'HKG' },
  { code: 'DNK', name: 'Denmark', bis: 'DK', imf: 'DNK', oecd: 'DNK', wto: '208' },
  { code: 'MYS', name: 'Malaysia', bis: 'MY', imf: 'MYS', wto: '458', adb: 'MYS' },
  { code: 'PHL', name: 'Philippines', bis: 'PH', imf: 'PHL', wto: '608', adb: 'PHI' },
  { code: 'CHL', name: 'Chile', bis: 'CL', imf: 'CHL', oecd: 'CHL', wto: '152' },
  { code: 'FIN', name: 'Finland', bis: 'FI', imf: 'FIN', oecd: 'FIN', wto: '246' },
  { code: 'COL', name: 'Colombia', bis: 'CO', imf: 'COL', oecd: 'COL', wto: '170' },
  { code: 'CZE', name: 'Czech Republic', bis: 'CZ', imf: 'CZE', oecd: 'CZE', wto: '203' },
  { code: 'PRT', name: 'Portugal', bis: 'PT', imf: 'PRT', oecd: 'PRT', wto: '620' },
  { code: 'NZL', name: 'New Zealand', bis: 'NZ', imf: 'NZL', oecd: 'NZL', wto: '554', adb: 'NZL' },
  { code: 'GRC', name: 'Greece', bis: 'GR', imf: 'GRC', oecd: 'GRC', wto: '300' },
  { code: 'HUN', name: 'Hungary', bis: 'HU', imf: 'HUN', oecd: 'HUN', wto: '348' },
  { code: 'IRL', name: 'Ireland', bis: 'IE', imf: 'IRL', oecd: 'IRL', wto: '372' },
  { code: 'THA', name: 'Thailand', bis: 'TH', imf: 'THA', wto: '764', adb: 'THA' },
  { code: 'VNM', name: 'Vietnam', bis: 'VN', imf: 'VNM', wto: '704', adb: 'VNM' },
  { code: 'TWN', name: 'Taiwan', bis: 'TW', adb: 'TWN' },
  { code: 'PAK', name: 'Pakistan', bis: 'PK', imf: 'PAK', wto: '586', adb: 'PAK' },
  { code: 'BGD', name: 'Bangladesh', bis: 'BD', imf: 'BGD', wto: '050', adb: 'BGD' },
  { code: 'MMR', name: 'Myanmar', imf: 'MMR', adb: 'MYA' },
  { code: 'KHM', name: 'Cambodia', imf: 'KHM', adb: 'CAM' },
  { code: 'LAO', name: 'Lao PDR', imf: 'LAO', adb: 'LAO' },
  { code: 'MNG', name: 'Mongolia', imf: 'MNG', adb: 'MON' },
  { code: 'NPL', name: 'Nepal', imf: 'NPL', adb: 'NEP' },
  { code: 'LKA', name: 'Sri Lanka', imf: 'LKA', adb: 'SRI' },
];

// Get indicators for selected data source
export const getIndicatorsForSource = (source: DataSource): Indicator[] => {
  switch (source) {
    case 'worldbank': return WORLDBANK_INDICATORS;
    case 'bis': return BIS_INDICATORS;
    case 'imf': return IMF_INDICATORS;
    case 'fred': return FRED_INDICATORS;
    case 'oecd': return OECD_INDICATORS;
    case 'wto': return WTO_INDICATORS;
    case 'cftc': return CFTC_MARKETS;
    case 'eia': return EIA_INDICATORS;
    case 'adb': return ADB_INDICATORS;
    case 'fed': return FED_INDICATORS;
    case 'bls': return BLS_INDICATORS;
    case 'unesco': return UNESCO_INDICATORS;
    case 'fiscaldata': return FISCALDATA_INDICATORS;
    case 'bea': return BEA_INDICATORS;
    default: return WORLDBANK_INDICATORS;
  }
};

// Get default indicator for source
export const getDefaultIndicator = (source: DataSource): string => {
  switch (source) {
    case 'worldbank': return 'NY.GDP.MKTP.CD';
    case 'bis': return 'WS_CBPOL';
    case 'imf': return 'NGDP_RPCH';
    case 'fred': return 'GDP';
    case 'oecd': return 'GDP';
    case 'wto': return 'TP_A_0010';
    case 'cftc': return 'gold';
    case 'eia': return 'crude_petroleum_stocks';
    case 'adb': return 'NGDP_XDC';
    case 'fed': return 'federal_funds_rate';
    case 'bls': return 'CUSR0000SA0';
    case 'unesco': return 'GER.1';
    case 'fiscaldata': return 'total_public_debt';
    case 'bea': return 'gdp_growth';
    default: return 'NY.GDP.MKTP.CD';
  }
};
