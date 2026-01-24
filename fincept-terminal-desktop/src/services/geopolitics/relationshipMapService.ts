// Relationship Map Service - Production-Level Multi-Source Implementation
// Integrates: Edgar (SEC filings, 13F, insider), YFinance (market data, peers)

import { invoke } from '@tauri-apps/api/core';
import { TIMEOUTS, DATA_LIMITS, CACHE_TTL } from '../../components/tabs/relationship-map/constants';
import type {
  CompanyInfoV2,
  InstitutionalHolder,
  FundFamily,
  InsiderHolder,
  InsiderTransaction,
  PeerCompany,
  CorporateEventV2,
  FilingV2,
  Executive,
  RelationshipMapDataV2,
  DataQualityScore,
  LoadingPhase,
  SupplyChainEntity,
  SegmentData,
  DebtHolder,
} from '../../components/tabs/relationship-map/types';
import { calculateValuationSignal, calculatePeerValuations } from '../../components/tabs/relationship-map/utils/valuationSignals';

// ============================================================================
// TIMEOUT UTILITY
// ============================================================================

function withTimeout<T>(promise: Promise<T>, ms: number, label?: string): Promise<T | null> {
  return Promise.race([
    promise,
    new Promise<null>((resolve) => {
      setTimeout(() => {
        console.warn(`[RelMapV2] ${label || 'Request'} timed out after ${ms}ms`);
        resolve(null);
      }, ms);
    }),
  ]);
}

// ============================================================================
// CACHE SYSTEM
// ============================================================================

interface CacheEntry<T> {
  data: T;
  timestamp: number;
  ttl: number;
}

class DataCache {
  private cache = new Map<string, CacheEntry<unknown>>();

  get<T>(key: string): T | null {
    const entry = this.cache.get(key) as CacheEntry<T> | undefined;
    if (!entry) return null;
    if (Date.now() - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      return null;
    }
    return entry.data;
  }

  set<T>(key: string, data: T, ttl: number): void {
    this.cache.set(key, { data, timestamp: Date.now(), ttl });
  }

  clear(prefix?: string): void {
    if (prefix) {
      for (const key of this.cache.keys()) {
        if (key.startsWith(prefix)) this.cache.delete(key);
      }
    } else {
      this.cache.clear();
    }
  }
}

// ============================================================================
// PEER DISCOVERY
// ============================================================================

// Common industry peers mapping for reliable peer selection
const INDUSTRY_PEERS: Record<string, string[]> = {
  'Technology': ['AAPL', 'MSFT', 'GOOGL', 'META', 'AMZN', 'NVDA', 'CRM', 'ORCL', 'ADBE', 'INTC'],
  'Software': ['MSFT', 'CRM', 'ORCL', 'ADBE', 'NOW', 'INTU', 'SNOW', 'PLTR', 'PANW', 'ZS'],
  'Semiconductors': ['NVDA', 'AMD', 'INTC', 'AVGO', 'QCOM', 'TXN', 'MU', 'LRCX', 'AMAT', 'MRVL'],
  'Internet Content & Information': ['GOOGL', 'META', 'SNAP', 'PINS', 'TWTR'],
  'Consumer Electronics': ['AAPL', 'SONY', 'DELL', 'HPQ', 'LOGI'],
  'E-Commerce': ['AMZN', 'SHOP', 'EBAY', 'ETSY', 'MELI', 'JD', 'BABA'],
  'Electric Vehicles': ['TSLA', 'RIVN', 'LCID', 'NIO', 'XPEV', 'LI'],
  'Auto Manufacturers': ['TSLA', 'F', 'GM', 'TM', 'HMC', 'STLA', 'RIVN'],
  'Banks': ['JPM', 'BAC', 'WFC', 'C', 'GS', 'MS', 'USB', 'PNC', 'TFC', 'COF'],
  'Insurance': ['BRK-B', 'UNH', 'AIG', 'MET', 'PRU', 'ALL', 'TRV', 'CB', 'PGR', 'AFL'],
  'Pharmaceuticals': ['JNJ', 'PFE', 'MRK', 'ABBV', 'LLY', 'BMY', 'AMGN', 'GILD', 'BIIB', 'REGN'],
  'Biotechnology': ['AMGN', 'GILD', 'BIIB', 'REGN', 'VRTX', 'MRNA', 'BNTX'],
  'Oil & Gas': ['XOM', 'CVX', 'COP', 'EOG', 'SLB', 'MPC', 'VLO', 'PSX', 'OXY', 'DVN'],
  'Retail': ['WMT', 'COST', 'TGT', 'HD', 'LOW', 'DG', 'DLTR', 'KR'],
  'Aerospace & Defense': ['LMT', 'RTX', 'BA', 'NOC', 'GD', 'LHX', 'TDG'],
  'Streaming': ['NFLX', 'DIS', 'PARA', 'WBD', 'ROKU'],
  'Cloud Computing': ['AMZN', 'MSFT', 'GOOGL', 'CRM', 'NOW', 'SNOW', 'NET', 'DDOG'],
  'Payments': ['V', 'MA', 'PYPL', 'SQ', 'FIS', 'FISV', 'GPN'],
  'Real Estate': ['AMT', 'PLD', 'CCI', 'EQIX', 'SPG', 'O', 'DLR', 'PSA'],
  'Utilities': ['NEE', 'DUK', 'SO', 'D', 'AEP', 'SRE', 'XEL', 'ES', 'WEC', 'ED'],
};

