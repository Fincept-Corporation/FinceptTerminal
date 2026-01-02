/**
 * Confirmation Service
 * Handles user confirmation dialogs for trading actions
 * Provides promise-based API for workflow nodes
 */

import { IDataObject } from '../types';
import { OrderRequest } from '../adapters/TradingBridge';
import { RiskCheckResult } from './RiskManager';

// ================================
// TYPES
// ================================

export type ConfirmationType =
  | 'trade'
  | 'risk_override'
  | 'limit_breach'
  | 'live_trading'
  | 'workflow_start'
  | 'dangerous_action';

export interface ConfirmationRequest {
  id: string;
  type: ConfirmationType;
  title: string;
  message: string;
  details: ConfirmationDetails;
  riskLevel: 'low' | 'medium' | 'high' | 'critical';
  workflowContext?: WorkflowContext;
  createdAt: string;
  expiresAt?: string;
  status: 'pending' | 'approved' | 'rejected' | 'expired';
}

export interface ConfirmationDetails {
  // Order details
  symbol?: string;
  side?: 'BUY' | 'SELL';
  quantity?: number;
  price?: number;
  orderType?: string;
  estimatedValue?: number;
  broker?: string;

  // Risk details
  riskWarnings?: string[];
  riskChecks?: RiskCheckResult;

  // Additional info
  additionalInfo?: IDataObject;
}

export interface WorkflowContext {
  workflowId: string;
  workflowName: string;
  nodeId: string;
  nodeName: string;
}

export interface ConfirmationResponse {
  confirmed: boolean;
  confirmationId: string;
  respondedAt: string;
  notes?: string;
}

type ConfirmationCallback = (response: ConfirmationResponse) => void;

// ================================
// EVENT BUS
// ================================

type EventHandler = (data: any) => void;

class EventBus {
  private handlers: Map<string, EventHandler[]> = new Map();

  on(event: string, handler: EventHandler): void {
    if (!this.handlers.has(event)) {
      this.handlers.set(event, []);
    }
    this.handlers.get(event)!.push(handler);
  }

  off(event: string, handler: EventHandler): void {
    const handlers = this.handlers.get(event);
    if (handlers) {
      const index = handlers.indexOf(handler);
      if (index > -1) {
        handlers.splice(index, 1);
      }
    }
  }

  emit(event: string, data: any): void {
    const handlers = this.handlers.get(event);
    if (handlers) {
      handlers.forEach((handler) => handler(data));
    }
  }
}

export const confirmationEventBus = new EventBus();

// ================================
// CONFIRMATION SERVICE
// ================================

class ConfirmationServiceClass {
  private pendingConfirmations: Map<string, ConfirmationRequest> = new Map();
  private callbacks: Map<string, ConfirmationCallback> = new Map();
  private defaultTimeout: number = 60000; // 1 minute
  private autoApproveInPaperTrading: boolean = false;

  /**
   * Set auto-approve for paper trading mode
   */
  setAutoApproveInPaperTrading(enabled: boolean): void {
    this.autoApproveInPaperTrading = enabled;
    console.log(`[ConfirmationService] Auto-approve in paper trading: ${enabled}`);
  }

  /**
   * Request confirmation for a trade
   */
  async requestTradeConfirmation(
    order: OrderRequest,
    riskCheck: RiskCheckResult,
    workflowContext?: WorkflowContext,
    isPaperTrading: boolean = true
  ): Promise<boolean> {
    // Auto-approve in paper trading if enabled
    if (isPaperTrading && this.autoApproveInPaperTrading) {
      console.log('[ConfirmationService] Auto-approved (paper trading)');
      return true;
    }

    const estimatedValue = order.quantity * (order.price || 100);

    const request: ConfirmationRequest = {
      id: this.generateId(),
      type: 'trade',
      title: `Confirm ${order.side} Order`,
      message: `${order.side} ${order.quantity} ${order.symbol} @ ${order.price || 'MARKET'}`,
      details: {
        symbol: order.symbol,
        side: order.side,
        quantity: order.quantity,
        price: order.price,
        orderType: order.type,
        estimatedValue,
        riskWarnings: riskCheck.failed.map((f) => f.message),
        riskChecks: riskCheck,
      },
      riskLevel: riskCheck.overallRisk,
      workflowContext,
      createdAt: new Date().toISOString(),
      expiresAt: new Date(Date.now() + this.defaultTimeout).toISOString(),
      status: 'pending',
    };

    return this.waitForConfirmation(request);
  }

  /**
   * Request confirmation for enabling live trading
   */
  async requestLiveTradingConfirmation(
    broker: string,
    workflowContext?: WorkflowContext
  ): Promise<boolean> {
    const request: ConfirmationRequest = {
      id: this.generateId(),
      type: 'live_trading',
      title: 'Enable Live Trading',
      message: `You are about to enable LIVE trading on ${broker}. Real money will be at risk.`,
      details: {
        broker,
        riskWarnings: [
          'Live trading uses real money',
          'All trades will be executed on the exchange',
          'Losses are real and irreversible',
        ],
      },
      riskLevel: 'critical',
      workflowContext,
      createdAt: new Date().toISOString(),
      expiresAt: new Date(Date.now() + this.defaultTimeout).toISOString(),
      status: 'pending',
    };

    return this.waitForConfirmation(request);
  }

