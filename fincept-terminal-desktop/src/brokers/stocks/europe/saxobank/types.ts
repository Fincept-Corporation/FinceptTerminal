/**
 * Saxo Bank Types
 *
 * Type definitions for Saxo Bank OpenAPI responses and requests
 * Based on: https://www.developer.saxo/openapi/learn
 */

// ============================================================================
// SAXO BANK ENUMS AND CONSTANTS
// ============================================================================

export type SaxoAssetType =
  | 'Stock'
  | 'StockOption'
  | 'StockIndexOption'
  | 'Bond'
  | 'FxSpot'
  | 'FxForwards'
  | 'FxSwap'
  | 'FxVanillaOption'
  | 'CfdOnStock'
  | 'CfdOnIndex'
  | 'CfdOnFutures'
  | 'CfdOnEtc'
  | 'CfdOnEtn'
  | 'CfdOnEtf'
  | 'CfdOnFunds'
  | 'CfdOnRights'
  | 'ContractFutures'
  | 'FuturesOption'
  | 'Etc'
  | 'Etn'
  | 'Etf'
  | 'Fund'
  | 'Rights'
  | 'MutualFund'
  | 'CompanyWarrant'
  | 'StockIndex';

export type SaxoOrderType =
  | 'Market'
  | 'Limit'
  | 'Stop'
  | 'StopLimit'
  | 'TrailingStop'
  | 'TrailingStopLimit'
  | 'StopIfTraded'
  | 'StopIfBid'
  | 'StopIfAsked';

export type SaxoBuySell = 'Buy' | 'Sell';

export type SaxoOrderDuration =
  | 'AtTheClose'
  | 'AtTheOpening'
  | 'DayOrder'
  | 'FillOrKill'
  | 'GoodForPeriod'
  | 'GoodTillCancel'
  | 'GoodTillDate'
  | 'ImmediateOrCancel';

export type SaxoOrderStatus =
  | 'Working'
  | 'Filled'
  | 'PartiallyFilled'
  | 'Expired'
  | 'Cancelled'
  | 'Rejected'
  | 'Placed'
  | 'PendingFill'
  | 'PendingModify'
  | 'PendingCancel'
  | 'PendingDelete';

export type SaxoPositionStatus = 'Open' | 'Closed' | 'Closing' | 'PartiallyClosed';

export type SaxoMarketState =
  | 'PreOpen'
  | 'Open'
  | 'PostClose'
  | 'Closed'
  | 'PriceIndication';

// ============================================================================
// SAXO BANK ACCOUNT TYPES
// ============================================================================

export interface SaxoAccount {
  AccountId: string;
  AccountKey: string;
  AccountType: string;
  Active: boolean;
  CanUseCashPositionsAsMarginCollateral: boolean;
  ClientId: string;
  ClientKey: string;
  Currency: string;
  CurrencyDecimals: number;
  DisplayName: string;
  IsCurrencyConversionAtSettlementTime: boolean;
  IsMarginTradingAllowed: boolean;
  IsShareable: boolean;
  IsTrialAccount: boolean;
  LegalAssetTypes: SaxoAssetType[];
  ManagementType: string;
  Sharing: string[];
}

export interface SaxoAccountBalance {
  AccountId: string;
  CalculationReliability: string;
  CashAvailableForTrading: number;
  CashBalance: number;
  CashBlocked: number;
  ChangesScheduled: boolean;
  ClosedPositionsCount: number;
  CollateralAvailable: number;
  CollateralCreditValue: {
    Line: number;
    UtilizationPct: number;
  };
  CorporateActionUnrealizedAmounts: number;
  CostToClosePositions: number;
  Currency: string;
  CurrencyDecimals: number;
  InitialMargin: {
    CollateralAvailable: number;
    CollateralCreditValue: {
      Line: number;
      UtilizationPct: number;
    };
    MarginAvailable: number;
    MarginCollateralNotAvailable: number;
    MarginUsedByCurrentPositions: number;
    MarginUtilizationPct: number;
    NetEquityForMargin: number;
  };
  IsPortfolioMarginModelSimple: boolean;
  MarginAvailableForTrading: number;
  MarginCollateralNotAvailable: number;
  MarginExposureCoveragePct: number;
  MarginNetExposure: number;
  MarginUsedByCurrentPositions: number;
  MarginUtilizationPct: number;
  NetEquityForMargin: number;
  NetPositionsCount: number;
  NonMarginPositionsValue: number;
  OpenIpoOrdersCount: number;
  OpenPositionsCount: number;
  OptionPremiumsMarketValue: number;
  OrdersCount: number;
  OtherCollateral: number;
  PositionsNotMarginable: number;
  SpendingPower: number;
  SpendingPowerDetail: {
    Current: number;
  };
  TotalValue: number;
  TransactionsNotBooked: number;
  TriggerOrdersCount: number;
  UnrealizedMarginClosedProfitLoss: number;
  UnrealizedMarginOpenProfitLoss: number;
  UnrealizedMarginProfitLoss: number;
  UnrealizedPositionsValue: number;
}

