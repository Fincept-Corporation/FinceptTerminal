/**
 * Interactive Brokers (IBKR) Types
 *
 * Type definitions for IBKR Client Portal Web API responses and requests
 */

// ============================================================================
// IBKR ACCOUNT TYPES
// ============================================================================

export interface IBKRAccount {
  id: string;
  accountId: string;
  accountVan: string;
  accountTitle: string;
  displayName: string;
  accountAlias: string | null;
  accountStatus: number;
  currency: string;
  type: string;
  tradingType: string;
  faclient: boolean;
  clearingStatus: string;
  covestor: boolean;
  parent: {
    mmc: string[];
    accountId: string;
    isMParent: boolean;
    isMChild: boolean;
    isMultiplex: boolean;
  };
  desc: string;
}

export interface IBKRAccountSummary {
  accountready: boolean;
  accounttype: string;
  accruedcash: number;
  accruedcash_c: number;
  accruedcash_f: number;
  accruedcash_s: number;
  accrueddividend: number;
  accrueddividend_c: number;
  accrueddividend_f: number;
  accrueddividend_s: number;
  availablefunds: number;
  availablefunds_c: number;
  availablefunds_f: number;
  availablefunds_s: number;
  billable: number;
  billable_c: number;
  billable_f: number;
  billable_s: number;
  buyingpower: number;
  cushion: number;
  daytradesremaining: number;
  daytradesremainingt1: number;
  daytradesremainingt2: number;
  daytradesremainingt3: number;
  daytradesremainingt4: number;
  equitywithloanvalue: number;
  equitywithloanvalue_c: number;
  equitywithloanvalue_f: number;
  equitywithloanvalue_s: number;
  excessliquidity: number;
  excessliquidity_c: number;
  excessliquidity_f: number;
  excessliquidity_s: number;
  fullavailablefunds: number;
  fullavailablefunds_c: number;
  fullavailablefunds_f: number;
  fullavailablefunds_s: number;
  fullexcessliquidity: number;
  fullexcessliquidity_c: number;
  fullexcessliquidity_f: number;
  fullexcessliquidity_s: number;
  fullinitmarginreq: number;
  fullinitmarginreq_c: number;
  fullinitmarginreq_f: number;
  fullinitmarginreq_s: number;
  fullmaintmarginreq: number;
  fullmaintmarginreq_c: number;
  fullmaintmarginreq_f: number;
  fullmaintmarginreq_s: number;
  grosspositionvalue: number;
  grosspositionvalue_s: number;
  guarantee: number;
  guarantee_c: number;
  guarantee_f: number;
  guarantee_s: number;
  highestseverity: string;
  indianstockhaircut: number;
  indianstockhaircut_c: number;
  indianstockhaircut_f: number;
  indianstockhaircut_s: number;
  initmarginreq: number;
  initmarginreq_c: number;
  initmarginreq_f: number;
  initmarginreq_s: number;
  leverage_s: number;
  lookaheadavailablefunds: number;
  lookaheadavailablefunds_c: number;
  lookaheadavailablefunds_f: number;
  lookaheadavailablefunds_s: number;
  lookaheadexcessliquidity: number;
  lookaheadexcessliquidity_c: number;
  lookaheadexcessliquidity_f: number;
  lookaheadexcessliquidity_s: number;
  lookaheadinitmarginreq: number;
  lookaheadinitmarginreq_c: number;
  lookaheadinitmarginreq_f: number;
  lookaheadinitmarginreq_s: number;
  lookaheadmaintmarginreq: number;
  lookaheadmaintmarginreq_c: number;
  lookaheadmaintmarginreq_f: number;
  lookaheadmaintmarginreq_s: number;
  maintmarginreq: number;
  maintmarginreq_c: number;
  maintmarginreq_f: number;
  maintmarginreq_s: number;
  netliquidation: number;
  netliquidation_c: number;
  netliquidation_f: number;
  netliquidation_s: number;
  netliquidationuncertainty: number;
  nlvandmargininreview: boolean;
  pasharesvalue: number;
  pasharesvalue_c: number;
  pasharesvalue_f: number;
  pasharesvalue_s: number;
  physicalcertificatevalue: number;
  physicalcertificatevalue_c: number;
  physicalcertificatevalue_f: number;
  physicalcertificatevalue_s: number;
  postexpirationexcess: number;
  postexpirationexcess_c: number;
  postexpirationexcess_f: number;
  postexpirationexcess_s: number;
  postexpirationmargin: number;
  postexpirationmargin_c: number;
  postexpirationmargin_f: number;
  postexpirationmargin_s: number;
  previousdayequitywithloanvalue: number;
  previousdayequitywithloanvalue_s: number;
  regtequity: number;
  regtequity_s: number;
  regtmargin: number;
  regtmargin_s: number;
  segmenttitle_c: string;
  segmenttitle_f: string;
  segmenttitle_s: string;
  sma: number;
  sma_s: number;
  totalcashvalue: number;
  totalcashvalue_c: number;
  totalcashvalue_f: number;
  totalcashvalue_s: number;
  totaldebitcardpendingcharges: number;
  totaldebitcardpendingcharges_c: number;
  totaldebitcardpendingcharges_f: number;
  totaldebitcardpendingcharges_s: number;
  tradingtype_f: string;
  tradingtype_s: string;
}

