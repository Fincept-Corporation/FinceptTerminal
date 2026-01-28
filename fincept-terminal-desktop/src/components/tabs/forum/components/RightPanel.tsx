// Forum Tab Right Panel Component (Trending, Activity, Sentiment)
// Redesigned to follow Fincept Terminal UI Design System

import React, { useMemo } from 'react';
import { TrendingUp, Activity, BarChart3, Flame } from 'lucide-react';
import type { ForumColors, TrendingTopic, RecentActivity, ForumPost } from '../types';
import { getSentimentColor } from '../constants';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface RightPanelProps {
  colors: ForumColors;
  trendingTopics: TrendingTopic[];
  recentActivity: RecentActivity[];
  forumPosts?: ForumPost[];
}

export const RightPanel: React.FC<RightPanelProps> = ({
  colors,
  trendingTopics,
  recentActivity,
  forumPosts = []
}) => {
  // Calculate real sentiment analysis from posts
  const sentimentAnalysis = useMemo(() => {
    if (forumPosts.length === 0) {
      return {
        bullish: 0,
        bearish: 0,
        neutral: 0,
        overall: 0,
        overallLabel: 'NEUTRAL'
      };
    }

    const bullishCount = forumPosts.filter(p => p.sentiment === 'BULLISH').length;
    const bearishCount = forumPosts.filter(p => p.sentiment === 'BEARISH').length;
    const neutralCount = forumPosts.filter(p => p.sentiment === 'NEUTRAL').length;
    const total = forumPosts.length;

    const bullishPct = Math.round((bullishCount / total) * 100);
    const bearishPct = Math.round((bearishCount / total) * 100);
    const neutralPct = Math.round((neutralCount / total) * 100);

    const overall = ((bullishCount - bearishCount) / total).toFixed(2);
    const overallLabel = parseFloat(overall) > 0.2 ? 'BULLISH' : parseFloat(overall) < -0.2 ? 'BEARISH' : 'NEUTRAL';

    return {
      bullish: bullishPct,
      bearish: bearishPct,
      neutral: neutralPct,
      overall: parseFloat(overall),
      overallLabel
    };
  }, [forumPosts]);

  return (
    <div style={{
      width: '320px',
      backgroundColor: colors.PANEL_BG,
      borderLeft: `1px solid ${colors.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Trending Topics Section */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.HEADER_BG,
        borderBottom: `1px solid ${colors.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '12px' }}>
          <TrendingUp size={12} color={colors.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: colors.GRAY,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY
          }}>
            TRENDING (1H)
          </span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {trendingTopics.map((item, index) => (
            <div
              key={index}
              style={{
                padding: '8px',
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = colors.ORANGE;
                e.currentTarget.style.backgroundColor = colors.HOVER;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = colors.BORDER;
                e.currentTarget.style.backgroundColor = colors.DARK_BG;
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '6px'
              }}>
                <span style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: colors.WHITE,
                  fontFamily: FONT_FAMILY
                }}>
                  {item.topic}
                </span>
                <Flame size={10} color={colors.ORANGE} />
              </div>
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                fontSize: '8px',
                fontFamily: FONT_FAMILY
              }}>
                <span style={{ color: colors.GRAY, letterSpacing: '0.5px' }}>
                  {item.mentions} MENTIONS
                </span>
                <span style={{
                  padding: '2px 6px',
                  backgroundColor: `${getSentimentColor(item.sentiment, colors)}20`,
                  color: getSentimentColor(item.sentiment, colors),
                  borderRadius: '2px',
                  fontWeight: 700,
                  letterSpacing: '0.5px'
                }}>
                  {item.sentiment}
                </span>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Recent Activity Section */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.HEADER_BG,
        borderBottom: `1px solid ${colors.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px' }}>
          <Activity size={12} color={colors.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: colors.GRAY,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY
          }}>
            RECENT ACTIVITY
          </span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {recentActivity.map((activity, index) => (
            <div
              key={index}
              style={{
                padding: '6px 8px',
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.BORDER}`,
                borderRadius: '2px',
                fontSize: '8px',
                fontFamily: FONT_FAMILY,
                lineHeight: '1.4',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = colors.MUTED;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = colors.BORDER;
              }}
            >
              <span style={{ color: colors.CYAN, fontWeight: 700 }}>@{activity.user}</span>
              {' '}
              <span style={{ color: colors.GRAY }}>{activity.action}</span>
              {' '}
              <span style={{ color: colors.WHITE, fontWeight: 700 }}>{activity.target}</span>
              <div style={{
                marginTop: '4px',
                color: colors.MUTED,
                fontSize: '7px',
                letterSpacing: '0.5px'
              }}>
                {activity.time}
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Sentiment Analysis Section */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.HEADER_BG,
          borderBottom: `1px solid ${colors.BORDER}`
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <BarChart3 size={12} color={colors.ORANGE} />
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              SENTIMENT ANALYSIS
            </span>
          </div>
        </div>

        <div style={{ padding: '12px' }}>
          {forumPosts.length === 0 ? (
            <div style={{
              textAlign: 'center',
              padding: '20px',
              color: colors.MUTED,
              fontSize: '9px',
              fontFamily: FONT_FAMILY
            }}>
              NO DATA AVAILABLE
            </div>
          ) : (
            <>
              <div style={{ marginBottom: '12px' }}>
                <div style={{
                  padding: '8px',
                  backgroundColor: colors.DARK_BG,
                  border: `1px solid ${colors.BORDER}`,
                  borderRadius: '2px',
                  marginBottom: '8px'
                }}>
                  <div style={{
                    fontSize: '8px',
                    color: colors.GRAY,
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: FONT_FAMILY
                  }}>
                    BULLISH
                  </div>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px'
                  }}>
                    <div style={{
                      flex: 1,
                      height: '6px',
                      backgroundColor: colors.DARK_BG,
                      border: `1px solid ${colors.BORDER}`,
                      borderRadius: '2px',
                      overflow: 'hidden'
                    }}>
                      <div style={{
                        height: '100%',
                        width: `${sentimentAnalysis.bullish}%`,
                        backgroundColor: colors.GREEN,
                        transition: 'width 0.3s'
                      }} />
                    </div>
                    <span style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: colors.GREEN,
                      fontFamily: FONT_FAMILY,
                      minWidth: '32px',
                      textAlign: 'right'
                    }}>
                      {sentimentAnalysis.bullish}%
                    </span>
                  </div>
                </div>

                <div style={{
                  padding: '8px',
                  backgroundColor: colors.DARK_BG,
                  border: `1px solid ${colors.BORDER}`,
                  borderRadius: '2px',
                  marginBottom: '8px'
                }}>
                  <div style={{
                    fontSize: '8px',
                    color: colors.GRAY,
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: FONT_FAMILY
                  }}>
                    BEARISH
                  </div>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px'
                  }}>
                    <div style={{
                      flex: 1,
                      height: '6px',
                      backgroundColor: colors.DARK_BG,
                      border: `1px solid ${colors.BORDER}`,
                      borderRadius: '2px',
                      overflow: 'hidden'
                    }}>
                      <div style={{
                        height: '100%',
                        width: `${sentimentAnalysis.bearish}%`,
                        backgroundColor: colors.RED,
                        transition: 'width 0.3s'
                      }} />
                    </div>
                    <span style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: colors.RED,
                      fontFamily: FONT_FAMILY,
                      minWidth: '32px',
                      textAlign: 'right'
                    }}>
                      {sentimentAnalysis.bearish}%
                    </span>
                  </div>
                </div>

                <div style={{
                  padding: '8px',
                  backgroundColor: colors.DARK_BG,
                  border: `1px solid ${colors.BORDER}`,
                  borderRadius: '2px'
                }}>
                  <div style={{
                    fontSize: '8px',
                    color: colors.GRAY,
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: FONT_FAMILY
                  }}>
                    NEUTRAL
                  </div>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px'
                  }}>
                    <div style={{
                      flex: 1,
                      height: '6px',
                      backgroundColor: colors.DARK_BG,
                      border: `1px solid ${colors.BORDER}`,
                      borderRadius: '2px',
                      overflow: 'hidden'
                    }}>
                      <div style={{
                        height: '100%',
                        width: `${sentimentAnalysis.neutral}%`,
                        backgroundColor: colors.YELLOW,
                        transition: 'width 0.3s'
                      }} />
                    </div>
                    <span style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: colors.YELLOW,
                      fontFamily: FONT_FAMILY,
                      minWidth: '32px',
                      textAlign: 'right'
                    }}>
                      {sentimentAnalysis.neutral}%
                    </span>
                  </div>
                </div>
              </div>

              <div style={{
                padding: '12px',
                backgroundColor: colors.DARK_BG,
                border: `2px solid ${colors.ORANGE}`,
                borderRadius: '2px',
                textAlign: 'center'
              }}>
                <div style={{
                  fontSize: '8px',
                  color: colors.GRAY,
                  marginBottom: '4px',
                  letterSpacing: '0.5px',
                  fontFamily: FONT_FAMILY
                }}>
                  OVERALL SENTIMENT
                </div>
                <div style={{
                  fontSize: '16px',
                  fontWeight: 700,
                  color: sentimentAnalysis.overallLabel === 'BULLISH' ? colors.GREEN :
                         sentimentAnalysis.overallLabel === 'BEARISH' ? colors.RED : colors.YELLOW,
                  fontFamily: FONT_FAMILY,
                  marginBottom: '2px'
                }}>
                  {sentimentAnalysis.overall >= 0 ? '+' : ''}{sentimentAnalysis.overall.toFixed(2)}
                </div>
                <div style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: sentimentAnalysis.overallLabel === 'BULLISH' ? colors.GREEN :
                         sentimentAnalysis.overallLabel === 'BEARISH' ? colors.RED : colors.YELLOW,
                  letterSpacing: '0.5px',
                  fontFamily: FONT_FAMILY
                }}>
                  {sentimentAnalysis.overallLabel}
                </div>
              </div>
            </>
          )}
        </div>
      </div>
    </div>
  );
};
