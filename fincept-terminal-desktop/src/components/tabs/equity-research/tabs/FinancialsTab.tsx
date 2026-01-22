import React, { useState, useMemo } from 'react';
import {
  BarChart, Bar, LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip,
  ResponsiveContainer, ComposedChart, ReferenceLine, Cell, PieChart, Pie,
  AreaChart, Area, RadarChart, Radar, PolarGrid, PolarAngleAxis, PolarRadiusAxis,
  Treemap, Legend
} from 'recharts';
import { FINCEPT, TYPOGRAPHY, BORDERS } from '../../portfolio-tab/finceptStyles';
import { formatLargeNumber, getYearFromPeriod } from '../utils/formatters';
import type { FinancialsData, StockInfo } from '../types';
import {
  DollarSign, CreditCard, ArrowUpRight, ArrowDownRight, TrendingUp, TrendingDown,
  Building2, Banknote, Receipt, CircleDollarSign, Landmark, PiggyBank,
  ChevronDown, ChevronRight, AlertTriangle, CheckCircle, XCircle, Info,
  BarChart3, PieChart as PieChartIcon, Activity, Wallet, Scale, Percent,
  ArrowRight, Zap, Target, Shield, Clock, Layers
} from 'lucide-react';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  MUTED: FINCEPT.MUTED,
  BLUE: FINCEPT.BLUE,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  HOVER: FINCEPT.HOVER,
  BORDER: FINCEPT.BORDER,
  PURPLE: FINCEPT.PURPLE,
};

const CHART_COLORS = [COLORS.GREEN, COLORS.CYAN, COLORS.ORANGE, COLORS.YELLOW, COLORS.PURPLE, COLORS.BLUE, COLORS.RED];

interface FinancialsTabProps {
  financials: FinancialsData | null;
  stockInfo?: StockInfo | null;
  loading: boolean;
}

// Helper functions
const formatPercent = (value: number | null | undefined): string => {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${(value * 100).toFixed(1)}%`;
};

const formatCurrency = (value: number | null | undefined): string => {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  const absValue = Math.abs(value);
  const sign = value < 0 ? '-' : '';
  if (absValue >= 1e12) return `${sign}$${(absValue / 1e12).toFixed(2)}T`;
  if (absValue >= 1e9) return `${sign}$${(absValue / 1e9).toFixed(2)}B`;
  if (absValue >= 1e6) return `${sign}$${(absValue / 1e6).toFixed(2)}M`;
  if (absValue >= 1e3) return `${sign}$${(absValue / 1e3).toFixed(2)}K`;
  return `${sign}$${absValue.toFixed(2)}`;
};

const formatRatio = (value: number | null | undefined): string => {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${value.toFixed(2)}x`;
};

const getHealthColor = (value: number, thresholds: { good: number; warning: number }, higherIsBetter: boolean = true): string => {
  if (higherIsBetter) {
    if (value >= thresholds.good) return COLORS.GREEN;
    if (value >= thresholds.warning) return COLORS.YELLOW;
    return COLORS.RED;
  } else {
    if (value <= thresholds.good) return COLORS.GREEN;
    if (value <= thresholds.warning) return COLORS.YELLOW;
    return COLORS.RED;
  }
};

const calculateGrowth = (current: number, previous: number): number | null => {
  if (!previous || previous === 0) return null;
  return ((current - previous) / Math.abs(previous)) * 100;
};

// ===== REUSABLE COMPONENTS =====

// Metric Card with proper sizing
const MetricCard: React.FC<{
  label: string;
  value: string | number;
  change?: number | null;
  color?: string;
  icon?: React.ReactNode;
  subtitle?: string;
  trend?: 'up' | 'down' | 'neutral';
  size?: 'small' | 'medium' | 'large';
}> = ({ label, value, change, color = COLORS.WHITE, icon, subtitle, trend, size = 'medium' }) => {
  const TrendIcon = trend === 'up' ? ArrowUpRight : trend === 'down' ? ArrowDownRight : null;
  const trendColor = trend === 'up' ? COLORS.GREEN : trend === 'down' ? COLORS.RED : COLORS.GRAY;

  const padding = size === 'small' ? '10px' : size === 'large' ? '16px' : '12px';
  const labelSize = size === 'small' ? '10px' : size === 'large' ? '12px' : '11px';
  const valueSize = size === 'small' ? '16px' : size === 'large' ? '24px' : '20px';

  return (
    <div style={{
      backgroundColor: COLORS.DARK_BG,
      border: BORDERS.STANDARD,
      borderLeft: `3px solid ${color}`,
      padding: padding,
      minHeight: size === 'large' ? '90px' : '70px',
      display: 'flex',
      flexDirection: 'column',
      justifyContent: 'space-between',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
        <span style={{ color: COLORS.GRAY, fontSize: labelSize, textTransform: 'uppercase', fontWeight: 600 }}>{label}</span>
        {icon && <span style={{ color: color, opacity: 0.8 }}>{icon}</span>}
      </div>
      <div style={{ color: color, fontSize: valueSize, fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>{value}</div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
        {(change !== undefined && change !== null) && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
            {TrendIcon && <TrendIcon size={12} color={trendColor} />}
            <span style={{ color: trendColor, fontSize: '11px', fontWeight: 600 }}>
              {change >= 0 ? '+' : ''}{change.toFixed(1)}% YoY
            </span>
          </div>
        )}
        {subtitle && <span style={{ color: COLORS.MUTED, fontSize: '10px' }}>{subtitle}</span>}
      </div>
    </div>
  );
};

// Health Score with visual bar
const HealthScore: React.FC<{
  label: string;
  value: number;
  max?: number;
  thresholds: { good: number; warning: number };
  higherIsBetter?: boolean;
  format?: 'percent' | 'ratio' | 'number';
  description?: string;
}> = ({ label, value, max = 100, thresholds, higherIsBetter = true, format = 'number', description }) => {
  const healthColor = getHealthColor(value, thresholds, higherIsBetter);
  const percentage = Math.min(100, Math.max(0, (Math.abs(value) / max) * 100));

  let displayValue = value.toFixed(2);
  if (format === 'percent') displayValue = `${(value * 100).toFixed(1)}%`;
  if (format === 'ratio') displayValue = `${value.toFixed(2)}x`;

  return (
    <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
        <span style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600 }}>{label}</span>
        <span style={{ color: healthColor, fontSize: '16px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>{displayValue}</span>
      </div>
      <div style={{ height: '6px', backgroundColor: COLORS.PANEL_BG, borderRadius: '3px', overflow: 'hidden' }}>
        <div style={{
          width: `${percentage}%`,
          height: '100%',
          backgroundColor: healthColor,
          borderRadius: '3px',
          transition: 'width 0.3s ease'
        }} />
      </div>
      {description && <div style={{ color: COLORS.MUTED, fontSize: '10px', marginTop: '6px' }}>{description}</div>}
    </div>
  );
};

// Section Panel with collapsible content
const SectionPanel: React.FC<{
  title: string;
  color: string;
  icon: React.ReactNode;
  children: React.ReactNode;
  defaultExpanded?: boolean;
  subtitle?: string;
}> = ({ title, color, icon, children, defaultExpanded = true, subtitle }) => {
  const [expanded, setExpanded] = useState(defaultExpanded);

  return (
    <div style={{ backgroundColor: COLORS.PANEL_BG, border: BORDERS.STANDARD, marginBottom: '16px', borderRadius: '4px', overflow: 'hidden' }}>
      <div
        onClick={() => setExpanded(!expanded)}
        style={{
          backgroundColor: COLORS.HEADER_BG,
          padding: '12px 16px',
          borderBottom: expanded ? `2px solid ${color}` : 'none',
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
          cursor: 'pointer',
        }}
      >
        {expanded ? <ChevronDown size={16} color={color} /> : <ChevronRight size={16} color={color} />}
        {icon}
        <div style={{ flex: 1 }}>
          <span style={{ color: color, fontSize: '13px', fontWeight: 700, textTransform: 'uppercase' }}>{title}</span>
          {subtitle && <span style={{ color: COLORS.MUTED, fontSize: '11px', marginLeft: '12px' }}>{subtitle}</span>}
        </div>
      </div>
      {expanded && <div style={{ padding: '16px' }}>{children}</div>}
    </div>
  );
};

// Chart Wrapper for consistent styling
const ChartWrapper: React.FC<{
  title: string;
  color: string;
  height?: number;
  children: React.ReactNode;
}> = ({ title, color, height = 220, children }) => (
  <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px', overflow: 'hidden' }}>
    <div style={{
      padding: '10px 14px',
      borderBottom: `2px solid ${color}`,
      backgroundColor: COLORS.HEADER_BG
    }}>
      <span style={{ color: color, fontSize: '12px', fontWeight: 700, textTransform: 'uppercase' }}>{title}</span>
    </div>
    <div style={{ padding: '12px', height: height }}>
      {children}
    </div>
  </div>
);

// Custom Tooltip for charts
const CustomTooltip: React.FC<any> = ({ active, payload, label, formatter }) => {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      backgroundColor: COLORS.DARK_BG,
      border: BORDERS.STANDARD,
      padding: '10px 14px',
      borderRadius: '4px',
      boxShadow: '0 4px 12px rgba(0,0,0,0.3)'
    }}>
      <div style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, marginBottom: '6px' }}>{label}</div>
      {payload.map((entry: any, idx: number) => (
        <div key={idx} style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
          <div style={{ width: '10px', height: '10px', backgroundColor: entry.color, borderRadius: '2px' }} />
          <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>{entry.name}:</span>
          <span style={{ color: entry.color, fontSize: '11px', fontWeight: 600, fontFamily: TYPOGRAPHY.MONO }}>
            {formatter ? formatter(entry.value) : entry.value}
          </span>
        </div>
      ))}
    </div>
  );
};

