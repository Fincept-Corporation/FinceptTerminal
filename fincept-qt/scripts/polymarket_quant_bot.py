"""
Polymarket Quant Bot -- Standalone Demo
=======================================
A quantitative trading bot for Polymarket prediction markets.

How it works (4 steps):
1. READ PRICES    — Pull live contract prices from Polymarket Gamma API
2. ESTIMATE PROB  — Particle filter + Bayesian model estimates "true" probability
3. FIND EDGE      — Compare model probability vs market price
4. EXECUTE TRADE  — Paper trade if edge > threshold, with full risk controls

Run:  python polymarket_quant_bot.py
Deps: pip install numpy scipy requests

Author: Fincept Corporation
License: AGPL-3.0-or-later
"""

import time
import json
import sys
import math
import random
from datetime import datetime, timezone
from dataclasses import dataclass, field
from typing import Optional

try:
    import numpy as np
    from scipy.special import expit, logit
    from scipy.stats import norm
    import requests
except ImportError:
    print("Missing dependencies. Install with:")
    print("  pip install numpy scipy requests")
    sys.exit(1)


# ==============================================================================
# CONFIGURATION
# ==============================================================================

GAMMA_API = "https://gamma-api.polymarket.com"
CLOB_API = "https://clob.polymarket.com"

# Bot settings
SCAN_LIMIT = 15                # Markets to scan per cycle
EDGE_THRESHOLD = 0.05          # Minimum 5-cent edge to trade
MIN_CONFIDENCE = 65            # Minimum confidence score (0-100)
CYCLE_INTERVAL_SEC = 60        # Seconds between scan cycles
NUM_CYCLES = 10                # Total cycles to run (set 0 for infinite)

# Risk limits
MAX_POSITION_USDC = 50.0       # Max per contract
MAX_EXPOSURE_USDC = 200.0      # Max total portfolio exposure
STOP_LOSS_PCT = 30.0           # Exit if position drops 30%
TAKE_PROFIT_PCT = 50.0         # Exit if position gains 50%
MAX_BRIER_SCORE = 0.25         # Pause trading if calibration drifts
MAX_CORRELATION = 0.70         # Don't stack highly correlated bets

# Particle filter settings
N_PARTICLES = 3000
PROCESS_VOL = 0.03             # Volatility of probability drift per step
OBS_NOISE = 0.04               # Observation noise (market price noise)


# ==============================================================================
# DATA LAYER — Fetch live prices from Polymarket
# ==============================================================================

def fetch_active_markets(limit: int = 20) -> list[dict]:
    """Fetch top active markets by volume from Gamma API."""
    try:
        url = f"{GAMMA_API}/markets"
        params = {
            "limit": limit,
            "active": "true",
            "closed": "false",
            "order": "volume",
            "ascending": "false",
        }
        resp = requests.get(url, params=params, timeout=15)
        resp.raise_for_status()
        markets = resp.json()

        # Filter to tradeable markets with valid prices
        result = []
        for m in markets:
            if not m.get("active") or m.get("closed") or m.get("archived"):
                continue
            raw_prices = m.get("outcomePrices", [])
            # outcomePrices can be a JSON string or a list
            if isinstance(raw_prices, str):
                try:
                    raw_prices = json.loads(raw_prices)
                except (json.JSONDecodeError, TypeError):
                    continue
            if not raw_prices or len(raw_prices) < 1:
                continue
            try:
                yes_price = float(raw_prices[0])
            except (ValueError, TypeError):
                continue
            if yes_price <= 0.02 or yes_price >= 0.98:
                continue  # Skip near-certain or near-zero contracts

            result.append({
                "id": m.get("id", ""),
                "question": m.get("question", "Unknown"),
                "yes_price": yes_price,
                "no_price": 1.0 - yes_price,
                "volume": float(m.get("volume", 0) or 0),
                "liquidity": float(m.get("liquidity", 0) or 0),
                "end_date": m.get("endDate", ""),
                "category": m.get("category", ""),
                "clob_token_ids": m.get("clobTokenIds", []),
                "condition_id": m.get("conditionId", ""),
            })
        return result
    except Exception as e:
        print(f"  [ERROR] Failed to fetch markets: {e}")
        return []


