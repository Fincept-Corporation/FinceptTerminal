// Portfolio Alerts Service
// Manages price alerts and notifications for portfolio holdings

import Database from '@tauri-apps/plugin-sql';

export interface PriceAlert {
  id: string;
  portfolio_id: string;
  symbol: string;
  type: 'PRICE_TARGET' | 'STOP_LOSS' | 'TAKE_PROFIT' | 'PRICE_CHANGE';
  condition: 'ABOVE' | 'BELOW' | 'PERCENT_UP' | 'PERCENT_DOWN';
  target_value: number;
  is_triggered: boolean;
  created_at: string;
  triggered_at?: string;
  notes?: string;
}

class AlertsService {
  private db: Database | null = null;
  private dbPath = 'sqlite:fincept_terminal.db';
  private isInitialized = false;

  async initialize(): Promise<void> {
    if (this.isInitialized && this.db) {
      return;
    }

    try {
      this.db = await Database.load(this.dbPath);
      await this.createTable();
      this.isInitialized = true;
      console.log('[AlertsService] Initialized successfully');
    } catch (error) {
      console.error('[AlertsService] Initialization failed:', error);
      throw error;
    }
  }

  private async createTable(): Promise<void> {
    if (!this.db) throw new Error('Database not initialized');

    await this.db.execute(`
      CREATE TABLE IF NOT EXISTS portfolio_alerts (
        id TEXT PRIMARY KEY,
        portfolio_id TEXT NOT NULL,
        symbol TEXT NOT NULL,
        type TEXT NOT NULL,
        condition TEXT NOT NULL,
        target_value REAL NOT NULL,
        is_triggered INTEGER DEFAULT 0,
        created_at TEXT DEFAULT (datetime('now')),
        triggered_at TEXT,
        notes TEXT,
        FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
      )
    `);

    await this.db.execute(`
      CREATE INDEX IF NOT EXISTS idx_alerts_portfolio ON portfolio_alerts(portfolio_id)
    `);

    await this.db.execute(`
      CREATE INDEX IF NOT EXISTS idx_alerts_symbol ON portfolio_alerts(symbol)
    `);
  }

  private ensureInitialized(): void {
    if (!this.db || !this.isInitialized) {
      throw new Error('AlertsService not initialized. Call initialize() first.');
    }
  }

  /**
   * Create a new price alert
   */
  async createAlert(
    portfolioId: string,
    symbol: string,
    type: PriceAlert['type'],
    condition: PriceAlert['condition'],
    targetValue: number,
    notes?: string
  ): Promise<PriceAlert> {
    this.ensureInitialized();

    const id = crypto.randomUUID();
    await this.db!.execute(
      `INSERT INTO portfolio_alerts (id, portfolio_id, symbol, type, condition, target_value, notes)
       VALUES ($1, $2, $3, $4, $5, $6, $7)`,
      [id, portfolioId, symbol, type, condition, targetValue, notes || null]
    );

    const result = await this.db!.select<PriceAlert[]>(
      'SELECT * FROM portfolio_alerts WHERE id = $1',
      [id]
    );

    return this.mapAlert(result[0]);
  }

  /**
   * Get all alerts for a portfolio
   */
  async getPortfolioAlerts(portfolioId: string, includeTriggered: boolean = false): Promise<PriceAlert[]> {
    this.ensureInitialized();

    const query = includeTriggered
      ? 'SELECT * FROM portfolio_alerts WHERE portfolio_id = $1 ORDER BY created_at DESC'
      : 'SELECT * FROM portfolio_alerts WHERE portfolio_id = $1 AND is_triggered = 0 ORDER BY created_at DESC';

    const results = await this.db!.select<any[]>(query, [portfolioId]);
    return results.map(r => this.mapAlert(r));
  }

  /**
   * Get alerts for a specific symbol
   */
  async getSymbolAlerts(portfolioId: string, symbol: string): Promise<PriceAlert[]> {
    this.ensureInitialized();

    const results = await this.db!.select<any[]>(
      'SELECT * FROM portfolio_alerts WHERE portfolio_id = $1 AND symbol = $2 AND is_triggered = 0 ORDER BY created_at DESC',
      [portfolioId, symbol]
    );

    return results.map(r => this.mapAlert(r));
  }

  /**
   * Delete an alert
   */
  async deleteAlert(alertId: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute('DELETE FROM portfolio_alerts WHERE id = $1', [alertId]);
    console.log(`[AlertsService] Deleted alert: ${alertId}`);
  }

  /**
   * Trigger an alert
   */
  async triggerAlert(alertId: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute(
      `UPDATE portfolio_alerts SET is_triggered = 1, triggered_at = datetime('now') WHERE id = $1`,
      [alertId]
    );

    console.log(`[AlertsService] Triggered alert: ${alertId}`);
  }

  /**
   * Check if price conditions are met and trigger alerts
   */
  async checkAlerts(symbol: string, currentPrice: number, portfolioId?: string): Promise<PriceAlert[]> {
    this.ensureInitialized();

    const query = portfolioId
      ? 'SELECT * FROM portfolio_alerts WHERE symbol = $1 AND portfolio_id = $2 AND is_triggered = 0'
      : 'SELECT * FROM portfolio_alerts WHERE symbol = $1 AND is_triggered = 0';

    const params = portfolioId ? [symbol, portfolioId] : [symbol];
    const alerts = await this.db!.select<any[]>(query, params);

    const triggeredAlerts: PriceAlert[] = [];

    for (const alert of alerts) {
      let shouldTrigger = false;

      switch (alert.condition) {
        case 'ABOVE':
          shouldTrigger = currentPrice >= alert.target_value;
          break;
        case 'BELOW':
          shouldTrigger = currentPrice <= alert.target_value;
          break;
        // PERCENT_UP and PERCENT_DOWN would need base price tracking
      }

      if (shouldTrigger) {
        await this.triggerAlert(alert.id);
        triggeredAlerts.push(this.mapAlert(alert));
      }
    }

    return triggeredAlerts;
  }

  /**
   * Get alert statistics
   */
  async getAlertStats(portfolioId: string): Promise<{
    total: number;
    active: number;
    triggered: number;
    byType: Record<string, number>;
  }> {
    this.ensureInitialized();

    const alerts = await this.db!.select<any[]>(
      'SELECT * FROM portfolio_alerts WHERE portfolio_id = $1',
      [portfolioId]
    );

    const stats = {
      total: alerts.length,
      active: alerts.filter(a => a.is_triggered === 0).length,
      triggered: alerts.filter(a => a.is_triggered === 1).length,
      byType: {} as Record<string, number>
    };

    alerts.forEach(alert => {
      stats.byType[alert.type] = (stats.byType[alert.type] || 0) + 1;
    });

    return stats;
  }

  /**
   * Map database row to PriceAlert
   */
  private mapAlert(row: any): PriceAlert {
    return {
      ...row,
      is_triggered: row.is_triggered === 1
    };
  }

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return this.isInitialized && this.db !== null;
  }
}

// Export singleton instance
export const alertsService = new AlertsService();
export default alertsService;
