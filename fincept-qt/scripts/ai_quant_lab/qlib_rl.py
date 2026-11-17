"""
AI Quant Lab - Reinforcement Learning Module
Qlib RL integration for portfolio management and trading strategy optimization
Supports PPO, DQN, A2C, SAC and TD3 (Stable-Baselines3) for automated trading

CLI -- every command prints one {"event": "result", "success": ..., ...} line; `train`
streams {"event": "progress" | "log", ...} lines before it:
  train '{"ticker": "AAPL", "algorithm": "PPO", "episodes": 10, "start_date": "2024-01-01", "end_date": "2025-12-31"}'
      Trains on the first 80% of the daily bars, evaluates out of sample on the rest
      against buy-and-hold and saves the model; the result carries its model_path.
      "timesteps" is the number of environment steps actually trained (on-policy
      algorithms collect whole rollouts, so it can exceed "requested_timesteps").
  evaluate '{"model_path": "<from train>", "ticker": "AAPL", "start_date": "2026-01-01", "end_date": "2026-06-30", "n_episodes": 1}'
      Replays a saved model over a window against buy-and-hold. Default window: from the
      day after the model's training data ended up to today (out of sample).
      algorithm and ticker default to the model's sidecar / file name.
  list_algorithms
"""

import os

# Native thread pools (OpenMP / MKL / OpenBLAS, torch) size themselves to every core by
# default. On a 48-core host one SAC run spun ~770 % CPU and starved itself; on a laptop
# it starves the terminal. Must run before numpy / torch are imported; a value the user
# set in the environment wins.
_MAX_THREADS = min(4, os.cpu_count() or 1)
for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "NUMEXPR_NUM_THREADS", "VECLIB_MAXIMUM_THREADS"):
    os.environ.setdefault(_v, str(_MAX_THREADS))

import json
import sys
from datetime import datetime, date, timedelta
from pathlib import Path
from typing import Dict, List, Any, Optional, Union, Tuple
import warnings
warnings.filterwarnings('ignore')

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import fincept_json

import numpy as np
import pandas as pd

# Qlib RL -- loaded on demand. `import qlib.rl` costs seconds of cold start and only
# `initialize` / `list_algorithms` use it; train / evaluate run on Stable-Baselines3.
RL_AVAILABLE = None   # unknown until _load_qlib_rl()
RL_ERROR = None


def _load_qlib_rl() -> bool:
    global RL_AVAILABLE, RL_ERROR, qlib, REG_CN, REG_US
    if RL_AVAILABLE is None:
        try:
            import qlib
            from qlib.rl import Interpreter  # noqa: F401
            from qlib.config import REG_CN, REG_US
            RL_AVAILABLE = True
        except Exception as e:  # ImportError, or a broken optional dependency
            RL_AVAILABLE = False
            RL_ERROR = str(e)
    return RL_AVAILABLE


# Gym/Stable-Baselines3 for RL algorithms
STABLE_BASELINES_AVAILABLE = False
gym = None
try:
    import gymnasium as gym
    import torch
    from stable_baselines3 import PPO, DQN, A2C, SAC, TD3
    from stable_baselines3.common.callbacks import BaseCallback
    STABLE_BASELINES_AVAILABLE = True
    torch.set_num_threads(_MAX_THREADS)
    try:
        torch.set_num_interop_threads(_MAX_THREADS)
    except RuntimeError:  # already set (only allowed once per process)
        pass
except ImportError:
    pass


# Layout of the observation vector (see TradingEnvironment._get_observation). Version 2
# made every input scale-free (exposure instead of a share count, volume vs its 20-day
# mean, bands / ATR relative to price) and filled bb/atr/adx/obv, which version 1 left
# at a constant 0. A model trained on version 1 has the same (20,) shape but reads
# different inputs, so `evaluate` refuses it (see the sidecar written by `train`).
OBS_VERSION = 2
OBS_DIM = 20

# Loss keys SB3 records, in preference order: PPO/DQN "train/loss", SAC/TD3
# "train/critic_loss", A2C "train/value_loss".
_LOSS_KEYS = ("train/loss", "train/critic_loss", "train/value_loss")


def _emit_event(obj: Dict[str, Any]) -> None:
    """One streamed NDJSON line (progress / log) the C++ service reads as it arrives."""
    print(fincept_json.dumps(obj), flush=True)