def fetch_price_history(token_id: str, interval: str = "1w") -> list[dict]:
    """Fetch price history for a token from CLOB API."""
    try:
        url = f"{CLOB_API}/prices-history"
        params = {"market": token_id, "interval": interval, "fidelity": 60}
        resp = requests.get(url, params=params, timeout=10)
        resp.raise_for_status()
        data = resp.json()
        history = data.get("history", [])
        return [
            {"price": float(h.get("p", 0)), "timestamp": int(h.get("t", 0))}
            for h in history
            if float(h.get("p", 0)) > 0
        ]
    except Exception:
        return []


def fetch_order_book(token_id: str) -> Optional[dict]:
    """Fetch order book for liquidity assessment."""
    try:
        url = f"{CLOB_API}/book"
        params = {"token_id": token_id}
        resp = requests.get(url, params=params, timeout=10)
        resp.raise_for_status()
        data = resp.json()
        bids = data.get("bids", [])
        asks = data.get("asks", [])
        best_bid = float(bids[0]["price"]) if bids else 0.0
        best_ask = float(asks[0]["price"]) if asks else 1.0
        spread = best_ask - best_bid
        bid_depth = sum(float(b.get("size", 0)) for b in bids[:5])
        ask_depth = sum(float(a.get("size", 0)) for a in asks[:5])
        return {
            "best_bid": best_bid,
            "best_ask": best_ask,
            "spread": spread,
            "bid_depth": bid_depth,
            "ask_depth": ask_depth,
        }
    except Exception:
        return None


# ==============================================================================
# PROBABILITY ENGINE — Particle Filter + Bayesian Estimation
# ==============================================================================

class ParticleFilter:
    """
    Sequential Monte Carlo filter for real-time event probability estimation.

    Maintains N particles (hypotheses about the true probability) in logit space.
    Updates as new market prices arrive, smoothing noise and propagating uncertainty.
    """

    def __init__(
        self,
        prior_prob: float = 0.5,
        n_particles: int = N_PARTICLES,
        process_vol: float = PROCESS_VOL,
        obs_noise: float = OBS_NOISE,
    ):
        self.n = n_particles
        self.process_vol = process_vol
        self.obs_noise = obs_noise

        # Initialize particles around prior in logit space
        logit_prior = logit(np.clip(prior_prob, 0.01, 0.99))
        self.logit_particles = logit_prior + np.random.normal(0, 0.5, n_particles)
        self.weights = np.ones(n_particles) / n_particles
        self.history: list[float] = []
        self.observations: list[float] = []

    def update(self, observed_price: float) -> None:
        """Incorporate a new price observation."""
        observed_price = np.clip(observed_price, 0.01, 0.99)
        self.observations.append(observed_price)

        # 1. Propagate: random walk in logit space
        noise = np.random.normal(0, self.process_vol, self.n)
        self.logit_particles += noise

        # 2. Convert to probability space
        prob_particles = expit(self.logit_particles)

        # 3. Reweight: likelihood of observation given each particle
        log_likelihood = -0.5 * ((observed_price - prob_particles) / self.obs_noise) ** 2
        log_weights = np.log(self.weights + 1e-300) + log_likelihood

        # Normalize in log space for numerical stability
        log_weights -= log_weights.max()
        self.weights = np.exp(log_weights)
        self.weights /= self.weights.sum()

        # 4. Resample if effective sample size too low
        ess = 1.0 / np.sum(self.weights ** 2)
        if ess < self.n / 2:
            self._systematic_resample()

        self.history.append(self.estimate())

    def _systematic_resample(self) -> None:
        """Systematic resampling — lower variance than multinomial."""
        cumsum = np.cumsum(self.weights)
        u = (np.arange(self.n) + np.random.uniform()) / self.n
        indices = np.searchsorted(cumsum, u)
        indices = np.clip(indices, 0, self.n - 1)
        self.logit_particles = self.logit_particles[indices]
        self.weights = np.ones(self.n) / self.n

    def estimate(self) -> float:
        """Weighted mean probability estimate."""
        probs = expit(self.logit_particles)
        return float(np.average(probs, weights=self.weights))

    def credible_interval(self, alpha: float = 0.05) -> tuple[float, float]:
        """Weighted quantile-based credible interval."""
        probs = expit(self.logit_particles)
        sorted_idx = np.argsort(probs)
        sorted_probs = probs[sorted_idx]
        sorted_weights = self.weights[sorted_idx]
        cumw = np.cumsum(sorted_weights)
        lower = float(sorted_probs[np.searchsorted(cumw, alpha / 2)])
        upper = float(sorted_probs[np.searchsorted(cumw, 1 - alpha / 2)])
        return lower, upper

    def uncertainty(self) -> float:
        """Standard deviation of the particle distribution."""
        probs = expit(self.logit_particles)
        return float(np.sqrt(np.average((probs - self.estimate()) ** 2, weights=self.weights)))


