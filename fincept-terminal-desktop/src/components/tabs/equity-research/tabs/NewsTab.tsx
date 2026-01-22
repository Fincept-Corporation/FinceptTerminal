import React from 'react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../../portfolio-tab/finceptStyles';
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
  HEADER_BG: FINCEPT.HEADER_BG,
  BORDER: FINCEPT.BORDER,
  HOVER: FINCEPT.HOVER,
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
        gap: SPACING.LARGE,
      }}>
        <div style={{
          width: '50px',
          height: '50px',
          border: `4px solid ${COLORS.BORDER}`,
          borderTop: `4px solid ${COLORS.ORANGE}`,
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }} />
        <div style={{ color: COLORS.YELLOW, fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD }}>
          FETCHING LATEST NEWS...
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.DEFAULT }}>
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
        gap: SPACING.LARGE,
      }}>
        <div style={{ color: COLORS.RED, fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD }}>
          [WARN] NO NEWS AVAILABLE
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.DEFAULT }}>
          {newsData?.error || 'Unable to fetch news at this time'}
        </div>
        <button
          onClick={onRefresh}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            marginTop: SPACING.MEDIUM,
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
        gap: SPACING.LARGE,
      }}>
        <div style={{ color: COLORS.YELLOW, fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD }}>
          NO NEWS FOUND
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.DEFAULT }}>
          No recent news articles found for {stockInfo?.company_name || currentSymbol}
        </div>
      </div>
    );
  }

  return (
    <div style={{ padding: SPACING.MEDIUM, height: 'calc(100vh - 280px)', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      {/* News Header */}
      <div style={{
        ...COMMON_STYLES.panel,
        backgroundColor: COLORS.HEADER_BG,
        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
        marginBottom: SPACING.MEDIUM,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
        borderBottom: `2px solid ${COLORS.ORANGE}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT }}>
          <span style={{ color: COLORS.ORANGE, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD, letterSpacing: TYPOGRAPHY.WIDE }}>
            LATEST NEWS
          </span>
          <span style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.SMALL }}>
            {newsData.count || 0} articles found
          </span>
          <span style={{ color: COLORS.CYAN, fontSize: TYPOGRAPHY.SMALL }}>
            {stockInfo?.company_name || currentSymbol}
          </span>
        </div>
        <button
          onClick={onRefresh}
          style={{
            ...COMMON_STYLES.buttonSecondary,
            padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
            fontSize: TYPOGRAPHY.SMALL,
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
        gap: SPACING.MEDIUM,
      }} className="custom-scrollbar">
        {newsData.data.map((article, index) => (
          <div
            key={index}
            style={{
              ...COMMON_STYLES.panel,
              padding: SPACING.DEFAULT,
              cursor: 'pointer',
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = COLORS.HOVER;
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
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: SPACING.MEDIUM }}>
              <div style={{ flex: 1, marginRight: SPACING.DEFAULT }}>
                <h3 style={{
                  color: COLORS.WHITE,
                  fontSize: TYPOGRAPHY.SUBHEADING,
                  fontWeight: TYPOGRAPHY.BOLD,
                  margin: 0,
                  marginBottom: SPACING.GAP_SMALL,
                  lineHeight: '1.4',
                }}>
                  {article.title}
                </h3>
                <div style={{ display: 'flex', gap: SPACING.DEFAULT, alignItems: 'center' }}>
                  <span style={{
                    color: COLORS.CYAN,
                    fontSize: TYPOGRAPHY.SMALL,
                    fontWeight: TYPOGRAPHY.BOLD,
                  }}>
                    {article.publisher || 'Unknown Source'}
                  </span>
                  <span style={{
                    color: COLORS.GRAY,
                    fontSize: TYPOGRAPHY.SMALL,
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
                fontSize: TYPOGRAPHY.BODY,
                lineHeight: '1.5',
                marginBottom: SPACING.MEDIUM,
              }}>
                {article.description}
              </div>
            )}

            {/* Read More Link */}
            {article.url && article.url !== '#' && (
              <div style={{
                color: COLORS.ORANGE,
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.SMALL,
                letterSpacing: TYPOGRAPHY.WIDE,
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