export interface SaxoClient {
  ClientId: string;
  ClientKey: string;
  ClientType: string;
  CultureCode: string;
  Currency: string;
  CurrencyDecimals: number;
  DefaultAccountId: string;
  DefaultAccountKey: string;
  DefaultCurrency: string;
  ForceOpenDefaultValue: boolean;
  IsMarginTradingAllowed: boolean;
  IsVariationMarginEligible: boolean;
  LegalAssetTypes: SaxoAssetType[];
  LegalAssetTypesAreIndicative: boolean;
  MarginCalculationMethod: string;
  MarginMonitoringMode: string;
  Name: string;
  PartnerPlatformId: string;
  PositionNettingMethod: string;
  PositionNettingMode: string;
  PositionNettingProfile: string;
  ReduceExposureOnly: boolean;
  SupportsAccountValueProtectionLimit: boolean;
}

// ============================================================================
// SAXO BANK ORDER TYPES
// ============================================================================

export interface SaxoOrder {
  OrderId: string;
  AccountId: string;
  AccountKey: string;
  Amount: number;
  AssetType: SaxoAssetType;
  BuySell: SaxoBuySell;
  CalculationReliability: string;
  ClientId: string;
  ClientKey: string;
  ClientName: string;
  CorrelationKey: string;
  CurrentPrice: number;
  CurrentPriceDelayMinutes: number;
  CurrentPriceType: string;
  DisplayAndFormat: {
    Currency: string;
    Decimals: number;
    Description: string;
    Format: string;
    Symbol: string;
  };
  Duration: {
    DurationType: SaxoOrderDuration;
    ExpirationDateTime?: string;
  };
  Exchange: {
    CountryCode: string;
    ExchangeId: string;
    IsOpen: boolean;
    Name: string;
    TimeZone: string;
    TimeZoneAbbreviation: string;
  };
  FilledAmount: number;
  IsForceOpen: boolean;
  IsMarketOpen: boolean;
  MarketPrice: number;
  MarketState: SaxoMarketState;
  MarketValue: number;
  OpenOrderType: SaxoOrderType;
  OrderAmountType: string;
  OrderTime: string;
  Price?: number;
  RelatedOpenOrders?: SaxoRelatedOrder[];
  Status: SaxoOrderStatus;
  Uic: number;
  ValueDate?: string;
}

export interface SaxoRelatedOrder {
  Amount: number;
  Duration: {
    DurationType: SaxoOrderDuration;
  };
  OpenOrderType: SaxoOrderType;
  OrderId: string;
  OrderPrice: number;
  Status: SaxoOrderStatus;
}

export interface SaxoOrderRequest {
  AccountKey: string;
  Amount: number;
  AssetType: SaxoAssetType;
  BuySell: SaxoBuySell;
  ExternalReference?: string;
  ManualOrder?: boolean;
  OrderDuration: {
    DurationType: SaxoOrderDuration;
    ExpirationDateTime?: string;
  };
  OrderPrice?: number;
  OrderType: SaxoOrderType;
  Uic: number;
  StopLimitPrice?: number;
  TrailingStopDistanceToMarket?: number;
  TrailingStopStep?: number;
  Orders?: SaxoRelatedOrderRequest[];
}

export interface SaxoRelatedOrderRequest {
  Amount: number;
  AssetType: SaxoAssetType;
  BuySell: SaxoBuySell;
  OrderDuration: {
    DurationType: SaxoOrderDuration;
  };
  OrderPrice?: number;
  OrderType: SaxoOrderType;
  Uic: number;
}

