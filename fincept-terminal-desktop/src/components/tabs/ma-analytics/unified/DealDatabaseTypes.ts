/**
 * DealDatabase - Types and Constants
 */

import { MA_COLORS } from '../constants';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';

export interface SECFiling {
  accession_number: string;
  filing_type: string;
  filing_date: string;
  company_name: string;
  cik: string;
  filing_url: string;
  confidence_score: number;
  deal_indicators?: string;
}

export type PanelView = 'deal' | 'filing' | 'summary' | 'empty';
export type LeftTab = 'deals' | 'filings';

export const ACCENT = MA_COLORS.dealDb;

export const STATUS_COLORS: Record<string, string> = {
  'Completed': FINCEPT.GREEN,
  'completed': FINCEPT.GREEN,
  'pending': '#FFC400',
  'Pending': '#FFC400',
  'announced': FINCEPT.CYAN,
  'Announced': FINCEPT.CYAN,
  'withdrawn': FINCEPT.RED,
  'Withdrawn': FINCEPT.RED,
  'unknown': FINCEPT.GRAY,
};

export const CONFIDENCE_COLOR = (score: number) =>
  score >= 0.9 ? FINCEPT.GREEN : score >= 0.75 ? '#FFC400' : score >= 0.5 ? ACCENT : FINCEPT.GRAY;

export const FORM_TYPE_LABELS: Record<string, string> = {
  'DEFM14A': 'Definitive Merger Proxy',
  'PREM14A': 'Preliminary Merger Proxy',
  'S-4': 'Registration (Merger)',
  'SC TO-T': 'Tender Offer (3rd Party)',
  'SC TO-I': 'Tender Offer (Issuer)',
  'SC 13E-3': 'Going Private',
  '8-K': 'Material Event',
  '425': 'Merger Communication',
};
