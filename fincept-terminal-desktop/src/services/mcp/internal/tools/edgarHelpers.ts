// Edgar Tools MCP - Helper functions for SEC EDGAR data access

import { invoke } from '@tauri-apps/api/core';

export interface CompanyTicker {
  cik: number;
  ticker: string;
  company: string;
}

/**
 * Load and cache all company tickers from SEC into SQLite
 */
export async function loadCompanyTickers(): Promise<CompanyTicker[]> {
  try {
    // Check if cache is already populated
    const isPopulated = await invoke<boolean>('edgar_cache_is_populated');

    if (isPopulated) {
      console.log('[Edgar Cache] Using SQLite cache');
      const companies = await invoke<CompanyTicker[]>('edgar_cache_get_all_companies');
      console.log(`[Edgar Cache] Loaded ${companies.length} companies from SQLite`);
      return companies;
    }

    console.log('[Edgar Cache] Populating SQLite cache from SEC...');

    // Get ALL tickers from SEC (no limit)
    const result = await executeEdgarCommand('get_company_tickers', [999999]);

    if (result.success && result.data) {
      const tickers: CompanyTicker[] = result.data;

      // Store in SQLite
      await invoke('edgar_cache_store_tickers', { tickers });

      console.log(`[Edgar Cache] Stored ${tickers.length} companies in SQLite`);
      return tickers;
    }

    throw new Error('Failed to load company tickers from SEC');
  } catch (error) {
    console.error('[Edgar Cache] Error loading tickers:', error);
    // Try to return whatever is in cache
    try {
      return await invoke<CompanyTicker[]>('edgar_cache_get_all_companies');
    } catch {
      return [];
    }
  }
}

/**
 * Common company name aliases and abbreviations
 */
const COMPANY_ALIASES: Record<string, string[]> = {
  'sp': ['s&p', 'sandp', 's and p', 'standard and poors', 'standard poors'],
  'snp': ['s&p', 'sandp', 's and p', 'standard and poors', 'standard poors'],
  's&p': ['sp', 'snp', 'sandp', 'standard and poors'],
  'jp': ['jpmorgan', 'jp morgan'],
  'jpmorgan': ['jp', 'jp morgan'],
  'ms': ['morgan stanley'],
  'gs': ['goldman', 'goldman sachs'],
  'bofa': ['bank of america'],
  'citi': ['citigroup', 'citibank'],
  'wells': ['wells fargo'],
  'amex': ['american express'],
};

/**
 * Normalize query for better matching
 */
function normalizeQuery(query: string): string[] {
  const lower = query.toLowerCase().replace(/[^\w\s&]/g, '');
  const variants = [lower];

  // Check for known aliases
  for (const [abbrev, aliases] of Object.entries(COMPANY_ALIASES)) {
    if (lower.includes(abbrev)) {
      for (const alias of aliases) {
        variants.push(lower.replace(abbrev, alias));
      }
    }
  }

  return variants;
}

/**
 * Fuzzy search company tickers from cache with improved scoring
 */
export function fuzzySearchCompanies(query: string, companies: CompanyTicker[], maxResults: number = 10): CompanyTicker[] {
  const queryVariants = normalizeQuery(query);
  const queryLower = query.toLowerCase().replace(/[^\w\s&]/g, '');
  const queryWords = queryLower.split(/\s+/).filter(w => w.length > 0);

  if (queryWords.length === 0) return [];

  const scoredResults = companies.map(company => {
    const nameLower = company.company.toLowerCase().replace(/[^\w\s&]/g, '');
    const tickerLower = company.ticker.toLowerCase();
    let score = 0;

    // Check all query variants
    for (const variant of queryVariants) {
      const variantWords = variant.split(/\s+/).filter(w => w.length > 0);

      // Exact ticker match = highest priority
      if (tickerLower === variant.replace(/\s+/g, '')) {
        score = Math.max(score, 10000);
      }
      // Ticker starts with query (no spaces)
      else if (tickerLower.startsWith(variant.replace(/\s+/g, ''))) {
        score = Math.max(score, 5000);
      }
      // Exact company name match
      else if (nameLower === variant) {
        score = Math.max(score, 2000);
      }
      // Company name starts with query
      else if (nameLower.startsWith(variant)) {
        score = Math.max(score, 1000);
      }
      // Company name contains exact query phrase
      else if (nameLower.includes(variant)) {
        score = Math.max(score, 800);
      }
      // Word-based matching
      else {
        const nameWords = nameLower.split(/\s+/);

        // Count matching words with position bonus
        let wordScore = 0;
        let matchedCount = 0;

        for (let i = 0; i < variantWords.length; i++) {
          const qw = variantWords[i];
          for (let j = 0; j < nameWords.length; j++) {
            const nw = nameWords[j];

            // Exact word match
            if (nw === qw) {
              wordScore += 100;
              matchedCount++;
              // Bonus for matching at same position
              if (i === j) wordScore += 50;
            }
            // Word starts with query word
            else if (nw.startsWith(qw) && qw.length >= 2) {
              wordScore += 60;
              matchedCount++;
            }
            // Query word starts with name word (e.g., "global" matches "glob")
            else if (qw.startsWith(nw) && nw.length >= 3) {
              wordScore += 40;
              matchedCount++;
            }
          }
        }

        // Only count if we matched most words
        if (matchedCount >= Math.ceil(variantWords.length * 0.5)) {
          // Bonus for matching all query words
          if (matchedCount >= variantWords.length) {
            wordScore += 200;
          }
          score = Math.max(score, wordScore);
        }
      }
    }

    // Prefer shorter names (more specific companies) - but less aggressively
    if (score > 0) {
      score -= company.company.length * 0.02;
    }

    return { company, score };
  });

  // Filter and sort by score
  return scoredResults
    .filter(r => r.score > 0)
    .sort((a, b) => b.score - a.score)
    .slice(0, maxResults)
    .map(r => r.company);
}