export interface SaxoOrderResponse {
  OrderId: string;
  Orders?: {
    OrderId: string;
  }[];
}

export interface SaxoModifyOrderRequest {
  AccountKey: string;
  Amount?: number;
  AssetType: SaxoAssetType;
  OrderDuration?: {
    DurationType: SaxoOrderDuration;
    ExpirationDateTime?: string;
  };
  OrderPrice?: number;
  OrderType?: SaxoOrderType;
  StopLimitPrice?: number;
}

// ============================================================================
// SAXO BANK POSITION TYPES
// ============================================================================

export interface SaxoPosition {
  NetPositionId: string;
  PositionBase: {
    AccountId: string;
    AccountKey: string;
    Amount: number;
    AssetType: SaxoAssetType;
    CanBeClosed: boolean;
    ClientId: string;
    CloseConversionRateSettled: boolean;
    CorrelationKey: string;
    ExecutionTimeOpen: string;
    IsForceOpen: boolean;
    IsMarketOpen: boolean;
    LockedByBackOffice: boolean;
    OpenPrice: number;
    OpenPriceIncludingCosts: number;
    RelatedOpenOrders?: SaxoRelatedOrder[];
    SourceOrderId: string;
    SpotDate: string;
    Status: SaxoPositionStatus;
    Uic: number;
    ValueDate: string;
  };
  PositionId: string;
  PositionView: {
    Ask: number;
    Bid: number;
    CalculationReliability: string;
    ConversionRateCurrent: number;
    ConversionRateOpen: number;
    CurrentPrice: number;
    CurrentPriceDelayMinutes: number;
    CurrentPriceType: string;
    Exposure: number;
    ExposureCurrency: string;
    ExposureInBaseCurrency: number;
    InstrumentPriceDayPercentChange: number;
    MarketState: SaxoMarketState;
    MarketValue: number;
    MarketValueInBaseCurrency: number;
    ProfitLossOnTrade: number;
    ProfitLossOnTradeInBaseCurrency: number;
    TradeCostsTotal: number;
    TradeCostsTotalInBaseCurrency: number;
  };
  DisplayAndFormat: {
    Currency: string;
    Decimals: number;
    Description: string;
    Format: string;
    Symbol: string;
  };
  Exchange: {
    CountryCode: string;
    ExchangeId: string;
    IsOpen: boolean;
    Name: string;
  };
}

export interface SaxoNetPosition {
  NetPositionBase: {
    AccountId: string;
    Amount: number;
    AssetType: SaxoAssetType;
    AverageOpenPrice: number;
    AverageOpenPriceIncludingCosts: number;
    CanBeClosed: boolean;
    HasForceOpenPositions: boolean;
    IsMarketOpen: boolean;
    NumberOfRelatedOrders: number;
    OpenIpoOrdersCount: number;
    OpenOrdersCount: number;
    OpenTriggerOrdersCount: number;
    PositionsCount: number;
    SinglePositionId?: string;
    SinglePositionStatus?: SaxoPositionStatus;
    Status: SaxoPositionStatus;
    Uic: number;
    ValueDate: string;
  };
  NetPositionId: string;
  NetPositionView: {
    Ask: number;
    Bid: number;
    CalculationReliability: string;
    ConversionRateCurrent: number;
    CurrentPrice: number;
    CurrentPriceDelayMinutes: number;
    CurrentPriceType: string;
    Exposure: number;
    ExposureCurrency: string;
    ExposureInBaseCurrency: number;
    MarketState: SaxoMarketState;
    MarketValue: number;
    MarketValueInBaseCurrency: number;
    ProfitLossOnTrade: number;
    ProfitLossOnTradeInBaseCurrency: number;
    TradeCostsTotal: number;
    TradeCostsTotalInBaseCurrency: number;
  };
  DisplayAndFormat: {
    Currency: string;
    Decimals: number;
    Description: string;
    Format: string;
    Symbol: string;
  };
  Exchange: {
    CountryCode: string;
    ExchangeId: string;
    Name: string;
  };
}

// ============================================================================
// SAXO BANK INSTRUMENT TYPES
// ============================================================================

