// Relationship Map Service - Production-Level Implementation
// Combines Edgar (SEC filings) + yfinance (market data) for comprehensive company intelligence

import { invoke } from '@tauri-apps/api/core';

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

export interface CompanyInfo {
  ticker: string;
  name: string;
  cik: string;
  industry: string;
  sic: string;
  sector?: string;
  address?: string;
  exchange?: string;
  employees?: number;
  website?: string;
  description?: string;
  market_cap?: number;
}

export interface Executive {
  name: string;
  role: string;
  compensation?: number;
}

export interface InstitutionalHolder {
  name: string;
  value: number;
  shares?: number;
  percentage?: number;
}

export interface Filing {
  form: string;
  filing_date: string;
  description?: string;
  url?: string;
  period?: string;
}

export interface CorporateEvent {
  filing_date: string;
  period_of_report?: string;
  form: string;
  filing_url?: string;
  accession_number: string;
  items?: string[];
  description?: string;
}

export interface BalanceSheetMetrics {
  total_assets: number;
  total_liabilities: number;
  stockholders_equity: number;
  shares_outstanding: number;
  total_cash?: number;
  total_debt?: number;
}

export interface FinancialMetrics {
  revenue: number;
  gross_profit: number;
  operating_cashflow: number;
  free_cashflow: number;
  ebitda_margins: number;
  profit_margins: number;
  revenue_growth: number;
  earnings_growth: number;
}

export interface AnalystData {
  recommendation: string;
  recommendation_mean?: number;
  target_price: number;
  target_high: number;
  target_low: number;
  analyst_count: number;
}

export interface OwnershipData {
  insider_percent: number;
  institutional_percent: number;
  float_shares: number;
  shares_outstanding: number;
}

export interface ValuationMetrics {
  pe_ratio: number;
  forward_pe: number;
  price_to_book: number;
  enterprise_value: number;
  enterprise_to_revenue: number;
  peg_ratio: number;
}

export interface RelationshipMapData {
  company: CompanyInfo;
  executives: Executive[];
  holders: InstitutionalHolder[];
  filings: Filing[];
  corporate_events: CorporateEvent[];
  balance_sheet: BalanceSheetMetrics;
  financials: FinancialMetrics;
  analysts: AnalystData;
  ownership: OwnershipData;
  valuation: ValuationMetrics;
  timestamp: string;
}

// ============================================================================
// SERVICE CLASS
// ============================================================================

// Cache configuration
const CACHE_DURATION = 5 * 60 * 1000; // 5 minutes

interface CacheEntry<T> {
  data: T;
  timestamp: number;
}

class RelationshipMapService {
  // In-memory cache for relationship data
  private cache: Map<string, CacheEntry<RelationshipMapData>> = new Map();

  /**
   * Check if cache entry is still valid
   */
  private isCacheValid<T>(entry: CacheEntry<T> | undefined): boolean {
    if (!entry) return false;
    return Date.now() - entry.timestamp < CACHE_DURATION;
  }

  /**
   * Clear cache for a specific ticker or all
   */
  clearCache(ticker?: string): void {
    if (ticker) {
      this.cache.delete(ticker.toUpperCase());
    } else {
      this.cache.clear();
    }
  }

  /**
   * Get comprehensive relationship map data for a ticker
   */
  async getRelationshipMap(ticker: string): Promise<RelationshipMapData> {
    const upperTicker = ticker.toUpperCase();

    // Check cache first
    const cached = this.cache.get(upperTicker);
    if (this.isCacheValid(cached)) {
      console.log(`[RelationshipMapService] Cache hit for ${upperTicker}`);
      return cached!.data;
    }

    console.log(`[RelationshipMapService] Cache miss for ${upperTicker}, fetching...`);
    try {
      const [edgarCompany, edgarProxy, edgarFilings, edgarEvents, yfinanceInfo] = await Promise.allSettled([
        this.getEdgarCompany(ticker),
        this.getEdgarProxy(ticker),
        this.getEdgarFilings(ticker),
        this.getEdgarCorporateEvents(ticker),
        this.getYFinanceInfo(ticker)
      ]);

      const companyData = edgarCompany.status === 'fulfilled' ? edgarCompany.value : null;
      const proxyData = edgarProxy.status === 'fulfilled' ? edgarProxy.value : null;
      const filingsData = edgarFilings.status === 'fulfilled' ? edgarFilings.value : null;
      const eventsData = edgarEvents.status === 'fulfilled' ? edgarEvents.value : null;
      const yfinanceData = yfinanceInfo.status === 'fulfilled' ? yfinanceInfo.value : null;

      const result = this.combineData(ticker, companyData, proxyData, filingsData, eventsData, yfinanceData);

      // Store in cache
      this.cache.set(upperTicker, {
        data: result,
        timestamp: Date.now(),
      });

      return result;
    } catch (error) {
      console.error('[RelationshipMapService] Error:', error);
      throw error;
    }
  }