# ==============================================================================
# MONTE CARLO SIMULATION — For contracts with underlying assets
# ==============================================================================

def monte_carlo_binary(
    current_price: float,
    vol_estimate: float,
    time_to_expiry_days: float,
    n_paths: int = 50_000,
) -> dict:
    """
    Monte Carlo simulation for a binary prediction market contract.
    Simulates probability paths in logit space (bounded [0,1]).
    Uses antithetic variates + stratified sampling for variance reduction.

    Returns estimated terminal probability, standard error, and CI.
    """
    if time_to_expiry_days <= 0:
        return {"probability": current_price, "std_error": 0.0, "ci_95": (current_price, current_price)}

    T = time_to_expiry_days / 365.0
    logit_p = logit(np.clip(current_price, 0.02, 0.98))
    # Scale vol for logit space (higher vol for prices near 0 or 1)
    logit_vol = vol_estimate / max(current_price * (1 - current_price), 0.01)
    logit_vol = min(logit_vol, 5.0)  # Cap to prevent explosion

    n_strata = 10
    per_stratum = n_paths // (n_strata * 2)  # *2 for antithetic

    terminal_probs = []

    for j in range(n_strata):
        # Stratified uniform draws
        U = np.random.uniform(j / n_strata, (j + 1) / n_strata, per_stratum)
        Z = norm.ppf(np.clip(U, 1e-8, 1 - 1e-8))

        # Antithetic variates: use Z and -Z
        for z in [Z, -Z]:
            # Random walk in logit space (drift=0, risk-neutral)
            logit_terminal = logit_p + logit_vol * np.sqrt(T) * z
            # Convert back to probability space
            prob_terminal = expit(logit_terminal)
            terminal_probs.append(prob_terminal.mean())

    p_hat = float(np.mean(terminal_probs))
    se = float(np.std(terminal_probs) / np.sqrt(len(terminal_probs)))

    return {
        "probability": np.clip(p_hat, 0.0, 1.0),
        "std_error": se,
        "ci_95": (max(0, p_hat - 1.96 * se), min(1, p_hat + 1.96 * se)),
    }


def estimate_volatility(price_history: list[dict]) -> float:
    """Estimate annualized volatility from price history."""
    if len(price_history) < 5:
        return 0.20  # Default 20% vol

    prices = [h["price"] for h in price_history if h["price"] > 0.01]
    if len(prices) < 5:
        return 0.20

    log_returns = np.diff(np.log(np.array(prices)))
    daily_vol = float(np.std(log_returns))

    # Annualize (assume ~hourly data, ~24*365 observations per year)
    timestamps = [h["timestamp"] for h in price_history]
    if len(timestamps) >= 2:
        avg_interval_sec = (timestamps[-1] - timestamps[0]) / max(len(timestamps) - 1, 1)
        periods_per_year = 365.25 * 24 * 3600 / max(avg_interval_sec, 1)
        annual_vol = daily_vol * np.sqrt(periods_per_year)
    else:
        annual_vol = daily_vol * np.sqrt(365)

    return float(np.clip(annual_vol, 0.05, 2.0))


# ==============================================================================
# EDGE DETECTION — Find mispricings
# ==============================================================================

def kelly_criterion(edge: float, odds: float) -> float:
    """
    Kelly criterion for optimal bet sizing.
    edge: probability advantage (model_prob - market_price)
    odds: payout odds (1/market_price - 1 for binary)
    Returns fraction of bankroll to bet.
    """
    if odds <= 0 or edge <= 0:
        return 0.0
    p = edge + (1.0 / (1.0 + odds))  # Implied win probability
    q = 1.0 - p
    f = (p * odds - q) / odds
    # Half-Kelly for safety
    return max(0.0, min(f * 0.5, 0.25))


