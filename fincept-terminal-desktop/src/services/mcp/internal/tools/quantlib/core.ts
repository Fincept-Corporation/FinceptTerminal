// QuantLib Core MCP Tools - 51 endpoints (combiner)
// Types, Conventions, AutoDiff, Distributions, Math, Operations, Legs, Periods

import { InternalTool } from '../../types';
import { quantlibCoreTypesConventionsTools } from './coreTypesConventions';
import { quantlibCoreOpsLegsTools } from './coreOpsLegs';

export const quantlibCoreTools: InternalTool[] = [
  ...quantlibCoreTypesConventionsTools,
  ...quantlibCoreOpsLegsTools,
];