  /**
   * Fetch Edgar company info
   */
  private async getEdgarCompany(ticker: string) {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_company',
        args: [ticker]
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[Edgar] Company error:', error);
      return null;
    }
  }

  /**
   * Fetch Edgar proxy statement
   */
  private async getEdgarProxy(ticker: string) {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_proxy_statement',
        args: [ticker]
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[Edgar] Proxy error:', error);
      return null;
    }
  }

  /**
   * Fetch Edgar filings
   */
  private async getEdgarFilings(ticker: string) {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_company_filings',
        args: [ticker]
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[Edgar] Filings error:', error);
      return null;
    }
  }

  /**
   * Fetch Edgar corporate events (8-K filings)
   */
  private async getEdgarCorporateEvents(ticker: string) {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_corporate_events',
        args: [ticker]
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[Edgar] Corporate Events error:', error);
      return null;
    }
  }

  /**
   * Fetch yfinance data
   */
  private async getYFinanceInfo(ticker: string) {
    try {
      const result = await invoke<string>('execute_yfinance_command', {
        command: 'info',
        args: [ticker]
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[YFinance] Info error:', error);
      return null;
    }
  }

  /**
   * Combine all data sources
   */
  private combineData(
    ticker: string,
    edgarCompany: any,
    edgarProxy: any,
    edgarFilings: any,
    edgarEvents: any,
    yfinanceData: any
  ): RelationshipMapData {

    // Extract company info
    const company: CompanyInfo = {
      ticker: ticker.toUpperCase(),
      name: edgarCompany?.data?.name || yfinanceData?.company_name || ticker,
      cik: edgarCompany?.data?.cik?.toString() || '',
      industry: edgarCompany?.data?.industry || yfinanceData?.industry || 'N/A',
      sic: edgarCompany?.data?.sic || '',
      sector: yfinanceData?.sector || 'N/A',
      address: edgarCompany?.data?.business_address || '',
      exchange: yfinanceData?.exchange || 'N/A',
      employees: yfinanceData?.employees,
      website: yfinanceData?.website,
      description: yfinanceData?.description,
      market_cap: yfinanceData?.market_cap,
    };

    // Extract executives
    const executives: Executive[] = this.extractExecutives(edgarProxy);

    // Extract holders (placeholder - will implement when yfinance adds institutional_holders)
    const holders: InstitutionalHolder[] = this.extractHolders(yfinanceData);

    // Extract filings
    const filings: Filing[] = this.extractFilings(edgarFilings);

    // Extract corporate events
    const corporate_events: CorporateEvent[] = this.extractCorporateEvents(edgarEvents);

    // Extract balance sheet
    const balance_sheet: BalanceSheetMetrics = {
      total_assets: yfinanceData?.total_assets || 0,
      total_liabilities: yfinanceData?.total_liabilities || 0,
      stockholders_equity: (yfinanceData?.total_assets || 0) - (yfinanceData?.total_liabilities || 0),
      shares_outstanding: yfinanceData?.shares_outstanding || 0,
      total_cash: yfinanceData?.total_cash || 0,
      total_debt: yfinanceData?.total_debt || 0,
    };

    // Extract financial metrics
    const financials: FinancialMetrics = {
      revenue: yfinanceData?.total_revenue || 0,
      gross_profit: yfinanceData?.gross_profits || 0,
      operating_cashflow: yfinanceData?.operating_cashflow || 0,
      free_cashflow: yfinanceData?.free_cashflow || 0,
      ebitda_margins: yfinanceData?.ebitda_margins || 0,
      profit_margins: yfinanceData?.profit_margins || 0,
      revenue_growth: yfinanceData?.revenue_growth || 0,
      earnings_growth: yfinanceData?.earnings_growth || 0,
    };

    // Extract analyst data
    const analysts: AnalystData = {
      recommendation: yfinanceData?.recommendation_key || 'N/A',
      recommendation_mean: yfinanceData?.recommendation_mean,
      target_price: yfinanceData?.target_mean_price || 0,
      target_high: yfinanceData?.target_high_price || 0,
      target_low: yfinanceData?.target_low_price || 0,
      analyst_count: yfinanceData?.number_of_analyst_opinions || 0,
    };

    // Extract ownership
    const ownership: OwnershipData = {
      insider_percent: (yfinanceData?.held_percent_insiders || 0) * 100,
      institutional_percent: (yfinanceData?.held_percent_institutions || 0) * 100,
      float_shares: yfinanceData?.float_shares || 0,
      shares_outstanding: yfinanceData?.shares_outstanding || 0,
    };

    // Extract valuation
    const valuation: ValuationMetrics = {
      pe_ratio: yfinanceData?.pe_ratio || 0,
      forward_pe: yfinanceData?.forward_pe || 0,
      price_to_book: yfinanceData?.price_to_book || 0,
      enterprise_value: yfinanceData?.enterprise_value || 0,
      enterprise_to_revenue: yfinanceData?.enterprise_to_revenue || 0,
      peg_ratio: yfinanceData?.peg_ratio || 0,
    };

    return {
      company,
      executives,
      holders,
      filings,
      corporate_events,
      balance_sheet,
      financials,
      analysts,
      ownership,
      valuation,
      timestamp: new Date().toISOString()
    };
  }

  /**
   * Extract corporate events from Edgar data
   */
  private extractCorporateEvents(eventsData: any): CorporateEvent[] {
    if (!eventsData?.data) {
      return [];
    }

    return eventsData.data.slice(0, 15).map((event: any) => ({
      filing_date: event.filing_date || '',
      period_of_report: event.period_of_report || '',
      form: event.form || '8-K',
      filing_url: event.filing_url || '',
      accession_number: event.accession_number || '',
      items: event.items || [],
      description: event.description || '',
    }));
  }

  /**
   * Extract executives from proxy data
   */
  private extractExecutives(proxyData: any): Executive[] {
    if (!proxyData?.data?.named_executives || proxyData.data.named_executives.length === 0) {
      return [];
    }

    const executives: Executive[] = [];
    const seen = new Set<string>();

    for (const exec of proxyData.data.named_executives) {
      const name = exec.name;
      if (name && !seen.has(name)) {
        seen.add(name);
        executives.push({
          name,
          role: this.parseRole(exec.role),
          compensation: exec.total_comp
        });
      }
    }

    return executives.slice(0, 20);
  }

  /**
   * Parse executive role
   */
  private parseRole(role: string): string {
    if (!role) return 'Executive';
    if (role.includes('Peo') || role.includes('CEO')) return 'CEO';
    if (role.includes('CFO')) return 'CFO';
    if (role.includes('Neo')) return 'Executive Officer';
    if (role.includes('COO')) return 'COO';
    if (role.includes('CTO')) return 'CTO';
    return role;
  }

  /**
   * Extract institutional holders (placeholder)
   */
  private extractHolders(yfinanceData: any): InstitutionalHolder[] {
    // TODO: Implement when yfinance_data.py adds institutional_holders method
    return [];
  }

  /**
   * Extract recent filings
   */
  private extractFilings(filingsData: any): Filing[] {
    if (!filingsData?.data) {
      return [];
    }

    return filingsData.data.slice(0, 20).map((filing: any) => ({
      form: filing.form || '',
      filing_date: filing.filing_date || '',
      description: this.getFilingDescription(filing.form),
      url: filing.filing_url || '',
      period: filing.period_of_report || ''
    }));
  }

  /**
   * Get filing description
   */
  private getFilingDescription(form: string): string {
    const descriptions: Record<string, string> = {
      '10-K': 'Annual Report',
      '10-Q': 'Quarterly Report',
      '8-K': 'Current Event',
      '4': 'Insider Trading',
      'DEF 14A': 'Proxy Statement',
      '13F-HR': 'Institutional Holdings',
      'S-8': 'Employee Stock Plan',
      'SCHEDULE 13G/A': '5%+ Ownership Amendment',
      'DEFA14A': 'Proxy Additional Materials',
      '144': 'Insider Sale Notice'
    };
    return descriptions[form] || form;
  }
}

export const relationshipMapService = new RelationshipMapService();