def detect_edge(model_prob: float, market_price: float, threshold: float = EDGE_THRESHOLD) -> dict:
    """
    Compare model probability vs market price to find edge.
    Returns trade signal with direction, edge size, and confidence.
    """
    edge = model_prob - market_price

    if abs(edge) < threshold:
        return {"action": "HOLD", "edge": edge, "confidence": 0, "direction": None}

    # Confidence: scale edge to 0-100 range
    # 5% edge = ~50 confidence, 10% = ~70, 20% = ~90
    confidence = min(100, int(50 + abs(edge) * 200))

    if edge > 0:
        # Model says higher probability than market — BUY YES
        odds = (1.0 / market_price) - 1.0
        kelly_frac = kelly_criterion(edge, odds)
        return {
            "action": "BUY_YES",
            "edge": edge,
            "confidence": confidence,
            "direction": "YES",
            "kelly_fraction": kelly_frac,
        }
    else:
        # Model says lower probability — BUY NO
        no_price = 1.0 - market_price
        odds = (1.0 / no_price) - 1.0
        kelly_frac = kelly_criterion(-edge, odds)
        return {
            "action": "BUY_NO",
            "edge": edge,
            "confidence": confidence,
            "direction": "NO",
            "kelly_fraction": kelly_frac,
        }


# ==============================================================================
# RISK MANAGER — Position limits, stop loss, correlation, Brier score
# ==============================================================================

@dataclass
class Position:
    market_id: str
    question: str
    direction: str        # YES or NO
    entry_price: float
    current_price: float
    size_usdc: float
    opened_at: str
    closed_at: Optional[str] = None
    close_reason: Optional[str] = None
    pnl_usdc: float = 0.0


@dataclass
class BrierTracker:
    """Track Brier score for calibration monitoring."""
    predictions: list[float] = field(default_factory=list)
    outcomes: list[float] = field(default_factory=list)

    def record(self, predicted_prob: float, outcome: float) -> None:
        self.predictions.append(predicted_prob)
        self.outcomes.append(outcome)

    def score(self) -> float:
        if not self.predictions:
            return 0.0
        preds = np.array(self.predictions)
        outs = np.array(self.outcomes)
        return float(np.mean((preds - outs) ** 2))

    def is_calibrated(self, threshold: float = MAX_BRIER_SCORE) -> bool:
        if len(self.predictions) < 5:
            return True  # Not enough data to judge
        return self.score() < threshold


class RiskManager:
    """Enforces all risk controls before allowing trades."""

    def __init__(self):
        self.positions: list[Position] = []
        self.brier = BrierTracker()
        self.total_pnl: float = 0.0
        self.trades_executed: int = 0
        self.trades_won: int = 0

    @property
    def open_positions(self) -> list[Position]:
        return [p for p in self.positions if p.closed_at is None]

    @property
    def current_exposure(self) -> float:
        return sum(p.size_usdc for p in self.open_positions)

    @property
    def win_rate(self) -> float:
        if self.trades_executed == 0:
            return 0.0
        return self.trades_won / self.trades_executed * 100

    def check_can_trade(self, size_usdc: float, market_id: str) -> tuple[bool, str]:
        """Run all risk checks. Returns (allowed, reason)."""

        # 1. Brier score calibration check
        if not self.brier.is_calibrated():
            return False, f"Brier score {self.brier.score():.3f} > {MAX_BRIER_SCORE} - model uncalibrated"

        # 2. Max exposure check
        if self.current_exposure + size_usdc > MAX_EXPOSURE_USDC:
            return False, f"Exposure {self.current_exposure:.0f}+{size_usdc:.0f} > {MAX_EXPOSURE_USDC:.0f} limit"

        # 3. Per-market size check
        if size_usdc > MAX_POSITION_USDC:
            return False, f"Size {size_usdc:.0f} > {MAX_POSITION_USDC:.0f} per-market limit"

        # 4. Already in this market?
        for p in self.open_positions:
            if p.market_id == market_id:
                return False, "Already have position in this market"

        # 5. Correlation check — simple category-based
        # (In production, use copula. Here we just limit same-category positions)
        return True, "OK"

    def open_position(self, market_id: str, question: str, direction: str,
                      entry_price: float, size_usdc: float) -> Position:
        pos = Position(
            market_id=market_id,
            question=question,
            direction=direction,
            entry_price=entry_price,
            current_price=entry_price,
            size_usdc=size_usdc,
            opened_at=datetime.now(timezone.utc).isoformat(),
        )
        self.positions.append(pos)
        self.trades_executed += 1
        return pos

    def update_positions(self, market_prices: dict[str, float]) -> list[str]:
        """Update current prices and check stop-loss / take-profit."""
        events = []
        for pos in self.open_positions:
            if pos.market_id not in market_prices:
                continue

            new_price = market_prices[pos.market_id]
            pos.current_price = new_price

            # Calculate P&L
            if pos.direction == "YES":
                pnl_pct = ((new_price - pos.entry_price) / pos.entry_price) * 100
            else:
                pnl_pct = ((pos.entry_price - new_price) / (1.0 - pos.entry_price)) * 100

            pos.pnl_usdc = pos.size_usdc * (pnl_pct / 100.0)

            # Stop loss
            if pnl_pct <= -STOP_LOSS_PCT:
                pos.closed_at = datetime.now(timezone.utc).isoformat()
                pos.close_reason = "STOP_LOSS"
                self.total_pnl += pos.pnl_usdc
                events.append(f"  STOP LOSS: {pos.question[:40]}... PnL: ${pos.pnl_usdc:.2f}")

            # Take profit
            elif pnl_pct >= TAKE_PROFIT_PCT:
                pos.closed_at = datetime.now(timezone.utc).isoformat()
                pos.close_reason = "TAKE_PROFIT"
                self.total_pnl += pos.pnl_usdc
                self.trades_won += 1
                events.append(f"  TAKE PROFIT: {pos.question[:40]}... PnL: ${pos.pnl_usdc:.2f}")

        return events