if STABLE_BASELINES_AVAILABLE:
    class ProgressCallback(BaseCallback):
        """Emits newline-delimited JSON progress events during SB3 training."""

        def __init__(self, total_timesteps: int, report_every: int = 256):
            super().__init__(verbose=0)
            self.total_timesteps = int(total_timesteps)
            self.report_every = max(1, int(report_every))
            self.last_report = 0
            self.last_loss = None

        def _capture_loss(self) -> None:
            # SB3 clears logger.name_to_value on every dump, so a value read only at
            # report time was almost always gone (PPO "loss" was always 0.0). Keep the
            # latest one seen instead.
            try:
                values = self.model.logger.name_to_value
                for key in _LOSS_KEYS:
                    if key in values:
                        self.last_loss = float(values[key])
                        return
            except Exception:
                pass

        def _on_rollout_start(self) -> None:
            self._capture_loss()

        def _emit(self) -> None:
            reward_mean = None
            try:
                buf = getattr(self.model, "ep_info_buffer", None)
                if buf:
                    rewards = [ep["r"] for ep in buf if "r" in ep]
                    if rewards:
                        reward_mean = float(sum(rewards) / len(rewards))
            except Exception:
                pass
            step = int(self.num_timesteps)
            _emit_event({
                "event": "progress",
                "step": step,
                # On-policy rollouts can run past the requested count; never report
                # a step beyond the total (the progress bar went past 100 %).
                "total": max(self.total_timesteps, step),
                "reward_mean": reward_mean,
                "loss": self.last_loss,
            })

        def _on_step(self) -> bool:
            self._capture_loss()
            if self.num_timesteps - self.last_report < self.report_every:
                return True
            self.last_report = self.num_timesteps
            self._emit()
            return True

        def _on_training_end(self) -> None:
            self._capture_loss()
            self._emit()
else:
    ProgressCallback = None  # type: ignore


