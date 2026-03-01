/**
 * DealComparison - Types and Constants
 */

export type AnalysisType = 'compare' | 'rank' | 'benchmark' | 'payment' | 'industry';

export type RankCriteria = 'premium' | 'deal_value' | 'ev_revenue' | 'ev_ebitda' | 'synergies';

export interface ComparisonDeal {
  deal_id: string;
  acquirer_name: string;
  target_name: string;
  deal_value: number;
  announced_date: string;
  status: string;
  premium_1day: number;
  ev_revenue?: number;
  ev_ebitda?: number;
  synergies?: number;
  payment_cash_pct: number;
  payment_stock_pct: number;
  industry: string;
}

export const DEFAULT_DEALS: ComparisonDeal[] = [
  {
    deal_id: '1', acquirer_name: 'TechCorp Inc', target_name: 'StartupA', deal_value: 2500,
    announced_date: '2024-01-15', status: 'completed', premium_1day: 35, ev_revenue: 8.5,
    ev_ebitda: 18.2, synergies: 150, payment_cash_pct: 100, payment_stock_pct: 0, industry: 'Technology',
  },
  {
    deal_id: '2', acquirer_name: 'MegaCorp', target_name: 'GrowthCo', deal_value: 4200,
    announced_date: '2024-02-20', status: 'completed', premium_1day: 28, ev_revenue: 6.2,
    ev_ebitda: 14.5, synergies: 280, payment_cash_pct: 50, payment_stock_pct: 50, industry: 'Technology',
  },
  {
    deal_id: '3', acquirer_name: 'GlobalTech', target_name: 'InnovateCo', deal_value: 1800,
    announced_date: '2024-03-10', status: 'pending', premium_1day: 42, ev_revenue: 10.1,
    ev_ebitda: 22.0, synergies: 95, payment_cash_pct: 0, payment_stock_pct: 100, industry: 'Healthcare',
  },
];

export const RANK_CRITERIA_OPTIONS: { value: RankCriteria; label: string; desc: string }[] = [
  { value: 'premium', label: 'Premium Paid', desc: 'Rank by 1-day premium to target shareholders' },
  { value: 'deal_value', label: 'Deal Value', desc: 'Rank by total transaction value' },
  { value: 'ev_revenue', label: 'EV/Revenue', desc: 'Rank by enterprise value to revenue multiple' },
  { value: 'ev_ebitda', label: 'EV/EBITDA', desc: 'Rank by enterprise value to EBITDA multiple' },
  { value: 'synergies', label: 'Synergies', desc: 'Rank by estimated synergy value' },
];
