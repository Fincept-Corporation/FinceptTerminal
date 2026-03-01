/**
 * GsQuantPanel - Sub-components: Tooltip, InputField, HowToUsePanel
 */

import React, { useState } from 'react';
import { HelpCircle, Info } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import type { AnalysisMode } from './GsQuantPanelTypes';

const GS_PURPLE = '#6F42C1';

// Tooltip component for help text
export interface TooltipProps {
  text: string;
  children: React.ReactNode;
}

export function Tooltip({ text, children }: TooltipProps) {
  const [show, setShow] = useState(false);
  const [position, setPosition] = useState({ top: 0, left: 0 });
  const triggerRef = React.useRef<HTMLDivElement>(null);
  const { colors } = useTerminalTheme();

  const handleMouseEnter = () => {
    if (triggerRef.current) {
      const rect = triggerRef.current.getBoundingClientRect();
      setPosition({
        top: rect.top - 10,
        left: rect.left + rect.width / 2,
      });
    }
    setShow(true);
  };

  return (
    <div
      ref={triggerRef}
      style={{ position: 'relative', display: 'inline-flex', alignItems: 'center' }}
      onMouseEnter={handleMouseEnter}
      onMouseLeave={() => setShow(false)}
    >
      {children}
      {show && (
        <div
          style={{
            position: 'fixed',
            zIndex: 9999,
            padding: '10px 14px',
            backgroundColor: '#1a1a2e',
            border: `1px solid ${GS_PURPLE}60`,
            borderRadius: '6px',
            color: colors.text,
            fontSize: '12px',
            lineHeight: 1.5,
            whiteSpace: 'normal',
            width: '260px',
            textAlign: 'left',
            boxShadow: '0 8px 24px rgba(0,0,0,0.5)',
            top: position.top,
            left: position.left,
            transform: 'translate(-50%, -100%)',
          }}
        >
          {text}
        </div>
      )}
    </div>
  );
}

// Input field with label, unit, help tooltip, and example
export interface InputFieldProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  unit?: string;
  help?: string;
  example?: string;
  placeholder?: string;
  type?: 'text' | 'number' | 'textarea';
  rows?: number;
  style?: React.CSSProperties;
}

export function InputField({ label, value, onChange, unit, help, example, placeholder, type = 'text', rows, style }: InputFieldProps) {
  const { colors } = useTerminalTheme();

  const inputStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: `1px solid #333`,
    color: colors.text,
    padding: '8px 10px',
    paddingRight: unit ? '40px' : '10px',
    fontSize: '13px',
    fontFamily: 'monospace',
    width: '100%',
    outline: 'none',
    borderRadius: '2px',
  };

  const labelStyle: React.CSSProperties = {
    color: colors.textMuted,
    fontSize: '11px',
    fontWeight: 600,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
    marginBottom: '6px',
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
  };

  return (
    <div style={style}>
      <div style={labelStyle}>
        <span>{label}</span>
        {unit && <span style={{ color: GS_PURPLE, fontWeight: 400 }}>({unit})</span>}
        {help && (
          <Tooltip text={help}>
            <HelpCircle size={12} color={colors.textMuted} style={{ cursor: 'help' }} />
          </Tooltip>
        )}
      </div>
      <div style={{ position: 'relative' }}>
        {type === 'textarea' ? (
          <textarea
            value={value}
            onChange={e => onChange(e.target.value)}
            rows={rows || 3}
            style={{ ...inputStyle, resize: 'vertical' }}
            placeholder={placeholder || example}
          />
        ) : (
          <input
            type={type}
            value={value}
            onChange={e => onChange(e.target.value)}
            style={inputStyle}
            placeholder={placeholder || example}
          />
        )}
        {unit && type !== 'textarea' && (
          <span
            style={{
              position: 'absolute',
              right: '10px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: colors.textMuted,
              fontSize: '11px',
              pointerEvents: 'none',
            }}
          >
            {unit}
          </span>
        )}
      </div>
      {example && (
        <div style={{ color: colors.textMuted, fontSize: '10px', marginTop: '4px', fontStyle: 'italic' }}>
          Example: {example}
        </div>
      )}
    </div>
  );
}

// Comprehensive "How to Use" panel with input guide and output preview
export interface HowToUsePanelProps {
  mode: AnalysisMode;
}

