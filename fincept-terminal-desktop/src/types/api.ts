/**
 * API Response Types
 * 
 * Centralized type definitions for external API responses.
 * These types document the structure of data from various APIs.
 */

// =============================================================================
// Polygon.io API Types
// =============================================================================

/**
 * Polygon ticker details
 */
export interface PolygonTickerDetails {
    ticker: string;
    name: string;
    market: string;
    locale: string;
    primary_exchange: string;
    type: string;
    active: boolean;
    currency_name: string;
    cik?: string;
    composite_figi?: string;
    share_class_figi?: string;
    last_updated_utc?: string;
    delisted_utc?: string;
    description?: string;
    sic_code?: string;
    sic_description?: string;
    total_employees?: number;
    list_date?: string;
    market_cap?: number;
    homepage_url?: string;
    branding?: {
        logo_url?: string;
        icon_url?: string;
    };
    address?: {
        address1?: string;
        city?: string;
        state?: string;
        postal_code?: string;
    };
}

/**
 * Polygon quote/snapshot
 */
export interface PolygonQuote {
    ticker: string;
    todaysChange: number;
    todaysChangePerc: number;
    updated: number;
    day: {
        o: number;  // open
        h: number;  // high
        l: number;  // low
        c: number;  // close
        v: number;  // volume
        vw: number; // volume weighted
    };
    min?: {
        av: number;  // accumulated volume
        t: number;   // timestamp
        n: number;   // trades
        o: number;
        h: number;
        l: number;
        c: number;
        v: number;
        vw: number;
    };
    prevDay: {
        o: number;
        h: number;
        l: number;
        c: number;
        v: number;
        vw: number;
    };
}

/**
 * Polygon news article
 */
export interface PolygonNews {
    id: string;
    publisher: {
        name: string;
        homepage_url: string;
        logo_url: string;
        favicon_url: string;
    };
    title: string;
    author: string;
    published_utc: string;
    article_url: string;
    tickers: string[];
    amp_url?: string;
    image_url?: string;
    description?: string;
    keywords?: string[];
}

/**
 * Polygon aggregates (bars/candles)
 */
export interface PolygonAggregate {
    v: number;   // volume
    vw: number;  // volume weighted
    o: number;   // open
    c: number;   // close
    h: number;   // high
    l: number;   // low
    t: number;   // timestamp
    n: number;   // trades
}

/**
 * Polygon trade
 */
export interface PolygonTrade {
    conditions?: number[];
    exchange: number;
    id: string;
    participant_timestamp: number;
    price: number;
    sequence_number: number;
    sip_timestamp: number;
    size: number;
    tape: number;
}

/**
 * Polygon financial statement base
 */
export interface PolygonFinancialBase {
    cik: string;
    company_name: string;
    start_date: string;
    end_date: string;
    filing_date: string;
    fiscal_period: string;
    fiscal_year: string;
    source_filing_url: string;
    source_filing_file_url: string;
}

/**
 * Polygon balance sheet
 */
export interface PolygonBalanceSheet extends PolygonFinancialBase {
    financials: {
        balance_sheet: {
            assets: Record<string, { value: number; unit: string; label: string }>;
            liabilities: Record<string, { value: number; unit: string; label: string }>;
            equity: Record<string, { value: number; unit: string; label: string }>;
        };
    };
}

/**
 * Polygon income statement
 */
export interface PolygonIncomeStatement extends PolygonFinancialBase {
    financials: {
        income_statement: Record<string, { value: number; unit: string; label: string }>;
    };
}

/**
 * Polygon cash flow statement
 */
export interface PolygonCashFlow extends PolygonFinancialBase {
    financials: {
        cash_flow_statement: Record<string, { value: number; unit: string; label: string }>;
    };
}

/**
 * Polygon technical indicator result
 */
export interface PolygonIndicatorResult {
    results: {
        values: Array<{
            timestamp: number;
            value: number;
        }>;
    };
    status: string;
    request_id: string;
    next_url?: string;
}

/**
 * Common Polygon API response wrapper
 */
export interface PolygonResponse<T> {
    results: T;
    status: string;
    request_id: string;
    count?: number;
    next_url?: string;
}

// =============================================================================
// Forum API Types
// =============================================================================

/**
 * Forum category
 */
export interface ForumCategory {
    id: number;
    name: string;
    description?: string;
    post_count: number;
    color?: string;
    icon?: string;
}

/**
 * Forum post
 */
export interface ForumPost {
    uuid: string;
    title: string;
    content: string;
    author: {
        id: number;
        username: string;
        avatar_url?: string;
    };
    category_id: number;
    category_name?: string;
    created_at: string;
    updated_at: string;
    view_count: number;
    comment_count: number;
    like_count: number;
    is_pinned: boolean;
    tags?: string[];
}

/**
 * Forum comment
 */
export interface ForumComment {
    id: number;
    post_uuid: string;
    content: string;
    author: {
        id: number;
        username: string;
        avatar_url?: string;
    };
    created_at: string;
    updated_at: string;
    like_count: number;
    parent_id?: number;
}

/**
 * Forum stats
 */
export interface ForumStats {
    total_posts: number;
    total_comments: number;
    total_users: number;
    posts_today: number;
    active_users_today: number;
}

// =============================================================================
// Market Data Cache Types
// =============================================================================

/**
 * Cached market data entry
 */
export interface MarketDataCache {
    symbol: string;
    category: string;
    quote_data: string; // JSON stringified
    cached_at: string;
}

/**
 * Parsed market data
 */
export interface MarketQuoteData {
    symbol: string;
    price: number;
    change: number;
    changePercent: number;
    volume?: number;
    marketCap?: number;
    high?: number;
    low?: number;
    open?: number;
    previousClose?: number;
    timestamp: number;
}

// =============================================================================
// MCP Tool Types
// =============================================================================

/**
 * MCP tool parameter
 */
export interface MCPToolParameter {
    name: string;
    type: 'string' | 'number' | 'boolean' | 'object' | 'array';
    description?: string;
    required?: boolean;
    default?: unknown;
    enum?: string[];
}

// Note: MCPToolDefinition is defined in workflow.ts to avoid duplication

/**
 * MCP tool execution result
 */
export interface MCPToolResult {
    success: boolean;
    content?: unknown;
    error?: string;
    executionTime?: number;
}

// =============================================================================
// LLM API Types
// =============================================================================

/**
 * LLM chat message
 */
export interface LLMChatMessage {
    role: 'system' | 'user' | 'assistant' | 'function' | 'tool';
    content: string;
    name?: string;
    tool_call_id?: string;
}

/**
 * LLM usage stats
 */
export interface LLMUsage {
    promptTokens: number;
    completionTokens: number;
    totalTokens: number;
}

/**
 * LLM API response
 */
export interface LLMApiResponse {
    content: string;
    error?: string;
    usage?: LLMUsage;
    finishReason?: 'stop' | 'length' | 'tool_calls' | 'content_filter';
    model?: string;
}

// =============================================================================
// Generic API Response Types
// =============================================================================

/**
 * Standard API response wrapper
 */
export interface ApiResponse<T> {
    success: boolean;
    data?: T;
    error?: string;
    message?: string;
    timestamp?: number;
}

/**
 * Paginated API response
 */
export interface PaginatedResponse<T> {
    items: T[];
    total: number;
    page: number;
    pageSize: number;
    hasMore: boolean;
    nextCursor?: string;
}

/**
 * API error details
 */
export interface ApiError {
    code: string;
    message: string;
    details?: Record<string, unknown>;
    timestamp: number;
}
