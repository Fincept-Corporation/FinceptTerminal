// QuantLib Instruments MCP Tools
// 26 tools for bonds, swaps, FRA, money market, OIS, equity, commodity, FX, CDS, futures

import type { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

// ══════════════════════════════════════════════════════════════════════════════
// FIXED BOND (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const fixedBondCashflowsTool: InternalTool = {
  name: 'quantlib_instruments_bond_fixed_cashflows',
  description: 'Generate cashflow schedule for a fixed-rate bond.',
  inputSchema: {
    type: 'object',
    properties: {
      issue_date: { type: 'string', description: 'Issue date (YYYY-MM-DD)' },
      maturity_date: { type: 'string', description: 'Maturity date (YYYY-MM-DD)' },
      coupon_rate: { type: 'number', description: 'Annual coupon rate (e.g., 0.05 for 5%)' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      frequency: { type: 'number', description: 'Coupon frequency per year (default: 2)' },
      day_count: { type: 'string', description: 'Day count convention (default: ACT/365)' },
      settlement_date: { type: 'string', description: 'Settlement date (optional)' },
    },
    required: ['issue_date', 'maturity_date', 'coupon_rate'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/bond/fixed/cashflows', {
        issue_date: args.issue_date,
        maturity_date: args.maturity_date,
        coupon_rate: args.coupon_rate,
        face_value: args.face_value || 100,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        settlement_date: args.settlement_date,
      }, apiKey);
      return { success: true, data: result, message: `Generated ${result.cashflows?.length || 0} cashflows for fixed bond` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const fixedBondPriceTool: InternalTool = {
  name: 'quantlib_instruments_bond_fixed_price',
  description: 'Calculate price of a fixed-rate bond given yield.',
  inputSchema: {
    type: 'object',
    properties: {
      issue_date: { type: 'string', description: 'Issue date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      coupon_rate: { type: 'number', description: 'Annual coupon rate' },
      yield_rate: { type: 'number', description: 'Yield to maturity' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      frequency: { type: 'number', description: 'Coupon frequency (default: 2)' },
      day_count: { type: 'string', description: 'Day count convention' },
      settlement_date: { type: 'string', description: 'Settlement date' },
    },
    required: ['issue_date', 'maturity_date', 'coupon_rate', 'yield_rate'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/bond/fixed/price', {
        issue_date: args.issue_date,
        maturity_date: args.maturity_date,
        coupon_rate: args.coupon_rate,
        yield_rate: args.yield_rate,
        face_value: args.face_value || 100,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        settlement_date: args.settlement_date,
      }, apiKey);
      return { success: true, data: result, message: `Bond price: ${formatNumber(result.clean_price)} (clean), ${formatNumber(result.dirty_price)} (dirty)` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const fixedBondYieldTool: InternalTool = {
  name: 'quantlib_instruments_bond_fixed_yield',
  description: 'Calculate yield to maturity from bond price.',
  inputSchema: {
    type: 'object',
    properties: {
      issue_date: { type: 'string', description: 'Issue date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      coupon_rate: { type: 'number', description: 'Annual coupon rate' },
      clean_price: { type: 'number', description: 'Clean price' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      frequency: { type: 'number', description: 'Coupon frequency (default: 2)' },
      day_count: { type: 'string', description: 'Day count convention' },
      settlement_date: { type: 'string', description: 'Settlement date' },
    },
    required: ['issue_date', 'maturity_date', 'coupon_rate', 'clean_price'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/bond/fixed/yield', {
        issue_date: args.issue_date,
        maturity_date: args.maturity_date,
        coupon_rate: args.coupon_rate,
        clean_price: args.clean_price,
        face_value: args.face_value || 100,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        settlement_date: args.settlement_date,
      }, apiKey);
      return { success: true, data: result, message: `Yield to maturity: ${formatPercent(result.yield)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const fixedBondAnalyticsTool: InternalTool = {
  name: 'quantlib_instruments_bond_fixed_analytics',
  description: 'Calculate bond analytics (duration, convexity, etc.).',
  inputSchema: {
    type: 'object',
    properties: {
      issue_date: { type: 'string', description: 'Issue date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      coupon_rate: { type: 'number', description: 'Annual coupon rate' },
      yield_rate: { type: 'number', description: 'Yield to maturity' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      frequency: { type: 'number', description: 'Coupon frequency (default: 2)' },
      day_count: { type: 'string', description: 'Day count convention' },
      settlement_date: { type: 'string', description: 'Settlement date' },
    },
    required: ['issue_date', 'maturity_date', 'coupon_rate', 'yield_rate'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/bond/fixed/analytics', {
        issue_date: args.issue_date,
        maturity_date: args.maturity_date,
        coupon_rate: args.coupon_rate,
        yield_rate: args.yield_rate,
        face_value: args.face_value || 100,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        settlement_date: args.settlement_date,
      }, apiKey);
      return { success: true, data: result, message: `Duration: ${formatNumber(result.duration)}, Convexity: ${formatNumber(result.convexity)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// ZERO COUPON BOND (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const zeroBondPriceTool: InternalTool = {
  name: 'quantlib_instruments_bond_zero_coupon_price',
  description: 'Price a zero-coupon bond.',
  inputSchema: {
    type: 'object',
    properties: {
      issue_date: { type: 'string', description: 'Issue date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      day_count: { type: 'string', description: 'Day count convention' },
      settlement_date: { type: 'string', description: 'Settlement date' },
    },
    required: ['issue_date', 'maturity_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/bond/zero-coupon/price', {
        issue_date: args.issue_date,
        maturity_date: args.maturity_date,
        face_value: args.face_value || 100,
        day_count: args.day_count || 'ACT/365',
        settlement_date: args.settlement_date,
      }, apiKey);
      return { success: true, data: result, message: `Zero-coupon bond price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// INFLATION-LINKED BOND (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const inflationBondCalcTool: InternalTool = {
  name: 'quantlib_instruments_bond_inflation_linked',
  description: 'Calculate inflation-linked bond values.',
  inputSchema: {
    type: 'object',
    properties: {
      issue_date: { type: 'string', description: 'Issue date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      coupon_rate: { type: 'number', description: 'Real coupon rate' },
      base_cpi: { type: 'number', description: 'Base CPI at issue' },
      current_cpi: { type: 'number', description: 'Current CPI' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      frequency: { type: 'number', description: 'Coupon frequency (default: 2)' },
    },
    required: ['issue_date', 'maturity_date', 'coupon_rate', 'base_cpi', 'current_cpi'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/bond/inflation-linked', {
        issue_date: args.issue_date,
        maturity_date: args.maturity_date,
        coupon_rate: args.coupon_rate,
        base_cpi: args.base_cpi,
        current_cpi: args.current_cpi,
        face_value: args.face_value || 100,
        frequency: args.frequency || 2,
      }, apiKey);
      return { success: true, data: result, message: `Inflation-adjusted principal: ${formatNumber(result.adjusted_principal)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// INTEREST RATE SWAP (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const irsValueTool: InternalTool = {
  name: 'quantlib_instruments_swap_irs_value',
  description: 'Calculate the present value of an interest rate swap.',
  inputSchema: {
    type: 'object',
    properties: {
      effective_date: { type: 'string', description: 'Effective date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      notional: { type: 'number', description: 'Notional amount' },
      fixed_rate: { type: 'number', description: 'Fixed rate' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      reference_date: { type: 'string', description: 'Valuation date' },
      frequency: { type: 'number', description: 'Payment frequency (default: 2)' },
      day_count: { type: 'string', description: 'Day count convention' },
      swap_type: { type: 'string', description: 'payer or receiver (default: payer)' },
    },
    required: ['effective_date', 'maturity_date', 'notional', 'fixed_rate', 'curve_tenors', 'curve_rates', 'reference_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/swap/irs/value', {
        effective_date: args.effective_date,
        maturity_date: args.maturity_date,
        notional: args.notional,
        fixed_rate: args.fixed_rate,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        reference_date: args.reference_date,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        swap_type: args.swap_type || 'payer',
      }, apiKey);
      return { success: true, data: result, message: `IRS NPV: ${formatNumber(result.npv)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const irsParRateTool: InternalTool = {
  name: 'quantlib_instruments_swap_irs_par_rate',
  description: 'Calculate the par swap rate.',
  inputSchema: {
    type: 'object',
    properties: {
      effective_date: { type: 'string', description: 'Effective date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      notional: { type: 'number', description: 'Notional amount' },
      fixed_rate: { type: 'number', description: 'Current fixed rate' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      reference_date: { type: 'string', description: 'Valuation date' },
      frequency: { type: 'number', description: 'Payment frequency' },
      day_count: { type: 'string', description: 'Day count convention' },
      swap_type: { type: 'string', description: 'payer or receiver' },
    },
    required: ['effective_date', 'maturity_date', 'notional', 'fixed_rate', 'curve_tenors', 'curve_rates', 'reference_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/swap/irs/par-rate', {
        effective_date: args.effective_date,
        maturity_date: args.maturity_date,
        notional: args.notional,
        fixed_rate: args.fixed_rate,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        reference_date: args.reference_date,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        swap_type: args.swap_type || 'payer',
      }, apiKey);
      return { success: true, data: result, message: `Par swap rate: ${formatPercent(result.par_rate)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const irsDv01Tool: InternalTool = {
  name: 'quantlib_instruments_swap_irs_dv01',
  description: 'Calculate DV01 (dollar value of 1bp) for an IRS.',
  inputSchema: {
    type: 'object',
    properties: {
      effective_date: { type: 'string', description: 'Effective date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      notional: { type: 'number', description: 'Notional amount' },
      fixed_rate: { type: 'number', description: 'Fixed rate' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      reference_date: { type: 'string', description: 'Valuation date' },
      frequency: { type: 'number', description: 'Payment frequency' },
      day_count: { type: 'string', description: 'Day count convention' },
      swap_type: { type: 'string', description: 'payer or receiver' },
    },
    required: ['effective_date', 'maturity_date', 'notional', 'fixed_rate', 'curve_tenors', 'curve_rates', 'reference_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/swap/irs/dv01', {
        effective_date: args.effective_date,
        maturity_date: args.maturity_date,
        notional: args.notional,
        fixed_rate: args.fixed_rate,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        reference_date: args.reference_date,
        frequency: args.frequency || 2,
        day_count: args.day_count || 'ACT/365',
        swap_type: args.swap_type || 'payer',
      }, apiKey);
      return { success: true, data: result, message: `IRS DV01: ${formatNumber(result.dv01)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// FRA (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const fraValueTool: InternalTool = {
  name: 'quantlib_instruments_fra_value',
  description: 'Calculate the present value of a Forward Rate Agreement.',
  inputSchema: {
    type: 'object',
    properties: {
      trade_date: { type: 'string', description: 'Trade date' },
      start_months: { type: 'number', description: 'Start period in months' },
      end_months: { type: 'number', description: 'End period in months' },
      notional: { type: 'number', description: 'Notional amount' },
      fixed_rate: { type: 'number', description: 'FRA fixed rate' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      reference_date: { type: 'string', description: 'Valuation date' },
    },
    required: ['trade_date', 'start_months', 'end_months', 'notional', 'fixed_rate', 'curve_tenors', 'curve_rates', 'reference_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/fra/value', {
        trade_date: args.trade_date,
        start_months: args.start_months,
        end_months: args.end_months,
        notional: args.notional,
        fixed_rate: args.fixed_rate,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        reference_date: args.reference_date,
      }, apiKey);
      return { success: true, data: result, message: `FRA NPV: ${formatNumber(result.npv)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const fraBreakEvenTool: InternalTool = {
  name: 'quantlib_instruments_fra_break_even',
  description: 'Calculate the break-even rate for a FRA.',
  inputSchema: {
    type: 'object',
    properties: {
      trade_date: { type: 'string', description: 'Trade date' },
      start_months: { type: 'number', description: 'Start period in months' },
      end_months: { type: 'number', description: 'End period in months' },
      notional: { type: 'number', description: 'Notional amount' },
      fixed_rate: { type: 'number', description: 'Current FRA rate' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      reference_date: { type: 'string', description: 'Valuation date' },
    },
    required: ['trade_date', 'start_months', 'end_months', 'notional', 'fixed_rate', 'curve_tenors', 'curve_rates', 'reference_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/fra/break-even', {
        trade_date: args.trade_date,
        start_months: args.start_months,
        end_months: args.end_months,
        notional: args.notional,
        fixed_rate: args.fixed_rate,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        reference_date: args.reference_date,
      }, apiKey);
      return { success: true, data: result, message: `FRA break-even rate: ${formatPercent(result.break_even_rate)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// MONEY MARKET (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const depositTool: InternalTool = {
  name: 'quantlib_instruments_money_market_deposit',
  description: 'Calculate money market deposit value and accrued interest.',
  inputSchema: {
    type: 'object',
    properties: {
      start_date: { type: 'string', description: 'Start date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      notional: { type: 'number', description: 'Deposit amount' },
      rate: { type: 'number', description: 'Interest rate' },
      day_count: { type: 'string', description: 'Day count convention (default: ACT/360)' },
    },
    required: ['start_date', 'maturity_date', 'notional', 'rate'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/money-market/deposit', {
        start_date: args.start_date,
        maturity_date: args.maturity_date,
        notional: args.notional,
        rate: args.rate,
        day_count: args.day_count || 'ACT/360',
      }, apiKey);
      return { success: true, data: result, message: `Maturity value: ${formatNumber(result.maturity_value)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const tbillTool: InternalTool = {
  name: 'quantlib_instruments_money_market_tbill',
  description: 'Calculate T-Bill price/yield conversion.',
  inputSchema: {
    type: 'object',
    properties: {
      settlement_date: { type: 'string', description: 'Settlement date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      face_value: { type: 'number', description: 'Face value (default: 100)' },
      discount_rate: { type: 'number', description: 'Discount rate (provide either this or price)' },
      price: { type: 'number', description: 'Price (provide either this or discount_rate)' },
    },
    required: ['settlement_date', 'maturity_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/money-market/tbill', {
        settlement_date: args.settlement_date,
        maturity_date: args.maturity_date,
        face_value: args.face_value || 100,
        discount_rate: args.discount_rate,
        price: args.price,
      }, apiKey);
      return { success: true, data: result, message: `T-Bill: price=${formatNumber(result.price)}, yield=${formatPercent(result.yield)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const repoTool: InternalTool = {
  name: 'quantlib_instruments_money_market_repo',
  description: 'Calculate repo transaction values.',
  inputSchema: {
    type: 'object',
    properties: {
      start_date: { type: 'string', description: 'Repo start date' },
      end_date: { type: 'string', description: 'Repo end date' },
      collateral_value: { type: 'number', description: 'Collateral market value' },
      repo_rate: { type: 'number', description: 'Repo rate' },
      haircut: { type: 'number', description: 'Haircut percentage (default: 0.02)' },
    },
    required: ['start_date', 'end_date', 'collateral_value', 'repo_rate'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/money-market/repo', {
        start_date: args.start_date,
        end_date: args.end_date,
        collateral_value: args.collateral_value,
        repo_rate: args.repo_rate,
        haircut: args.haircut || 0.02,
      }, apiKey);
      return { success: true, data: result, message: `Repo: purchase=${formatNumber(result.purchase_price)}, repurchase=${formatNumber(result.repurchase_price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// OIS (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const oisValueTool: InternalTool = {
  name: 'quantlib_instruments_ois_value',
  description: 'Calculate the present value of an OIS swap.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Valuation date' },
      effective_date: { type: 'string', description: 'Effective date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      notional: { type: 'number', description: 'Notional amount' },
      fixed_rate: { type: 'number', description: 'Fixed OIS rate' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      index: { type: 'string', description: 'OIS index (default: SOFR)' },
    },
    required: ['reference_date', 'effective_date', 'maturity_date', 'notional', 'fixed_rate', 'curve_tenors', 'curve_rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/ois/value', {
        reference_date: args.reference_date,
        effective_date: args.effective_date,
        maturity_date: args.maturity_date,
        notional: args.notional,
        fixed_rate: args.fixed_rate,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        index: args.index || 'SOFR',
      }, apiKey);
      return { success: true, data: result, message: `OIS NPV: ${formatNumber(result.npv)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const oisBuildCurveTool: InternalTool = {
  name: 'quantlib_instruments_ois_build_curve',
  description: 'Build OIS discount curve from OIS rates.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Reference date' },
      ois_rates: { type: 'string', description: 'JSON array of OIS rates' },
    },
    required: ['reference_date', 'ois_rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/ois/build-curve', {
        reference_date: args.reference_date,
        ois_rates: JSON.parse(args.ois_rates as string),
      }, apiKey);
      return { success: true, data: result, message: 'Built OIS discount curve' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EQUITY (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const varianceSwapTool: InternalTool = {
  name: 'quantlib_instruments_equity_variance_swap',
  description: 'Calculate variance swap P&L.',
  inputSchema: {
    type: 'object',
    properties: {
      strike_variance: { type: 'number', description: 'Strike variance' },
      realized_returns: { type: 'string', description: 'JSON array of realized returns' },
      notional: { type: 'number', description: 'Variance notional' },
      annualization_factor: { type: 'number', description: 'Annualization factor (default: 252)' },
    },
    required: ['strike_variance', 'realized_returns', 'notional'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/equity/variance-swap', {
        strike_variance: args.strike_variance,
        realized_returns: JSON.parse(args.realized_returns as string),
        notional: args.notional,
        annualization_factor: args.annualization_factor || 252,
      }, apiKey);
      return { success: true, data: result, message: `Variance swap P&L: ${formatNumber(result.pnl)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const volatilitySwapTool: InternalTool = {
  name: 'quantlib_instruments_equity_volatility_swap',
  description: 'Calculate volatility swap P&L.',
  inputSchema: {
    type: 'object',
    properties: {
      strike_vol: { type: 'number', description: 'Strike volatility' },
      realized_returns: { type: 'string', description: 'JSON array of realized returns' },
      notional: { type: 'number', description: 'Vega notional' },
      annualization_factor: { type: 'number', description: 'Annualization factor (default: 252)' },
    },
    required: ['strike_vol', 'realized_returns', 'notional'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/equity/volatility-swap', {
        strike_vol: args.strike_vol,
        realized_returns: JSON.parse(args.realized_returns as string),
        notional: args.notional,
        annualization_factor: args.annualization_factor || 252,
      }, apiKey);
      return { success: true, data: result, message: `Volatility swap P&L: ${formatNumber(result.pnl)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// COMMODITY (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const commodityFutureTool: InternalTool = {
  name: 'quantlib_instruments_commodity_future',
  description: 'Calculate commodity futures analytics.',
  inputSchema: {
    type: 'object',
    properties: {
      commodity_type: { type: 'string', description: 'Commodity type (e.g., CRUDE, GOLD)' },
      spot_price: { type: 'number', description: 'Current spot price' },
      delivery_date: { type: 'string', description: 'Delivery date' },
      trade_date: { type: 'string', description: 'Trade date' },
      futures_price: { type: 'number', description: 'Futures price' },
      contract_size: { type: 'number', description: 'Contract size (default: 1)' },
    },
    required: ['commodity_type', 'spot_price', 'delivery_date', 'trade_date', 'futures_price'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/commodity/future', {
        commodity_type: args.commodity_type,
        spot_price: args.spot_price,
        delivery_date: args.delivery_date,
        trade_date: args.trade_date,
        futures_price: args.futures_price,
        contract_size: args.contract_size || 1,
      }, apiKey);
      return { success: true, data: result, message: `${args.commodity_type} futures: basis=${formatNumber(result.basis)}, carry=${formatPercent(result.implied_carry)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// FX (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const fxForwardTool: InternalTool = {
  name: 'quantlib_instruments_fx_forward',
  description: 'Calculate FX forward points and value.',
  inputSchema: {
    type: 'object',
    properties: {
      spot_rate: { type: 'number', description: 'Current spot rate' },
      base_rate: { type: 'number', description: 'Base currency interest rate' },
      quote_rate: { type: 'number', description: 'Quote currency interest rate' },
      time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
      notional: { type: 'number', description: 'Notional amount' },
      pair: { type: 'string', description: 'Currency pair (default: EURUSD)' },
    },
    required: ['spot_rate', 'base_rate', 'quote_rate', 'time_to_maturity', 'notional'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/fx/forward', {
        spot_rate: args.spot_rate,
        base_rate: args.base_rate,
        quote_rate: args.quote_rate,
        time_to_maturity: args.time_to_maturity,
        notional: args.notional,
        pair: args.pair || 'EURUSD',
      }, apiKey);
      return { success: true, data: result, message: `FX forward: ${formatNumber(result.forward_rate)}, points=${formatNumber(result.forward_points * 10000)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const garmanKohlhagenTool: InternalTool = {
  name: 'quantlib_instruments_fx_garman_kohlhagen',
  description: 'Price FX options using Garman-Kohlhagen model.',
  inputSchema: {
    type: 'object',
    properties: {
      spot: { type: 'number', description: 'Spot FX rate' },
      strike: { type: 'number', description: 'Strike rate' },
      domestic_rate: { type: 'number', description: 'Domestic interest rate' },
      foreign_rate: { type: 'number', description: 'Foreign interest rate' },
      volatility: { type: 'number', description: 'FX volatility' },
      time_to_expiry: { type: 'number', description: 'Time to expiry in years' },
      option_type: { type: 'string', description: 'call or put (default: call)' },
    },
    required: ['spot', 'strike', 'domestic_rate', 'foreign_rate', 'volatility', 'time_to_expiry'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/fx/garman-kohlhagen', {
        spot: args.spot,
        strike: args.strike,
        domestic_rate: args.domestic_rate,
        foreign_rate: args.foreign_rate,
        volatility: args.volatility,
        time_to_expiry: args.time_to_expiry,
        option_type: args.option_type || 'call',
      }, apiKey);
      return { success: true, data: result, message: `FX ${args.option_type || 'call'} price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CDS (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const cdsValueTool: InternalTool = {
  name: 'quantlib_instruments_cds_value',
  description: 'Calculate CDS present value.',
  inputSchema: {
    type: 'object',
    properties: {
      reference_date: { type: 'string', description: 'Valuation date' },
      effective_date: { type: 'string', description: 'Effective date' },
      maturity_date: { type: 'string', description: 'Maturity date' },
      notional: { type: 'number', description: 'Notional amount' },
      spread_bps: { type: 'number', description: 'CDS spread in bps' },
      curve_tenors: { type: 'string', description: 'JSON array of curve tenors' },
      curve_rates: { type: 'string', description: 'JSON array of curve rates' },
      recovery_rate: { type: 'number', description: 'Recovery rate (default: 0.4)' },
      frequency: { type: 'number', description: 'Premium frequency (default: 4)' },
    },
    required: ['reference_date', 'effective_date', 'maturity_date', 'notional', 'spread_bps', 'curve_tenors', 'curve_rates'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/cds/value', {
        reference_date: args.reference_date,
        effective_date: args.effective_date,
        maturity_date: args.maturity_date,
        notional: args.notional,
        spread_bps: args.spread_bps,
        curve_tenors: JSON.parse(args.curve_tenors as string),
        curve_rates: JSON.parse(args.curve_rates as string),
        recovery_rate: args.recovery_rate || 0.4,
        frequency: args.frequency || 4,
      }, apiKey);
      return { success: true, data: result, message: `CDS NPV: ${formatNumber(result.npv)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const cdsHazardRateTool: InternalTool = {
  name: 'quantlib_instruments_cds_hazard_rate',
  description: 'Calculate implied hazard rate from CDS spread.',
  inputSchema: {
    type: 'object',
    properties: {
      spread_bps: { type: 'number', description: 'CDS spread in bps' },
      recovery_rate: { type: 'number', description: 'Recovery rate (default: 0.4)' },
    },
    required: ['spread_bps'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/cds/hazard-rate', {
        spread_bps: args.spread_bps,
        recovery_rate: args.recovery_rate || 0.4,
      }, apiKey);
      return { success: true, data: result, message: `Implied hazard rate: ${formatPercent(result.hazard_rate)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const cdsSurvivalProbTool: InternalTool = {
  name: 'quantlib_instruments_cds_survival_probability',
  description: 'Calculate survival probabilities from hazard rate.',
  inputSchema: {
    type: 'object',
    properties: {
      hazard_rate: { type: 'number', description: 'Constant hazard rate' },
      time_horizon: { type: 'number', description: 'Time horizon in years' },
      time_points: { type: 'string', description: 'Optional JSON array of time points' },
    },
    required: ['hazard_rate', 'time_horizon'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/cds/survival-probability', {
        hazard_rate: args.hazard_rate,
        time_horizon: args.time_horizon,
        time_points: args.time_points ? JSON.parse(args.time_points as string) : undefined,
      }, apiKey);
      return { success: true, data: result, message: `Survival probability at ${args.time_horizon}Y: ${formatPercent(result.survival_probability)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// FUTURES (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const stirFutureTool: InternalTool = {
  name: 'quantlib_instruments_futures_stir',
  description: 'Calculate STIR (Short Term Interest Rate) futures analytics.',
  inputSchema: {
    type: 'object',
    properties: {
      price: { type: 'number', description: 'Futures price (e.g., 95.50)' },
      contract_date: { type: 'string', description: 'Contract expiry date' },
      reference_date: { type: 'string', description: 'Valuation date' },
      notional: { type: 'number', description: 'Notional (default: 1000000)' },
      tick_value: { type: 'number', description: 'Tick value (default: 25)' },
    },
    required: ['price', 'contract_date', 'reference_date'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/futures/stir', {
        price: args.price,
        contract_date: args.contract_date,
        reference_date: args.reference_date,
        notional: args.notional || 1000000,
        tick_value: args.tick_value || 25,
      }, apiKey);
      return { success: true, data: result, message: `STIR implied rate: ${formatPercent(result.implied_rate)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const bondCtdTool: InternalTool = {
  name: 'quantlib_instruments_futures_bond_ctd',
  description: 'Find cheapest-to-deliver bond for bond futures.',
  inputSchema: {
    type: 'object',
    properties: {
      futures_price: { type: 'number', description: 'Futures price' },
      delivery_date: { type: 'string', description: 'Delivery date' },
      reference_date: { type: 'string', description: 'Valuation date' },
      deliverables: { type: 'string', description: 'JSON array of deliverable bonds with conversion factors' },
    },
    required: ['futures_price', 'delivery_date', 'reference_date', 'deliverables'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/instruments/futures/bond/ctd', {
        futures_price: args.futures_price,
        delivery_date: args.delivery_date,
        reference_date: args.reference_date,
        deliverables: JSON.parse(args.deliverables as string),
      }, apiKey);
      return { success: true, data: result, message: `CTD: ${result.ctd_bond}, basis=${formatNumber(result.basis)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EXPORT ALL TOOLS
// ══════════════════════════════════════════════════════════════════════════════

export const quantlibInstrumentsTools: InternalTool[] = [
  // Fixed Bond (4)
  fixedBondCashflowsTool,
  fixedBondPriceTool,
  fixedBondYieldTool,
  fixedBondAnalyticsTool,
  // Zero Coupon (1)
  zeroBondPriceTool,
  // Inflation-Linked (1)
  inflationBondCalcTool,
  // IRS (3)
  irsValueTool,
  irsParRateTool,
  irsDv01Tool,
  // FRA (2)
  fraValueTool,
  fraBreakEvenTool,
  // Money Market (3)
  depositTool,
  tbillTool,
  repoTool,
  // OIS (2)
  oisValueTool,
  oisBuildCurveTool,
  // Equity (2)
  varianceSwapTool,
  volatilitySwapTool,
  // Commodity (1)
  commodityFutureTool,
  // FX (2)
  fxForwardTool,
  garmanKohlhagenTool,
  // CDS (3)
  cdsValueTool,
  cdsHazardRateTool,
  cdsSurvivalProbTool,
  // Futures (2)
  stirFutureTool,
  bondCtdTool,
];
