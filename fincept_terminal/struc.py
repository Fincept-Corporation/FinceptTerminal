import os
from pathlib import Path

# Define the project structure as a dictionary
project_structure = {
    'ai_hedge_fund': [
        'README.md',
        'LICENSE',
        'setup.py',
        'requirements.txt',
        {'ai_hedge_fund': [
            '__init__.py',
            {'agents': [
                '__init__.py',
                'sentiment_agent.py',
                'valuation_agent.py',
                'fundamentals_agent.py',
                'technical_analyst.py',
                'risk_manager.py',
                'portfolio_manager.py',
            ]},
            {'data': [
                '__init__.py',
                'data_acquisition.py',
                'data_preprocessing.py',
            ]},
            {'utils': [
                '__init__.py',
                'config.py',
            ]},
            'main.py',
        ]},
        {'tests': [
            '__init__.py',
            'test_sentiment_agent.py',
            'test_valuation_agent.py',
            'test_fundamentals_agent.py',
            'test_technical_analyst.py',
            'test_risk_manager.py',
            'test_portfolio_manager.py',
        ]},
        '.gitignore',
    ]
}

def create_structure(base_path, structure):
    for item in structure:
        if isinstance(item, dict):
            for folder, sub_structure in item.items():
                folder_path = Path(base_path) / folder
                folder_path.mkdir(parents=True, exist_ok=True)
                create_structure(folder_path, sub_structure)
        else:
            file_path = Path(base_path) / item
            if not file_path.exists():
                file_path.touch()

if __name__ == '__main__':
    base_directory = Path('.')  # Current directory
    create_structure(base_directory, project_structure)
    print("Project structure created successfully.")