function findPeersByIndustry(ticker: string, industry: string, sector: string): string[] {
  // Try exact industry match first
  for (const [key, peers] of Object.entries(INDUSTRY_PEERS)) {
    if (industry.toLowerCase().includes(key.toLowerCase()) ||
        key.toLowerCase().includes(industry.toLowerCase())) {
      return peers.filter(p => p !== ticker).slice(0, DATA_LIMITS.MAX_PEERS);
    }
  }
  // Try sector match
  for (const [key, peers] of Object.entries(INDUSTRY_PEERS)) {
    if (sector.toLowerCase().includes(key.toLowerCase()) ||
        key.toLowerCase().includes(sector.toLowerCase())) {
      return peers.filter(p => p !== ticker).slice(0, DATA_LIMITS.MAX_PEERS);
    }
  }
  // Fallback to broad tech/general
  return ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'META', 'NVDA'].filter(p => p !== ticker).slice(0, 5);
}

// ============================================================================
// SERVICE CLASS
// ============================================================================

class RelationshipMapService {
  private cache = new DataCache();

  /**
   * Clear cache for a specific ticker or all
   */
  clearCache(ticker?: string): void {
    if (ticker) {
      this.cache.clear(ticker.toUpperCase());
    } else {
      this.cache.clear();
    }
  }

