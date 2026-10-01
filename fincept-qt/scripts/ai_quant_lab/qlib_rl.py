"""
AI Quant Lab - Reinforcement Learning Module
Qlib RL integration for portfolio management and trading strategy optimization
Supports DQN, PPO, A2C, SAC algorithms for automated trading
"""

import json
import sys
import os
from datetime import datetime
from typing import Dict, List, Any, Optional, Union, Tuple
import warnings
warnings.filterwarnings('ignore')

import numpy as np
import pandas as pd

# Qlib RL imports with availability check
RL_AVAILABLE = False
RL_ERROR = None

try:
    import qlib
    from qlib.rl import Interpreter
    from qlib.rl.order_execution import SingleAssetOrderExecutionSimple
    from qlib.rl.reward import Reward
    from qlib.rl.simulator import InitialStateType, Simulator
    from qlib.rl.trainer import Trainer
    from qlib.rl.utils import LogLevel, LogWriter
    from qlib.data import D
    from qlib.config import REG_CN, REG_US
    RL_AVAILABLE = True
except ImportError as e:
    RL_ERROR = str(e)

# Gym/Stable-Baselines3 for RL algorithms
STABLE_BASELINES_AVAILABLE = False
gym = None
try:
    import gymnasium as gym
    from stable_baselines3 import PPO, DQN, A2C, SAC, TD3
    from stable_baselines3.common.vec_env import DummyVecEnv, SubprocVecEnv
    from stable_baselines3.common.callbacks import BaseCallback, EvalCallback
    from stable_baselines3.common.monitor import Monitor
    STABLE_BASELINES_AVAILABLE = True
except ImportError:
    pass


if STABLE_BASELINES_AVAILABLE:
    class ProgressCallback(BaseCallback):
        """Emits newline-delimited JSON progress events during SB3 training."""

        def __init__(self, total_timesteps: int, report_every: int = 256):
            super().__init__(verbose=0)
            self.total_timesteps = int(total_timesteps)
            self.report_every = max(1, int(report_every))
            self.last_report = 0

        def _on_step(self) -> bool:
            if self.num_timesteps - self.last_report < self.report_every:
                return True
            self.last_report = self.num_timesteps
            reward_mean = 0.0
            loss = 0.0
            try:
                buf = getattr(self.model, "ep_info_buffer", None)
                if buf:
                    rewards = [ep["r"] for ep in buf if "r" in ep]
                    if rewards:
                        reward_mean = float(sum(rewards) / len(rewards))
                loss = float(self.logger.name_to_value.get("train/loss", 0.0))
            except Exception:
                pass
            print(json.dumps({
                "event": "progress",
                "step": int(self.num_timesteps),
                "total": self.total_timesteps,
                "reward_mean": reward_mean,
                "loss": loss,
            }), flush=True)
            return True
else:
    ProgressCallback = None  # type: ignore


