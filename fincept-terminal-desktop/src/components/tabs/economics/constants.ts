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

// Chart dimensions
export const CHART_WIDTH = 1200;
export const CHART_HEIGHT = 400;

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

// BIS indicators
export const BIS_INDICATORS: Indicator[] = [
  { id: 'CBPOL_D', name: 'Central Bank Policy Rates (Daily)', category: 'Rates' },
  { id: 'CBPOL_M', name: 'Central Bank Policy Rates (Monthly)', category: 'Rates' },
  { id: 'EER_B', name: 'Effective Exchange Rates (Broad)', category: 'Exchange Rates' },
  { id: 'EER_N', name: 'Effective Exchange Rates (Narrow)', category: 'Exchange Rates' },
  { id: 'REER_B', name: 'Real Effective Exchange Rates (Broad)', category: 'Exchange Rates' },
  { id: 'SPP', name: 'Residential Property Prices', category: 'Property' },
  { id: 'CPP', name: 'Commercial Property Prices', category: 'Property' },
  { id: 'CREDIT', name: 'Credit to Non-financial Sector', category: 'Credit' },
  { id: 'DEBT', name: 'Debt Service Ratios', category: 'Credit' },
  { id: 'LBS', name: 'Locational Banking Statistics', category: 'Banking' },
  { id: 'CBS', name: 'Consolidated Banking Statistics', category: 'Banking' },
  { id: 'DSR', name: 'Debt Service Ratios', category: 'Debt' },
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
    default: return WORLDBANK_INDICATORS;
  }
};

// Get default indicator for source
export const getDefaultIndicator = (source: DataSource): string => {
  switch (source) {
    case 'worldbank': return 'NY.GDP.MKTP.CD';
    case 'bis': return 'CBPOL_M';
    case 'imf': return 'NGDP_RPCH';
    case 'fred': return 'GDP';
    case 'oecd': return 'GDP';
    case 'wto': return 'TP_A_0010';
    case 'cftc': return 'gold';
    case 'eia': return 'crude_petroleum_stocks';
    case 'adb': return 'NGDP_XDC';
    case 'fed': return 'federal_funds_rate';
    default: return 'NY.GDP.MKTP.CD';
  }
};
