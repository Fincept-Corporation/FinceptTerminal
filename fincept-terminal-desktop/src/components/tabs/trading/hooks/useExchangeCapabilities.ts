/**
 * Exchange Capabilities Hook
 * Detects and returns what features the current exchange supports
 */

import { useMemo } from 'react';
import { useBrokerContext } from '../../../../contexts/BrokerContext';
import type { ExchangeOrderCapabilities } from '../types';
import type { OrderType } from '../../../../brokers/crypto/types';

export function useExchangeCapabilities(): ExchangeOrderCapabilities {
  const { features, supports, activeBrokerMetadata } = useBrokerContext();

  return useMemo(() => {
    if (!features || !supports) {
      // Default minimal capabilities when no exchange is connected
      return {
        supportsMarket: true,
        supportsLimit: true,
        supportsStop: false,
        supportsStopLimit: false,
        supportsTrailingStop: false,
        supportsIceberg: false,
        supportsStopLoss: false,
        supportsTakeProfit: false,
        supportsPostOnly: false,
        supportsReduceOnly: false,
        supportsTimeInForce: false,
        supportsMargin: false,
        supportsLeverage: false,
        maxLeverage: undefined,
        supportsOrderEdit: false,
        supportsBatchOrders: false,
        supportsCancelAll: false,
        supportsDeadManSwitch: false,
        supportedOrderTypes: ['market', 'limit'],
        supportedTimeInForce: ['GTC'],
      };
    }

    // Detect supported order types
    const supportedOrderTypes: OrderType[] = ['market', 'limit'];

    // Check for advanced order types
    const supportsStop = activeBrokerMetadata?.tradingFeatures?.stopOrders ?? false;
    const supportsStopLimit = activeBrokerMetadata?.tradingFeatures?.stopLimitOrders ?? false;
    const supportsTrailingStop = activeBrokerMetadata?.tradingFeatures?.trailingStopOrders ?? false;

    if (supportsStop) {
      supportedOrderTypes.push('stop');
    }
    if (supportsStopLimit) {
      supportedOrderTypes.push('stop_limit');
    }
    if (supportsTrailingStop) {
      supportedOrderTypes.push('trailing_stop');
    }

    // Detect leverage support
    const supportsLeverage = activeBrokerMetadata?.advancedFeatures?.leverage ?? false;
    const maxLeverage = activeBrokerMetadata?.advancedFeatures?.maxLeverage;

    // Detect margin support
    const supportsMargin = activeBrokerMetadata?.features?.margin ?? false;

    // Build capabilities object
    const capabilities: ExchangeOrderCapabilities = {
      // Basic order types
      supportsMarket: true,
      supportsLimit: true,
      supportsStop,
      supportsStopLimit,
      supportsTrailingStop,
      supportsIceberg: activeBrokerMetadata?.tradingFeatures?.icebergOrders ?? false,

      // Advanced order features
      supportsStopLoss: supportsStop || supportsStopLimit,
      supportsTakeProfit: supportsStop || supportsStopLimit,
      supportsPostOnly: true, // Most exchanges support this
      supportsReduceOnly: supportsMargin,
      supportsTimeInForce: true, // Most exchanges support this

      // Margin & leverage
      supportsMargin,
      supportsLeverage,
      maxLeverage,

      // Order management
      supportsOrderEdit: supports?.canEditOrder?.() ?? false,
      supportsBatchOrders: supports?.canBatchOrder?.() ?? false,
      supportsCancelAll: supports?.canCancelAll?.() ?? false,
      supportsDeadManSwitch: activeBrokerMetadata?.tradingFeatures?.editOrders ?? false,

      // Available options
      supportedOrderTypes,
      supportedTimeInForce: ['GTC', 'IOC', 'FOK'],
    };

    return capabilities;
  }, [features, supports, activeBrokerMetadata]);
}

/**
 * Hook to check if a specific feature is supported
 */
export function useSupportsFeature(feature: keyof ExchangeOrderCapabilities): boolean {
  const capabilities = useExchangeCapabilities();
  return capabilities[feature] as boolean;
}

/**
 * Hook to get available order types
 */
export function useAvailableOrderTypes(): OrderType[] {
  const capabilities = useExchangeCapabilities();
  return capabilities.supportedOrderTypes;
}

/**
 * Hook to check if advanced orders are available
 */
export function useHasAdvancedOrders(): boolean {
  const capabilities = useExchangeCapabilities();
  return (
    capabilities.supportsStopLoss ||
    capabilities.supportsTakeProfit ||
    capabilities.supportsTrailingStop ||
    capabilities.supportsIceberg
  );
}

/**
 * Hook to check if margin trading is available
 */
export function useHasMarginTrading(): boolean {
  const capabilities = useExchangeCapabilities();
  return capabilities.supportsMargin || capabilities.supportsLeverage;
}
