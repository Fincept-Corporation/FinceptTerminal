"""
Configuration Manager for Economic Agents
"""

import json
import os
from typing import Dict, Any, Optional
from pathlib import Path
import yaml

class EconomicConfig:
    """Configuration manager for economic agent settings"""

    def __init__(self, config_file: Optional[str] = None):
        self.config_file = config_file or self._get_default_config_path()
        self.config = self._load_default_config()
        self.load_config()

    def _get_default_config_path(self) -> str:
        """Get default configuration file path"""
        # Try multiple locations
        possible_paths = [
            os.path.expanduser("~/.economic_agents/config.json"),
            os.path.join(os.getcwd(), "config.json"),
            os.path.join(os.path.dirname(__file__), "default_config.json")
        ]

        for path in possible_paths:
            if os.path.exists(path):
                return path

        # Return the first one for creation
        return possible_paths[0]

    def _load_default_config(self) -> Dict[str, Any]:
        """Load default configuration"""
        return {
            "analysis": {
                "default_weights": {
                    "market_efficiency": 0.25,
                    "social_welfare": 0.25,
                    "economic_stability": 0.25,
                    "growth_potential": 0.25
                },
                "scoring_ranges": {
                    "gdp_growth": {"min": -10, "max": 20},
                    "inflation": {"min": -5, "max": 25},
                    "unemployment": {"min": 0, "max": 30},
                    "interest_rate": {"min": 0, "max": 25},
                    "tax_rate": {"min": 0, "max": 80},
                    "government_spending": {"min": 0, "max": 100},
                    "private_investment": {"min": 0, "max": 80},
                    "consumer_confidence": {"min": 0, "max": 150}
                },
                "thresholds": {
                    "excellent": 80,
                    "good": 60,
                    "moderate": 40,
                    "poor": 0
                }
            },
            "agents": {
                "capitalism": {
                    "enabled": True,
                    "weights": {
                        "market_efficiency": 0.4,
                        "investment_attractiveness": 0.3,
                        "economic_freedom": 0.2,
                        "competitiveness": 0.1
                    },
                    "parameters": {
                        "optimal_tax_rate": 25,
                        "optimal_spending": 20,
                        "inflation_target": 2.5
                    }
                },
                "socialism": {
                    "enabled": True,
                    "weights": {
                        "equality_index": 0.3,
                        "social_welfare": 0.3,
                        "workers_protection": 0.2,
                        "public_services": 0.2
                    },
                    "parameters": {
                        "optimal_tax_rate": 45,
                        "optimal_spending": 40,
                        "unemployment_target": 4
                    }
                },
                "mixed_economy": {
                    "enabled": True,
                    "weights": {
                        "balance_score": 0.3,
                        "stability_score": 0.25,
                        "efficiency_equity": 0.25,
                        "regulatory_effectiveness": 0.2
                    },
                    "parameters": {
                        "optimal_tax_rate": 30,
                        "optimal_spending": 35,
                        "inflation_target": 2.5
                    }
                },
                "keynesian": {
                    "enabled": True,
                    "weights": {
                        "aggregate_demand": 0.3,
                        "fiscal_stance": 0.25,
                        "monetary_stance": 0.25,
                        "employment_gap": 0.2
                    },
                    "parameters": {
                        "target_unemployment": 5,
                        "target_inflation": 2.5,
                        "recession_threshold": 1.5
                    }
                },
                "neoliberal": {
                    "enabled": True,
                    "weights": {
                        "market_freedom": 0.35,
                        "liberalization": 0.25,
                        "fiscal_discipline": 0.25,
                        "globalization": 0.15
                    },
                    "parameters": {
                        "optimal_tax_rate": 20,
                        "optimal_spending": 15,
                        "inflation_target": 2
                    }
                },
                "mercantilism": {
                    "enabled": True,
                    "weights": {
                        "trade_balance": 0.35,
                        "self_sufficiency": 0.25,
                        "protection_score": 0.25,
                        "national_accumulation": 0.15
                    },
                    "parameters": {
                        "optimal_trade_surplus": 5,
                        "target_self_sufficiency": 70
                    }
                }
            },
            "data_sources": {
                "default_format": "json",
                "date_format": "%Y-%m-%d",
                "interpolation_method": "linear",
                "validation_level": "standard"
            },
            "output": {
                "default_format": "text",
                "decimal_places": 2,
                "include_timestamps": True,
                "color_output": True
            },
            "comparison": {
                "weights": {
                    "health_score": 0.3,
                    "policy_alignment": 0.25,
                    "consistency": 0.2,
                    "feasibility": 0.15,
                    "stability": 0.1
                },
                "consensus_threshold": 60,
                "divergence_threshold": 30
            },
            "api": {
                "default_timeout": 30,
                "retry_attempts": 3,
                "rate_limit": 100
            }
        }

    def load_config(self):
        """Load configuration from file"""
        if not os.path.exists(self.config_file):
            self.save_config()  # Create default config file
            return

        try:
            with open(self.config_file, 'r') as f:
                if self.config_file.endswith('.yaml') or self.config_file.endswith('.yml'):
                    file_config = yaml.safe_load(f)
                else:
                    file_config = json.load(f)

            # Merge with default config
            self._deep_update(self.config, file_config)

        except Exception as e:
            print(f"Warning: Could not load config file {self.config_file}: {e}")
            print("Using default configuration.")

    def save_config(self):
        """Save current configuration to file"""
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(self.config_file), exist_ok=True)

        try:
            with open(self.config_file, 'w') as f:
                if self.config_file.endswith('.yaml') or self.config_file.endswith('.yml'):
                    yaml.dump(self.config, f, default_flow_style=False, indent=2)
                else:
                    json.dump(self.config, f, indent=2)

        except Exception as e:
            print(f"Error saving config file: {e}")

    def get(self, key: str, default: Any = None) -> Any:
        """Get configuration value using dot notation"""
        keys = key.split('.')
        value = self.config

        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default

        return value

    def set(self, key: str, value: Any):
        """Set configuration value using dot notation"""
        keys = key.split('.')
        config = self.config

        for k in keys[:-1]:
            if k not in config:
                config[k] = {}
            config = config[k]

        config[keys[-1]] = value

    def get_agent_config(self, agent_name: str) -> Dict[str, Any]:
        """Get configuration for specific agent"""
        return self.get(f'agents.{agent_name}', {})

    def get_agent_weights(self, agent_name: str) -> Dict[str, float]:
        """Get weights for specific agent"""
        agent_config = self.get_agent_config(agent_name)
        return agent_config.get('weights', {})

    def get_agent_parameters(self, agent_name: str) -> Dict[str, Any]:
        """Get parameters for specific agent"""
        agent_config = self.get_agent_config(agent_name)
        return agent_config.get('parameters', {})

    def is_agent_enabled(self, agent_name: str) -> bool:
        """Check if agent is enabled"""
        return self.get(f'agents.{agent_name}.enabled', True)

    def enable_agent(self, agent_name: str):
        """Enable specific agent"""
        self.set(f'agents.{agent_name}.enabled', True)

    def disable_agent(self, agent_name: str):
        """Disable specific agent"""
        self.set(f'agents.{agent_name}.enabled', False)

    def update_agent_weights(self, agent_name: str, weights: Dict[str, float]):
        """Update weights for specific agent"""
        self.set(f'agents.{agent_name}.weights', weights)

    def update_agent_parameters(self, agent_name: str, parameters: Dict[str, Any]):
        """Update parameters for specific agent"""
        self.set(f'agents.{agent_name}.parameters', parameters)

    def get_output_config(self) -> Dict[str, Any]:
        """Get output configuration"""
        return self.get('output', {})

    def set_output_config(self, config: Dict[str, Any]):
        """Set output configuration"""
        for key, value in config.items():
            self.set(f'output.{key}', value)

    def get_data_source_config(self) -> Dict[str, Any]:
        """Get data source configuration"""
        return self.get('data_sources', {})

    def set_data_source_config(self, config: Dict[str, Any]):
        """Set data source configuration"""
        for key, value in config.items():
            self.set(f'data_sources.{key}', value)

    def get_comparison_config(self) -> Dict[str, Any]:
        """Get comparison configuration"""
        return self.get('comparison', {})

    def get_analysis_config(self) -> Dict[str, Any]:
        """Get analysis configuration"""
        return self.get('analysis', {})

    def reset_to_defaults(self):
        """Reset configuration to defaults"""
        self.config = self._load_default_config()

    def validate_config(self) -> List[str]:
        """Validate configuration and return list of issues"""
        issues = []

        # Check agent configurations
        for agent_name in ['capitalism', 'socialism', 'mixed_economy', 'keynesian', 'neoliberal', 'mercantilism']:
            agent_config = self.get_agent_config(agent_name)

            if not agent_config:
                issues.append(f"Missing configuration for agent: {agent_name}")
                continue

            # Check weights sum to 1.0 (approximately)
            weights = agent_config.get('weights', {})
            if weights:
                weight_sum = sum(weights.values())
                if abs(weight_sum - 1.0) > 0.1:
                    issues.append(f"Agent {agent_name} weights don't sum to 1.0: {weight_sum}")

            # Check required parameters exist
            required_params = self._get_required_parameters(agent_name)
            parameters = agent_config.get('parameters', {})
            for param in required_params:
                if param not in parameters:
                    issues.append(f"Agent {agent_name} missing required parameter: {param}")

        # Check analysis configuration
        analysis_config = self.get_analysis_config()
        thresholds = analysis_config.get('thresholds', {})
        if not thresholds:
            issues.append("Missing analysis thresholds configuration")

        # Check data source configuration
        data_config = self.get_data_source_config()
        if not data_config.get('default_format'):
            issues.append("Missing default data format configuration")

        return issues

    def _get_required_parameters(self, agent_name: str) -> List[str]:
        """Get list of required parameters for agent"""
        required_params = {
            'capitalism': ['optimal_tax_rate', 'optimal_spending'],
            'socialism': ['optimal_tax_rate', 'optimal_spending'],
            'mixed_economy': ['optimal_tax_rate', 'optimal_spending'],
            'keynesian': ['target_unemployment', 'target_inflation'],
            'neoliberal': ['optimal_tax_rate', 'optimal_spending'],
            'mercantilism': ['optimal_trade_surplus']
        }
        return required_params.get(agent_name, [])

    def _deep_update(self, base_dict: Dict, update_dict: Dict):
        """Deep update dictionary"""
        for key, value in update_dict.items():
            if key in base_dict and isinstance(base_dict[key], dict) and isinstance(value, dict):
                self._deep_update(base_dict[key], value)
            else:
                base_dict[key] = value

    def export_config(self, filepath: str):
        """Export configuration to file"""
        try:
            with open(filepath, 'w') as f:
                if filepath.endswith('.yaml') or filepath.endswith('.yml'):
                    yaml.dump(self.config, f, default_flow_style=False, indent=2)
                else:
                    json.dump(self.config, f, indent=2)
            print(f"Configuration exported to {filepath}")
        except Exception as e:
            print(f"Error exporting configuration: {e}")

    def import_config(self, filepath: str):
        """Import configuration from file"""
        try:
            with open(filepath, 'r') as f:
                if filepath.endswith('.yaml') or filepath.endswith('.yml'):
                    imported_config = yaml.safe_load(f)
                else:
                    imported_config = json.load(f)

            self._deep_update(self.config, imported_config)
            print(f"Configuration imported from {filepath}")
        except Exception as e:
            print(f"Error importing configuration: {e}")

    def get_summary(self) -> str:
        """Get configuration summary"""
        summary = []
        summary.append("Economic Agents Configuration Summary:")
        summary.append("-" * 40)

        # Enabled agents
        enabled_agents = []
        for agent_name in ['capitalism', 'socialism', 'mixed_economy', 'keynesian', 'neoliberal', 'mercantilism']:
            if self.is_agent_enabled(agent_name):
                enabled_agents.append(agent_name)

        summary.append(f"Enabled Agents: {', '.join(enabled_agents)}")
        summary.append(f"Config File: {self.config_file}")
        summary.append(f"Default Output Format: {self.get('output.default_format', 'text')}")
        summary.append(f"Default Data Format: {self.get('data_sources.default_format', 'json')}")
        summary.append(f"Analysis Thresholds: Excellent={self.get('analysis.thresholds.excellent')}, "
                     f"Good={self.get('analysis.thresholds.good')}, "
                     f"Moderate={self.get('analysis.thresholds.moderate')}")

        return "\n".join(summary)


# Global configuration instance
_config_instance = None

def get_config() -> EconomicConfig:
    """Get global configuration instance"""
    global _config_instance
    if _config_instance is None:
        _config_instance = EconomicConfig()
    return _config_instance

def reload_config():
    """Reload global configuration"""
    global _config_instance
    if _config_instance is not None:
        _config_instance.load_config()
    else:
        _config_instance = EconomicConfig()

def save_config():
    """Save global configuration"""
    global _config_instance
    if _config_instance is not None:
        _config_instance.save_config()