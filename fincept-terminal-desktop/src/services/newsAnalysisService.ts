/**
 * News Analysis Service
 * AI-powered news article analysis via Fincept Backend API
 */

const API_BASE_URL = 'https://finceptbackend.share.zrok.io';
const API_KEY = 'fk_user_uhwLowYhfwEg6H3WCtXK4hqG0DhZfNhK2UmtNpHuLuB';

export interface NewsAnalysisRequest {
  url: string;
}

export interface SentimentAnalysis {
  score: number;
  intensity: number;
  confidence: number;
}

export interface MarketImpact {
  urgency: 'LOW' | 'MEDIUM' | 'HIGH';
  prediction: 'negative' | 'neutral' | 'moderate_positive' | 'positive';
}

export interface Entity {
  name: string;
  ticker: string | null;
  sector: string;
  sentiment: number;
}

export interface Entities {
  organizations: Entity[];
  people: Entity[];
  locations: Entity[];
}

export interface RiskSignal {
  level: string;
  details: string;
}

export interface RiskSignals {
  regulatory: RiskSignal;
  geopolitical: RiskSignal;
  operational: RiskSignal;
  market: RiskSignal;
}

export interface NewsAnalysis {
  sentiment: SentimentAnalysis;
  market_impact: MarketImpact;
  entities: Entities;
  keywords: string[];
  topics: string[];
  risk_signals: RiskSignals;
  summary: string;
  key_points: string[];
}

export interface ArticleContent {
  headline: string;
  word_count: number;
}

export interface NewsAnalysisData {
  article_id: string;
  source: string;
  url: string;
  published_at: string;
  ingested_at: string;
  processing_version: string;
  content: ArticleContent;
  analysis: NewsAnalysis;
  credits_used: number;
  credits_remaining: number;
}

export interface NewsAnalysisResponse {
  success: boolean;
  message: string;
  data: NewsAnalysisData;
}

export interface NewsAnalysisError {
  success: false;
  error: string;
  status_code: number;
}

/**
 * Analyze a news article using AI
 * @param url - Article URL to analyze
 * @returns Analysis results or error
 */
export async function analyzeNewsArticle(
  url: string
): Promise<NewsAnalysisResponse | NewsAnalysisError> {
  try {
    const response = await fetch(`${API_BASE_URL}/news/analyze`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-API-Key': API_KEY,
      },
      body: JSON.stringify({ url }),
    });

    const data = await response.json();

    if (!response.ok) {
      return {
        success: false,
        error: data.message || 'Failed to analyze article',
        status_code: response.status,
      };
    }

    return data as NewsAnalysisResponse;
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
      status_code: 0,
    };
  }
}

/**
 * Get sentiment color based on score
 */
export function getSentimentColor(score: number): string {
  if (score > 0.3) return '#00ff00'; // Green - positive
  if (score < -0.3) return '#ff0000'; // Red - negative
  return '#ffaa00'; // Orange - neutral
}

/**
 * Get urgency color
 */
export function getUrgencyColor(urgency: string): string {
  switch (urgency) {
    case 'HIGH':
      return '#ff0000';
    case 'MEDIUM':
      return '#ffaa00';
    case 'LOW':
      return '#00ff00';
    default:
      return '#888888';
  }
}

/**
 * Get risk level color
 */
export function getRiskColor(level: string): string {
  switch (level.toLowerCase()) {
    case 'high':
      return '#ff0000';
    case 'medium':
      return '#ffaa00';
    case 'low':
      return '#00ff00';
    case 'none':
      return '#888888';
    default:
      return '#888888';
  }
}
