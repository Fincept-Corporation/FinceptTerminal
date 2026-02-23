// QuantLib Curves MCP Tools
// 31 tools for yield curves, interpolation, transformations, Nelson-Siegel, inflation, multicurve

import type { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

// ══════════════════════════════════════════════════════════════════════════════
// CURVE BUILDING & QUERYING (6 tools)
// ══════════════════════════════════════════════════════════════════════════════

const curveBuildTool: InternalTool = {
  name: 'quantlib_curves_build',
  description: 'Build a yield curve from reference date, tenors, and rates. Supports zero, discount, or forward curve types with various interpolation methods.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of tenors in years, e.g. [0.25, 0.5, 1, 2, 5, 10]' },
      values: { type: 'string', description: 'JSON array of rate values corresponding to tenors' },
      curve_type: { type: 'string', description: 'Curve type: zero, discount, or forward (default: zero)' },
      interpolation: { type: 'string', description: 'Interpolation method: linear, log_linear, cubic, etc. (default: linear)' },
    },
    required: ['reference_date', 'tenors', 'values'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const tenors = JSON.parse(args.tenors as string);
      const values = JSON.parse(args.values as string);
      const result = await quantlibApiCall('/quantlib/curves/build', {
        reference_date: args.reference_date,
        tenors,
        values,
        curve_type: args.curve_type || 'zero',
        interpolation: args.interpolation || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Built ${args.curve_type || 'zero'} curve with ${tenors.length} points` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const curveZeroRateTool: InternalTool = {
  name: 'quantlib_curves_zero_rate',
  description: 'Query the zero rate at a specific tenor from a yield curve.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      query_tenor: { type: 'number', description: 'Tenor to query in years' },
      curve_type: { type: 'string', description: 'Curve type (default: zero)' },
      interpolation: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['reference_date', 'tenors', 'values', 'query_tenor'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/zero-rate', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        query_tenor: args.query_tenor,
        curve_type: args.curve_type || 'zero',
        interpolation: args.interpolation || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Zero rate at ${args.query_tenor}Y: ${formatPercent(result.zero_rate)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const curveDiscountFactorTool: InternalTool = {
  name: 'quantlib_curves_discount_factor',
  description: 'Query the discount factor at a specific tenor from a yield curve.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      query_tenor: { type: 'number', description: 'Tenor to query in years' },
      curve_type: { type: 'string', description: 'Curve type (default: zero)' },
      interpolation: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['reference_date', 'tenors', 'values', 'query_tenor'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/discount-factor', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        query_tenor: args.query_tenor,
        curve_type: args.curve_type || 'zero',
        interpolation: args.interpolation || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Discount factor at ${args.query_tenor}Y: ${formatNumber(result.discount_factor)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const curveForwardRateTool: InternalTool = {
  name: 'quantlib_curves_forward_rate',
  description: 'Calculate the forward rate between two tenors from a yield curve.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      start_tenor: { type: 'number', description: 'Start tenor in years' },
      end_tenor: { type: 'number', description: 'End tenor in years' },
      curve_type: { type: 'string', description: 'Curve type (default: zero)' },
      interpolation: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['reference_date', 'tenors', 'values', 'start_tenor', 'end_tenor'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/forward-rate', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        start_tenor: args.start_tenor,
        end_tenor: args.end_tenor,
        curve_type: args.curve_type || 'zero',
        interpolation: args.interpolation || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Forward rate ${args.start_tenor}Y-${args.end_tenor}Y: ${formatPercent(result.forward_rate)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const curveInstantaneousForwardTool: InternalTool = {
  name: 'quantlib_curves_instantaneous_forward',
  description: 'Calculate the instantaneous forward rate at a specific tenor.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      query_tenor: { type: 'number', description: 'Tenor to query in years' },
      curve_type: { type: 'string', description: 'Curve type (default: zero)' },
      interpolation: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['reference_date', 'tenors', 'values', 'query_tenor'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/instantaneous-forward', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        query_tenor: args.query_tenor,
        curve_type: args.curve_type || 'zero',
        interpolation: args.interpolation || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Instantaneous forward at ${args.query_tenor}Y: ${formatPercent(result.instantaneous_forward)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const curvePointsTool: InternalTool = {
  name: 'quantlib_curves_points',
  description: 'Get curve values at multiple query tenors.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to query' },
      curve_type: { type: 'string', description: 'Curve type (default: zero)' },
      interpolation: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['reference_date', 'tenors', 'values', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const queryTenors = JSON.parse(args.query_tenors as string);
      const result = await quantlibApiCall('/quantlib/curves/curve-points', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        query_tenors: queryTenors,
        curve_type: args.curve_type || 'zero',
        interpolation: args.interpolation || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Retrieved ${queryTenors.length} curve points` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// INTERPOLATION (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const interpolateTool: InternalTool = {
  name: 'quantlib_curves_interpolate',
  description: 'Interpolate values at query points using various methods (linear, cubic, log_linear, etc.).',
  inputSchema: {
    type: 'object',
    properties: {
      x: { type: 'string', description: 'JSON array of x coordinates' },
      y: { type: 'string', description: 'JSON array of y coordinates' },
      query_x: { type: 'string', description: 'JSON array of x values to interpolate' },
      method: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['x', 'y', 'query_x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const queryX = JSON.parse(args.query_x as string);
      const result = await quantlibApiCall('/quantlib/curves/interpolate', {
        x: JSON.parse(args.x as string),
        y: JSON.parse(args.y as string),
        query_x: queryX,
        method: args.method || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Interpolated ${queryX.length} points using ${args.method || 'linear'} method` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const interpolateDerivativeTool: InternalTool = {
  name: 'quantlib_curves_interpolate_derivative',
  description: 'Calculate the derivative of an interpolated function at a query point.',
  inputSchema: {
    type: 'object',
    properties: {
      x: { type: 'string', description: 'JSON array of x coordinates' },
      y: { type: 'string', description: 'JSON array of y coordinates' },
      query_x: { type: 'number', description: 'X value to calculate derivative at' },
      method: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['x', 'y', 'query_x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/interpolate-derivative', {
        x: JSON.parse(args.x as string),
        y: JSON.parse(args.y as string),
        query_x: args.query_x,
        method: args.method || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Derivative at x=${args.query_x}: ${formatNumber(result.derivative)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const monotonicityCheckTool: InternalTool = {
  name: 'quantlib_curves_monotonicity_check',
  description: 'Check if an interpolation preserves monotonicity at query points.',
  inputSchema: {
    type: 'object',
    properties: {
      x: { type: 'string', description: 'JSON array of x coordinates' },
      y: { type: 'string', description: 'JSON array of y coordinates' },
      query_x: { type: 'string', description: 'JSON array of x values to check' },
      method: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['x', 'y', 'query_x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/monotonicity-check', {
        x: JSON.parse(args.x as string),
        y: JSON.parse(args.y as string),
        query_x: JSON.parse(args.query_x as string),
        method: args.method || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Monotonicity check: ${result.is_monotonic ? 'PASSED' : 'FAILED'}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CURVE TRANSFORMATIONS (6 tools)
// ══════════════════════════════════════════════════════════════════════════════

const parallelShiftTool: InternalTool = {
  name: 'quantlib_curves_parallel_shift',
  description: 'Apply a parallel shift (bump) to a yield curve in basis points.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      shift_bps: { type: 'number', description: 'Shift amount in basis points' },
      space: { type: 'string', description: 'Transformation space: zero_rate or discount (default: zero_rate)' },
    },
    required: ['reference_date', 'tenors', 'values', 'shift_bps'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/parallel-shift', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        shift_bps: args.shift_bps,
        space: args.space || 'zero_rate',
      }, apiKey);
      return { success: true, data: result, message: `Applied ${args.shift_bps}bps parallel shift` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const twistTool: InternalTool = {
  name: 'quantlib_curves_twist',
  description: 'Apply a twist (steepening/flattening) transformation to a yield curve.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      pivot_years: { type: 'number', description: 'Pivot point in years' },
      short_shift_bps: { type: 'number', description: 'Shift for short end in bps' },
      long_shift_bps: { type: 'number', description: 'Shift for long end in bps' },
    },
    required: ['reference_date', 'tenors', 'values', 'pivot_years', 'short_shift_bps', 'long_shift_bps'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/twist', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        pivot_years: args.pivot_years,
        short_shift_bps: args.short_shift_bps,
        long_shift_bps: args.long_shift_bps,
      }, apiKey);
      return { success: true, data: result, message: `Applied twist: short ${args.short_shift_bps}bps, long ${args.long_shift_bps}bps, pivot ${args.pivot_years}Y` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const butterflyTool: InternalTool = {
  name: 'quantlib_curves_butterfly',
  description: 'Apply a butterfly transformation to a yield curve (wings vs belly).',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      belly_years: { type: 'number', description: 'Belly point in years' },
      wing_shift_bps: { type: 'number', description: 'Shift for wings in bps' },
      belly_shift_bps: { type: 'number', description: 'Shift for belly in bps' },
    },
    required: ['reference_date', 'tenors', 'values', 'belly_years', 'wing_shift_bps', 'belly_shift_bps'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/butterfly', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        belly_years: args.belly_years,
        wing_shift_bps: args.wing_shift_bps,
        belly_shift_bps: args.belly_shift_bps,
      }, apiKey);
      return { success: true, data: result, message: `Applied butterfly: wings ${args.wing_shift_bps}bps, belly ${args.belly_shift_bps}bps` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const keyRateShiftTool: InternalTool = {
  name: 'quantlib_curves_key_rate_shift',
  description: 'Apply a key rate shift (localized bump) at a specific tenor.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      tenor_years: { type: 'number', description: 'Target tenor for key rate shift' },
      shift_bps: { type: 'number', description: 'Shift amount in bps' },
      width_years: { type: 'number', description: 'Width of the shift region in years (default: 2)' },
    },
    required: ['reference_date', 'tenors', 'values', 'tenor_years', 'shift_bps'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/key-rate-shift', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        tenor_years: args.tenor_years,
        shift_bps: args.shift_bps,
        width_years: args.width_years || 2,
      }, apiKey);
      return { success: true, data: result, message: `Applied ${args.shift_bps}bps key rate shift at ${args.tenor_years}Y` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const rollTool: InternalTool = {
  name: 'quantlib_curves_roll',
  description: 'Roll a curve forward in time by a specified number of days.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      days: { type: 'number', description: 'Number of days to roll forward' },
      preserve_shape: { type: 'boolean', description: 'Whether to preserve curve shape (default: true)' },
    },
    required: ['reference_date', 'tenors', 'values', 'days'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/roll', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        days: args.days,
        preserve_shape: args.preserve_shape !== false,
      }, apiKey);
      return { success: true, data: result, message: `Rolled curve forward by ${args.days} days` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const scaleTool: InternalTool = {
  name: 'quantlib_curves_scale',
  description: 'Scale all curve values by a factor.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      scale_factor: { type: 'number', description: 'Scaling factor to apply' },
    },
    required: ['reference_date', 'tenors', 'values', 'scale_factor'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/scale', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        scale_factor: args.scale_factor,
      }, apiKey);
      return { success: true, data: result, message: `Scaled curve by factor ${args.scale_factor}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CURVE CONSTRUCTION (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const compositeTool: InternalTool = {
  name: 'quantlib_curves_composite',
  description: 'Build a composite curve from base and forecast curves.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      base_tenors: { type: 'string', description: 'JSON array of base curve tenors' },
      base_values: { type: 'string', description: 'JSON array of base curve values' },
      forecast_tenors: { type: 'string', description: 'JSON array of forecast curve tenors' },
      forecast_values: { type: 'string', description: 'JSON array of forecast curve values' },
    },
    required: ['reference_date', 'base_tenors', 'base_values', 'forecast_tenors', 'forecast_values'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/composite', {
        reference_date: args.reference_date,
        base_tenors: JSON.parse(args.base_tenors as string),
        base_values: JSON.parse(args.base_values as string),
        forecast_tenors: JSON.parse(args.forecast_tenors as string),
        forecast_values: JSON.parse(args.forecast_values as string),
      }, apiKey);
      return { success: true, data: result, message: 'Built composite curve from base and forecast curves' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const proxyTool: InternalTool = {
  name: 'quantlib_curves_proxy',
  description: 'Create a proxy curve by adding a spread to a base curve.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      base_tenors: { type: 'string', description: 'JSON array of base curve tenors' },
      base_values: { type: 'string', description: 'JSON array of base curve values' },
      spread_bps: { type: 'number', description: 'Spread to add in basis points' },
    },
    required: ['reference_date', 'base_tenors', 'base_values', 'spread_bps'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/proxy', {
        reference_date: args.reference_date,
        base_tenors: JSON.parse(args.base_tenors as string),
        base_values: JSON.parse(args.base_values as string),
        spread_bps: args.spread_bps,
      }, apiKey);
      return { success: true, data: result, message: `Created proxy curve with ${args.spread_bps}bps spread` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const timeShiftTool: InternalTool = {
  name: 'quantlib_curves_time_shift',
  description: 'Shift a curve in time and query at new tenors.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      tenors: { type: 'string', description: 'JSON array of curve tenors in years' },
      values: { type: 'string', description: 'JSON array of curve values' },
      shift_days: { type: 'number', description: 'Time shift in days' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to query after shift' },
      preserve_shape: { type: 'boolean', description: 'Whether to preserve curve shape (default: true)' },
    },
    required: ['reference_date', 'tenors', 'values', 'shift_days', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/time-shift', {
        reference_date: args.reference_date,
        tenors: JSON.parse(args.tenors as string),
        values: JSON.parse(args.values as string),
        shift_days: args.shift_days,
        query_tenors: JSON.parse(args.query_tenors as string),
        preserve_shape: args.preserve_shape !== false,
      }, apiKey);
      return { success: true, data: result, message: `Time-shifted curve by ${args.shift_days} days` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// SMOOTHNESS (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const constrainedFitTool: InternalTool = {
  name: 'quantlib_curves_constrained_fit',
  description: 'Fit a smooth curve with constraints (non-negative forwards, etc.).',
  inputSchema: {
    type: 'object',
    properties: {
      tenors: { type: 'string', description: 'JSON array of tenors' },
      rates: { type: 'string', description: 'JSON array of rates' },
    },
    required: ['tenors', 'rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/constrained-fit', {
        tenors: JSON.parse(args.tenors as string),
        rates: JSON.parse(args.rates as string),
      }, apiKey);
      return { success: true, data: result, message: 'Fitted constrained smooth curve' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const smoothnessPenaltyTool: InternalTool = {
  name: 'quantlib_curves_smoothness_penalty',
  description: 'Calculate smoothness penalty for an interpolated curve.',
  inputSchema: {
    type: 'object',
    properties: {
      x: { type: 'string', description: 'JSON array of x coordinates' },
      y: { type: 'string', description: 'JSON array of y coordinates' },
      query_x: { type: 'string', description: 'JSON array of x values to evaluate' },
      method: { type: 'string', description: 'Interpolation method (default: linear)' },
    },
    required: ['x', 'y', 'query_x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/smoothness-penalty', {
        x: JSON.parse(args.x as string),
        y: JSON.parse(args.y as string),
        query_x: JSON.parse(args.query_x as string),
        method: args.method || 'linear',
      }, apiKey);
      return { success: true, data: result, message: `Smoothness penalty: ${formatNumber(result.penalty)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// NELSON-SIEGEL / NSS (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const nelsonSiegelFitTool: InternalTool = {
  name: 'quantlib_curves_nelson_siegel_fit',
  description: 'Fit Nelson-Siegel model parameters to market data.',
  inputSchema: {
    type: 'object',
    properties: {
      tenors: { type: 'string', description: 'JSON array of tenors in years' },
      rates: { type: 'string', description: 'JSON array of market rates' },
    },
    required: ['tenors', 'rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/nelson-siegel/fit', {
        tenors: JSON.parse(args.tenors as string),
        rates: JSON.parse(args.rates as string),
      }, apiKey);
      return { success: true, data: result, message: `Fitted NS: β0=${formatNumber(result.beta0)}, β1=${formatNumber(result.beta1)}, β2=${formatNumber(result.beta2)}, τ=${formatNumber(result.tau)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const nelsonSiegelEvaluateTool: InternalTool = {
  name: 'quantlib_curves_nelson_siegel_evaluate',
  description: 'Evaluate Nelson-Siegel curve at given tenors using specified parameters.',
  inputSchema: {
    type: 'object',
    properties: {
      beta0: { type: 'number', description: 'Level parameter' },
      beta1: { type: 'number', description: 'Slope parameter' },
      beta2: { type: 'number', description: 'Curvature parameter' },
      tau: { type: 'number', description: 'Decay parameter' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to evaluate' },
    },
    required: ['beta0', 'beta1', 'beta2', 'tau', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const queryTenors = JSON.parse(args.query_tenors as string);
      const result = await quantlibApiCall('/quantlib/curves/nelson-siegel/evaluate', {
        beta0: args.beta0,
        beta1: args.beta1,
        beta2: args.beta2,
        tau: args.tau,
        query_tenors: queryTenors,
      }, apiKey);
      return { success: true, data: result, message: `Evaluated NS curve at ${queryTenors.length} points` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const nssFitTool: InternalTool = {
  name: 'quantlib_curves_nss_fit',
  description: 'Fit Nelson-Siegel-Svensson (NSS) model parameters to market data.',
  inputSchema: {
    type: 'object',
    properties: {
      tenors: { type: 'string', description: 'JSON array of tenors in years' },
      rates: { type: 'string', description: 'JSON array of market rates' },
    },
    required: ['tenors', 'rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/nss/fit', {
        tenors: JSON.parse(args.tenors as string),
        rates: JSON.parse(args.rates as string),
      }, apiKey);
      return { success: true, data: result, message: 'Fitted NSS model parameters' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const nssEvaluateTool: InternalTool = {
  name: 'quantlib_curves_nss_evaluate',
  description: 'Evaluate Nelson-Siegel-Svensson curve at given tenors.',
  inputSchema: {
    type: 'object',
    properties: {
      beta0: { type: 'number', description: 'Level parameter' },
      beta1: { type: 'number', description: 'Slope parameter' },
      beta2: { type: 'number', description: 'First curvature parameter' },
      beta3: { type: 'number', description: 'Second curvature parameter' },
      tau1: { type: 'number', description: 'First decay parameter' },
      tau2: { type: 'number', description: 'Second decay parameter' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to evaluate' },
    },
    required: ['beta0', 'beta1', 'beta2', 'beta3', 'tau1', 'tau2', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const queryTenors = JSON.parse(args.query_tenors as string);
      const result = await quantlibApiCall('/quantlib/curves/nss/evaluate', {
        beta0: args.beta0,
        beta1: args.beta1,
        beta2: args.beta2,
        beta3: args.beta3,
        tau1: args.tau1,
        tau2: args.tau2,
        query_tenors: queryTenors,
      }, apiKey);
      return { success: true, data: result, message: `Evaluated NSS curve at ${queryTenors.length} points` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// INFLATION (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const inflationBuildTool: InternalTool = {
  name: 'quantlib_curves_inflation_build',
  description: 'Build an inflation curve from base CPI and inflation rates.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      base_cpi: { type: 'number', description: 'Base CPI index value' },
      tenors: { type: 'string', description: 'JSON array of tenors in years' },
      inflation_rates: { type: 'string', description: 'JSON array of inflation rates' },
    },
    required: ['reference_date', 'base_cpi', 'tenors', 'inflation_rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/inflation/build', {
        reference_date: args.reference_date,
        base_cpi: args.base_cpi,
        tenors: JSON.parse(args.tenors as string),
        inflation_rates: JSON.parse(args.inflation_rates as string),
      }, apiKey);
      return { success: true, data: result, message: `Built inflation curve with base CPI ${args.base_cpi}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const inflationBootstrapTool: InternalTool = {
  name: 'quantlib_curves_inflation_bootstrap',
  description: 'Bootstrap an inflation curve from inflation swap rates and nominal curve.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      base_cpi: { type: 'number', description: 'Base CPI index value' },
      swap_tenors: { type: 'string', description: 'JSON array of inflation swap tenors' },
      swap_rates: { type: 'string', description: 'JSON array of inflation swap rates' },
      nominal_tenors: { type: 'string', description: 'JSON array of nominal curve tenors' },
      nominal_rates: { type: 'string', description: 'JSON array of nominal curve rates' },
    },
    required: ['reference_date', 'base_cpi', 'swap_tenors', 'swap_rates', 'nominal_tenors', 'nominal_rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/inflation/bootstrap', {
        reference_date: args.reference_date,
        base_cpi: args.base_cpi,
        swap_tenors: JSON.parse(args.swap_tenors as string),
        swap_rates: JSON.parse(args.swap_rates as string),
        nominal_tenors: JSON.parse(args.nominal_tenors as string),
        nominal_rates: JSON.parse(args.nominal_rates as string),
      }, apiKey);
      return { success: true, data: result, message: 'Bootstrapped inflation curve from swap and nominal data' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const inflationSeasonalityTool: InternalTool = {
  name: 'quantlib_curves_inflation_seasonality',
  description: 'Apply seasonality adjustments to inflation data.',
  inputSchema: {
    type: 'object',
    properties: {
      monthly_factors: { type: 'string', description: 'JSON array of 12 monthly seasonality factors' },
      cpi_values: { type: 'string', description: 'JSON array of CPI values' },
      months: { type: 'string', description: 'JSON array of month indices (1-12)' },
    },
    required: ['monthly_factors', 'cpi_values', 'months'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/inflation/seasonality', {
        monthly_factors: JSON.parse(args.monthly_factors as string),
        cpi_values: JSON.parse(args.cpi_values as string),
        months: JSON.parse(args.months as string),
      }, apiKey);
      return { success: true, data: result, message: 'Applied seasonality adjustments to inflation data' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// MULTICURVE (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const multicurveSetupTool: InternalTool = {
  name: 'quantlib_curves_multicurve_setup',
  description: 'Set up a dual-curve framework with OIS discounting and IBOR forecasting.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      ois_tenors: { type: 'string', description: 'JSON array of OIS curve tenors' },
      ois_rates: { type: 'string', description: 'JSON array of OIS rates' },
      ibor_tenors: { type: 'string', description: 'JSON array of IBOR curve tenors' },
      ibor_rates: { type: 'string', description: 'JSON array of IBOR rates' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to query' },
    },
    required: ['reference_date', 'ois_tenors', 'ois_rates', 'ibor_tenors', 'ibor_rates', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/multicurve/setup', {
        reference_date: args.reference_date,
        ois_tenors: JSON.parse(args.ois_tenors as string),
        ois_rates: JSON.parse(args.ois_rates as string),
        ibor_tenors: JSON.parse(args.ibor_tenors as string),
        ibor_rates: JSON.parse(args.ibor_rates as string),
        query_tenors: JSON.parse(args.query_tenors as string),
      }, apiKey);
      return { success: true, data: result, message: 'Set up multicurve framework with OIS/IBOR curves' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const basisSpreadTool: InternalTool = {
  name: 'quantlib_curves_multicurve_basis_spread',
  description: 'Calculate the basis spread between OIS and IBOR curves at a specific tenor.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      ois_tenors: { type: 'string', description: 'JSON array of OIS curve tenors' },
      ois_rates: { type: 'string', description: 'JSON array of OIS rates' },
      ibor_tenors: { type: 'string', description: 'JSON array of IBOR curve tenors' },
      ibor_rates: { type: 'string', description: 'JSON array of IBOR rates' },
      query_tenor: { type: 'number', description: 'Tenor to query in years' },
    },
    required: ['reference_date', 'ois_tenors', 'ois_rates', 'ibor_tenors', 'ibor_rates', 'query_tenor'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/multicurve/basis-spread', {
        reference_date: args.reference_date,
        ois_tenors: JSON.parse(args.ois_tenors as string),
        ois_rates: JSON.parse(args.ois_rates as string),
        ibor_tenors: JSON.parse(args.ibor_tenors as string),
        ibor_rates: JSON.parse(args.ibor_rates as string),
        query_tenor: args.query_tenor,
      }, apiKey);
      return { success: true, data: result, message: `Basis spread at ${args.query_tenor}Y: ${formatNumber(result.basis_spread * 10000)}bps` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CROSS-CURRENCY / REAL RATE (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const crossCurrencyBasisTool: InternalTool = {
  name: 'quantlib_curves_cross_currency_basis',
  description: 'Build a cross-currency basis curve for FX-adjusted discounting.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      domestic_currency: { type: 'string', description: 'Domestic currency code (e.g., USD)' },
      foreign_currency: { type: 'string', description: 'Foreign currency code (e.g., EUR)' },
      tenors: { type: 'string', description: 'JSON array of tenors in years' },
      basis_spreads_bps: { type: 'string', description: 'JSON array of basis spreads in bps' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to query' },
    },
    required: ['reference_date', 'domestic_currency', 'foreign_currency', 'tenors', 'basis_spreads_bps', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/curves/cross-currency-basis', {
        reference_date: args.reference_date,
        domestic_currency: args.domestic_currency,
        foreign_currency: args.foreign_currency,
        tenors: JSON.parse(args.tenors as string),
        basis_spreads_bps: JSON.parse(args.basis_spreads_bps as string),
        query_tenors: JSON.parse(args.query_tenors as string),
      }, apiKey);
      return { success: true, data: result, message: `Built ${args.domestic_currency}/${args.foreign_currency} cross-currency basis curve` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const realRateTool: InternalTool = {
  name: 'quantlib_curves_real_rate',
  description: 'Calculate real interest rates from nominal rates and inflation expectations.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date in YYYY-MM-DD format' },
      nominal_tenors: { type: 'string', description: 'JSON array of nominal curve tenors' },
      nominal_rates: { type: 'string', description: 'JSON array of nominal rates' },
      base_cpi: { type: 'number', description: 'Base CPI index value' },
      inflation_tenors: { type: 'string', description: 'JSON array of inflation curve tenors' },
      inflation_rates: { type: 'string', description: 'JSON array of inflation rates' },
      query_tenors: { type: 'string', description: 'JSON array of tenors to query' },
    },
    required: ['reference_date', 'nominal_tenors', 'nominal_rates', 'base_cpi', 'inflation_tenors', 'inflation_rates', 'query_tenors'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const queryTenors = JSON.parse(args.query_tenors as string);
      const result = await quantlibApiCall('/quantlib/curves/real-rate', {
        reference_date: args.reference_date,
        nominal_tenors: JSON.parse(args.nominal_tenors as string),
        nominal_rates: JSON.parse(args.nominal_rates as string),
        base_cpi: args.base_cpi,
        inflation_tenors: JSON.parse(args.inflation_tenors as string),
        inflation_rates: JSON.parse(args.inflation_rates as string),
        query_tenors: queryTenors,
      }, apiKey);
      return { success: true, data: result, message: `Calculated real rates at ${queryTenors.length} tenors` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EXPORT ALL TOOLS
// ══════════════════════════════════════════════════════════════════════════════

export const quantlibCurvesTools: InternalTool[] = [
  // Curve Building & Querying (6)
  curveBuildTool,
  curveZeroRateTool,
  curveDiscountFactorTool,
  curveForwardRateTool,
  curveInstantaneousForwardTool,
  curvePointsTool,
  // Interpolation (3)
  interpolateTool,
  interpolateDerivativeTool,
  monotonicityCheckTool,
  // Curve Transformations (6)
  parallelShiftTool,
  twistTool,
  butterflyTool,
  keyRateShiftTool,
  rollTool,
  scaleTool,
  // Curve Construction (3)
  compositeTool,
  proxyTool,
  timeShiftTool,
  // Smoothness (2)
  constrainedFitTool,
  smoothnessPenaltyTool,
  // Nelson-Siegel / NSS (4)
  nelsonSiegelFitTool,
  nelsonSiegelEvaluateTool,
  nssFitTool,
  nssEvaluateTool,
  // Inflation (3)
  inflationBuildTool,
  inflationBootstrapTool,
  inflationSeasonalityTool,
  // Multicurve (2)
  multicurveSetupTool,
  basisSpreadTool,
  // Cross-Currency / Real Rate (2)
  crossCurrencyBasisTool,
  realRateTool,
];
