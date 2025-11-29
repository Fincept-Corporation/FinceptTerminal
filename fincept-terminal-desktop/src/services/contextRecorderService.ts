/**
 * Context Recorder Service
 * Records and manages data contexts from tabs for AI chat integration
 */

import { sqliteService, RecordedContext, RecordingSession } from './sqliteService';

export interface RecordingOptions {
  autoRecord?: boolean;
  dataTypes?: string[]; // Filter: ['api_response', 'websocket', 'table_data']
  label?: string;
  tags?: string[];
}

export interface ContextMetadata {
  endpoint?: string;
  timestamp: string;
  filters?: Record<string, any>;
  session_id?: string;
  [key: string]: any;
}

class ContextRecorderService {
  private activeSessions = new Map<string, string>(); // tabName -> sessionId

  /**
   * Initialize the service
   */
  async initialize(): Promise<void> {
    if (!sqliteService.isReady()) {
      await sqliteService.initialize();
    }
    console.log('[ContextRecorder] Service initialized');
  }

  // =========================
  // RECORDING CONTROL
  // =========================

  /**
   * Start recording for a tab
   */
  async startRecording(tabName: string, options?: RecordingOptions): Promise<string> {
    const sessionId = crypto.randomUUID();

    const session: Omit<RecordingSession, 'started_at'> = {
      id: sessionId,
      tab_name: tabName,
      is_active: true,
      auto_record: options?.autoRecord || false,
      filters: options?.dataTypes ? JSON.stringify({ dataTypes: options.dataTypes }) : undefined
    };

    await sqliteService.startRecordingSession(session);
    this.activeSessions.set(tabName, sessionId);

    console.log(`[ContextRecorder] Started recording for ${tabName} (Session: ${sessionId})`);
    return sessionId;
  }

  /**
   * Stop recording for a tab
   */
  async stopRecording(tabName: string): Promise<void> {
    await sqliteService.stopRecordingSession(tabName);
    this.activeSessions.delete(tabName);
    console.log(`[ContextRecorder] Stopped recording for ${tabName}`);
  }

  /**
   * Check if tab is currently recording
   */
  async isRecording(tabName: string): Promise<boolean> {
    const session = await sqliteService.getActiveRecordingSession(tabName);
    return session !== null && session.is_active;
  }

  /**
   * Get active session for tab
   */
  async getActiveSession(tabName: string): Promise<RecordingSession | null> {
    return await sqliteService.getActiveRecordingSession(tabName);
  }

  // =========================
  // DATA RECORDING
  // =========================

  /**
   * Record data from a tab
   */
  async recordData(
    tabName: string,
    dataType: string,
    data: any,
    label?: string,
    tags?: string[],
    metadata?: ContextMetadata
  ): Promise<string | null> {
    // Check if recording is active
    const session = await sqliteService.getActiveRecordingSession(tabName);

    if (!session || !session.is_active) {
      console.warn(`[ContextRecorder] No active recording session for ${tabName}`);
      return null;
    }

    // Check if this data type should be recorded
    if (session.filters) {
      try {
        const filters = JSON.parse(session.filters);
        if (filters.dataTypes && !filters.dataTypes.includes(dataType)) {
          console.log(`[ContextRecorder] Skipping ${dataType} - filtered out`);
          return null;
        }
      } catch (e) {
        console.error('[ContextRecorder] Error parsing filters:', e);
      }
    }

    const contextId = crypto.randomUUID();
    const rawData = typeof data === 'string' ? data : JSON.stringify(data, null, 2);

    const fullMetadata: ContextMetadata = {
      ...metadata,
      timestamp: new Date().toISOString(),
      session_id: session.id
    };

    const context: Omit<RecordedContext, 'created_at'> = {
      id: contextId,
      tab_name: tabName,
      data_type: dataType,
      label: label || `${tabName} - ${dataType}`,
      raw_data: rawData,
      metadata: JSON.stringify(fullMetadata),
      data_size: 0, // Will be calculated in saveRecordedContext
      tags: tags?.join(',')
    };

    await sqliteService.saveRecordedContext(context);

    console.log(`[ContextRecorder] Recorded ${dataType} for ${tabName} (ID: ${contextId}, Size: ${rawData.length} bytes)`);
    return contextId;
  }

