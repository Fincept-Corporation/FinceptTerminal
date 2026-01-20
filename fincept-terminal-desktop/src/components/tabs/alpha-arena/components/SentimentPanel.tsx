/**
 * Sentiment Analysis Panel
 *
 * Displays market sentiment analysis from news and social sources.
 * Shows sentiment scores, headlines, and market mood indicators.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Newspaper, TrendingUp, TrendingDown, Minus, RefreshCw,
  Globe, AlertCircle, ChevronRight, ExternalLink, Loader2,
} from 'lucide-react';
import {
  alphaArenaEnhancedService,
  type SentimentResult,
  type MarketMood,
} from '../services/alphaArenaEnhancedService';

const COLORS = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  YELLOW: '#EAB308',
  PURPLE: '#8B5CF6',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
};

const SENTIMENT_COLORS: Record<string, string> = {
  very_bearish: COLORS.RED,
  bearish: '#FF6B6B',
  neutral: COLORS.GRAY,
  bullish: '#4ADE80',
  very_bullish: COLORS.GREEN,
};

const SENTIMENT_LABELS: Record<string, string> = {
  very_bearish: 'Very Bearish',
  bearish: 'Bearish',
  neutral: 'Neutral',
  bullish: 'Bullish',
  very_bullish: 'Very Bullish',
};

interface SentimentPanelProps {
  symbol?: string;
  showMarketMood?: boolean;
}

const SentimentPanel: React.FC<SentimentPanelProps> = ({
  symbol = 'BTC',
  showMarketMood = true,
}) => {
  const [sentiment, setSentiment] = useState<SentimentResult | null>(null);
  const [marketMood, setMarketMood] = useState<MarketMood | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [activeTab, setActiveTab] = useState<'symbol' | 'market'>('symbol');
  const [error, setError] = useState<string | null>(null);

  const fetchSentiment = useCallback(async () => {
    setIsLoading(true);
    setError(null);
    try {
      const result = await alphaArenaEnhancedService.getSentiment(symbol);
      if (result.success && result.sentiment) {
        setSentiment(result.sentiment);
      } else {
        setError(result.error || 'Failed to fetch sentiment');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to fetch sentiment');
    } finally {
      setIsLoading(false);
    }
  }, [symbol]);

  const fetchMarketMood = useCallback(async () => {
    setIsLoading(true);
    setError(null);
    try {
      const result = await alphaArenaEnhancedService.getMarketMood();
      if (result.success && result.market_mood) {
        setMarketMood(result.market_mood);
      }
    } catch (err) {
      console.error('Error fetching market mood:', err);
    } finally {
      setIsLoading(false);
    }
  }, []);

  useEffect(() => {
    if (activeTab === 'symbol') {
      fetchSentiment();
    } else {
      fetchMarketMood();
    }
  }, [activeTab, fetchSentiment, fetchMarketMood]);

  const getSentimentIcon = (sentimentLevel: string) => {
    if (sentimentLevel.includes('bullish')) {
      return <TrendingUp size={16} style={{ color: SENTIMENT_COLORS[sentimentLevel] }} />;
    } else if (sentimentLevel.includes('bearish')) {
      return <TrendingDown size={16} style={{ color: SENTIMENT_COLORS[sentimentLevel] }} />;
    }
    return <Minus size={16} style={{ color: COLORS.GRAY }} />;
  };

  const renderSentimentMeter = (score: number) => {
    // Score ranges from -1 (bearish) to 1 (bullish)
    const percentage = ((score + 1) / 2) * 100;

    return (
      <div className="relative h-2 rounded-full overflow-hidden" style={{ backgroundColor: COLORS.BORDER }}>
        {/* Background gradient */}
        <div
          className="absolute inset-0"
          style={{
            background: `linear-gradient(to right, ${COLORS.RED}, ${COLORS.GRAY}, ${COLORS.GREEN})`,
          }}
        />
        {/* Indicator */}
        <div
          className="absolute w-3 h-3 rounded-full bg-white -top-0.5 transform -translate-x-1/2"
          style={{
            left: `${percentage}%`,
            boxShadow: '0 0 4px rgba(0,0,0,0.5)',
          }}
        />
      </div>
    );
  };

  return (
    <div
      className="rounded-lg overflow-hidden"
      style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}
    >
      {/* Header */}
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <Newspaper size={16} style={{ color: COLORS.BLUE }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>
            Sentiment Analysis
          </span>
        </div>
        <button
          onClick={activeTab === 'symbol' ? fetchSentiment : fetchMarketMood}
          disabled={isLoading}
          className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
        >
          <RefreshCw
            size={14}
            className={isLoading ? 'animate-spin' : ''}
            style={{ color: COLORS.GRAY }}
          />
        </button>
      </div>

      {/* Tabs */}
      {showMarketMood && (
        <div className="flex border-b" style={{ borderColor: COLORS.BORDER }}>
          <button
            onClick={() => setActiveTab('symbol')}
            className="flex-1 px-4 py-2 text-xs font-medium transition-colors"
            style={{
              backgroundColor: activeTab === 'symbol' ? COLORS.CARD_BG : 'transparent',
              color: activeTab === 'symbol' ? COLORS.ORANGE : COLORS.GRAY,
              borderBottom: activeTab === 'symbol' ? `2px solid ${COLORS.ORANGE}` : 'none',
            }}
          >
            {symbol} Sentiment
          </button>
          <button
            onClick={() => setActiveTab('market')}
            className="flex-1 px-4 py-2 text-xs font-medium transition-colors"
            style={{
              backgroundColor: activeTab === 'market' ? COLORS.CARD_BG : 'transparent',
              color: activeTab === 'market' ? COLORS.ORANGE : COLORS.GRAY,
              borderBottom: activeTab === 'market' ? `2px solid ${COLORS.ORANGE}` : 'none',
            }}
          >
            <Globe size={12} className="inline mr-1" />
            Market Mood
          </button>
        </div>
      )}

      {/* Content */}
      <div className="p-4">
        {error && (
          <div className="flex items-center gap-2 p-2 rounded mb-3" style={{ backgroundColor: COLORS.RED + '10' }}>
            <AlertCircle size={14} style={{ color: COLORS.RED }} />
            <span className="text-xs" style={{ color: COLORS.RED }}>{error}</span>
          </div>
        )}

        {isLoading ? (
          <div className="flex items-center justify-center py-8">
            <Loader2 size={24} className="animate-spin" style={{ color: COLORS.ORANGE }} />
          </div>
        ) : activeTab === 'symbol' && sentiment ? (
          <>
            {/* Main Sentiment Display */}
            <div className="text-center mb-4">
              <div className="flex items-center justify-center gap-2 mb-2">
                {getSentimentIcon(sentiment.overall_sentiment)}
                <span
                  className="text-lg font-bold"
                  style={{ color: SENTIMENT_COLORS[sentiment.overall_sentiment] }}
                >
                  {SENTIMENT_LABELS[sentiment.overall_sentiment]}
                </span>
              </div>
              <div className="text-2xl font-bold mb-1" style={{ color: COLORS.WHITE }}>
                {sentiment.sentiment_score >= 0 ? '+' : ''}{(sentiment.sentiment_score * 100).toFixed(0)}%
              </div>
              <div className="text-xs" style={{ color: COLORS.GRAY }}>
                Confidence: {(sentiment.confidence * 100).toFixed(0)}%
              </div>
            </div>

            {/* Sentiment Meter */}
            <div className="mb-4">
              {renderSentimentMeter(sentiment.sentiment_score)}
              <div className="flex justify-between text-xs mt-1" style={{ color: COLORS.GRAY }}>
                <span>Bearish</span>
                <span>Neutral</span>
                <span>Bullish</span>
              </div>
            </div>

            {/* Article Breakdown */}
            <div className="grid grid-cols-3 gap-2 mb-4">
              <div className="p-2 rounded text-center" style={{ backgroundColor: COLORS.GREEN + '10' }}>
                <div className="text-lg font-bold" style={{ color: COLORS.GREEN }}>
                  {sentiment.bullish_count}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>Bullish</div>
              </div>
              <div className="p-2 rounded text-center" style={{ backgroundColor: COLORS.GRAY + '10' }}>
                <div className="text-lg font-bold" style={{ color: COLORS.GRAY }}>
                  {sentiment.neutral_count}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>Neutral</div>
              </div>
              <div className="p-2 rounded text-center" style={{ backgroundColor: COLORS.RED + '10' }}>
                <div className="text-lg font-bold" style={{ color: COLORS.RED }}>
                  {sentiment.bearish_count}
                </div>
                <div className="text-xs" style={{ color: COLORS.GRAY }}>Bearish</div>
              </div>
            </div>

            {/* Key Headlines */}
            {sentiment.key_headlines.length > 0 && (
              <div>
                <h4 className="text-xs font-medium mb-2" style={{ color: COLORS.GRAY }}>Key Headlines</h4>
                <div className="space-y-2">
                  {sentiment.key_headlines.slice(0, 4).map((headline, idx) => (
                    <div
                      key={idx}
                      className="flex items-start gap-2 p-2 rounded"
                      style={{ backgroundColor: COLORS.CARD_BG }}
                    >
                      <ChevronRight size={12} style={{ color: COLORS.GRAY, marginTop: 2 }} />
                      <span className="text-xs line-clamp-2" style={{ color: COLORS.WHITE }}>
                        {headline}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Sources */}
            {sentiment.sources.length > 0 && (
              <div className="mt-3 pt-3 border-t" style={{ borderColor: COLORS.BORDER }}>
                <div className="flex flex-wrap gap-1">
                  {sentiment.sources.slice(0, 5).map((source, idx) => (
                    <span
                      key={idx}
                      className="text-xs px-2 py-0.5 rounded"
                      style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}
                    >
                      {source}
                    </span>
                  ))}
                </div>
              </div>
            )}
          </>
        ) : activeTab === 'market' && marketMood ? (
          <>
            {/* Market Mood Overview */}
            <div className="text-center mb-4">
              <div
                className="inline-flex items-center gap-2 px-4 py-2 rounded-full mb-2"
                style={{
                  backgroundColor: marketMood.overall_mood === 'risk_on' ? COLORS.GREEN + '20' :
                    marketMood.overall_mood === 'risk_off' ? COLORS.RED + '20' : COLORS.GRAY + '20',
                }}
              >
                <Globe size={16} style={{
                  color: marketMood.overall_mood === 'risk_on' ? COLORS.GREEN :
                    marketMood.overall_mood === 'risk_off' ? COLORS.RED : COLORS.GRAY,
                }} />
                <span
                  className="text-sm font-bold"
                  style={{
                    color: marketMood.overall_mood === 'risk_on' ? COLORS.GREEN :
                      marketMood.overall_mood === 'risk_off' ? COLORS.RED : COLORS.GRAY,
                  }}
                >
                  {marketMood.overall_mood === 'risk_on' ? 'RISK ON' :
                    marketMood.overall_mood === 'risk_off' ? 'RISK OFF' : 'MIXED'}
                </span>
              </div>
              <div className="text-xs" style={{ color: COLORS.GRAY }}>
                Avg Sentiment: {(marketMood.average_sentiment * 100).toFixed(0)}% • {marketMood.symbols_analyzed} symbols
              </div>
            </div>

            {/* Individual Sentiments */}
            <div className="space-y-2">
              {Object.entries(marketMood.individual_sentiments).map(([sym, data]) => (
                <div
                  key={sym}
                  className="flex items-center justify-between p-2 rounded"
                  style={{ backgroundColor: COLORS.CARD_BG }}
                >
                  <div className="flex items-center gap-2">
                    {getSentimentIcon(data.overall_sentiment)}
                    <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>{sym}</span>
                  </div>
                  <div className="flex items-center gap-2">
                    <span
                      className="text-xs"
                      style={{ color: SENTIMENT_COLORS[data.overall_sentiment] }}
                    >
                      {(data.sentiment_score * 100).toFixed(0)}%
                    </span>
                    <span className="text-xs" style={{ color: COLORS.GRAY }}>
                      ({data.articles_analyzed} articles)
                    </span>
                  </div>
                </div>
              ))}
            </div>
          </>
        ) : (
          <div className="text-center py-8" style={{ color: COLORS.GRAY }}>
            <Newspaper size={32} className="mx-auto mb-2 opacity-30" />
            <p className="text-sm">No sentiment data available</p>
          </div>
        )}
      </div>

      {/* Footer */}
      <div className="px-4 py-2 border-t text-xs" style={{ borderColor: COLORS.BORDER, color: COLORS.GRAY }}>
        {sentiment && (
          <span>
            Analyzed {sentiment.articles_analyzed} articles • Updated {new Date(sentiment.timestamp).toLocaleTimeString()}
          </span>
        )}
      </div>
    </div>
  );
};

export default SentimentPanel;