export interface SaxoInstrument {
  AssetType: SaxoAssetType;
  CurrencyCode: string;
  Description: string;
  ExchangeId: string;
  GroupId: number;
  Identifier: string;
  IssuerCountry?: string;
  PrimaryListing?: number;
  SummaryType: string;
  Symbol: string;
  TradableAs?: SaxoAssetType[];
  Uic: number;
}

export interface SaxoInstrumentDetails {
  AssetType: SaxoAssetType;
  CurrencyCode: string;
  DefaultAmount: number;
  DefaultSlippage: number;
  DefaultSlippageType: string;
  Description: string;
  Exchange: {
    CountryCode: string;
    ExchangeId: string;
    IsOpen: boolean;
    Name: string;
    TimeZone: string;
    TimeZoneAbbreviation: string;
    TimeZoneOffset: string;
  };
  Format: {
    Decimals: number;
    Format: string;
    OrderDecimals: number;
    PriceDecimals: number;
  };
  FxSpotDate?: string;
  GroupId: number;
  IncrementSize: number;
  IsTradable: boolean;
  LotSize: number;
  LotSizeType: string;
  MinimumLotSize: number;
  MinimumTradeSize: number;
  OrderDistances: {
    EntryDefaultDistance: number;
    EntryDefaultDistanceType: string;
    LimitDefaultDistance: number;
    LimitDefaultDistanceType: string;
    StopLimitDefaultDistance: number;
    StopLimitDefaultDistanceType: string;
    StopLossDefaultDistance: number;
    StopLossDefaultDistanceType: string;
    StopLossDefaultEnabled: boolean;
    StopLossDefaultOrderType: SaxoOrderType;
    TakeProfitDefaultDistance: number;
    TakeProfitDefaultDistanceType: string;
    TakeProfitDefaultEnabled: boolean;
  };
  PriceToContractFactor: number;
  PrimaryListing: number;
  RelatedInstruments?: {
    AssetType: SaxoAssetType;
    Uic: number;
  }[];
  StandardAmounts: number[];
  SupportedOrderTypes: SaxoOrderType[];
  SupportedOrderTypeSettings?: {
    DurationTypes: SaxoOrderDuration[];
    OrderType: SaxoOrderType;
  }[];
  Symbol: string;
  TickSize: number;
  TickSizeLimitOrder: number;
  TickSizeScheme?: {
    DefaultTickSize: number;
    Elements: {
      HighPrice: number;
      TickSize: number;
    }[];
  };
  TradableAs: SaxoAssetType[];
  TradableOn: string[];
  TradingSignals?: string;
  TradingStatus: string;
  Uic: number;
}

// ============================================================================
// SAXO BANK MARKET DATA TYPES
// ============================================================================

export interface SaxoQuote {
  AssetType: SaxoAssetType;
  LastUpdated: string;
  PriceSource: string;
  Quote: {
    Amount: number;
    Ask: number;
    Bid: number;
    DelayedByMinutes: number;
    ErrorCode?: string;
    High: number;
    Low: number;
    Mid: number;
    Open: number;
    PriceSource: string;
    PriceSourceType: string;
    PriceTypeAsk: string;
    PriceTypeBid: string;
    TradeTime?: string;
  };
  Uic: number;
  DisplayAndFormat?: {
    Currency: string;
    Decimals: number;
    Description: string;
    Symbol: string;
  };
}

export interface SaxoInfoPrice {
  AssetType: SaxoAssetType;
  DisplayAndFormat: {
    Currency: string;
    Decimals: number;
    Description: string;
    Format: string;
    Symbol: string;
  };
  InstrumentPriceDetails?: {
    AskSize: number;
    BidSize: number;
    LastClose: number;
    LastTraded: number;
    LastTradedSize: number;
    Open: number;
    Volume: number;
  };
  LastUpdated: string;
  PriceInfo?: string;
  Quote: {
    Amount: number;
    Ask: number;
    Bid: number;
    DelayedByMinutes: number;
    High: number;
    Low: number;
    Mid: number;
    Open: number;
    PriceSource: string;
    PriceSourceType: string;
    PriceTypeAsk: string;
    PriceTypeBid: string;
  };
  Uic: number;
}

