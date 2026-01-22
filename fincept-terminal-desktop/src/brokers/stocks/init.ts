/**
 * Stock Broker Initialization
 *
 * Registers all broker adapters at application startup
 */

import { registerBrokerAdapter } from './registry';

// Indian Brokers
import { ZerodhaAdapter } from './india/zerodha/ZerodhaAdapter';
import { FyersAdapter } from './india/fyers/FyersAdapter';
import { AngelOneAdapter } from './india/angelone/AngelOneAdapter';
import { UpstoxAdapter } from './india/upstox/UpstoxAdapter';
import { DhanAdapter } from './india/dhan/DhanAdapter';
import { KotakAdapter } from './india/kotak/KotakAdapter';
import { GrowwAdapter } from './india/groww/GrowwAdapter';
import { AliceBlueAdapter } from './india/aliceblue/AliceBlueAdapter';
import { FivePaisaAdapter } from './india/fivepaisa/FivePaisaAdapter';
import { IIFLAdapter } from './india/iifl/IIFLAdapter';
import { MotilalAdapter } from './india/motilal/MotilalAdapter';
import { ShoonyaAdapter } from './india/shoonya/ShoonyaAdapter';

// US Brokers
import { AlpacaAdapter } from './us/alpaca/AlpacaAdapter';
import { IBKRAdapter } from './us/ibkr/IBKRAdapter';
import { TradierAdapter } from './us/tradier/TradierAdapter';

// European Brokers
// Paper Trading
import { YFinancePaperTradingAdapter } from './yfinance/YFinancePaperTradingAdapter';
import { SaxoBankAdapter } from './europe/saxobank/SaxoBankAdapter';

/**
 * Initialize all stock broker adapters
 * Must be called during application startup
 */
export function initializeStockBrokers(): void {
  // Register Indian brokers
  registerBrokerAdapter('zerodha', ZerodhaAdapter);
  registerBrokerAdapter('fyers', FyersAdapter);
  registerBrokerAdapter('angelone', AngelOneAdapter);
  registerBrokerAdapter('upstox', UpstoxAdapter);
  registerBrokerAdapter('dhan', DhanAdapter);
  registerBrokerAdapter('kotak', KotakAdapter);
  registerBrokerAdapter('groww', GrowwAdapter);
  registerBrokerAdapter('aliceblue', AliceBlueAdapter);
  registerBrokerAdapter('fivepaisa', FivePaisaAdapter);
  registerBrokerAdapter('iifl', IIFLAdapter);
  registerBrokerAdapter('motilal', MotilalAdapter);
  registerBrokerAdapter('shoonya', ShoonyaAdapter);

  // Register US brokers
  registerBrokerAdapter('alpaca', AlpacaAdapter);
  registerBrokerAdapter('ibkr', IBKRAdapter);
  registerBrokerAdapter('tradier', TradierAdapter);

  // Register European brokers
  registerBrokerAdapter('saxobank', SaxoBankAdapter);

  // Register Paper Trading
  registerBrokerAdapter('yfinance', YFinancePaperTradingAdapter);
}
