/**
 * Audit Logger
 * Logs all trading actions for compliance and debugging
 * Stores logs in SQLite database via Tauri
 */

import { IDataObject } from '../types';

// ================================
// TYPES
// ================================

export type AuditActionType =
  | 'ORDER_PLACED'
  | 'ORDER_MODIFIED'
  | 'ORDER_CANCELLED'
  | 'ORDER_FILLED'
  | 'POSITION_OPENED'
  | 'POSITION_CLOSED'
  | 'WORKFLOW_STARTED'
  | 'WORKFLOW_COMPLETED'
  | 'WORKFLOW_FAILED'
  | 'NODE_EXECUTED'
  | 'NODE_FAILED'
  | 'RISK_CHECK_PASSED'
  | 'RISK_CHECK_FAILED'
  | 'CONFIRMATION_APPROVED'
  | 'CONFIRMATION_REJECTED'
  | 'LIMIT_UPDATED'
  | 'SETTINGS_CHANGED';

export interface AuditLogEntry {
  id?: string;
  timestamp: string;
  actionType: AuditActionType;
  workflowId?: string;
  workflowName?: string;
  nodeId?: string;
  nodeName?: string;
  symbol?: string;
  side?: 'BUY' | 'SELL';
  quantity?: number;
  price?: number;
  orderId?: string;
  orderStatus?: string;
  paperTrading: boolean;
  userConfirmed?: boolean;
  riskCheckResult?: IDataObject;
  details?: IDataObject;
  errorMessage?: string;
}

export interface AuditQuery {
  startDate?: string;
  endDate?: string;
  actionTypes?: AuditActionType[];
  workflowId?: string;
  symbol?: string;
  paperTrading?: boolean;
  limit?: number;
  offset?: number;
}

export interface AuditStats {
  totalEntries: number;
  entriesByAction: Record<AuditActionType, number>;
  entriesByDay: Record<string, number>;
  totalTrades: number;
  successfulTrades: number;
  failedTrades: number;
  totalVolume: number;
}

// ================================
// AUDIT LOGGER
// ================================

class AuditLoggerClass {
  private logs: AuditLogEntry[] = [];
  private maxInMemoryLogs: number = 10000;
  private sqliteService: any = null;
  private initialized: boolean = false;

  /**
   * Initialize the audit logger
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    try {
      // Try to load SQLite service
      const module = await import('@/services/sqliteService');
      this.sqliteService = module.default || module.sqliteService;

      // Create audit table if not exists
      await this.createAuditTable();
      this.initialized = true;
      console.log('[AuditLogger] Initialized with SQLite storage');
    } catch (error) {
      console.warn('[AuditLogger] SQLite not available, using in-memory storage:', error);
      this.initialized = true;
    }
  }

  /**
   * Log an audit entry
   */
  async log(entry: Omit<AuditLogEntry, 'id' | 'timestamp'>): Promise<string> {
    const fullEntry: AuditLogEntry = {
      ...entry,
      id: this.generateId(),
      timestamp: new Date().toISOString(),
    };

    // Add to in-memory logs
    this.logs.push(fullEntry);
    if (this.logs.length > this.maxInMemoryLogs) {
      this.logs.shift();
    }

    // Persist to SQLite if available
    if (this.sqliteService) {
      try {
        await this.persistToDatabase(fullEntry);
      } catch (error) {
        console.error('[AuditLogger] Failed to persist log:', error);
      }
    }

    // Console log for debugging
    console.log(`[Audit] ${fullEntry.actionType}:`, {
      symbol: fullEntry.symbol,
      workflow: fullEntry.workflowName,
      paper: fullEntry.paperTrading,
    });

    return fullEntry.id!;
  }

  /**
   * Log an order action
   */
  async logOrder(
    action: 'ORDER_PLACED' | 'ORDER_MODIFIED' | 'ORDER_CANCELLED' | 'ORDER_FILLED',
    order: IDataObject,
    workflowContext?: { workflowId?: string; workflowName?: string; nodeId?: string; nodeName?: string },
    paperTrading: boolean = true
  ): Promise<string> {
    return this.log({
      actionType: action,
      workflowId: workflowContext?.workflowId,
      workflowName: workflowContext?.workflowName,
      nodeId: workflowContext?.nodeId,
      nodeName: workflowContext?.nodeName,
      symbol: order.symbol as string,
      side: order.side as 'BUY' | 'SELL',
      quantity: order.quantity as number,
      price: order.price as number,
      orderId: order.orderId as string,
      orderStatus: order.status as string,
      paperTrading,
      details: order,
    });
  }

  /**
   * Log a workflow action
   */
  async logWorkflow(
    action: 'WORKFLOW_STARTED' | 'WORKFLOW_COMPLETED' | 'WORKFLOW_FAILED',
    workflowId: string,
    workflowName: string,
    details?: IDataObject,
    errorMessage?: string
  ): Promise<string> {
    return this.log({
      actionType: action,
      workflowId,
      workflowName,
      paperTrading: true, // Workflow logs are always true
      details,
      errorMessage,
    });
  }