export interface IBKRLedger {
  [currency: string]: {
    commoditymarketvalue: number;
    futuremarketvalue: number;
    settledcash: number;
    exchangerate: number;
    sessionid: number;
    cashbalance: number;
    corporatebondsmarketvalue: number;
    warrantsmarketvalue: number;
    netliquidationvalue: number;
    interest: number;
    unrealizedpnl: number;
    stockmarketvalue: number;
    moneyfunds: number;
    currency: string;
    realizedpnl: number;
    funds: number;
    acctcode: string;
    issueroptionsmarketvalue: number;
    key: string;
    timestamp: number;
    severity: number;
  };
}

// ============================================================================
// IBKR ORDER TYPES
// ============================================================================

export interface IBKROrder {
  acct: string;
  conidex: string;
  conid: number;
  orderId: number;
  cashCcy: string;
  sizeAndFills: string;
  orderDesc: string;
  description1: string;
  description2?: string;
  ticker: string;
  secType: string;
  listingExchange: string;
  remainingQuantity: number;
  filledQuantity: number;
  totalSize: number;
  companyName: string;
  status: IBKROrderStatus;
  order_ccp_status: string;
  origOrderType: string;
  supportsTaxOpt: string;
  lastExecutionTime: string;
  orderType: IBKROrderType;
  bgColor: string;
  fgColor: string;
  isEventTrading: string;
  price?: number;
  timeInForce: IBKRTimeInForce;
  lastExecutionTime_r: number;
  side: IBKRSide;
  avgPrice?: number;
  order_ref?: string;
}

export type IBKROrderType =
  | 'LMT'      // Limit
  | 'MKT'      // Market
  | 'STP'      // Stop
  | 'STOP_LIMIT' // Stop Limit
  | 'MIDPRICE' // Midprice
  | 'TRAIL'    // Trailing Stop
  | 'TRAILLMT' // Trailing Stop Limit
  | 'MOC'      // Market on Close
  | 'LOC'      // Limit on Close
  | 'MIT'      // Market if Touched
  | 'LIT';     // Limit if Touched

export type IBKRSide = 'BUY' | 'SELL';

export type IBKRTimeInForce =
  | 'GTC'  // Good Till Cancel
  | 'OPG'  // Opening
  | 'DAY'  // Day
  | 'IOC'  // Immediate or Cancel
  | 'GTD'  // Good Till Date
  | 'PAX'  // Crypto
  | 'OVT'  // Overnight
  | 'OND'; // Overnight + DAY

