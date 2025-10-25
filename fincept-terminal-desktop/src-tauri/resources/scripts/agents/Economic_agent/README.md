# Economic Agents Package

A comprehensive Python package for analyzing economic data through different ideological perspectives. This package provides agents representing various economic ideologies (Capitalism, Socialism, Mixed Economy, Keynesian, Neoliberal, Mercantilism) that can analyze economic indicators and provide policy recommendations based on their ideological principles.

## Features

- **Multiple Economic Ideologies**: 6 different economic agents with unique perspectives
- **Comprehensive Analysis**: GDP, inflation, unemployment, trade balance, and more
- **Policy Recommendations**: Ideology-specific policy suggestions
- **Data Processing**: Support for JSON, CSV, XML, Excel, and SQLite formats
- **Agent Comparison**: Compare analyses from different ideological perspectives
- **CLI Interface**: Easy-to-use command-line interface
- **Configuration Management**: Customizable agent parameters and weights

## Available Agents

| Agent | Ideology | Focus Areas |
|--------|-----------|-------------|
| `CapitalismAgent` | Capitalism | Market efficiency, private property, competition |
| `SocialismAgent` | Socialism | Economic equality, social welfare, workers' rights |
| `MixedEconomyAgent` | Mixed Economy | Balance of public and private sectors |
| `KeynesianAgent` | Keynesian Economics | Demand management, counter-cyclical policies |
| `NeoliberalAgent` | Neoliberalism | Deregulation, privatization, free markets |
| `MercantilistAgent` | Mercantilism | Trade surplus, protectionism, national wealth |

## Installation

```bash
# Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git

# Navigate to the economic agents directory
cd FinceptTerminal/src-tauri/resources/scripts/agents/Economic_agent

# Install required dependencies
pip install pandas requests pyyaml
```

## Quick Start

### Basic Usage

```python
from economicagents import get_agent, EconomicData
from datetime import datetime

# Create economic data
data = EconomicData(
    gdp=3.2,                    # GDP growth rate (%)
    inflation=2.8,              # Inflation rate (%)
    unemployment=5.5,            # Unemployment rate (%)
    interest_rate=4.2,          # Interest rate (%)
    trade_balance=2.1,          # Trade balance (% of GDP, positive = surplus)
    government_spending=32.5,   # Government spending (% of GDP)
    tax_rate=28.0,              # Average tax rate (%)
    private_investment=22.3,    # Private investment (% of GDP)
    consumer_confidence=67.8,   # Consumer confidence index (0-100)
    timestamp=datetime.now()
)

# Get specific agent
capitalist_agent = get_agent('capitalism')

# Analyze economy
analysis = capitalist_agent.analyze_economy(data)
print(f"Health Score: {analysis['overall_health_score']}/100")
print(f"Assessment: {analysis['assessment']}")

# Get policy recommendations
policies = capitalist_agent.get_policy_recommendations(data, analysis)
for policy in policies:
    print(f"Policy: {policy.title}")
    print(f"Priority: {policy.priority}")
    print(f"Impact: {policy.expected_impact}")
```

### Compare Multiple Agents

```python
from economicagents import get_all_agents

# Get all available agents
agents = get_all_agents()

# Compare analyses
comparison_result = {}
for name, agent in agents.items():
    analysis = agent.analyze_economy(data)
    comparison_result[name] = {
        'health_score': analysis['overall_health_score'],
        'assessment': analysis['assessment']
    }

# Print comparison
for ideology, result in comparison_result.items():
    print(f"{ideology}: {result['health_score']:.1f}/100 - {result['assessment']}")
```

### Using the Comparison Module

```python
from economicagents import EconomicAgentComparator

# Create comparator
comparator = EconomicAgentComparator()

# Compare all agents
comparison = comparator.compare_agents(agents, data)

# View comparison results
print(f"Average Health Score: {comparison['comparison_metrics']['average_health_score']:.1f}")
print(f"Most Optimistic: {comparison['comparison_metrics']['most_optimistic']}")
print(f"Most Pessimistic: {comparison['comparison_metrics']['most_pessimistic']}")
```

