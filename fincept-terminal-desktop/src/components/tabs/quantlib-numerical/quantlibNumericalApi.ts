// File: src/components/tabs/quantlib-numerical/quantlibNumericalApi.ts
// 28 endpoints — differentiation, FFT, integration, interpolation, linalg, ODE, roots, optimize, least squares

const BASE_URL = 'https://api.fincept.in';
let _apiKey: string | null = null;
export function setNumericalApiKey(key: string | null) { _apiKey = key; }

async function apiCall<T = any>(path: string, body?: any): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...(_apiKey ? { 'X-API-Key': _apiKey } : {}) },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

// ── Differentiation (3) ─────────────────────────────────────────
export const diffApi = {
  derivative: (func_name: string, x: number, h = 1e-7, method = 'central') =>
    apiCall('/quantlib/numerical/differentiation/derivative', { func_name, x, h, method }),
  gradient: (func_name: string, x: number[], h = 1e-7, method = 'forward') =>
    apiCall('/quantlib/numerical/differentiation/gradient', { func_name, x, h, method }),
  hessian: (func_name: string, x: number[], h = 1e-5) =>
    apiCall('/quantlib/numerical/differentiation/hessian', { func_name, x, h }),
};

// ── FFT (3) ─────────────────────────────────────────────────────
export const fftApi = {
  forward: (data: number[]) =>
    apiCall('/quantlib/numerical/fft/forward', { data }),
  inverse: (data: number[]) =>
    apiCall('/quantlib/numerical/fft/inverse', { data }),
  convolve: (a: number[], b: number[]) =>
    apiCall('/quantlib/numerical/fft/convolve', { a, b }),
};

// ── Integration (3) ─────────────────────────────────────────────
export const integrationApi = {
  quadrature: (func_name: string, a: number, b: number, method = 'simpson', n = 100) =>
    apiCall('/quantlib/numerical/integration/quadrature', { func_name, a, b, method, n }),
  monteCarlo: (func_name: string, bounds: number[][], n_samples = 10000, seed = 42) =>
    apiCall('/quantlib/numerical/integration/monte-carlo', { func_name, bounds, n_samples, seed }),
  stratified: (func_name: string, a: number, b: number, n_strata = 10, samples_per_stratum = 100, seed = 42) =>
    apiCall('/quantlib/numerical/integration/stratified', { func_name, a, b, n_strata, samples_per_stratum, seed }),
};

// ── Interpolation (3) ───────────────────────────────────────────
export const interpApi = {
  evaluate: (x_data: number[], y_data: number[], xi: number, method = 'linear') =>
    apiCall('/quantlib/numerical/interpolation/evaluate', { x_data, y_data, xi, method }),
  splineCurve: (x_data: number[], y_data: number[], xi: number[], bc_type = 'natural') =>
    apiCall('/quantlib/numerical/interpolation/spline-curve', { x_data, y_data, xi, bc_type }),
  splineDerivative: (x_data: number[], y_data: number[], xi: number, bc_type = 'natural') =>
    apiCall('/quantlib/numerical/interpolation/spline-derivative', { x_data, y_data, xi, bc_type }),
};

// ── Linear Algebra (10) ─────────────────────────────────────────
export const linalgApi = {
  matmul: (A: number[][], B: number[][]) =>
    apiCall('/quantlib/numerical/linalg/matmul', { A, B }),
  matvec: (A: number[][], x: number[]) =>
    apiCall('/quantlib/numerical/linalg/matvec', { A, x }),
  dot: (a: number[], b: number[]) =>
    apiCall('/quantlib/numerical/linalg/dot', { a, b }),
  norm: (v: number[], p = 2) =>
    apiCall('/quantlib/numerical/linalg/norm', { v, p }),
  outer: (a: number[], b: number[]) =>
    apiCall('/quantlib/numerical/linalg/outer', { a, b }),
  transpose: (A: number[][]) =>
    apiCall('/quantlib/numerical/linalg/transpose', { A }),
  decompose: (A: number[][], method = 'lu') =>
    apiCall('/quantlib/numerical/linalg/decompose', { A, method }),
  solve: (A: number[][], b: number[]) =>
    apiCall('/quantlib/numerical/linalg/solve', { A, b }),
  lstsq: (A: number[][], b: number[]) =>
    apiCall('/quantlib/numerical/linalg/lstsq', { A, b }),
  inverse: (A: number[][]) =>
    apiCall('/quantlib/numerical/linalg/inverse', { A }),
};

// ── ODE (1) ─────────────────────────────────────────────────────
export const odeApi = {
  solve: (system: string, t_span: number[], y0: number[], method = 'rk45', h = 0.01, params?: any) =>
    apiCall('/quantlib/numerical/ode/solve', { system, t_span, y0, method, h, params }),
};

// ── Roots (3) ───────────────────────────────────────────────────
export const rootsApi = {
  find1d: (func_name: string, a: number, b: number, method = 'brent', tol = 1e-12, poly_coeffs?: number[]) =>
    apiCall('/quantlib/numerical/roots/find-1d', { func_name, a, b, method, tol, poly_coeffs }),
  newton: (func_name: string, x0: number, tol = 1e-12, poly_coeffs?: number[]) =>
    apiCall('/quantlib/numerical/roots/newton', { func_name, x0, tol, poly_coeffs }),
  findNd: (system: string, x0: number[], method = 'newton', tol = 1e-10, params?: any) =>
    apiCall('/quantlib/numerical/roots/find-nd', { system, x0, method, tol, params }),
};

// ── Optimization (1) ────────────────────────────────────────────
export const optimizeApi = {
  minimize: (func_name: string, x0: number[], method = 'bfgs', tol = 1e-8) =>
    apiCall('/quantlib/numerical/optimize/minimize', { func_name, x0, method, tol }),
};

// ── Least Squares (1) ───────────────────────────────────────────
export const leastSquaresApi = {
  fit: (model: string, x_data: number[], y_data: number[], x0: number[]) =>
    apiCall('/quantlib/numerical/least-squares/fit', { model, x_data, y_data, x0 }),
};
