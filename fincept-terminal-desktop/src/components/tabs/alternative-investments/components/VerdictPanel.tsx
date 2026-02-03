import React from 'react';
import { InvestmentVerdict } from '@/services/alternativeInvestments/api/types';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  GREEN: '#00D66F',
  RED: '#FF4444',
  YELLOW: '#FFD700',
};

interface VerdictPanelProps {
  verdict: InvestmentVerdict;
}

export const VerdictPanel: React.FC<VerdictPanelProps> = ({ verdict }) => {
  const categoryConfig = getCategoryConfig(verdict.category);

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `2px solid ${categoryConfig.borderColor}`,
      borderRadius: '2px',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* Header */}
      <div style={{
        padding: '16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${categoryConfig.borderColor}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: categoryConfig.textColor, letterSpacing: '0.5px' }}>
            {verdict.category}
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px' }}>
            {verdict.analysis_summary}
          </div>
        </div>
        <div style={{ fontSize: '20px', fontWeight: 700, color: categoryConfig.textColor }}>
          {verdict.rating}
        </div>
      </div>

      {/* Content */}
      <div style={{ padding: '16px' }}>
        {/* Key Findings */}
        {verdict.key_findings && verdict.key_findings.length > 0 && (
          <div style={{ marginBottom: '16px' }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              KEY FINDINGS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {verdict.key_findings.map((finding, index) => (
                <div key={index} style={{
                  fontSize: '10px',
                  color: FINCEPT.WHITE,
                  paddingLeft: '8px',
                  borderLeft: `2px solid ${FINCEPT.BORDER}`,
                }}>
                  {finding}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Recommendation */}
        <div style={{
          padding: '12px',
          backgroundColor: categoryConfig.bgColor,
          border: `1px solid ${categoryConfig.borderColor}`,
          borderRadius: '2px',
          marginBottom: '16px',
        }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            marginBottom: '8px',
          }}>
            RECOMMENDATION
          </div>
          <div style={{ fontSize: '10px', color: FINCEPT.WHITE }}>
            {verdict.recommendation}
          </div>
        </div>

        {/* When Acceptable */}
        {verdict.when_acceptable && (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            marginBottom: '16px',
          }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.YELLOW,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              WHEN IT MIGHT BE ACCEPTABLE
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              {verdict.when_acceptable}
            </div>
          </div>
        )}

        {/* Better Alternatives */}
        {verdict.better_alternatives && verdict.better_alternatives.benefits && (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
          }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GREEN,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              BETTER ALTERNATIVES
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.WHITE, marginBottom: '8px', fontWeight: 600 }}>
              {verdict.better_alternatives.portfolio}
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {verdict.better_alternatives.benefits.map((benefit, index) => (
                <div key={index} style={{
                  fontSize: '9px',
                  color: FINCEPT.GRAY,
                  paddingLeft: '8px',
                  borderLeft: `2px solid ${FINCEPT.GREEN}`,
                }}>
                  {benefit}
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

function getCategoryConfig(category: string) {
  const configs = {
    'THE GOOD': {
      borderColor: FINCEPT.GREEN,
      bgColor: `${FINCEPT.GREEN}10`,
      textColor: FINCEPT.GREEN,
    },
    'THE BAD': {
      borderColor: FINCEPT.ORANGE,
      bgColor: `${FINCEPT.ORANGE}10`,
      textColor: FINCEPT.ORANGE,
    },
    'THE FLAWED': {
      borderColor: FINCEPT.YELLOW,
      bgColor: `${FINCEPT.YELLOW}10`,
      textColor: FINCEPT.YELLOW,
    },
    'THE UGLY': {
      borderColor: FINCEPT.RED,
      bgColor: `${FINCEPT.RED}10`,
      textColor: FINCEPT.RED,
    },
    MIXED: {
      borderColor: FINCEPT.CYAN,
      bgColor: `${FINCEPT.CYAN}10`,
      textColor: FINCEPT.CYAN,
    },
  };

  return configs[category as keyof typeof configs] || configs.MIXED;
}

export default VerdictPanel;