## Command Line Interface

The package includes a comprehensive CLI for easy usage:

```bash
# List available agents
python -m economicagents.cli list-agents

# Analyze with specific agent
python -m economicagents.cli analyze --agent capitalism --data '{"gdp": 3.2, "inflation": 2.8}'

# Compare multiple agents
python -m economicagents.cli compare --all-agents --file data.json

# Get policy recommendations
python -m economicagents.cli policies --agent keynesian --data '{"gdp": 1.5, "unemployment": 8.2}'

# Generate comprehensive report
python -m economicagents.cli report --file time_series_data.csv --output report.json

# Interactive mode
python -m economicagents.cli interactive
```

## Data Processing

### Loading Data from Files

```python
from economicagents import EconomicDataProcessor

processor = EconomicDataProcessor()

# Load from JSON
data = processor.load_from_json('economic_data.json')

# Load from CSV
data_points = processor.load_from_csv('time_series.csv')

# Load from Excel
data_points = processor.load_from_excel('data.xlsx', sheet_name='EconomicData')
```

### Data Validation

```python
from economicagents import validate_economic_data

issues = validate_economic_data(data)
if issues:
    print("Data validation issues:")
    for issue in issues:
        print(f"  - {issue}")
else:
    print("Data validation passed!")
```

## Configuration

Customize agent behavior using the configuration system:

```python
from economicagents import EconomicConfig

# Create or load configuration
config = EconomicConfig()

# Update agent weights
config.update_agent_weights('capitalism', {
    'market_efficiency': 0.5,
    'investment_attractiveness': 0.3,
    'economic_freedom': 0.2
})

# Update agent parameters
config.update_agent_parameters('keynesian', {
    'target_unemployment': 4.5,
    'target_inflation': 2.0
})

# Save configuration
config.save_config()
```

## Economic Data Structure

The `EconomicData` class includes the following indicators:

| Field | Type | Description | Range |
|-------|------|-------------|--------|
| `gdp` | float | GDP growth rate (%) | -20 to 20 |
| `inflation` | float | Inflation rate (%) | -10 to 50 |
| `unemployment` | float | Unemployment rate (%) | 0 to 50 |
| `interest_rate` | float | Interest rate (%) | 0 to 50 |
| `trade_balance` | float | Trade balance (% of GDP) | -50 to 50 |
| `government_spending` | float | Government spending (% of GDP) | 0 to 100 |
| `tax_rate` | float | Average tax rate (%) | 0 to 100 |
| `private_investment` | float | Private investment (% of GDP) | 0 to 100 |
| `consumer_confidence` | float | Consumer confidence (0-100) | 0 to 150 |
| `timestamp` | datetime | Data timestamp | - |

## API Reference

### Core Classes

#### `EconomicAgent` (Abstract Base Class)

```python
class EconomicAgent:
    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]
    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]
    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]
```

#### `EconomicData`

```python
@dataclass
class EconomicData:
    gdp: float
    inflation: float
    unemployment: float
    interest_rate: float
    trade_balance: float
    government_spending: float
    tax_rate: float
    private_investment: float
    consumer_confidence: float
    timestamp: datetime
```

#### `PolicyRecommendation`

```python
@dataclass
class PolicyRecommendation:
    title: str
    description: str
    priority: str  # high, medium, low
    expected_impact: str
    implementation_time: str
    risk_level: str
```

### Utility Functions

```python
# Data validation
validate_economic_data(data: EconomicData) -> List[str]

# Score calculations
normalize_score(value: float, min_val: float, max_val: float) -> float
calculate_composite_score(scores: List[float], weights: List[float]) -> float

# Export utilities
export_to_json(data: Any, filename: str) -> bool
export_to_csv(data: List[Dict], filename: str) -> bool
```

## Examples

### Example 1: Basic Analysis

