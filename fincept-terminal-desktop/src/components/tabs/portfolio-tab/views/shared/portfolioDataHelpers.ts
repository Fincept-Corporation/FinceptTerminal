/**
 * Shared helpers for building Python-compatible data payloads from portfolio holdings.
 * Used across RiskManagement, Planning, Economics, and other analytics panels.
 */
import type { PortfolioSummary } from '../../../../../services/portfolio/portfolioService';

/** Extract active holdings (symbol present, quantity > 0) */
export function getActiveHoldings(portfolioSummary: PortfolioSummary) {
  return portfolioSummary.holdings.filter(h => h.symbol && h.quantity > 0);
}

/** Build a JSON string of comma-separated symbols for returns-based Python commands */
export function buildSymbolsCsv(portfolioSummary: PortfolioSummary): string {
  const holdings = getActiveHoldings(portfolioSummary);
  return JSON.stringify(holdings.map(h => h.symbol).join(','));
}

/** Build a JSON string of weights (0-1 scale) for Python commands */
export function buildWeightsJson(portfolioSummary: PortfolioSummary): string {
  const holdings = getActiveHoldings(portfolioSummary);
  return JSON.stringify(holdings.map(h => h.weight / 100));
}

/** Build a JSON string of current prices for Python commands */
export function buildPricesJson(portfolioSummary: PortfolioSummary): string {
  const holdings = getActiveHoldings(portfolioSummary);
  return JSON.stringify(holdings.map(h => h.current_price));
}

/** Safely parse a JSON string, returning the raw string if parsing fails */
export function safeParse(raw: string): any {
  try {
    return JSON.parse(raw);
  } catch {
    return raw;
  }
}