export type IBKROrderStatus =
  | 'PendingSubmit'
  | 'PendingCancel'
  | 'PreSubmitted'
  | 'Submitted'
  | 'ApiCancelled'
  | 'Cancelled'
  | 'Filled'
  | 'Inactive'
  | 'WarnState'
  | 'SortByTime';

// ============================================================================
// IBKR POSITION TYPES
// ============================================================================

export interface IBKRPosition {
  acctId: string;
  conid: number;
  contractDesc: string;
  position: number;
  mktPrice: number;
  mktValue: number;
  currency: string;
  avgCost: number;
  avgPrice: number;
  realizedPnl: number;
  unrealizedPnl: number;
  exchs: string | null;
  expiry: string | null;
  putOrCall: string | null;
  multiplier: number | null;
  strike: number;
  exerciseStyle: string | null;
  conExchMap: string[];
  assetClass: string;
  undConid: number;
  model: string;
  time: number;
  chineseName: string | null;
  allExchanges: string;
  listingExchange: string;
  countryCode: string;
  name: string;
  lastTradingDay: string | null;
  group: string | null;
  sector: string | null;
  sectorGroup: string | null;
  ticker: string;
  type: string;
  undComp: string;
  undSym: string;
  fullName: string;
  pageSize: number;
  isEventContract: boolean;
}

// ============================================================================
// IBKR CONTRACT/ASSET TYPES
// ============================================================================

export interface IBKRContract {
  conid: number;
  symbol: string;
  secType: string;
  listingExchange: string;
  exchange: string;
  companyName: string;
  companyHeader: string;
  currency: string;
  validExchanges: string;
  name: string;
  description?: string;
  assetClass?: string;
  expiry?: string;
  lastTradingDay?: string;
  group?: string;
  putOrCall?: string;
  sector?: string;
  sectorGroup?: string;
  strike?: number;
  ticker?: string;
  undConid?: number;
  multiplier?: number;
  type?: string;
  hasOptions?: boolean;
  fullName?: string;
  isUS?: boolean;
  incrementRules?: IBKRIncrementRule[];
}

export interface IBKRIncrementRule {
  lowerEdge: number;
  increment: number;
}

export interface IBKRSecDefSearch {
  conid: number;
  companyHeader: string;
  companyName: string;
  symbol: string;
  description: string;
  restricted: string | null;
  fop: string | null;
  opt: string | null;
  war: string | null;
  sections: IBKRSecDefSection[];
}

export interface IBKRSecDefSection {
  secType: string;
  months?: string;
  symbol?: string;
  exchange?: string;
  legSecType?: string | null;
}

// ============================================================================
// IBKR MARKET DATA TYPES
// ============================================================================

