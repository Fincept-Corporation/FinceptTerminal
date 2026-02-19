// Economics MCP Tools - Query Fincept Macro API endpoints via MCP
// Covers: CEIC economic series, economic calendar, World Government Bonds (WGB)

import { InternalTool } from '../types';

export const economicsTools: InternalTool[] = [

  // ── CEIC Economic Series ────────────────────────────────────────────────────

  {
    name: 'economics_ceic_countries',
    description: 'List all countries available in the CEIC economic series database. Use this to discover valid country slugs (e.g. "united-states", "china", "india") before fetching indicators or series data.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getFinceptCeicCountries) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptCeicCountries();
      return { success: true, data, message: `Found ${data.length} countries in CEIC database` };
    },
  },

  {
    name: 'economics_ceic_indicators',
    description: 'List all economic indicators available for a specific country in the CEIC database. Returns indicator names and their series IDs. Call economics_ceic_countries first to get valid country slugs.',
    inputSchema: {
      type: 'object',
      properties: {
        country: {
          type: 'string',
          description: 'Country slug from CEIC, e.g. "united-states", "china", "india", "germany", "japan", "brazil"',
        },
      },
      required: ['country'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getFinceptCeicIndicators) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptCeicIndicators(args.country);
      return {
        success: true,
        data,
        message: `Found ${data.length} indicators for ${args.country}`,
      };
    },
  },

  {
    name: 'economics_ceic_series',
    description: 'Fetch CEIC economic time-series data for a country. Optionally filter by indicator name. Returns rows with a `year` integer field (use this as the date), `value`, and `series_id`. Filter precision: use year_from/year_to to scope the range. Use this to get GDP, CPI, unemployment, trade balance, bank lending rate, and hundreds of other economic series.',
    inputSchema: {
      type: 'object',
      properties: {
        country: {
          type: 'string',
          description: 'Country slug, e.g. "united-states", "china", "india"',
        },
        indicator: {
          type: 'string',
          description: 'Optional: specific indicator name to filter, e.g. "GDP", "CPI", "Unemployment Rate". Leave empty to return all available indicators for the country.',
        },
        year_from: {
          type: 'number',
          description: 'Start year for data range (default: 5 years ago)',
        },
        year_to: {
          type: 'number',
          description: 'End year for data range (default: current year)',
        },
        limit: {
          type: 'number',
          description: 'Maximum number of data rows to return (default: 500)',
        },
      },
      required: ['country'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getFinceptCeicSeries) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptCeicSeries({
        country: args.country,
        indicator: args.indicator,
        yearFrom: args.year_from,
        yearTo: args.year_to,
        limit: args.limit,
      });
      return {
        success: true,
        data,
        message: `Fetched ${data.length} series rows for ${args.country}${args.indicator ? ` (${args.indicator})` : ''}`,
      };
    },
  },

  // ── Economic Calendar ───────────────────────────────────────────────────────

  {
    name: 'economics_calendar',
    description: 'Get economic calendar events for a date range. Returns scheduled macro releases like GDP, CPI, NFP, central bank decisions, PMI prints, and more. Each event includes country, event name, date/time, actual value, forecast, and previous.',
    inputSchema: {
      type: 'object',
      properties: {
        start_date: {
          type: 'string',
          description: 'Start date in YYYY-MM-DD format (default: today)',
        },
        end_date: {
          type: 'string',
          description: 'End date in YYYY-MM-DD format (default: 30 days from today)',
        },
        limit: {
          type: 'number',
          description: 'Maximum number of events to return (default: 200)',
        },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getFinceptEconomicCalendar) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptEconomicCalendar({
        startDate: args.start_date,
        endDate: args.end_date,
        limit: args.limit,
      });
      return {
        success: true,
        data,
        message: `Found ${data.length} economic events`,
      };
    },
  },

  {
    name: 'economics_upcoming_events',
    description: 'Get upcoming economic events from Trading Economics. Returns the next scheduled macro releases globally — useful for event-driven trading and risk awareness.',
    inputSchema: {
      type: 'object',
      properties: {
        limit: {
          type: 'number',
          description: 'Maximum number of upcoming events to return (default: 200)',
        },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.getFinceptUpcomingEvents) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptUpcomingEvents(args.limit);
      return {
        success: true,
        data,
        message: `Found ${data.length} upcoming economic events`,
      };
    },
  },

  // ── World Government Bonds ──────────────────────────────────────────────────

  {
    name: 'economics_central_bank_rates',
    description: 'Get current central bank policy interest rates for all major countries. Shows the official benchmark rate set by each country\'s central bank (e.g. Fed Funds Rate, ECB rate, BOJ rate, RBI repo rate).',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getFinceptCentralBankRates) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptCentralBankRates();
      return {
        success: true,
        data,
        message: `Fetched central bank rates for ${data.length} countries`,
      };
    },
  },

  {
    name: 'economics_sovereign_credit_ratings',
    description: 'Get sovereign credit ratings for all countries from Moody\'s, S&P, and Fitch. Useful for country risk analysis, sovereign debt investing, and fixed income research.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getFinceptCreditRatings) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptCreditRatings();
      return {
        success: true,
        data,
        message: `Fetched sovereign credit ratings for ${data.length} countries`,
      };
    },
  },

  {
    name: 'economics_sovereign_cds',
    description: 'Get sovereign Credit Default Swap (CDS) spreads for major countries. CDS spreads reflect the market-implied probability of sovereign default — a key measure of country credit risk.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getFinceptSovereignCds) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptSovereignCds();
      return {
        success: true,
        data,
        message: `Fetched sovereign CDS spreads for ${data.length} countries`,
      };
    },
  },

  {
    name: 'economics_bond_spreads',
    description: 'Get government bond yield spreads for major countries vs benchmarks (e.g. vs US Treasuries or German Bunds). Spreads indicate relative sovereign risk and borrowing costs.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getFinceptBondSpreads) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptBondSpreads();
      return {
        success: true,
        data,
        message: `Fetched government bond spreads for ${data.length} countries`,
      };
    },
  },

  {
    name: 'economics_inverted_yields',
    description: 'Get countries currently showing inverted yield curves. An inverted yield curve (short-term rates > long-term rates) is historically a leading indicator of recession.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getFinceptInvertedYields) {
        return { success: false, error: 'Economics context not available' };
      }
      const data = await contexts.getFinceptInvertedYields();
      return {
        success: true,
        data,
        message: `Found ${data.length} countries with inverted yield curves`,
      };
    },
  },
];