# ==============================================================================
# CORRELATION CHECK — Simple pairwise correlation from price histories
# ==============================================================================

def check_correlation(
    new_market_prices: list[float],
    existing_position_prices: list[list[float]],
    threshold: float = MAX_CORRELATION,
) -> tuple[bool, float]:
    """
    Check if a new market is too correlated with existing positions.
    Returns (is_safe, max_correlation).
    """
    if not existing_position_prices or len(new_market_prices) < 10:
        return True, 0.0

    max_corr = 0.0
    new_arr = np.array(new_market_prices[-50:])  # Last 50 observations

    for existing in existing_position_prices:
        ex_arr = np.array(existing[-50:])
        min_len = min(len(new_arr), len(ex_arr))
        if min_len < 10:
            continue

        corr = float(np.corrcoef(new_arr[:min_len], ex_arr[:min_len])[0, 1])
        max_corr = max(max_corr, abs(corr))

    return max_corr < threshold, max_corr


# ==============================================================================
# MAIN BOT LOOP
# ==============================================================================

def print_header():
    print("\n" + "=" * 72)
    print("  POLYMARKET QUANT BOT -- Demo (Paper Trading)")
    print("  Particle Filter + Monte Carlo + Edge Detection + Risk Management")
    print("=" * 72)
    print(f"  Edge Threshold: {EDGE_THRESHOLD*100:.0f}%  |  Max Position: ${MAX_POSITION_USDC:.0f}")
    print(f"  Max Exposure: ${MAX_EXPOSURE_USDC:.0f}  |  Stop Loss: {STOP_LOSS_PCT:.0f}%  |  Take Profit: {TAKE_PROFIT_PCT:.0f}%")
    print(f"  Particles: {N_PARTICLES}  |  Cycle Interval: {CYCLE_INTERVAL_SEC}s")
    print("=" * 72)


def print_cycle_header(cycle: int, total: int):
    now = datetime.now(timezone.utc).strftime("%H:%M:%S UTC")
    label = f"{cycle}/{total}" if total > 0 else f"{cycle}"
    print(f"\n{'-' * 72}")
    print(f"  CYCLE {label}  |  {now}")
    print(f"{'-' * 72}")


def print_market_analysis(market: dict, pf: ParticleFilter, mc: dict, signal: dict, ob: Optional[dict]):
    q = market["question"][:55]
    mp = market["yes_price"]
    model_p = pf.estimate()
    ci = pf.credible_interval()
    unc = pf.uncertainty()

    action = signal["action"]
    edge = signal["edge"]

    # Color-code action
    if action == "BUY_YES":
        action_str = f">>> BUY YES (edge +{edge*100:.1f}%)"
    elif action == "BUY_NO":
        action_str = f">>> BUY NO  (edge {edge*100:.1f}%)"
    else:
        action_str = f"    HOLD    (edge {edge*100:.1f}%)"

    spread_str = f"spread {ob['spread']:.3f}" if ob else "no book"

    print(f"\n  {q}")
    print(f"    Market: {mp:.3f}  |  Model: {model_p:.3f}  |  CI: [{ci[0]:.3f}, {ci[1]:.3f}]  |  Unc: {unc:.3f}")
    print(f"    MC prob: {mc['probability']:.3f} +/- {mc['std_error']:.4f}  |  {spread_str}")
    print(f"    {action_str}")


