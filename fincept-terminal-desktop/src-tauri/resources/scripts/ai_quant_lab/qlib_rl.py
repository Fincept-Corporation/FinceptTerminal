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
            row.get('close', 0) / 100,  # Normalized price
            row.get('volume', 0) / 1e6,  # Normalized volume
            row.get('open', 0) / 100,
            row.get('high', 0) / 100,
            row.get('low', 0) / 100,
            row.get('vwap', 0) / 100,
            row.get('returns', 0),
            row.get('volatility', 0),
            row.get('rsi', 50) / 100,
            row.get('macd', 0),
            row.get('signal', 0),
            row.get('bb_upper', 0) / 100,
            row.get('bb_lower', 0) / 100,
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
            # action is target position size (-1 to +1)
            target_position = float(action[0])
            position_change = target_position - self.holdings

            if position_change > 0:  # Buy
                shares_to_buy = (self.cash * abs(position_change)) / current_price
                cost = shares_to_buy * current_price * (1 + self.commission)
                if cost <= self.cash:
                    self.holdings += shares_to_buy
                    self.cash -= cost
                    self.trades.append({'step': self.current_step, 'action': 'buy', 'shares': shares_to_buy, 'price': current_price})

            elif position_change < 0:  # Sell
                shares_to_sell = self.holdings * abs(position_change)
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

        # Add Sharpe ratio component
        if len(self.trades) > 10:
            returns = [t.get('price', 0) for t in self.trades[-10:]]
            if len(returns) > 1:
                sharpe = np.mean(returns) / (np.std(returns) + 1e-6)
                reward += sharpe * 0.1

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

    def create_trading_env(self,
                          tickers: List[str],
                          start_date: str,
                          end_date: str,
                          initial_cash: float = 1000000.0,
                          commission: float = 0.001,
                          action_space_type: str = 'continuous') -> Dict[str, Any]:
        """Create trading environment with market data"""
        if not STABLE_BASELINES_AVAILABLE:
            return {'success': False, 'error': 'Stable-Baselines3 not installed'}

        try:
            # Fetch market data from Qlib
            instruments = tickers if isinstance(tickers, list) else [tickers]

            # For demo, generate synthetic data
            dates = pd.date_range(start=start_date, end=end_date, freq='D')
            data = {
                'close': np.random.randn(len(dates)).cumsum() + 100,
                'open': np.random.randn(len(dates)).cumsum() + 100,
                'high': np.random.randn(len(dates)).cumsum() + 102,
                'low': np.random.randn(len(dates)).cumsum() + 98,
                'volume': np.random.randint(1e6, 1e8, len(dates)),
                'returns': np.random.randn(len(dates)) * 0.02,
                'volatility': np.abs(np.random.randn(len(dates)) * 0.01),
                'rsi': np.random.uniform(30, 70, len(dates)),
                'macd': np.random.randn(len(dates)) * 0.5,
                'signal': np.random.randn(len(dates)) * 0.5,
            }
            market_data = pd.DataFrame(data, index=dates)

            # Create environment
            self.env = TradingEnvironment(
                market_data=market_data,
                initial_cash=initial_cash,
                commission=commission,
                action_space_type=action_space_type
            )

            return {
                'success': True,
                'message': 'Trading environment created',
                'data_points': len(market_data),
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

            # Create model
            self.model = AlgoClass(
                'MlpPolicy',
                self.env,
                learning_rate=learning_rate,
                verbose=1,
                **kwargs
            )

            # Train
            self.model.learn(total_timesteps=total_timesteps)

            return {
                'success': True,
                'algorithm': algorithm,
                'timesteps': total_timesteps,
                'message': f'{algorithm} agent trained successfully'
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def evaluate_agent(self, n_episodes: int = 10) -> Dict[str, Any]:
        """Evaluate trained RL agent"""
        if self.model is None:
            return {'success': False, 'error': 'No trained model available'}

        try:
            episode_rewards = []
            episode_lengths = []
            final_portfolios = []

            for episode in range(n_episodes):
                obs, _ = self.env.reset()
                done = False
                episode_reward = 0
                steps = 0

                while not done:
                    action, _ = self.model.predict(obs, deterministic=True)
                    obs, reward, done, truncated, info = self.env.step(action)
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
                'portfolio_return': float((np.mean(final_portfolios) / self.env.initial_cash - 1) * 100),
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
        result = agent.train_agent(**params)

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

    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
