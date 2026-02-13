/**
 * Sentiment Analysis Panel
 *
 * Displays market sentiment analysis from news and social sources.
 * Shows sentiment scores, headlines, and market mood indicators.
 */

import React, { useState } from 'react';
import {
  Newspaper, TrendingUp, TrendingDown, Minus, RefreshCw,
  Globe, AlertCircle, ChevronRight, Loader2,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { useCache, cacheKey } from '@/hooks/useCache';
import { validateSymbol } from '@/services/core/validators';
import {
  alphaArenaEnhancedService,
  type SentimentResult,
  type MarketMood,
} from '../services/alphaArenaEnhancedService';

const FINCEPT = {
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
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

const SENTIMENT_COLORS: Record<string, string> = {
  very_bearish: FINCEPT.RED,
  bearish: '#FF6B6B',
  neutral: FINCEPT.GRAY,
  bullish: '#4ADE80',
  very_bullish: FINCEPT.GREEN,
};

const SENTIMENT_LABELS: Record<string, string> = {
  very_bearish: 'VERY BEARISH',
  bearish: 'BEARISH',
  neutral: 'NEUTRAL',
  bullish: 'BULLISH',
  very_bullish: 'VERY BULLISH',
};

const SENTIMENT_STYLES = `
  .sentiment-panel *::-webkit-scrollbar { width: 6px; height: 6px; }
  .sentiment-panel *::-webkit-scrollbar-track { background: #000000; }
  .sentiment-panel *::-webkit-scrollbar-thumb { background: #2A2A2A; }
  @keyframes sentiment-spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
`;

interface SentimentPanelProps {
  symbol?: string;
  showMarketMood?: boolean;
}

const SentimentPanel: React.FC<SentimentPanelProps> = ({
  symbol = 'BTC',
  showMarketMood = true,
}) => {
  const [activeTab, setActiveTab] = useState<'symbol' | 'market'>('symbol');
  const [hoveredBtn, setHoveredBtn] = useState<string | null>(null);

  // Cache: symbol sentiment (enabled when activeTab === 'symbol')
  const sentimentCache = useCache<SentimentResult>({
    key: cacheKey('alpha-arena', 'sentiment', symbol),
    category: 'alpha-arena',
    fetcher: async () => {
      const validSym = validateSymbol(symbol);
      if (!validSym.valid) throw new Error(validSym.error);
      const result = await alphaArenaEnhancedService.getSentiment(symbol);
      if (result.success && result.sentiment) {
        return result.sentiment;
      }
      throw new Error(result.error || 'Failed to fetch sentiment');
    },
    ttl: 300,
    enabled: activeTab === 'symbol',
    staleWhileRevalidate: true,
  });

  // Cache: market mood (enabled when activeTab === 'market')
  const marketMoodCache = useCache<MarketMood>({
    key: cacheKey('alpha-arena', 'market-mood'),
    category: 'alpha-arena',
    fetcher: async () => {
      const result = await alphaArenaEnhancedService.getMarketMood();
      if (result.success && result.market_mood) {
        return result.market_mood;
      }
      throw new Error(result.error || 'Failed to fetch market mood');
    },
    ttl: 300,
    enabled: activeTab === 'market',
    staleWhileRevalidate: true,
  });

  const sentiment = sentimentCache.data;
  const marketMood = marketMoodCache.data;
  const isLoading = activeTab === 'symbol' ? sentimentCache.isLoading : marketMoodCache.isLoading;
  const error = activeTab === 'symbol' ? sentimentCache.error : marketMoodCache.error;

  const handleRefresh = () => {
    if (activeTab === 'symbol') {
      sentimentCache.refresh();
    } else {
      marketMoodCache.refresh();
    }
  };

  const getSentimentIcon = (sentimentLevel: string) => {
    if (sentimentLevel.includes('bullish')) {
      return <TrendingUp size={16} style={{ color: SENTIMENT_COLORS[sentimentLevel] }} />;
    } else if (sentimentLevel.includes('bearish')) {
      return <TrendingDown size={16} style={{ color: SENTIMENT_COLORS[sentimentLevel] }} />;
    }
    return <Minus size={16} style={{ color: FINCEPT.GRAY }} />;
  };

  const renderSentimentMeter = (score: number) => {
    const percentage = ((score + 1) / 2) * 100;

    return (
      <div style={{
        position: 'relative',
        height: '6px',
        overflow: 'hidden',
        backgroundColor: FINCEPT.BORDER,
      }}>
        <div style={{
          position: 'absolute',
          inset: 0,
          background: `linear-gradient(to right, ${FINCEPT.RED}, ${FINCEPT.GRAY}, ${FINCEPT.GREEN})`,
        }} />
        <div style={{
          position: 'absolute',
          width: '10px',
          height: '10px',
          backgroundColor: FINCEPT.WHITE,
          top: '-2px',
          left: `${percentage}%`,
          transform: 'translateX(-50%)',
          boxShadow: '0 0 4px rgba(0,0,0,0.5)',
        }} />
      </div>
    );
  };

  return (
    <div className="sentiment-panel" style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      fontFamily: TERMINAL_FONT,
    }}>
      <style>{SENTIMENT_STYLES}</style>

      {/* Header */}
      <div style={{
        padding: '10px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Newspaper size={16} style={{ color: FINCEPT.BLUE }} />
          <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            SENTIMENT ANALYSIS
          </span>
        </div>
        <button
          onClick={handleRefresh}
          disabled={isLoading}
          onMouseEnter={() => setHoveredBtn('refresh')}
          onMouseLeave={() => setHoveredBtn(null)}
          style={{
            padding: '4px',
            backgroundColor: hoveredBtn === 'refresh' ? FINCEPT.HOVER : 'transparent',
            border: 'none',
            cursor: isLoading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
          }}
        >
          <RefreshCw
            size={14}
            style={{
              color: FINCEPT.GRAY,
              animation: isLoading ? 'sentiment-spin 1s linear infinite' : 'none',
            }}
          />
        </button>
      </div>

      {/* Tabs */}
      {showMarketMood && (
        <div style={{ display: 'flex', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
          <button
            onClick={() => setActiveTab('symbol')}
            onMouseEnter={() => setHoveredBtn('tab-symbol')}
            onMouseLeave={() => setHoveredBtn(null)}
            style={{
              flex: 1,
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 500,
              fontFamily: TERMINAL_FONT,
              backgroundColor: activeTab === 'symbol' ? FINCEPT.CARD_BG : (hoveredBtn === 'tab-symbol' ? FINCEPT.HOVER : 'transparent'),
              color: activeTab === 'symbol' ? FINCEPT.ORANGE : FINCEPT.GRAY,
              borderBottom: activeTab === 'symbol' ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
              border: 'none',
              borderBottomStyle: 'solid',
              borderBottomWidth: '2px',
              borderBottomColor: activeTab === 'symbol' ? FINCEPT.ORANGE : 'transparent',
              cursor: 'pointer',
              letterSpacing: '0.5px',
            }}
          >
            {symbol} SENTIMENT
          </button>
          <button
            onClick={() => setActiveTab('market')}
            onMouseEnter={() => setHoveredBtn('tab-market')}
            onMouseLeave={() => setHoveredBtn(null)}
            style={{
              flex: 1,
              padding: '8px 16px',
              fontSize: '10px',
              fontWeight: 500,
              fontFamily: TERMINAL_FONT,
              backgroundColor: activeTab === 'market' ? FINCEPT.CARD_BG : (hoveredBtn === 'tab-market' ? FINCEPT.HOVER : 'transparent'),
              color: activeTab === 'market' ? FINCEPT.ORANGE : FINCEPT.GRAY,
              border: 'none',
              borderBottomStyle: 'solid',
              borderBottomWidth: '2px',
              borderBottomColor: activeTab === 'market' ? FINCEPT.ORANGE : 'transparent',
              cursor: 'pointer',
              letterSpacing: '0.5px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
            }}
          >
            <Globe size={12} />
            MARKET MOOD
          </button>
        </div>
      )}

      {/* Content */}
      <div style={{ padding: '16px' }}>
        {error && (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            padding: '8px',
            marginBottom: '12px',
            backgroundColor: FINCEPT.RED + '10',
          }}>
            <AlertCircle size={14} style={{ color: FINCEPT.RED }} />
            <span style={{ fontSize: '11px', color: FINCEPT.RED, fontFamily: TERMINAL_FONT }}>{error.message}</span>
          </div>
        )}

        {isLoading ? (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
            <Loader2 size={24} style={{ color: FINCEPT.ORANGE, animation: 'sentiment-spin 1s linear infinite' }} />
          </div>
        ) : activeTab === 'symbol' && sentiment ? (
          <>
            {/* Main Sentiment Display */}
            <div style={{ textAlign: 'center', marginBottom: '16px' }}>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px', marginBottom: '8px' }}>
                {getSentimentIcon(sentiment.overall_sentiment)}
                <span style={{
                  fontSize: '14px',
                  fontWeight: 700,
                  color: SENTIMENT_COLORS[sentiment.overall_sentiment],
                  letterSpacing: '1px',
                }}>
                  {SENTIMENT_LABELS[sentiment.overall_sentiment]}
                </span>
              </div>
              <div style={{ fontSize: '24px', fontWeight: 700, marginBottom: '4px', color: FINCEPT.WHITE }}>
                {sentiment.sentiment_score >= 0 ? '+' : ''}{(sentiment.sentiment_score * 100).toFixed(0)}%
              </div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                Confidence: {(sentiment.confidence * 100).toFixed(0)}%
              </div>
            </div>

            {/* Sentiment Meter */}
            <div style={{ marginBottom: '16px' }}>
              {renderSentimentMeter(sentiment.sentiment_score)}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                fontSize: '9px',
                marginTop: '4px',
                color: FINCEPT.GRAY,
              }}>
                <span>Bearish</span>
                <span>Neutral</span>
                <span>Bullish</span>
              </div>
            </div>

            {/* Article Breakdown */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '16px' }}>
              <div style={{ padding: '8px', textAlign: 'center', backgroundColor: FINCEPT.GREEN + '10' }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {sentiment.bullish_count}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Bullish</div>
              </div>
              <div style={{ padding: '8px', textAlign: 'center', backgroundColor: FINCEPT.GRAY + '10' }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.GRAY }}>
                  {sentiment.neutral_count}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Neutral</div>
              </div>
              <div style={{ padding: '8px', textAlign: 'center', backgroundColor: FINCEPT.RED + '10' }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.RED }}>
                  {sentiment.bearish_count}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Bearish</div>
              </div>
            </div>

            {/* Key Headlines */}
            {sentiment.key_headlines.length > 0 && (
              <div>
                <div style={{ fontSize: '10px', fontWeight: 500, marginBottom: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                  KEY HEADLINES
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                  {sentiment.key_headlines.slice(0, 4).map((headline, idx) => (
                    <div
                      key={idx}
                      style={{
                        display: 'flex',
                        alignItems: 'flex-start',
                        gap: '8px',
                        padding: '8px',
                        backgroundColor: FINCEPT.CARD_BG,
                        border: `1px solid ${FINCEPT.BORDER}`,
                      }}
                    >
                      <ChevronRight size={12} style={{ color: FINCEPT.GRAY, marginTop: 2, flexShrink: 0 }} />
                      <span style={{
                        fontSize: '11px',
                        color: FINCEPT.WHITE,
                        overflow: 'hidden',
                        display: '-webkit-box',
                        WebkitLineClamp: 2,
                        WebkitBoxOrient: 'vertical',
                      }}>
                        {headline}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* Sources */}
            {sentiment.sources.length > 0 && (
              <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                  {sentiment.sources.slice(0, 5).map((source, idx) => (
                    <span
                      key={idx}
                      style={{
                        fontSize: '9px',
                        padding: '2px 8px',
                        backgroundColor: FINCEPT.BORDER,
                        color: FINCEPT.GRAY,
                        fontFamily: TERMINAL_FONT,
                      }}
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
            <div style={{ textAlign: 'center', marginBottom: '16px' }}>
              <div style={{
                display: 'inline-flex',
                alignItems: 'center',
                gap: '8px',
                padding: '8px 16px',
                marginBottom: '8px',
                backgroundColor: marketMood.overall_mood === 'risk_on' ? FINCEPT.GREEN + '20' :
                  marketMood.overall_mood === 'risk_off' ? FINCEPT.RED + '20' : FINCEPT.GRAY + '20',
              }}>
                <Globe size={16} style={{
                  color: marketMood.overall_mood === 'risk_on' ? FINCEPT.GREEN :
                    marketMood.overall_mood === 'risk_off' ? FINCEPT.RED : FINCEPT.GRAY,
                }} />
                <span style={{
                  fontSize: '13px',
                  fontWeight: 700,
                  letterSpacing: '1px',
                  color: marketMood.overall_mood === 'risk_on' ? FINCEPT.GREEN :
                    marketMood.overall_mood === 'risk_off' ? FINCEPT.RED : FINCEPT.GRAY,
                }}>
                  {marketMood.overall_mood === 'risk_on' ? 'RISK ON' :
                    marketMood.overall_mood === 'risk_off' ? 'RISK OFF' : 'MIXED'}
                </span>
              </div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                Avg Sentiment: {(marketMood.average_sentiment * 100).toFixed(0)}% | {marketMood.symbols_analyzed} symbols
              </div>
            </div>

            {/* Individual Sentiments */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {Object.entries(marketMood.individual_sentiments).map(([sym, data]) => (
                <div
                  key={sym}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    padding: '8px',
                    backgroundColor: FINCEPT.CARD_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    {getSentimentIcon(data.overall_sentiment)}
                    <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>{sym}</span>
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{
                      fontSize: '11px',
                      color: SENTIMENT_COLORS[data.overall_sentiment],
                      fontFamily: TERMINAL_FONT,
                    }}>
                      {(data.sentiment_score * 100).toFixed(0)}%
                    </span>
                    <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                      ({data.articles_analyzed} articles)
                    </span>
                  </div>
                </div>
              ))}
            </div>
          </>
        ) : (
          <div style={{ textAlign: 'center', padding: '32px 0' }}>
            <Newspaper size={32} style={{ color: FINCEPT.GRAY, opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '12px', color: FINCEPT.GRAY }}>No sentiment data available</p>
          </div>
        )}
      </div>

      {/* Footer */}
      <div style={{
        padding: '6px 16px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '9px',
        backgroundColor: FINCEPT.HEADER_BG,
        color: FINCEPT.GRAY,
      }}>
        {sentiment && (
          <span>
            Analyzed {sentiment.articles_analyzed} articles | Updated {new Date(sentiment.timestamp).toLocaleTimeString()}
          </span>
        )}
      </div>
    </div>
  );
};

export default withErrorBoundary(SentimentPanel, { name: 'AlphaArena.SentimentPanel' });
