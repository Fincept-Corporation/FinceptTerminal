// File: src/components/tabs/quantlib-economics/quantlibEconomicsApi.ts
// 25 endpoints — equilibrium, game theory, auctions, utility theory

const BASE_URL = 'https://finceptbackend.share.zrok.io';
let _apiKey: string | null = null;
export function setEconomicsApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Equilibrium (4) ────────────────────────────────────────────
export const equilibriumApi = {
  cobbDouglas: (alphas: number[], prices: number[], wealth = 100) =>
    apiCall('/quantlib/economics/equilibrium/cobb-douglas', { alphas, prices, wealth }),
  ces: (alphas: number[], rho: number, prices: number[], wealth = 100) =>
    apiCall('/quantlib/economics/equilibrium/ces', { alphas, rho, prices, wealth }),
  exchangeEconomy: (endowments: number[][], alphas: number[][]) =>
    apiCall('/quantlib/economics/equilibrium/exchange-economy', { endowments, alphas }),
  walrasian: (endowments: number[][], alphas: number[][], max_iter = 1000, tol = 0.000001) =>
    apiCall('/quantlib/economics/equilibrium/walrasian', { endowments, alphas, max_iter, tol }),
};

// ── Game Theory (7) ────────────────────────────────────────────
export const gameApi = {
  create: (payoff_matrix_1: number[][], payoff_matrix_2: number[][]) =>
    apiCall('/quantlib/economics/games/create', { payoff_matrix_1, payoff_matrix_2 }),
  classic: (game_name: string) =>
    apiCall('/quantlib/economics/games/classic', { game_name }),
  nashCheck: (payoff_matrix_1: number[][], payoff_matrix_2: number[][], strategy_1: number[], strategy_2: number[]) =>
    apiCall('/quantlib/economics/games/nash-check', { payoff_matrix_1, payoff_matrix_2, strategy_1, strategy_2 }),
  mixedNash: (payoff_matrix_1: number[][], payoff_matrix_2: number[][]) =>
    apiCall('/quantlib/economics/games/mixed-nash', { payoff_matrix_1, payoff_matrix_2 }),
  fictitiousPlay: (payoff_matrix_1: number[][], payoff_matrix_2: number[][], iterations = 1000) =>
    apiCall('/quantlib/economics/games/fictitious-play', { payoff_matrix_1, payoff_matrix_2, iterations }),
  bestResponse: (payoff_matrix_1: number[][], payoff_matrix_2: number[][], strategy_1: number[], strategy_2: number[]) =>
    apiCall('/quantlib/economics/games/best-response', { payoff_matrix_1, payoff_matrix_2, strategy_1, strategy_2 }),
  eliminateDominated: (payoff_matrix_1: number[][], payoff_matrix_2: number[][]) =>
    apiCall('/quantlib/economics/games/eliminate-dominated', { payoff_matrix_1, payoff_matrix_2 }),
};

// ── Auctions (4) ───────────────────────────────────────────────
export const auctionApi = {
  run: (auction_type: string, n_bidders: number, valuations?: number[]) =>
    apiCall('/quantlib/economics/auctions/run', { auction_type, n_bidders, valuations }),
  equilibriumBid: (auction_type: string, n_bidders: number, valuations?: number[]) =>
    apiCall('/quantlib/economics/auctions/equilibrium-bid', { auction_type, n_bidders, valuations }),
  expectedRevenue: (auction_type: string, n_bidders: number) =>
    apiCall('/quantlib/economics/auctions/expected-revenue', { auction_type, n_bidders }),
  simulate: (auction_type: string, n_bidders: number, n_simulations = 10000, value_dist = 'uniform') =>
    apiCall('/quantlib/economics/auctions/simulate', { auction_type, n_bidders, n_simulations, value_dist }),
};

// ── Utility Theory (10) ────────────────────────────────────────
export const utilityApi = {
  cara: (risk_aversion: number, wealth: number) =>
    apiCall('/quantlib/economics/utility/cara', { risk_aversion, wealth }),
  crra: (gamma: number, wealth: number) =>
    apiCall('/quantlib/economics/utility/crra', { gamma, wealth }),
  log: (wealth: number) =>
    apiCall('/quantlib/economics/utility/log', { wealth }),
  prospectTheory: (alpha = 0.88, beta = 0.88, lambda_param = 2.25, wealth = 0, outcome = 0) =>
    apiCall('/quantlib/economics/utility/prospect-theory', { alpha, beta, lambda_param, wealth, outcome }),
  quadratic: (wealth: number, b = 0.01) =>
    apiCall('/quantlib/economics/utility/quadratic', { wealth, b }),
  expectedUtility: (utility_type: string, outcomes: number[], probabilities?: number[], param?: number) =>
    apiCall('/quantlib/economics/utility/expected-utility', { utility_type, outcomes, probabilities, param }),
  certaintyEquivalent: (utility_type: string, outcomes: number[], probabilities?: number[], param?: number) =>
    apiCall('/quantlib/economics/utility/certainty-equivalent', { utility_type, outcomes, probabilities, param }),
  ceApprox: (mean: number, variance: number, risk_aversion: number) =>
    apiCall('/quantlib/economics/utility/certainty-equivalent-approximation', { mean, variance, risk_aversion }),
  riskPremium: (utility_type: string, outcomes: number[], probabilities?: number[], param?: number) =>
    apiCall('/quantlib/economics/utility/risk-premium', { utility_type, outcomes, probabilities, param }),
  stochasticDominance: (cdf_a: number[], cdf_b: number[], order = 1) =>
    apiCall('/quantlib/economics/utility/stochastic-dominance', { cdf_a, cdf_b, order }),
};
