/**
 * IIFL Broker Module
 *
 * Export all IIFL-related components
 */

// Adapter
export { IIFLAdapter, iiflAdapter } from './IIFLAdapter';

// Constants
export {
  IIFL_BASE_URL,
  IIFL_INTERACTIVE_URL,
  IIFL_MARKET_DATA_URL,
  IIFL_ENDPOINTS,
  IIFL_EXCHANGE_MAP,
  IIFL_EXCHANGE_REVERSE_MAP,
  IIFL_EXCHANGE_SEGMENT_ID,
  IIFL_SEGMENT_ID_TO_EXCHANGE,
  IIFL_PRODUCT_MAP,
  IIFL_PRODUCT_REVERSE_MAP,
  IIFL_ORDER_SIDE_MAP,
  IIFL_ORDER_SIDE_REVERSE_MAP,
  IIFL_ORDER_TYPE_MAP,
  IIFL_ORDER_TYPE_REVERSE_MAP,
  IIFL_STATUS_MAP,
  IIFL_VALIDITY_MAP,
  IIFL_INTERVAL_MAP,
  IIFL_METADATA,
} from './constants';

// Mappers
export {
  toIIFLOrderParams,
  toIIFLModifyParams,
  toIIFLInterval,
  fromIIFLOrder,
  fromIIFLTrade,
  fromIIFLPosition,
  fromIIFLHolding,
  fromIIFLFunds,
  fromIIFLQuote,
  fromIIFLOHLCV,
  fromIIFLDepth,
  getExchangeSegmentId,
  getExchangeSegment,
} from './mapper';