```python
from economicagents import CapitalismAgent, EconomicData
from datetime import datetime

# Sample data for a growing economy
data = EconomicData(
    gdp=4.1, inflation=3.2, unemployment=4.8, interest_rate=5.2,
    trade_balance=3.5, government_spending=28.5, tax_rate=26.0,
    private_investment=24.3, consumer_confidence=72.1,
    timestamp=datetime.now()
)

# Capitalist analysis
capitalist = CapitalismAgent()
analysis = capitalist.analyze_economy(data)

print(f"Capitalist Analysis:")
print(f"Health Score: {analysis['overall_health_score']:.1f}/100")
print(f"Market Efficiency: {analysis['market_efficiency']:.1f}/100")
print(f"Assessment: {analysis['assessment']}")
```

### Example 2: Recession Scenario Analysis

```python
from economicagents import KeynesianAgent

# Sample recession data
recession_data = EconomicData(
    gdp=1.2, inflation=1.5, unemployment=8.5, interest_rate=1.8,
    trade_balance=-4.2, government_spending=38.5, tax_rate=35.0,
    private_investment=15.3, consumer_confidence=42.1,
    timestamp=datetime.now()
)

# Keynesian analysis
keynesian = KeynesianAgent()
analysis = keynesian.analyze_economy(recession_data)

print(f"Keynesian Analysis for Recession:")
print(f"Economic Phase: {analysis['economic_phase']}")
print(f"Health Score: {analysis['overall_health_score']:.1f}/100")

# Get specific recession policies
policies = keynesian.get_policy_recommendations(recession_data, analysis)
print(f"\nRecommended Policies:")
for policy in policies[:3]:
    print(f"  • {policy.title} ({policy.priority.upper()})")
```

### Example 3: Comprehensive Agent Comparison

```python
from economicagents import EconomicAgentComparator

# Create comparison
comparator = EconomicAgentComparator()

# Analyze economic crisis data
crisis_data = EconomicData(
    gdp=0.5, inflation=0.8, unemployment=12.3, interest_rate=0.5,
    trade_balance=-8.7, government_spending=42.1, tax_rate=38.0,
    private_investment=12.8, consumer_confidence=31.2,
    timestamp=datetime.now()
)

# Compare all agents
agents = get_all_agents()
comparison = comparator.compare_agents(agents, crisis_data)

print(f"CRISIS SCENARIO COMPARISON")
print(f"Average Health Score: {comparison['comparison_metrics']['average_health_score']:.1f}/100")
print(f"Health Score Range: {comparison['comparison_metrics']['health_score_range']:.1f} points")
print(f"Most Optimistic Agent: {comparison['comparison_metrics']['most_optimistic']}")
print(f"Most Pessimistic Agent: {comparison['comparison_metrics']['most_pessimistic']}")
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Adding New Economic Agents

To add a new economic ideology agent:

1. Create a new agent class inheriting from `EconomicAgent`
2. Implement all abstract methods:
   - `analyze_economy()`
   - `get_policy_recommendations()`
   - `evaluate_policy()`

```python
from economicagents.base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class CustomEconomicsAgent(EconomicAgent):
    def __init__(self):
        super().__init__(
            "Custom Economics Agent",
            "Description of your economic ideology"
        )
        self.principles = ["Your", "principles", "here"]
        self.priorities = ["Your", "priorities", "here"]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        # Implement your analysis logic
        pass

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        # Implement your policy recommendation logic
        pass

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        # Implement your policy evaluation logic
        pass
```

3. Add your agent to the factory function in `__init__.py`
4. Update configuration to include your agent
5. Add tests and documentation

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

For questions and support:

- Open an issue on GitHub
- Check the [documentation](https://github.com/Fincept-Corporation/FinceptTerminal/docs)
- Review example usage in the `examples/` directory

## Changelog

### Version 1.0.0
- Initial release with 6 economic ideology agents
- Comprehensive CLI interface
- Data processing and comparison modules
- Configuration management system
- Full documentation and examples