  /**
   * Request confirmation for risk override
   */
  async requestRiskOverrideConfirmation(
    riskCheck: RiskCheckResult,
    workflowContext?: WorkflowContext
  ): Promise<boolean> {
    const request: ConfirmationRequest = {
      id: this.generateId(),
      type: 'risk_override',
      title: 'Override Risk Check',
      message: 'This trade violates risk management rules. Do you want to override?',
      details: {
        riskWarnings: riskCheck.failed.map((f) => f.message),
        riskChecks: riskCheck,
      },
      riskLevel: 'critical',
      workflowContext,
      createdAt: new Date().toISOString(),
      expiresAt: new Date(Date.now() + this.defaultTimeout).toISOString(),
      status: 'pending',
    };

    return this.waitForConfirmation(request);
  }

  /**
   * Request general confirmation
   */
  async requestConfirmation(
    type: ConfirmationType,
    title: string,
    message: string,
    details: ConfirmationDetails = {},
    riskLevel: 'low' | 'medium' | 'high' | 'critical' = 'medium',
    workflowContext?: WorkflowContext
  ): Promise<boolean> {
    const request: ConfirmationRequest = {
      id: this.generateId(),
      type,
      title,
      message,
      details,
      riskLevel,
      workflowContext,
      createdAt: new Date().toISOString(),
      expiresAt: new Date(Date.now() + this.defaultTimeout).toISOString(),
      status: 'pending',
    };

    return this.waitForConfirmation(request);
  }

  /**
   * Approve a pending confirmation
   */
  approve(confirmationId: string, notes?: string): void {
    this.respond(confirmationId, true, notes);
  }

  /**
   * Reject a pending confirmation
   */
  reject(confirmationId: string, notes?: string): void {
    this.respond(confirmationId, false, notes);
  }

  /**
   * Get all pending confirmations
   */
  getPendingConfirmations(): ConfirmationRequest[] {
    this.cleanupExpired();
    return Array.from(this.pendingConfirmations.values())
      .filter((c) => c.status === 'pending');
  }

  /**
   * Get confirmation by ID
   */
  getConfirmation(id: string): ConfirmationRequest | undefined {
    return this.pendingConfirmations.get(id);
  }

  /**
   * Check if there are pending confirmations
   */
  hasPendingConfirmations(): boolean {
    return this.getPendingConfirmations().length > 0;
  }

  /**
   * Subscribe to confirmation requests
   */
  onConfirmationRequest(handler: (request: ConfirmationRequest) => void): () => void {
    confirmationEventBus.on('confirmation:requested', handler);
    return () => confirmationEventBus.off('confirmation:requested', handler);
  }

  /**
   * Subscribe to confirmation responses
   */
  onConfirmationResponse(handler: (response: ConfirmationResponse) => void): () => void {
    confirmationEventBus.on('confirmation:responded', handler);
    return () => confirmationEventBus.off('confirmation:responded', handler);
  }

  // ================================
  // PRIVATE METHODS
  // ================================

  private generateId(): string {
    return `confirm_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  private waitForConfirmation(request: ConfirmationRequest): Promise<boolean> {
    return new Promise((resolve) => {
      // Store the request
      this.pendingConfirmations.set(request.id, request);

      // Set up callback
      this.callbacks.set(request.id, (response) => {
        resolve(response.confirmed);
      });

      // Emit event for UI to show dialog
      confirmationEventBus.emit('confirmation:requested', request);

      // Set up timeout
      const timeout = setTimeout(() => {
        if (this.pendingConfirmations.get(request.id)?.status === 'pending') {
          this.expire(request.id);
          resolve(false);
        }
      }, this.defaultTimeout);

      // Clean up timeout when resolved
      const cleanup = () => clearTimeout(timeout);
      this.callbacks.set(`${request.id}_cleanup`, cleanup as any);
    });
  }

  private respond(confirmationId: string, confirmed: boolean, notes?: string): void {
    const request = this.pendingConfirmations.get(confirmationId);
    if (!request || request.status !== 'pending') {
      console.warn(`[ConfirmationService] No pending confirmation found: ${confirmationId}`);
      return;
    }

    // Update status
    request.status = confirmed ? 'approved' : 'rejected';

    // Create response
    const response: ConfirmationResponse = {
      confirmed,
      confirmationId,
      respondedAt: new Date().toISOString(),
      notes,
    };

    // Call the callback
    const callback = this.callbacks.get(confirmationId);
    if (callback) {
      callback(response);
      this.callbacks.delete(confirmationId);
    }

    // Clean up timeout
    const cleanup = this.callbacks.get(`${confirmationId}_cleanup`);
    if (cleanup) {
      (cleanup as unknown as () => void)();
      this.callbacks.delete(`${confirmationId}_cleanup`);
    }

    // Emit response event
    confirmationEventBus.emit('confirmation:responded', response);

    console.log(`[ConfirmationService] ${confirmed ? 'Approved' : 'Rejected'}: ${confirmationId}`);
  }

  private expire(confirmationId: string): void {
    const request = this.pendingConfirmations.get(confirmationId);
    if (request && request.status === 'pending') {
      request.status = 'expired';
      console.log(`[ConfirmationService] Expired: ${confirmationId}`);

      // Emit response event
      confirmationEventBus.emit('confirmation:responded', {
        confirmed: false,
        confirmationId,
        respondedAt: new Date().toISOString(),
        notes: 'Confirmation expired',
      });
    }
  }

  private cleanupExpired(): void {
    const now = new Date().toISOString();
    for (const [id, request] of this.pendingConfirmations) {
      if (request.status === 'pending' && request.expiresAt && request.expiresAt < now) {
        this.expire(id);
      }
    }
  }
}

// Export singleton
export const ConfirmationService = new ConfirmationServiceClass();
export { ConfirmationServiceClass };