  /**
   * Record API response
   */
  async recordApiResponse(
    tabName: string,
    endpoint: string,
    response: any,
    label?: string,
    tags?: string[]
  ): Promise<string | null> {
    return await this.recordData(
      tabName,
      'api_response',
      response,
      label || `API: ${endpoint}`,
      tags,
      { endpoint, timestamp: new Date().toISOString() }
    );
  }

  /**
   * Record WebSocket data
   */
  async recordWebSocketData(
    tabName: string,
    data: any,
    label?: string,
    tags?: string[]
  ): Promise<string | null> {
    return await this.recordData(
      tabName,
      'websocket',
      data,
      label || 'WebSocket Data',
      tags
    );
  }

  /**
   * Record table/chart data
   */
  async recordTableData(
    tabName: string,
    data: any,
    label?: string,
    tags?: string[]
  ): Promise<string | null> {
    return await this.recordData(
      tabName,
      'table_data',
      data,
      label || 'Table Data',
      tags
    );
  }

  // =========================
  // CONTEXT RETRIEVAL
  // =========================

  /**
   * Get all recorded contexts with filters
   */
  async getRecordedContexts(filters?: {
    tabName?: string;
    dataType?: string;
    tags?: string[];
    limit?: number;
  }): Promise<RecordedContext[]> {
    return await sqliteService.getRecordedContexts(filters);
  }

  /**
   * Get context by ID
   */
  async getContextById(id: string): Promise<RecordedContext | null> {
    return await sqliteService.getRecordedContext(id);
  }

  /**
   * Get contexts for specific tab
   */
  async getContextsByTab(tabName: string, limit?: number): Promise<RecordedContext[]> {
    return await sqliteService.getRecordedContexts({ tabName, limit });
  }

  /**
   * Search contexts
   */
  async searchContexts(query: string, limit: number = 50): Promise<RecordedContext[]> {
    const allContexts = await sqliteService.getRecordedContexts({ limit: 1000 });

    const lowerQuery = query.toLowerCase();
    return allContexts.filter(ctx =>
      ctx.label?.toLowerCase().includes(lowerQuery) ||
      ctx.tab_name.toLowerCase().includes(lowerQuery) ||
      ctx.data_type.toLowerCase().includes(lowerQuery) ||
      ctx.tags?.toLowerCase().includes(lowerQuery) ||
      ctx.raw_data.toLowerCase().includes(lowerQuery)
    ).slice(0, limit);
  }

  // =========================
  // CHAT INTEGRATION
  // =========================

  /**
   * Link context to chat session
   */
  async linkToChat(chatSessionUuid: string, contextId: string): Promise<void> {
    await sqliteService.linkContextToChat(chatSessionUuid, contextId);
    console.log(`[ContextRecorder] Linked context ${contextId} to chat ${chatSessionUuid}`);
  }

  /**
   * Unlink context from chat session
   */
  async unlinkFromChat(chatSessionUuid: string, contextId: string): Promise<void> {
    await sqliteService.unlinkContextFromChat(chatSessionUuid, contextId);
    console.log(`[ContextRecorder] Unlinked context ${contextId} from chat ${chatSessionUuid}`);
  }

  /**
   * Get all contexts linked to a chat session
   */
  async getChatContexts(chatSessionUuid: string): Promise<RecordedContext[]> {
    return await sqliteService.getChatContexts(chatSessionUuid);
  }

  /**
   * Toggle context active state in chat
   */
  async toggleChatContextActive(chatSessionUuid: string, contextId: string): Promise<void> {
    await sqliteService.toggleChatContextActive(chatSessionUuid, contextId);
  }