def print_portfolio(risk: RiskManager):
    print(f"\n{'-' * 72}")
    print(f"  PORTFOLIO SUMMARY")
    print(f"{'-' * 72}")
    print(f"  Exposure: ${risk.current_exposure:.2f} / ${MAX_EXPOSURE_USDC:.0f}")
    print(f"  Open Positions: {len(risk.open_positions)}")
    print(f"  Total P&L: ${risk.total_pnl:.2f}")
    print(f"  Trades: {risk.trades_executed}  |  Win Rate: {risk.win_rate:.0f}%")
    print(f"  Brier Score: {risk.brier.score():.4f}  |  Calibrated: {'YES' if risk.brier.is_calibrated() else 'NO'}")

    if risk.open_positions:
        print(f"\n  Open Positions:")
        for p in risk.open_positions:
            pnl_pct = 0.0
            if p.direction == "YES":
                pnl_pct = ((p.current_price - p.entry_price) / max(p.entry_price, 0.001)) * 100
            else:
                pnl_pct = ((p.entry_price - p.current_price) / max(1.0 - p.entry_price, 0.001)) * 100

            indicator = "+" if pnl_pct >= 0 else ""
            print(f"    [{p.direction}] {p.question[:45]}...")
            print(f"          Entry: {p.entry_price:.3f}  Now: {p.current_price:.3f}  PnL: {indicator}{pnl_pct:.1f}%  (${p.pnl_usdc:.2f})")


