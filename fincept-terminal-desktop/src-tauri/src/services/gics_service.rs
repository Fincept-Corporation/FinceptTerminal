// GICS (Global Industry Classification Standard) Service
// Implements CFA Institute-compliant sector classification and peer identification

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum GICSSector {
    Energy,
    Materials,
    Industrials,
    ConsumerDiscretionary,
    ConsumerStaples,
    HealthCare,
    Financials,
    InformationTechnology,
    CommunicationServices,
    Utilities,
    RealEstate,
}

impl GICSSector {
    pub fn from_string(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "energy" => Some(GICSSector::Energy),
            "materials" => Some(GICSSector::Materials),
            "industrials" => Some(GICSSector::Industrials),
            "consumer discretionary" | "consumerdiscretionary" => Some(GICSSector::ConsumerDiscretionary),
            "consumer staples" | "consumerstaples" => Some(GICSSector::ConsumerStaples),
            "health care" | "healthcare" => Some(GICSSector::HealthCare),
            "financials" => Some(GICSSector::Financials),
            "information technology" | "informationtechnology" | "technology" => Some(GICSSector::InformationTechnology),
            "communication services" | "communicationservices" => Some(GICSSector::CommunicationServices),
            "utilities" => Some(GICSSector::Utilities),
            "real estate" | "realestate" => Some(GICSSector::RealEstate),
            _ => None,
        }
    }

    pub fn to_string(&self) -> String {
        match self {
            GICSSector::Energy => "Energy".to_string(),
            GICSSector::Materials => "Materials".to_string(),
            GICSSector::Industrials => "Industrials".to_string(),
            GICSSector::ConsumerDiscretionary => "Consumer Discretionary".to_string(),
            GICSSector::ConsumerStaples => "Consumer Staples".to_string(),
            GICSSector::HealthCare => "Health Care".to_string(),
            GICSSector::Financials => "Financials".to_string(),
            GICSSector::InformationTechnology => "Information Technology".to_string(),
            GICSSector::CommunicationServices => "Communication Services".to_string(),
            GICSSector::Utilities => "Utilities".to_string(),
            GICSSector::RealEstate => "Real Estate".to_string(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum MarketCapCategory {
    Mega,    // > $200B
    Large,   // $10B - $200B
    Mid,     // $2B - $10B
    Small,   // $300M - $2B
    Micro,   // $50M - $300M
    Nano,    // < $50M
}

impl MarketCapCategory {
    pub fn from_market_cap(market_cap: f64) -> Self {
        if market_cap > 200_000_000_000.0 {
            MarketCapCategory::Mega
        } else if market_cap > 10_000_000_000.0 {
            MarketCapCategory::Large
        } else if market_cap > 2_000_000_000.0 {
            MarketCapCategory::Mid
        } else if market_cap > 300_000_000.0 {
            MarketCapCategory::Small
        } else if market_cap > 50_000_000.0 {
            MarketCapCategory::Micro
        } else {
            MarketCapCategory::Nano
        }
    }

    pub fn to_string(&self) -> String {
        match self {
            MarketCapCategory::Mega => "Mega Cap".to_string(),
            MarketCapCategory::Large => "Large Cap".to_string(),
            MarketCapCategory::Mid => "Mid Cap".to_string(),
            MarketCapCategory::Small => "Small Cap".to_string(),
            MarketCapCategory::Micro => "Micro Cap".to_string(),
            MarketCapCategory::Nano => "Nano Cap".to_string(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PeerCompany {
    pub symbol: String,
    pub name: String,
    pub sector: String,
    pub industry: String,
    pub market_cap: f64,
    pub similarity_score: f64,
    pub match_details: MatchDetails,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MatchDetails {
    pub sector_match: bool,
    pub industry_match: bool,
    pub market_cap_similarity: f64, // 0.0 to 1.0
    pub geographic_match: bool,
}

pub struct PeerIdentificationService;

impl PeerIdentificationService {
    pub fn new() -> Self {
        PeerIdentificationService
    }

    /// Calculate similarity score between two companies
    pub fn calculate_similarity(
        &self,
        target_sector: &str,
        target_industry: &str,
        target_market_cap: f64,
        peer_sector: &str,
        peer_industry: &str,
        peer_market_cap: f64,
    ) -> f64 {
        let mut score = 0.0;
        let max_score = 100.0;

        // Sector match (40 points)
        if target_sector.to_lowercase() == peer_sector.to_lowercase() {
            score += 40.0;
        }

        // Industry match (30 points)
        if target_industry.to_lowercase() == peer_industry.to_lowercase() {
            score += 30.0;
        }

        // Market cap similarity (30 points)
        let market_cap_ratio = if target_market_cap > peer_market_cap {
            peer_market_cap / target_market_cap
        } else {
            target_market_cap / peer_market_cap
        };
        score += 30.0 * market_cap_ratio;

        // Normalize to 0-1 scale
        score / max_score
    }

    /// Find peer companies based on sector, industry, and market cap
    pub fn find_peers(
        &self,
        target_symbol: &str,
        target_sector: &str,
        target_industry: &str,
        target_market_cap: f64,
        candidate_companies: Vec<HashMap<String, serde_json::Value>>,
        min_similarity: f64,
        max_peers: usize,
    ) -> Vec<PeerCompany> {
        let mut peers: Vec<PeerCompany> = candidate_companies
            .iter()
            .filter_map(|company| {
                let symbol = company.get("symbol")?.as_str()?.to_string();

                // Skip target company
                if symbol == target_symbol {
                    return None;
                }

                let name = company.get("name")?.as_str()?.to_string();
                let sector = company.get("sector")?.as_str()?.to_string();
                let industry = company.get("industry")?.as_str()?.to_string();
                let market_cap = company.get("marketCap")?.as_f64()?;

                let similarity_score = self.calculate_similarity(
                    target_sector,
                    target_industry,
                    target_market_cap,
                    &sector,
                    &industry,
                    market_cap,
                );

                if similarity_score < min_similarity {
                    return None;
                }

                let market_cap_ratio = if target_market_cap > market_cap {
                    market_cap / target_market_cap
                } else {
                    target_market_cap / market_cap
                };

                Some(PeerCompany {
                    symbol,
                    name,
                    sector: sector.clone(),
                    industry: industry.clone(),
                    market_cap,
                    similarity_score,
                    match_details: MatchDetails {
                        sector_match: target_sector.to_lowercase() == sector.to_lowercase(),
                        industry_match: target_industry.to_lowercase() == industry.to_lowercase(),
                        market_cap_similarity: market_cap_ratio,
                        geographic_match: false, // Could be enhanced with geographic data
                    },
                })
            })
            .collect();

        // Sort by similarity score (descending)
        peers.sort_by(|a, b| {
            b.similarity_score
                .partial_cmp(&a.similarity_score)
                .unwrap_or(std::cmp::Ordering::Equal)
        });

        // Take top N peers
        peers.into_iter().take(max_peers).collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_gics_sector_from_string() {
        assert_eq!(
            GICSSector::from_string("Energy"),
            Some(GICSSector::Energy)
        );
        assert_eq!(
            GICSSector::from_string("information technology"),
            Some(GICSSector::InformationTechnology)
        );
        assert_eq!(GICSSector::from_string("invalid"), None);
    }

    #[test]
    fn test_market_cap_category() {
        assert_eq!(
            MarketCapCategory::from_market_cap(300_000_000_000.0),
            MarketCapCategory::Mega
        );
        assert_eq!(
            MarketCapCategory::from_market_cap(50_000_000_000.0),
            MarketCapCategory::Large
        );
        assert_eq!(
            MarketCapCategory::from_market_cap(5_000_000_000.0),
            MarketCapCategory::Mid
        );
    }

    #[test]
    fn test_similarity_calculation() {
        let service = PeerIdentificationService::new();

        // Same sector, same industry, similar market cap
        let score = service.calculate_similarity(
            "Technology",
            "Software",
            100_000_000_000.0,
            "Technology",
            "Software",
            90_000_000_000.0,
        );
        assert!(score > 0.95);

        // Different sector
        let score = service.calculate_similarity(
            "Technology",
            "Software",
            100_000_000_000.0,
            "Energy",
            "Oil & Gas",
            100_000_000_000.0,
        );
        assert!(score < 0.5);
    }
}
