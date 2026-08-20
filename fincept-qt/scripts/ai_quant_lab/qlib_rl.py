"""
AI Quant Lab - Reinforcement Learning Module
Qlib RL integration for portfolio management and trading strategy optimization
Supports DQN, PPO, A2C, SAC, TD3 algorithms for automated trading
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

STABLE_BASELINES_AVAILABLE = False
gym = None

try:
    import gymnasium as gym
    from stable_baselines3 import PPO, DQN, A2C, SAC, TD3
    from stable_baselines3.common.callbacks import BaseCallback
    from stable_baselines3.common.monitor import Monitor
    STABLE_BASELINES_AVAILABLE = True
except ImportError:
    pass


MODEL_DIR = os.path.abspath(
    os.path.expanduser(
        os.environ.get("AI_QUANT_MODEL_DIR", "./models")
    )
)


if STABLE_BASELINES_AVAILABLE:
    class ProgressCallback(BaseCallback):
        def __init__(self, total_timesteps: int, report_every: int = 256):
            super().__init__(verbose=0)
            self.total_timesteps = max(1, int(total_timesteps))
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
                    rewards = [
                        float(ep["r"])
                        for ep in buf
                        if "r" in ep and np.isfinite(ep["r"])
                    ]

                    if rewards:
                        reward_mean = float(np.mean(rewards))

                logger = getattr(self.model, "logger", None)

                if logger is not None:
                    value = logger.name_to_value.get("train/loss", 0.0)

                    if value is not None and np.isfinite(value):
                        loss = float(value)

            except Exception:
                pass

            print(
                json.dumps({
                    "event": "progress",
                    "step": int(self.num_timesteps),
                    "total": self.total_timesteps,
                    "reward_mean": reward_mean,
                    "loss": loss
                }),
                flush=True
            )

            return True
else:
    ProgressCallback = None


class TradingEnvironment(gym.Env if gym else object):

    metadata = {"render_modes": []}

    def __init__(
        self,
        market_data: pd.DataFrame,
        initial_cash: float = 1000000.0,
        commission: float = 0.001,
        slippage: float = 0.0005,
        action_space_type: str = "continuous",
        episode_length: Optional[int] = None,
        random_start: bool = False
    ):
        super().__init__()

        self.market_data = self._validate_market_data(market_data)
        self.initial_cash = float(initial_cash)
        self.commission = float(commission)
        self.slippage = float(slippage)
        self.action_space_type = str(action_space_type).lower()
        self.random_start = bool(random_start)

        if self.initial_cash <= 0:
            raise ValueError("initial_cash must be greater than zero")

        if self.commission < 0:
            raise ValueError("commission cannot be negative")

        if self.slippage < 0:
            raise ValueError("slippage cannot be negative")

        if self.action_space_type not in {"continuous", "discrete"}:
            raise ValueError(
                "action_space_type must be 'continuous' or 'discrete'"
            )

        self.episode_length = (
            int(episode_length)
            if episode_length is not None
            else len(self.market_data) - 1
        )

        if self.episode_length < 1:
            raise ValueError("episode_length must be at least 1")

        if self.episode_length >= len(self.market_data):
            self.episode_length = len(self.market_data) - 1

        self.observation_space = gym.spaces.Box(
            low=-np.inf,
            high=np.inf,
            shape=(20,),
            dtype=np.float32
        )

        if self.action_space_type == "continuous":
            self.action_space = gym.spaces.Box(
                low=-1.0,
                high=1.0,
                shape=(1,),
                dtype=np.float32
            )
        else:
            self.action_space = gym.spaces.Discrete(3)

        self.current_step = 0
        self.start_step = 0
        self.end_step = 0
        self.cash = self.initial_cash
        self.holdings = 0.0
        self.portfolio_value = self.initial_cash
        self.previous_portfolio_value = self.initial_cash
        self.trades = []
        self.returns_history = []

    @staticmethod
    def _validate_market_data(
        market_data: pd.DataFrame
    ) -> pd.DataFrame:

        if not isinstance(market_data, pd.DataFrame):
            raise TypeError("market_data must be a pandas DataFrame")

        required = [
            "open",
            "high",
            "low",
            "close",
            "volume"
        ]

        missing = [
            column
            for column in required
            if column not in market_data.columns
        ]

        if missing:
            raise ValueError(
                f"Missing required market data columns: {missing}"
            )

        data = market_data.copy()

        data = data.replace([np.inf, -np.inf], np.nan)
        data = data.dropna(subset=required)

        if data.empty:
            raise ValueError("market_data is empty after validation")

        if (data["close"] <= 0).any():
            raise ValueError("close prices must be greater than zero")

        if (data["open"] <= 0).any():
            raise ValueError("open prices must be greater than zero")

        if (data["high"] <= 0).any():
            raise ValueError("high prices must be greater than zero")

        if (data["low"] <= 0).any():
            raise ValueError("low prices must be greater than zero")

        if (data["volume"] < 0).any():
            raise ValueError("volume cannot be negative")

        invalid_ohlc = (
            (data["high"] < data[["open", "close"]].max(axis=1))
            |
            (data["low"] > data[["open", "close"]].min(axis=1))
            |
            (data["high"] < data["low"])
        )

        if invalid_ohlc.any():
            raise ValueError("Invalid OHLC data")

        for column in data.columns:
            if not np.issubdtype(data[column].dtype, np.number):
                continue

            data[column] = data[column].astype(np.float64)

        return data.reset_index(drop=True)

    def reset(self, seed=None, options=None):

        super().reset(seed=seed)

        max_start = len(self.market_data) - self.episode_length - 1

        if self.random_start and max_start > 0:
            self.start_step = int(
                self.np_random.integers(0, max_start + 1)
            )
        else:
            self.start_step = 0

        self.current_step = self.start_step
        self.end_step = min(
            self.start_step + self.episode_length,
            len(self.market_data) - 1
        )

        self.cash = self.initial_cash
        self.holdings = 0.0
        self.portfolio_value = self.initial_cash
        self.previous_portfolio_value = self.initial_cash
        self.trades = []
        self.returns_history = []

        return self._get_observation(), {}

    def _get_observation(self) -> np.ndarray:

        row = self.market_data.iloc[self.current_step]

        portfolio_value = max(
            float(self.portfolio_value),
            1e-8
        )

        def safe_float(value, default=0.0):
            try:
                value = float(value)
                return value if np.isfinite(value) else default
            except Exception:
                return default

        obs = np.array([
            self.cash / self.initial_cash,
            self._position_weight(),
            safe_float(row.get("close", 0.0)) / 100.0,
            safe_float(row.get("volume", 0.0)) / 1e6,
            safe_float(row.get("open", 0.0)) / 100.0,
            safe_float(row.get("high", 0.0)) / 100.0,
            safe_float(row.get("low", 0.0)) / 100.0,
            safe_float(row.get("vwap", row.get("close", 0.0))) / 100.0,
            safe_float(row.get("returns", 0.0)),
            safe_float(row.get("volatility", 0.0)),
            safe_float(row.get("rsi", 50.0)) / 100.0,
            safe_float(row.get("macd", 0.0)),
            safe_float(row.get("signal", 0.0)),
            safe_float(row.get("bb_upper", row.get("close", 0.0))) / 100.0,
            safe_float(row.get("bb_lower", row.get("close", 0.0))) / 100.0,
            safe_float(row.get("atr", 0.0)),
            safe_float(row.get("adx", 0.0)) / 100.0,
            safe_float(row.get("obv", 0.0)) / 1e9,
            portfolio_value / self.initial_cash,
            float(
                self.current_step - self.start_step
            ) / max(
                1,
                self.end_step - self.start_step
            )
        ], dtype=np.float32)

        return np.nan_to_num(
            obs,
            nan=0.0,
            posinf=0.0,
            neginf=0.0
        )

    def _position_weight(self) -> float:

        price = float(
            self.market_data.iloc[self.current_step]["close"]
        )

        position_value = self.holdings * price
        portfolio_value = self.cash + position_value

        if portfolio_value <= 0:
            return 0.0

        return float(position_value / portfolio_value)

    def _execute_target_position(
        self,
        target_weight: float,
        price: float
    ):

        target_weight = float(
            np.clip(target_weight, -1.0, 1.0)
        )

        current_position_value = self.holdings * price
        portfolio_value = self.cash + current_position_value

        if portfolio_value <= 0:
            return

        target_position_value = portfolio_value * target_weight
        target_shares = target_position_value / price

        delta_shares = target_shares - self.holdings

        if abs(delta_shares) < 1e-12:
            return

        execution_price = price

        if delta_shares > 0:
            execution_price = price * (1.0 + self.slippage)

            shares = delta_shares

            gross_cost = shares * execution_price
            commission_cost = gross_cost * self.commission
            total_cost = gross_cost + commission_cost

            if total_cost > self.cash:
                available = self.cash / (
                    execution_price * (1.0 + self.commission)
                )

                shares = max(0.0, available)

                gross_cost = shares * execution_price
                commission_cost = gross_cost * self.commission
                total_cost = gross_cost + commission_cost

            if shares > 0:
                self.holdings += shares
                self.cash -= total_cost

                self.trades.append({
                    "step": self.current_step,
                    "action": "buy",
                    "shares": float(shares),
                    "price": float(execution_price),
                    "commission": float(commission_cost)
                })

        else:
            shares = min(
                abs(delta_shares),
                max(0.0, self.holdings)
            )

            if shares <= 0:
                return

            execution_price = price * (1.0 - self.slippage)

            gross_revenue = shares * execution_price
            commission_cost = gross_revenue * self.commission
            net_revenue = gross_revenue - commission_cost

            self.holdings -= shares
            self.cash += net_revenue

            self.trades.append({
                "step": self.current_step,
                "action": "sell",
                "shares": float(shares),
                "price": float(execution_price),
                "commission": float(commission_cost)
            })

    def _execute_discrete_action(
        self,
        action: int,
        price: float
    ):

        if action == 2:
            current_weight = self._position_weight()
            target_weight = min(1.0, current_weight + 0.10)
            self._execute_target_position(
                target_weight,
                price
            )

        elif action == 0:
            current_weight = self._position_weight()
            target_weight = max(0.0, current_weight - 0.10)
            self._execute_target_position(
                target_weight,
                price
            )

    def step(self, action):

        if self.current_step >= self.end_step:
            return (
                self._get_observation(),
                0.0,
                True,
                False,
                self._info()
            )

        current_price = float(
            self.market_data.iloc[self.current_step]["close"]
        )

        self.previous_portfolio_value = max(
            self.portfolio_value,
            1e-8
        )

        if self.action_space_type == "continuous":

            action_array = np.asarray(action).reshape(-1)

            if len(action_array) == 0:
                target_weight = 0.0
            else:
                target_weight = float(action_array[0])

            if not np.isfinite(target_weight):
                target_weight = 0.0

            target_weight = float(
                np.clip(target_weight, 0.0, 1.0)
            )

            self._execute_target_position(
                target_weight,
                current_price
            )

        else:

            action_value = int(np.asarray(action).item())

            if action_value not in (0, 1, 2):
                action_value = 1

            self._execute_discrete_action(
                action_value,
                current_price
            )

        self.current_step += 1

        next_price = float(
            self.market_data.iloc[self.current_step]["close"]
        )

        self.portfolio_value = (
            self.cash +
            self.holdings * next_price
        )

        if not np.isfinite(self.portfolio_value):
            self.portfolio_value = self.previous_portfolio_value

        reward = (
            self.portfolio_value /
            max(self.previous_portfolio_value, 1e-8)
        ) - 1.0

        reward = float(
            np.clip(reward, -1.0, 1.0)
        )

        self.returns_history.append(reward)

        done = self.current_step >= self.end_step

        info = self._info()

        return (
            self._get_observation(),
            reward,
            done,
            False,
            info
        )

    def _info(self):

        return {
            "portfolio_value": float(self.portfolio_value),
            "cash": float(self.cash),
            "holdings": float(self.holdings),
            "position_weight": float(self._position_weight()),
            "step": int(self.current_step)
        }

    def render(self):

        print(
            f"Step: {self.current_step}, "
            f"Portfolio: ${self.portfolio_value:.2f}, "
            f"Cash: ${self.cash:.2f}, "
            f"Holdings: {self.holdings:.4f}, "
            f"Position: {self._position_weight():.2%}"
        )


class RLTradingAgent:

    def __init__(self):

        self.qlib_initialized = False
        self.model = None
        self.env = None
        self.eval_env = None
        self.training_history = []

        os.makedirs(MODEL_DIR, exist_ok=True)

    def initialize_qlib(
        self,
        provider_uri: str = "~/.qlib/qlib_data/cn_data",
        region: str = "cn"
    ):

        if not RL_AVAILABLE:
            return {
                "success": False,
                "error": f"Qlib RL not available: {RL_ERROR}"
            }

        try:

            if region == "cn":
                qlib.init(
                    provider_uri=provider_uri,
                    region=REG_CN
                )
            elif region == "us":
                qlib.init(
                    provider_uri=provider_uri,
                    region=REG_US
                )
            else:
                return {
                    "success": False,
                    "error": "region must be 'cn' or 'us'"
                }

            self.qlib_initialized = True

            return {
                "success": True,
                "message": "Qlib initialized for RL"
            }

        except Exception as e:

            return {
                "success": False,
                "error": str(e)
            }

    def _generate_synthetic_data(
        self,
        start_date: str,
        end_date: str,
        seed: Optional[int] = None
    ):

        rng = np.random.default_rng(seed)

        dates = pd.bdate_range(
            start=start_date,
            end=end_date
        )

        if len(dates) < 10:
            raise ValueError(
                "Date range must contain at least 10 business days"
            )

        returns = rng.normal(
            loc=0.0002,
            scale=0.015,
            size=len(dates)
        )

        close = np.empty(len(dates))
        close[0] = 100.0

        for i in range(1, len(dates)):
            close[i] = close[i - 1] * (
                1.0 + returns[i]
            )

        close = np.maximum(close, 1.0)

        open_price = close * (
            1.0 + rng.normal(0.0, 0.003, len(dates))
        )

        high = np.maximum(
            open_price,
            close
        ) * (
            1.0 + rng.uniform(0.0, 0.01, len(dates))
        )

        low = np.minimum(
            open_price,
            close
        ) * (
            1.0 - rng.uniform(0.0, 0.01, len(dates))
        )

        volume = rng.integers(
            1_000_000,
            100_000_000,
            len(dates)
        )

        data = pd.DataFrame({
            "open": open_price,
            "high": high,
            "low": low,
            "close": close,
            "volume": volume,
            "returns": returns
        }, index=dates)

        data["vwap"] = (
            data["open"] +
            data["high"] +
            data["low"] +
            data["close"]
        ) / 4.0

        data["volatility"] = (
            data["returns"]
            .rolling(20)
            .std()
            .fillna(0.0)
        )

        delta = data["close"].diff()

        gain = delta.clip(lower=0).rolling(14).mean()
        loss = (-delta.clip(upper=0)).rolling(14).mean()

        rs = gain / loss.replace(0, np.nan)

        data["rsi"] = (
            100.0 -
            (100.0 / (1.0 + rs))
        ).fillna(50.0)

        ema12 = data["close"].ewm(
            span=12,
            adjust=False
        ).mean()

        ema26 = data["close"].ewm(
            span=26,
            adjust=False
        ).mean()

        data["macd"] = ema12 - ema26
        data["signal"] = data["macd"].ewm(
            span=9,
            adjust=False
        ).mean()

        data["bb_upper"] = (
            data["close"].rolling(20).mean() +
            2.0 * data["close"].rolling(20).std()
        )

        data["bb_lower"] = (
            data["close"].rolling(20).mean() -
            2.0 * data["close"].rolling(20).std()
        )

        data["atr"] = (
            data["high"] -
            data["low"]
        ).rolling(14).mean()

        data["adx"] = 25.0

        direction = np.sign(
            data["close"].diff()
        ).fillna(0.0)

        data["obv"] = (
            direction *
            data["volume"]
        ).cumsum()

        data = data.replace(
            [np.inf, -np.inf],
            np.nan
        )

        data = data.ffill().bfill()

        return data

    def _load_qlib_data(
        self,
        ticker: str,
        start_date: str,
        end_date: str
    ):

        if not self.qlib_initialized:
            raise RuntimeError(
                "Qlib is not initialized"
            )

        fields = [
            "$open",
            "$high",
            "$low",
            "$close",
            "$volume"
        ]

        df = D.features(
            instruments=[ticker],
            fields=fields,
            start_time=start_date,
            end_time=end_date
        )

        if df is None or df.empty:
            raise ValueError(
                f"No Qlib data available for {ticker}"
            )

        if isinstance(df.index, pd.MultiIndex):
            df = df.reset_index()

            if "instrument" in df.columns:
                df = df[
                    df["instrument"] == ticker
                ]

            if "datetime" in df.columns:
                df = df.set_index("datetime")

        rename_map = {
            "$open": "open",
            "$high": "high",
            "$low": "low",
            "$close": "close",
            "$volume": "volume"
        }

        df = df.rename(
            columns=rename_map
        )

        df["returns"] = (
            df["close"]
            .pct_change()
            .fillna(0.0)
        )

        df["vwap"] = (
            df["open"] +
            df["high"] +
            df["low"] +
            df["close"]
        ) / 4.0

        df["volatility"] = (
            df["returns"]
            .rolling(20)
            .std()
            .fillna(0.0)
        )

        delta = df["close"].diff()

        gain = delta.clip(
            lower=0
        ).rolling(14).mean()

        loss = (
            -delta.clip(upper=0)
        ).rolling(14).mean()

        rs = gain / loss.replace(
            0,
            np.nan
        )

        df["rsi"] = (
            100.0 -
            100.0 / (1.0 + rs)
        ).fillna(50.0)

        ema12 = df["close"].ewm(
            span=12,
            adjust=False
        ).mean()

        ema26 = df["close"].ewm(
            span=26,
            adjust=False
        ).mean()

        df["macd"] = ema12 - ema26

        df["signal"] = df["macd"].ewm(
            span=9,
            adjust=False
        ).mean()

        rolling_mean = df["close"].rolling(
            20
        ).mean()

        rolling_std = df["close"].rolling(
            20
        ).std()

        df["bb_upper"] = (
            rolling_mean + 2.0 * rolling_std
        )

        df["bb_lower"] = (
            rolling_mean - 2.0 * rolling_std
        )

        df["atr"] = (
            df["high"] -
            df["low"]
        ).rolling(14).mean()

        df["adx"] = 25.0

        direction = np.sign(
            df["close"].diff()
        ).fillna(0.0)

        df["obv"] = (
            direction *
            df["volume"]
        ).cumsum()

        return df.replace(
            [np.inf, -np.inf],
            np.nan
        ).ffill().bfill()

    def create_trading_env(
        self,
        tickers: Union[List[str], str],
        start_date: str,
        end_date: str,
        initial_cash: float = 1000000.0,
        commission: float = 0.001,
        slippage: float = 0.0005,
        action_space_type: str = "continuous",
        episode_length: Optional[int] = None,
        random_start: bool = False,
        use_qlib: bool = True,
        seed: Optional[int] = None
    ):

        if not STABLE_BASELINES_AVAILABLE:
            return {
                "success": False,
                "error": "Stable-Baselines3 not installed"
            }

        try:

            instruments = (
                tickers
                if isinstance(tickers, list)
                else [tickers]
            )

            if not instruments:
                raise ValueError(
                    "At least one ticker is required"
                )

            ticker = str(instruments[0])

            if use_qlib and self.qlib_initialized:

                market_data = self._load_qlib_data(
                    ticker,
                    start_date,
                    end_date
                )

            else:

                market_data = self._generate_synthetic_data(
                    start_date,
                    end_date,
                    seed=seed
                )

            market_data = market_data.replace(
                [np.inf, -np.inf],
                np.nan
            ).dropna(
                subset=[
                    "open",
                    "high",
                    "low",
                    "close",
                    "volume"
                ]
            )

            if len(market_data) < 20:
                raise ValueError(
                    "Not enough market data"
                )

            self.env = TradingEnvironment(
                market_data=market_data,
                initial_cash=initial_cash,
                commission=commission,
                slippage=slippage,
                action_space_type=action_space_type,
                episode_length=episode_length,
                random_start=random_start
            )

            self.env = Monitor(self.env)

            return {
                "success": True,
                "message": "Trading environment created",
                "ticker": ticker,
                "data_points": len(market_data),
                "action_space": action_space_type,
                "initial_cash": initial_cash,
                "using_qlib": bool(
                    use_qlib and self.qlib_initialized
                )
            }

        except Exception as e:

            return {
                "success": False,
                "error": str(e)
            }

    def _algorithm_map(self):

        return {
            "PPO": PPO,
            "DQN": DQN,
            "A2C": A2C,
            "SAC": SAC,
            "TD3": TD3
        }

    def _validate_algorithm(self, algorithm: str):

        algorithm = str(algorithm).upper()

        if algorithm not in self._algorithm_map():
            return False, f"Unknown algorithm: {algorithm}"

        action_space = self.env.action_space

        if algorithm == "DQN":
            if not isinstance(
                action_space,
                gym.spaces.Discrete
            ):
                return False, (
                    "DQN requires a discrete action space"
                )

        if algorithm in {"SAC", "TD3"}:
            if not isinstance(
                action_space,
                gym.spaces.Box
            ):
                return False, (
                    f"{algorithm} requires a continuous action space"
                )

        return True, None

    def train_agent(
        self,
        algorithm: str = "PPO",
        total_timesteps: int = 100000,
        learning_rate: float = 3e-4,
        **kwargs
    ):

        if not STABLE_BASELINES_AVAILABLE:
            return {
                "success": False,
                "error": "Stable-Baselines3 not available"
            }

        if self.env is None:
            return {
                "success": False,
                "error": (
                    "Environment not created. "
                    "Call create_trading_env first"
                )
            }

        try:

            algorithm = str(
                algorithm
            ).upper()

            valid, error = self._validate_algorithm(
                algorithm
            )

            if not valid:
                return {
                    "success": False,
                    "error": error
                }

            total_timesteps = int(total_timesteps)

            if total_timesteps <= 0:
                return {
                    "success": False,
                    "error": (
                        "total_timesteps must be greater than zero"
                    )
                }

            algo_map = self._algorithm_map()
            AlgoClass = algo_map[algorithm]

            model_kwargs = dict(kwargs)

            model_kwargs.pop(
                "env",
                None
            )

            model_kwargs.pop(
                "policy",
                None
            )

            model_kwargs.pop(
                "verbose",
                None
            )

            self.model = AlgoClass(
                "MlpPolicy",
                self.env,
                learning_rate=float(learning_rate),
                verbose=0,
                **model_kwargs
            )

            report_every = max(
                256,
                total_timesteps // 200
            )

            callback = (
                ProgressCallback(
                    total_timesteps,
                    report_every
                )
                if ProgressCallback
                else None
            )

            self.model.learn(
                total_timesteps=total_timesteps,
                callback=callback
            )

            return {
                "success": True,
                "algorithm": algorithm,
                "timesteps": total_timesteps,
                "message": (
                    f"{algorithm} agent trained successfully"
                )
            }

        except Exception as e:

            try:
                print(
                    json.dumps({
                        "event": "log",
                        "level": "error",
                        "msg": f"train_agent exception: {e}"
                    }),
                    flush=True
                )
            except Exception:
                pass

            return {
                "success": False,
                "error": str(e)
            }

    def evaluate_agent(
        self,
        n_episodes: int = 10
    ):

        if self.model is None:
            return {
                "success": False,
                "error": "No trained model available"
            }

        if self.env is None:
            return {
                "success": False,
                "error": "No environment available"
            }

        try:

            n_episodes = int(n_episodes)

            if n_episodes <= 0:
                return {
                    "success": False,
                    "error": (
                        "n_episodes must be greater than zero"
                    )
                }

            episode_rewards = []
            episode_lengths = []
            final_portfolios = []
            episode_returns = []
            episode_sharpes = []

            for episode in range(n_episodes):

                obs, _ = self.env.reset()

                done = False
                truncated = False
                episode_reward = 0.0
                steps = 0
                episode_returns_local = []

                while not done and not truncated:

                    action, _ = self.model.predict(
                        obs,
                        deterministic=True
                    )

                    obs, reward, done, truncated, info = (
                        self.env.step(action)
                    )

                    reward = float(reward)

                    if np.isfinite(reward):
                        episode_reward += reward
                        episode_returns_local.append(
                            reward
                        )

                    steps += 1

                final_value = float(
                    info.get(
                        "portfolio_value",
                        self.env.initial_cash
                    )
                )

                final_portfolios.append(
                    final_value
                )

                episode_rewards.append(
                    episode_reward
                )

                episode_lengths.append(
                    steps
                )

                episode_returns.append(
                    (
                        final_value /
                        self.env.initial_cash -
                        1.0
                    )
                )

                if len(episode_returns_local) > 1:

                    returns_array = np.asarray(
                        episode_returns_local,
                        dtype=np.float64
                    )

                    std = np.std(
                        returns_array,
                        ddof=1
                    )

                    if std > 1e-12:

                        sharpe = (
                            np.mean(
                                returns_array
                            ) /
                            std
                        ) * np.sqrt(252.0)

                    else:
                        sharpe = 0.0

                else:
                    sharpe = 0.0

                episode_sharpes.append(
                    float(sharpe)
                )

            mean_portfolio = float(
                np.mean(final_portfolios)
            )

            portfolio_return = (
                mean_portfolio /
                self.env.initial_cash -
                1.0
            ) * 100.0

            return {
                "success": True,
                "n_episodes": n_episodes,
                "mean_reward": float(
                    np.mean(episode_rewards)
                ),
                "std_reward": float(
                    np.std(episode_rewards)
                ),
                "mean_length": float(
                    np.mean(episode_lengths)
                ),
                "mean_portfolio_value": mean_portfolio,
                "portfolio_return": float(
                    portfolio_return
                ),
                "mean_sharpe": float(
                    np.mean(episode_sharpes)
                ),
                "all_rewards": [
                    float(r)
                    for r in episode_rewards
                ],
                "all_portfolio_returns": [
                    float(r * 100.0)
                    for r in episode_returns
                ]
            }

        except Exception as e:

            return {
                "success": False,
                "error": str(e)
            }

    def _safe_model_path(self, path: str):

        if not isinstance(path, str):
            raise ValueError(
                "Model path must be a string"
            )

        if not path.strip():
            raise ValueError(
                "Model path cannot be empty"
            )

        base = os.path.realpath(
            MODEL_DIR
        )

        os.makedirs(
            base,
            exist_ok=True
        )

        if os.path.isabs(path):
            candidate = os.path.realpath(path)
        else:
            candidate = os.path.realpath(
                os.path.join(
                    base,
                    path
                )
            )

        try:
            inside = (
                os.path.commonpath(
                    [base, candidate]
                ) == base
            )
        except ValueError:
            inside = False

        if not inside:
            raise ValueError(
                "Model path is outside the allowed model directory"
            )

        return candidate

    def save_model(self, path: str):

        if self.model is None:
            return {
                "success": False,
                "error": "No model to save"
            }

        try:

            safe_path = self._safe_model_path(
                path
            )

            parent = os.path.dirname(
                safe_path
            )

            os.makedirs(
                parent,
                exist_ok=True
            )

            self.model.save(
                safe_path
            )

            return {
                "success": True,
                "path": safe_path
            }

        except Exception as e:

            return {
                "success": False,
                "error": str(e)
            }

    def load_model(
        self,
        path: str,
        algorithm: str = "PPO"
    ):

        if not STABLE_BASELINES_AVAILABLE:
            return {
                "success": False,
                "error": (
                    "Stable-Baselines3 not available"
                )
            }

        try:

            algorithm = str(
                algorithm
            ).upper()

            algo_map = self._algorithm_map()

            if algorithm not in algo_map:
                return {
                    "success": False,
                    "error": (
                        f"Unknown algorithm: {algorithm}"
                    )
                }

            safe_path = self._safe_model_path(
                path
            )

            if not os.path.exists(
                safe_path
            ):
                return {
                    "success": False,
                    "error": "Model file does not exist"
                }

            if not os.path.isfile(
                safe_path
            ):
                return {
                    "success": False,
                    "error": "Model path is not a file"
                }

            AlgoClass = algo_map[
                algorithm
            ]

            if self.env is not None:

                self.model = AlgoClass.load(
                    safe_path,
                    env=self.env
                )

            else:

                self.model = AlgoClass.load(
                    safe_path
                )

            return {
                "success": True,
                "path": safe_path,
                "algorithm": algorithm
            }

        except Exception as e:

            return {
                "success": False,
                "error": str(e)
            }

    def get_available_algorithms(self):

        algorithms = {
            "PPO": (
                "Proximal Policy Optimization - "
                "Best for continuous action spaces"
            ),
            "A2C": (
                "Advantage Actor-Critic - "
                "Fast training, good baseline"
            ),
            "DQN": (
                "Deep Q-Network - "
                "For discrete action spaces"
            ),
            "SAC": (
                "Soft Actor-Critic - "
                "Off-policy, continuous actions"
            ),
            "TD3": (
                "Twin Delayed DDPG - "
                "Robust continuous control"
            )
        }

        return {
            "success": True,
            "algorithms": algorithms,
            "stable_baselines_available": (
                STABLE_BASELINES_AVAILABLE
            ),
            "qlib_rl_available": RL_AVAILABLE
        }


def main():

    if len(sys.argv) < 2:

        print(
            json.dumps({
                "success": False,
                "error": (
                    "Usage: python qlib_rl.py "
                    "<command> [args...]"
                )
            })
        )

        return

    command = sys.argv[1]
    agent = RLTradingAgent()

    if command == "list_algorithms":

        result = agent.get_available_algorithms()

    elif command == "initialize":

        provider_uri = (
            sys.argv[2]
            if len(sys.argv) > 2
            else "~/.qlib/qlib_data/cn_data"
        )

        region = (
            sys.argv[3]
            if len(sys.argv) > 3
            else "cn"
        )

        result = agent.initialize_qlib(
            provider_uri,
            region
        )

    elif command == "create_env":

        try:

            params = (
                json.loads(sys.argv[2])
                if len(sys.argv) > 2
                else {}
            )

            result = agent.create_trading_env(
                **params
            )

        except Exception as e:

            result = {
                "success": False,
                "error": str(e)
            }

    elif command == "train":

        try:

            params = (
                json.loads(sys.argv[2])
                if len(sys.argv) > 2
                else {}
            )

            if "ticker" in params:
                params["tickers"] = params.pop(
                    "ticker"
                )

            if "initial_capital" in params:
                params["initial_cash"] = params.pop(
                    "initial_capital"
                )

            if "episodes" in params:

                episodes = int(
                    params.pop("episodes")
                )

                params.setdefault(
                    "total_timesteps",
                    episodes * 252
                )

            env_keys = [
                "tickers",
                "initial_cash",
                "start_date",
                "end_date",
                "commission",
                "slippage",
                "action_space_type",
                "episode_length",
                "random_start",
                "use_qlib",
                "seed"
            ]

            env_params = {}

            for key in env_keys:

                if key in params:
                    env_params[key] = params.pop(
                        key
                    )

            env_params.setdefault(
                "tickers",
                ["AAPL"]
            )

            env_params.setdefault(
                "initial_cash",
                100000
            )

            env_params.setdefault(
                "start_date",
                "2022-01-01"
            )

            env_params.setdefault(
                "end_date",
                "2024-01-01"
            )

            env_params.setdefault(
                "use_qlib",
                False
            )

            env_result = (
                agent.create_trading_env(
                    **env_params
                )
            )

            if not env_result.get(
                "success"
            ):

                result = env_result

            else:

                result = agent.train_agent(
                    **params
                )

        except Exception as e:

            result = {
                "success": False,
                "error": str(e)
            }

    elif command == "evaluate":

        try:

            n_episodes = (
                int(sys.argv[2])
                if len(sys.argv) > 2
                else 10
            )

            result = agent.evaluate_agent(
                n_episodes
            )

        except Exception as e:

            result = {
                "success": False,
                "error": str(e)
            }

    elif command == "save_model":

        path = (
            sys.argv[2]
            if len(sys.argv) > 2
            else "rl_model"
        )

        result = agent.save_model(
            path
        )

    elif command == "load_model":

        path = (
            sys.argv[2]
            if len(sys.argv) > 2
            else "rl_model"
        )

        algorithm = (
            sys.argv[3]
            if len(sys.argv) > 3
            else "PPO"
        )

        result = agent.load_model(
            path,
            algorithm
        )

    else:

        result = {
            "success": False,
            "error": (
                f"Unknown command: {command}"
            )
        }

    payload = {
        "event": "result"
    }

    payload.update(
        result
    )

    print(
        json.dumps(
            payload,
            allow_nan=False
        ),
        flush=True
    )


if __name__ == "__main__":
    main()