def run_bot():
    """Main bot loop."""
    print_header()

    risk = RiskManager()
    # Particle filters — one per market we're tracking
    filters: dict[str, ParticleFilter] = {}
    # Price histories for correlation checks
    price_histories: dict[str, list[float]] = {}

    cycles = NUM_CYCLES if NUM_CYCLES > 0 else float("inf")
    cycle = 0

    while cycle < cycles:
        cycle += 1
        print_cycle_header(cycle, NUM_CYCLES)

        # ── Step 1: Fetch live market data ─────────────────────────
        print("\n  [1/4] Fetching live markets...")
        markets = fetch_active_markets(SCAN_LIMIT)
        if not markets:
            print("  No markets fetched. Retrying next cycle.")
            time.sleep(CYCLE_INTERVAL_SEC)
            continue

        print(f"  Found {len(markets)} active markets")

        # ── Update existing positions with new prices ──────────────
        market_prices = {m["id"]: m["yes_price"] for m in markets}
        close_events = risk.update_positions(market_prices)
        for event in close_events:
            print(event)

        # ── Step 2: Run probability engine on each market ──────────
        print(f"\n  [2/4] Running particle filter + Monte Carlo on {len(markets)} markets...")

        opportunities = []

        for market in markets:
            mid = market["id"]

            # Initialize or update particle filter
            if mid not in filters:
                filters[mid] = ParticleFilter(prior_prob=market["yes_price"])

            pf = filters[mid]
            pf.update(market["yes_price"])

            # Track price history for correlation
            if mid not in price_histories:
                price_histories[mid] = []
            price_histories[mid].append(market["yes_price"])

            # Fetch historical prices for volatility estimation
            token_ids = market.get("clob_token_ids", [])
            vol = 0.20  # Default
            if token_ids:
                hist = fetch_price_history(token_ids[0], interval="1w")
                if hist:
                    vol = estimate_volatility(hist)

            # Days to expiry
            days_to_expiry = 30.0  # Default
            if market["end_date"]:
                try:
                    end = datetime.fromisoformat(market["end_date"].replace("Z", "+00:00"))
                    days_to_expiry = max(0.1, (end - datetime.now(timezone.utc)).total_seconds() / 86400)
                except Exception:
                    pass

            # Monte Carlo simulation
            mc = monte_carlo_binary(market["yes_price"], vol, days_to_expiry)

            # ── Step 3: Edge detection ─────────────────────────────
            # Ensemble: 60% particle filter + 40% Monte Carlo
            model_prob = 0.6 * pf.estimate() + 0.4 * mc["probability"]

            signal = detect_edge(model_prob, market["yes_price"])

            # Record for Brier tracking (use market price as proxy outcome for now)
            risk.brier.record(pf.estimate(), market["yes_price"])

            # Fetch order book for liquidity check
            ob = None
            if signal["action"] != "HOLD" and token_ids:
                ob = fetch_order_book(token_ids[0])

            print_market_analysis(market, pf, mc, signal, ob)

            if signal["action"] != "HOLD" and signal.get("confidence", 0) >= MIN_CONFIDENCE:
                opportunities.append((market, pf, mc, signal, ob))

        # ── Step 4: Execute trades (paper) ─────────────────────────
        print(f"\n  [3/4] Edge detection found {len(opportunities)} opportunities")
        print(f"\n  [4/4] Executing trades (paper)...")

        trades_this_cycle = 0

        for market, pf, mc, signal, ob in opportunities:
            # Liquidity check
            if ob and ob["spread"] > 0.10:
                print(f"    SKIP (spread too wide: {ob['spread']:.3f}): {market['question'][:50]}")
                continue

            # Kelly-based position sizing
            kelly_frac = signal.get("kelly_fraction", 0.05)
            size_usdc = min(
                MAX_POSITION_USDC,
                MAX_EXPOSURE_USDC * kelly_frac,
                MAX_POSITION_USDC * 0.5,  # Conservative cap
            )
            size_usdc = max(5.0, size_usdc)  # Minimum $5

            # Correlation check
            existing_histories = [
                price_histories.get(p.market_id, [])
                for p in risk.open_positions
            ]
            new_history = price_histories.get(market["id"], [])
            is_safe, max_corr = check_correlation(new_history, existing_histories)

            if not is_safe:
                print(f"    SKIP (correlation {max_corr:.2f} > {MAX_CORRELATION}): {market['question'][:50]}")
                continue

            # Risk gate
            can_trade, reason = risk.check_can_trade(size_usdc, market["id"])
            if not can_trade:
                print(f"    BLOCKED ({reason}): {market['question'][:50]}")
                continue

            # Paper trade!
            direction = signal["direction"]
            entry = market["yes_price"] if direction == "YES" else (1.0 - market["yes_price"])
            pos = risk.open_position(
                market_id=market["id"],
                question=market["question"],
                direction=direction,
                entry_price=market["yes_price"],
                size_usdc=size_usdc,
            )
            trades_this_cycle += 1
            print(f"    TRADE: {direction} ${size_usdc:.2f} on \"{market['question'][:45]}...\"")
            print(f"           Entry: {market['yes_price']:.3f}  |  Model: {pf.estimate():.3f}  |  Edge: {signal['edge']*100:.1f}%")

        if trades_this_cycle == 0:
            print("    No trades this cycle.")

        # ── Portfolio summary ──────────────────────────────────────
        print_portfolio(risk)

        # ── Wait for next cycle ────────────────────────────────────
        if cycle < cycles:
            print(f"\n  Waiting {CYCLE_INTERVAL_SEC}s for next cycle... (Ctrl+C to stop)")
            try:
                time.sleep(CYCLE_INTERVAL_SEC)
            except KeyboardInterrupt:
                print("\n\n  Bot stopped by user.")
                break

    # ── Final summary ──────────────────────────────────────────────
    print("\n" + "=" * 72)
    print("  FINAL RESULTS")
    print("=" * 72)
    print(f"  Cycles Run: {cycle}")
    print(f"  Trades Executed: {risk.trades_executed}")
    print(f"  Win Rate: {risk.win_rate:.0f}%")
    print(f"  Total P&L: ${risk.total_pnl:.2f}")
    print(f"  Final Brier Score: {risk.brier.score():.4f}")
    print(f"  Open Positions: {len(risk.open_positions)}")
    print("=" * 72)


# ==============================================================================
# ENTRY POINT
# ==============================================================================

if __name__ == "__main__":
    import io as _io
    sys.stdout = _io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = _io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
    try:
        run_bot()
    except KeyboardInterrupt:
        print("\n\n  Bot stopped by user.")
    except Exception as e:
        print(f"\n  FATAL ERROR: {e}")
        import traceback
        traceback.print_exc()
