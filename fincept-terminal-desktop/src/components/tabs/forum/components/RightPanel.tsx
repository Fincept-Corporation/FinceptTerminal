// Forum Tab Right Panel Component (Trending, Activity, Sentiment)

import React from 'react';
import type { ForumColors, TrendingTopic, RecentActivity } from '../types';
import { getSentimentColor } from '../constants';

interface RightPanelProps {
  colors: ForumColors;
  trendingTopics: TrendingTopic[];
  recentActivity: RecentActivity[];
}

export const RightPanel: React.FC<RightPanelProps> = ({
  colors,
  trendingTopics,
  recentActivity
}) => {
  return (
    <div style={{
      width: '280px',
      backgroundColor: colors.PANEL_BG,
      border: `1px solid ${colors.GRAY}`,
      padding: '4px',
      overflow: 'auto'
    }}>
      {/* Trending Topics Section */}
      <div style={{ color: colors.ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
        TRENDING TOPICS (1H)
      </div>
      <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '8px' }}></div>

      {trendingTopics.map((item, index) => (
        <div key={index} style={{
          padding: '3px',
          marginBottom: '3px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          fontSize: '9px'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
            <span style={{ color: colors.YELLOW }}>{item.topic}</span>
            <span style={{ color: colors.GREEN }}>{item.change}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', color: colors.GRAY }}>
            <span>{item.mentions} mentions</span>
            <span style={{ color: getSentimentColor(item.sentiment, colors) }}>{item.sentiment}</span>
          </div>
        </div>
      ))}

      {/* Recent Activity Section */}
      <div style={{ borderTop: `1px solid ${colors.GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
        <div style={{ color: colors.YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
          RECENT ACTIVITY
        </div>
        {recentActivity.map((activity, index) => (
          <div key={index} style={{
            padding: '2px',
            marginBottom: '2px',
            fontSize: '8px',
            color: colors.GRAY,
            lineHeight: '1.3'
          }}>
            <span style={{ color: colors.CYAN }}>@{activity.user}</span>
            {' '}<span>{activity.action}</span>{' '}
            <span style={{ color: colors.WHITE }}>{activity.target}</span>
            {' '}<span style={{ color: colors.YELLOW }}>• {activity.time}</span>
          </div>
        ))}
      </div>

      {/* Sentiment Analysis Section */}
      <div style={{ borderTop: `1px solid ${colors.GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
        <div style={{ color: colors.YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
          SENTIMENT ANALYSIS
        </div>
        <div style={{ fontSize: '9px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
            <span style={{ color: colors.GRAY }}>Bullish Posts:</span>
            <span style={{ color: colors.GREEN }}>68%</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
            <span style={{ color: colors.GRAY }}>Bearish Posts:</span>
            <span style={{ color: colors.RED }}>18%</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
            <span style={{ color: colors.GRAY }}>Neutral Posts:</span>
            <span style={{ color: colors.YELLOW }}>14%</span>
          </div>
          <div style={{ borderTop: `1px solid ${colors.GRAY}`, marginTop: '4px', paddingTop: '4px' }}>
            <span style={{ color: colors.CYAN }}>Overall Sentiment:</span>
            <span style={{ color: colors.GREEN, fontWeight: 'bold', marginLeft: '4px' }}>+0.62 BULLISH</span>
          </div>
        </div>
      </div>
    </div>
  );
};
