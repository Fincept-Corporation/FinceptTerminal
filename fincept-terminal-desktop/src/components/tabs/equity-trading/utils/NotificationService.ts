/**
 * Notification Service
 * Toast notifications for trade updates and alerts
 */

import { UnifiedOrder, OrderStatus, BrokerType } from '../core/types';

export type NotificationType = 'info' | 'success' | 'warning' | 'error';

export interface Notification {
  id: string;
  type: NotificationType;
  title: string;
  message: string;
  timestamp: Date;
  duration?: number;
  brokerId?: BrokerType;
}

type NotificationListener = (notification: Notification) => void;

export class NotificationService {
  private static instance: NotificationService;
  private listeners: Set<NotificationListener> = new Set();
  private notificationHistory: Notification[] = [];
  private maxHistory = 100;

  private constructor() {}

  static getInstance(): NotificationService {
    if (!NotificationService.instance) {
      NotificationService.instance = new NotificationService();
    }
    return NotificationService.instance;
  }

  /**
   * Subscribe to notifications
   */
  subscribe(listener: NotificationListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /**
   * Show notification
   */
  notify(
    type: NotificationType,
    title: string,
    message: string,
    duration: number = 5000,
    brokerId?: BrokerType
  ): string {
    const notification: Notification = {
      id: Date.now().toString(),
      type,
      title,
      message,
      timestamp: new Date(),
      duration,
      brokerId,
    };

    // Add to history
    this.notificationHistory.push(notification);
    if (this.notificationHistory.length > this.maxHistory) {
      this.notificationHistory.shift();
    }

    // Notify listeners
    this.listeners.forEach((listener) => {
      try {
        listener(notification);
      } catch (error) {
        console.error('[NotificationService] Listener error:', error);
      }
    });

    return notification.id;
  }

  /**
   * Show info notification
   */
  info(title: string, message: string, brokerId?: BrokerType): string {
    return this.notify('info', title, message, 5000, brokerId);
  }

  /**
   * Show success notification
   */
  success(title: string, message: string, brokerId?: BrokerType): string {
    return this.notify('success', title, message, 5000, brokerId);
  }

  /**
   * Show warning notification
   */
  warning(title: string, message: string, brokerId?: BrokerType): string {
    return this.notify('warning', title, message, 7000, brokerId);
  }

  /**
   * Show error notification
   */
  error(title: string, message: string, brokerId?: BrokerType): string {
    return this.notify('error', title, message, 10000, brokerId);
  }

  /**
   * Notify order update
   */
  notifyOrderUpdate(order: UnifiedOrder): void {
    const broker = order.brokerId.toUpperCase();

    switch (order.status) {
      case OrderStatus.OPEN:
        this.info(
          'Order Placed',
          `${order.side} ${order.quantity} ${order.symbol} @ ₹${order.price || 'MARKET'} on ${broker}`,
          order.brokerId
        );
        break;

      case OrderStatus.COMPLETE:
        this.success(
          'Order Filled',
          `${order.side} ${order.filledQuantity} ${order.symbol} @ ₹${order.averagePrice.toFixed(2)} on ${broker}`,
          order.brokerId
        );
        break;

      case OrderStatus.CANCELLED:
        this.warning(
          'Order Cancelled',
          `${order.side} ${order.quantity} ${order.symbol} on ${broker}`,
          order.brokerId
        );
        break;

      case OrderStatus.REJECTED:
        this.error(
          'Order Rejected',
          `${order.side} ${order.quantity} ${order.symbol} on ${broker} - Check your margin and order details`,
          order.brokerId
        );
        break;
    }
  }

  /**
   * Notify connection status
   */
  notifyConnection(brokerId: BrokerType, connected: boolean): void {
    if (connected) {
      this.success(
        'Broker Connected',
        `Successfully connected to ${brokerId.toUpperCase()}`,
        brokerId
      );
    } else {
      this.error(
        'Broker Disconnected',
        `Lost connection to ${brokerId.toUpperCase()}. Attempting to reconnect...`,
        brokerId
      );
    }
  }

  /**
   * Notify authentication status
   */
  notifyAuth(brokerId: BrokerType, success: boolean, message?: string): void {
    if (success) {
      this.success(
        'Authentication Successful',
        `Authenticated with ${brokerId.toUpperCase()}`,
        brokerId
      );
    } else {
      this.error(
        'Authentication Failed',
        message || `Failed to authenticate with ${brokerId.toUpperCase()}`,
        brokerId
      );
    }
  }

  /**
   * Notify token refresh
   */
  notifyTokenRefresh(brokerId: BrokerType, success: boolean): void {
    if (success) {
      this.info(
        'Token Refreshed',
        `Access token refreshed for ${brokerId.toUpperCase()}`,
        brokerId
      );
    } else {
      this.error(
        'Token Refresh Failed',
        `Failed to refresh token for ${brokerId.toUpperCase()}. Please re-authenticate.`,
        brokerId
      );
    }
  }

  /**
   * Get notification history
   */
  getHistory(): Notification[] {
    return [...this.notificationHistory];
  }

  /**
   * Get history by broker
   */
  getHistoryByBroker(brokerId: BrokerType): Notification[] {
    return this.notificationHistory.filter((n) => n.brokerId === brokerId);
  }

  /**
   * Clear history
   */
  clearHistory(): void {
    this.notificationHistory = [];
  }
}

export const notificationService = NotificationService.getInstance();
