/**
 * API Response Types
 * 
 * Centralized type definitions for external API responses.
 * These types document the structure of data from various APIs.
 */

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