  /**
   * Log a node execution
   */
  async logNode(
    action: 'NODE_EXECUTED' | 'NODE_FAILED',
    nodeId: string,
    nodeName: string,
    workflowContext?: { workflowId?: string; workflowName?: string },
    details?: IDataObject,
    errorMessage?: string
  ): Promise<string> {
    return this.log({
      actionType: action,
      workflowId: workflowContext?.workflowId,
      workflowName: workflowContext?.workflowName,
      nodeId,
      nodeName,
      paperTrading: true,
      details,
      errorMessage,
    });
  }

  /**
   * Log a risk check
   */
  async logRiskCheck(
    passed: boolean,
    order: IDataObject,
    riskResult: IDataObject,
    workflowContext?: { workflowId?: string; workflowName?: string; nodeId?: string }
  ): Promise<string> {
    return this.log({
      actionType: passed ? 'RISK_CHECK_PASSED' : 'RISK_CHECK_FAILED',
      workflowId: workflowContext?.workflowId,
      workflowName: workflowContext?.workflowName,
      nodeId: workflowContext?.nodeId,
      symbol: order.symbol as string,
      side: order.side as 'BUY' | 'SELL',
      quantity: order.quantity as number,
      price: order.price as number,
      paperTrading: true,
      riskCheckResult: riskResult,
    });
  }

  /**
   * Log a confirmation action
   */
  async logConfirmation(
    approved: boolean,
    order: IDataObject,
    workflowContext?: { workflowId?: string; workflowName?: string; nodeId?: string }
  ): Promise<string> {
    return this.log({
      actionType: approved ? 'CONFIRMATION_APPROVED' : 'CONFIRMATION_REJECTED',
      workflowId: workflowContext?.workflowId,
      workflowName: workflowContext?.workflowName,
      nodeId: workflowContext?.nodeId,
      symbol: order.symbol as string,
      side: order.side as 'BUY' | 'SELL',
      quantity: order.quantity as number,
      price: order.price as number,
      paperTrading: true,
      userConfirmed: approved,
    });
  }

  /**
   * Query audit logs
   */
  async query(query: AuditQuery = {}): Promise<AuditLogEntry[]> {
    // If SQLite available, query from database
    if (this.sqliteService) {
      try {
        return await this.queryFromDatabase(query);
      } catch (error) {
        console.error('[AuditLogger] Database query failed, using in-memory:', error);
      }
    }

    // Fall back to in-memory query
    let results = [...this.logs];

    if (query.startDate) {
      results = results.filter((l) => l.timestamp >= query.startDate!);
    }
    if (query.endDate) {
      results = results.filter((l) => l.timestamp <= query.endDate!);
    }
    if (query.actionTypes && query.actionTypes.length > 0) {
      results = results.filter((l) => query.actionTypes!.includes(l.actionType));
    }
    if (query.workflowId) {
      results = results.filter((l) => l.workflowId === query.workflowId);
    }
    if (query.symbol) {
      results = results.filter((l) => l.symbol === query.symbol);
    }
    if (query.paperTrading !== undefined) {
      results = results.filter((l) => l.paperTrading === query.paperTrading);
    }

    // Sort by timestamp descending
    results.sort((a, b) => b.timestamp.localeCompare(a.timestamp));

    // Apply pagination
    const offset = query.offset || 0;
    const limit = query.limit || 100;
    return results.slice(offset, offset + limit);
  }

  /**
   * Get audit statistics
   */
  async getStats(startDate?: string, endDate?: string): Promise<AuditStats> {
    const entries = await this.query({ startDate, endDate, limit: 100000 });

    const stats: AuditStats = {
      totalEntries: entries.length,
      entriesByAction: {} as Record<AuditActionType, number>,
      entriesByDay: {},
      totalTrades: 0,
      successfulTrades: 0,
      failedTrades: 0,
      totalVolume: 0,
    };

    for (const entry of entries) {
      // Count by action type
      stats.entriesByAction[entry.actionType] =
        (stats.entriesByAction[entry.actionType] || 0) + 1;

      // Count by day
      const day = entry.timestamp.split('T')[0];
      stats.entriesByDay[day] = (stats.entriesByDay[day] || 0) + 1;

      // Trade statistics
      if (entry.actionType === 'ORDER_PLACED') {
        stats.totalTrades++;
        stats.totalVolume += (entry.quantity || 0) * (entry.price || 0);
      }
      if (entry.actionType === 'ORDER_FILLED') {
        stats.successfulTrades++;
      }
      if (entry.actionType === 'RISK_CHECK_FAILED') {
        stats.failedTrades++;
      }
    }

    return stats;
  }

  /**
   * Get recent logs
   */
  async getRecentLogs(limit: number = 50): Promise<AuditLogEntry[]> {
    return this.query({ limit });
  }

  /**
   * Export logs to JSON
   */
  async exportToJSON(query: AuditQuery = {}): Promise<string> {
    const logs = await this.query(query);
    return JSON.stringify(logs, null, 2);
  }

