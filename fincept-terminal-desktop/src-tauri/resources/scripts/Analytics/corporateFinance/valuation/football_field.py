"""Football Field Valuation Chart"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional, Tuple
import numpy as np
from dataclasses import dataclass

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
except ImportError:
    plt = None

@dataclass
class ValuationRange:
    method_name: str
    low: float
    high: float
    midpoint: float
    weight: float = 1.0

class FootballFieldChart:
    def __init__(self):
        if plt is None:
            raise ImportError("matplotlib required: pip install matplotlib")

        self.colors = {
            'DCF': '#2E86AB',
            'Trading Comps': '#A23B72',
            'Transaction Comps': '#F18F01',
            'LBO': '#C73E1D',
            'Premiums Paid': '#6A994E',
            'Sum of Parts': '#BC4B51'
        }

    def add_valuation_range(self, method: str, low: float, high: float,
                           midpoint: Optional[float] = None, weight: float = 1.0) -> ValuationRange:
        """Add valuation range from methodology"""

        if midpoint is None:
            midpoint = (low + high) / 2

        return ValuationRange(
            method_name=method,
            low=low,
            high=high,
            midpoint=midpoint,
            weight=weight
        )

    def generate_chart(self, valuation_ranges: List[ValuationRange],
                      current_price: Optional[float] = None,
                      offer_price: Optional[float] = None,
                      output_path: Optional[str] = None) -> Dict[str, Any]:
        """Generate football field chart"""

        if not valuation_ranges:
            return {'error': 'No valuation ranges provided'}

        fig, ax = plt.subplots(figsize=(14, 8))

        all_values = []
        for vr in valuation_ranges:
            all_values.extend([vr.low, vr.high])
        if current_price:
            all_values.append(current_price)
        if offer_price:
            all_values.append(offer_price)

        min_val = min(all_values) * 0.9
        max_val = max(all_values) * 1.1

        y_positions = list(range(len(valuation_ranges), 0, -1))

        for i, vr in enumerate(valuation_ranges):
            y = y_positions[i]
            color = self.colors.get(vr.method_name, '#95B8D1')

            ax.barh(y, vr.high - vr.low, left=vr.low, height=0.6,
                   color=color, alpha=0.7, edgecolor='black', linewidth=1.5)

            ax.plot(vr.midpoint, y, 'D', color='black', markersize=8, zorder=5)

            ax.text(min_val, y, f'{vr.method_name}', va='center', ha='right',
                   fontsize=11, fontweight='bold')

            range_text = f'${vr.low/1e9:.2f}B - ${vr.high/1e9:.2f}B'
            ax.text(max_val, y, range_text, va='center', ha='left',
                   fontsize=10)

        if current_price:
            ax.axvline(current_price, color='blue', linestyle='--', linewidth=2,
                      label=f'Current Price: ${current_price/1e9:.2f}B', alpha=0.8)

        if offer_price:
            ax.axvline(offer_price, color='red', linestyle='--', linewidth=2,
                      label=f'Offer Price: ${offer_price/1e9:.2f}B', alpha=0.8)

        ax.set_xlim(min_val, max_val)
        ax.set_ylim(0.5, len(valuation_ranges) + 0.5)

        ax.set_xlabel('Enterprise Value ($ Billions)', fontsize=12, fontweight='bold')
        ax.set_title('Valuation Summary - Football Field Chart', fontsize=14, fontweight='bold', pad=20)

        ax.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'${x/1e9:.1f}B'))

        ax.set_yticks([])
        ax.grid(axis='x', alpha=0.3, linestyle='--')

        if current_price or offer_price:
            ax.legend(loc='upper right', fontsize=10)

        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.spines['left'].set_visible(False)

        plt.tight_layout()

        if output_path:
            plt.savefig(output_path, dpi=300, bbox_inches='tight')

        chart_data = {
            'valuation_ranges': [
                {
                    'method': vr.method_name,
                    'low': vr.low,
                    'high': vr.high,
                    'midpoint': vr.midpoint,
                    'range_pct': ((vr.high - vr.low) / vr.midpoint) * 100
                }
                for vr in valuation_ranges
            ],
            'overall_low': min(vr.low for vr in valuation_ranges),
            'overall_high': max(vr.high for vr in valuation_ranges),
            'overall_midpoint': np.mean([vr.midpoint for vr in valuation_ranges]),
            'current_price': current_price,
            'offer_price': offer_price
        }

        if current_price:
            chart_data['current_vs_low_pct'] = ((current_price - chart_data['overall_low']) / chart_data['overall_low']) * 100
            chart_data['current_vs_high_pct'] = ((current_price - chart_data['overall_high']) / chart_data['overall_high']) * 100

        if offer_price:
            chart_data['offer_vs_current_pct'] = ((offer_price - current_price) / current_price) * 100 if current_price else 0
            chart_data['offer_vs_midpoint_pct'] = ((offer_price - chart_data['overall_midpoint']) / chart_data['overall_midpoint']) * 100

        return chart_data

    def calculate_weighted_valuation(self, valuation_ranges: List[ValuationRange]) -> Dict[str, float]:
        """Calculate weighted average valuation"""

        total_weight = sum(vr.weight for vr in valuation_ranges)

        weighted_low = sum(vr.low * vr.weight for vr in valuation_ranges) / total_weight
        weighted_high = sum(vr.high * vr.weight for vr in valuation_ranges) / total_weight
        weighted_midpoint = sum(vr.midpoint * vr.weight for vr in valuation_ranges) / total_weight

        return {
            'weighted_low': weighted_low,
            'weighted_high': weighted_high,
            'weighted_midpoint': weighted_midpoint,
            'total_weight': total_weight
        }

    def analyze_valuation_dispersion(self, valuation_ranges: List[ValuationRange]) -> Dict[str, float]:
        """Analyze dispersion across valuation methods"""

        midpoints = [vr.midpoint for vr in valuation_ranges]

        return {
            'mean_midpoint': np.mean(midpoints),
            'median_midpoint': np.median(midpoints),
            'std_dev': np.std(midpoints),
            'coefficient_of_variation': (np.std(midpoints) / np.mean(midpoints)) * 100 if np.mean(midpoints) else 0,
            'min_midpoint': min(midpoints),
            'max_midpoint': max(midpoints),
            'range': max(midpoints) - min(midpoints),
            'range_pct': ((max(midpoints) - min(midpoints)) / np.mean(midpoints)) * 100 if np.mean(midpoints) else 0
        }

    def generate_summary_table(self, valuation_ranges: List[ValuationRange],
                               current_price: Optional[float] = None) -> Dict[str, Any]:
        """Generate valuation summary table"""

        table_rows = []
        for vr in valuation_ranges:
            row = {
                'Method': vr.method_name,
                'Low ($M)': vr.low / 1_000_000,
                'Midpoint ($M)': vr.midpoint / 1_000_000,
                'High ($M)': vr.high / 1_000_000,
                'Weight': vr.weight
            }

            if current_price:
                row['Implied Premium to Current (%)'] = ((vr.midpoint - current_price) / current_price) * 100

            table_rows.append(row)

        weighted = self.calculate_weighted_valuation(valuation_ranges)
        dispersion = self.analyze_valuation_dispersion(valuation_ranges)

        summary_row = {
            'Method': 'WEIGHTED AVERAGE',
            'Low ($M)': weighted['weighted_low'] / 1_000_000,
            'Midpoint ($M)': weighted['weighted_midpoint'] / 1_000_000,
            'High ($M)': weighted['weighted_high'] / 1_000_000,
            'Weight': weighted['total_weight']
        }

        if current_price:
            summary_row['Implied Premium to Current (%)'] = ((weighted['weighted_midpoint'] - current_price) / current_price) * 100

        return {
            'valuation_methods': table_rows,
            'weighted_average': summary_row,
            'dispersion_analysis': dispersion
        }

if __name__ == '__main__':
    chart = FootballFieldChart()

    valuation_ranges = [
        chart.add_valuation_range('DCF', 4.5e9, 5.5e9, 5.0e9, weight=1.5),
        chart.add_valuation_range('Trading Comps', 4.8e9, 6.0e9, 5.4e9, weight=1.0),
        chart.add_valuation_range('Transaction Comps', 5.0e9, 6.2e9, 5.6e9, weight=1.2),
        chart.add_valuation_range('LBO', 4.2e9, 5.2e9, 4.7e9, weight=0.8),
        chart.add_valuation_range('Premiums Paid', 5.5e9, 6.5e9, 6.0e9, weight=1.0)
    ]

    current_price = 4.0e9
    offer_price = 5.5e9

    result = chart.generate_chart(
        valuation_ranges,
        current_price=current_price,
        offer_price=offer_price,
        output_path='football_field.png'
    )

    print(f"Overall Valuation Range: ${result['overall_low']/1e9:.2f}B - ${result['overall_high']/1e9:.2f}B")
    print(f"Offer vs Current: {result['offer_vs_current_pct']:.1f}% premium")

    summary = chart.generate_summary_table(valuation_ranges, current_price)
    print(f"\nWeighted Midpoint: ${summary['weighted_average']['Midpoint ($M)']:.0f}M")
