@echo off
:: Define the base directory
set BASE_DIR=ai_hedge_fund

:: Create directories
mkdir %BASE_DIR%
mkdir %BASE_DIR%\ai_hedge_fund
mkdir %BASE_DIR%\ai_hedge_fund\agents
mkdir %BASE_DIR%\ai_hedge_fund\data
mkdir %BASE_DIR%\ai_hedge_fund\utils
mkdir %BASE_DIR%\tests

:: Create empty files
type nul > %BASE_DIR%\README.md
type nul > %BASE_DIR%\LICENSE
type nul > %BASE_DIR%\setup.py
type nul > %BASE_DIR%\requirements.txt
type nul > %BASE_DIR%\.gitignore
type nul > %BASE_DIR%\ai_hedge_fund\__init__.py
type nul > %BASE_DIR%\ai_hedge_fund\main.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\__init__.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\sentiment_agent.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\valuation_agent.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\fundamentals_agent.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\technical_analyst.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\risk_manager.py
type nul > %BASE_DIR%\ai_hedge_fund\agents\portfolio_manager.py
type nul > %BASE_DIR%\ai_hedge_fund\data\__init__.py
type nul > %BASE_DIR%\ai_hedge_fund\data\data_acquisition.py
type nul > %BASE_DIR%\ai_hedge_fund\data\data_preprocessing.py
type nul > %BASE_DIR%\ai_hedge_fund\utils\__init__.py
type nul > %BASE_DIR%\ai_hedge_fund\utils\config.py
type nul > %BASE_DIR%\tests\__init__.py
type nul > %BASE_DIR%\tests\test_sentiment_agent.py
type nul > %BASE_DIR%\tests\test_valuation_agent.py
type nul > %BASE_DIR%\tests\test_fundamentals_agent.py
type nul > %BASE_DIR%\tests\test_technical_analyst.py
type nul > %BASE_DIR%\tests\test_risk_manager.py
type nul > %BASE_DIR%\tests\test_portfolio_manager.py

echo Project structure created successfully.
