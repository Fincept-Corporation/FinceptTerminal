// File: src/components/tabs/quantlib-physics/quantlibPhysicsApi.ts
// API service for QuantLib Physics endpoints (24 endpoints)

const BASE_URL = 'https://finceptbackend.share.zrok.io';

let _apiKey: string | null = null;
export function setPhysicsApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (_apiKey) headers['X-API-Key'] = _apiKey;
  const res = await fetch(`${BASE_URL}${path}`, { method: 'POST', headers, body: JSON.stringify(body) });
  if (!res.ok) { const text = await res.text(); throw new Error(`API ${res.status}: ${text}`); }
  return res.json();
}

// === ENTROPY (11) ===
export const entropyApi = {
  shannon: (probabilities: number[], base?: number) =>
    apiCall('/quantlib/physics/entropy/shannon', { probabilities, base }),
  renyi: (probabilities: number[], alpha: number, base?: number) =>
    apiCall('/quantlib/physics/entropy/renyi', { probabilities, alpha, base }),
  tsallis: (probabilities: number[], q: number) =>
    apiCall('/quantlib/physics/entropy/tsallis', { probabilities, q }),
  cross: (p: number[], q: number[], base?: number) =>
    apiCall('/quantlib/physics/entropy/cross', { p, q, base }),
  joint: (joint_prob: number[][], base?: number) =>
    apiCall('/quantlib/physics/entropy/joint', { joint_prob, base }),
  conditional: (joint_prob: number[][], base?: number) =>
    apiCall('/quantlib/physics/entropy/conditional', { joint_prob, base }),
  mutualInformation: (joint_prob: number[][], base?: number) =>
    apiCall('/quantlib/physics/entropy/mutual-information', { joint_prob, base }),
  transfer: (x_history: number[], y_history: number[], lag?: number, n_bins?: number) =>
    apiCall('/quantlib/physics/entropy/transfer', { x_history, y_history, lag, n_bins }),
  markovRate: (transition_matrix: number[][], stationary_dist?: number[], base?: number) =>
    apiCall('/quantlib/physics/entropy/markov-rate', { transition_matrix, stationary_dist, base }),
  differential: (distribution?: string, sigma?: number, rate?: number) =>
    apiCall('/quantlib/physics/entropy/differential', { distribution, sigma, rate }),
  fisherInformation: (distribution?: string, theta?: number, h?: number, n_samples?: number, seed?: number) =>
    apiCall('/quantlib/physics/entropy/fisher-information', { distribution, theta, h, n_samples, seed }),
};

// === DIVERGENCE (2) ===
export const divergenceApi = {
  kl: (p: number[], q: number[], base?: number) =>
    apiCall('/quantlib/physics/divergence/kl', { p, q, base }),
  js: (p: number[], q: number[], base?: number) =>
    apiCall('/quantlib/physics/divergence/js', { p, q, base }),
};

// === THERMODYNAMICS (7) ===
export const thermoApi = {
  freeEnergy: (internal_energy: number, temperature: number, entropy: number, pressure?: number, volume?: number, type?: string) =>
    apiCall('/quantlib/physics/thermodynamics/free-energy', { internal_energy, pressure, volume, temperature, entropy, type }),
  carnot: (T_hot: number, T_cold: number, Q_hot?: number) =>
    apiCall('/quantlib/physics/thermodynamics/carnot', { T_hot, T_cold, Q_hot }),
  vanDerWaals: (a: number, b: number, temperature: number, volume: number, n?: number) =>
    apiCall('/quantlib/physics/thermodynamics/van-der-waals', { a, b, n, temperature, volume }),
  maxwellRelations: () =>
    apiCall('/quantlib/physics/thermodynamics/maxwell-relations', {}),
  idealGas: (n?: number, temperature?: number, volume?: number, pressure?: number) =>
    apiCall('/quantlib/physics/thermodynamics/ideal-gas', { n, temperature, volume, pressure }),
  clausiusClapeyron: (delta_H: number, T: number, P0?: number, T0?: number, R?: number) =>
    apiCall('/quantlib/physics/thermodynamics/clausius-clapeyron', { delta_H, T, P0, T0, R }),
  jouleThomson: (Cp?: number, V?: number, T?: number, dV_dT_P?: number) =>
    apiCall('/quantlib/physics/thermodynamics/joule-thomson', { Cp, V, T, dV_dT_P }),
};

// === ISING (2) ===
export const isingApi = {
  simulate: (size?: number, J?: number, h?: number, temperature?: number, n_steps?: number, seed?: number) =>
    apiCall('/quantlib/physics/ising', { size, J, h, temperature, n_steps, seed }),
  criticalTemperature: (J?: number) =>
    apiCall('/quantlib/physics/ising/critical-temperature', { J }),
};

// === BOLTZMANN (1) ===
export const boltzmannApi = {
  distribution: (energies: number[], temperature: number) =>
    apiCall('/quantlib/physics/boltzmann', { energies, temperature }),
};

// === MAX ENTROPY (1) ===
export const maxEntropyApi = {
  distribution: (n_states: number, constraint_values: number[]) =>
    apiCall('/quantlib/physics/max-entropy', { n_states, constraint_values }),
};
