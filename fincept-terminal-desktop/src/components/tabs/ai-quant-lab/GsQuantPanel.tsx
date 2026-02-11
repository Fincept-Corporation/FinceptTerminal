/**
 * GS Quant Panel - Goldman Sachs Quant Analytics
 * Covers: Risk Metrics, Portfolio Analytics, Greeks, VaR, Stress Testing, Backtesting, Statistics
 *
 * User-friendly interface with:
 * - Clear input labels with units (%, $, months)
 * - Tooltips explaining each field
 * - Example values for guidance
 * - Percentage inputs converted to decimals internally
 */

import React, { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Activity,
  TrendingUp,
  BarChart2,
  Calculator,
  RefreshCw,
  AlertCircle,
  Play,
  Shield,
  Target,
  Zap,
  PieChart,
  LineChart,
  BarChart3,
  HelpCircle,
  Info,
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

const GS_PURPLE = '#6F42C1';

// Tooltip component for help text
interface TooltipProps {
  text: string;
  children: React.ReactNode;
}

function Tooltip({ text, children }: TooltipProps) {
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
interface InputFieldProps {
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

function InputField({ label, value, onChange, unit, help, example, placeholder, type = 'text', rows, style }: InputFieldProps) {
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
interface HowToUsePanelProps {
  mode: AnalysisMode;
}

function HowToUsePanel({ mode }: HowToUsePanelProps) {
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

type AnalysisMode = 'risk_metrics' | 'portfolio' | 'greeks' | 'var_analysis' | 'stress_test' | 'backtest' | 'statistics';

interface ModeConfig {
  id: AnalysisMode;
  label: string;
  icon: React.ElementType;
  description: string;
  detailedDescription: string;
  tips: string[];
}

const MODES: ModeConfig[] = [
  {
    id: 'risk_metrics',
    label: 'Risk Metrics',
    icon: Shield,
    description: 'Calculate volatility, drawdown, Sharpe, Sortino, and VaR ratios',
    detailedDescription: 'Analyze the risk profile of your portfolio or asset returns. Enter historical returns to calculate key risk metrics used by professional investors.',
    tips: [
      'Enter daily returns as percentages (e.g., 1.5 for +1.5%, -0.8 for -0.8%)',
      'More data points = more accurate metrics (aim for 20+ returns)',
      'Risk-free rate is the annual rate (e.g., 5 for 5% annual T-bill rate)',
      'Sharpe ratio > 1 is good, > 2 is excellent',
    ],
  },
  {
    id: 'portfolio',
    label: 'Portfolio Analytics',
    icon: PieChart,
    description: 'Compare portfolio vs benchmark: alpha, tracking error, information ratio',
    detailedDescription: 'Measure how your portfolio performs relative to a benchmark. Calculates alpha, beta, capture ratios, and other relative performance metrics.',
    tips: [
      'Enter both portfolio and benchmark returns for the same time periods',
      'Returns should be in the same format (daily, weekly, or monthly)',
      'Alpha > 0 means you\'re outperforming the benchmark',
      'Information ratio > 0.5 indicates skilled active management',
    ],
  },
  {
    id: 'greeks',
    label: 'Options Greeks',
    icon: Calculator,
    description: 'Calculate Delta, Gamma, Vega, Theta, Rho and higher-order Greeks',
    detailedDescription: 'Compute option sensitivities using Black-Scholes model. Essential for options traders to understand position risk and manage hedging.',
    tips: [
      'Spot & Strike prices should be in the same currency (e.g., $100)',
      'Expiry is in months (e.g., 3 for 3-month option)',
      'Volatility is annualized implied volatility in % (e.g., 25 for 25%)',
      'Delta ranges from 0-1 for calls, 0 to -1 for puts',
    ],
  },
  {
    id: 'var_analysis',
    label: 'VaR Analysis',
    icon: Target,
    description: 'Value at Risk: parametric, historical, and Monte Carlo methods',
    detailedDescription: 'Calculate potential portfolio loss using multiple VaR methodologies. Includes Conditional VaR (CVaR) for tail risk analysis.',
    tips: [
      '95% confidence = expect loss to exceed VaR ~5% of the time',
      '99% confidence is more conservative (used by banks)',
      'Monte Carlo VaR uses 10,000 simulations for accuracy',
      'CVaR shows average loss when VaR is exceeded',
    ],
  },
  {
    id: 'stress_test',
    label: 'Stress Test',
    icon: Zap,
    description: 'Simulate portfolio impact across 9 market stress scenarios',
    detailedDescription: 'Test how your portfolio would perform in historical crisis scenarios like 2008 Financial Crisis, COVID crash, and interest rate shocks.',
    tips: [
      'Scenarios are based on historical market events',
      'Position value is your total portfolio value in $',
      'Shows both % drawdown and $ loss for each scenario',
      'Use this to understand worst-case risks',
    ],
  },
  {
    id: 'backtest',
    label: 'Backtest',
    icon: BarChart3,
    description: 'Test trading strategies: buy-and-hold, momentum, mean-reversion',
    detailedDescription: 'Backtest common trading strategies against historical data. Compare returns, Sharpe ratios, and drawdowns across different approaches.',
    tips: [
      'Returns are converted to prices internally (starting at $100)',
      'Lookback period is used for momentum/mean-reversion signals',
      'Compare strategies to find what works best for your data',
      'Past performance doesn\'t guarantee future results!',
    ],
  },
  {
    id: 'statistics',
    label: 'Statistics',
    icon: LineChart,
    description: 'Descriptive statistics: mean, std dev, skewness, kurtosis, percentiles',
    detailedDescription: 'Calculate comprehensive descriptive statistics for any dataset. Includes distribution shape analysis and percentile breakdowns.',
    tips: [
      'Enter any numeric values separated by commas',
      'Negative skewness = more extreme losses than gains',
      'High kurtosis = fat tails (more extreme events)',
      'Percentiles show value distribution breakdown',
    ],
  },
];

export function GsQuantPanel() {
  const { colors } = useTerminalTheme();
  const [mode, setMode] = useState<AnalysisMode>('risk_metrics');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<Record<string, any> | null>(null);

  // Common form state - VALUES ARE IN USER-FRIENDLY PERCENTAGES
  // Returns as percentages (1.0 = 1%, -0.5 = -0.5%)
  const [returnsInput, setReturnsInput] = useState('1.0, -0.5, 0.8, -0.3, 1.2, -0.7, 0.6, 0.9, -0.4, 1.1, -0.2, 0.7, -0.6, 1.3, -0.1, 0.5, -0.8, 1.0, 0.3, -0.9');
  const [benchmarkInput, setBenchmarkInput] = useState('0.8, -0.3, 0.6, -0.2, 0.9, -0.5, 0.4, 0.7, -0.3, 0.8, -0.1, 0.5, -0.4, 1.0, -0.2, 0.3, -0.6, 0.7, 0.2, -0.7');
  // Risk-free rate as percentage (2 = 2% annual)
  const [riskFreeRate, setRiskFreeRate] = useState('2');
  // Confidence as percentage (95 = 95%)
  const [confidence, setConfidence] = useState('95');
  const [positionValue, setPositionValue] = useState('1000000');

  // Greeks form - expiry in months, rates/vol in percentages
  const [spot, setSpot] = useState('100');
  const [strike, setStrike] = useState('100');
  // Expiry in months (3 = 3 months)
  const [expiry, setExpiry] = useState('3');
  // Rate as percentage (5 = 5%)
  const [rate, setRate] = useState('5');
  // Volatility as percentage (20 = 20%)
  const [vol, setVol] = useState('20');
  const [optionType, setOptionType] = useState('call');

  // Backtest form
  const [strategy, setStrategy] = useState('buy_and_hold');
  const [initialCapital, setInitialCapital] = useState('100000');
  const [lookback, setLookback] = useState('20');

  // Helper to convert user-friendly percentage to decimal
  const pctToDecimal = (pct: string): number => {
    const val = parseFloat(pct);
    return isNaN(val) ? 0 : val / 100;
  };

  // Helper to convert months to years
  const monthsToYears = (months: string): number => {
    const val = parseFloat(months);
    return isNaN(val) ? 0.25 : val / 12;
  };

  // Parse comma-separated values, converting from % to decimal
  const parseFloatArrayAsDecimal = (input: string): number[] => {
    return input.split(',')
      .map(s => parseFloat(s.trim()))
      .filter(n => !isNaN(n))
      .map(n => n / 100); // Convert percentage to decimal (1% -> 0.01)
  };

  // Parse comma-separated values as-is (for statistics mode)
  const parseFloatArray = (input: string): number[] => {
    return input.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
  };

  const runAnalysis = useCallback(async () => {
    setLoading(true);
    setError(null);
    setResult(null);

    try {
      let response: string;
      // Convert user-friendly percentage returns to decimals for API
      const returns = parseFloatArrayAsDecimal(returnsInput);
      const benchmark = parseFloatArrayAsDecimal(benchmarkInput);
      // Convert user-friendly percentage risk-free rate to decimal
      const riskFreeRateDecimal = pctToDecimal(riskFreeRate);

      switch (mode) {
        case 'risk_metrics':
          response = await invoke<string>('gs_quant_risk_metrics', {
            returns,
            riskFreeRate: riskFreeRateDecimal,
          });
          break;

        case 'portfolio':
          response = await invoke<string>('gs_quant_portfolio_analytics', {
            returns,
            benchmarkReturns: benchmark,
            riskFreeRate: riskFreeRateDecimal,
          });
          break;

        case 'greeks':
          response = await invoke<string>('gs_quant_greeks', {
            spot: parseFloat(spot),
            strike: parseFloat(strike),
            expiry: monthsToYears(expiry), // Convert months to years
            rate: pctToDecimal(rate), // Convert % to decimal
            vol: pctToDecimal(vol), // Convert % to decimal
            optionType,
          });
          break;

        case 'var_analysis':
          response = await invoke<string>('gs_quant_var_analysis', {
            returns,
            confidence: pctToDecimal(confidence) || 0.95, // Convert 95 -> 0.95
            positionValue: parseFloat(positionValue) || 1000000,
          });
          break;

        case 'stress_test':
          response = await invoke<string>('gs_quant_stress_test', {
            returns,
            positionValue: parseFloat(positionValue) || 1000000,
          });
          break;

        case 'backtest': {
          const pricesArr = returns.length > 0
            ? returns.reduce((acc: number[], r, i) => {
                acc.push(i === 0 ? 100 : acc[i - 1] * (1 + r));
                return acc;
              }, [])
            : [100];
          response = await invoke<string>('gs_quant_backtest', {
            prices: JSON.stringify({ asset: pricesArr }),
            strategy,
            initialCapital: parseFloat(initialCapital) || 100000,
            lookback: parseInt(lookback) || 20,
          });
          break;
        }

        case 'statistics':
          // For statistics, use raw values (not percentages)
          response = await invoke<string>('gs_quant_statistics', {
            values: parseFloatArray(returnsInput),
          });
          break;

        default:
          throw new Error(`Unknown mode: ${mode}`);
      }

      const parsed = JSON.parse(response);
      if (parsed.error) {
        setError(parsed.error);
      } else {
        setResult(parsed);
      }
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally {
      setLoading(false);
    }
  }, [mode, returnsInput, benchmarkInput, riskFreeRate, confidence, positionValue, spot, strike, expiry, rate, vol, optionType, strategy, initialCapital, lookback]);

  const inputStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: `1px solid ${'#333'}`,
    color: colors.text,
    padding: '6px 10px',
    fontSize: '12px',
    fontFamily: 'monospace',
    width: '100%',
    outline: 'none',
  };

  const labelStyle: React.CSSProperties = {
    color: colors.textMuted,
    fontSize: '10px',
    fontWeight: 600,
    textTransform: 'uppercase' as const,
    letterSpacing: '0.5px',
    marginBottom: '4px',
  };

  const cardStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    border: `1px solid ${'#333'}`,
    padding: '12px',
  };

  const renderCommonInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <InputField
        label="Daily Returns"
        value={returnsInput}
        onChange={setReturnsInput}
        unit="%"
        help="Enter daily returns as percentages. Positive = gain, negative = loss."
        example="1.5, -0.8, 2.1, -1.2 (representing +1.5%, -0.8%, +2.1%, -1.2%)"
        type="textarea"
        rows={3}
        placeholder="1.0, -0.5, 0.8, -0.3, ..."
      />
      {(mode === 'portfolio') && (
        <InputField
          label="Benchmark Returns"
          value={benchmarkInput}
          onChange={setBenchmarkInput}
          unit="%"
          help="Enter benchmark (e.g., S&P 500) returns for the same time periods."
          example="0.9, -0.4, 1.8, -0.9"
          type="textarea"
          rows={2}
          placeholder="0.8, -0.3, 0.6, -0.2, ..."
        />
      )}
      {['risk_metrics', 'portfolio'].includes(mode) && (
        <InputField
          label="Risk-Free Rate"
          value={riskFreeRate}
          onChange={setRiskFreeRate}
          unit="% annual"
          help="Annual risk-free rate, typically T-bill or savings rate."
          example="2 for 2% annual rate, 5 for 5%"
          placeholder="2"
        />
      )}
      {['var_analysis', 'stress_test'].includes(mode) && (
        <div style={{ display: 'flex', gap: '12px' }}>
          <InputField
            label="Confidence Level"
            value={confidence}
            onChange={setConfidence}
            unit="%"
            help="VaR confidence level. 95% means loss exceeds VaR only 5% of the time."
            example="95 for 95% confidence, 99 for 99%"
            placeholder="95"
            style={{ flex: 1 }}
          />
          <InputField
            label="Position Value"
            value={positionValue}
            onChange={setPositionValue}
            unit="$"
            help="Total value of your position or portfolio in dollars."
            example="1000000 for $1 million"
            placeholder="1000000"
            style={{ flex: 1 }}
          />
        </div>
      )}
    </div>
  );

  const renderGreeksInputs = () => (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
      <InputField
        label="Spot Price"
        value={spot}
        onChange={setSpot}
        unit="$"
        help="Current price of the underlying asset."
        example="100 for $100"
        placeholder="100"
      />
      <InputField
        label="Strike Price"
        value={strike}
        onChange={setStrike}
        unit="$"
        help="Option exercise price."
        example="105 for $105 strike"
        placeholder="100"
      />
      <InputField
        label="Time to Expiry"
        value={expiry}
        onChange={setExpiry}
        unit="months"
        help="Time until option expiration in months."
        example="3 for 3-month option, 12 for 1-year"
        placeholder="3"
      />
      <InputField
        label="Risk-Free Rate"
        value={rate}
        onChange={setRate}
        unit="% annual"
        help="Annual risk-free interest rate."
        example="5 for 5% annual rate"
        placeholder="5"
      />
      <InputField
        label="Implied Volatility"
        value={vol}
        onChange={setVol}
        unit="% annual"
        help="Annualized implied volatility of the underlying."
        example="20 for 20% volatility, 35 for higher vol stocks"
        placeholder="20"
      />
      <div>
        <div style={labelStyle}>
          <span>Option Type</span>
          <Tooltip text="Call = right to buy, Put = right to sell">
            <HelpCircle size={12} color={colors.textMuted} style={{ cursor: 'help' }} />
          </Tooltip>
        </div>
        <select value={optionType} onChange={e => setOptionType(e.target.value)} style={inputStyle}>
          <option value="call">Call (right to buy)</option>
          <option value="put">Put (right to sell)</option>
        </select>
      </div>
    </div>
  );

  const renderBacktestInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <InputField
        label="Historical Returns"
        value={returnsInput}
        onChange={setReturnsInput}
        unit="%"
        help="Daily returns to backtest against. Will be converted to price series."
        example="1.0, -0.5, 0.8, -0.3 (representing +1%, -0.5%, +0.8%, -0.3%)"
        type="textarea"
        rows={3}
        placeholder="1.0, -0.5, 0.8, -0.3, ..."
      />
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
        <div>
          <div style={labelStyle}>
            <span>Strategy</span>
            <Tooltip text="Trading strategy to backtest against historical data">
              <HelpCircle size={12} color={colors.textMuted} style={{ cursor: 'help' }} />
            </Tooltip>
          </div>
          <select value={strategy} onChange={e => setStrategy(e.target.value)} style={inputStyle}>
            <option value="buy_and_hold">Buy & Hold (baseline)</option>
            <option value="momentum">Momentum (trend following)</option>
            <option value="mean_reversion">Mean Reversion (contrarian)</option>
            <option value="rebalancing">Rebalancing (periodic)</option>
          </select>
        </div>
        <InputField
          label="Initial Capital"
          value={initialCapital}
          onChange={setInitialCapital}
          unit="$"
          help="Starting capital for the backtest simulation."
          example="100000 for $100K, 1000000 for $1M"
          placeholder="100000"
        />
        <InputField
          label="Lookback Period"
          value={lookback}
          onChange={setLookback}
          unit="days"
          help="Number of days to look back for momentum/mean-reversion signals."
          example="20 for 20-day moving average"
          placeholder="20"
        />
      </div>
    </div>
  );

  const renderMetricRow = (label: string, value: any, color?: string) => {
    const displayValue = typeof value === 'number'
      ? (Math.abs(value) < 0.01 && value !== 0 ? value.toExponential(4) : value.toFixed(4))
      : String(value ?? 'N/A');
    const isNeg = typeof value === 'number' && value < 0;
    return (
      <div key={label} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '4px 0', borderBottom: `1px solid ${'#222'}` }}>
        <span style={{ color: colors.textMuted, fontSize: '11px' }}>{label}</span>
        <span style={{ color: color || (isNeg ? colors.alert : colors.success), fontSize: '12px', fontFamily: 'monospace', fontWeight: 600 }}>{displayValue}</span>
      </div>
    );
  };

  const renderResults = () => {
    if (!result) return null;

    if (mode === 'greeks') {
      const greekNames = ['delta', 'gamma', 'vega', 'theta', 'rho', 'vanna', 'volga', 'charm', 'speed', 'zomma', 'color'];
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>GREEKS RESULT</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {greekNames.map(g => {
              const val = result[g];
              if (val === undefined || val === null) return null;
              return renderMetricRow(g.charAt(0).toUpperCase() + g.slice(1), val, GS_PURPLE);
            })}
            {result.price !== undefined && renderMetricRow('Option Price', result.price, colors.success)}
          </div>
        </div>
      );
    }

    if (mode === 'risk_metrics') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>RISK METRICS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {renderMetricRow('Ann. Volatility', result.volatility_annualized)}
            {renderMetricRow('Daily Risk', result.daily_risk)}
            {renderMetricRow('Downside Risk', result.downside_risk)}
            {renderMetricRow('Max Drawdown', result.max_drawdown)}
            {renderMetricRow('Max DD Length', result.max_drawdown_length, colors.text)}
            {renderMetricRow('Recovery Period', result.max_recovery_period, colors.text)}
            {renderMetricRow('Sharpe Ratio', result.sharpe_ratio)}
            {renderMetricRow('Sortino Ratio', result.sortino_ratio)}
            {renderMetricRow('Calmar Ratio', result.calmar_ratio)}
            {renderMetricRow('Omega Ratio', result.omega_ratio)}
            {renderMetricRow('VaR (95%)', result.var_95)}
            {renderMetricRow('VaR (99%)', result.var_99)}
          </div>
        </div>
      );
    }

    if (mode === 'portfolio') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>PORTFOLIO ANALYTICS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {renderMetricRow('Alpha', result.alpha)}
            {renderMetricRow('Jensen Alpha', result.jensen_alpha)}
            {renderMetricRow('Bull Alpha', result.jensen_alpha_bull)}
            {renderMetricRow('Bear Alpha', result.jensen_alpha_bear)}
            {renderMetricRow('Sharpe Ratio', result.sharpe_ratio)}
            {renderMetricRow('Sortino Ratio', result.sortino_ratio)}
            {renderMetricRow('Calmar Ratio', result.calmar_ratio)}
            {renderMetricRow('Treynor', result.treynor_measure)}
            {renderMetricRow('Modigliani (M2)', result.modigliani_ratio)}
            {renderMetricRow('Tracking Error', result.tracking_error)}
            {renderMetricRow('Info Ratio', result.information_ratio)}
            {renderMetricRow('R-squared', result.r_squared, colors.text)}
            {renderMetricRow('Hit Rate', result.hit_rate, colors.text)}
            {renderMetricRow('Up Capture', result.up_capture, colors.text)}
            {renderMetricRow('Down Capture', result.down_capture, colors.text)}
            {renderMetricRow('Max Drawdown', result.max_drawdown)}
            {renderMetricRow('Skewness', result.skewness, colors.text)}
            {renderMetricRow('Kurtosis', result.kurtosis, colors.text)}
          </div>
        </div>
      );
    }

    if (mode === 'var_analysis') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>VALUE AT RISK</div>
          {['parametric_var', 'historical_var', 'monte_carlo_var', 'cvar'].map(key => {
            const section = result[key];
            if (!section || typeof section !== 'object') return null;
            const displayName = key.replace(/_/g, ' ').toUpperCase();
            return (
              <div key={key} style={{ marginBottom: '12px' }}>
                <div style={{ color: GS_PURPLE, fontSize: '11px', fontWeight: 600, marginBottom: '4px' }}>{displayName}</div>
                {Object.entries(section).map(([k, v]) => renderMetricRow(k.replace(/_/g, ' '), v as number))}
              </div>
            );
          })}
        </div>
      );
    }

    if (mode === 'stress_test') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>STRESS TEST SCENARIOS</div>
          {Object.entries(result).map(([scenarioName, scenarioData]) => (
            <div key={scenarioName} style={{ marginBottom: '12px', padding: '8px', border: `1px solid ${'#333'}` }}>
              <div style={{ color: GS_PURPLE, fontSize: '11px', fontWeight: 600, marginBottom: '4px' }}>
                {scenarioName.replace(/_/g, ' ').toUpperCase()}
              </div>
              {typeof scenarioData === 'object' && scenarioData !== null
                ? Object.entries(scenarioData as Record<string, any>).map(([k, v]) =>
                    renderMetricRow(k.replace(/_/g, ' '), v as number)
                  )
                : <div style={{ color: colors.text, fontSize: '11px' }}>{String(scenarioData)}</div>
              }
            </div>
          ))}
        </div>
      );
    }

    if (mode === 'backtest') {
      const metrics = result.metrics || result;
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>BACKTEST RESULTS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {Object.entries(metrics).map(([k, v]) => {
              if (typeof v === 'object') return null;
              return renderMetricRow(k.replace(/_/g, ' '), v as number);
            })}
          </div>
        </div>
      );
    }

    if (mode === 'statistics') {
      return (
        <div style={cardStyle}>
          <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>DESCRIPTIVE STATISTICS</div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 16px' }}>
            {renderMetricRow('Mean', result.mean)}
            {renderMetricRow('Median', result.median)}
            {renderMetricRow('Std Dev', result.std)}
            {renderMetricRow('Variance', result.variance)}
            {renderMetricRow('Min', result.min)}
            {renderMetricRow('Max', result.max)}
            {renderMetricRow('Range', result.range)}
            {renderMetricRow('Skewness', result.skewness, colors.text)}
            {renderMetricRow('Kurtosis', result.kurtosis, colors.text)}
            {renderMetricRow('Count', result.count, colors.text)}
            {renderMetricRow('Sum', result.sum)}
            {renderMetricRow('Semi-Variance', result.semi_variance)}
            {renderMetricRow('Realized Var', result.realized_variance)}
          </div>
          {result.percentiles && (
            <div style={{ marginTop: '12px' }}>
              <div style={{ ...labelStyle, marginBottom: '4px' }}>PERCENTILES</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px 16px' }}>
                {Object.entries(result.percentiles).map(([pct, v]) =>
                  renderMetricRow(`P${pct}`, v as number, colors.text)
                )}
              </div>
            </div>
          )}
        </div>
      );
    }

    // Fallback: render all key-value pairs
    return (
      <div style={cardStyle}>
        <div style={{ ...labelStyle, color: GS_PURPLE, marginBottom: '8px' }}>RESULT</div>
        <pre style={{ color: colors.text, fontSize: '11px', fontFamily: 'monospace', whiteSpace: 'pre-wrap', overflowX: 'auto' }}>
          {JSON.stringify(result, null, 2)}
        </pre>
      </div>
    );
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Mode Tabs */}
      <div
        style={{
          display: 'flex',
          gap: '2px',
          padding: '8px 12px',
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${'#333'}`,
          flexWrap: 'wrap',
        }}
      >
        {MODES.map(m => {
          const Icon = m.icon;
          const isActive = mode === m.id;
          return (
            <button
              key={m.id}
              onClick={() => { setMode(m.id); setResult(null); setError(null); }}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                padding: '5px 10px',
                fontSize: '11px',
                fontWeight: isActive ? 700 : 400,
                color: isActive ? GS_PURPLE : colors.textMuted,
                backgroundColor: isActive ? `${GS_PURPLE}20` : 'transparent',
                border: isActive ? `1px solid ${GS_PURPLE}` : `1px solid transparent`,
                cursor: 'pointer',
                transition: 'all 0.15s',
              }}
            >
              <Icon size={13} />
              {m.label}
            </button>
          );
        })}
      </div>

      {/* Content Area */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* How to Use Guide */}
        <HowToUsePanel mode={mode} />

        {/* Inputs */}
        {mode === 'greeks' ? renderGreeksInputs() :
         mode === 'backtest' ? renderBacktestInputs() :
         renderCommonInputs()}

        {/* Run Button */}
        <button
          onClick={runAnalysis}
          disabled={loading}
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            padding: '8px 16px',
            backgroundColor: loading ? `${GS_PURPLE}60` : GS_PURPLE,
            color: '#fff',
            border: 'none',
            cursor: loading ? 'not-allowed' : 'pointer',
            fontSize: '12px',
            fontWeight: 600,
          }}
        >
          {loading ? <RefreshCw size={14} className="animate-spin" /> : <Play size={14} />}
          {loading ? 'ANALYZING...' : 'RUN ANALYSIS'}
        </button>

        {/* Error */}
        {error && (
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px', padding: '10px', backgroundColor: `${colors.alert}15`, border: `1px solid ${colors.alert}` }}>
            <AlertCircle size={14} color={colors.alert} style={{ flexShrink: 0, marginTop: 2 }} />
            <div style={{ color: colors.alert, fontSize: '11px', fontFamily: 'monospace', whiteSpace: 'pre-wrap' }}>{error}</div>
          </div>
        )}

        {/* Results */}
        {renderResults()}
      </div>
    </div>
  );
}

export default GsQuantPanel;
