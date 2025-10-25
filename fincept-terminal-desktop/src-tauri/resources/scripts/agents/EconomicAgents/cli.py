"""
Command Line Interface for Economic Agents
"""

import argparse
import sys
import json
from typing import Dict, List, Any
from datetime import datetime
from pathlib import Path

from base_agent import EconomicData
from data_processor import EconomicDataProcessor
from comparison import EconomicAgentComparator
from utils import export_to_json, export_to_csv

class EconomicCLI:
    """Command line interface for economic agent analysis"""

    def __init__(self):
        self.processor = EconomicDataProcessor()
        self.comparator = EconomicAgentComparator()

    def create_parser(self):
        """Create argument parser"""
        parser = argparse.ArgumentParser(
            description="Economic Agents CLI - Analyze economic data through different ideological perspectives",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  # Analyze single data point with specific agent
  python cli.py analyze --agent capitalism --data "{'gdp': 3.2, 'inflation': 2.1, 'unemployment': 5.5}"

  # Load data from file and compare all agents
  python cli.py compare --file data.json --all-agents

  # Load CSV data and generate report
  python cli.py report --file economic_data.csv --output report.json

  # Interactive mode
  python cli.py interactive

  # List available agents
  python cli.py list-agents
            """
        )

        subparsers = parser.add_subparsers(dest='command', help='Available commands')

        # Analyze command
        analyze_parser = subparsers.add_parser('analyze', help='Analyze economic data with specific agent')
        analyze_parser.add_argument('--agent', '-a', required=True,
                                 help='Agent name (capitalism, socialism, mixed_economy, keynesian, neoliberal, mercantilism)')
        analyze_parser.add_argument('--data', '-d', help='JSON string of economic data')
        analyze_parser.add_argument('--file', '-f', help='File containing economic data')
        analyze_parser.add_argument('--output', '-o', help='Output file for results')
        analyze_parser.add_argument('--format', choices=['json', 'csv', 'text'], default='text',
                                 help='Output format')

        # Compare command
        compare_parser = subparsers.add_parser('compare', help='Compare multiple agents')
        compare_parser.add_argument('--file', '-f', help='File containing economic data')
        compare_parser.add_argument('--data', '-d', help='JSON string of economic data')
        compare_parser.add_argument('--all-agents', action='store_true', help='Compare all available agents')
        compare_parser.add_argument('--agents', nargs='+', help='Specific agents to compare')
        compare_parser.add_argument('--output', '-o', help='Output file for comparison results')
        compare_parser.add_argument('--format', choices=['json', 'csv', 'text'], default='text',
                                 help='Output format')

        # Report command
        report_parser = subparsers.add_parser('report', help='Generate comprehensive economic report')
        report_parser.add_argument('--file', '-f', required=True,
                                 help='File containing economic time series data')
        report_parser.add_argument('--output', '-o', help='Output file for report')
        report_parser.add_argument('--format', choices=['json', 'html', 'text'], default='text',
                                 help='Output format')
        report_parser.add_argument('--agents', nargs='+', help='Agents to include in report')

        # Policy command
        policy_parser = subparsers.add_parser('policies', help='Get policy recommendations')
        policy_parser.add_argument('--agent', '-a', required=True,
                                 help='Agent name for policy recommendations')
        policy_parser.add_argument('--data', '-d', help='JSON string of economic data')
        policy_parser.add_argument('--file', '-f', help='File containing economic data')
        policy_parser.add_argument('--priority', choices=['high', 'medium', 'low'],
                                 help='Filter by priority level')
        policy_parser.add_argument('--output', '-o', help='Output file for policies')

        # Validate command
        validate_parser = subparsers.add_parser('validate', help='Validate economic data')
        validate_parser.add_argument('--file', '-f', help='File containing economic data')
        validate_parser.add_argument('--data', '-d', help='JSON string of economic data')

        # List agents command
        list_parser = subparsers.add_parser('list-agents', help='List available economic agents')

        # Interactive mode
        interactive_parser = subparsers.add_parser('interactive', help='Interactive analysis mode')

        return parser

    def run(self, args=None):
        """Run the CLI with provided arguments"""
        parser = self.create_parser()

        if args is None:
            args = parser.parse_args()
        else:
            args = parser.parse_args(args)

        if not args.command:
            parser.print_help()
            return

        try:
            if args.command == 'analyze':
                self.handle_analyze(args)
            elif args.command == 'compare':
                self.handle_compare(args)
            elif args.command == 'report':
                self.handle_report(args)
            elif args.command == 'policies':
                self.handle_policies(args)
            elif args.command == 'validate':
                self.handle_validate(args)
            elif args.command == 'list-agents':
                self.handle_list_agents(args)
            elif args.command == 'interactive':
                self.handle_interactive(args)
        except Exception as e:
            print(f"Error: {e}")
            sys.exit(1)

    def handle_analyze(self, args):
        """Handle analyze command"""
        from __init__ import get_agent

        # Load data
        data = self._load_data(args.data, args.file)
        if data is None:
            print("Error: No data provided")
            return

        # Get agent
        try:
            agent = get_agent(args.agent)
        except ValueError as e:
            print(f"Error: {e}")
            return

        # Perform analysis
        analysis = agent.analyze_economy(data)

        # Output results
        self._output_results(analysis, args.output, args.format)

    def handle_compare(self, args):
        """Handle compare command"""
        from __init__ import get_all_agents

        # Load data
        data = self._load_data(args.data, args.file)
        if data is None:
            print("Error: No data provided")
            return

        # Get agents to compare
        if args.all_agents:
            agents = get_all_agents()
        elif args.agents:
            agents = {}
            for agent_name in args.agents:
                try:
                    agents[agent_name] = get_agent(agent_name)
                except ValueError as e:
                    print(f"Warning: {e}")
        else:
            print("Error: Must specify --all-agents or --agents")
            return

        # Perform comparison
        comparison = self.comparator.compare_agents(agents, data)

        # Output results
        self._output_results(comparison, args.output, args.format)

    def handle_report(self, args):
        """Handle report command"""
        from __init__ import get_all_agents

        # Load time series data
        if not args.file:
            print("Error: Must specify file for report generation")
            return

        try:
            data_points = self.processor.load_from_json(args.file)
            if isinstance(data_points, dict):
                data_points = [data_points]
        except:
            try:
                data_points = self.processor.load_from_csv(args.file)
            except Exception as e:
                print(f"Error loading data file: {e}")
                return

        if not data_points:
            print("Error: No data loaded")
            return

        # Get agents
        all_agents = get_all_agents()
        if args.agents:
            agents = {name: all_agents[name] for name in args.agents if name in all_agents}
        else:
            agents = all_agents

        # Generate comprehensive report
        report = self._generate_comprehensive_report(agents, data_points)

        # Output report
        self._output_results(report, args.output, args.format)

    def handle_policies(self, args):
        """Handle policies command"""
        from __init__ import get_agent

        # Load data
        data = self._load_data(args.data, args.file)
        if data is None:
            print("Error: No data provided")
            return

        # Get agent
        try:
            agent = get_agent(args.agent)
        except ValueError as e:
            print(f"Error: {e}")
            return

        # Get analysis and policies
        analysis = agent.analyze_economy(data)
        policies = agent.get_policy_recommendations(data, analysis)

        # Filter by priority if specified
        if args.priority:
            policies = [p for p in policies if p.priority == args.priority]

        # Output results
        result = {
            'agent': args.agent,
            'analysis_summary': {
                'overall_health_score': analysis.get('overall_health_score', 0),
                'assessment': analysis.get('assessment', 'No assessment available')
            },
            'policy_recommendations': [
                {
                    'title': p.title,
                    'description': p.description,
                    'priority': p.priority,
                    'expected_impact': p.expected_impact,
                    'implementation_time': p.implementation_time,
                    'risk_level': p.risk_level
                }
                for p in policies
            ]
        }

        self._output_results(result, args.output, 'json')

    def handle_validate(self, args):
        """Handle validate command"""
        # Load data
        data = self._load_data(args.data, args.file)
        if data is None:
            print("Error: No data provided")
            return

        # Validate data
        from utils import validate_economic_data
        issues = validate_economic_data(data)

        if not issues:
            print("✓ Data validation passed")
        else:
            print("✗ Data validation failed:")
            for issue in issues:
                print(f"  - {issue}")

        # Additional validation for consistency
        if isinstance(data, list):
            is_valid, consistency_issues = self.processor.validate_data_consistency(data)
            if not is_valid:
                print("✗ Data consistency issues:")
                for issue in consistency_issues:
                    print(f"  - {issue}")

    def handle_list_agents(self, args):
        """Handle list-agents command"""
        from __init__ import list_agents

        agents = list_agents()
        print("Available Economic Agents:")
        print("-" * 40)

        agent_descriptions = {
            'capitalism': 'Free market capitalism with minimal government intervention',
            'socialism': 'Social ownership and economic equality',
            'mixed_economy': 'Balance of public and private sectors',
            'keynesian': 'Demand management and counter-cyclical policies',
            'neoliberal': 'Deregulation and free market fundamentalism',
            'mercantilism': 'Trade surplus and national wealth accumulation'
        }

        for agent in agents:
            description = agent_descriptions.get(agent, 'No description available')
            print(f"  {agent:<15} - {description}")

        print("-" * 40)
        print(f"Total: {len(agents)} agents available")

    def handle_interactive(self, args):
        """Handle interactive mode"""
        print("=" * 60)
        print("ECONOMIC AGENTS INTERACTIVE MODE")
        print("=" * 60)
        print("Type 'help' for available commands, 'quit' to exit")

        from __init__ import get_agent, get_all_agents

        while True:
            try:
                command = input("\n> ").strip().lower()

                if command in ['quit', 'exit', 'q']:
                    break
                elif command == 'help':
                    print("\nAvailable commands:")
                    print("  analyze <agent> - Analyze current data with specified agent")
                    print("  compare [agents...] - Compare agents with current data")
                    print("  data <json> - Set economic data for analysis")
                    print("  load <file> - Load data from file")
                    print("  policies <agent> - Get policy recommendations")
                    print("  validate - Validate current data")
                    print("  status - Show current data and settings")
                    print("  help - Show this help")
                    print("  quit - Exit interactive mode")
                elif command.startswith('data '):
                    try:
                        data_json = command[5:]
                        self.current_data = self._parse_data_string(data_json)
                        print("Data loaded successfully")
                    except Exception as e:
                        print(f"Error loading data: {e}")
                elif command.startswith('load '):
                    try:
                        filename = command[5:]
                        self.current_data = self._load_data(None, filename)
                        print(f"Data loaded from {filename}")
                    except Exception as e:
                        print(f"Error loading file: {e}")
                elif command.startswith('analyze '):
                    if not hasattr(self, 'current_data'):
                        print("No data loaded. Use 'data' or 'load' command first")
                        continue

                    agent_name = command[8:].strip()
                    try:
                        agent = get_agent(agent_name)
                        analysis = agent.analyze_economy(self.current_data)
                        print(f"\nAnalysis from {agent_name} agent:")
                        print(f"Health Score: {analysis.get('overall_health_score', 0):.1f}/100")
                        print(f"Assessment: {analysis.get('assessment', 'N/A')}")
                    except Exception as e:
                        print(f"Error: {e}")
                elif command == 'status':
                    if hasattr(self, 'current_data'):
                        print("\nCurrent economic data:")
                        print(f"  GDP Growth: {self.current_data.gdp}%")
                        print(f"  Inflation: {self.current_data.inflation}%")
                        print(f"  Unemployment: {self.current_data.unemployment}%")
                        print(f"  Interest Rate: {self.current_data.interest_rate}%")
                        print(f"  Trade Balance: {self.current_data.trade_balance}%")
                        print(f"  Government Spending: {self.current_data.government_spending}%")
                        print(f"  Tax Rate: {self.current_data.tax_rate}%")
                        print(f"  Private Investment: {self.current_data.private_investment}%")
                        print(f"  Consumer Confidence: {self.current_data.consumer_confidence}")
                    else:
                        print("No data currently loaded")
                else:
                    print("Unknown command. Type 'help' for available commands")

            except KeyboardInterrupt:
                break
            except EOFError:
                break

        print("\nGoodbye!")

    def _load_data(self, data_string: str, file_path: str):
        """Load data from string or file"""
        if data_string:
            return self._parse_data_string(data_string)
        elif file_path:
            file_path = Path(file_path)
            if file_path.suffix == '.json':
                data = self.processor.load_from_json(str(file_path))
                return data if isinstance(data, EconomicData) else data[0] if data else None
            elif file_path.suffix == '.csv':
                data_points = self.processor.load_from_csv(str(file_path))
                return data_points[0] if data_points else None
            else:
                raise ValueError(f"Unsupported file format: {file_path.suffix}")
        else:
            return None

    def _parse_data_string(self, data_string: str) -> EconomicData:
        """Parse data string into EconomicData object"""
        try:
            data_dict = json.loads(data_string)
            return EconomicData(
                gdp=float(data_dict.get('gdp', 0)),
                inflation=float(data_dict.get('inflation', 0)),
                unemployment=float(data_dict.get('unemployment', 0)),
                interest_rate=float(data_dict.get('interest_rate', 0)),
                trade_balance=float(data_dict.get('trade_balance', 0)),
                government_spending=float(data_dict.get('government_spending', 0)),
                tax_rate=float(data_dict.get('tax_rate', 0)),
                private_investment=float(data_dict.get('private_investment', 0)),
                consumer_confidence=float(data_dict.get('consumer_confidence', 0)),
                timestamp=datetime.now()
            )
        except Exception as e:
            raise ValueError(f"Invalid data format: {e}")

    def _output_results(self, results: Any, output_file: str = None, format_type: str = 'text'):
        """Output results in specified format"""
        if format_type == 'json':
            output = json.dumps(results, indent=2, default=str)
        elif format_type == 'csv' and isinstance(results, list):
            # Convert list of dicts to CSV format
            import csv
            import io
            output = io.StringIO()
            if results:
                writer = csv.DictWriter(output, fieldnames=results[0].keys())
                writer.writeheader()
                writer.writerows(results)
                output = output.getvalue()
            else:
                output = "No data to export"
        else:  # text format
            output = self._format_as_text(results)

        if output_file:
            with open(output_file, 'w') as f:
                f.write(output)
            print(f"Results saved to {output_file}")
        else:
            print(output)

    def _format_as_text(self, results: Any) -> str:
        """Format results as readable text"""
        if isinstance(results, dict):
            output = []
            for key, value in results.items():
                if isinstance(value, dict):
                    output.append(f"\n{key.upper()}:")
                    for subkey, subvalue in value.items():
                        output.append(f"  {subkey}: {subvalue}")
                elif isinstance(value, list):
                    output.append(f"\n{key.upper()}:")
                    for i, item in enumerate(value, 1):
                        output.append(f"  {i}. {item}")
                else:
                    output.append(f"{key}: {value}")
            return "\n".join(output)
        elif isinstance(results, list):
            return "\n".join(f"{i+1}. {item}" for i, item in enumerate(results))
        else:
            return str(results)

    def _generate_comprehensive_report(self, agents: Dict, data_points: List) -> Dict[str, Any]:
        """Generate comprehensive economic report"""
        if not data_points:
            return {"error": "No data provided"}

        # Get latest data point for current analysis
        latest_data = max(data_points, key=lambda x: x.timestamp)

        # Analyze with all agents
        current_analyses = {}
        for name, agent in agents.items():
            try:
                current_analyses[name] = agent.analyze_economy(latest_data)
            except Exception as e:
                current_analyses[name] = {"error": str(e)}

        # Compare agents
        comparison = self.comparator.compare_agents(agents, latest_data)

        # Analyze trends over time
        from utils import create_economic_summary
        trends = create_economic_summary(data_points)

        # Generate report
        report = {
            "report_metadata": {
                "generated_at": datetime.now().isoformat(),
                "data_points_analyzed": len(data_points),
                "date_range": trends["date_range"],
                "agents_included": list(agents.keys())
            },
            "current_situation": {
                "latest_data": {
                    "timestamp": latest_data.timestamp.isoformat(),
                    "gdp": latest_data.gdp,
                    "inflation": latest_data.inflation,
                    "unemployment": latest_data.unemployment,
                    "interest_rate": latest_data.interest_rate,
                    "trade_balance": latest_data.trade_balance,
                    "government_spending": latest_data.government_spending,
                    "tax_rate": latest_data.tax_rate,
                    "private_investment": latest_data.private_investment,
                    "consumer_confidence": latest_data.consumer_confidence
                },
                "agent_analyses": current_analyses
            },
            "comparative_analysis": comparison,
            "trend_analysis": trends,
            "policy_recommendations": self.comparator.compare_policy_recommendations(agents, latest_data),
            "executive_summary": self._generate_executive_summary(comparison, trends)
        }

        return report

    def _generate_executive_summary(self, comparison: Dict, trends: Dict) -> str:
        """Generate executive summary"""
        avg_health = comparison.get('comparison_metrics', {}).get('average_health_score', 0)
        diversity = comparison.get('comparison_metrics', {}).get('ideological_diversity', 0)

        # Determine overall economic trend
        gdp_trend = trends.get('indicators', {}).get('gdp', {}).get('trend', 'neutral')
        unemployment_trend = trends.get('indicators', {}).get('unemployment', {}).get('trend', 'neutral')

        summary = f"""
Executive Summary:

Overall Economic Health: {avg_health:.1f}/100
Ideological Agreement: {100 - diversity:.1f}% average consensus

Economic Trends:
- GDP Growth: {gdp_trend}
- Unemployment: {unemployment_trend}

Key Findings:
{comparison.get('summary', 'Unable to generate summary')}

Recommendations:
{self._get_top_recommendations(comparison.get('policy_comparison', {}))}
        """.strip()

        return summary

    def _get_top_recommendations(self, policy_comparison: Dict) -> str:
        """Extract top policy recommendations"""
        if not policy_comparison or 'recommendations' not in policy_comparison:
            return "No specific recommendations available."

        top_recommendations = policy_comparison['recommendations'][:3]
        recommendations_text = []

        for rec in top_recommendations:
            category = rec.get('category', 'Unknown')
            support = rec.get('support_level', 0)
            recommendations_text.append(f"- Consider {category}-focused policies (support: {support:.1f}%)")

        return "\n".join(recommendations_text)


def main():
    """Main entry point for CLI"""
    cli = EconomicCLI()
    cli.run()


if __name__ == '__main__':
    main()