export function HowToUsePanel({ mode }: HowToUsePanelProps) {
  const { colors } = useTerminalTheme();
  const [expanded, setExpanded] = useState(true);

  const guides: Record<AnalysisMode, {
    title: string;
    whatIsIt: string;
    whereToGetData: string[];
    whatYouWillGet: string[];
    exampleUseCase: string;
  }> = {
    risk_metrics: {
      title: 'Risk Metrics Analysis',
      whatIsIt: 'Calculate how risky your investment is based on its historical price movements.',
      whereToGetData: [
        'Go to Yahoo Finance, enter a stock ticker (e.g., AAPL)',
        'Download historical prices (1 year of daily data recommended)',
        'Calculate daily returns: (Today\'s Price - Yesterday\'s Price) / Yesterday\'s Price × 100',
        'Or use our Equity Research tab to get returns automatically',
      ],
      whatYouWillGet: [
        'Sharpe Ratio - Risk-adjusted return (>1 is good, >2 is excellent)',
        'Sortino Ratio - Like Sharpe but only penalizes downside risk',
        'Max Drawdown - Largest peak-to-trough decline (worst case loss)',
        'Volatility - How much the price swings (annualized)',
        'VaR 95%/99% - Maximum expected loss at 95%/99% confidence',
      ],
      exampleUseCase: 'You want to compare AAPL vs MSFT - which stock has better risk-adjusted returns?',
    },
    portfolio: {
      title: 'Portfolio vs Benchmark Analysis',
      whatIsIt: 'Compare how your portfolio performs against a benchmark like S&P 500.',
      whereToGetData: [
        'Your Portfolio Returns: Calculate daily % change of your portfolio value',
        'Benchmark Returns: Get S&P 500 (^GSPC) daily returns from Yahoo Finance',
        'Both should cover the same date range',
        'Make sure both have the same number of data points',
      ],
      whatYouWillGet: [
        'Alpha - Excess return over benchmark (positive = outperforming)',
        'Beta - Sensitivity to market movements (1 = moves with market)',
        'Information Ratio - Skill of active management (>0.5 is good)',
        'Tracking Error - How much your returns deviate from benchmark',
        'Up/Down Capture - Performance in up vs down markets',
      ],
      exampleUseCase: 'Is your stock picks portfolio beating the S&P 500 after adjusting for risk?',
    },
    greeks: {
      title: 'Options Greeks Calculator',
      whatIsIt: 'Calculate option price sensitivities using Black-Scholes model.',
      whereToGetData: [
        'Spot Price: Current stock price (e.g., AAPL at $175)',
        'Strike Price: The exercise price of your option',
        'Expiry: Time until expiration in months',
        'Volatility: Look up IV on options chain or use historical vol (20-40% typical)',
        'Risk-Free Rate: Current T-bill rate (~5% as of 2024)',
      ],
      whatYouWillGet: [
        'Delta - How much option price moves per $1 stock move',
        'Gamma - Rate of change of Delta',
        'Theta - Time decay per day (usually negative)',
        'Vega - Sensitivity to volatility changes',
        'Rho - Sensitivity to interest rate changes',
        'Option Price - Theoretical fair value',
      ],
      exampleUseCase: 'You\'re buying AAPL $180 calls expiring in 3 months. What\'s the fair price and Greeks?',
    },
    var_analysis: {
      title: 'Value at Risk (VaR) Analysis',
      whatIsIt: 'Estimate the maximum potential loss over a time period at a confidence level.',
      whereToGetData: [
        'Historical daily returns of your portfolio/asset',
        'Position value: Total $ amount invested',
        'Confidence level: 95% (standard) or 99% (conservative)',
      ],
      whatYouWillGet: [
        'Parametric VaR - Assumes normal distribution of returns',
        'Historical VaR - Based on actual historical percentiles',
        'Monte Carlo VaR - Simulated using 10,000 scenarios',
        'CVaR (Expected Shortfall) - Average loss when VaR is exceeded',
        'Dollar VaR - Actual $ amount at risk',
      ],
      exampleUseCase: 'You have $1M in stocks. What\'s the worst-case daily loss you should expect 95% of the time?',
    },
    stress_test: {
      title: 'Stress Test Scenarios',
      whatIsIt: 'See how your portfolio would perform in historical crisis events.',
      whereToGetData: [
        'Your portfolio\'s historical daily returns',
        'Total position value in $',
        'The system applies 9 built-in stress scenarios automatically',
      ],
      whatYouWillGet: [
        '2008 Financial Crisis scenario',
        'COVID-19 March 2020 crash',
        'Interest Rate Shock (+200bps)',
        'Equity Market Crash (-20%)',
        'Currency Crisis scenario',
        'Commodity Shock scenario',
        'And 3 more stress scenarios',
      ],
      exampleUseCase: 'If 2008 happens again, how much would your current portfolio lose?',
    },
    backtest: {
      title: 'Strategy Backtesting',
      whatIsIt: 'Test how different trading strategies would have performed historically.',
      whereToGetData: [
        'Historical returns of the asset you want to trade',
        'Choose initial capital amount',
        'Select strategy: Buy & Hold, Momentum, Mean Reversion, or Rebalancing',
        'Set lookback period for signal calculation',
      ],
      whatYouWillGet: [
        'Total Return - Overall % gain/loss',
        'Sharpe Ratio - Risk-adjusted performance',
        'Max Drawdown - Worst peak-to-trough decline',
        'Win Rate - % of profitable trades',
        'Comparison across strategies',
      ],
      exampleUseCase: 'Would a 20-day momentum strategy beat buy-and-hold for Bitcoin?',
    },
    statistics: {
      title: 'Descriptive Statistics',
      whatIsIt: 'Calculate comprehensive statistics for any numeric dataset.',
      whereToGetData: [
        'Any comma-separated numbers (returns, prices, volumes, etc.)',
        'For returns analysis, enter daily % changes',
        'For price analysis, enter raw prices',
        'More data points = more reliable statistics',
      ],
      whatYouWillGet: [
        'Mean, Median, Mode - Central tendency measures',
        'Standard Deviation, Variance - Spread measures',
        'Skewness - Distribution asymmetry (negative = more losses)',
        'Kurtosis - Tail thickness (high = more extreme events)',
        'Percentiles (1st, 5th, 25th, 50th, 75th, 95th, 99th)',
        'Min, Max, Range',
      ],
      exampleUseCase: 'Analyze the distribution of your trading returns to understand if they\'re normally distributed.',
    },
  };

  const guide = guides[mode];

  return (
    <div
      style={{
        backgroundColor: `${GS_PURPLE}08`,
        border: `1px solid ${GS_PURPLE}25`,
        borderRadius: '6px',
        marginBottom: '16px',
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
          padding: '12px 16px',
          backgroundColor: `${GS_PURPLE}15`,
          cursor: 'pointer',
          borderBottom: expanded ? `1px solid ${GS_PURPLE}20` : 'none',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <Info size={16} color={GS_PURPLE} />
        <span style={{ color: colors.text, fontWeight: 600, fontSize: '13px', flex: 1 }}>
          {guide.title}
        </span>
        <span style={{ color: colors.textMuted, fontSize: '11px' }}>
          {expanded ? '▼ Hide guide' : '▶ How to use this'}
        </span>
      </div>

      {expanded && (
        <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '16px', maxHeight: '300px', overflowY: 'scroll' }}>
          {/* What is it */}
          <div style={{ color: colors.text, fontSize: '12px', lineHeight: 1.5 }}>
            {guide.whatIsIt}
          </div>

          {/* Two columns: Input guide and Output preview */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
            {/* Where to get data */}
            <div style={{ backgroundColor: `${colors.background}`, border: `1px solid #333`, borderRadius: '4px', padding: '12px' }}>
              <div style={{ color: GS_PURPLE, fontSize: '11px', fontWeight: 600, marginBottom: '8px', textTransform: 'uppercase' }}>
                Where to Get the Data
              </div>
              <ol style={{ margin: 0, paddingLeft: '18px', color: colors.textMuted, fontSize: '11px', lineHeight: 1.6 }}>
                {guide.whereToGetData.map((item, i) => (
                  <li key={i} style={{ marginBottom: '4px' }}>{item}</li>
                ))}
              </ol>
            </div>

            {/* What you will get */}
            <div style={{ backgroundColor: `${colors.background}`, border: `1px solid #333`, borderRadius: '4px', padding: '12px' }}>
              <div style={{ color: GS_PURPLE, fontSize: '11px', fontWeight: 600, marginBottom: '8px', textTransform: 'uppercase' }}>
                Output You Will Get
              </div>
              <ul style={{ margin: 0, paddingLeft: '18px', color: colors.textMuted, fontSize: '11px', lineHeight: 1.6 }}>
                {guide.whatYouWillGet.map((item, i) => (
                  <li key={i} style={{ marginBottom: '4px' }}>{item}</li>
                ))}
              </ul>
            </div>
          </div>

          {/* Example use case */}
          <div style={{
            backgroundColor: `${GS_PURPLE}10`,
            padding: '10px 12px',
            borderRadius: '4px',
            borderLeft: `3px solid ${GS_PURPLE}`,
            flexShrink: 0,
          }}>
            <div style={{ color: GS_PURPLE, fontSize: '10px', fontWeight: 600, textTransform: 'uppercase', marginBottom: '4px' }}>
              Example Use Case:
            </div>
            <div style={{ color: colors.text, fontSize: '11px', lineHeight: 1.5 }}>
              {guide.exampleUseCase}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