  /**
   * Clear in-memory logs
   */
  clearInMemoryLogs(): void {
    this.logs = [];
    console.log('[AuditLogger] In-memory logs cleared');
  }

  // ================================
  // PRIVATE METHODS
  // ================================

  private generateId(): string {
    return `audit_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  private async createAuditTable(): Promise<void> {
    if (!this.sqliteService) return;

    const sql = `
      CREATE TABLE IF NOT EXISTS workflow_audit_log (
        id TEXT PRIMARY KEY,
        timestamp TEXT NOT NULL,
        action_type TEXT NOT NULL,
        workflow_id TEXT,
        workflow_name TEXT,
        node_id TEXT,
        node_name TEXT,
        symbol TEXT,
        side TEXT,
        quantity REAL,
        price REAL,
        order_id TEXT,
        order_status TEXT,
        paper_trading INTEGER NOT NULL DEFAULT 1,
        user_confirmed INTEGER,
        risk_check_result TEXT,
        details TEXT,
        error_message TEXT
      );

      CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON workflow_audit_log(timestamp);
      CREATE INDEX IF NOT EXISTS idx_audit_action ON workflow_audit_log(action_type);
      CREATE INDEX IF NOT EXISTS idx_audit_workflow ON workflow_audit_log(workflow_id);
      CREATE INDEX IF NOT EXISTS idx_audit_symbol ON workflow_audit_log(symbol);
    `;

    try {
      await this.sqliteService.execute(sql);
    } catch (error) {
      console.warn('[AuditLogger] Could not create audit table:', error);
    }
  }

  private async persistToDatabase(entry: AuditLogEntry): Promise<void> {
    if (!this.sqliteService) return;

    const sql = `
      INSERT INTO workflow_audit_log (
        id, timestamp, action_type, workflow_id, workflow_name,
        node_id, node_name, symbol, side, quantity, price,
        order_id, order_status, paper_trading, user_confirmed,
        risk_check_result, details, error_message
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `;

    await this.sqliteService.execute(sql, [
      entry.id,
      entry.timestamp,
      entry.actionType,
      entry.workflowId || null,
      entry.workflowName || null,
      entry.nodeId || null,
      entry.nodeName || null,
      entry.symbol || null,
      entry.side || null,
      entry.quantity || null,
      entry.price || null,
      entry.orderId || null,
      entry.orderStatus || null,
      entry.paperTrading ? 1 : 0,
      entry.userConfirmed !== undefined ? (entry.userConfirmed ? 1 : 0) : null,
      entry.riskCheckResult ? JSON.stringify(entry.riskCheckResult) : null,
      entry.details ? JSON.stringify(entry.details) : null,
      entry.errorMessage || null,
    ]);
  }

  private async queryFromDatabase(query: AuditQuery): Promise<AuditLogEntry[]> {
    if (!this.sqliteService) return [];

    let sql = 'SELECT * FROM workflow_audit_log WHERE 1=1';
    const params: any[] = [];

    if (query.startDate) {
      sql += ' AND timestamp >= ?';
      params.push(query.startDate);
    }
    if (query.endDate) {
      sql += ' AND timestamp <= ?';
      params.push(query.endDate);
    }
    if (query.actionTypes && query.actionTypes.length > 0) {
      sql += ` AND action_type IN (${query.actionTypes.map(() => '?').join(',')})`;
      params.push(...query.actionTypes);
    }
    if (query.workflowId) {
      sql += ' AND workflow_id = ?';
      params.push(query.workflowId);
    }
    if (query.symbol) {
      sql += ' AND symbol = ?';
      params.push(query.symbol);
    }
    if (query.paperTrading !== undefined) {
      sql += ' AND paper_trading = ?';
      params.push(query.paperTrading ? 1 : 0);
    }

    sql += ' ORDER BY timestamp DESC';

    if (query.limit) {
      sql += ' LIMIT ?';
      params.push(query.limit);
    }
    if (query.offset) {
      sql += ' OFFSET ?';
      params.push(query.offset);
    }

    const rows = await this.sqliteService.query(sql, params);

    return rows.map((row: any) => ({
      id: row.id,
      timestamp: row.timestamp,
      actionType: row.action_type as AuditActionType,
      workflowId: row.workflow_id,
      workflowName: row.workflow_name,
      nodeId: row.node_id,
      nodeName: row.node_name,
      symbol: row.symbol,
      side: row.side,
      quantity: row.quantity,
      price: row.price,
      orderId: row.order_id,
      orderStatus: row.order_status,
      paperTrading: row.paper_trading === 1,
      userConfirmed: row.user_confirmed !== null ? row.user_confirmed === 1 : undefined,
      riskCheckResult: row.risk_check_result ? JSON.parse(row.risk_check_result) : undefined,
      details: row.details ? JSON.parse(row.details) : undefined,
      errorMessage: row.error_message,
    }));
  }
}

// Export singleton
export const AuditLogger = new AuditLoggerClass();
export { AuditLoggerClass };
