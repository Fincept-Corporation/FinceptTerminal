// Edgar Tools MCP - SEC EDGAR data access via edgartools Python library
// Provides comprehensive access to SEC filings, financials, insider transactions, etc.

import { InternalTool } from '../types';
import { invoke } from '@tauri-apps/api/core';

interface CompanyTicker {
  cik: number;
  ticker: string;
  company: string;
}

/**
 * Load and cache all company tickers from SEC into SQLite
 */
async function loadCompanyTickers(): Promise<CompanyTicker[]> {
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
function fuzzySearchCompanies(query: string, companies: CompanyTicker[], maxResults: number = 10): CompanyTicker[] {
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
function buildArgsArray(command: string, params: any): any[] {
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
async function executeEdgarCommand(command: string, args: any[]): Promise<any> {
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
async function resolveTickerFromQuery(query: string): Promise<string | null> {
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

export const edgarTools: InternalTool[] = [
  // ============================================================================
  // UNIFIED EDGAR EXECUTOR - Handles ALL edgar_tools.py commands
  // ============================================================================

  {
    name: 'edgar_execute',
    description: 'Execute any SEC Edgar command. Automatically resolves company ticker from name. Supports 90+ commands for financials, filings, insider transactions, holdings, entity data, documents, batch operations, and storage management.',
    inputSchema: {
      type: 'object',
      properties: {
        command: {
          type: 'string',
          description: 'Edgar command to execute',
          enum: [
            // Configuration
            'set_identity', 'get_identity', 'configure_connection', 'clear_cache', 'get_storage_info',
            // Company
            'get_company', 'get_company_filings', 'get_corporate_events', 'get_latest_filing',
            'get_corporate_events_categorized',
            // Financials
            'get_financials', 'get_balance_sheet', 'get_income_statement', 'get_cash_flow',
            'get_company_facts', 'get_xbrl_data',
            // Insider & Holdings
            'get_insider_transactions', 'get_insider_transactions_detailed',
            'get_fund_holdings', 'get_fund_holdings_detailed',
            // Enhanced extraction
            'extract_supply_chain', 'get_segment_financials', 'extract_debt_holders',
            // Proxy
            'get_proxy_statement',
            // Current filings
            'get_current_filings', 'get_latest_filings',
            // Search
            'find_company', 'find', 'get_company_tickers',
            // Filing operations
            'get_filing_by_accession', 'get_filing_text', 'get_filings_by_date',
            // Entity operations
            'get_entity', 'get_entity_submissions',
            // Fund reports
            'get_fund_report',
            // Documents
            'get_filing_documents', 'get_filing_attachments',
            // Batch operations
            'download_filings_batch',
            // Storage management
            'analyze_storage', 'optimize_storage', 'cleanup_storage',
          ],
        },
        query: {
          type: 'string',
          description: 'Company name to search (e.g., "Apple", "Microsoft"). Will auto-resolve to ticker symbol.',
        },
        ticker: {
          type: 'string',
          description: 'Stock ticker symbol (e.g., AAPL, MSFT). Use this if you already know the exact ticker.',
        },
        form: {
          type: 'string',
          description: 'SEC form type (e.g., "10-K", "10-Q", "8-K", "DEF 14A", "13F-HR", "NPORT-P", "NPX")',
        },
        year: {
          type: 'number',
          description: 'Year filter for filings',
        },
        quarter: {
          type: 'number',
          description: 'Quarter filter (1-4)',
        },
        limit: {
          type: 'number',
          description: 'Maximum number of results to return',
        },
        periods: {
          type: 'number',
          description: 'Number of periods for financial data',
        },
        annual: {
          type: 'boolean',
          description: 'True for annual data, false for quarterly',
        },
        as_json: {
          type: 'boolean',
          description: 'Return as JSON format',
        },
        n: {
          type: 'number',
          description: 'Number of items',
        },
        quarters: {
          type: 'number',
          description: 'Number of quarters',
        },
        cik: {
          type: 'string',
          description: 'Company CIK number',
        },
        identifier: {
          type: 'string',
          description: 'Ticker, CIK, or accession number',
        },
        top_n: {
          type: 'number',
          description: 'Number of top results',
        },
        accession_number: {
          type: 'string',
          description: 'SEC accession number',
        },
        max_length: {
          type: 'number',
          description: 'Maximum text length',
        },
        page_size: {
          type: 'number',
          description: 'Results per page',
        },
        dry_run: {
          type: 'boolean',
          description: 'Dry run mode for cache operations',
        },
        verify_ssl: {
          type: 'boolean',
          description: 'Verify SSL certificates',
        },
        proxy: {
          type: 'string',
          description: 'Proxy server URL',
        },
        timeout: {
          type: 'number',
          description: 'Request timeout in seconds',
        },
        identity: {
          type: 'string',
          description: 'User identity for SEC (name + email)',
        },
        older_than_days: {
          type: 'number',
          description: 'Remove cache files older than this many days',
        },
      },
      required: ['command'],
    },
    handler: async (args) => {
      const { command, query, ticker, ...restParams } = args;

      // Auto-resolve ticker from query if provided
      let finalTicker = ticker;
      if (query && !ticker) {
        const resolvedTicker = await resolveTickerFromQuery(query);
        if (!resolvedTicker) {
          return {
            success: false,
            error: `Could not resolve ticker for company "${query}". Please verify the company name or provide exact ticker symbol.`,
          };
        }
        finalTicker = resolvedTicker;
      }

      // Build final params with resolved ticker
      const params = {
        ...restParams,
        ticker: finalTicker,
      };

      // Build args array
      const commandArgs = buildArgsArray(command, params);

      try {
        const result = await executeEdgarCommand(command, commandArgs);

        return {
          success: true,
          data: result,
          message: `Executed ${command} successfully`,
          resolvedTicker: finalTicker !== ticker ? finalTicker : undefined,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
          command,
          params,
        };
      }
    },
  },

  // ============================================================================
  // QUICK ACCESS SHORTCUTS - Common operations with simple interfaces
  // ============================================================================

  {
    name: 'edgar_get_company_overview',
    description: 'Get complete company overview including info, latest financials, and recent filings. One-stop query for company analysis.',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name (e.g., "Tesla", "Apple")' },
        ticker: { type: 'string', description: 'Or exact ticker symbol (e.g., TSLA, AAPL)' },
      },
      required: [],
    },
    handler: async (args) => {
      // Resolve ticker
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) {
          return { success: false, error: `Could not find company: ${args.query}` };
        }
      }

      if (!ticker) {
        return { success: false, error: 'Must provide either query or ticker' };
      }

      try {
        // Get company info
        const companyInfo = await executeEdgarCommand('get_company', [ticker]);

        // Get latest financials
        const financials = await executeEdgarCommand('get_financials', [ticker, 4, true]);

        // Get recent filings
        const filings = await executeEdgarCommand('get_company_filings', [ticker, '', '', '', 10]);

        return {
          success: true,
          data: {
            company: companyInfo,
            financials: financials,
            recent_filings: filings,
          },
          ticker,
          message: `Retrieved complete overview for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_search_company',
    description: 'Search for companies by name and get basic info with tickers',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name or partial name to search' },
        top_n: { type: 'number', description: 'Number of results (default: 10)' },
      },
      required: ['query'],
    },
    handler: async (args) => {
      const topN = args.top_n || 10;

      try {
        const result = await executeEdgarCommand('find_company', [args.query, topN]);
        return {
          success: true,
          data: result,
          message: `Found ${result.count || 0} companies matching "${args.query}"`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_financials',
    description: 'Get complete financial statements (income statement, balance sheet, cash flow) for any company. Supports both ANNUAL and QUARTERLY data. Use annual=false for quarterly/Q1/Q2/Q3/Q4/10-Q data. Returns beautifully formatted markdown tables.',
    inputSchema: {
      type: 'object',
      properties: {
        company_name: {
          type: 'string',
          description: 'Company name to get financials for (e.g., "Apple", "Microsoft", "Tesla", "S&P Global", "JPMorgan"). Will automatically resolve to correct ticker.'
        },
        periods: {
          type: 'number',
          description: 'Number of years/quarters to show (default: 3)'
        },
        annual: {
          type: 'boolean',
          description: 'IMPORTANT: Set to FALSE for quarterly/Q1/Q2/Q3/Q4/10-Q data. Set to TRUE (default) for annual/yearly/10-K data. If user says "quarterly", "Q4", "Q1", "10-Q" etc., use annual=false.'
        },
      },
      required: ['company_name'],
    },
    handler: async (args) => {
      console.log('[edgar_get_financials] Called with args:', JSON.stringify(args));

      // Try multiple possible parameter names
      let companyName = args.company_name || args.query || args.ticker || args.company || args.name || args.symbol;

      // If still nothing, try to find any string value in args
      if (!companyName) {
        const stringValues = Object.values(args).filter(v => typeof v === 'string' && v.length > 0);
        if (stringValues.length > 0) {
          companyName = stringValues[0];
          console.log(`[edgar_get_financials] Using first string value found: "${companyName}"`);
        }
      }

      if (!companyName) {
        console.error('[edgar_get_financials] ERROR: No company identifier found. Received args:', args);
        return {
          success: false,
          error: 'Missing company name. Please provide a company name like "Apple" or "Microsoft".',
          debug: { receivedKeys: Object.keys(args), receivedValues: args }
        };
      }

      console.log(`[edgar_get_financials] Resolving company: "${companyName}"`);
      const ticker = await resolveTickerFromQuery(companyName);

      if (!ticker) {
        return {
          success: false,
          error: `Could not find company "${companyName}". Please check the spelling or try a different company name.`
        };
      }

      console.log(`[edgar_get_financials] Resolved "${companyName}" → ${ticker}`);

      const periods = args.periods || 3;
      const annual = args.annual !== false; // default true

      try {
        const result = await executeEdgarCommand('get_financials', [ticker, periods, annual]);

        console.log('[edgar_get_financials] Raw result from Python:', JSON.stringify(result, null, 2).slice(0, 500));

        // Extract markdown and chart data from result
        const markdown = result?.data?.markdown || result?.markdown;
        const chartData = result?.data?.chart_data;

        console.log('[edgar_get_financials] chart_data:', chartData ? 'present' : 'MISSING');

        if (markdown) {
          // Return markdown + chart data for LLM to display
          const toolResult = {
            success: true,
            content: markdown,
            chart_data: chartData,
            ticker,
            company: result?.data?.company_name,
            message: `Financial statements for ${ticker} (${annual ? 'Annual' : 'Quarterly'})`,
          };
          console.log('[edgar_get_financials] Returning with chart_data:', !!chartData);
          return toolResult;
        }

        // Fallback to raw data
        return {
          success: true,
          data: result,
          ticker,
          message: `Retrieved ${annual ? 'annual' : 'quarterly'} financials for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_insider_activity',
    description: 'Get insider transaction activity (Form 4 filings) with buy/sell details',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        limit: { type: 'number', description: 'Number of transactions (default: 25)' },
        detailed: { type: 'boolean', description: 'Get detailed transaction breakdown (default: true)' },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) {
          return { success: false, error: `Could not find company: ${args.query}` };
        }
      }

      if (!ticker) {
        return { success: false, error: 'Must provide either query or ticker' };
      }

      const limit = args.limit || 25;
      const detailed = args.detailed !== false; // default true
      const command = detailed ? 'get_insider_transactions_detailed' : 'get_insider_transactions';

      try {
        const result = await executeEdgarCommand(command, [ticker, limit]);
        return {
          success: true,
          data: result,
          ticker,
          message: `Retrieved insider transactions for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_institutional_holdings',
    description: 'Get institutional holdings (13F filings) showing major fund ownership',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        quarters: { type: 'number', description: 'Number of quarters (default: 2)' },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) {
          return { success: false, error: `Could not find company: ${args.query}` };
        }
      }

      if (!ticker) {
        return { success: false, error: 'Must provide either query or ticker' };
      }

      const quarters = args.quarters || 2;

      try {
        const result = await executeEdgarCommand('get_fund_holdings_detailed', [ticker, quarters]);
        return {
          success: true,
          data: result,
          ticker,
          message: `Retrieved institutional holdings for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_corporate_events',
    description: 'Get corporate events from 8-K filings (M&A, management changes, earnings, etc.)',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        limit: { type: 'number', description: 'Number of events (default: 30)' },
        categorized: { type: 'boolean', description: 'Categorize by event type (default: true)' },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) {
          return { success: false, error: `Could not find company: ${args.query}` };
        }
      }

      if (!ticker) {
        return { success: false, error: 'Must provide either query or ticker' };
      }

      const limit = args.limit || 30;
      const categorized = args.categorized !== false; // default true
      const command = categorized ? 'get_corporate_events_categorized' : 'get_corporate_events';

      try {
        const result = await executeEdgarCommand(command, [ticker, limit]);
        return {
          success: true,
          data: result,
          ticker,
          message: `Retrieved corporate events for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_current_filings',
    description: 'Get real-time current SEC filings across all companies',
    inputSchema: {
      type: 'object',
      properties: {
        form: { type: 'string', description: 'Filter by form type (e.g., "10-K", "8-K")' },
        page_size: { type: 'number', description: 'Results per page (default: 40)' },
      },
      required: [],
    },
    handler: async (args) => {
      const form = args.form || '';
      const pageSize = args.page_size || 40;

      try {
        const result = await executeEdgarCommand('get_current_filings', [form, pageSize]);
        return {
          success: true,
          data: result,
          message: `Retrieved current ${form || 'all'} filings`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_entity_info',
    description: 'Get detailed entity information by CIK number',
    inputSchema: {
      type: 'object',
      properties: {
        cik: { type: 'string', description: 'Company CIK number' },
      },
      required: ['cik'],
    },
    handler: async (args) => {
      try {
        const result = await executeEdgarCommand('get_entity', [args.cik]);
        return {
          success: true,
          data: result,
          message: `Retrieved entity info for CIK ${args.cik}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_get_filing_documents',
    description: 'Get all documents and attachments from a specific filing',
    inputSchema: {
      type: 'object',
      properties: {
        accession_number: { type: 'string', description: 'SEC accession number' },
      },
      required: ['accession_number'],
    },
    handler: async (args) => {
      try {
        const documents = await executeEdgarCommand('get_filing_documents', [args.accession_number]);
        const attachments = await executeEdgarCommand('get_filing_attachments', [args.accession_number]);

        return {
          success: true,
          data: {
            documents,
            attachments,
          },
          message: `Retrieved documents for filing ${args.accession_number}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_analyze_storage',
    description: 'Analyze Edgar cache storage usage and get statistics',
    inputSchema: {
      type: 'object',
      properties: {},
      required: [],
    },
    handler: async () => {
      try {
        const result = await executeEdgarCommand('analyze_storage', []);
        return {
          success: true,
          data: result,
          message: 'Storage analysis completed',
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  // ============================================================================
  // NEW: 10-K SECTION EXTRACTION
  // ============================================================================

  {
    name: 'edgar_10k_sections',
    description: 'Extract specific sections from 10-K annual report. Get Item 1 (Business), Item 1A (Risk Factors), Item 7 (MD&A), Item 10 (Governance), and more.',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name to search' },
        ticker: { type: 'string', description: 'Or exact ticker symbol' },
        sections: {
          type: 'array',
          items: { type: 'string' },
          description: 'Sections to extract: business, risk_factors, legal_proceedings, md&a, controls, compensation, governance, all_items'
        },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) {
          return { success: false, error: `Could not find company: ${args.query}` };
        }
      }

      if (!ticker) {
        return { success: false, error: 'Must provide either query or ticker' };
      }

      const sections = args.sections || [];

      try {
        const result = await executeEdgarCommand('10k_sections', [ticker, ...sections]);
        return {
          success: true,
          data: result.data,
          ticker,
          message: `Extracted 10-K sections for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_10k_full_text',
    description: 'Get full text content of 10-K annual report',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        max_length: { type: 'number', description: 'Maximum text length' },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) return { success: false, error: `Could not find company: ${args.query}` };
      }
      if (!ticker) return { success: false, error: 'Must provide either query or ticker' };

      const maxLength = args.max_length || null;
      const commandArgs = maxLength ? [ticker, maxLength] : [ticker];

      try {
        const result = await executeEdgarCommand('10k_full_text', commandArgs);
        return {
          success: true,
          data: result.data,
          ticker,
          message: `Retrieved 10-K full text for ${ticker}`,
        };
      } catch (error: any) {
        return { success: false, error: error?.message || String(error) };
      }
    },
  },

  {
    name: 'edgar_10k_search',
    description: 'Search for text within 10-K annual report',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        search_query: { type: 'string', description: 'Text to search for' },
        max_results: { type: 'number', description: 'Maximum results (default: 10)' },
      },
      required: ['search_query'],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) return { success: false, error: `Could not find company: ${args.query}` };
      }
      if (!ticker) return { success: false, error: 'Must provide either query or ticker' };

      const searchQuery = args.search_query;
      const maxResults = args.max_results || 10;

      try {
        const result = await executeEdgarCommand('10k_search', [ticker, searchQuery, maxResults]);
        return {
          success: true,
          data: result.data,
          ticker,
          message: `Searched 10-K for "${searchQuery}"`,
        };
      } catch (error: any) {
        return { success: false, error: error?.message || String(error) };
      }
    },
  },

  // ============================================================================
  // NEW: 10-Q SECTION EXTRACTION
  // ============================================================================

  {
    name: 'edgar_10q_sections',
    description: 'Extract specific sections from 10-Q quarterly report. Get Part I Item 1 (Financials), Item 2 (MD&A), Part II Item 1A (Risk Factors), and more.',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name to search' },
        ticker: { type: 'string', description: 'Or exact ticker symbol' },
        sections: {
          type: 'array',
          items: { type: 'string' },
          description: 'Sections to extract: part_i_item_1, part_i_item_2, part_i_item_3, part_i_item_4, part_i_item_8, part_ii_item_1, part_ii_item_1a, part_ii_item_2, part_ii_item_5, part_ii_item_6'
        },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) {
          return { success: false, error: `Could not find company: ${args.query}` };
        }
      }

      if (!ticker) {
        return { success: false, error: 'Must provide either query or ticker' };
      }

      const sections = args.sections || [];

      try {
        const result = await executeEdgarCommand('10q_sections', [ticker, ...sections]);
        return {
          success: true,
          data: result.data,
          ticker,
          message: `Extracted 10-Q sections for ${ticker}`,
        };
      } catch (error: any) {
        return {
          success: false,
          error: error?.message || String(error),
        };
      }
    },
  },

  {
    name: 'edgar_10q_full_text',
    description: 'Get full text content of 10-Q quarterly report',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        max_length: { type: 'number', description: 'Maximum text length' },
      },
      required: [],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) return { success: false, error: `Could not find company: ${args.query}` };
      }
      if (!ticker) return { success: false, error: 'Must provide either query or ticker' };

      const maxLength = args.max_length || null;
      const commandArgs = maxLength ? [ticker, maxLength] : [ticker];

      try {
        const result = await executeEdgarCommand('10q_full_text', commandArgs);
        return {
          success: true,
          data: result.data,
          ticker,
          message: `Retrieved 10-Q full text for ${ticker}`,
        };
      } catch (error: any) {
        return { success: false, error: error?.message || String(error) };
      }
    },
  },

  {
    name: 'edgar_10q_search',
    description: 'Search for text within 10-Q quarterly report',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Company name' },
        ticker: { type: 'string', description: 'Or ticker symbol' },
        search_query: { type: 'string', description: 'Text to search for' },
        max_results: { type: 'number', description: 'Maximum results (default: 10)' },
      },
      required: ['search_query'],
    },
    handler: async (args) => {
      let ticker = args.ticker;
      if (args.query && !ticker) {
        ticker = await resolveTickerFromQuery(args.query);
        if (!ticker) return { success: false, error: `Could not find company: ${args.query}` };
      }
      if (!ticker) return { success: false, error: 'Must provide either query or ticker' };

      const searchQuery = args.search_query;
      const maxResults = args.max_results || 10;

      try {
        const result = await executeEdgarCommand('10q_search', [ticker, searchQuery, maxResults]);
        return {
          success: true,
          data: result.data,
          ticker,
          message: `Searched 10-Q for "${searchQuery}"`,
        };
      } catch (error: any) {
        return { success: false, error: error?.message || String(error) };
      }
    },
  },
];
