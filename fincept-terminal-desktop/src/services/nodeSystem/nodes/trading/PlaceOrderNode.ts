/**
 * Place Order Node
 * Executes buy/sell orders through connected brokers
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { TradingBridge, OrderSide, OrderType, ProductType, Validity } from '../../adapters/TradingBridge';
import { RiskManager } from '../../safety/RiskManager';
import { AuditLogger, LogType } from '../../safety/AuditLogger';
import { ConfirmationService } from '../../safety/ConfirmationService';

export class PlaceOrderNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Place Order',
    name: 'placeOrder',
    icon: 'file:order.svg',
    group: ['trading'],
    version: 1,
    subtitle: '={{$parameter["side"]}} {{$parameter["symbol"]}}',
    description: 'Executes buy/sell orders through connected brokers',
    defaults: {
      name: 'Place Order',
      color: '#ef4444',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL',
        description: 'Symbol to trade',
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data',
      },
      {
        displayName: 'Side',
        name: 'side',
        type: NodePropertyType.Options,
        default: 'BUY',
        options: [
          { name: 'Buy', value: 'BUY' },
          { name: 'Sell', value: 'SELL' },
        ],
        description: 'Order side (buy or sell)',
      },
      {
        displayName: 'Order Type',
        name: 'orderType',
        type: NodePropertyType.Options,
        default: 'MARKET',
        options: [
          { name: 'Market', value: 'MARKET', description: 'Execute at current market price' },
          { name: 'Limit', value: 'LIMIT', description: 'Execute at specified price or better' },
          { name: 'Stop Loss', value: 'SL', description: 'Trigger at stop price, execute at market' },
          { name: 'Stop Limit', value: 'SL-L', description: 'Trigger at stop price, execute at limit' },
        ],
        description: 'Type of order',
      },
      {
        displayName: 'Quantity Type',
        name: 'quantityType',
        type: NodePropertyType.Options,
        default: 'shares',
        options: [
          { name: 'Number of Shares', value: 'shares' },
          { name: 'Dollar Amount', value: 'dollars' },
          { name: 'Percentage of Balance', value: 'percent' },
          { name: 'From Input', value: 'input' },
        ],
        description: 'How to specify quantity',
      },
      {
        displayName: 'Quantity',
        name: 'quantity',
        type: NodePropertyType.Number,
        default: 1,
        description: 'Number of shares',
        displayOptions: {
          show: { quantityType: ['shares'] },
        },
      },
      {
        displayName: 'Dollar Amount',
        name: 'dollarAmount',
        type: NodePropertyType.Number,
        default: 1000,
        description: 'Amount in dollars to invest',
        displayOptions: {
          show: { quantityType: ['dollars'] },
        },
      },
      {
        displayName: 'Percentage',
        name: 'percentage',
        type: NodePropertyType.Number,
        default: 10,
        description: 'Percentage of available balance',
        displayOptions: {
          show: { quantityType: ['percent'] },
        },
      },
      {
        displayName: 'Input Quantity Field',
        name: 'inputQuantityField',
        type: NodePropertyType.String,
        default: 'quantity',
        description: 'Field name in input data containing quantity',
        displayOptions: {
          show: { quantityType: ['input'] },
        },
      },
      {
        displayName: 'Limit Price',
        name: 'limitPrice',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Price for limit orders',
        displayOptions: {
          show: { orderType: ['LIMIT', 'SL-L'] },
        },
      },
      {
        displayName: 'Stop Price',
        name: 'stopPrice',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Trigger price for stop orders',
        displayOptions: {
          show: { orderType: ['SL', 'SL-L'] },
        },
      },
      {
        displayName: 'Product Type',
        name: 'productType',
        type: NodePropertyType.Options,
        default: 'CNC',
        options: [
          { name: 'Delivery (CNC)', value: 'CNC', description: 'Cash and carry, delivery trade' },
          { name: 'Intraday (MIS)', value: 'MIS', description: 'Margin intraday, auto-squared off' },
          { name: 'Normal (NRML)', value: 'NRML', description: 'Normal F&O margin' },
        ],
        description: 'Product type for the order',
      },
      {
        displayName: 'Validity',
        name: 'validity',
        type: NodePropertyType.Options,
        default: 'DAY',
        options: [
          { name: 'Day', value: 'DAY', description: 'Valid for the trading day' },
          { name: 'IOC (Immediate or Cancel)', value: 'IOC', description: 'Execute immediately or cancel' },
          { name: 'GTC (Good Till Cancel)', value: 'GTC', description: 'Valid until cancelled' },
        ],
        description: 'Order validity',
      },
      {
        displayName: 'Broker',
        name: 'broker',
        type: NodePropertyType.Options,
        default: 'paper',
        options: [
          { name: 'Paper Trading', value: 'paper', description: 'Simulated trading (safe)' },
          { name: 'Zerodha Kite', value: 'zerodha' },
          { name: 'Fyers', value: 'fyers' },
          { name: 'Alpaca', value: 'alpaca' },
          { name: 'Binance', value: 'binance' },
          { name: 'Kraken', value: 'kraken' },
        ],
        description: 'Broker to execute through',
      },
      {
        displayName: 'Require Confirmation',
        name: 'requireConfirmation',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Require user confirmation before placing order',
      },
      {
        displayName: 'Validate Against Risk Limits',
        name: 'validateRisk',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Check order against risk management rules',
      },
      {
        displayName: 'Stop Loss %',
        name: 'stopLossPercent',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Automatic stop loss percentage (0 to disable)',
      },
      {
        displayName: 'Take Profit %',
        name: 'takeProfitPercent',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Automatic take profit percentage (0 to disable)',
      },
      {
        displayName: 'Tag',
        name: 'tag',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'workflow-order',
        description: 'Optional tag for order tracking',
      },
    ],
    hints: [
      {
        message: 'Paper Trading mode is the default - no real money at risk',
        type: 'info',
        location: 'outputPane',
        whenToDisplay: 'always',
      },
      {
        message: 'Risk validation is enabled - orders exceeding limits will be blocked',
        type: 'warning',
        location: 'outputPane',
        whenToDisplay: 'always',
      },
    ],
  };

  private tradingBridge = new TradingBridge();
  private riskManager = new RiskManager();
  private auditLogger = new AuditLogger();
  private confirmationService = new ConfirmationService();

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
    const side = this.getNodeParameter('side', 0) as OrderSide;
    const orderType = this.getNodeParameter('orderType', 0) as OrderType;
    const quantityType = this.getNodeParameter('quantityType', 0) as string;
    const productType = this.getNodeParameter('productType', 0) as ProductType;
    const validity = this.getNodeParameter('validity', 0) as Validity;
    const broker = this.getNodeParameter('broker', 0) as string;
    const requireConfirmation = this.getNodeParameter('requireConfirmation', 0) as boolean;
    const validateRisk = this.getNodeParameter('validateRisk', 0) as boolean;
    const tag = this.getNodeParameter('tag', 0) as string;

    // Get symbol
    let symbol: string;
    if (useInputSymbol) {
      const inputData = this.getInputData();
      symbol = inputData[0]?.json?.symbol as string || '';
    } else {
      symbol = this.getNodeParameter('symbol', 0) as string;
    }

    if (!symbol) {
      return [[{
        json: {
          success: false,
          error: 'No symbol provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    // Calculate quantity
    let quantity: number;
    switch (quantityType) {
      case 'dollars': {
        const dollarAmount = this.getNodeParameter('dollarAmount', 0) as number;
        // Would need current price to calculate shares
        quantity = Math.floor(dollarAmount / 100); // Placeholder
        break;
      }
      case 'percent': {
        const percentage = this.getNodeParameter('percentage', 0) as number;
        // Would need balance to calculate
        quantity = 10; // Placeholder
        break;
      }
      case 'input': {
        const inputData = this.getInputData();
        const field = this.getNodeParameter('inputQuantityField', 0) as string;
        quantity = Number(inputData[0]?.json?.[field]) || 1;
        break;
      }
      case 'shares':
      default:
        quantity = this.getNodeParameter('quantity', 0) as number;
    }

    // Get prices for limit/stop orders
    let limitPrice: number | undefined;
    let stopPrice: number | undefined;

    if (orderType === 'LIMIT' || orderType === 'SL-L') {
      limitPrice = this.getNodeParameter('limitPrice', 0) as number;
    }
    if (orderType === 'SL' || orderType === 'SL-L') {
      stopPrice = this.getNodeParameter('stopPrice', 0) as number;
    }

    // Estimate order value (would use real price in production)
    const estimatedPrice = limitPrice || 100; // Placeholder
    const estimatedValue = estimatedPrice * quantity;

    // Build order request
    const orderRequest = {
      symbol,
      side,
      orderType,
      quantity,
      price: limitPrice,
      triggerPrice: stopPrice,
      productType,
      validity,
      tag: tag || `workflow_${this.getExecutionId()}`,
    };

    // Validate against risk limits if enabled
    if (validateRisk) {
      const riskManager = new RiskManager();
      const validation = riskManager.validateOrder({
        symbol,
        side,
        quantity,
        price: estimatedPrice,
        orderType,
      });

      if (!validation.valid) {
        // Log the blocked order
        const auditLogger = new AuditLogger();
        await auditLogger.logOrder({
          orderId: 'BLOCKED',
          symbol,
          side,
          quantity,
          price: estimatedPrice,
          status: 'blocked',
          broker,
          reason: validation.errors.join(', '),
        });

        return [[{
          json: {
            success: false,
            blocked: true,
            reason: 'Risk validation failed',
            errors: validation.errors,
            warnings: validation.warnings,
            order: orderRequest,
            timestamp: new Date().toISOString(),
          },
        }]];
      }
    }

    // Request confirmation if enabled (and not paper trading)
    if (requireConfirmation && broker !== 'paper') {
      const confirmationService = new ConfirmationService();
      const confirmed = await confirmationService.requestTradeConfirmation({
        symbol,
        side,
        quantity,
        price: estimatedPrice,
        broker,
        estimatedValue,
      });

      if (!confirmed) {
        return [[{
          json: {
            success: false,
            cancelled: true,
            reason: 'User cancelled the order',
            order: orderRequest,
            timestamp: new Date().toISOString(),
          },
        }]];
      }
    }

    // Execute the order
    const tradingBridge = new TradingBridge();
    const isPaper = broker === 'paper';

    try {
      const result = await tradingBridge.placeOrder(
        {
          symbol,
          side,
          orderType,
          quantity,
          price: limitPrice,
          triggerPrice: stopPrice,
          productType,
          validity,
          tag: orderRequest.tag,
        },
        broker,
        isPaper
      );

      // Log the order
      const auditLogger = new AuditLogger();
      await auditLogger.logOrder({
        orderId: result.orderId,
        symbol,
        side,
        quantity,
        price: estimatedPrice,
        status: result.status,
        broker,
        paperTrading: isPaper,
      });

      // Handle stop loss / take profit if configured
      const stopLossPercent = this.getNodeParameter('stopLossPercent', 0) as number;
      const takeProfitPercent = this.getNodeParameter('takeProfitPercent', 0) as number;

      const childOrders: Array<Record<string, unknown>> = [];

      if (stopLossPercent > 0 && side === 'BUY') {
        const slPrice = estimatedPrice * (1 - stopLossPercent / 100);
        childOrders.push({
          type: 'stop_loss',
          triggerPrice: slPrice,
          quantity,
          side: 'SELL',
          status: 'pending',
        });
      }

      if (takeProfitPercent > 0 && side === 'BUY') {
        const tpPrice = estimatedPrice * (1 + takeProfitPercent / 100);
        childOrders.push({
          type: 'take_profit',
          price: tpPrice,
          quantity,
          side: 'SELL',
          status: 'pending',
        });
      }

      return [[{
        json: {
          success: true,
          orderId: result.orderId,
          symbol,
          side,
          quantity,
          orderType,
          price: limitPrice || result.price,
          status: result.status,
          broker,
          paperTrading: isPaper,
          estimatedValue,
          childOrders: childOrders.length > 0 ? childOrders : undefined,
          executionId: this.getExecutionId(),
          timestamp: new Date().toISOString(),
          message: `${side} ${quantity} ${symbol} @ ${orderType} via ${broker}`,
        },
      }]];
    } catch (error) {
      // Log the error
      const auditLogger = new AuditLogger();
      await auditLogger.logOrder({
        orderId: 'ERROR',
        symbol,
        side,
        quantity,
        price: estimatedPrice,
        status: 'error',
        broker,
        reason: error instanceof Error ? error.message : 'Unknown error',
      });

      return [[{
        json: {
          success: false,
          error: error instanceof Error ? error.message : 'Unknown error',
          order: orderRequest,
          broker,
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }
}
