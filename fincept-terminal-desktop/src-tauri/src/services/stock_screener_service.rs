// Stock Screener Service
// Advanced filtering system for equity screening based on fundamental metrics

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

// ============================================================================
// Core Types
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub enum FilterOperator {
    GreaterThan,
    LessThan,
    GreaterThanOrEqual,
    LessThanOrEqual,
    Equal,
    NotEqual,
    Between,
    InList,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub enum MetricType {
    // Valuation
    PeRatio,
    PbRatio,
    PsRatio,
    PcfRatio,
    EvToEbitda,
    PegRatio,

    // Profitability
    Roe,
    Roa,
    Roic,
    GrossMargin,
    OperatingMargin,
    NetMargin,

    // Growth
    RevenueGrowth,
    EarningsGrowth,
    FcfGrowth,

    // Leverage
    DebtToEquity,
    DebtToAssets,
    InterestCoverage,

    // Market Data
    MarketCap,
    Price,
    Volume,
    Beta,

    // Dividend
    DividendYield,
    PayoutRatio,
    DividendGrowth,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct FilterCondition {
    pub metric: MetricType,
    pub operator: FilterOperator,
    pub value: FilterValue,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", untagged)]
pub enum FilterValue {
    Single(f64),
    Range { min: f64, max: f64 },
    List(Vec<String>),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ScreenCriteria {
    pub name: String,
    pub description: String,
    pub conditions: Vec<FilterCondition>,
    pub sector_filter: Option<Vec<String>>,
    pub industry_filter: Option<Vec<String>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct StockData {
    pub symbol: String,
    pub name: String,
    pub sector: String,
    pub industry: String,
    pub metrics: HashMap<String, f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ScreenResult {
    pub total_stocks_screened: usize,
    pub matching_stocks: Vec<StockData>,
    pub criteria_used: ScreenCriteria,
}

// ============================================================================
// Stock Screener Service
// ============================================================================

pub struct StockScreenerService {
    stock_universe: Vec<StockData>,
}

impl StockScreenerService {
    pub fn new() -> Self {
        StockScreenerService {
            stock_universe: create_mock_stock_universe(),
        }
    }

    /// Create screener with custom stock universe (for real FMP data)
    pub fn with_stock_universe(stock_universe: Vec<StockData>) -> Self {
        StockScreenerService {
            stock_universe,
        }
    }

    /// Execute a screen with given criteria
    pub fn execute_screen(&self, criteria: ScreenCriteria) -> ScreenResult {
        let mut matching_stocks = Vec::new();

        for stock in &self.stock_universe {
            // Apply sector filter
            if let Some(ref sectors) = criteria.sector_filter {
                if !sectors.contains(&stock.sector) {
                    continue;
                }
            }

            // Apply industry filter
            if let Some(ref industries) = criteria.industry_filter {
                if !industries.contains(&stock.industry) {
                    continue;
                }
            }

            // Apply metric conditions
            if self.matches_conditions(stock, &criteria.conditions) {
                matching_stocks.push(stock.clone());
            }
        }

        ScreenResult {
            total_stocks_screened: self.stock_universe.len(),
            matching_stocks,
            criteria_used: criteria,
        }
    }

    /// Check if stock matches all conditions
    fn matches_conditions(&self, stock: &StockData, conditions: &[FilterCondition]) -> bool {
        for condition in conditions {
            if !self.matches_condition(stock, condition) {
                return false;
            }
        }
        true
    }

    /// Check if stock matches a single condition
    fn matches_condition(&self, stock: &StockData, condition: &FilterCondition) -> bool {
        let metric_key = self.metric_to_key(&condition.metric);
        let stock_value = match stock.metrics.get(&metric_key) {
            Some(&value) => value,
            None => return false, // Missing metric = no match
        };

        match (&condition.operator, &condition.value) {
            (FilterOperator::GreaterThan, FilterValue::Single(threshold)) => {
                stock_value > *threshold
            }
            (FilterOperator::LessThan, FilterValue::Single(threshold)) => {
                stock_value < *threshold
            }
            (FilterOperator::GreaterThanOrEqual, FilterValue::Single(threshold)) => {
                stock_value >= *threshold
            }
            (FilterOperator::LessThanOrEqual, FilterValue::Single(threshold)) => {
                stock_value <= *threshold
            }
            (FilterOperator::Equal, FilterValue::Single(threshold)) => {
                (stock_value - threshold).abs() < 0.01 // Float comparison tolerance
            }
            (FilterOperator::NotEqual, FilterValue::Single(threshold)) => {
                (stock_value - threshold).abs() >= 0.01
            }
            (FilterOperator::Between, FilterValue::Range { min, max }) => {
                stock_value >= *min && stock_value <= *max
            }
            _ => false, // Invalid combination
        }
    }

    /// Convert metric type to string key
    fn metric_to_key(&self, metric: &MetricType) -> String {
        match metric {
            MetricType::PeRatio => "peRatio".to_string(),
            MetricType::PbRatio => "pbRatio".to_string(),
            MetricType::PsRatio => "psRatio".to_string(),
            MetricType::PcfRatio => "pcfRatio".to_string(),
            MetricType::EvToEbitda => "evToEbitda".to_string(),
            MetricType::PegRatio => "pegRatio".to_string(),
            MetricType::Roe => "roe".to_string(),
            MetricType::Roa => "roa".to_string(),
            MetricType::Roic => "roic".to_string(),
            MetricType::GrossMargin => "grossMargin".to_string(),
            MetricType::OperatingMargin => "operatingMargin".to_string(),
            MetricType::NetMargin => "netMargin".to_string(),
            MetricType::RevenueGrowth => "revenueGrowth".to_string(),
            MetricType::EarningsGrowth => "earningsGrowth".to_string(),
            MetricType::FcfGrowth => "fcfGrowth".to_string(),
            MetricType::DebtToEquity => "debtToEquity".to_string(),
            MetricType::DebtToAssets => "debtToAssets".to_string(),
            MetricType::InterestCoverage => "interestCoverage".to_string(),
            MetricType::MarketCap => "marketCap".to_string(),
            MetricType::Price => "price".to_string(),
            MetricType::Volume => "volume".to_string(),
            MetricType::Beta => "beta".to_string(),
            MetricType::DividendYield => "dividendYield".to_string(),
            MetricType::PayoutRatio => "payoutRatio".to_string(),
            MetricType::DividendGrowth => "dividendGrowth".to_string(),
        }
    }

    /// Get preset screen: Value Stocks
    pub fn get_value_screen() -> ScreenCriteria {
        ScreenCriteria {
            name: "Value Stocks".to_string(),
            description: "Stocks trading below intrinsic value with strong fundamentals".to_string(),
            conditions: vec![
                FilterCondition {
                    metric: MetricType::PeRatio,
                    operator: FilterOperator::LessThan,
                    value: FilterValue::Single(15.0),
                },
                FilterCondition {
                    metric: MetricType::PbRatio,
                    operator: FilterOperator::LessThan,
                    value: FilterValue::Single(3.0),
                },
                FilterCondition {
                    metric: MetricType::DebtToEquity,
                    operator: FilterOperator::LessThan,
                    value: FilterValue::Single(0.8),
                },
                FilterCondition {
                    metric: MetricType::Roe,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(12.0),
                },
            ],
            sector_filter: None,
            industry_filter: None,
        }
    }

    /// Get preset screen: Growth Stocks
    pub fn get_growth_screen() -> ScreenCriteria {
        ScreenCriteria {
            name: "Growth Stocks".to_string(),
            description: "High-growth companies with strong revenue and earnings expansion".to_string(),
            conditions: vec![
                FilterCondition {
                    metric: MetricType::RevenueGrowth,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(20.0),
                },
                FilterCondition {
                    metric: MetricType::EarningsGrowth,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(15.0),
                },
                FilterCondition {
                    metric: MetricType::Roe,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(15.0),
                },
                FilterCondition {
                    metric: MetricType::GrossMargin,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(40.0),
                },
            ],
            sector_filter: None,
            industry_filter: None,
        }
    }

    /// Get preset screen: Dividend Aristocrats
    pub fn get_dividend_screen() -> ScreenCriteria {
        ScreenCriteria {
            name: "Dividend Aristocrats".to_string(),
            description: "Stable dividend payers with sustainable payouts".to_string(),
            conditions: vec![
                FilterCondition {
                    metric: MetricType::DividendYield,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(3.0),
                },
                FilterCondition {
                    metric: MetricType::PayoutRatio,
                    operator: FilterOperator::LessThan,
                    value: FilterValue::Single(70.0),
                },
                FilterCondition {
                    metric: MetricType::DividendGrowth,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(5.0),
                },
                FilterCondition {
                    metric: MetricType::DebtToEquity,
                    operator: FilterOperator::LessThan,
                    value: FilterValue::Single(1.0),
                },
            ],
            sector_filter: None,
            industry_filter: None,
        }
    }

    /// Get preset screen: Momentum Stocks
    pub fn get_momentum_screen() -> ScreenCriteria {
        ScreenCriteria {
            name: "Momentum Stocks".to_string(),
            description: "Stocks with strong price momentum and volume".to_string(),
            conditions: vec![
                FilterCondition {
                    metric: MetricType::RevenueGrowth,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(15.0),
                },
                FilterCondition {
                    metric: MetricType::Roe,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(18.0),
                },
                FilterCondition {
                    metric: MetricType::OperatingMargin,
                    operator: FilterOperator::GreaterThan,
                    value: FilterValue::Single(15.0),
                },
            ],
            sector_filter: Some(vec![
                "Information Technology".to_string(),
                "Communication Services".to_string(),
                "Consumer Discretionary".to_string(),
            ]),
            industry_filter: None,
        }
    }
}

// ============================================================================
// Mock Data (For Development)
// ============================================================================

fn create_mock_stock_universe() -> Vec<StockData> {
    vec![
        StockData {
            symbol: "AAPL".to_string(),
            name: "Apple Inc.".to_string(),
            sector: "Information Technology".to_string(),
            industry: "Consumer Electronics".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 28.5),
                ("pbRatio".to_string(), 45.2),
                ("psRatio".to_string(), 7.1),
                ("roe".to_string(), 147.4),
                ("roa".to_string(), 27.8),
                ("roic".to_string(), 54.2),
                ("grossMargin".to_string(), 43.3),
                ("operatingMargin".to_string(), 30.7),
                ("netMargin".to_string(), 26.4),
                ("revenueGrowth".to_string(), 7.8),
                ("earningsGrowth".to_string(), 13.4),
                ("debtToEquity".to_string(), 2.0),
                ("marketCap".to_string(), 2_800_000_000_000.0),
                ("dividendYield".to_string(), 0.5),
                ("payoutRatio".to_string(), 15.0),
            ]),
        },
        StockData {
            symbol: "MSFT".to_string(),
            name: "Microsoft Corporation".to_string(),
            sector: "Information Technology".to_string(),
            industry: "Software".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 32.4),
                ("pbRatio".to_string(), 12.8),
                ("psRatio".to_string(), 11.2),
                ("roe".to_string(), 39.7),
                ("roa".to_string(), 15.2),
                ("roic".to_string(), 24.5),
                ("grossMargin".to_string(), 68.4),
                ("operatingMargin".to_string(), 42.1),
                ("netMargin".to_string(), 34.8),
                ("revenueGrowth".to_string(), 16.2),
                ("earningsGrowth".to_string(), 22.1),
                ("debtToEquity".to_string(), 0.45),
                ("marketCap".to_string(), 2_900_000_000_000.0),
                ("dividendYield".to_string(), 0.8),
                ("payoutRatio".to_string(), 25.0),
            ]),
        },
        StockData {
            symbol: "JNJ".to_string(),
            name: "Johnson & Johnson".to_string(),
            sector: "Health Care".to_string(),
            industry: "Pharmaceuticals".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 14.2),
                ("pbRatio".to_string(), 5.1),
                ("psRatio".to_string(), 4.2),
                ("roe".to_string(), 23.4),
                ("roa".to_string(), 9.8),
                ("roic".to_string(), 15.7),
                ("grossMargin".to_string(), 66.8),
                ("operatingMargin".to_string(), 24.5),
                ("netMargin".to_string(), 18.9),
                ("revenueGrowth".to_string(), 6.3),
                ("earningsGrowth".to_string(), 8.1),
                ("debtToEquity".to_string(), 0.52),
                ("marketCap".to_string(), 380_000_000_000.0),
                ("dividendYield".to_string(), 3.1),
                ("payoutRatio".to_string(), 44.0),
                ("dividendGrowth".to_string(), 6.2),
            ]),
        },
        StockData {
            symbol: "KO".to_string(),
            name: "The Coca-Cola Company".to_string(),
            sector: "Consumer Staples".to_string(),
            industry: "Beverages".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 24.8),
                ("pbRatio".to_string(), 10.2),
                ("psRatio".to_string(), 6.1),
                ("roe".to_string(), 39.5),
                ("roa".to_string(), 8.7),
                ("roic".to_string(), 14.2),
                ("grossMargin".to_string(), 59.8),
                ("operatingMargin".to_string(), 27.6),
                ("netMargin".to_string(), 23.1),
                ("revenueGrowth".to_string(), 11.2),
                ("earningsGrowth".to_string(), 9.8),
                ("debtToEquity".to_string(), 1.78),
                ("marketCap".to_string(), 270_000_000_000.0),
                ("dividendYield".to_string(), 3.0),
                ("payoutRatio".to_string(), 73.0),
                ("dividendGrowth".to_string(), 3.5),
            ]),
        },
        StockData {
            symbol: "NVDA".to_string(),
            name: "NVIDIA Corporation".to_string(),
            sector: "Information Technology".to_string(),
            industry: "Semiconductors".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 65.3),
                ("pbRatio".to_string(), 52.8),
                ("psRatio".to_string(), 38.2),
                ("roe".to_string(), 85.2),
                ("roa".to_string(), 48.5),
                ("roic".to_string(), 62.7),
                ("grossMargin".to_string(), 72.4),
                ("operatingMargin".to_string(), 54.2),
                ("netMargin".to_string(), 48.9),
                ("revenueGrowth".to_string(), 126.0),
                ("earningsGrowth".to_string(), 168.0),
                ("debtToEquity".to_string(), 0.25),
                ("marketCap".to_string(), 2_200_000_000_000.0),
                ("dividendYield".to_string(), 0.03),
                ("payoutRatio".to_string(), 1.5),
            ]),
        },
        StockData {
            symbol: "PG".to_string(),
            name: "Procter & Gamble Co.".to_string(),
            sector: "Consumer Staples".to_string(),
            industry: "Household Products".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 25.1),
                ("pbRatio".to_string(), 7.8),
                ("psRatio".to_string(), 4.5),
                ("roe".to_string(), 31.2),
                ("roa".to_string(), 9.4),
                ("roic".to_string(), 16.8),
                ("grossMargin".to_string(), 50.2),
                ("operatingMargin".to_string(), 23.4),
                ("netMargin".to_string(), 17.6),
                ("revenueGrowth".to_string(), 5.8),
                ("earningsGrowth".to_string(), 6.4),
                ("debtToEquity".to_string(), 0.62),
                ("marketCap".to_string(), 390_000_000_000.0),
                ("dividendYield".to_string(), 2.4),
                ("payoutRatio".to_string(), 60.0),
                ("dividendGrowth".to_string(), 4.8),
            ]),
        },
        StockData {
            symbol: "V".to_string(),
            name: "Visa Inc.".to_string(),
            sector: "Information Technology".to_string(),
            industry: "Financial Services".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 31.2),
                ("pbRatio".to_string(), 13.5),
                ("psRatio".to_string(), 15.8),
                ("roe".to_string(), 44.7),
                ("roa".to_string(), 14.8),
                ("roic".to_string(), 22.5),
                ("grossMargin".to_string(), 98.5),
                ("operatingMargin".to_string(), 67.2),
                ("netMargin".to_string(), 51.4),
                ("revenueGrowth".to_string(), 11.5),
                ("earningsGrowth".to_string(), 13.2),
                ("debtToEquity".to_string(), 0.58),
                ("marketCap".to_string(), 530_000_000_000.0),
                ("dividendYield".to_string(), 0.7),
                ("payoutRatio".to_string(), 21.0),
                ("dividendGrowth".to_string(), 17.5),
            ]),
        },
        StockData {
            symbol: "XOM".to_string(),
            name: "Exxon Mobil Corporation".to_string(),
            sector: "Energy".to_string(),
            industry: "Oil & Gas".to_string(),
            metrics: HashMap::from([
                ("peRatio".to_string(), 9.8),
                ("pbRatio".to_string(), 1.9),
                ("psRatio".to_string(), 1.2),
                ("roe".to_string(), 19.5),
                ("roa".to_string(), 10.2),
                ("roic".to_string(), 14.8),
                ("grossMargin".to_string(), 34.2),
                ("operatingMargin".to_string(), 15.8),
                ("netMargin".to_string(), 10.5),
                ("revenueGrowth".to_string(), 8.2),
                ("earningsGrowth".to_string(), 25.4),
                ("debtToEquity".to_string(), 0.28),
                ("marketCap".to_string(), 420_000_000_000.0),
                ("dividendYield".to_string(), 3.4),
                ("payoutRatio".to_string(), 33.0),
                ("dividendGrowth".to_string(), 2.1),
            ]),
        },
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_value_screen() {
        let service = StockScreenerService::new();
        let criteria = StockScreenerService::get_value_screen();
        let result = service.execute_screen(criteria);

        // XOM and JNJ should match value criteria
        assert!(result.matching_stocks.len() > 0);
        assert!(result.matching_stocks.iter().any(|s| s.symbol == "XOM" || s.symbol == "JNJ"));
    }

    #[test]
    fn test_growth_screen() {
        let service = StockScreenerService::new();
        let criteria = StockScreenerService::get_growth_screen();
        let result = service.execute_screen(criteria);

        // NVDA should match growth criteria
        assert!(result.matching_stocks.len() > 0);
        assert!(result.matching_stocks.iter().any(|s| s.symbol == "NVDA"));
    }

    #[test]
    fn test_dividend_screen() {
        let service = StockScreenerService::new();
        let criteria = StockScreenerService::get_dividend_screen();
        let result = service.execute_screen(criteria);

        // JNJ, XOM should match dividend criteria
        assert!(result.matching_stocks.len() > 0);
    }

    #[test]
    fn test_custom_filter() {
        let service = StockScreenerService::new();
        let criteria = ScreenCriteria {
            name: "Custom Test".to_string(),
            description: "Test".to_string(),
            conditions: vec![
                FilterCondition {
                    metric: MetricType::PeRatio,
                    operator: FilterOperator::LessThan,
                    value: FilterValue::Single(20.0),
                },
            ],
            sector_filter: None,
            industry_filter: None,
        };
        let result = service.execute_screen(criteria);

        // Should find stocks with P/E < 20
        assert!(result.matching_stocks.len() > 0);
        for stock in &result.matching_stocks {
            let pe = stock.metrics.get("peRatio").unwrap();
            assert!(*pe < 20.0);
        }
    }
}