export interface IBKRMarketDataSnapshot {
  [conid: string]: {
    '31'?: string;  // Last Price
    '55'?: string;  // Symbol
    '58'?: string;  // Text
    '70'?: string;  // High
    '71'?: string;  // Low
    '73'?: string;  // Market Value
    '74'?: string;  // Avg Price
    '75'?: string;  // Unrealized PnL
    '76'?: string;  // Formatted Position
    '77'?: string;  // Formatted Unrealized PnL
    '78'?: string;  // Daily PnL
    '79'?: string;  // Realized PnL
    '80'?: string;  // Unrealized PnL %
    '82'?: string;  // Change
    '83'?: string;  // Change %
    '84'?: string;  // Bid Price
    '85'?: string;  // Ask Size
    '86'?: string;  // Ask Price
    '87'?: string;  // Volume
    '88'?: string;  // Bid Size
    '6004'?: string; // Exchange
    '6008'?: string; // Conid
    '6070'?: string; // SecType
    '6072'?: string; // Months
    '6073'?: string; // Regular Expiry
    '6119'?: string; // Marker for market data delivery method
    '6457'?: string; // Underlying Conid
    '6509'?: string; // Market Data Availability
    '7051'?: string; // Company Name
    '7057'?: string; // Last Size
    '7058'?: string; // Conid + Exchange
    '7059'?: string; // Last Size
    '7068'?: string; // Dividends
    '7084'?: string; // Bid Exch
    '7085'?: string; // Ask Exch
    '7086'?: string; // Auction details
    '7087'?: string; // Auction
    '7088'?: string; // Mark Price
    '7089'?: string; // Shortable Shares
    '7094'?: string; // News
    '7184'?: string; // Dividends TTM
    '7219'?: string; // Conid
    '7220'?: string; // Listing Exchange
    '7221'?: string; // Industry
    '7280'?: string; // Bond Factor Multiplier
    '7282'?: string; // Dividends Per Share
    '7284'?: string; // EPS
    '7286'?: string; // Time
    '7287'?: string; // Open
    '7288'?: string; // Close
    '7289'?: string; // Volatility
    '7291'?: string; // Hi/Lo 13Wks
    '7292'?: string; // Hi/Lo 26Wks
    '7293'?: string; // Hi/Lo 52Wks
    '7294'?: string; // Open Price
    '7295'?: string; // Shortable
    '7296'?: string; // Halted
    '7308'?: string; // ESG
    '7309'?: string; // 52 Week High
    '7310'?: string; // 52 Week Low
    '7311'?: string; // Avg Volume
    '7607'?: string; // Option Implied Vol %
    '7633'?: string; // Put/Call Interest
    '7635'?: string; // Put/Call Volume
    '7636'?: string; // Hist Vol %
    '7637'?: string; // Hist Vol Close %
    '7638'?: string; // Opt Volume
    '7639'?: string; // Conid + Exchange
    '7644'?: string; // Server ID
    '7674'?: string; // EMA(200)
    '7675'?: string; // EMA(100)
    '7676'?: string; // EMA(50)
    '7677'?: string; // EMA(20)
    '7678'?: string; // Price/EMA(200)
    '7679'?: string; // Price/EMA(100)
    '7680'?: string; // Price/EMA(50)
    '7681'?: string; // Price/EMA(20)
    '7682'?: string; // Change Since Open
    '7698'?: string; // Market Cap
    '7699'?: string; // P/E
    '7700'?: string; // EPS
    '7702'?: string; // Overnight Volume
    '7703'?: string; // Change Since Open %
    '7704'?: string; // Upcoming Event
    '7707'?: string; // Imbalance
    '7714'?: string; // Shortable Inventory
    '7718'?: string; // Trade Count
    '7720'?: string; // Beta
    '7724'?: string; // Price to EMA
    '7741'?: string; // 26 Week High
    '7742'?: string; // 26 Week Low
    '7743'?: string; // 13 Week High
    '7744'?: string; // 13 Week Low
    '7745'?: string; // IPO Price
    '7762'?: string; // Prior Close
    conid?: number;
    server_id?: string;
    _updated?: number;
    minTick?: number;
  };
}

export interface IBKRHistoricalBar {
  o: number;  // open
  c: number;  // close
  h: number;  // high
  l: number;  // low
  v: number;  // volume
  t: number;  // timestamp (epoch ms)
}

export interface IBKRHistoricalData {
  symbol: string;
  text: string;
  priceFactor: number;
  startTime: string;
  high: string;
  low: string;
  timePeriod: string;
  barLength: number;
  mdAvailability: string;
  mktDataDelay: number;
  outsideRth: boolean;
  tradingDayDuration: number;
  volumeFactor: number;
  priceDisplayRule: number;
  priceDisplayValue: string;
  chartPanStartTime: string;
  direction: number;
  negativeCapable: boolean;
  messageVersion: number;
  data: IBKRHistoricalBar[];
  points: number;
  travelTime: number;
}

// ============================================================================
// IBKR WATCHLIST TYPES
// ============================================================================