class TradingEnvironment(gym.Env if gym else object):
    """
    Custom Gym environment for trading using Qlib data
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
            market_data: DataFrame with OHLCV data
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

        # State space: [cash, holdings, price, volume, technical indicators]
        self.observation_space = gym.spaces.Box(
            low=-np.inf,
            high=np.inf,
            shape=(20,),
            dtype=np.float32
        )

        # Action space
        if action_space_type == 'continuous':
            # Continuous: position size from -1 (full short) to +1 (full long)
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
        """Get current state observation"""
        if self.current_step >= len(self.market_data):
            return np.zeros(20, dtype=np.float32)

        row = self.market_data.iloc[self.current_step]

        # Normalize state features
        obs = np.array([
            self.cash / self.initial_cash,
            self.holdings,
            row.get('close', 0) / self.price_scale,  # Normalized price
            row.get('volume', 0) / 1e6,  # Normalized volume
            row.get('open', 0) / self.price_scale,
            row.get('high', 0) / self.price_scale,
            row.get('low', 0) / self.price_scale,
            row.get('vwap', 0) / self.price_scale,
            row.get('returns', 0),
            row.get('volatility', 0),
            row.get('rsi', 50) / 100,
            row.get('macd', 0),
            row.get('signal', 0),
            row.get('bb_upper', 0) / self.price_scale,
            row.get('bb_lower', 0) / self.price_scale,
            row.get('atr', 0),
            row.get('adx', 0) / 100,
            row.get('obv', 0) / 1e9,
            self.portfolio_value / self.initial_cash,
            float(self.current_step) / len(self.market_data)
        ], dtype=np.float32)

        return obs

    def step(self, action):
        """Execute one step in the environment"""
        if self.current_step >= len(self.market_data) - 1:
            return self._get_observation(), 0.0, True, False, {}

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
              f"Cash: ${self.cash:.2f}, Holdings: {self.holdings:.4f}")


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
                       provider_uri: str = "~/.qlib/qlib_data/cn_data",
                       region: str = "cn"):
        """Initialize Qlib"""
        if not RL_AVAILABLE:
            return {'success': False, 'error': f'Qlib RL not available: {RL_ERROR}'}

        try:
            if region == "cn":
                qlib.init(provider_uri=provider_uri, region=REG_CN)
            else:
                qlib.init(provider_uri=provider_uri, region=REG_US)

            self.qlib_initialized = True
            return {'success': True, 'message': 'Qlib initialized for RL'}
        except Exception as e:
            return {'success': False, 'error': str(e)}

    @staticmethod
    def _download_market_data(ticker: str, start_date: str, end_date: str) -> pd.DataFrame:
        """Daily OHLCV from Yahoo Finance plus the indicator columns the env observes."""
        import yfinance as yf

        raw = yf.download(ticker, start=start_date, end=end_date, auto_adjust=True, progress=False)
        if raw is None or raw.empty:
            raise ValueError(f"No price data returned for '{ticker}' between {start_date} and {end_date}")
        if isinstance(raw.columns, pd.MultiIndex):
            raw.columns = raw.columns.get_level_values(0)
        df = raw.rename(columns=str.lower)[['open', 'high', 'low', 'close', 'volume']].astype(float)
        df = df.dropna()

        close = df['close']
        df['vwap'] = (df['high'] + df['low'] + close) / 3.0
        df['returns'] = close.pct_change()
        df['volatility'] = df['returns'].rolling(20).std()
        delta = close.diff()
        gain = delta.clip(lower=0).ewm(alpha=1 / 14, adjust=False).mean()
        loss = (-delta.clip(upper=0)).ewm(alpha=1 / 14, adjust=False).mean()
        df['rsi'] = 100 - 100 / (1 + gain / (loss + 1e-10))
        macd = close.ewm(span=12, adjust=False).mean() - close.ewm(span=26, adjust=False).mean()
        df['macd'] = macd / close                      # relative to price so it is scale-free
        df['signal'] = df['macd'].ewm(span=9, adjust=False).mean()
        df = df.dropna()
        if len(df) < 60:
            raise ValueError(f"Only {len(df)} usable daily bars for '{ticker}'; need at least 60 -- "
                             "widen the date range")
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

            # Report roughly 200 progress ticks regardless of run length.
            report_every = max(256, total_timesteps // 200)
            callback = ProgressCallback(total_timesteps, report_every) if ProgressCallback else None

            # Train
            self.model.learn(total_timesteps=total_timesteps, callback=callback)

            return {
                'success': True,
                'algorithm': algorithm,
                'timesteps': total_timesteps,
                'message': f'{algorithm} agent trained successfully'
            }
        except Exception as e:
            # Emit an error log event so the UI log console sees it live,
            # in addition to the final result payload.
            try:
                print(json.dumps({
                    "event": "log",
                    "level": "error",
                    "msg": f"train_agent exception: {e}",
                }), flush=True)
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

    def save_model(self, path: str) -> Dict[str, Any]:
        """Save trained RL model"""
        if self.model is None:
            return {'success': False, 'error': 'No model to save'}

        try:
            self.model.save(path)
            return {'success': True, 'path': path}
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
            'qlib_rl_available': RL_AVAILABLE
        }


def _json_safe(obj):
    """Replace NaN / +-Infinity with None before json.dumps.

    Python emits them as bare ``NaN`` / ``Infinity`` tokens, which are not JSON: the
    terminal's parser rejects the WHOLE payload ("malformed JSON") over one empty cell.
    """
    if isinstance(obj, float):
        return obj if obj == obj and obj not in (float('inf'), float('-inf')) else None
    if isinstance(obj, dict):
        return {k: _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    return obj


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            'success': False,
            'error': 'Usage: python qlib_rl.py <command> [args...]'
        }))
        return

    command = sys.argv[1]
    agent = RLTradingAgent()

    if command == 'list_algorithms':
        result = agent.get_available_algorithms()

    elif command == 'initialize':
        provider_uri = sys.argv[2] if len(sys.argv) > 2 else "~/.qlib/qlib_data/cn_data"
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
        from datetime import date, timedelta
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
                evaluation = agent.evaluate_agent(5)
                if evaluation.get('success'):
                    evaluation['buy_hold_return_pct'] = agent.buy_hold_return_pct
                    result['evaluation'] = evaluation
                result['data'] = agent.data_info
                try:
                    from pathlib import Path
                    models_dir = Path.home() / '.fincept' / 'rl_models'
                    models_dir.mkdir(parents=True, exist_ok=True)
                    model_path = models_dir / (f"{algorithm}_{agent.ticker}_"
                                               f"{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip")
                    agent.model.save(str(model_path))
                    result['model_path'] = str(model_path)
                except Exception as e:
                    result['model_save_error'] = str(e)

    elif command == 'evaluate':
        n_episodes = int(sys.argv[2]) if len(sys.argv) > 2 else 10
        result = agent.evaluate_agent(n_episodes)

    elif command == 'save_model':
        path = sys.argv[2] if len(sys.argv) > 2 else 'rl_model.zip'
        result = agent.save_model(path)

    elif command == 'load_model':
        path = sys.argv[2] if len(sys.argv) > 2 else 'rl_model.zip'
        algorithm = sys.argv[3] if len(sys.argv) > 3 else 'PPO'
        result = agent.load_model(path, algorithm)

    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    # Emit final result as a single-line "result" event so the C++ service can parse
    # it reliably. Intermediate progress/log events are already printed from
    # ProgressCallback during training.
    payload = {"event": "result"}
    payload.update(result)
    print(json.dumps(_json_safe(payload)), flush=True)


if __name__ == '__main__':
    main()