/**
 * Helper to build args array from params object based on command signature
 */
export function buildArgsArray(command: string, params: any): any[] {
  const args: any[] = [];

  // Map of command -> expected argument order
  const commandSignatures: Record<string, string[]> = {
    // Configuration
    'set_identity': ['identity'],
    'get_identity': [],
    'configure_connection': ['verify_ssl', 'proxy', 'timeout'],
    'clear_cache': ['dry_run'],
    'get_storage_info': [],

    // Company operations
    'get_company': ['ticker'],
    'get_company_filings': ['ticker', 'form', 'year', 'quarter', 'limit'],
    'get_corporate_events': ['ticker', 'limit'],
    'get_latest_filing': ['ticker', 'form', 'n'],
    'get_corporate_events_categorized': ['ticker', 'limit'],

    // Financials
    'get_financials': ['ticker', 'periods', 'annual'],
    'get_balance_sheet': ['ticker', 'periods', 'annual', 'as_json'],
    'get_income_statement': ['ticker', 'periods', 'annual', 'as_json'],
    'get_cash_flow': ['ticker', 'periods', 'annual', 'as_json'],
    'get_company_facts': ['cik'],
    'get_xbrl_data': ['ticker', 'form'],

    // Insider transactions
    'get_insider_transactions': ['ticker', 'limit'],
    'get_insider_transactions_detailed': ['ticker', 'limit'],

    // Fund holdings
    'get_fund_holdings': ['ticker', 'limit'],
    'get_fund_holdings_detailed': ['ticker', 'quarters'],

    // Enhanced data extraction
    'extract_supply_chain': ['ticker'],
    'get_segment_financials': ['ticker'],
    'extract_debt_holders': ['ticker'],

    // Proxy statements
    'get_proxy_statement': ['ticker'],

    // Current filings
    'get_current_filings': ['form', 'page_size'],
    'get_latest_filings': ['form', 'page_size'],

    // Search & discovery
    'find_company': ['query', 'top_n'],
    'find': ['identifier'],
    'get_company_tickers': ['limit'],

    // Filing operations
    'get_filing_by_accession': ['accession_number'],
    'get_filing_text': ['accession_number', 'max_length'],
    'get_filings_by_date': ['year', 'quarter', 'form', 'limit'],

    // Entity operations
    'get_entity': ['cik'],
    'get_entity_submissions': ['cik'],

    // Fund reports (NPX/NPORT)
    'get_fund_report': ['ticker', 'form'],

    // Document & attachment operations
    'get_filing_documents': ['accession_number'],
    'get_filing_attachments': ['accession_number'],

    // Batch operations
    'download_filings_batch': ['ticker', 'form', 'year', 'limit'],

    // Storage management
    'analyze_storage': [],
    'optimize_storage': ['dry_run'],
    'cleanup_storage': ['older_than_days'],
  };

  const signature = commandSignatures[command];
  if (!signature) {
    // Unknown command, pass all params as-is
    return Object.values(params);
  }

  // Build args in correct order
  for (const paramName of signature) {
    const value = params[paramName];
    if (value !== undefined && value !== null) {
      args.push(value);
    }
  }

  return args;
}

/**
 * Execute Edgar tools command via Python script
 */
export async function executeEdgarCommand(command: string, args: any[]): Promise<any> {
  try {
    // Build args: [command, ...params] - all must be strings
    const scriptArgs = [command, ...args.map(arg => String(arg))];

    const result = await invoke('execute_python_script', {
      app: null, // Will be injected by Tauri
      scriptName: 'edgar_mcp.py',
      args: scriptArgs,
      env: {}
    });

    // Parse JSON response from Python
    const parsed = typeof result === 'string' ? JSON.parse(result) : result;
    return parsed;
  } catch (error) {
    console.error(`[Edgar MCP] Error executing ${command}:`, error);
    throw error;
  }
}

/**
 * Auto-resolve ticker from company name query using cached fuzzy matching
 */
export async function resolveTickerFromQuery(query: string): Promise<string | null> {
  try {
    // Load all companies into cache (only happens once per 24h)
    const allCompanies = await loadCompanyTickers();

    if (allCompanies.length === 0) {
      console.warn('[Edgar MCP] No company data available in cache');
      return null;
    }

    // Fuzzy search against cached data
    const matches = fuzzySearchCompanies(query, allCompanies, 5);

    if (matches.length > 0) {
      const bestMatch = matches[0];
      console.log(`[Edgar MCP] Resolved "${query}" → "${bestMatch.ticker}" (${bestMatch.company}) from ${allCompanies.length} companies`);
      return bestMatch.ticker;
    }

    console.warn(`[Edgar MCP] No matches found for "${query}"`);
    return null;
  } catch (error) {
    console.warn(`[Edgar MCP] Failed to resolve ticker for "${query}":`, error);
    return null;
  }
}
