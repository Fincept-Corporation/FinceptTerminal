// QuantLib Pricing MCP Tools - 29 endpoints (combined)
// Split into: pricingBS (8), pricingBlack76Bachelier (14), pricingNumerical (7)

import { InternalTool } from '../../types';
import { quantlibPricingBSTools } from './pricingBS';
import { quantlibPricingBlack76BachelierTools } from './pricingBlack76Bachelier';
import { quantlibPricingNumericalTools } from './pricingNumerical';

export const quantlibPricingTools: InternalTool[] = [
  ...quantlibPricingBSTools,
  ...quantlibPricingBlack76BachelierTools,
  ...quantlibPricingNumericalTools,
];
