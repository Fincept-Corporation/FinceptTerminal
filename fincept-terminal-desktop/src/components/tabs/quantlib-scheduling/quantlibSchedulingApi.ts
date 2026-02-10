// File: src/components/tabs/quantlib-scheduling/quantlibSchedulingApi.ts
// API service for QuantLib Scheduling endpoints (14 endpoints)

const BASE_URL = 'https://finceptbackend.share.zrok.io';

let _apiKey: string | null = null;
export function setSchedulingApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any, method: string = 'POST'): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;
  const opts: RequestInit = { method, headers };
  if (body && method === 'POST') opts.body = JSON.stringify(body);
  const res = await fetch(`${BASE_URL}${path}`, opts);
  if (!res.ok) { const text = await res.text(); throw new Error(`API ${res.status}: ${text}`); }
  return res.json();
}

// === CALENDAR (6) ===
export const calendarApi = {
  isBusinessDay: (date: string, calendar?: string) =>
    apiCall('/quantlib/scheduling/calendar/is-business-day', { date, calendar }),
  nextBusinessDay: (date: string, calendar?: string) =>
    apiCall('/quantlib/scheduling/calendar/next-business-day', { date, calendar }),
  previousBusinessDay: (date: string, calendar?: string) =>
    apiCall('/quantlib/scheduling/calendar/previous-business-day', { date, calendar }),
  addBusinessDays: (date: string, days: number, calendar?: string) =>
    apiCall('/quantlib/scheduling/calendar/add-business-days', { date, days, calendar }),
  businessDaysBetween: (start_date: string, end_date: string, calendar?: string) =>
    apiCall('/quantlib/scheduling/calendar/business-days-between', { start_date, end_date, calendar }),
  list: () =>
    apiCall('/quantlib/scheduling/calendar/list', undefined, 'GET'),
};

// === ADJUSTMENT (3) ===
export const adjustmentApi = {
  adjustDate: (date: string, adjustment?: string, calendar?: string) =>
    apiCall('/quantlib/scheduling/adjustment/adjust-date', { date, adjustment, calendar }),
  batchAdjust: (dates: string[], adjustment?: string, calendar?: string) =>
    apiCall('/quantlib/scheduling/adjustment/batch-adjust', { dates, adjustment, calendar }),
  methods: () =>
    apiCall('/quantlib/scheduling/adjustment/methods', undefined, 'GET'),
};

// === DAY COUNT (4) ===
export const dayCountApi = {
  yearFraction: (start_date: string, end_date: string, convention?: string) =>
    apiCall('/quantlib/scheduling/daycount/year-fraction', { start_date, end_date, convention }),
  dayCount: (start_date: string, end_date: string, convention?: string) =>
    apiCall('/quantlib/scheduling/daycount/day-count', { start_date, end_date, convention }),
  batchYearFraction: (date_pairs: string[][], convention?: string) =>
    apiCall('/quantlib/scheduling/daycount/batch-year-fraction', { date_pairs, convention }),
  conventions: () =>
    apiCall('/quantlib/scheduling/daycount/conventions', undefined, 'GET'),
};

// === SCHEDULE (1) ===
export const scheduleApi = {
  generate: (effective_date: string, termination_date: string, frequency?: string, calendar?: string, adjustment?: string, stub?: string) =>
    apiCall('/quantlib/scheduling/schedule/generate', { effective_date, termination_date, frequency, calendar, adjustment, stub }),
};