export interface IBKRWatchlist {
  id: string;
  hash: string;
  name: string;
  readOnly: boolean;
  instruments: IBKRWatchlistInstrument[];
}

export interface IBKRWatchlistInstrument {
  C?: number;  // conid
  ST?: string; // secType
  name?: string;
}

// ============================================================================
// IBKR ORDER REQUEST TYPES
// ============================================================================

export interface IBKROrderRequest {
  acctId: string;
  conid: number;
  conidex?: string;
  secType?: string;
  cOID?: string;       // Client Order ID
  parentId?: string;   // For bracket orders
  orderType: IBKROrderType;
  listingExchange?: string;
  isSingleGroup?: boolean;
  outsideRTH?: boolean;
  price?: number;
  auxPrice?: number;   // For stop orders
  side: IBKRSide;
  ticker?: string;
  tif: IBKRTimeInForce;
  referrer?: string;
  quantity: number;
  fxQty?: number;
  useAdaptive?: boolean;
  isCcyConv?: boolean;
  allocationMethod?: string;
  strategy?: string;
  strategyParameters?: Record<string, string>;
}

export interface IBKROrderReply {
  id: string;
  message: string[];
  isSuppressed?: boolean;
  messageIds?: string[];
}

export interface IBKROrderConfirmation {
  order_id: string;
  order_status: string;
  encrypt_message?: string;
}

// ============================================================================
// IBKR AUTH/SESSION TYPES
// ============================================================================

export interface IBKRAuthStatus {
  authenticated: boolean;
  competing: boolean;
  connected: boolean;
  message: string;
  MAC: string;
  serverInfo: {
    serverName: string;
    serverVersion: string;
  };
  fail?: string;
}

export interface IBKRTickle {
  session: string;
  ssoExpires: number;
  collission: boolean;
  userId: number;
  hmds: {
    error: string;
  };
  iserver: {
    authStatus: {
      authenticated: boolean;
      competing: boolean;
      connected: boolean;
      message: string;
      MAC: string;
      serverInfo: {
        serverName: string;
        serverVersion: string;
      };
    };
  };
}

export interface IBKRClock {
  is_open: boolean;
  timestamp: number;
  next_open: number;
  next_close: number;
}

// ============================================================================
// IBKR PORTFOLIO HISTORY TYPES
// ============================================================================

export interface IBKRPortfolioHistory {
  id: string;
  nav: number[];
  cps: number[];
  twr: number[];
  timestamp: number[];
  freq: string;
  start: string;
  end: string;
}

export interface IBKRPerformance {
  currencyType: string;
  rc: number;
  nav: {
    data: Array<{
      idType: string;
      id: string;
      baseCurrency: string;
      start: string;
      end: string;
      returns: number[];
      total: number;
      dates: string[];
    }>;
  };
  cps: {
    data: Array<{
      idType: string;
      id: string;
      baseCurrency: string;
      start: string;
      end: string;
      returns: number[];
      total: number;
      dates: string[];
    }>;
  };
  tpps: {
    data: Array<{
      idType: string;
      id: string;
      baseCurrency: string;
      start: string;
      end: string;
      returns: number[];
      total: number;
      dates: string[];
    }>;
  };
}

// ============================================================================
// IBKR ACCOUNT ACTIVITY TYPES
// ============================================================================