class TradingEnvironment(gym.Env if gym else object):
    """
    Custom Gym environment for trading on daily bars
    Supports continuous and discrete action spaces
    """

    def __init__(self,
                 market_data: pd.DataFrame,
                 initial_cash: float = 1000000.0,
                 commission: float = 0.001,
                 action_space_type: str = 'continuous'):
        """
        Initialize trading environment

        Args:
            market_data: DataFrame with OHLCV data plus the indicator columns
                         _download_market_data adds
            initial_cash: Starting capital
            commission: Trading commission rate
            action_space_type: 'continuous' or 'discrete'
        """
        super().__init__()

        self.market_data = market_data
        self.initial_cash = initial_cash
        self.commission = commission
        self.action_space_type = action_space_type
        # Observation prices are expressed relative to the first close. They used to
        # be divided by a hard-coded 100, which hands the network raw values in the
        # thousands for anything priced above ~$100 (BTC, NVDA, indices, ...).
        first_close = float(market_data['close'].iloc[0]) if len(market_data) else 100.0
        self.price_scale = first_close if first_close > 0 else 100.0

        # State space: see OBS_VERSION / _get_observation
        self.observation_space = gym.spaces.Box(
            low=-np.inf,
            high=np.inf,
            shape=(OBS_DIM,),
            dtype=np.float32
        )

        # Action space
        if action_space_type == 'continuous':
            # Continuous: target fraction of portfolio value held in the asset
            # (long-only: <= 0 means flat, 1 = fully invested)
            self.action_space = gym.spaces.Box(
                low=-1.0,
                high=1.0,
                shape=(1,),
                dtype=np.float32
            )
        else:
            # Discrete: 0=sell, 1=hold, 2=buy
            self.action_space = gym.spaces.Discrete(3)

        self.reset()

    def reset(self, seed=None, options=None):
        """Reset environment to initial state"""
        super().reset(seed=seed)

        self.current_step = 0
        self.cash = self.initial_cash
        self.holdings = 0.0
        self.portfolio_value = self.initial_cash
        self.trades = []

        return self._get_observation(), {}

    def _get_observation(self) -> np.ndarray:
        """Current state, every element scale-free (OBS_VERSION 2)."""
        if self.current_step >= len(self.market_data):
            return np.zeros(OBS_DIM, dtype=np.float32)

        row = self.market_data.iloc[self.current_step]
        close = float(row.get('close', 0.0))
        portfolio_now = self.cash + self.holdings * close
        exposure = (self.holdings * close / portfolio_now) if portfolio_now > 0 else 0.0

        obs = np.array([
            self.cash / self.initial_cash,
            exposure,                                   # fraction of the portfolio in the asset
            close / self.price_scale,                   # price relative to the first bar
            row.get('volume_ratio', 1.0),               # volume / its 20-day mean
            row.get('open', 0) / self.price_scale,
            row.get('high', 0) / self.price_scale,
            row.get('low', 0) / self.price_scale,
            row.get('vwap', 0) / self.price_scale,
            row.get('returns', 0),
            row.get('volatility', 0),
            row.get('rsi', 50) / 100,
            row.get('macd', 0),                         # relative to price
            row.get('signal', 0),
            row.get('bb_upper', 0),                     # upper band / close - 1
            row.get('bb_lower', 0),                     # lower band / close - 1
            row.get('atr', 0),                          # ATR / close
            row.get('adx', 0) / 100,
            row.get('obv', 0),                          # 20-day OBV change / 20-day volume, in [-1, 1]
            portfolio_now / self.initial_cash,
            float(self.current_step) / len(self.market_data)
        ], dtype=np.float32)

        return np.nan_to_num(obs, nan=0.0, posinf=0.0, neginf=0.0)

    def step(self, action):
        """Execute one step in the environment"""
        if self.current_step >= len(self.market_data) - 1:
            return self._get_observation(), 0.0, True, False, {
                'portfolio_value': self.portfolio_value, 'cash': self.cash,
                'holdings': self.holdings, 'step': self.current_step}

        current_price = self.market_data.iloc[self.current_step].get('close', 0)

        # Execute action
        if self.action_space_type == 'continuous':
            # The action is the TARGET FRACTION of portfolio value held in the asset
            # (long-only: <= 0 means flat). The old code compared that fraction with
            # self.holdings -- a SHARE COUNT -- so after the first buy the "change" was
            # always negative and the agent could only ever sell.
            raw_action = float(np.asarray(action, dtype=float).reshape(-1)[0])
            target_exposure = min(max(raw_action, 0.0), 1.0)
            portfolio_now = self.cash + self.holdings * current_price
            exposure = (self.holdings * current_price / portfolio_now) if portfolio_now > 0 else 0.0
            delta_value = (target_exposure - exposure) * portfolio_now

            if delta_value > 0 and current_price > 0:  # Buy
                spend = min(delta_value, self.cash / (1 + self.commission))
                shares_to_buy = spend / current_price
                if shares_to_buy > 0:
                    self.holdings += shares_to_buy
                    self.cash -= spend * (1 + self.commission)
                    self.trades.append({'step': self.current_step, 'action': 'buy', 'shares': shares_to_buy, 'price': current_price})

            elif delta_value < 0 and current_price > 0:  # Sell
                shares_to_sell = min(-delta_value / current_price, self.holdings)
                if shares_to_sell > 0:
                    revenue = shares_to_sell * current_price * (1 - self.commission)
                    self.holdings -= shares_to_sell
                    self.cash += revenue
                    self.trades.append({'step': self.current_step, 'action': 'sell', 'shares': shares_to_sell, 'price': current_price})

        else:  # Discrete
            if action == 2:  # Buy
                shares_to_buy = (self.cash * 0.1) / current_price
                cost = shares_to_buy * current_price * (1 + self.commission)
                if cost <= self.cash:
                    self.holdings += shares_to_buy
                    self.cash -= cost
            elif action == 0:  # Sell
                if self.holdings > 0:
                    shares_to_sell = self.holdings * 0.1
                    revenue = shares_to_sell * current_price * (1 - self.commission)
                    self.holdings -= shares_to_sell
                    self.cash += revenue

        # Move to next step
        self.current_step += 1
        next_price = self.market_data.iloc[self.current_step].get('close', current_price)

        # Calculate portfolio value and reward
        prev_portfolio_value = self.portfolio_value
        self.portfolio_value = self.cash + (self.holdings * next_price)

        # Reward: portfolio return
        reward = (self.portfolio_value - prev_portfolio_value) / prev_portfolio_value

        done = self.current_step >= len(self.market_data) - 1
        truncated = False

        info = {
            'portfolio_value': self.portfolio_value,
            'cash': self.cash,
            'holdings': self.holdings,
            'step': self.current_step
        }

        return self._get_observation(), reward, done, truncated, info

    def render(self):
        """Render environment state"""
        print(f"Step: {self.current_step}, Portfolio: ${self.portfolio_value:.2f}, "
              f"Cash: ${self.cash:.2f}, Holdings: {self.holdings:.4f}", file=sys.stderr)


