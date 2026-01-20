import React from 'react';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';
import type { NewsData, StockInfo } from '../types';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  BORDER: FINCEPT.BORDER,
};

interface NewsTabProps {
  newsData: NewsData | null;
  newsLoading: boolean;
  currentSymbol: string;
  stockInfo: StockInfo | null;
  onRefresh: () => void;
}

export const NewsTab: React.FC<NewsTabProps> = ({
  newsData,
  newsLoading,
  currentSymbol,
  stockInfo,
  onRefresh,
}) => {
  if (newsLoading) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: '16px',
      }}>
        <div style={{
          width: '50px',
          height: '50px',
          border: '4px solid #404040',
          borderTop: '4px solid #ea580c',
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }} />
        <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
          FETCHING LATEST NEWS...
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
          Loading news for {currentSymbol}
        </div>
      </div>
    );
  }

  if (!newsData || !newsData.success) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: '16px',
      }}>
        <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>
          [WARN] NO NEWS AVAILABLE
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
          {newsData?.error || 'Unable to fetch news at this time'}
        </div>
        <button
          onClick={onRefresh}
          style={{
            backgroundColor: COLORS.ORANGE,
            border: 'none',
            color: COLORS.DARK_BG,
            padding: '8px 16px',
            fontSize: '10px',
            cursor: 'pointer',
            fontWeight: 'bold',
            marginTop: '8px',
          }}
        >
          RETRY
        </button>
      </div>
    );
  }

  if (newsData.data && newsData.data.length === 0) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: '16px',
      }}>
        <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
          NO NEWS FOUND
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
          No recent news articles found for {stockInfo?.company_name || currentSymbol}
        </div>
      </div>
    );
  }

  return (
    <div style={{ padding: '8px', height: 'calc(100vh - 280px)', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      {/* News Header */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '8px 12px',
        marginBottom: '8px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ color: COLORS.ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
            LATEST NEWS
          </span>
          <span style={{ color: COLORS.GRAY, fontSize: '9px' }}>
            {newsData.count || 0} articles found
          </span>
          <span style={{ color: COLORS.CYAN, fontSize: '9px' }}>
            {stockInfo?.company_name || currentSymbol}
          </span>
        </div>
        <button
          onClick={onRefresh}
          style={{
            backgroundColor: COLORS.DARK_BG,
            border: `1px solid ${COLORS.BORDER}`,
            color: COLORS.WHITE,
            padding: '4px 12px',
            fontSize: '9px',
            cursor: 'pointer',
            fontWeight: 'bold',
          }}
        >
          REFRESH
        </button>
      </div>

      {/* News Articles List */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        display: 'flex',
        flexDirection: 'column',
        gap: '8px',
      }} className="custom-scrollbar">
        {newsData.data.map((article, index) => (
          <div
            key={index}
            style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '12px',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#252525';
              e.currentTarget.style.borderColor = COLORS.ORANGE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = COLORS.PANEL_BG;
              e.currentTarget.style.borderColor = COLORS.BORDER;
            }}
            onClick={() => {
              if (article.url && article.url !== '#') {
                window.open(article.url, '_blank');
              }
            }}
          >
            {/* Article Header */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
              <div style={{ flex: 1, marginRight: '12px' }}>
                <h3 style={{
                  color: COLORS.WHITE,
                  fontSize: '12px',
                  fontWeight: 'bold',
                  margin: 0,
                  marginBottom: '6px',
                  lineHeight: '1.4',
                }}>
                  {article.title}
                </h3>
                <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
                  <span style={{
                    color: COLORS.CYAN,
                    fontSize: '9px',
                    fontWeight: 'bold',
                  }}>
                    {article.publisher || 'Unknown Source'}
                  </span>
                  <span style={{
                    color: COLORS.GRAY,
                    fontSize: '9px',
                  }}>
                    {article.published_date || 'Unknown Date'}
                  </span>
                </div>
              </div>
            </div>

            {/* Article Description */}
            {article.description && article.description !== 'N/A' && (
              <div style={{
                color: COLORS.GRAY,
                fontSize: '10px',
                lineHeight: '1.5',
                marginBottom: '8px',
              }}>
                {article.description}
              </div>
            )}

            {/* Read More Link */}
            {article.url && article.url !== '#' && (
              <div style={{
                color: COLORS.ORANGE,
                fontSize: '9px',
                fontWeight: 'bold',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}>
                READ FULL ARTICLE
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
};

export default NewsTab;