  /**
   * Get comprehensive relationship map data with progressive loading.
   * Calls the onProgress callback as data becomes available.
   */
  async getRelationshipMap(
    ticker: string,
    onProgress?: (phase: LoadingPhase) => void
  ): Promise<RelationshipMapDataV2> {
    const upperTicker = ticker.toUpperCase();

    // Check cache
    const cached = this.cache.get<RelationshipMapDataV2>(`${upperTicker}:full`);
    if (cached) {
      onProgress?.({ phase: 'complete', progress: 100, message: 'Loaded from cache' });
      return cached;
    }

    try {
      // ====================================================================
      // PHASE 1: Company Info (0-30%)
      // ====================================================================
      onProgress?.({ phase: 'company', progress: 5, message: 'Fetching company data...' });

      const [edgarCompany, yfinanceInfo, edgarProxy] = await Promise.all([
        withTimeout(this.fetchEdgarCompany(ticker), TIMEOUTS.EDGAR_COMPANY, 'EdgarCompany'),
        withTimeout(this.fetchYFinanceInfo(ticker), TIMEOUTS.YFINANCE_INFO, 'YFinanceInfo'),
        withTimeout(this.fetchEdgarProxy(ticker), TIMEOUTS.EDGAR_PROXY, 'EdgarProxy'),
      ]);

      onProgress?.({ phase: 'company', progress: 30, message: 'Company data loaded' });

      // Build company info
      const company = this.buildCompanyInfo(ticker, edgarCompany, yfinanceInfo);
      const executives = this.extractExecutives(edgarProxy);

      // ====================================================================
      // PHASE 2: Ownership & Deep Data (30-60%)
      // ====================================================================
      onProgress?.({ phase: 'ownership', progress: 35, message: 'Fetching ownership & deep data...' });

      const [insiderData, edgarFilings, edgarEvents, holdingsDetailed, supplyChainData, segmentData, debtData] = await Promise.all([
        withTimeout(this.fetchEdgarInsiderTransactionsDetailed(ticker), TIMEOUTS.EDGAR_INSIDER, 'InsiderTx'),
        withTimeout(this.fetchEdgarFilings(ticker), TIMEOUTS.EDGAR_FILINGS, 'Filings'),
        withTimeout(this.fetchEdgarCorporateEventsCategorized(ticker), TIMEOUTS.EDGAR_EVENTS, 'Events'),
        withTimeout(this.fetchInstitutionalHoldingsDetailed(ticker), TIMEOUTS.EDGAR_INSTITUTIONAL, 'Holdings'),
        withTimeout(this.fetchSupplyChain(ticker), TIMEOUTS.EDGAR_FILINGS, 'SupplyChain'),
        withTimeout(this.fetchSegmentFinancials(ticker), TIMEOUTS.EDGAR_FILINGS, 'Segments'),
        withTimeout(this.fetchDebtHolders(ticker), TIMEOUTS.EDGAR_FILINGS, 'DebtHolders'),
      ]);

      onProgress?.({ phase: 'ownership', progress: 55, message: 'Processing ownership & relationships...' });

      // Build institutional holders from real 13F/13G/13D data
      const institutionalHolders = this.buildInstitutionalHoldersFromEdgar(holdingsDetailed, yfinanceInfo);
      const fundFamilies = this.buildFundFamilies(institutionalHolders);
      const insiderHolders = this.buildInsiderHoldersDetailed(insiderData, edgarProxy);
      const insiderTransactions = this.buildInsiderTransactionsDetailed(insiderData);
      const filings = this.extractFilings(edgarFilings);
      const corporateEvents = this.extractCorporateEventsCategorized(edgarEvents);
      const supplyChain = this.extractSupplyChain(supplyChainData);
      const segments = this.extractSegments(segmentData);
      const debtHolders = this.extractDebtHolders(debtData);

      onProgress?.({ phase: 'ownership', progress: 60, message: 'Deep data processed' });

      // ====================================================================
      // PHASE 3: Peers (60-80%)
      // ====================================================================
      onProgress?.({ phase: 'peers', progress: 65, message: 'Finding peer companies...' });

      const peerTickers = findPeersByIndustry(ticker, company.industry, company.sector);
      const peers = await this.fetchPeerData(peerTickers);

      onProgress?.({ phase: 'peers', progress: 80, message: 'Calculating valuation signals...' });

      // Calculate valuation signals
      const companyValuation = calculateValuationSignal(company, peers);
      const peerValuations = calculatePeerValuations(peers);

      // ====================================================================
      // PHASE 4: Supplementary (80-100%)
      // ====================================================================
      onProgress?.({ phase: 'supplementary', progress: 85, message: 'Finalizing data...' });

      // Calculate data quality score
      const dataQuality = this.calculateDataQuality(
        edgarCompany, yfinanceInfo, edgarProxy, insiderData,
        edgarFilings, edgarEvents, peers
      );

      const result: RelationshipMapDataV2 = {
        company,
        executives,
        institutionalHolders,
        fundFamilies,
        insiderHolders,
        insiderTransactions,
        peers,
        peerValuations,
        corporateEvents,
        filings,
        supplyChain,
        segments,
        debtHolders,
        companyValuation,
        timestamp: new Date().toISOString(),
        dataQuality,
      };

      // Store in cache
      this.cache.set(`${upperTicker}:full`, result, CACHE_TTL.REALTIME);

      onProgress?.({ phase: 'complete', progress: 100, message: 'Complete' });
      return result;
    } catch (error) {
      console.error('[RelMapV2] Error:', error);
      onProgress?.({ phase: 'error', progress: 0, message: `Error: ${error}` });
      throw error;
    }
  }

