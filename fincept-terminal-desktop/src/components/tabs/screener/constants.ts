import type { PopularSeries, PopularCategory } from './types';

export const POPULAR_SERIES: PopularSeries[] = [
  { id: 'GDP', name: 'Gross Domestic Product', cat: 'Economic' },
  { id: 'UNRATE', name: 'Unemployment Rate', cat: 'Labor' },
  { id: 'CPIAUCSL', name: 'Consumer Price Index', cat: 'Inflation' },
  { id: 'FEDFUNDS', name: 'Federal Funds Rate', cat: 'Interest Rates' },
  { id: 'DGS10', name: '10-Year Treasury Rate', cat: 'Bonds' },
  { id: 'DEXUSEU', name: 'USD/EUR Exchange Rate', cat: 'Forex' },
  { id: 'DCOILWTICO', name: 'WTI Crude Oil Price', cat: 'Commodities' },
  { id: 'GOLDAMGBD228NLBM', name: 'Gold Price', cat: 'Commodities' }
];

export const POPULAR_CATEGORIES: PopularCategory[] = [
  { id: 32991, name: 'Money, Banking, & Finance', desc: 'Money supply, interest rates, banking data' },
  { id: 10, name: 'Population, Employment, Labor', desc: 'Employment, unemployment, labor statistics' },
  { id: 32455, name: 'Prices', desc: 'Inflation, CPI, PPI, commodity prices' },
  { id: 1, name: 'Production & Business', desc: 'Industrial production, capacity utilization' },
  { id: 32263, name: 'International Data', desc: 'Trade, exchange rates, foreign markets' },
  { id: 33060, name: 'U.S. Regional Data', desc: 'State and metro area statistics' },
];

// Cache duration in milliseconds (5 minutes)
export const CACHE_DURATION = 5 * 60 * 1000;

// Search debounce delay
export const SEARCH_DEBOUNCE_MS = 300;

// Chart colors
export const CHART_COLORS = ['#ea580c', '#22c55e', '#3b82f6', '#eab308', '#ef4444'];
