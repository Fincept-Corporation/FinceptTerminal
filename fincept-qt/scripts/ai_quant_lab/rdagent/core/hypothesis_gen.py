"""
Hypothesis Generator for RD-Agent
Automated hypothesis generation for quantitative research
"""

import json
from typing import List, Dict, Any, Optional
from dataclasses import dataclass, asdict
from datetime import datetime
import random


@dataclass
class Hypothesis:
    """Research hypothesis"""
    id: str
    description: str
    rationale: str
    expected_outcome: str
    confidence: float  # 0.0 to 1.0
    priority: str  # 'high', 'medium', 'low'
    category: str  # 'factor', 'model', 'strategy', 'feature'
    created_at: str
    status: str = 'pending'  # 'pending', 'testing', 'validated', 'rejected'
    metadata: Dict[str, Any] = None

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


class HypothesisGenerator:
    """
    Generates research hypotheses for quantitative trading
    Uses templates, market knowledge, and historical patterns
    """

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        self.config = config or {}
        self.templates = self._load_templates()
        self.hypotheses: List[Hypothesis] = []

    def _load_templates(self) -> Dict[str, List[Dict[str, str]]]:
        """Load hypothesis templates"""
        return {
            'factor': [
                {
                    'template': 'Momentum effect exists in {market} for {timeframe}',
                    'rationale': 'Historical price trends tend to continue in the short term',
                    'expected': 'Positive correlation between past returns and future returns'
                },
                {
                    'template': 'Mean reversion occurs after {threshold}% moves in {asset_class}',
                    'rationale': 'Extreme price movements are often followed by corrections',
                    'expected': 'Negative correlation after extreme price movements'
                },
                {
                    'template': 'Volume spike predicts {direction} movement in next {period}',
                    'rationale': 'Unusual volume indicates institutional activity',
                    'expected': 'Volume changes lead price movements'
                },
                {
                    'template': 'Volatility clustering in {market} affects predictability',
                    'rationale': 'High volatility periods tend to cluster together',
                    'expected': 'GARCH effects improve forecasting accuracy'
                },
                {
                    'template': 'Cross-sectional momentum in {sector} outperforms market',
                    'rationale': 'Sector rotation follows momentum patterns',
                    'expected': 'Top performers continue outperforming within sector'
                }
            ],
            'model': [
                {
                    'template': '{model_type} outperforms linear models for {prediction_task}',
                    'rationale': 'Non-linear patterns exist in financial data',
                    'expected': 'Higher accuracy and better risk-adjusted returns'
                },
                {
                    'template': 'Ensemble of {n} models reduces overfitting',
                    'rationale': 'Model diversity improves generalization',
                    'expected': 'More stable out-of-sample performance'
                },
                {
                    'template': 'Attention mechanism improves {feature} importance',
                    'rationale': 'Not all features are equally important at all times',
                    'expected': 'Dynamic feature weighting improves predictions'
                },
                {
                    'template': 'Transfer learning from {source_market} to {target_market}',
                    'rationale': 'Similar market dynamics across regions',
                    'expected': 'Reduced training time and improved accuracy'
                }
            ],
            'strategy': [
                {
                    'template': 'Long-short {factor} strategy generates alpha',
                    'rationale': 'Market neutral approach reduces systematic risk',
                    'expected': 'Positive risk-adjusted returns with low correlation'
                },
                {
                    'template': 'Dynamic position sizing based on {signal} improves Sharpe',
                    'rationale': 'Bet sizing should reflect signal strength',
                    'expected': 'Higher Sharpe ratio than fixed position sizing'
                },
                {
                    'template': 'Multi-factor combination beats single factors',
                    'rationale': 'Diversification across factors reduces risk',
                    'expected': 'More consistent returns across market regimes'
                }
            ],
            'feature': [
                {
                    'template': '{technical_indicator} has predictive power for {horizon}',
                    'rationale': 'Technical patterns reflect market psychology',
                    'expected': 'Statistically significant correlation with future returns'
                },
                {
                    'template': 'Fundamental ratio {ratio} predicts long-term returns',
                    'rationale': 'Value investing principles',
                    'expected': 'Outperformance over extended holding periods'
                },
                {
                    'template': 'Alternative data {source} contains alpha signal',
                    'rationale': 'Novel data sources provide information edge',
                    'expected': 'Incremental improvement in prediction accuracy'
                }
            ]
        }

    def generate_factor_hypothesis(self, params: Optional[Dict[str, Any]] = None) -> Hypothesis:
        """Generate factor-related hypothesis"""
        params = params or {}
        template_data = random.choice(self.templates['factor'])

        # Fill template with parameters
        market = params.get('market', random.choice(['US equities', 'cryptocurrencies', 'forex', 'commodities']))
        timeframe = params.get('timeframe', random.choice(['5 days', '20 days', '60 days']))
        threshold = params.get('threshold', random.choice([5, 10, 15, 20]))
        asset_class = params.get('asset_class', random.choice(['stocks', 'crypto', 'futures']))
        direction = params.get('direction', random.choice(['upward', 'downward']))
        period = params.get('period', random.choice(['1 day', '3 days', '1 week']))
        sector = params.get('sector', random.choice(['technology', 'finance', 'healthcare', 'energy']))

        description = template_data['template'].format(
            market=market,
            timeframe=timeframe,
            threshold=threshold,
            asset_class=asset_class,
            direction=direction,
            period=period,
            sector=sector
        )

        hypothesis = Hypothesis(
            id=f"HYP-FACTOR-{datetime.now().strftime('%Y%m%d%H%M%S')}-{random.randint(1000, 9999)}",
            description=description,
            rationale=template_data['rationale'],
            expected_outcome=template_data['expected'],
            confidence=random.uniform(0.6, 0.9),
            priority=random.choice(['high', 'medium', 'low']),
            category='factor',
            created_at=datetime.now().isoformat(),
            metadata=params
        )

        self.hypotheses.append(hypothesis)
        return hypothesis

    def generate_model_hypothesis(self, params: Optional[Dict[str, Any]] = None) -> Hypothesis:
        """Generate model-related hypothesis"""
        params = params or {}
        template_data = random.choice(self.templates['model'])

        model_type = params.get('model_type', random.choice(['LSTM', 'Transformer', 'GRU', 'TCN', 'HIST']))
        prediction_task = params.get('task', random.choice(['return prediction', 'volatility forecasting', 'trend classification']))
        n = params.get('n_models', random.choice([3, 5, 7, 10]))
        feature = params.get('feature', random.choice(['temporal', 'cross-sectional', 'multi-modal']))
        source_market = params.get('source', random.choice(['S&P 500', 'NASDAQ', 'CSI 300']))
        target_market = params.get('target', random.choice(['small-cap', 'emerging markets', 'sector-specific']))

        description = template_data['template'].format(
            model_type=model_type,
            prediction_task=prediction_task,
            n=n,
            feature=feature,
            source_market=source_market,
            target_market=target_market
        )

        hypothesis = Hypothesis(
            id=f"HYP-MODEL-{datetime.now().strftime('%Y%m%d%H%M%S')}-{random.randint(1000, 9999)}",
            description=description,
            rationale=template_data['rationale'],
            expected_outcome=template_data['expected'],
            confidence=random.uniform(0.65, 0.95),
            priority=random.choice(['high', 'medium']),
            category='model',
            created_at=datetime.now().isoformat(),
            metadata=params
        )

        self.hypotheses.append(hypothesis)
        return hypothesis

    def generate_strategy_hypothesis(self, params: Optional[Dict[str, Any]] = None) -> Hypothesis:
        """Generate strategy-related hypothesis"""
        params = params or {}
        template_data = random.choice(self.templates['strategy'])

        factor = params.get('factor', random.choice(['momentum', 'value', 'quality', 'low-volatility']))
        signal = params.get('signal', random.choice(['volatility', 'volume', 'confidence', 'market regime']))

        description = template_data['template'].format(
            factor=factor,
            signal=signal
        )

        hypothesis = Hypothesis(
            id=f"HYP-STRAT-{datetime.now().strftime('%Y%m%d%H%M%S')}-{random.randint(1000, 9999)}",
            description=description,
            rationale=template_data['rationale'],
            expected_outcome=template_data['expected'],
            confidence=random.uniform(0.7, 0.9),
            priority='high',
            category='strategy',
            created_at=datetime.now().isoformat(),
            metadata=params
        )

        self.hypotheses.append(hypothesis)
        return hypothesis

    def generate_feature_hypothesis(self, params: Optional[Dict[str, Any]] = None) -> Hypothesis:
        """Generate feature engineering hypothesis"""
        params = params or {}
        template_data = random.choice(self.templates['feature'])

        indicator = params.get('indicator', random.choice(['RSI', 'MACD', 'Bollinger Bands', 'ATR']))
        horizon = params.get('horizon', random.choice(['1 day', '5 days', '20 days']))
        ratio = params.get('ratio', random.choice(['P/E', 'P/B', 'EV/EBITDA', 'ROE']))
        source = params.get('source', random.choice(['social sentiment', 'satellite imagery', 'web traffic', 'supply chain']))

        description = template_data['template'].format(
            technical_indicator=indicator,
            horizon=horizon,
            ratio=ratio,
            source=source
        )

        hypothesis = Hypothesis(
            id=f"HYP-FEAT-{datetime.now().strftime('%Y%m%d%H%M%S')}-{random.randint(1000, 9999)}",
            description=description,
            rationale=template_data['rationale'],
            expected_outcome=template_data['expected'],
            confidence=random.uniform(0.5, 0.85),
            priority=random.choice(['medium', 'low']),
            category='feature',
            created_at=datetime.now().isoformat(),
            metadata=params
        )

        self.hypotheses.append(hypothesis)
        return hypothesis

    def generate_batch(self, n: int = 10, categories: Optional[List[str]] = None) -> List[Hypothesis]:
        """Generate multiple hypotheses"""
        categories = categories or ['factor', 'model', 'strategy', 'feature']
        hypotheses = []

        generators = {
            'factor': self.generate_factor_hypothesis,
            'model': self.generate_model_hypothesis,
            'strategy': self.generate_strategy_hypothesis,
            'feature': self.generate_feature_hypothesis
        }

        for _ in range(n):
            category = random.choice(categories)
            hyp = generators[category]()
            hypotheses.append(hyp)

        return hypotheses

    def rank_hypotheses(self, criteria: str = 'confidence') -> List[Hypothesis]:
        """Rank hypotheses by criteria"""
        if criteria == 'confidence':
            return sorted(self.hypotheses, key=lambda h: h.confidence, reverse=True)
        elif criteria == 'priority':
            priority_order = {'high': 3, 'medium': 2, 'low': 1}
            return sorted(self.hypotheses, key=lambda h: priority_order.get(h.priority, 0), reverse=True)
        elif criteria == 'date':
            return sorted(self.hypotheses, key=lambda h: h.created_at, reverse=True)
        else:
            return self.hypotheses

    def filter_by_category(self, category: str) -> List[Hypothesis]:
        """Filter hypotheses by category"""
        return [h for h in self.hypotheses if h.category == category]

    def update_status(self, hypothesis_id: str, status: str) -> bool:
        """Update hypothesis status"""
        for hyp in self.hypotheses:
            if hyp.id == hypothesis_id:
                hyp.status = status
                return True
        return False

    def get_statistics(self) -> Dict[str, Any]:
        """Get hypothesis generation statistics"""
        total = len(self.hypotheses)
        if total == 0:
            return {'total': 0, 'by_category': {}, 'by_status': {}, 'by_priority': {}}

        by_category = {}
        by_status = {}
        by_priority = {}

        for hyp in self.hypotheses:
            by_category[hyp.category] = by_category.get(hyp.category, 0) + 1
            by_status[hyp.status] = by_status.get(hyp.status, 0) + 1
            by_priority[hyp.priority] = by_priority.get(hyp.priority, 0) + 1

        avg_confidence = sum(h.confidence for h in self.hypotheses) / total

        return {
            'total': total,
            'by_category': by_category,
            'by_status': by_status,
            'by_priority': by_priority,
            'avg_confidence': round(avg_confidence, 3)
        }

    def export_hypotheses(self, filepath: Optional[str] = None) -> str:
        """Export hypotheses to JSON"""
        data = {
            'generated_at': datetime.now().isoformat(),
            'total': len(self.hypotheses),
            'hypotheses': [h.to_dict() for h in self.hypotheses],
            'statistics': self.get_statistics()
        }

        json_str = json.dumps(data, indent=2)

        if filepath:
            with open(filepath, 'w') as f:
                f.write(json_str)

        return json_str


# CLI interface for testing
if __name__ == '__main__':
    import sys

    if len(sys.argv) > 1:
        command = sys.argv[1]

        generator = HypothesisGenerator()

        if command == 'generate':
            n = int(sys.argv[2]) if len(sys.argv) > 2 else 5
            category = sys.argv[3] if len(sys.argv) > 3 else None

            if category:
                hypotheses = generator.generate_batch(n, [category])
            else:
                hypotheses = generator.generate_batch(n)

            print(json.dumps({
                'success': True,
                'count': len(hypotheses),
                'hypotheses': [h.to_dict() for h in hypotheses],
                'statistics': generator.get_statistics()
            }, indent=2))

        elif command == 'stats':
            stats = generator.get_statistics()
            print(json.dumps({'success': True, 'statistics': stats}, indent=2))

    else:
        print(json.dumps({
            'success': False,
            'error': 'No command provided',
            'usage': 'python hypothesis_gen.py [generate|stats] [n] [category]'
        }))
