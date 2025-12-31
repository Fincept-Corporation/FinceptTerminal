// Kite Connect Trading Module
// Handles order placement, modification, cancellation, and GTT orders

import { AxiosInstance } from 'axios';
import {
  OrderParams,
  OrderResponse,
  Order,
  Trade,
  GTTParams,
  GTTResponse,
  GTT,
  AlertParams,
  Alert,
  MarginOrder,
  MarginResponse,
  MFOrder,
  MFHolding,
  MFSip
} from './types';

export class KiteTrading {
  constructor(private client: AxiosInstance) {}

  /**
   * Place a new order
   */
  async placeOrder(params: OrderParams): Promise<OrderResponse> {
    const variety = params.variety || 'regular';
    const data = new URLSearchParams();

    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined && key !== 'variety') {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.post(`/orders/${variety}`, data);
    return response.data.data;
  }

  /**
   * Modify an existing order
   */
  async modifyOrder(
    orderId: string,
    params: Partial<OrderParams>,
    variety: string = 'regular'
  ): Promise<OrderResponse> {
    const data = new URLSearchParams();

    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined) {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.put(`/orders/${variety}/${orderId}`, data);
    return response.data.data;
  }

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string, variety: string = 'regular'): Promise<OrderResponse> {
    const response = await this.client.delete(`/orders/${variety}/${orderId}`);
    return response.data.data;
  }

  /**
   * Get all orders for the day
   */
  async getOrders(): Promise<Order[]> {
    const response = await this.client.get('/orders');
    return response.data.data;
  }

  /**
   * Get order history for a specific order
   */
  async getOrderHistory(orderId: string): Promise<Order[]> {
    const response = await this.client.get(`/orders/${orderId}`);
    return response.data.data;
  }

  /**
   * Get all trades for the day
   */
  async getTrades(): Promise<Trade[]> {
    const response = await this.client.get('/trades');
    return response.data.data;
  }

  /**
   * Get trades for a specific order
   */
  async getOrderTrades(orderId: string): Promise<Trade[]> {
    const response = await this.client.get(`/orders/${orderId}/trades`);
    return response.data.data;
  }

  // GTT (Good Till Triggered) Orders

  /**
   * Place GTT order
   */
  async placeGTT(params: GTTParams): Promise<GTTResponse> {
    const data = new URLSearchParams({
      type: params.type,
      condition: JSON.stringify(params.condition),
      orders: JSON.stringify(params.orders),
    });

    const response = await this.client.post('/gtt/triggers', data);
    return response.data.data;
  }

  /**
   * Get all GTT orders
   */
  async getGTTs(): Promise<GTT[]> {
    const response = await this.client.get('/gtt/triggers');
    return response.data.data;
  }

  /**
   * Get specific GTT order
   */
  async getGTT(triggerId: number): Promise<GTT> {
    const response = await this.client.get(`/gtt/triggers/${triggerId}`);
    return response.data.data;
  }

  /**
   * Modify GTT order
   */
  async modifyGTT(triggerId: number, params: GTTParams): Promise<GTTResponse> {
    const data = new URLSearchParams({
      type: params.type,
      condition: JSON.stringify(params.condition),
      orders: JSON.stringify(params.orders),
    });

    const response = await this.client.put(`/gtt/triggers/${triggerId}`, data);
    return response.data.data;
  }

  /**
   * Cancel GTT order
   */
  async cancelGTT(triggerId: number): Promise<GTTResponse> {
    const response = await this.client.delete(`/gtt/triggers/${triggerId}`);
    return response.data.data;
  }

  // Price Alerts

  /**
   * Create alert
   */
  async createAlert(params: AlertParams): Promise<Alert> {
    const data = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined) {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.post('/alerts', data);
    return response.data.data;
  }

  /**
   * Get all alerts
   */
  async getAlerts(): Promise<Alert[]> {
    const response = await this.client.get('/alerts');
    return response.data.data;
  }

  /**
   * Get specific alert
   */
  async getAlert(alertUuid: string): Promise<Alert> {
    const response = await this.client.get(`/alerts/${alertUuid}`);
    return response.data.data;
  }

  /**
   * Modify alert
   */
  async modifyAlert(alertUuid: string, params: Partial<AlertParams>): Promise<Alert> {
    const data = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined) {
        data.append(key, value.toString());
      }
    });

    const response = await this.client.put(`/alerts/${alertUuid}`, data);
    return response.data.data;
  }

  /**
   * Delete alerts
   */
  async deleteAlerts(alertUuids: string[]): Promise<void> {
    const params = new URLSearchParams();
    alertUuids.forEach(uuid => params.append('uuid', uuid));

    await this.client.delete(`/alerts?${params.toString()}`);
  }

  /**
   * Get alert history
   */
  async getAlertHistory(alertUuid: string): Promise<any[]> {
    const response = await this.client.get(`/alerts/${alertUuid}/history`);
    return response.data.data;
  }

  // Margin Calculations

  /**
   * Calculate order margins
   */
  async calculateOrderMargins(orders: MarginOrder[], mode?: 'compact'): Promise<MarginResponse[]> {
    const params = new URLSearchParams();
    if (mode) params.append('mode', mode);

    const response = await this.client.post(
      `/margins/orders?${params.toString()}`,
      JSON.stringify(orders),
      {
        headers: { 'Content-Type': 'application/json' }
      }
    );
    return response.data.data;
  }

  /**
   * Calculate basket margins
   */
  async calculateBasketMargins(orders: MarginOrder[]): Promise<any> {
    const response = await this.client.post(
      '/margins/basket',
      JSON.stringify(orders),
      {
        headers: { 'Content-Type': 'application/json' }
      }
    );
    return response.data.data;
  }

  /**
   * Calculate order charges
   */
  async calculateOrderCharges(orders: MarginOrder[]): Promise<any> {
    const response = await this.client.post(
      '/charges/orders',
      JSON.stringify(orders),
      {
        headers: { 'Content-Type': 'application/json' }
      }
    );
    return response.data.data;
  }

  // Mutual Funds

  /**
   * Get mutual fund orders
   */
  async getMFOrders(): Promise<MFOrder[]> {
    const response = await this.client.get('/mf/orders');
    return response.data.data;
  }

  /**
   * Get specific mutual fund order
   */
  async getMFOrder(orderId: string): Promise<MFOrder> {
    const response = await this.client.get(`/mf/orders/${orderId}`);
    return response.data.data;
  }

  /**
   * Get mutual fund holdings
   */
  async getMFHoldings(): Promise<MFHolding[]> {
    const response = await this.client.get('/mf/holdings');
    return response.data.data;
  }

  /**
   * Get all SIPs
   */
  async getMFSips(): Promise<MFSip[]> {
    const response = await this.client.get('/mf/sips');
    return response.data.data;
  }

  /**
   * Get mutual fund instruments
   */
  async getMFInstruments(): Promise<string> {
    const response = await this.client.get('/mf/instruments');
    return response.data;
  }
}