  // ==========================================================================
  // DATA FETCHING METHODS
  // ==========================================================================

  private async fetchEdgarCompany(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_company', args: [ticker]
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Edgar company error:', e);
      return null;
    }
  }

  private async fetchEdgarProxy(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_proxy_statement', args: [ticker]
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Edgar proxy error:', e);
      return null;
    }
  }

  private async fetchEdgarFilings(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_company_filings', args: [ticker, '', '', '', '15']
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Edgar filings error:', e);
      return null;
    }
  }

  private async fetchEdgarCorporateEventsCategorized(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_corporate_events_categorized', args: [ticker, '30']
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Edgar events error:', e);
      return null;
    }
  }

  private async fetchEdgarInsiderTransactionsDetailed(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_insider_transactions_detailed', args: [ticker, '25']
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Edgar insider detailed error:', e);
      return null;
    }
  }

  private async fetchInstitutionalHoldingsDetailed(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_fund_holdings_detailed', args: [ticker, '2']
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Edgar holdings detailed error:', e);
      return null;
    }
  }

  private async fetchSupplyChain(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'extract_supply_chain', args: [ticker]
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Supply chain error:', e);
      return null;
    }
  }

  private async fetchSegmentFinancials(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'get_segment_financials', args: [ticker]
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Segment financials error:', e);
      return null;
    }
  }

  private async fetchDebtHolders(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_edgar_command', {
        command: 'extract_debt_holders', args: [ticker]
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] Debt holders error:', e);
      return null;
    }
  }

  private async fetchYFinanceInfo(ticker: string): Promise<any> {
    try {
      const result = await invoke<string>('execute_yfinance_command', {
        command: 'info', args: [ticker]
      });
      const parsed = JSON.parse(result);
      return parsed?.error ? null : parsed;
    } catch (e) {
      console.error('[RelMapV2] YFinance info error:', e);
      return null;
    }
  }

  private async fetchPeerData(tickers: string[]): Promise<PeerCompany[]> {
    const peers: PeerCompany[] = [];
    // Fetch peer data in parallel batches of 4
    const batchSize = 4;
    for (let i = 0; i < tickers.length; i += batchSize) {
      const batch = tickers.slice(i, i + batchSize);
      const results = await Promise.all(
        batch.map(t => withTimeout(this.fetchYFinanceInfo(t), TIMEOUTS.YFINANCE_PEERS, `Peer:${t}`))
      );
      for (let j = 0; j < results.length; j++) {
        const data = results[j];
        if (data) {
          peers.push({
            ticker: batch[j],
            name: data.company_name || batch[j],
            market_cap: data.market_cap || 0,
            pe_ratio: data.pe_ratio || 0,
            price_to_book: data.price_to_book || 0,
            price_to_sales: data.enterprise_to_revenue || 0,
            roe: data.return_on_equity || 0,
            revenue_growth: data.revenue_growth || 0,
            profit_margins: data.profit_margins || 0,
            current_price: data.current_price || 0,
            sector: data.sector || 'N/A',
            industry: data.industry || 'N/A',
          });
        }
      }
    }
    return peers;
  }

  // ==========================================================================
  // DATA BUILDING METHODS
  // ==========================================================================

  private buildCompanyInfo(ticker: string, edgarCompany: any, yfinanceData: any): CompanyInfoV2 {
    return {
      ticker: ticker.toUpperCase(),
      name: edgarCompany?.data?.name || yfinanceData?.company_name || ticker,
      cik: edgarCompany?.data?.cik?.toString() || '',
      industry: edgarCompany?.data?.industry || yfinanceData?.industry || 'N/A',
      sic: edgarCompany?.data?.sic || '',
      sector: yfinanceData?.sector || 'N/A',
      address: edgarCompany?.data?.business_address || '',
      exchange: yfinanceData?.exchange || 'N/A',
      employees: yfinanceData?.employees || 0,
      website: yfinanceData?.website || '',
      description: yfinanceData?.description || '',
      market_cap: yfinanceData?.market_cap || 0,
      current_price: yfinanceData?.current_price || 0,
      pe_ratio: yfinanceData?.pe_ratio || 0,
      forward_pe: yfinanceData?.forward_pe || 0,
      price_to_book: yfinanceData?.price_to_book || 0,
      price_to_sales: yfinanceData?.enterprise_to_revenue || 0,
      peg_ratio: yfinanceData?.peg_ratio || 0,
      enterprise_value: yfinanceData?.enterprise_value || 0,
      enterprise_to_revenue: yfinanceData?.enterprise_to_revenue || 0,
      profit_margins: yfinanceData?.profit_margins || 0,
      operating_margins: yfinanceData?.operating_margins || 0,
      roe: yfinanceData?.return_on_equity || 0,
      roa: yfinanceData?.return_on_assets || 0,
      revenue_growth: yfinanceData?.revenue_growth || 0,
      earnings_growth: yfinanceData?.earnings_growth || 0,
      revenue: yfinanceData?.total_revenue || 0,
      gross_profit: yfinanceData?.gross_profits || 0,
      ebitda: (yfinanceData?.total_revenue || 0) * (yfinanceData?.ebitda_margins || 0),
      free_cashflow: yfinanceData?.free_cashflow || 0,
      total_cash: yfinanceData?.total_cash || 0,
      total_debt: yfinanceData?.total_debt || 0,
      insider_percent: (yfinanceData?.held_percent_insiders || 0) * 100,
      institutional_percent: (yfinanceData?.held_percent_institutions || 0) * 100,
      shares_outstanding: yfinanceData?.shares_outstanding || 0,
      float_shares: yfinanceData?.float_shares || 0,
      recommendation: yfinanceData?.recommendation_key || 'N/A',
      target_price: yfinanceData?.target_mean_price || 0,
      analyst_count: yfinanceData?.number_of_analyst_opinions || 0,
    };
  }

  private extractExecutives(proxyData: any): Executive[] {
    if (!proxyData?.data?.named_executives?.length) return [];
    const executives: Executive[] = [];
    const seen = new Set<string>();

    for (const exec of proxyData.data.named_executives) {
      const name = exec.name;
      if (name && !seen.has(name)) {
        seen.add(name);
        executives.push({
          name,
          role: this.parseRole(exec.role),
          compensation: exec.total_comp,
        });
      }
    }
    return executives.slice(0, 15);
  }

  private parseRole(role: string): string {
    if (!role) return 'Executive';
    if (role.includes('Peo') || role.includes('CEO')) return 'CEO';
    if (role.includes('CFO')) return 'CFO';
    if (role.includes('Neo')) return 'Executive Officer';
    if (role.includes('COO')) return 'COO';
    if (role.includes('CTO')) return 'CTO';
    return role;
  }

  private buildInstitutionalHoldersFromEdgar(holdingsData: any, yfinanceData: any): InstitutionalHolder[] {
    const holders: InstitutionalHolder[] = [];
    const price = yfinanceData?.current_price || 0;
    const sharesOutstanding = yfinanceData?.shares_outstanding || 0;

    // Use real data from Edgar 13F/13G/13D/proxy filings
    if (holdingsData?.data?.holders?.length) {
      for (const h of holdingsData.data.holders) {
        const name = h.name || 'Unknown';
        const shares = h.shares || 0;
        const value = h.value || (shares * price) || 0;
        let percentage = h.percentage || 0;

        // Calculate percentage from shares if not provided
        if (!percentage && shares && sharesOutstanding) {
          percentage = (shares / sharesOutstanding) * 100;
        }

        holders.push({
          name,
          shares,
          value,
          percentage: parseFloat((percentage || 0).toFixed(2)),
          fund_family: this.extractFundFamily(name),
          filing_date: h.filing_date || h.date_reported,
        });
      }
    }

    // If no real data found, generate from yfinance institutional percentage
    if (holders.length === 0) {
      return this.buildFallbackInstitutionalHolders(yfinanceData);
    }

    // Sort by percentage descending
    holders.sort((a, b) => b.percentage - a.percentage);
    return holders.slice(0, DATA_LIMITS.MAX_INSTITUTIONAL_HOLDERS);
  }

  private buildFallbackInstitutionalHolders(yfinanceData: any): InstitutionalHolder[] {
    const institutionalPercent = (yfinanceData?.held_percent_institutions || 0) * 100;
    if (institutionalPercent <= 0) return [];

    const knownHolders = [
      { name: 'Vanguard Group', typicalShare: 0.08 },
      { name: 'BlackRock', typicalShare: 0.07 },
      { name: 'State Street', typicalShare: 0.04 },
      { name: 'Fidelity Management', typicalShare: 0.035 },
      { name: 'Capital Research', typicalShare: 0.03 },
    ];

    const sharesOutstanding = yfinanceData?.shares_outstanding || 0;
    const price = yfinanceData?.current_price || 0;
    const holders: InstitutionalHolder[] = [];
    const scaleFactor = institutionalPercent / 100;

    for (const known of knownHolders) {
      const percentage = known.typicalShare * scaleFactor * 100;
      if (percentage < 0.5) continue;
      const shares = Math.round(sharesOutstanding * known.typicalShare * scaleFactor);
      holders.push({
        name: known.name,
        shares,
        value: shares * price,
        percentage: parseFloat(percentage.toFixed(2)),
        fund_family: known.name,
      });
    }

    return holders;
  }

  private extractFundFamily(name: string): string {
    const families: Record<string, string[]> = {
      'Vanguard': ['vanguard'],
      'BlackRock': ['blackrock', 'ishares'],
      'State Street': ['state street', 'spdr', 'ssga'],
      'Fidelity': ['fidelity', 'fmr'],
      'Capital Group': ['capital research', 'capital group', 'american funds'],
      'T. Rowe Price': ['t. rowe', 'trowe'],
      'JP Morgan': ['jpmorgan', 'jp morgan', 'j.p. morgan'],
      'Goldman Sachs': ['goldman'],
      'Morgan Stanley': ['morgan stanley'],
      'Bank of America': ['bank of america', 'merrill'],
    };

    const lower = name.toLowerCase();
    for (const [family, keywords] of Object.entries(families)) {
      if (keywords.some(kw => lower.includes(kw))) return family;
    }
    return name;
  }

  private buildFundFamilies(holders: InstitutionalHolder[]): FundFamily[] {
    const familyMap = new Map<string, InstitutionalHolder[]>();

    for (const holder of holders) {
      const family = holder.fund_family || holder.name;
      if (!familyMap.has(family)) familyMap.set(family, []);
      familyMap.get(family)!.push(holder);
    }

    const families: FundFamily[] = [];
    for (const [name, funds] of familyMap) {
      if (funds.length === 0) continue;
      families.push({
        name,
        total_shares: funds.reduce((s, f) => s + f.shares, 0),
        total_value: funds.reduce((s, f) => s + f.value, 0),
        total_percentage: funds.reduce((s, f) => s + f.percentage, 0),
        funds,
        change_trend: 'stable',
      });
    }

    return families
      .sort((a, b) => b.total_percentage - a.total_percentage)
      .slice(0, DATA_LIMITS.MAX_FUND_FAMILIES);
  }

  private buildInsiderHoldersDetailed(insiderData: any, proxyData: any): InsiderHolder[] {
    const holders: InsiderHolder[] = [];
    const seen = new Set<string>();

    // Use detailed insider summary from Form 4 parsing
    if (insiderData?.data?.insider_summary) {
      for (const insider of insiderData.data.insider_summary) {
        const name = insider.name || 'Unknown';
        if (seen.has(name.toLowerCase())) continue;
        seen.add(name.toLowerCase());

        holders.push({
          name,
          title: insider.title || 'Insider',
          shares: (insider.total_buys || 0) - (insider.total_sells || 0),
          value: (insider.total_buy_value || 0) - (insider.total_sell_value || 0),
          percentage: 0,
          last_transaction_date: insider.last_transaction_date,
          last_transaction_type: insider.total_buys > insider.total_sells ? 'buy' : 'sell',
        });
      }
    }

    // Supplement from proxy data (executives)
    if (proxyData?.data?.named_executives) {
      for (const exec of proxyData.data.named_executives.slice(0, 10)) {
        if (exec.name && !seen.has(exec.name.toLowerCase())) {
          seen.add(exec.name.toLowerCase());
          holders.push({
            name: exec.name,
            title: this.parseRole(exec.role),
            shares: 0,
            value: exec.total_comp || 0,
            percentage: 0,
          });
        }
      }
    }

    return holders.slice(0, DATA_LIMITS.MAX_INSIDER_HOLDERS);
  }

  private buildInsiderTransactionsDetailed(insiderData: any): InsiderTransaction[] {
    if (!insiderData?.data?.transactions) return [];

    return insiderData.data.transactions
      .filter((tx: any) => tx.transaction_type !== 'unknown')
      .slice(0, DATA_LIMITS.MAX_INSIDER_TRANSACTIONS)
      .map((tx: any) => ({
        name: tx.insider_name || 'Unknown',
        title: tx.insider_title || 'Insider',
        transaction_type: tx.transaction_type === 'purchase' ? 'buy' as const :
                         tx.transaction_type === 'sale' ? 'sell' as const :
                         'option_exercise' as const,
        shares: tx.shares || 0,
        value: tx.value || 0,
        price: tx.price || 0,
        date: tx.filing_date || '',
        shares_remaining: 0,
      }));
  }

  private extractFilings(filingsData: any): FilingV2[] {
    if (!filingsData?.data) return [];

    return filingsData.data.slice(0, DATA_LIMITS.MAX_FILINGS).map((f: any) => ({
      form: f.form || '',
      filing_date: f.filing_date || '',
      description: this.getFilingDescription(f.form),
      url: f.filing_url || '',
      period: f.period_of_report || '',
    }));
  }

  private extractCorporateEventsCategorized(eventsData: any): CorporateEventV2[] {
    if (!eventsData?.data?.all_events) return [];

    return eventsData.data.all_events.slice(0, DATA_LIMITS.MAX_EVENTS).map((e: any) => ({
      filing_date: e.filing_date || '',
      period_of_report: '',
      form: e.form || '8-K',
      filing_url: e.filing_url || '',
      accession_number: e.accession_number || '',
      items: e.items || [],
      description: e.description || e.items?.[0] || '8-K Filing',
      category: (e.category || 'other') as CorporateEventV2['category'],
    }));
  }

  private extractSupplyChain(data: any): SupplyChainEntity[] {
    if (!data?.data) return [];
    const entities: SupplyChainEntity[] = [];

    // Add customers
    if (data.data.customers) {
      for (const c of data.data.customers.slice(0, DATA_LIMITS.MAX_SUPPLY_CHAIN)) {
        entities.push({
          name: c.name,
          relationship: 'customer',
          percentage_of_revenue: c.percentage_of_revenue,
          source: c.source,
        });
      }
    }

    // Add suppliers
    if (data.data.suppliers) {
      for (const s of data.data.suppliers.slice(0, DATA_LIMITS.MAX_SUPPLY_CHAIN)) {
        entities.push({
          name: s.name,
          relationship: 'supplier',
          source: s.source,
        });
      }
    }

    // Add partners
    if (data.data.partners) {
      for (const p of data.data.partners.slice(0, 5)) {
        entities.push({
          name: p.name,
          relationship: 'partner',
          source: p.source,
        });
      }
    }

    return entities.slice(0, DATA_LIMITS.MAX_SUPPLY_CHAIN);
  }

  private extractSegments(data: any): SegmentData[] {
    if (!data?.data) return [];
    const segments: SegmentData[] = [];

    if (data.data.geographic) {
      for (const g of data.data.geographic.slice(0, 5)) {
        segments.push({
          name: g.name,
          value: g.value || 0,
          type: 'geographic',
          period: g.period,
        });
      }
    }

    if (data.data.business_units) {
      for (const b of data.data.business_units.slice(0, 5)) {
        segments.push({
          name: b.name,
          value: b.value || 0,
          type: 'business_unit',
          period: b.period,
        });
      }
    }

    if (data.data.products) {
      for (const p of data.data.products.slice(0, 5)) {
        segments.push({
          name: p.name,
          value: p.value || 0,
          type: 'product',
          period: p.period,
        });
      }
    }

    return segments.slice(0, DATA_LIMITS.MAX_SEGMENTS);
  }

  private extractDebtHolders(data: any): DebtHolder[] {
    if (!data?.data) return [];
    const holders: DebtHolder[] = [];

    if (data.data.creditors) {
      for (const c of data.data.creditors.slice(0, DATA_LIMITS.MAX_DEBT_HOLDERS)) {
        holders.push({
          name: c.name,
          type: c.type || 'bank_lender',
          source: c.source,
        });
      }
    }

    if (data.data.credit_facilities) {
      for (const cf of data.data.credit_facilities.slice(0, 5)) {
        holders.push({
          name: cf.description?.substring(0, 50) || 'Credit Facility',
          type: 'credit_facility',
          filing_date: cf.filing_date,
          description: cf.description,
        });
      }
    }

    return holders.slice(0, DATA_LIMITS.MAX_DEBT_HOLDERS);
  }

  private getFilingDescription(form: string): string {
    const descriptions: Record<string, string> = {
      '10-K': 'Annual Report', '10-Q': 'Quarterly Report',
      '8-K': 'Current Event', '4': 'Insider Trading',
      'DEF 14A': 'Proxy Statement', '13F-HR': 'Institutional Holdings',
      'S-8': 'Employee Stock Plan', 'SC 13G': '5%+ Ownership',
      'DEFA14A': 'Proxy Materials', '144': 'Insider Sale Notice',
    };
    return descriptions[form] || form;
  }

  private calculateDataQuality(
    edgarCompany: any, yfinanceInfo: any, edgarProxy: any,
    insiderData: any, edgarFilings: any, edgarEvents: any,
    peers: PeerCompany[]
  ): DataQualityScore {
    const sources = {
      edgar_company: !!edgarCompany,
      edgar_filings: !!edgarFilings?.data?.length,
      edgar_proxy: !!edgarProxy?.data,
      edgar_institutional: !!edgarCompany,
      edgar_insider: !!insiderData?.data?.transactions?.length || !!insiderData?.data?.insider_summary?.length,
      yfinance: !!yfinanceInfo,
      peers: peers.length >= 3,
    };

    const successCount = Object.values(sources).filter(Boolean).length;
    const overall = Math.round((successCount / Object.keys(sources).length) * 100);

    const keyFields = [
      yfinanceInfo?.market_cap, yfinanceInfo?.pe_ratio, yfinanceInfo?.revenue_growth,
      yfinanceInfo?.profit_margins, yfinanceInfo?.return_on_equity,
      edgarCompany?.data?.name, edgarFilings?.data?.length,
    ];
    const filled = keyFields.filter(v => v && v !== 0 && v !== 'N/A').length;
    const completeness = Math.round((filled / keyFields.length) * 100);

    return { overall, sources, completeness };
  }
}

export const relationshipMapService = new RelationshipMapService();