  /**
   * Format context for LLM (returns formatted string for AI consumption)
   */
  async formatContextForLLM(contextId: string, format: 'json' | 'markdown' | 'summary' = 'markdown'): Promise<string> {
    const context = await sqliteService.getRecordedContext(contextId);

    if (!context) {
      return '';
    }

    if (format === 'json') {
      return `## Context: ${context.label}
**Source:** ${context.tab_name}
**Type:** ${context.data_type}
**Recorded:** ${new Date(context.created_at).toLocaleString()}

\`\`\`json
${context.raw_data}
\`\`\``;
    }

    if (format === 'markdown') {
      return `## Context: ${context.label}

- **Source Tab:** ${context.tab_name}
- **Data Type:** ${context.data_type}
- **Recorded At:** ${new Date(context.created_at).toLocaleString()}
${context.tags ? `- **Tags:** ${context.tags}` : ''}

### Data:
\`\`\`
${context.raw_data}
\`\`\``;
    }

    // Summary format - extract key info
    try {
      const data = JSON.parse(context.raw_data);
      const summary = this.generateSummary(data);
      return `## ${context.label}
**From:** ${context.tab_name}
**Summary:** ${summary}`;
    } catch {
      // If not JSON, return truncated raw data
      const truncated = context.raw_data.length > 500
        ? context.raw_data.substring(0, 500) + '...'
        : context.raw_data;
      return `## ${context.label}
**From:** ${context.tab_name}
**Data:** ${truncated}`;
    }
  }

  /**
   * Format multiple contexts for LLM
   */
  async formatMultipleContexts(contextIds: string[], format: 'json' | 'markdown' | 'summary' = 'markdown'): Promise<string> {
    const formatted = await Promise.all(
      contextIds.map(id => this.formatContextForLLM(id, format))
    );

    return `# Recorded Contexts (${contextIds.length} items)

${formatted.join('\n\n---\n\n')}`;
  }

  // =========================
  // CONTEXT MANAGEMENT
  // =========================

  /**
   * Delete context
   */
  async deleteContext(id: string): Promise<void> {
    await sqliteService.deleteRecordedContext(id);
    console.log(`[ContextRecorder] Deleted context ${id}`);
  }

  /**
   * Clear old contexts
   */
  async clearOldContexts(maxAgeDays: number): Promise<void> {
    await sqliteService.clearOldContexts(maxAgeDays);
    console.log(`[ContextRecorder] Cleared contexts older than ${maxAgeDays} days`);
  }

  /**
   * Get storage statistics
   */
  async getStorageStats(): Promise<{ count: number; totalSize: number; formattedSize: string }> {
    const stats = await sqliteService.getContextStorageStats();

    return {
      ...stats,
      formattedSize: this.formatBytes(stats.totalSize)
    };
  }

  // =========================
  // EXPORT/IMPORT
  // =========================

  /**
   * Export context as JSON
   */
  async exportContext(id: string): Promise<string> {
    const context = await sqliteService.getRecordedContext(id);
    if (!context) {
      throw new Error(`Context ${id} not found`);
    }
    return JSON.stringify(context, null, 2);
  }

  /**
   * Export multiple contexts
   */
  async exportContexts(ids: string[]): Promise<string> {
    const contexts = await Promise.all(
      ids.map(id => sqliteService.getRecordedContext(id))
    );
    return JSON.stringify(contexts.filter(c => c !== null), null, 2);
  }

  /**
   * Import context from JSON
   */
  async importContext(json: string): Promise<string> {
    try {
      const context = JSON.parse(json) as RecordedContext;
      const newId = crypto.randomUUID();

      const newContext: Omit<RecordedContext, 'created_at'> = {
        ...context,
        id: newId
      };

      await sqliteService.saveRecordedContext(newContext);
      console.log(`[ContextRecorder] Imported context as ${newId}`);
      return newId;
    } catch (error) {
      console.error('[ContextRecorder] Failed to import context:', error);
      throw new Error('Invalid context JSON');
    }
  }

  // =========================
  // UTILITY METHODS
  // =========================

  /**
   * Generate summary from data
   */
  private generateSummary(data: any): string {
    if (Array.isArray(data)) {
      return `Array with ${data.length} items`;
    }

    if (typeof data === 'object' && data !== null) {
      const keys = Object.keys(data);
      if (keys.length <= 3) {
        return `Object with keys: ${keys.join(', ')}`;
      }
      return `Object with ${keys.length} properties`;
    }

    if (typeof data === 'string') {
      return data.length > 100 ? `${data.substring(0, 100)}...` : data;
    }

    return String(data);
  }

  /**
   * Format bytes to human readable
   */
  private formatBytes(bytes: number): string {
    if (bytes === 0) return '0 Bytes';

    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));

    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  }

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return sqliteService.isReady();
  }
}

// Export singleton instance
export const contextRecorderService = new ContextRecorderService();
export default contextRecorderService;
