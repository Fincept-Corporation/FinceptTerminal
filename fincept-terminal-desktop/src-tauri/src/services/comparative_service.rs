#![allow(dead_code)]
// Comparative Analysis Service
// Implements QoQ, YoY, MoM, TTM comparative calculations and percentile rankings

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComparativeMetrics {
    pub qoq_growth: Option<f64>,  // Quarter-over-Quarter
    pub yoy_growth: Option<f64>,  // Year-over-Year
    pub mom_growth: Option<f64>,  // Month-over-Month
    pub ttm_value: Option<f64>,   // Trailing Twelve Months
    pub cagr_3y: Option<f64>,     // 3-Year Compound Annual Growth Rate
    pub cagr_5y: Option<f64>,     // 5-Year Compound Annual Growth Rate
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ValuationMetrics {
    pub pe_ratio: Option<f64>,
    pub pb_ratio: Option<f64>,
    pub ps_ratio: Option<f64>,
    pub pcf_ratio: Option<f64>,
    pub ev_to_ebitda: Option<f64>,
    pub peg_ratio: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ProfitabilityMetrics {
    pub roe: Option<f64>,          // Return on Equity
    pub roa: Option<f64>,          // Return on Assets
    pub roic: Option<f64>,         // Return on Invested Capital
    pub gross_margin: Option<f64>,
    pub operating_margin: Option<f64>,
    pub net_margin: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct GrowthMetrics {
    pub revenue_growth: Option<f64>,
    pub earnings_growth: Option<f64>,
    pub fcf_growth: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LeverageMetrics {
    pub debt_to_equity: Option<f64>,
    pub debt_to_assets: Option<f64>,
    pub interest_coverage: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PercentileRanking {
    pub metric_name: String,
    pub value: f64,
    pub percentile: f64,      // 0.0 to 100.0
    pub z_score: f64,         // Standard deviations from mean
    pub peer_median: f64,
    pub peer_mean: f64,
    pub peer_std_dev: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ValuationStatus {
    pub status: String,  // "Undervalued", "Fairly Valued", "Overvalued"
    pub confidence: f64, // 0.0 to 1.0
    pub reasoning: Vec<String>,
}

pub struct ComparativeService;

impl ComparativeService {
    pub fn new() -> Self {
        ComparativeService
    }

    /// Calculate Quarter-over-Quarter growth
    pub fn calculate_qoq_growth(&self, current_quarter: f64, previous_quarter: f64) -> Option<f64> {
        if previous_quarter == 0.0 {
            return None;
        }
        Some(((current_quarter - previous_quarter) / previous_quarter) * 100.0)
    }

    /// Calculate Year-over-Year growth
    pub fn calculate_yoy_growth(&self, current_year: f64, previous_year: f64) -> Option<f64> {
        if previous_year == 0.0 {
            return None;
        }
        Some(((current_year - previous_year) / previous_year) * 100.0)
    }

    /// Calculate Month-over-Month growth
    pub fn calculate_mom_growth(&self, current_month: f64, previous_month: f64) -> Option<f64> {
        if previous_month == 0.0 {
            return None;
        }
        Some(((current_month - previous_month) / previous_month) * 100.0)
    }

    /// Calculate Compound Annual Growth Rate
    pub fn calculate_cagr(&self, beginning_value: f64, ending_value: f64, num_years: f64) -> Option<f64> {
        if beginning_value <= 0.0 || num_years <= 0.0 {
            return None;
        }
        Some((f64::powf(ending_value / beginning_value, 1.0 / num_years) - 1.0) * 100.0)
    }

    /// Calculate percentile ranking for a value within a peer group
    pub fn calculate_percentile(&self, value: f64, peer_values: &[f64]) -> f64 {
        if peer_values.is_empty() {
            return 50.0;
        }

        let count_below = peer_values.iter().filter(|&&v| v < value).count();
        (count_below as f64 / peer_values.len() as f64) * 100.0
    }

    /// Calculate z-score for a value within a peer group
    pub fn calculate_z_score(&self, value: f64, mean: f64, std_dev: f64) -> f64 {
        if std_dev == 0.0 {
            return 0.0;
        }
        (value - mean) / std_dev
    }

    /// Calculate mean (average) of values
    pub fn calculate_mean(&self, values: &[f64]) -> f64 {
        if values.is_empty() {
            return 0.0;
        }
        values.iter().sum::<f64>() / values.len() as f64
    }

    /// Calculate median of values
    pub fn calculate_median(&self, values: &[f64]) -> f64 {
        if values.is_empty() {
            return 0.0;
        }

        let mut sorted = values.to_vec();
        sorted.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));

        let mid = sorted.len() / 2;
        if sorted.len() % 2 == 0 {
            (sorted[mid - 1] + sorted[mid]) / 2.0
        } else {
            sorted[mid]
        }
    }

    /// Calculate standard deviation
    pub fn calculate_std_dev(&self, values: &[f64], mean: f64) -> f64 {
        if values.is_empty() {
            return 0.0;
        }

        let variance = values
            .iter()
            .map(|&v| {
                let diff = v - mean;
                diff * diff
            })
            .sum::<f64>()
            / values.len() as f64;

        variance.sqrt()
    }

    /// Generate comprehensive percentile ranking for a metric
    pub fn generate_percentile_ranking(
        &self,
        metric_name: &str,
        value: f64,
        peer_values: &[f64],
    ) -> PercentileRanking {
        let mean = self.calculate_mean(peer_values);
        let std_dev = self.calculate_std_dev(peer_values, mean);
        let percentile = self.calculate_percentile(value, peer_values);
        let z_score = self.calculate_z_score(value, mean, std_dev);
        let median = self.calculate_median(peer_values);

        PercentileRanking {
            metric_name: metric_name.to_string(),
            value,
            percentile,
            z_score,
            peer_median: median,
            peer_mean: mean,
            peer_std_dev: std_dev,
        }
    }

    /// Assess valuation status based on multiple metrics
    pub fn assess_valuation_status(
        &self,
        pe_percentile: f64,
        pb_percentile: f64,
        ps_percentile: f64,
    ) -> ValuationStatus {
        let mut score = 0.0;
        let mut reasoning = Vec::new();

        // Lower percentiles indicate cheaper valuation (better value)
        if pe_percentile < 33.0 {
            score += 1.0;
            reasoning.push(format!("P/E ratio in bottom third of peers ({}th percentile)", pe_percentile.round()));
        } else if pe_percentile > 66.0 {
            score -= 1.0;
            reasoning.push(format!("P/E ratio in top third of peers ({}th percentile)", pe_percentile.round()));
        }

        if pb_percentile < 33.0 {
            score += 1.0;
            reasoning.push(format!("P/B ratio in bottom third of peers ({}th percentile)", pb_percentile.round()));
        } else if pb_percentile > 66.0 {
            score -= 1.0;
            reasoning.push(format!("P/B ratio in top third of peers ({}th percentile)", pb_percentile.round()));
        }

        if ps_percentile < 33.0 {
            score += 1.0;
            reasoning.push(format!("P/S ratio in bottom third of peers ({}th percentile)", ps_percentile.round()));
        } else if ps_percentile > 66.0 {
            score -= 1.0;
            reasoning.push(format!("P/S ratio in top third of peers ({}th percentile)", ps_percentile.round()));
        }

        let (status, confidence) = if score >= 2.0 {
            ("Undervalued".to_string(), 0.8)
        } else if score >= 1.0 {
            ("Potentially Undervalued".to_string(), 0.6)
        } else if score <= -2.0 {
            ("Overvalued".to_string(), 0.8)
        } else if score <= -1.0 {
            ("Potentially Overvalued".to_string(), 0.6)
        } else {
            ("Fairly Valued".to_string(), 0.5)
        };

        ValuationStatus {
            status,
            confidence,
            reasoning,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_qoq_growth() {
        let service = ComparativeService::new();
        let growth = service.calculate_qoq_growth(110.0, 100.0);
        assert_eq!(growth, Some(10.0));
    }

    #[test]
    fn test_yoy_growth() {
        let service = ComparativeService::new();
        let growth = service.calculate_yoy_growth(120.0, 100.0);
        assert_eq!(growth, Some(20.0));
    }

    #[test]
    fn test_cagr() {
        let service = ComparativeService::new();
        let cagr = service.calculate_cagr(100.0, 121.0, 2.0).unwrap();
        assert!((cagr - 10.0).abs() < 0.1);
    }

    #[test]
    fn test_percentile() {
        let service = ComparativeService::new();
        let values = vec![10.0, 20.0, 30.0, 40.0, 50.0];
        let percentile = service.calculate_percentile(30.0, &values);
        assert_eq!(percentile, 40.0); // 2 out of 5 values are below 30
    }

    #[test]
    fn test_mean_median() {
        let service = ComparativeService::new();
        let values = vec![10.0, 20.0, 30.0, 40.0, 50.0];
        assert_eq!(service.calculate_mean(&values), 30.0);
        assert_eq!(service.calculate_median(&values), 30.0);
    }
}