export interface SaxoOHLC {
  CloseAsk: number;
  CloseBid: number;
  HighAsk: number;
  HighBid: number;
  LowAsk: number;
  LowBid: number;
  OpenAsk: number;
  OpenBid: number;
  Time: string;
  Volume?: number;
}

export interface SaxoChartData {
  ChartInfo: {
    DelayedByMinutes: number;
    ExchangeId: string;
    FirstSampleTime: string;
    Horizon: number;
  };
  Data: SaxoOHLC[];
  DisplayAndFormat: {
    Currency: string;
    Decimals: number;
    Description: string;
    Symbol: string;
  };
}

export type SaxoChartHorizon =
  | 1          // 1 minute
  | 5          // 5 minutes
  | 10         // 10 minutes
  | 15         // 15 minutes
  | 30         // 30 minutes
  | 60         // 1 hour
  | 120        // 2 hours
  | 240        // 4 hours
  | 360        // 6 hours
  | 480        // 8 hours
  | 1440       // 1 day
  | 10080      // 1 week
  | 43200;     // 1 month

// ============================================================================
// SAXO BANK TRADE TYPES
// ============================================================================

export interface SaxoTrade {
  TradeId: string;
  AccountId: string;
  AccountKey: string;
  Amount: number;
  AssetType: SaxoAssetType;
  BuySell: SaxoBuySell;
  ClientId: string;
  CorrelationKey: string;
  ExecutionTime: string;
  ExternalReference?: string;
  OrderId: string;
  Price: number;
  PositionId?: string;
  TradeTime: string;
  Uic: number;
  ValueDate: string;
  DisplayAndFormat: {
    Currency: string;
    Decimals: number;
    Description: string;
    Symbol: string;
  };
  Exchange: {
    CountryCode: string;
    ExchangeId: string;
    Name: string;
  };
}

// ============================================================================
// SAXO BANK WEBSOCKET TYPES
// ============================================================================

export interface SaxoWSSubscriptionRequest {
  ContextId: string;
  ReferenceId: string;
  Arguments: {
    AccountKey?: string;
    AssetType?: SaxoAssetType;
    ClientKey?: string;
    FieldGroups?: string[];
    Uic?: number;
    Uics?: number[];
  };
}

export interface SaxoWSMessage {
  ReferenceId: string;
  MessageId: number;
  Timestamp: string;
  Data?: unknown;
  Snapshot?: {
    Data: unknown[];
    __count?: number;
  };
  Delta?: {
    Data: unknown[];
    __count?: number;
  };
  Heartbeat?: boolean;
  ResetSubscription?: boolean;
}

export interface SaxoWSQuoteUpdate {
  AssetType: SaxoAssetType;
  LastUpdated: string;
  Quote: {
    Ask: number;
    Bid: number;
    High?: number;
    Low?: number;
    Mid?: number;
    Open?: number;
    PriceSource: string;
  };
  Uic: number;
}

export interface SaxoWSOrderUpdate {
  AccountId: string;
  OrderId: string;
  Status: SaxoOrderStatus;
  FilledAmount?: number;
  RemainingAmount?: number;
  FilledPrice?: number;
}

export interface SaxoWSPositionUpdate {
  PositionId: string;
  NetPositionId: string;
  AccountId: string;
  Amount: number;
  Status: SaxoPositionStatus;
  CurrentPrice?: number;
  ProfitLoss?: number;
}

// ============================================================================
// SAXO BANK OAUTH TYPES
// ============================================================================

export interface SaxoOAuthConfig {
  clientId: string;
  clientSecret: string;
  redirectUri: string;
  authEndpoint: string;
  tokenEndpoint: string;
}

export interface SaxoTokenResponse {
  access_token: string;
  token_type: string;
  expires_in: number;
  refresh_token: string;
  refresh_token_expires_in: number;
}

export interface SaxoAuthState {
  accessToken: string;
  refreshToken: string;
  expiresAt: number;
  refreshExpiresAt: number;
}

// ============================================================================
// SAXO BANK API RESPONSE WRAPPER
// ============================================================================

export interface SaxoApiResponse<T> {
  Data?: T[];
  __count?: number;
  __next?: string;
}

export interface SaxoApiError {
  ErrorCode: string;
  Message: string;
  ModelState?: Record<string, string[]>;
}