class RLTradingAgent:
    """
    Reinforcement Learning Trading Agent
    Supports multiple RL algorithms for trading strategy optimization
    """

    def __init__(self):
        self.qlib_initialized = False
        self.model = None
        self.env = None
        self.eval_env = None
        self.ticker = None
        self.buy_hold_return_pct = None
        self.data_info = {}
        self.training_history = []

    def initialize_qlib(self,
                       provider_uri: Optional[str] = None,
                       region: str = "cn"):
        """Initialize Qlib. Without a provider_uri: the public qlib CN dataset
        (qlib_datasets.public_dataset_dir)."""
        if not _load_qlib_rl():
            return {'success': False, 'error': f'Qlib RL not available: {RL_ERROR}'}

        try:
            if not provider_uri:
                _here = os.path.dirname(os.path.abspath(__file__))
                if _here not in sys.path:
                    sys.path.insert(1, _here)
                import qlib_datasets
                provider_uri = str(qlib_datasets.public_dataset_dir("cn"))
            if region == "cn":
                qlib.init(provider_uri=provider_uri, region=REG_CN)
            else:
                qlib.init(provider_uri=provider_uri, region=REG_US)

            self.qlib_initialized = True
            return {'success': True, 'message': 'Qlib initialized for RL'}
        except Exception as e:
            return {'success': False, 'error': str(e)}

    @staticmethod
    def _download_market_data(ticker: str, start_date: str, end_date: str,
                              warmup_days: int = 120) -> pd.DataFrame:
        """Daily OHLCV from Yahoo Finance plus the indicator columns the env observes.

        `warmup_days` of extra history before start_date feed the rolling indicators
        and are then dropped, so the returned frame starts at start_date.
        """
        import fincept_yahoo as fy  # the shared bar cache (one long download per symbol)

        try:
            fetch_start = (pd.Timestamp(start_date) - pd.Timedelta(days=int(warmup_days))).date().isoformat()
        except Exception:
            raise ValueError(f"start_date must be YYYY-MM-DD, got {start_date!r}")
        raw = fy.download(ticker, start=fetch_start, end=end_date, auto_adjust=True)
        if raw is None or raw.empty:
            raise ValueError(f"No price data returned for '{ticker}' between {start_date} and {end_date}")
        if isinstance(raw.columns, pd.MultiIndex):
            raw.columns = raw.columns.get_level_values(0)
        df = raw.rename(columns=str.lower)[['open', 'high', 'low', 'close', 'volume']].astype(float)
        df = df.dropna()

        close, high, low, volume = df['close'], df['high'], df['low'], df['volume']
        df['vwap'] = (high + low + close) / 3.0
        df['returns'] = close.pct_change()
        df['volatility'] = df['returns'].rolling(20).std()
        delta = close.diff()
        gain = delta.clip(lower=0).ewm(alpha=1 / 14, adjust=False).mean()
        loss = (-delta.clip(upper=0)).ewm(alpha=1 / 14, adjust=False).mean()
        df['rsi'] = 100 - 100 / (1 + gain / (loss + 1e-10))
        macd = close.ewm(span=12, adjust=False).mean() - close.ewm(span=26, adjust=False).mean()
        df['macd'] = macd / close                      # relative to price so it is scale-free
        df['signal'] = df['macd'].ewm(span=9, adjust=False).mean()

        # Bollinger bands (20, 2) as distance from the close.
        mid = close.rolling(20).mean()
        sd = close.rolling(20).std()
        df['bb_upper'] = (mid + 2 * sd) / close - 1.0
        df['bb_lower'] = (mid - 2 * sd) / close - 1.0

        # Wilder ATR(14) relative to price, and ADX(14).
        prev_close = close.shift(1)
        true_range = pd.concat([high - low, (high - prev_close).abs(), (low - prev_close).abs()], axis=1).max(axis=1)
        atr = true_range.ewm(alpha=1 / 14, adjust=False).mean()
        df['atr'] = atr / close
        up_move = high.diff()
        down_move = -low.diff()
        plus_dm = pd.Series(np.where((up_move > down_move) & (up_move > 0), up_move, 0.0), index=df.index)
        minus_dm = pd.Series(np.where((down_move > up_move) & (down_move > 0), down_move, 0.0), index=df.index)
        plus_di = 100 * plus_dm.ewm(alpha=1 / 14, adjust=False).mean() / (atr + 1e-12)
        minus_di = 100 * minus_dm.ewm(alpha=1 / 14, adjust=False).mean() / (atr + 1e-12)
        dx = 100 * (plus_di - minus_di).abs() / (plus_di + minus_di + 1e-12)
        df['adx'] = dx.ewm(alpha=1 / 14, adjust=False).mean()
        # The first 2 x 14 bars of ADX are still converging.
        df.loc[df.index[:28], 'adx'] = np.nan

        # On-balance volume as its 20-day change over the 20-day traded volume: in [-1, 1]
        # (+1 = every day of the last 20 closed up). The raw OBV is a cumulative share
        # count in the billions.
        obv = (np.sign(delta).fillna(0.0) * volume).cumsum()
        df['obv'] = (obv - obv.shift(20)) / (volume.rolling(20).sum() + 1e-12)
        df['volume_ratio'] = volume / (volume.rolling(20).mean() + 1e-12)

        df = df.dropna()
        df = df[df.index >= pd.Timestamp(start_date)]
        if len(df) < 60:
            raise ValueError(f"Only {len(df)} usable daily bars for '{ticker}' between {start_date} and "
                             f"{end_date}; need at least 60 -- widen the date range")
        return df

    def create_trading_env(self,
                          tickers: List[str],
                          start_date: str,
                          end_date: str,
                          initial_cash: float = 1000000.0,
                          commission: float = 0.001,
                          action_space_type: str = 'continuous',
                          train_fraction: float = 0.8) -> Dict[str, Any]:
        """Create trading environments from real daily bars.

        The first ``train_fraction`` of the history trains the agent; the remainder is
        held out (``self.eval_env``) so evaluation is out-of-sample. This used to
        ignore the ticker and train on cumulative random noise.
        """
        if not STABLE_BASELINES_AVAILABLE:
            return {'success': False, 'error': 'Stable-Baselines3 not installed'}

        try:
            ticker = tickers[0] if isinstance(tickers, (list, tuple)) else str(tickers).split(',')[0].strip()
            market_data = self._download_market_data(ticker, start_date, end_date)

            split = int(len(market_data) * train_fraction)
            if split < 40 or len(market_data) - split < 20:
                return {'success': False,
                        'error': f"Not enough history for a train/test split ({len(market_data)} bars)"}
            train_data = market_data.iloc[:split]
            test_data = market_data.iloc[split:]

            self.env = TradingEnvironment(
                market_data=train_data,
                initial_cash=initial_cash,
                commission=commission,
                action_space_type=action_space_type
            )
            self.eval_env = TradingEnvironment(
                market_data=test_data,
                initial_cash=initial_cash,
                commission=commission,
                action_space_type=action_space_type
            )
            self.ticker = ticker
            close_test = test_data['close']
            self.buy_hold_return_pct = float((close_test.iloc[-1] / close_test.iloc[0] - 1) * 100)
            self.data_info = {
                'ticker': ticker,
                'bars': int(len(market_data)),
                'train_bars': int(len(train_data)),
                'test_bars': int(len(test_data)),
                'train_period': [str(train_data.index[0].date()), str(train_data.index[-1].date())],
                'test_period': [str(test_data.index[0].date()), str(test_data.index[-1].date())],
            }

            return {
                'success': True,
                'message': 'Trading environment created',
                'data_points': int(len(market_data)),
                'action_space': action_space_type,
                'initial_cash': initial_cash
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def train_agent(self,
                   algorithm: str = 'PPO',
                   total_timesteps: int = 100000,
                   learning_rate: float = 3e-4,
                   **kwargs) -> Dict[str, Any]:
        """Train RL agent"""
        if not STABLE_BASELINES_AVAILABLE:
            return {'success': False, 'error': 'Stable-Baselines3 not available'}

        if self.env is None:
            return {'success': False, 'error': 'Environment not created. Call create_trading_env first'}

        try:
            # Select algorithm
            algo_map = {
                'PPO': PPO,
                'DQN': DQN,
                'A2C': A2C,
                'SAC': SAC,
                'TD3': TD3
            }

            if algorithm not in algo_map:
                return {'success': False, 'error': f'Unknown algorithm: {algorithm}'}

            AlgoClass = algo_map[algorithm]
            total_timesteps = max(1, int(total_timesteps))

            if algorithm == 'PPO':
                # PPO's default rollout (2048 steps) is longer than the ~400 training bars
                # of a two-year window: a 2-episode run trained 2048 steps and reported
                # 766. One rollout per pass over the data instead, in whole mini-batches.
                bars = len(self.env.market_data)
                kwargs.setdefault('n_steps', max(64, min(2048, (bars // 64) * 64)))
                kwargs.setdefault('batch_size', 64)

            # Create model with verbose=0 so SB3 doesn't emit its own rollout tables.
            # Our ProgressCallback is the single source of structured progress events;
            # any other stdout/stderr still flows to the UI as raw log lines.
            self.model = AlgoClass(
                'MlpPolicy',
                self.env,
                learning_rate=learning_rate,
                verbose=0,
                **kwargs
            )

            # On-policy algorithms train in whole rollouts and stop at the first rollout
            # boundary at or after the count they are given: hand them the nearest whole
            # number of rollouts (at least one), so the run, the progress total and the
            # reported timesteps agree.
            learn_timesteps = total_timesteps
            if algorithm in ('PPO', 'A2C'):
                rollout = int(self.model.n_steps) * int(self.model.n_envs)
                learn_timesteps = max(1, int(round(total_timesteps / rollout))) * rollout

            # Report roughly 200 progress ticks regardless of run length.
            report_every = max(256, learn_timesteps // 200)
            callback = ProgressCallback(learn_timesteps, report_every) if ProgressCallback else None

            # Train
            self.model.learn(total_timesteps=learn_timesteps, callback=callback)

            return {
                'success': True,
                'algorithm': algorithm,
                'timesteps': int(self.model.num_timesteps),
                'requested_timesteps': total_timesteps,
                'message': f'{algorithm} agent trained for {int(self.model.num_timesteps)} steps'
            }
        except Exception as e:
            # Emit an error log event so the UI log console sees it live,
            # in addition to the final result payload.
            try:
                _emit_event({
                    "event": "log",
                    "level": "error",
                    "msg": f"train_agent exception: {e}",
                })
            except Exception:
                pass
            return {'success': False, 'error': str(e)}

    def evaluate_agent(self, n_episodes: int = 10) -> Dict[str, Any]:
        """Evaluate trained RL agent"""
        if self.model is None:
            return {'success': False, 'error': 'No trained model available'}

        try:
            episode_rewards = []
            episode_lengths = []
            final_portfolios = []
            # Held-out segment when the env was built by create_trading_env().
            env = self.eval_env if self.eval_env is not None else self.env

            for episode in range(n_episodes):
                obs, _ = env.reset()
                done = False
                episode_reward = 0
                steps = 0

                while not done:
                    action, _ = self.model.predict(obs, deterministic=True)
                    obs, reward, done, truncated, info = env.step(action)
                    episode_reward += reward
                    steps += 1

                    if done or truncated:
                        final_portfolios.append(info['portfolio_value'])
                        break

                episode_rewards.append(episode_reward)
                episode_lengths.append(steps)

            return {
                'success': True,
                'n_episodes': n_episodes,
                'mean_reward': float(np.mean(episode_rewards)),
                'std_reward': float(np.std(episode_rewards)),
                'mean_length': float(np.mean(episode_lengths)),
                'mean_portfolio_value': float(np.mean(final_portfolios)),
                'portfolio_return': float((np.mean(final_portfolios) / env.initial_cash - 1) * 100),
                'all_rewards': [float(r) for r in episode_rewards]
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def load_model(self, path: str, algorithm: str = 'PPO') -> Dict[str, Any]:
        """Load trained RL model"""
        if not STABLE_BASELINES_AVAILABLE:
            return {'success': False, 'error': 'Stable-Baselines3 not available'}

        try:
            algo_map = {'PPO': PPO, 'DQN': DQN, 'A2C': A2C, 'SAC': SAC, 'TD3': TD3}
            AlgoClass = algo_map.get(algorithm, PPO)

            self.model = AlgoClass.load(path)
            return {'success': True, 'path': path, 'algorithm': algorithm}
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_available_algorithms(self) -> Dict[str, Any]:
        """Get list of available RL algorithms"""
        algorithms = {
            'PPO': 'Proximal Policy Optimization - Best for continuous action spaces',
            'A2C': 'Advantage Actor-Critic - Fast training, good baseline',
            'DQN': 'Deep Q-Network - For discrete action spaces',
            'SAC': 'Soft Actor-Critic - Off-policy, continuous actions',
            'TD3': 'Twin Delayed DDPG - Robust continuous control'
        }

        return {
            'success': True,
            'algorithms': algorithms,
            'stable_baselines_available': STABLE_BASELINES_AVAILABLE,
            'qlib_rl_available': _load_qlib_rl()
        }


_RL_ALGORITHMS = ('PPO', 'DQN', 'A2C', 'SAC', 'TD3')


def _sidecar_path(model_path: Path) -> Path:
    """<model>.json next to <model>.zip: what `evaluate` needs to know about a saved model."""
    return model_path.with_suffix('.json')


def _evaluate_saved_model(agent: RLTradingAgent, params: Dict[str, Any]) -> Dict[str, Any]:
    """`evaluate`: replay a model saved by `train` over daily bars, next to buy-and-hold.

    A fresh process has no agent in memory, so this loads the file `train` reported as
    model_path (named <ALGO>_<TICKER>_<YYYYmmdd>_<HHMMSS>.zip, with a <same>.json sidecar)
    and evaluates it on start_date..end_date. Default window: from the day after the
    model's training data ended up to today, i.e. out of sample.
    """
    if not isinstance(params, dict):
        return {'success': False, 'error': 'evaluate takes a JSON object, e.g. {"model_path": "..."}'}
    if not STABLE_BASELINES_AVAILABLE:
        return {'success': False, 'error': 'Stable-Baselines3 not available'}
    raw_path = str(params.get('model_path') or '').strip()
    if not raw_path:
        return {'success': False, 'error': "evaluate needs 'model_path' (the model_path a train run returned)"}
    model_path = Path(raw_path).expanduser()
    if not model_path.is_file():
        return {'success': False, 'error': f'Model file not found: {model_path}'}

    # The sidecar records the observation layout the model was trained on. Models saved
    # before OBS_VERSION 2 have none: their inputs differ (share count instead of
    # exposure, constant-zero indicators), so replaying them would produce numbers
    # that mean nothing.
    meta = {}
    sidecar = _sidecar_path(model_path)
    if sidecar.is_file():
        try:
            meta = json.loads(sidecar.read_text(encoding='utf-8'))
        except Exception:
            meta = {}
    if int(meta.get('obs_version') or 0) != OBS_VERSION:
        return {'success': False,
                'error': f'{model_path.name} was trained with an older observation layout (or not by this '
                         f'module) -- retrain it with TRAIN RL AGENT, then evaluate the new model.'}

    name_parts = model_path.stem.split('_')
    algorithm = str(params.get('algorithm') or meta.get('algorithm') or name_parts[0]).upper()
    if algorithm not in _RL_ALGORITHMS:
        return {'success': False,
                'error': f"Unknown algorithm '{algorithm}' -- pass 'algorithm' ({', '.join(_RL_ALGORITHMS)})"}
    ticker = str(params.get('ticker') or meta.get('ticker') or
                 ('_'.join(name_parts[1:-2]) if len(name_parts) >= 4 else '')).strip().upper()
    if not ticker:
        return {'success': False, 'error': "evaluate needs 'ticker' (the model file name does not carry one)"}

    loaded = agent.load_model(str(model_path), algorithm)
    if not loaded.get('success'):
        return {'success': False,
                'error': f"Could not load {model_path.name} as {algorithm}: {loaded.get('error')}"}
    obs_shape = tuple(getattr(agent.model.observation_space, 'shape', ()) or ())
    if obs_shape != (OBS_DIM,):
        return {'success': False,
                'error': f'{model_path.name} expects observations of shape {obs_shape}, this environment '
                         f'produces ({OBS_DIM},) -- retrain the model.'}

    train_end = (meta.get('train_period') or [None, None])[-1]
    end_date = str(params.get('end_date') or date.today().isoformat())
    if params.get('start_date'):
        start_date = str(params['start_date'])
    elif train_end:
        start_date = (pd.Timestamp(train_end) + pd.Timedelta(days=1)).date().isoformat()
    else:
        start_date = (date.today() - timedelta(days=365)).isoformat()
    if pd.Timestamp(start_date) >= pd.Timestamp(end_date):
        return {'success': False, 'error': f'start_date {start_date} must be before end_date {end_date}'}

    market_data = agent._download_market_data(ticker, start_date, end_date)
    # The environment's action space must be the one the model was trained with
    # (train builds a discrete env for DQN, a continuous one otherwise).
    discrete = isinstance(agent.model.action_space, gym.spaces.Discrete)
    agent.eval_env = TradingEnvironment(market_data=market_data,
                                        initial_cash=float(params.get('initial_capital', 100000)),
                                        action_space_type='discrete' if discrete else 'continuous')
    result = agent.evaluate_agent(max(1, int(params.get('n_episodes', 1))))
    if result.get('success'):
        close = market_data['close']
        first_bar = str(market_data.index[0].date())
        result.update({
            'algorithm': algorithm,
            'ticker': ticker,
            'model_path': str(model_path),
            'period': [first_bar, str(market_data.index[-1].date())],
            'bars': int(len(market_data)),
            'buy_hold_return_pct': float((close.iloc[-1] / close.iloc[0] - 1) * 100),
            'train_period': meta.get('train_period'),
        })
        if train_end and pd.Timestamp(first_bar) <= pd.Timestamp(train_end):
            result['warning'] = (f'The evaluation window starts on {first_bar}, inside the training data '
                                 f'(which ran to {train_end}): this part of the result is in-sample.')
    return result


def _dispatch(command: str, agent: RLTradingAgent) -> Dict[str, Any]:
    """Run one CLI command; its arguments are sys.argv[2:]."""
    if command == 'list_algorithms':
        result = agent.get_available_algorithms()

    elif command == 'initialize':
        provider_uri = sys.argv[2] if len(sys.argv) > 2 else None   # None: the public CN dataset
        region = sys.argv[3] if len(sys.argv) > 3 else "cn"
        result = agent.initialize_qlib(provider_uri, region)

    elif command == 'create_env':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        result = agent.create_trading_env(**params)

    elif command == 'train':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        # Normalize param names from C++ UI
        if 'ticker' in params:
            params['tickers'] = params.pop('ticker')
        if 'initial_capital' in params:
            params['initial_cash'] = params.pop('initial_capital')
        episodes = params.pop('episodes', None)
        # Auto-create env from same params before training
        env_params = {k: params.pop(k) for k in ['tickers', 'initial_cash', 'start_date', 'end_date']
                      if k in params}
        env_params.setdefault('tickers', 'AAPL')
        env_params.setdefault('initial_cash', 100000)
        # Default window: the last two years of daily bars (it was a fixed 2022-2024).
        env_params.setdefault('end_date', date.today().isoformat())
        env_params.setdefault('start_date', (date.today() - timedelta(days=730)).isoformat())
        # DQN only supports a discrete action space; the continuous env made
        # DQN('MlpPolicy', env) fail its action-space assertion on every run.
        algorithm = params.get('algorithm', 'PPO')
        env_params.setdefault('action_space_type', 'discrete' if algorithm == 'DQN' else 'continuous')
        env_result = agent.create_trading_env(**env_params)
        if not env_result.get('success'):
            result = env_result
        else:
            # One "episode" is one pass over the training bars (it was a flat 365 steps,
            # unrelated to how much data was actually loaded).
            if episodes is not None:
                params.setdefault('total_timesteps', int(episodes) * len(agent.env.market_data))
            result = agent.train_agent(**params)
            if result.get('success'):
                # Out-of-sample evaluation + persistence, in the same process: nothing
                # survives the process otherwise, so "evaluate" could never find a model.
                # The policy and the replay are deterministic, so one episode is the result.
                evaluation = agent.evaluate_agent(1)
                if evaluation.get('success'):
                    evaluation['buy_hold_return_pct'] = agent.buy_hold_return_pct
                    result['evaluation'] = evaluation
                result['data'] = agent.data_info
                try:
                    # <data root>/script-data/rl_models (the ~/.fincept/rl_models of
                    # releases is copied in once, never moved).
                    import fincept_paths
                    models_dir = fincept_paths.user_data_dir('rl_models', legacy='rl_models')
                    model_path = models_dir / (f"{algorithm}_{agent.ticker}_"
                                               f"{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip")
                    agent.model.save(str(model_path))
                    _sidecar_path(model_path).write_text(json.dumps({
                        'obs_version': OBS_VERSION,
                        'algorithm': algorithm,
                        'ticker': agent.ticker,
                        'action_space': env_params.get('action_space_type'),
                        'train_period': agent.data_info.get('train_period'),
                        'test_period': agent.data_info.get('test_period'),
                        'timesteps': result.get('timesteps'),
                        'saved_at': datetime.now().isoformat(timespec='seconds'),
                    }), encoding='utf-8')
                    result['model_path'] = str(model_path)
                except Exception as e:
                    result['model_save_error'] = str(e)

    elif command == 'evaluate':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        result = _evaluate_saved_model(agent, params)

    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    return result


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(fincept_json.dumps({
            'event': 'result',
            'success': False,
            'error': 'Usage: python qlib_rl.py <command> [args...]'
        }), flush=True)
        return

    command = sys.argv[1]
    # A malformed JSON argument, a missing ticker or an unreadable model file used to
    # escape as a bare traceback, leaving the caller no result line to read.
    try:
        result = _dispatch(command, RLTradingAgent())
    except Exception as exc:
        import traceback
        traceback.print_exc(file=sys.stderr)
        result = {'success': False, 'error': f'{command}: {exc}'}

    # Emit final result as a single-line "result" event so the C++ service can parse
    # it reliably. Intermediate progress/log events are already printed from
    # ProgressCallback during training.
    payload = {"event": "result"}
    payload.update(result)
    print(fincept_json.dumps(payload), flush=True)


if __name__ == '__main__':
    main()