export interface IBKRAccountActivity {
  id: string;
  currency: string;
  fxRateToBase: number;
  assetCategory: string;
  symbol: string;
  description: string;
  conid: number;
  securityID: string;
  cusip: string;
  isin: string;
  listingExchange: string;
  underlyingConid: number;
  underlyingSymbol: string;
  underlyingSecurityID: string;
  underlyingListingExchange: string;
  issuer: string;
  multiplier: number;
  strike: number;
  expiry: string;
  putCall: string;
  principalAdjustFactor: string;
  reportDate: string;
  date: string;
  settleDate: string;
  tradeID: string;
  transactionID: string;
  ibOrderID: string;
  ibExecID: string;
  brokerageOrderID: string;
  orderReference: string;
  volatilityOrderLink: string;
  clearingFirmID: string;
  origTradePrice: number;
  origTradeDate: string;
  origTradeID: string;
  origOrderID: string;
  clearingRule: string;
  taxes: number;
  ibCommission: number;
  ibCommissionCurrency: string;
  netCash: number;
  closePrice: number;
  openCloseIndicator: string;
  notes: string;
  cost: number;
  fifoPnlRealized: number;
  fxPnl: number;
  mtmPnl: number;
  origTransactionID: string;
  buySell: string;
  ibkrAccountId: string;
  acctAlias: string;
  model: string;
  levelOfDetail: string;
  transactionType: string;
  quantity: number;
  tradePrice: number;
  tradeMoney: number;
  proceeds: number;
  ibGrossCommission: number;
  ibGrossCommissionCurrency: string;
  orderTime: string;
  serialNumber: string;
  deliveryType: string;
  commodityType: string;
  fineness: number;
  weight: string;
}

// ============================================================================
// IBKR WEBSOCKET TYPES
// ============================================================================

export interface IBKRWSMessage {
  topic: string;
  args?: Record<string, unknown>;
}

export interface IBKRWSMarketData {
  topic: string;
  conid: number;
  '31'?: string;  // Last
  '84'?: string;  // Bid
  '86'?: string;  // Ask
  '88'?: string;  // Bid Size
  '85'?: string;  // Ask Size
  '87'?: string;  // Volume
  _updated?: number;
}

export interface IBKRWSOrderUpdate {
  topic: string;
  args: {
    acct: string;
    conid: number;
    orderId: number;
    status: IBKROrderStatus;
    side: IBKRSide;
    filledQuantity: number;
    remainingQuantity: number;
    avgPrice?: number;
  };
}

export interface IBKRWSTradeUpdate {
  topic: string;
  args: {
    acct: string;
    conid: number;
    orderId: number;
    execId: string;
    symbol: string;
    side: IBKRSide;
    shares: number;
    price: number;
    time: string;
  };
}

// ============================================================================
// IBKR SCANNER TYPES
// ============================================================================

export interface IBKRScannerParams {
  instrument_list: Array<{
    display_name: string;
    type: string;
    filters: string[];
  }>;
  scan_type_list: Array<{
    display_name: string;
    code: string;
    instruments: string[];
  }>;
  filter_list: Array<{
    group: string;
    display_name: string;
    code: string;
    type: string;
  }>;
  location_tree: Array<{
    display_name: string;
    type: string;
    locations: Array<{
      display_name: string;
      type: string;
    }>;
  }>;
}

export interface IBKRScannerResult {
  Contracts: {
    Contract: Array<{
      inScanTime: string;
      distance: number;
      con_id: number;
    }>;
  };
  offset: number;
  total: number;
  size: number;
}

// ============================================================================
// IBKR ALERT TYPES
// ============================================================================

export interface IBKRAlert {
  order_id: number;
  account: string;
  alert_name: string;
  alert_message: string;
  alert_active: number;
  alert_repeatable: number;
  alert_email: string;
  alert_send_message: number;
  alert_show_popup: number;
  alert_play_audio: string;
  alert_mta_currency: string;
  alert_mta_defaults: string;
  alert_default_type: string;
  tif: string;
  expire_time: string;
  outside_rth: number;
  itws_orders_only: number;
  tool_id: number;
  order_status: string;
  order_not_editable: boolean;
  conditions: IBKRAlertCondition[];
}

export interface IBKRAlertCondition {
  type: number;
  conidex: string;
  contract_description_1: string;
  operator: string;
  trigger_method: string;
  value: string;
  logic_bind: string;
  time_zone: string;
}