export const FinancialsTab: React.FC<FinancialsTabProps> = ({
  financials,
  stockInfo,
  loading,
}) => {
  const [activeStatement, setActiveStatement] = useState<'income' | 'balance' | 'cashflow'>('income');

  // Process financial data
  const processedData = useMemo(() => {
    if (!financials) return null;

    const incomeKeys = Object.keys(financials.income_statement).sort().reverse();
    const balanceKeys = Object.keys(financials.balance_sheet).sort().reverse();
    const cashflowKeys = Object.keys(financials.cash_flow).sort().reverse();

    // Get latest period data
    const latestIncome = incomeKeys[0] ? financials.income_statement[incomeKeys[0]] : {};
    const prevIncome = incomeKeys[1] ? financials.income_statement[incomeKeys[1]] : {};
    const latestBalance = balanceKeys[0] ? financials.balance_sheet[balanceKeys[0]] : {};
    const prevBalance = balanceKeys[1] ? financials.balance_sheet[balanceKeys[1]] : {};
    const latestCashflow = cashflowKeys[0] ? financials.cash_flow[cashflowKeys[0]] : {};
    const prevCashflow = cashflowKeys[1] ? financials.cash_flow[cashflowKeys[1]] : {};

    // ===== INCOME STATEMENT METRICS =====
    const revenue = latestIncome['Total Revenue'] || latestIncome['Revenue'] || 0;
    const prevRevenue = prevIncome['Total Revenue'] || prevIncome['Revenue'] || 0;
    const grossProfit = latestIncome['Gross Profit'] || 0;
    const prevGrossProfit = prevIncome['Gross Profit'] || 0;
    const operatingIncome = latestIncome['Operating Income'] || latestIncome['Operating Profit'] || 0;
    const prevOperatingIncome = prevIncome['Operating Income'] || prevIncome['Operating Profit'] || 0;
    const netIncome = latestIncome['Net Income'] || latestIncome['Net Income Common Stockholders'] || 0;
    const prevNetIncome = prevIncome['Net Income'] || prevIncome['Net Income Common Stockholders'] || 0;
    const ebitda = latestIncome['EBITDA'] || latestIncome['Normalized EBITDA'] || 0;
    const costOfRevenue = latestIncome['Cost Of Revenue'] || 0;
    const operatingExpenses = latestIncome['Operating Expense'] || latestIncome['Total Operating Expenses'] || 0;
    const interestExpense = latestIncome['Interest Expense'] || latestIncome['Interest Expense Non Operating'] || latestIncome['Net Interest Income'] || latestIncome['Interest And Debt Expense'] || Math.abs(latestIncome['Net Non Operating Interest Income Expense'] || 0) || 0;
    const interestIncome = latestIncome['Interest Income'] || 0;
    const taxExpense = latestIncome['Tax Provision'] || latestIncome['Income Tax Expense'] || 0;
    const researchDev = latestIncome['Research And Development'] || latestIncome['Research Development'] || 0;
    const sgaExpense = latestIncome['Selling General And Administration'] || latestIncome['SG&A'] || 0;
    const depreciation = latestIncome['Depreciation And Amortization'] || 0;
    const otherIncome = latestIncome['Other Income'] || latestIncome['Other Non Operating Income Expenses'] || 0;

    // ===== BALANCE SHEET METRICS =====
    const totalAssets = latestBalance['Total Assets'] || 0;
    const prevTotalAssets = prevBalance['Total Assets'] || 0;
    const totalLiabilities = latestBalance['Total Liabilities Net Minority Interest'] || latestBalance['Total Liabilities'] || 0;
    const totalEquity = latestBalance['Stockholders Equity'] || latestBalance['Total Equity'] || latestBalance['Total Stockholder Equity'] || 0;
    const prevTotalEquity = prevBalance['Stockholders Equity'] || prevBalance['Total Equity'] || prevBalance['Total Stockholder Equity'] || 0;
    const currentAssets = latestBalance['Current Assets'] || latestBalance['Total Current Assets'] || 0;
    const currentLiabilities = latestBalance['Current Liabilities'] || latestBalance['Total Current Liabilities'] || 0;
    const totalDebt = latestBalance['Total Debt'] || (latestBalance['Long Term Debt'] || 0) + (latestBalance['Short Term Debt'] || 0);
    const cash = latestBalance['Cash And Cash Equivalents'] || latestBalance['Cash'] || 0;
    const shortTermInvestments = latestBalance['Short Term Investments'] || latestBalance['Other Short Term Investments'] || 0;
    const inventory = latestBalance['Inventory'] || 0;
    const receivables = latestBalance['Accounts Receivable'] || latestBalance['Net Receivables'] || 0;
    const payables = latestBalance['Accounts Payable'] || 0;
    const longTermDebt = latestBalance['Long Term Debt'] || 0;
    const shortTermDebt = latestBalance['Short Term Debt'] || latestBalance['Current Debt'] || 0;
    const retainedEarnings = latestBalance['Retained Earnings'] || 0;
    const goodwill = latestBalance['Goodwill'] || 0;
    const intangibles = latestBalance['Intangible Assets'] || latestBalance['Other Intangible Assets'] || 0;
    const ppe = latestBalance['Net PPE'] || latestBalance['Property Plant Equipment'] || 0;
    const otherAssets = latestBalance['Other Assets'] || latestBalance['Other Non Current Assets'] || 0;
    const deferredRevenue = latestBalance['Deferred Revenue'] || 0;

    // ===== CASH FLOW METRICS =====
    const operatingCashflow = latestCashflow['Operating Cash Flow'] || latestCashflow['Total Cash From Operating Activities'] || 0;
    const prevOperatingCashflow = prevCashflow['Operating Cash Flow'] || prevCashflow['Total Cash From Operating Activities'] || 0;
    const investingCashflow = latestCashflow['Investing Cash Flow'] || latestCashflow['Total Cash From Investing Activities'] || 0;
    const financingCashflow = latestCashflow['Financing Cash Flow'] || latestCashflow['Total Cash From Financing Activities'] || 0;
    const capex = Math.abs(latestCashflow['Capital Expenditure'] || latestCashflow['Capital Expenditures'] || 0);
    const freeCashflow = operatingCashflow - capex;
    const prevFreeCashflow = prevOperatingCashflow - Math.abs(prevCashflow['Capital Expenditure'] || prevCashflow['Capital Expenditures'] || 0);
    const dividendsPaid = Math.abs(latestCashflow['Cash Dividends Paid'] || latestCashflow['Payment Of Dividends'] || 0);
    const stockRepurchase = Math.abs(latestCashflow['Repurchase Of Stock'] || latestCashflow['Common Stock Repurchased'] || latestCashflow['Repurchase Of Capital Stock'] || latestCashflow['Common Stock Payments'] || latestCashflow['Purchase Of Stock'] || 0);
    const depreciationCF = latestCashflow['Depreciation And Amortization'] || latestCashflow['Depreciation'] || 0;
    const stockBasedComp = latestCashflow['Stock Based Compensation'] || 0;
    const changeInWorkingCapital = latestCashflow['Change In Working Capital'] || 0;
    const debtIssuance = latestCashflow['Issuance Of Debt'] || latestCashflow['Long Term Debt Issuance'] || 0;
    const debtRepayment = Math.abs(latestCashflow['Repayment Of Debt'] || latestCashflow['Long Term Debt Payments'] || 0);

    // ===== CALCULATE RATIOS =====
    // Profitability Ratios
    const grossMargin = revenue ? (grossProfit / revenue) : 0;
    const operatingMargin = revenue ? (operatingIncome / revenue) : 0;
    const netMargin = revenue ? (netIncome / revenue) : 0;
    const ebitdaMargin = revenue ? (ebitda / revenue) : 0;

    // Return Ratios
    const avgTotalAssets = (totalAssets + prevTotalAssets) / 2 || totalAssets;
    const avgTotalEquity = (totalEquity + prevTotalEquity) / 2 || totalEquity;
    const roa = avgTotalAssets ? (netIncome / avgTotalAssets) : 0;
    const roe = avgTotalEquity ? (netIncome / avgTotalEquity) : 0;
    const investedCapital = totalEquity + totalDebt - cash;
    const roic = investedCapital > 0 ? (operatingIncome * (1 - 0.25)) / investedCapital : 0;
    const roce = (totalAssets - currentLiabilities) > 0 ? (operatingIncome / (totalAssets - currentLiabilities)) : 0;

    // Liquidity Ratios
    const currentRatio = currentLiabilities ? (currentAssets / currentLiabilities) : 0;
    const quickRatio = currentLiabilities ? ((currentAssets - inventory) / currentLiabilities) : 0;
    const cashRatio = currentLiabilities ? ((cash + shortTermInvestments) / currentLiabilities) : 0;
    const workingCapital = currentAssets - currentLiabilities;

    // Leverage/Solvency Ratios
    const debtToEquity = totalEquity ? (totalDebt / totalEquity) : 0;
    const debtToAssets = totalAssets ? (totalDebt / totalAssets) : 0;
    const debtToCapital = (totalDebt + totalEquity) ? (totalDebt / (totalDebt + totalEquity)) : 0;
    const equityMultiplier = totalEquity ? (totalAssets / totalEquity) : 0;
    const interestCoverage = interestExpense ? (ebitda / interestExpense) : 999;
    const netDebt = totalDebt - cash;

    // Efficiency Ratios
    const assetTurnover = avgTotalAssets ? (revenue / avgTotalAssets) : 0;
    const inventoryTurnover = inventory ? (costOfRevenue / inventory) : 0;
    const daysInventory = inventoryTurnover ? (365 / inventoryTurnover) : 0;
    const receivablesTurnover = receivables ? (revenue / receivables) : 0;
    const daysReceivables = receivablesTurnover ? (365 / receivablesTurnover) : 0;
    const payablesTurnover = payables ? (costOfRevenue / payables) : 0;
    const daysPayables = payablesTurnover ? (365 / payablesTurnover) : 0;
    const cashConversionCycle = daysInventory + daysReceivables - daysPayables;
    const fixedAssetTurnover = ppe ? (revenue / ppe) : 0;

    // Cash Flow Ratios
    const fcfMargin = revenue ? (freeCashflow / revenue) : 0;
    const fcfToNetIncome = netIncome ? (freeCashflow / netIncome) : 0;
    const capexToRevenue = revenue ? (capex / revenue) : 0;
    const capexToDepreciation = depreciationCF ? (capex / depreciationCF) : 0;
    const operatingCashflowRatio = currentLiabilities ? (operatingCashflow / currentLiabilities) : 0;
    const cashFlowCoverage = totalDebt ? (operatingCashflow / totalDebt) : 0;

    // DuPont Analysis
    const dupontNetMargin = netMargin;
    const dupontAssetTurnover = assetTurnover;
    const dupontEquityMultiplier = equityMultiplier;
    const dupontROE = dupontNetMargin * dupontAssetTurnover * dupontEquityMultiplier;

    // ===== CHART DATA =====

    // Revenue breakdown waterfall
    const revenueWaterfall = [
      { name: 'Revenue', value: revenue, fill: COLORS.GREEN },
      { name: 'Cost of Revenue', value: -costOfRevenue, fill: COLORS.RED },
      { name: 'Gross Profit', value: grossProfit, fill: COLORS.CYAN },
      { name: 'Operating Exp', value: -operatingExpenses, fill: COLORS.ORANGE },
      { name: 'Operating Income', value: operatingIncome, fill: COLORS.BLUE },
      { name: 'Interest & Tax', value: -(interestExpense + taxExpense - interestIncome), fill: COLORS.PURPLE },
      { name: 'Net Income', value: netIncome, fill: netIncome >= 0 ? COLORS.GREEN : COLORS.RED },
    ];

    // Historical trends (5 years)
    const revenueTrend = incomeKeys.slice(0, 5).reverse().map(period => ({
      period: getYearFromPeriod(period),
      revenue: (financials.income_statement[period]['Total Revenue'] || financials.income_statement[period]['Revenue'] || 0) / 1e9,
      netIncome: (financials.income_statement[period]['Net Income'] || financials.income_statement[period]['Net Income Common Stockholders'] || 0) / 1e9,
      grossProfit: (financials.income_statement[period]['Gross Profit'] || 0) / 1e9,
      operatingIncome: (financials.income_statement[period]['Operating Income'] || 0) / 1e9,
      ebitda: (financials.income_statement[period]['EBITDA'] || financials.income_statement[period]['Normalized EBITDA'] || 0) / 1e9,
    }));

    const marginTrend = incomeKeys.slice(0, 5).reverse().map(period => {
      const rev = financials.income_statement[period]['Total Revenue'] || financials.income_statement[period]['Revenue'] || 1;
      const gross = financials.income_statement[period]['Gross Profit'] || 0;
      const op = financials.income_statement[period]['Operating Income'] || 0;
      const net = financials.income_statement[period]['Net Income'] || financials.income_statement[period]['Net Income Common Stockholders'] || 0;
      const ebit = financials.income_statement[period]['EBITDA'] || financials.income_statement[period]['Normalized EBITDA'] || 0;
      return {
        period: getYearFromPeriod(period),
        grossMargin: (gross / rev) * 100,
        operatingMargin: (op / rev) * 100,
        netMargin: (net / rev) * 100,
        ebitdaMargin: (ebit / rev) * 100,
      };
    });

    const cashflowTrend = cashflowKeys.slice(0, 5).reverse().map(period => {
      const opCf = financials.cash_flow[period]['Operating Cash Flow'] || financials.cash_flow[period]['Total Cash From Operating Activities'] || 0;
      const invCf = financials.cash_flow[period]['Investing Cash Flow'] || financials.cash_flow[period]['Total Cash From Investing Activities'] || 0;
      const finCf = financials.cash_flow[period]['Financing Cash Flow'] || financials.cash_flow[period]['Total Cash From Financing Activities'] || 0;
      const capEx = Math.abs(financials.cash_flow[period]['Capital Expenditure'] || financials.cash_flow[period]['Capital Expenditures'] || 0);
      return {
        period: getYearFromPeriod(period),
        operating: opCf / 1e9,
        investing: invCf / 1e9,
        financing: finCf / 1e9,
        freeCashflow: (opCf - capEx) / 1e9,
        capex: -capEx / 1e9,
      };
    });

    // Balance sheet trends
    const balanceTrend = balanceKeys.slice(0, 5).reverse().map(period => {
      const assets = financials.balance_sheet[period]['Total Assets'] || 0;
      const liabilities = financials.balance_sheet[period]['Total Liabilities Net Minority Interest'] || financials.balance_sheet[period]['Total Liabilities'] || 0;
      const equity = financials.balance_sheet[period]['Stockholders Equity'] || financials.balance_sheet[period]['Total Equity'] || 0;
      const debt = financials.balance_sheet[period]['Total Debt'] || 0;
      const cashVal = financials.balance_sheet[period]['Cash And Cash Equivalents'] || financials.balance_sheet[period]['Cash'] || 0;
      return {
        period: getYearFromPeriod(period),
        assets: assets / 1e9,
        liabilities: liabilities / 1e9,
        equity: equity / 1e9,
        debt: debt / 1e9,
        cash: cashVal / 1e9,
        netDebt: (debt - cashVal) / 1e9,
      };
    });

    // Return metrics trend
    const returnTrend = incomeKeys.slice(0, 5).reverse().map((period, idx) => {
      const net = financials.income_statement[period]['Net Income'] || financials.income_statement[period]['Net Income Common Stockholders'] || 0;
      const balPeriod = balanceKeys[idx];
      const assets = balPeriod ? (financials.balance_sheet[balPeriod]['Total Assets'] || 0) : 0;
      const equity = balPeriod ? (financials.balance_sheet[balPeriod]['Stockholders Equity'] || financials.balance_sheet[balPeriod]['Total Equity'] || 0) : 0;
      return {
        period: getYearFromPeriod(period),
        roe: equity ? ((net / equity) * 100) : 0,
        roa: assets ? ((net / assets) * 100) : 0,
      };
    });

    // Asset composition for pie chart
    const assetComposition = [
      { name: 'Cash & Investments', value: (cash + shortTermInvestments) / 1e9, color: COLORS.GREEN },
      { name: 'Receivables', value: receivables / 1e9, color: COLORS.CYAN },
      { name: 'Inventory', value: inventory / 1e9, color: COLORS.BLUE },
      { name: 'PP&E', value: ppe / 1e9, color: COLORS.ORANGE },
      { name: 'Goodwill & Intangibles', value: (goodwill + intangibles) / 1e9, color: COLORS.PURPLE },
      { name: 'Other Assets', value: Math.max(0, (totalAssets - cash - shortTermInvestments - receivables - inventory - ppe - goodwill - intangibles) / 1e9), color: COLORS.YELLOW },
    ].filter(item => item.value > 0.01);

    const liabilityComposition = [
      { name: 'Short-term Debt', value: shortTermDebt / 1e9, color: COLORS.RED },
      { name: 'Long-term Debt', value: longTermDebt / 1e9, color: COLORS.ORANGE },
      { name: 'Accounts Payable', value: payables / 1e9, color: COLORS.YELLOW },
      { name: 'Deferred Revenue', value: deferredRevenue / 1e9, color: COLORS.PURPLE },
      { name: 'Other Liabilities', value: Math.max(0, (totalLiabilities - shortTermDebt - longTermDebt - payables - deferredRevenue) / 1e9), color: COLORS.CYAN },
    ].filter(item => item.value > 0.01);

    // Capital structure
    const capitalStructure = [
      { name: 'Equity', value: totalEquity / 1e9, color: COLORS.GREEN },
      { name: 'Long-term Debt', value: longTermDebt / 1e9, color: COLORS.ORANGE },
      { name: 'Short-term Debt', value: shortTermDebt / 1e9, color: COLORS.RED },
    ].filter(item => item.value > 0.01);

    // Operating expenses breakdown
    const opexBreakdown = [
      { name: 'Cost of Revenue', value: costOfRevenue / 1e9, color: COLORS.RED },
      { name: 'R&D', value: researchDev / 1e9, color: COLORS.BLUE },
      { name: 'SG&A', value: sgaExpense / 1e9, color: COLORS.ORANGE },
      { name: 'Depreciation', value: depreciation / 1e9, color: COLORS.PURPLE },
      { name: 'Other', value: Math.max(0, (operatingExpenses - researchDev - sgaExpense - depreciation) / 1e9), color: COLORS.YELLOW },
    ].filter(item => item.value > 0.01);

    // Cash flow composition
    const cashFlowComposition = [
      { name: 'Operating CF', value: Math.abs(operatingCashflow) / 1e9, actual: operatingCashflow / 1e9, color: COLORS.GREEN, isPositive: operatingCashflow >= 0 },
      { name: 'Investing CF', value: Math.abs(investingCashflow) / 1e9, actual: investingCashflow / 1e9, color: COLORS.ORANGE, isPositive: investingCashflow >= 0 },
      { name: 'Financing CF', value: Math.abs(financingCashflow) / 1e9, actual: financingCashflow / 1e9, color: COLORS.PURPLE, isPositive: financingCashflow >= 0 },
    ];

    // DuPont analysis data
    const dupontData = [
      { subject: 'Net Margin', value: dupontNetMargin * 100, fullMark: 50 },
      { subject: 'Asset Turnover', value: dupontAssetTurnover * 50, fullMark: 100 },
      { subject: 'Equity Multiplier', value: dupontEquityMultiplier * 20, fullMark: 100 },
      { subject: 'ROE', value: roe * 100, fullMark: 50 },
      { subject: 'ROA', value: roa * 100, fullMark: 30 },
    ];

    // Efficiency metrics for radar
    const efficiencyRadar = [
      { metric: 'Asset Turn', value: Math.min(100, assetTurnover * 50), fullMark: 100 },
      { metric: 'Inv Turn', value: Math.min(100, inventoryTurnover * 10), fullMark: 100 },
      { metric: 'AR Turn', value: Math.min(100, receivablesTurnover * 10), fullMark: 100 },
      { metric: 'FA Turn', value: Math.min(100, fixedAssetTurnover * 10), fullMark: 100 },
      { metric: 'OCF Ratio', value: Math.min(100, operatingCashflowRatio * 50), fullMark: 100 },
    ];

    return {
      // Key metrics
      revenue, prevRevenue, grossProfit, prevGrossProfit, operatingIncome, prevOperatingIncome,
      netIncome, prevNetIncome, ebitda, costOfRevenue, operatingExpenses,
      interestExpense, taxExpense, researchDev, sgaExpense,
      // Balance sheet
      totalAssets, totalLiabilities, totalEquity, currentAssets, currentLiabilities,
      totalDebt, cash, shortTermInvestments, inventory, receivables, payables,
      longTermDebt, shortTermDebt, retainedEarnings, goodwill, intangibles, ppe,
      workingCapital, netDebt,
      // Cashflow
      operatingCashflow, prevOperatingCashflow, investingCashflow, financingCashflow,
      capex, freeCashflow, prevFreeCashflow, dividendsPaid, stockRepurchase,
      depreciationCF, stockBasedComp, changeInWorkingCapital, debtIssuance, debtRepayment,
      // Profitability ratios
      grossMargin, operatingMargin, netMargin, ebitdaMargin,
      // Return ratios
      roa, roe, roic, roce,
      // Liquidity ratios
      currentRatio, quickRatio, cashRatio,
      // Leverage ratios
      debtToEquity, debtToAssets, debtToCapital, equityMultiplier, interestCoverage,
      // Efficiency ratios
      assetTurnover, inventoryTurnover, daysInventory, receivablesTurnover, daysReceivables,
      payablesTurnover, daysPayables, cashConversionCycle, fixedAssetTurnover,
      // Cash flow ratios
      fcfMargin, fcfToNetIncome, capexToRevenue, capexToDepreciation, operatingCashflowRatio, cashFlowCoverage,
      // DuPont
      dupontNetMargin, dupontAssetTurnover, dupontEquityMultiplier, dupontROE,
      // Charts data
      revenueWaterfall, revenueTrend, marginTrend, cashflowTrend, balanceTrend, returnTrend,
      assetComposition, liabilityComposition, capitalStructure, opexBreakdown, cashFlowComposition,
      dupontData, efficiencyRadar,
      // Periods
      incomeKeys, balanceKeys, cashflowKeys,
      latestIncome, latestBalance, latestCashflow,
    };
  }, [financials]);

  if (loading) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '400px', gap: '16px', fontFamily: TYPOGRAPHY.MONO }}>
        <div style={{ width: '40px', height: '40px', border: `3px solid ${COLORS.BORDER}`, borderTop: `3px solid ${COLORS.ORANGE}`, borderRadius: '50%', animation: 'spin 1s linear infinite' }} />
        <div style={{ color: COLORS.ORANGE, fontSize: '14px', fontWeight: 700, textTransform: 'uppercase' }}>Loading Financial Data</div>
      </div>
    );
  }

  if (!financials || !processedData) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '400px', gap: '16px', fontFamily: TYPOGRAPHY.MONO }}>
        <AlertTriangle size={40} color={COLORS.YELLOW} />
        <div style={{ color: COLORS.YELLOW, fontSize: '16px', fontWeight: 700, textTransform: 'uppercase' }}>No Financial Data Available</div>
        <div style={{ color: COLORS.GRAY, fontSize: '13px' }}>Financial statements are not available for this symbol</div>
      </div>
    );
  }

  const {
    revenue, prevRevenue, grossProfit, operatingIncome, netIncome, prevNetIncome, ebitda,
    totalAssets, totalLiabilities, totalEquity, currentAssets, currentLiabilities,
    totalDebt, cash, freeCashflow, prevFreeCashflow, operatingCashflow, workingCapital, netDebt,
    grossMargin, operatingMargin, netMargin, ebitdaMargin,
    currentRatio, quickRatio, cashRatio, debtToEquity, debtToAssets, interestCoverage, equityMultiplier,
    roa, roe, roic, roce, fcfMargin,
    assetTurnover, inventoryTurnover, daysInventory, receivablesTurnover, daysReceivables,
    daysPayables, cashConversionCycle,
    capex, capexToRevenue, capexToDepreciation,
    dupontNetMargin, dupontAssetTurnover, dupontEquityMultiplier,
    revenueWaterfall, revenueTrend, marginTrend, cashflowTrend, balanceTrend, returnTrend,
    assetComposition, liabilityComposition, capitalStructure, opexBreakdown, cashFlowComposition,
    dupontData, efficiencyRadar,
    incomeKeys, latestIncome, latestBalance, latestCashflow,
  } = processedData;

  const revenueGrowth = calculateGrowth(revenue, prevRevenue);
  const netIncomeGrowth = calculateGrowth(netIncome, prevNetIncome);
  const fcfGrowth = calculateGrowth(freeCashflow, prevFreeCashflow);

  return (
    <div style={{
      padding: '16px',
      overflow: 'auto',
      height: '100%',
      fontFamily: TYPOGRAPHY.MONO,
    }} className="custom-scrollbar">

      {/* ===== KEY FINANCIAL METRICS ===== */}
      <SectionPanel
        title="Key Financial Metrics"
        color={COLORS.ORANGE}
        icon={<DollarSign size={16} color={COLORS.ORANGE} />}
        subtitle={`Latest Period: ${incomeKeys[0] ? getYearFromPeriod(incomeKeys[0]) : 'N/A'}`}
      >
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
          <MetricCard
            label="Total Revenue"
            value={formatCurrency(revenue)}
            change={revenueGrowth}
            color={COLORS.GREEN}
            icon={<Receipt size={18} />}
            trend={revenueGrowth && revenueGrowth > 0 ? 'up' : revenueGrowth && revenueGrowth < 0 ? 'down' : 'neutral'}
            size="large"
          />
          <MetricCard
            label="Gross Profit"
            value={formatCurrency(grossProfit)}
            color={COLORS.CYAN}
            icon={<DollarSign size={18} />}
            subtitle={`${formatPercent(grossMargin)} margin`}
            size="large"
          />
          <MetricCard
            label="Operating Income"
            value={formatCurrency(operatingIncome)}
            color={COLORS.BLUE}
            icon={<Activity size={18} />}
            subtitle={`${formatPercent(operatingMargin)} margin`}
            size="large"
          />
          <MetricCard
            label="Net Income"
            value={formatCurrency(netIncome)}
            change={netIncomeGrowth}
            color={netIncome >= 0 ? COLORS.GREEN : COLORS.RED}
            icon={<Banknote size={18} />}
            trend={netIncomeGrowth && netIncomeGrowth > 0 ? 'up' : netIncomeGrowth && netIncomeGrowth < 0 ? 'down' : 'neutral'}
            size="large"
          />
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '12px', marginTop: '12px' }}>
          <MetricCard label="EBITDA" value={formatCurrency(ebitda)} color={COLORS.PURPLE} icon={<BarChart3 size={16} />} subtitle={`${formatPercent(ebitdaMargin)} margin`} />
          <MetricCard label="Free Cash Flow" value={formatCurrency(freeCashflow)} change={fcfGrowth} color={freeCashflow >= 0 ? COLORS.CYAN : COLORS.RED} icon={<CircleDollarSign size={16} />} trend={fcfGrowth && fcfGrowth > 0 ? 'up' : 'down'} />
          <MetricCard label="Total Assets" value={formatCurrency(totalAssets)} color={COLORS.BLUE} icon={<Building2 size={16} />} />
          <MetricCard label="Total Debt" value={formatCurrency(totalDebt)} color={COLORS.ORANGE} icon={<CreditCard size={16} />} subtitle={`Net: ${formatCurrency(netDebt)}`} />
          <MetricCard label="Cash Position" value={formatCurrency(cash)} color={COLORS.GREEN} icon={<Wallet size={16} />} />
        </div>
      </SectionPanel>

      {/* ===== PROFITABILITY ANALYSIS ===== */}
      <SectionPanel
        title="Profitability Analysis"
        color={COLORS.GREEN}
        icon={<TrendingUp size={16} color={COLORS.GREEN} />}
      >
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          {/* Profit Margins */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
            <HealthScore label="Gross Margin" value={grossMargin} max={1} thresholds={{ good: 0.4, warning: 0.2 }} format="percent" description="Revenue after COGS" />
            <HealthScore label="Operating Margin" value={operatingMargin} max={1} thresholds={{ good: 0.2, warning: 0.1 }} format="percent" description="Operational efficiency" />
            <HealthScore label="Net Profit Margin" value={netMargin} max={1} thresholds={{ good: 0.15, warning: 0.05 }} format="percent" description="Bottom line profitability" />
            <HealthScore label="EBITDA Margin" value={ebitdaMargin} max={1} thresholds={{ good: 0.25, warning: 0.15 }} format="percent" description="Cash flow proxy" />
          </div>

          {/* Margin Trends Chart */}
          <ChartWrapper title="Margin Trends (%)" color={COLORS.GREEN} height={200}>
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={marginTrend} margin={{ top: 10, right: 30, left: 0, bottom: 10 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
                <XAxis dataKey="period" tick={{ fill: COLORS.GRAY, fontSize: 11 }} axisLine={{ stroke: COLORS.BORDER }} />
                <YAxis tick={{ fill: COLORS.GRAY, fontSize: 11 }} tickFormatter={(v) => `${v.toFixed(0)}%`} axisLine={{ stroke: COLORS.BORDER }} />
                <Tooltip content={<CustomTooltip formatter={(v: number) => `${v.toFixed(1)}%`} />} />
                <Legend wrapperStyle={{ fontSize: '11px' }} />
                <Line type="monotone" dataKey="grossMargin" stroke={COLORS.GREEN} strokeWidth={2} dot={{ r: 4 }} name="Gross" />
                <Line type="monotone" dataKey="operatingMargin" stroke={COLORS.CYAN} strokeWidth={2} dot={{ r: 4 }} name="Operating" />
                <Line type="monotone" dataKey="netMargin" stroke={COLORS.ORANGE} strokeWidth={2} dot={{ r: 4 }} name="Net" />
                <Line type="monotone" dataKey="ebitdaMargin" stroke={COLORS.PURPLE} strokeWidth={2} dot={{ r: 4 }} name="EBITDA" />
              </LineChart>
            </ResponsiveContainer>
          </ChartWrapper>
        </div>

        {/* Revenue & Earnings Trend */}
        <div style={{ marginTop: '16px' }}>
          <ChartWrapper title="Revenue & Earnings Trend (Billions $)" color={COLORS.BLUE} height={220}>
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={revenueTrend} margin={{ top: 10, right: 30, left: 0, bottom: 10 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
                <XAxis dataKey="period" tick={{ fill: COLORS.GRAY, fontSize: 11 }} axisLine={{ stroke: COLORS.BORDER }} />
                <YAxis tick={{ fill: COLORS.GRAY, fontSize: 11 }} tickFormatter={(v) => `$${v.toFixed(0)}B`} axisLine={{ stroke: COLORS.BORDER }} />
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Legend wrapperStyle={{ fontSize: '11px' }} />
                <Bar dataKey="revenue" fill={COLORS.BLUE} name="Revenue" radius={[4, 4, 0, 0]} />
                <Bar dataKey="grossProfit" fill={COLORS.CYAN} name="Gross Profit" radius={[4, 4, 0, 0]} />
                <Line type="monotone" dataKey="operatingIncome" stroke={COLORS.ORANGE} strokeWidth={2} dot={{ r: 4 }} name="Operating Inc" />
                <Line type="monotone" dataKey="netIncome" stroke={COLORS.GREEN} strokeWidth={2} dot={{ r: 4 }} name="Net Income" />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartWrapper>
        </div>
      </SectionPanel>

      {/* ===== RETURN METRICS & DUPONT ANALYSIS ===== */}
      <SectionPanel
        title="Return on Investment & DuPont Analysis"
        color={COLORS.CYAN}
        icon={<Target size={16} color={COLORS.CYAN} />}
      >
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '16px' }}>
          {/* Return Metrics */}
          <div>
            <div style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, marginBottom: '10px', paddingBottom: '6px', borderBottom: `1px solid ${COLORS.CYAN}40` }}>
              RETURN METRICS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <HealthScore label="Return on Equity (ROE)" value={roe} max={0.5} thresholds={{ good: 0.15, warning: 0.08 }} format="percent" description="Profit per shareholder dollar" />
              <HealthScore label="Return on Assets (ROA)" value={roa} max={0.3} thresholds={{ good: 0.08, warning: 0.04 }} format="percent" description="Asset utilization efficiency" />
              <HealthScore label="Return on Invested Capital" value={roic} max={0.3} thresholds={{ good: 0.12, warning: 0.06 }} format="percent" description="Capital allocation effectiveness" />
              <HealthScore label="Return on Capital Employed" value={roce} max={0.3} thresholds={{ good: 0.15, warning: 0.08 }} format="percent" description="Operating return on capital" />
            </div>
          </div>

          {/* DuPont Breakdown */}
          <div>
            <div style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, marginBottom: '10px', paddingBottom: '6px', borderBottom: `1px solid ${COLORS.PURPLE}40` }}>
              DUPONT ANALYSIS
            </div>
            <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '16px', borderRadius: '4px' }}>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px' }}>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ color: COLORS.GREEN, fontSize: '18px', fontWeight: 700 }}>{formatPercent(dupontNetMargin)}</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>Net Margin</div>
                </div>
                <div style={{ color: COLORS.MUTED, fontSize: '16px' }}>×</div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ color: COLORS.CYAN, fontSize: '18px', fontWeight: 700 }}>{dupontAssetTurnover.toFixed(2)}x</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>Asset Turn</div>
                </div>
                <div style={{ color: COLORS.MUTED, fontSize: '16px' }}>×</div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ color: COLORS.ORANGE, fontSize: '18px', fontWeight: 700 }}>{equityMultiplier.toFixed(2)}x</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>Eq. Mult</div>
                </div>
              </div>
              <div style={{ borderTop: `1px solid ${COLORS.BORDER}`, paddingTop: '12px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}>
                <ArrowRight size={16} color={COLORS.PURPLE} />
                <span style={{ color: COLORS.WHITE, fontSize: '12px' }}>ROE =</span>
                <span style={{ color: COLORS.PURPLE, fontSize: '20px', fontWeight: 700 }}>{formatPercent(roe)}</span>
              </div>
            </div>

            {/* Four additional DuPont cards */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginTop: '12px' }}>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>ROA (2-Step)</div>
                <div style={{ color: COLORS.BLUE, fontSize: '18px', fontWeight: 700 }}>{formatPercent(roa)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Net Margin × Asset Turnover</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>Financial Leverage</div>
                <div style={{ color: equityMultiplier <= 2 ? COLORS.GREEN : equityMultiplier <= 3 ? COLORS.YELLOW : COLORS.RED, fontSize: '18px', fontWeight: 700 }}>{equityMultiplier.toFixed(2)}x</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Total Assets / Total Equity</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>Profit per $ Equity</div>
                <div style={{ color: COLORS.CYAN, fontSize: '18px', fontWeight: 700 }}>${totalEquity > 0 ? (netIncome / totalEquity).toFixed(2) : '0.00'}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Net Income / Shareholders Equity</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>Leverage Effect</div>
                <div style={{ color: roe > roa ? COLORS.GREEN : COLORS.RED, fontSize: '18px', fontWeight: 700 }}>{roe > roa ? '+' : ''}{((roe - roa) * 100).toFixed(1)}%</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>ROE - ROA (Debt Amplification)</div>
              </div>
            </div>
          </div>

          {/* Return Trends Chart */}
          <ChartWrapper title="Return Metrics Trend (%)" color={COLORS.CYAN} height={200}>
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={returnTrend} margin={{ top: 10, right: 20, left: 0, bottom: 10 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
                <XAxis dataKey="period" tick={{ fill: COLORS.GRAY, fontSize: 11 }} />
                <YAxis tick={{ fill: COLORS.GRAY, fontSize: 11 }} tickFormatter={(v) => `${v.toFixed(0)}%`} />
                <Tooltip content={<CustomTooltip formatter={(v: number) => `${v.toFixed(1)}%`} />} />
                <Area type="monotone" dataKey="roe" stroke={COLORS.CYAN} fill={COLORS.CYAN} fillOpacity={0.3} strokeWidth={2} name="ROE" />
                <Area type="monotone" dataKey="roa" stroke={COLORS.GREEN} fill={COLORS.GREEN} fillOpacity={0.3} strokeWidth={2} name="ROA" />
              </AreaChart>
            </ResponsiveContainer>
          </ChartWrapper>
        </div>
      </SectionPanel>

      {/* ===== BALANCE SHEET ANALYSIS ===== */}
      <SectionPanel
        title="Balance Sheet Analysis"
        color={COLORS.BLUE}
        icon={<Scale size={16} color={COLORS.BLUE} />}
      >
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '16px' }}>
          {/* Liquidity Ratios */}
          <div>
            <div style={{ color: COLORS.CYAN, fontSize: '12px', fontWeight: 600, marginBottom: '10px', paddingBottom: '6px', borderBottom: `1px solid ${COLORS.CYAN}40` }}>
              LIQUIDITY RATIOS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <HealthScore label="Current Ratio" value={currentRatio} max={4} thresholds={{ good: 1.5, warning: 1.0 }} format="ratio" description="Short-term solvency" />
              <HealthScore label="Quick Ratio" value={quickRatio} max={3} thresholds={{ good: 1.0, warning: 0.5 }} format="ratio" description="Acid test ratio" />
              <HealthScore label="Cash Ratio" value={cashRatio} max={2} thresholds={{ good: 0.5, warning: 0.2 }} format="ratio" description="Immediate liquidity" />
            </div>
            <div style={{ marginTop: '12px', backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>Working Capital</span>
                <span style={{ color: workingCapital >= 0 ? COLORS.GREEN : COLORS.RED, fontSize: '14px', fontWeight: 700 }}>{formatCurrency(workingCapital)}</span>
              </div>
            </div>
          </div>

          {/* Leverage Ratios */}
          <div>
            <div style={{ color: COLORS.ORANGE, fontSize: '12px', fontWeight: 600, marginBottom: '10px', paddingBottom: '6px', borderBottom: `1px solid ${COLORS.ORANGE}40` }}>
              LEVERAGE & SOLVENCY
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <HealthScore label="Debt to Equity" value={debtToEquity} max={3} thresholds={{ good: 0.5, warning: 1.0 }} higherIsBetter={false} format="ratio" description="Financial leverage" />
              <HealthScore label="Debt to Assets" value={debtToAssets} max={1} thresholds={{ good: 0.3, warning: 0.5 }} higherIsBetter={false} format="percent" description="Asset coverage" />
              <HealthScore label="Interest Coverage" value={Math.min(interestCoverage, 20)} max={20} thresholds={{ good: 5, warning: 2 }} format="ratio" description="Debt service ability" />
            </div>
            <div style={{ marginTop: '12px', backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ color: COLORS.GRAY, fontSize: '11px' }}>Net Debt</span>
                <span style={{ color: netDebt <= 0 ? COLORS.GREEN : COLORS.ORANGE, fontSize: '14px', fontWeight: 700 }}>{formatCurrency(netDebt)}</span>
              </div>
            </div>
          </div>

          {/* Capital Structure Pie */}
          <ChartWrapper title="Capital Structure" color={COLORS.PURPLE} height={200}>
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={capitalStructure}
                  cx="50%"
                  cy="45%"
                  innerRadius={40}
                  outerRadius={65}
                  paddingAngle={2}
                  dataKey="value"
                >
                  {capitalStructure.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Pie>
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Legend
                  wrapperStyle={{ fontSize: '10px' }}
                  layout="horizontal"
                  align="center"
                  verticalAlign="bottom"
                  formatter={(value, entry: any) => {
                    const item = capitalStructure.find(d => d.name === value);
                    const total = capitalStructure.reduce((sum, d) => sum + d.value, 0);
                    const percent = item && total ? ((item.value / total) * 100).toFixed(0) : '0';
                    return <span style={{ color: entry.color }}>{value}: {percent}%</span>;
                  }}
                />
              </PieChart>
            </ResponsiveContainer>
          </ChartWrapper>
        </div>

        {/* Balance Sheet Trend & Composition */}
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr', gap: '16px', marginTop: '16px' }}>
          {/* Balance Sheet Trend */}
          <ChartWrapper title="Balance Sheet Trend (Billions $)" color={COLORS.BLUE} height={220}>
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={balanceTrend} margin={{ top: 10, right: 30, left: 0, bottom: 10 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
                <XAxis dataKey="period" tick={{ fill: COLORS.GRAY, fontSize: 11 }} />
                <YAxis tick={{ fill: COLORS.GRAY, fontSize: 11 }} tickFormatter={(v) => `$${v.toFixed(0)}B`} />
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Legend wrapperStyle={{ fontSize: '11px' }} />
                <Bar dataKey="assets" fill={COLORS.BLUE} name="Assets" radius={[4, 4, 0, 0]} />
                <Bar dataKey="liabilities" fill={COLORS.RED} name="Liabilities" radius={[4, 4, 0, 0]} />
                <Line type="monotone" dataKey="equity" stroke={COLORS.GREEN} strokeWidth={2} dot={{ r: 4 }} name="Equity" />
                <Line type="monotone" dataKey="netDebt" stroke={COLORS.ORANGE} strokeWidth={2} dot={{ r: 4 }} name="Net Debt" />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartWrapper>

          {/* Asset Composition */}
          <ChartWrapper title="Asset Mix" color={COLORS.GREEN} height={220}>
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={assetComposition}
                  cx="50%"
                  cy="50%"
                  innerRadius={35}
                  outerRadius={65}
                  paddingAngle={2}
                  dataKey="value"
                >
                  {assetComposition.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Pie>
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Legend wrapperStyle={{ fontSize: '10px' }} layout="vertical" align="right" verticalAlign="middle" />
              </PieChart>
            </ResponsiveContainer>
          </ChartWrapper>

          {/* Liability Composition */}
          <ChartWrapper title="Liability Mix" color={COLORS.RED} height={220}>
            <ResponsiveContainer width="100%" height="100%">
              <PieChart>
                <Pie
                  data={liabilityComposition}
                  cx="50%"
                  cy="50%"
                  innerRadius={35}
                  outerRadius={65}
                  paddingAngle={2}
                  dataKey="value"
                >
                  {liabilityComposition.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Pie>
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Legend wrapperStyle={{ fontSize: '10px' }} layout="vertical" align="right" verticalAlign="middle" />
              </PieChart>
            </ResponsiveContainer>
          </ChartWrapper>
        </div>
      </SectionPanel>

      {/* ===== CASH FLOW ANALYSIS ===== */}
      <SectionPanel
        title="Cash Flow Analysis"
        color={COLORS.ORANGE}
        icon={<CircleDollarSign size={16} color={COLORS.ORANGE} />}
      >
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          {/* Cash Flow Summary */}
          <div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '16px' }}>
              <MetricCard label="Operating CF" value={formatCurrency(operatingCashflow)} color={operatingCashflow >= 0 ? COLORS.GREEN : COLORS.RED} icon={<Zap size={16} />} />
              <MetricCard label="Investing CF" value={formatCurrency(processedData.investingCashflow)} color={COLORS.ORANGE} icon={<Building2 size={16} />} />
              <MetricCard label="Financing CF" value={formatCurrency(processedData.financingCashflow)} color={COLORS.PURPLE} icon={<Landmark size={16} />} />
              <MetricCard label="Free Cash Flow" value={formatCurrency(freeCashflow)} color={freeCashflow >= 0 ? COLORS.CYAN : COLORS.RED} icon={<PiggyBank size={16} />} subtitle={`${formatPercent(fcfMargin)} of revenue`} />
            </div>

            {/* Cash Flow Ratios */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
              <HealthScore label="FCF Margin" value={fcfMargin} max={0.5} thresholds={{ good: 0.1, warning: 0.05 }} format="percent" />
              <HealthScore label="CapEx / Revenue" value={capexToRevenue} max={0.3} thresholds={{ good: 0.05, warning: 0.1 }} higherIsBetter={false} format="percent" />
              <HealthScore label="CapEx / D&A" value={capexToDepreciation} max={3} thresholds={{ good: 1.2, warning: 2 }} format="ratio" description="Growth investment" />
              <HealthScore label="OCF / Current Liab" value={processedData.operatingCashflowRatio} max={2} thresholds={{ good: 1.0, warning: 0.5 }} format="ratio" />
            </div>
          </div>

          {/* Cash Flow Trend Chart */}
          <ChartWrapper title="Cash Flow Trends (Billions $)" color={COLORS.ORANGE} height={280}>
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart data={cashflowTrend} margin={{ top: 10, right: 30, left: 0, bottom: 10 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
                <XAxis dataKey="period" tick={{ fill: COLORS.GRAY, fontSize: 11 }} />
                <YAxis tick={{ fill: COLORS.GRAY, fontSize: 11 }} tickFormatter={(v) => `$${v.toFixed(0)}B`} />
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Legend wrapperStyle={{ fontSize: '11px' }} />
                <ReferenceLine y={0} stroke={COLORS.GRAY} strokeDasharray="3 3" />
                <Bar dataKey="operating" fill={COLORS.GREEN} name="Operating" radius={[4, 4, 0, 0]} />
                <Bar dataKey="investing" fill={COLORS.ORANGE} name="Investing" radius={[4, 4, 0, 0]} />
                <Bar dataKey="financing" fill={COLORS.PURPLE} name="Financing" radius={[4, 4, 0, 0]} />
                <Line type="monotone" dataKey="freeCashflow" stroke={COLORS.CYAN} strokeWidth={3} dot={{ r: 5 }} name="FCF" />
              </ComposedChart>
            </ResponsiveContainer>
          </ChartWrapper>
        </div>

        {/* Cash Flow Uses */}
        <div style={{ marginTop: '16px', display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
          <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ color: COLORS.ORANGE, fontSize: '11px', textTransform: 'uppercase', marginBottom: '6px' }}>Capital Expenditure</div>
            <div style={{ color: COLORS.WHITE, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(-capex)}</div>
          </div>
          <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ color: COLORS.GREEN, fontSize: '11px', textTransform: 'uppercase', marginBottom: '6px' }}>Dividends Paid</div>
            <div style={{ color: COLORS.WHITE, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(-processedData.dividendsPaid)}</div>
          </div>
          <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ color: COLORS.CYAN, fontSize: '11px', textTransform: 'uppercase', marginBottom: '6px' }}>Stock Repurchase</div>
            <div style={{ color: COLORS.WHITE, fontSize: '18px', fontWeight: 700 }}>{processedData.stockRepurchase > 0 ? formatCurrency(-processedData.stockRepurchase) : 'No Buybacks'}</div>
          </div>
          <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px', textAlign: 'center' }}>
            <div style={{ color: COLORS.PURPLE, fontSize: '11px', textTransform: 'uppercase', marginBottom: '6px' }}>Net Debt Change</div>
            <div style={{ color: COLORS.WHITE, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(processedData.debtIssuance - processedData.debtRepayment)}</div>
          </div>
        </div>
      </SectionPanel>

      {/* ===== EFFICIENCY METRICS ===== */}
      <SectionPanel
        title="Efficiency & Working Capital"
        color={COLORS.PURPLE}
        icon={<Clock size={16} color={COLORS.PURPLE} />}
      >
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '16px' }}>
          {/* Turnover Ratios */}
          <div>
            <div style={{ color: COLORS.PURPLE, fontSize: '12px', fontWeight: 600, marginBottom: '10px', paddingBottom: '6px', borderBottom: `1px solid ${COLORS.PURPLE}40` }}>
              TURNOVER RATIOS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <HealthScore label="Asset Turnover" value={assetTurnover} max={3} thresholds={{ good: 1.0, warning: 0.5 }} format="ratio" description="Revenue per $ of assets" />
              <HealthScore label="Inventory Turnover" value={inventoryTurnover} max={15} thresholds={{ good: 6, warning: 3 }} format="ratio" description="Inventory efficiency" />
              <HealthScore label="Receivables Turnover" value={receivablesTurnover} max={20} thresholds={{ good: 8, warning: 4 }} format="ratio" description="Collection efficiency" />
              <HealthScore label="Fixed Asset Turnover" value={processedData.fixedAssetTurnover} max={10} thresholds={{ good: 3, warning: 1.5 }} format="ratio" description="PP&E utilization" />
            </div>
          </div>

          {/* Cash Conversion Cycle */}
          <div>
            <div style={{ color: COLORS.CYAN, fontSize: '12px', fontWeight: 600, marginBottom: '10px', paddingBottom: '6px', borderBottom: `1px solid ${COLORS.CYAN}40` }}>
              CASH CONVERSION CYCLE
            </div>
            <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '16px', borderRadius: '4px' }}>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
                <div style={{ textAlign: 'center', flex: 1 }}>
                  <div style={{ color: COLORS.BLUE, fontSize: '20px', fontWeight: 700 }}>{daysInventory.toFixed(0)}</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>Days Inventory</div>
                </div>
                <div style={{ color: COLORS.MUTED, fontSize: '14px' }}>+</div>
                <div style={{ textAlign: 'center', flex: 1 }}>
                  <div style={{ color: COLORS.GREEN, fontSize: '20px', fontWeight: 700 }}>{daysReceivables.toFixed(0)}</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>Days Receivables</div>
                </div>
                <div style={{ color: COLORS.MUTED, fontSize: '14px' }}>−</div>
                <div style={{ textAlign: 'center', flex: 1 }}>
                  <div style={{ color: COLORS.ORANGE, fontSize: '20px', fontWeight: 700 }}>{daysPayables.toFixed(0)}</div>
                  <div style={{ color: COLORS.GRAY, fontSize: '10px' }}>Days Payables</div>
                </div>
              </div>
              <div style={{ borderTop: `1px solid ${COLORS.BORDER}`, paddingTop: '12px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '4px' }}>Cash Conversion Cycle</div>
                <div style={{ color: cashConversionCycle <= 30 ? COLORS.GREEN : cashConversionCycle <= 60 ? COLORS.YELLOW : COLORS.RED, fontSize: '24px', fontWeight: 700 }}>
                  {cashConversionCycle.toFixed(0)} days
                </div>
              </div>
            </div>

            {/* Four additional cards */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginTop: '12px' }}>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>Working Capital</div>
                <div style={{ color: workingCapital >= 0 ? COLORS.GREEN : COLORS.RED, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(workingCapital)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Current Assets - Current Liab</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>Operating Cycle</div>
                <div style={{ color: COLORS.PURPLE, fontSize: '18px', fontWeight: 700 }}>{(daysInventory + daysReceivables).toFixed(0)} days</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Days Inv + Days Receivables</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>WC Turnover</div>
                <div style={{ color: COLORS.BLUE, fontSize: '18px', fontWeight: 700 }}>{workingCapital !== 0 ? (revenue / workingCapital).toFixed(2) : '0.00'}x</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Revenue / Working Capital</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '12px', borderRadius: '4px', textAlign: 'center' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '10px', marginBottom: '4px', textTransform: 'uppercase' }}>Cash to Current Liab</div>
                <div style={{ color: cashRatio >= 0.5 ? COLORS.GREEN : cashRatio >= 0.2 ? COLORS.YELLOW : COLORS.RED, fontSize: '18px', fontWeight: 700 }}>{formatPercent(cashRatio)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px' }}>Immediate Liquidity Coverage</div>
              </div>
            </div>
          </div>

          {/* Efficiency Radar */}
          <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px', overflow: 'hidden' }}>
            <div style={{
              padding: '10px 14px',
              borderBottom: `2px solid ${COLORS.PURPLE}`,
              backgroundColor: COLORS.HEADER_BG
            }}>
              <span style={{ color: COLORS.PURPLE, fontSize: '12px', fontWeight: 700, textTransform: 'uppercase' }}>Efficiency Scorecard</span>
            </div>
            <div style={{ height: '280px', padding: 0 }}>
              <ResponsiveContainer width="100%" height="100%">
                <RadarChart cx="50%" cy="50%" outerRadius="70%" data={efficiencyRadar}>
                  <PolarGrid stroke={COLORS.BORDER} />
                  <PolarAngleAxis dataKey="metric" tick={{ fill: COLORS.GRAY, fontSize: 12 }} />
                  <PolarRadiusAxis angle={30} domain={[0, 100]} tick={false} axisLine={false} />
                  <Radar name="Efficiency" dataKey="value" stroke={COLORS.PURPLE} fill={COLORS.PURPLE} fillOpacity={0.5} strokeWidth={2} />
                </RadarChart>
              </ResponsiveContainer>
            </div>
          </div>
        </div>
      </SectionPanel>

      {/* ===== COST STRUCTURE ===== */}
      <SectionPanel
        title="Cost Structure & Operating Expenses"
        color={COLORS.YELLOW}
        icon={<Layers size={16} color={COLORS.YELLOW} />}
        defaultExpanded={false}
      >
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          {/* Operating Expenses Breakdown */}
          <ChartWrapper title="Operating Expense Breakdown" color={COLORS.YELLOW} height={250}>
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={opexBreakdown} layout="vertical" margin={{ top: 10, right: 30, left: 80, bottom: 10 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} opacity={0.3} />
                <XAxis type="number" tick={{ fill: COLORS.GRAY, fontSize: 11 }} tickFormatter={(v) => `$${v.toFixed(0)}B`} />
                <YAxis type="category" dataKey="name" tick={{ fill: COLORS.GRAY, fontSize: 11 }} width={80} />
                <Tooltip content={<CustomTooltip formatter={(v: number) => `$${v.toFixed(2)}B`} />} />
                <Bar dataKey="value" radius={[0, 4, 4, 0]}>
                  {opexBreakdown.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </ChartWrapper>

          {/* Cost Metrics */}
          <div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '6px' }}>Cost of Revenue</div>
                <div style={{ color: COLORS.RED, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(processedData.costOfRevenue)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '10px' }}>{formatPercent(1 - grossMargin)} of revenue</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '6px' }}>Operating Expenses</div>
                <div style={{ color: COLORS.ORANGE, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(processedData.operatingExpenses)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '10px' }}>{formatPercent(processedData.operatingExpenses / revenue)} of revenue</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '6px' }}>R&D Expense</div>
                <div style={{ color: COLORS.BLUE, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(processedData.researchDev)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '10px' }}>{formatPercent(processedData.researchDev / revenue)} of revenue</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '6px' }}>SG&A Expense</div>
                <div style={{ color: COLORS.PURPLE, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(processedData.sgaExpense)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '10px' }}>{formatPercent(processedData.sgaExpense / revenue)} of revenue</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '6px' }}>Interest Expense</div>
                <div style={{ color: COLORS.YELLOW, fontSize: '18px', fontWeight: 700 }}>{processedData.interestExpense > 0 ? formatCurrency(processedData.interestExpense) : 'Debt Free'}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '10px' }}>{processedData.interestExpense > 0 ? `${formatPercent(processedData.interestExpense / revenue)} of revenue` : 'No interest payments'}</div>
              </div>
              <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, padding: '14px', borderRadius: '4px' }}>
                <div style={{ color: COLORS.GRAY, fontSize: '11px', marginBottom: '6px' }}>Tax Expense</div>
                <div style={{ color: COLORS.CYAN, fontSize: '18px', fontWeight: 700 }}>{formatCurrency(processedData.taxExpense)}</div>
                <div style={{ color: COLORS.MUTED, fontSize: '10px' }}>Effective Rate: {revenue > 0 && operatingIncome > 0 ? formatPercent(processedData.taxExpense / operatingIncome) : 'N/A'}</div>
              </div>
            </div>
          </div>
        </div>
      </SectionPanel>

      {/* ===== DETAILED FINANCIAL STATEMENTS ===== */}
      <SectionPanel
        title="Detailed Financial Statements"
        color={COLORS.GRAY}
        icon={<Receipt size={16} color={COLORS.GRAY} />}
        defaultExpanded={false}
      >
        <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
          {[
            { key: 'income', label: 'Income Statement', color: COLORS.GREEN },
            { key: 'balance', label: 'Balance Sheet', color: COLORS.YELLOW },
            { key: 'cashflow', label: 'Cash Flow', color: COLORS.CYAN },
          ].map(({ key, label, color }) => (
            <button
              key={key}
              onClick={() => setActiveStatement(key as any)}
              style={{
                padding: '8px 16px',
                backgroundColor: activeStatement === key ? color : 'transparent',
                border: `1px solid ${activeStatement === key ? color : COLORS.BORDER}`,
                color: activeStatement === key ? COLORS.DARK_BG : color,
                fontSize: '12px',
                fontWeight: 600,
                cursor: 'pointer',
                textTransform: 'uppercase',
                fontFamily: TYPOGRAPHY.MONO,
                borderRadius: '4px',
                transition: 'all 0.2s ease',
              }}
            >
              {label}
            </button>
          ))}
        </div>
        <div style={{ backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, maxHeight: '400px', overflow: 'auto', borderRadius: '4px' }} className="custom-scrollbar">
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px' }}>
            <thead>
              <tr style={{ backgroundColor: COLORS.HEADER_BG, position: 'sticky', top: 0 }}>
                <th style={{ padding: '12px', textAlign: 'left', color: COLORS.ORANGE, fontWeight: 700, borderBottom: `2px solid ${COLORS.ORANGE}` }}>METRIC</th>
                {(activeStatement === 'income' ? incomeKeys : activeStatement === 'balance' ? processedData.balanceKeys : processedData.cashflowKeys).slice(0, 4).map((period) => (
                  <th key={period} style={{ padding: '12px', textAlign: 'right', color: COLORS.ORANGE, fontWeight: 700, borderBottom: `2px solid ${COLORS.ORANGE}` }}>{getYearFromPeriod(period)}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {Object.entries(activeStatement === 'income' ? latestIncome : activeStatement === 'balance' ? latestBalance : latestCashflow).map(([metric, _], idx) => {
                const periods = (activeStatement === 'income' ? incomeKeys : activeStatement === 'balance' ? processedData.balanceKeys : processedData.cashflowKeys).slice(0, 4);
                const statement = activeStatement === 'income' ? financials.income_statement : activeStatement === 'balance' ? financials.balance_sheet : financials.cash_flow;
                return (
                  <tr key={metric} style={{ borderBottom: BORDERS.STANDARD, backgroundColor: idx % 2 === 0 ? 'transparent' : `${COLORS.WHITE}05` }}>
                    <td style={{ padding: '10px 12px', color: COLORS.WHITE, fontWeight: 600 }}>{metric}</td>
                    {periods.map((period) => {
                      const value = statement[period]?.[metric];
                      return (
                        <td key={period} style={{ padding: '10px 12px', textAlign: 'right', color: value && value < 0 ? COLORS.RED : COLORS.CYAN, fontFamily: TYPOGRAPHY.MONO }}>
                          {value !== undefined ? formatLargeNumber(value) : 'N/A'}
                        </td>
                      );
                    })}
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      </SectionPanel>

      {/* ===== FINANCIAL HEALTH SUMMARY ===== */}
      <div style={{ backgroundColor: COLORS.PANEL_BG, border: BORDERS.STANDARD, padding: '16px', borderRadius: '4px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '16px', paddingBottom: '10px', borderBottom: `2px solid ${COLORS.CYAN}` }}>
          <Shield size={18} color={COLORS.CYAN} />
          <span style={{ color: COLORS.CYAN, fontSize: '14px', fontWeight: 700, textTransform: 'uppercase' }}>Financial Health Summary</span>
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px' }}>
          {/* Profitability */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '14px', backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px' }}>
            {netMargin >= 0.1 ? <CheckCircle size={24} color={COLORS.GREEN} /> : netMargin >= 0.05 ? <AlertTriangle size={24} color={COLORS.YELLOW} /> : <XCircle size={24} color={COLORS.RED} />}
            <div>
              <div style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 700 }}>Profitability</div>
              <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>{formatPercent(netMargin)} net margin</div>
              <div style={{ color: netMargin >= 0.1 ? COLORS.GREEN : netMargin >= 0.05 ? COLORS.YELLOW : COLORS.RED, fontSize: '10px', marginTop: '2px' }}>
                {netMargin >= 0.1 ? 'Strong' : netMargin >= 0.05 ? 'Moderate' : 'Weak'}
              </div>
            </div>
          </div>

          {/* Liquidity */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '14px', backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px' }}>
            {currentRatio >= 1.5 ? <CheckCircle size={24} color={COLORS.GREEN} /> : currentRatio >= 1.0 ? <AlertTriangle size={24} color={COLORS.YELLOW} /> : <XCircle size={24} color={COLORS.RED} />}
            <div>
              <div style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 700 }}>Liquidity</div>
              <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>{currentRatio.toFixed(2)}x current ratio</div>
              <div style={{ color: currentRatio >= 1.5 ? COLORS.GREEN : currentRatio >= 1.0 ? COLORS.YELLOW : COLORS.RED, fontSize: '10px', marginTop: '2px' }}>
                {currentRatio >= 1.5 ? 'Strong' : currentRatio >= 1.0 ? 'Adequate' : 'At Risk'}
              </div>
            </div>
          </div>

          {/* Leverage */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '14px', backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px' }}>
            {debtToEquity <= 0.5 ? <CheckCircle size={24} color={COLORS.GREEN} /> : debtToEquity <= 1.0 ? <AlertTriangle size={24} color={COLORS.YELLOW} /> : <XCircle size={24} color={COLORS.RED} />}
            <div>
              <div style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 700 }}>Leverage</div>
              <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>{debtToEquity.toFixed(2)}x D/E ratio</div>
              <div style={{ color: debtToEquity <= 0.5 ? COLORS.GREEN : debtToEquity <= 1.0 ? COLORS.YELLOW : COLORS.RED, fontSize: '10px', marginTop: '2px' }}>
                {debtToEquity <= 0.5 ? 'Conservative' : debtToEquity <= 1.0 ? 'Moderate' : 'High'}
              </div>
            </div>
          </div>

          {/* Cash Flow */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '14px', backgroundColor: COLORS.DARK_BG, border: BORDERS.STANDARD, borderRadius: '4px' }}>
            {freeCashflow > 0 && fcfMargin >= 0.1 ? <CheckCircle size={24} color={COLORS.GREEN} /> : freeCashflow > 0 ? <AlertTriangle size={24} color={COLORS.YELLOW} /> : <XCircle size={24} color={COLORS.RED} />}
            <div>
              <div style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 700 }}>Cash Generation</div>
              <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>{formatCurrency(freeCashflow)} FCF</div>
              <div style={{ color: freeCashflow > 0 && fcfMargin >= 0.1 ? COLORS.GREEN : freeCashflow > 0 ? COLORS.YELLOW : COLORS.RED, fontSize: '10px', marginTop: '2px' }}>
                {freeCashflow > 0 && fcfMargin >= 0.1 ? 'Excellent' : freeCashflow > 0 ? 'Positive' : 'Negative'}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default FinancialsTab;
