# Python Developer Intern Guide
## Fincept Terminal - Building Financial Data API Wrappers

Welcome to the Fincept Terminal Python development team! This guide will help you contribute to building API wrappers for financial data sources, even if you're completely new to the project.

---

## 📋 Table of Contents

1. [What You'll Be Building](#what-youll-be-building)
2. [Getting Started](#getting-started)
3. [Understanding the Project](#understanding-the-project)
4. [Your First Contribution](#your-first-contribution)
5. [API Wrapper Development Guide](#api-wrapper-development-guide)
6. [Finding Financial APIs to Integrate](#finding-financial-apis-to-integrate)
7. [Code Standards & Best Practices](#code-standards--best-practices)
8. [Testing Your Code](#testing-your-code)
9. [Submitting Your Work](#submitting-your-work)
10. [Resources & Learning](#resources--learning)

---

## 🎯 What You'll Be Building

As a Python developer intern, your primary role is to **create API wrappers** that connect Fincept Terminal to various financial data sources. Think of yourself as building bridges between the terminal and the vast world of financial data.

### What are API Wrappers?

API wrappers are Python scripts that:
- Connect to financial data providers (like Yahoo Finance, Polygon.io, FRED, etc.)
- Fetch data (stock prices, economic indicators, news, etc.)
- Format the data in a standardized JSON format
- Return clean, usable data to the Fincept Terminal

### Why This Matters

Your work directly enables financial professionals to access and analyze data from hundreds of sources in one unified platform. Each wrapper you create adds immense value to the terminal!

---

## 🚀 Getting Started

### Prerequisites

You need:
- **Python 3.8+** installed on your computer
- **Git** for version control
- A **code editor** (VS Code, PyCharm, or any you prefer)
- **Basic Python knowledge** (functions, dictionaries, error handling)

### Setup Instructions

1. **Fork and Clone the Repository**
```bash
# Fork the repo on GitHub first, then:
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal
```

2. **Navigate to the Python Scripts Directory**
```bash
cd fincept-terminal-desktop/src-tauri/resources/scripts
```

3. **Set Up Python Environment** (Recommended)
```bash
# Create virtual environment
python -m venv venv

# Activate it
# On Windows:
venv\Scripts\activate
# On Mac/Linux:
source venv/bin/activate

# Install common dependencies
pip install requests pandas yfinance aiohttp
```

4. **Test Existing Scripts**
```bash
# Test Yahoo Finance wrapper
python yfinance_data.py quote AAPL

# Test FRED wrapper (requires API key in environment)
set FRED_API_KEY=your_key_here  # Windows
export FRED_API_KEY=your_key_here  # Mac/Linux
python fred_data.py series GDP
```

---

## 🏗️ Understanding the Project

### Project Architecture Overview

```
Fincept Terminal
├── Frontend (React/TypeScript) - User interface
│   └── Makes requests to backend
│
├── Backend (Rust/Tauri) - Native app framework
│   └── Calls Python scripts via command line
│
└── Python Scripts (YOU ARE HERE!)
    └── Fetch data from external APIs
    └── Return JSON to backend
```

### Your Python Scripts Location

All your work will be in:
```
fincept-terminal-desktop/src-tauri/resources/scripts/
```

Current example scripts:
- `polygon_data.py` - Polygon.io API wrapper
- `yfinance_data.py` - Yahoo Finance wrapper
- `fred_data.py` - Federal Reserve Economic Data
- `econdb_data.py` - Economic indicators database

---

## 🎓 Your First Contribution

Let's build your first API wrapper step-by-step! We'll create a simple wrapper for **Alpha Vantage** stock data.

### Step 1: Research the API

1. Go to https://www.alphavantage.co/
2. Sign up for a free API key
3. Read the documentation for their "Quote Endpoint"
4. Test the API in your browser or Postman

Example API call:
```
https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=AAPL&apikey=YOUR_KEY
```

### Step 2: Create Your Wrapper File

Create `alphavantage_data.py` in the scripts directory:

```python
"""
Alpha Vantage Data Fetcher
Fetches stock quotes and data from Alpha Vantage API
Returns JSON output for Rust integration
"""

import sys
import json
import os
import requests
from typing import Optional, Dict, Any

# API Configuration
API_KEY = os.environ.get('ALPHAVANTAGE_API_KEY', '')
BASE_URL = "https://www.alphavantage.co/query"

def get_quote(symbol: str) -> Dict[str, Any]:
    """
    Fetch real-time quote for a stock symbol

    Args:
        symbol: Stock ticker symbol (e.g., 'AAPL', 'MSFT')

    Returns:
        Dictionary with quote data or error
    """
    try:
        # Check if API key is configured
        if not API_KEY:
            return {"error": "Alpha Vantage API key not configured"}

        # Build API request
        params = {
            'function': 'GLOBAL_QUOTE',
            'symbol': symbol,
            'apikey': API_KEY
        }

        # Make request with timeout
        response = requests.get(BASE_URL, params=params, timeout=10)
        response.raise_for_status()

        # Parse JSON response
        data = response.json()

        # Check for API errors
        if 'Error Message' in data:
            return {"error": data['Error Message'], "symbol": symbol}

        if 'Note' in data:
            return {"error": "API rate limit exceeded", "symbol": symbol}

        # Extract quote data
        if 'Global Quote' not in data:
            return {"error": "No data returned for symbol", "symbol": symbol}

        quote = data['Global Quote']

        # Format standardized response
        result = {
            "symbol": symbol,
            "price": float(quote.get('05. price', 0)),
            "change": float(quote.get('09. change', 0)),
            "change_percent": float(quote.get('10. change percent', '0').replace('%', '')),
            "volume": int(quote.get('06. volume', 0)),
            "high": float(quote.get('03. high', 0)),
            "low": float(quote.get('04. low', 0)),
            "open": float(quote.get('02. open', 0)),
            "previous_close": float(quote.get('08. previous close', 0)),
            "trading_day": quote.get('07. latest trading day', '')
        }

        return result

    except requests.exceptions.RequestException as e:
        return {"error": f"Network error: {str(e)}", "symbol": symbol}
    except Exception as e:
        return {"error": str(e), "symbol": symbol}


def main():
    """Main CLI entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python alphavantage_data.py <command> <args>",
            "commands": ["quote <symbol>"]
        }))
        sys.exit(1)

    command = sys.argv[1]

    if command == "quote":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python alphavantage_data.py quote <symbol>"}))
            sys.exit(1)

        symbol = sys.argv[2]
        result = get_quote(symbol)
        print(json.dumps(result, indent=2))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))
        sys.exit(1)


if __name__ == "__main__":
    main()
```

### Step 3: Test Your Wrapper

```bash
# Set your API key
set ALPHAVANTAGE_API_KEY=your_actual_key  # Windows
export ALPHAVANTAGE_API_KEY=your_actual_key  # Mac/Linux

# Test the wrapper
python alphavantage_data.py quote AAPL

# Expected output:
{
  "symbol": "AAPL",
  "price": 175.43,
  "change": 2.34,
  "change_percent": 1.35,
  "volume": 45200000,
  ...
}
```

### Step 4: Document Your Wrapper

Add a README section or comments explaining:
- What API it connects to
- How to get an API key
- What commands are available
- Example usage

---

## 📚 API Wrapper Development Guide

### Standard Architecture Pattern

Every API wrapper should follow this structure:

```python
"""
[API NAME] Data Fetcher
[Brief description of what this wrapper does]
Returns JSON output for Rust integration
"""

import sys
import json
import os
import requests  # or other HTTP libraries
from typing import Optional, Dict, Any, List

# 1. CONFIGURATION
API_KEY = os.environ.get('API_NAME_KEY', '')
BASE_URL = "https://api.example.com"

# 2. CORE FUNCTIONS
def get_data_type_1(param1: str, param2: Optional[str] = None) -> Dict[str, Any]:
    """
    Fetch [data type 1]

    Args:
        param1: Description
        param2: Optional description

    Returns:
        Dictionary with data or error
    """
    try:
        # API call logic
        # Error handling
        # Data formatting
        return formatted_result
    except Exception as e:
        return {"error": str(e)}

def get_data_type_2(params) -> Dict[str, Any]:
    """Another data fetching function"""
    pass

# 3. CLI INTERFACE
def main():
    """Main CLI entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage information"}))
        sys.exit(1)

    command = sys.argv[1]

    # Route commands to functions
    if command == "command1":
        result = get_data_type_1(sys.argv[2])
        print(json.dumps(result, indent=2))
    elif command == "command2":
        result = get_data_type_2(sys.argv[2])
        print(json.dumps(result, indent=2))
    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### Standardized Response Format

Always return JSON in this format:

**Success Response:**
```json
{
  "symbol": "AAPL",
  "data_field1": value1,
  "data_field2": value2,
  "timestamp": 1234567890
}
```

**Error Response:**
```json
{
  "error": "Descriptive error message",
  "symbol": "AAPL"  // Include context if applicable
}
```

### Common Commands to Implement

Most financial APIs should support these commands:

1. **quote** - Get current price/quote for a symbol
2. **historical** - Get historical data for a date range
3. **info** - Get detailed information about a symbol
4. **search** - Search for symbols/securities
5. **batch** - Fetch multiple symbols at once

---

## 🔍 Finding Financial APIs to Integrate

### Where to Find APIs

1. **Financial Data Aggregators**
   - Alpha Vantage (https://www.alphavantage.co/)
   - Twelve Data (https://twelvedata.com/)
   - Financial Modeling Prep (https://financialmodelingprep.com/)
   - Finnhub (https://finnhub.io/)
   - IEX Cloud (https://iexcloud.io/)

2. **Cryptocurrency**
   - CoinGecko (https://www.coingecko.com/en/api)
   - CoinMarketCap (https://coinmarketcap.com/api/)
   - Binance API (https://binance-docs.github.io/apidocs/)
   - Kraken (https://www.kraken.com/features/api)

3. **Economic Data**
   - World Bank API (https://datahelpdesk.worldbank.org/knowledgebase/topics/125589)
   - IMF Data (https://data.imf.org/)
   - OECD Data (https://data.oecd.org/)
   - Bureau of Labor Statistics (https://www.bls.gov/developers/)

4. **Alternative Data**
   - Quandl (https://www.quandl.com/)
   - Intrinio (https://intrinio.com/)
   - Tiingo (https://www.tiingo.com/)
   - NewsAPI (https://newsapi.org/)

5. **Specialized Sources**
   - Options data (CBOE, tradier)
   - Fundamental data (Seeking Alpha, SimFin)
   - ESG data (Sustainalytics, MSCI)
   - Real estate (Zillow, Redfin APIs)

### How to Choose APIs

**Good APIs for beginners:**
- ✅ Free tier available
- ✅ Good documentation
- ✅ Simple authentication (API key)
- ✅ RESTful JSON responses
- ✅ No rate limit issues

**Ask yourself:**
1. Does this API provide unique value?
2. Is it reliable and well-maintained?
3. Can financial professionals use this data?
4. Does it fit into Fincept Terminal's vision?

### Research Process

1. **Find a tool financial professionals use**
   - LinkedIn: Ask "What tools do you use for market research?"
   - Reddit: r/algotrading, r/finance, r/investing
   - Twitter: Follow fintwit traders

2. **Check if they have an API**
   - Google: "[platform name] API documentation"
   - Look for developer portals

3. **Evaluate the API**
   - Sign up for API access
   - Read documentation
   - Test endpoints
   - Check rate limits and pricing

4. **Propose integration**
   - Create GitHub issue describing the API
   - Explain the value it brings
   - Get feedback before building

---

## ✅ Code Standards & Best Practices

### 1. Error Handling

Always wrap API calls in try-except blocks:

```python
def get_data(symbol):
    try:
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.Timeout:
        return {"error": "Request timed out"}
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP error: {e.response.status_code}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Network error: {str(e)}"}
    except Exception as e:
        return {"error": str(e)}
```

### 2. API Key Management

Never hardcode API keys:

```python
# ❌ BAD
API_KEY = "abc123xyz"

# ✅ GOOD
API_KEY = os.environ.get('SERVICE_NAME_API_KEY', '')

if not API_KEY:
    return {"error": "API key not configured"}
```

### 3. Timeout Settings

Always set timeouts for network requests:

```python
response = requests.get(url, timeout=10)  # 10 second timeout
```

### 4. Data Validation

Validate data before returning:

```python
def get_quote(symbol):
    # Validate input
    if not symbol or not symbol.isalnum():
        return {"error": "Invalid symbol format"}

    # Fetch data
    data = fetch_from_api(symbol)

    # Validate output
    if 'price' not in data:
        return {"error": "Invalid data received from API"}

    return data
```

### 5. Type Hints

Use type hints for better code clarity:

```python
from typing import Optional, Dict, Any, List

def get_historical(
    symbol: str,
    start_date: Optional[str] = None
) -> Dict[str, Any]:
    pass
```

### 6. Documentation

Document every function:

```python
def get_quote(symbol: str) -> Dict[str, Any]:
    """
    Fetch real-time quote for a stock symbol.

    Args:
        symbol: Stock ticker symbol (e.g., 'AAPL', 'MSFT')

    Returns:
        Dictionary containing:
        - symbol: The ticker symbol
        - price: Current price
        - change: Price change
        - change_percent: Percent change
        - volume: Trading volume

    Example:
        >>> get_quote('AAPL')
        {'symbol': 'AAPL', 'price': 175.43, ...}
    """
    pass
```

### 7. Rate Limiting

Respect API rate limits:

```python
import time

def fetch_batch(symbols, rate_limit_delay=1.0):
    """Fetch multiple symbols with rate limiting"""
    results = []
    for symbol in symbols:
        result = get_quote(symbol)
        results.append(result)
        time.sleep(rate_limit_delay)  # Respect rate limits
    return results
```

---

## 🧪 Testing Your Code

### Manual Testing Checklist

Test your wrapper with:

1. **Valid input**
```bash
python your_wrapper.py quote AAPL
```

2. **Invalid symbol**
```bash
python your_wrapper.py quote INVALIDXYZ
```

3. **Missing API key**
```bash
unset API_KEY_NAME
python your_wrapper.py quote AAPL
```

4. **Network timeout** (disconnect internet temporarily)

5. **Multiple symbols** (if batch command exists)
```bash
python your_wrapper.py batch AAPL MSFT GOOGL
```

### Writing Test Cases

Create a test file `test_your_wrapper.py`:

```python
import json
from your_wrapper import get_quote

def test_valid_symbol():
    result = get_quote('AAPL')
    assert 'error' not in result
    assert 'price' in result
    assert result['symbol'] == 'AAPL'
    print("✓ Valid symbol test passed")

def test_invalid_symbol():
    result = get_quote('INVALIDXYZ')
    assert 'error' in result
    print("✓ Invalid symbol test passed")

if __name__ == "__main__":
    test_valid_symbol()
    test_invalid_symbol()
    print("\n✅ All tests passed!")
```

---

## 🚀 Submitting Your Work

### Before Submitting

1. **Test thoroughly**
   - Test with real API keys
   - Test error cases
   - Test on both Windows and Mac/Linux (if possible)

2. **Document your work**
   - Add comments explaining complex logic
   - Create usage examples
   - Update any relevant README files

3. **Check code quality**
   - Remove debug print statements
   - Fix any linting errors
   - Follow the style of existing scripts

### Creating a Pull Request

1. **Create a new branch**
```bash
git checkout -b add-alphavantage-wrapper
```

2. **Commit your changes**
```bash
git add alphavantage_data.py
git commit -m "Add Alpha Vantage API wrapper

- Implements quote endpoint
- Supports real-time stock data
- Includes error handling and rate limiting
- Tested with multiple symbols"
```

3. **Push to your fork**
```bash
git push origin add-alphavantage-wrapper
```

4. **Create Pull Request on GitHub**
   - Go to https://github.com/Fincept-Corporation/FinceptTerminal
   - Click "New Pull Request"
   - Select your branch
   - Fill in the template:

```markdown
## Description
Add Alpha Vantage API wrapper for real-time stock quotes

## Type of Change
- [ ] Bug fix
- [x] New feature
- [ ] Breaking change

## Testing
- [x] Tested with valid symbols (AAPL, MSFT, GOOGL)
- [x] Tested with invalid symbols
- [x] Tested error handling
- [x] Tested rate limiting

## API Information
- API Provider: Alpha Vantage
- Free Tier: Yes (5 calls/minute, 500/day)
- API Key Required: Yes
- Documentation: https://www.alphavantage.co/documentation/

## Screenshots (if applicable)
[Paste terminal output showing successful data fetch]
```

### Review Process

1. Maintainers will review your code
2. They may request changes or improvements
3. Make requested changes and push updates
4. Once approved, your code will be merged!

---

## 📖 Resources & Learning

### Python Learning Resources

**Basics:**
- Python.org Tutorial: https://docs.python.org/3/tutorial/
- Real Python: https://realpython.com/
- Automate the Boring Stuff: https://automatetheboringstuff.com/

**HTTP Requests:**
- Requests Library: https://requests.readthedocs.io/
- Working with APIs: https://realpython.com/api-integration-in-python/

**JSON Handling:**
- JSON in Python: https://realpython.com/python-json/

**Error Handling:**
- Exception Handling: https://realpython.com/python-exceptions/

### Financial Data Knowledge

**Learn about:**
- Stock market basics
- Cryptocurrency fundamentals
- Economic indicators (GDP, CPI, unemployment)
- Options and derivatives
- Financial statements

**Resources:**
- Investopedia: https://www.investopedia.com/
- Khan Academy Finance: https://www.khanacademy.org/economics-finance-domain

### API Development

**RESTful APIs:**
- REST API Tutorial: https://restfulapi.net/
- HTTP Status Codes: https://httpstatuses.com/

**API Authentication:**
- API Keys, OAuth, JWT basics

### Project-Specific

**Existing Wrappers to Study:**
- `yfinance_data.py` - Simple, well-structured example
- `fred_data.py` - Environment variables, multiple commands
- `econdb_data.py` - Async operations, complex data
- `polygon_data.py` - Class-based wrapper, comprehensive

### Getting Help

**When stuck:**
1. Check existing wrapper code for examples
2. Read the API documentation thoroughly
3. Search GitHub issues for similar problems
4. Ask in GitHub Discussions
5. Reach out on community channels

**Where to Ask:**
- GitHub Issues: For bugs or questions
- GitHub Discussions: For general help
- Project Discord/Slack (if available)

---

## 🎯 Your Growth Path

### Beginner Tasks (Week 1-2)
- Study existing wrappers
- Test all current scripts
- Build your first simple wrapper (stock quotes)
- Submit your first PR

### Intermediate Tasks (Week 3-6)
- Build multi-command wrappers
- Implement caching mechanisms
- Add batch operations
- Create async wrappers (for faster fetching)

### Advanced Tasks (Week 7+)
- Build complex data processing pipelines
- Create analysis models/indicators
- Optimize performance (caching, async)
- Help review other interns' code

### Impact Metrics

Track your contributions:
- Number of API wrappers created
- Data sources added to terminal
- Pull requests merged
- Code reviews participated in
- Issues resolved

---

## 💡 Project Ideas & Inspiration

### High-Impact Wrappers to Build

1. **News & Sentiment**
   - NewsAPI wrapper for financial news
   - Reddit API for r/wallstreetbets sentiment
   - Twitter API for trader sentiment

2. **Fundamental Data**
   - SEC EDGAR filings parser
   - Company financials from SimFin
   - Insider trading data

3. **Alternative Data**
   - Satellite imagery analysis APIs
   - Weather data for commodity trading
   - Social media activity metrics

4. **International Markets**
   - NSE India (National Stock Exchange)
   - Tokyo Stock Exchange data
   - LSE (London Stock Exchange)

5. **Specialized Data**
   - Carbon credit prices
   - Real estate market data
   - Supply chain indicators

### Analysis Models

Beyond wrappers, you can create:
- Technical indicators (custom RSI, MACD)
- Statistical models
- Risk analysis tools
- Portfolio optimization algorithms

---

## 🤝 Community & Collaboration

### Connecting with Others

- **Financial Professionals**: Ask them what data they need
- **Other Interns**: Collaborate on complex projects
- **Open Source Community**: Participate in discussions

### Building Your Portfolio

This experience gives you:
- Real-world Python development experience
- API integration skills
- Financial domain knowledge
- Open source contributions (great for resume!)
- GitHub profile building

---

## ⚡ Quick Reference

### Template Checklist

When creating a new wrapper:
- [ ] Create `api_name_data.py` file
- [ ] Add docstring at the top
- [ ] Import required libraries
- [ ] Define configuration (API keys, base URL)
- [ ] Implement core functions with error handling
- [ ] Add type hints
- [ ] Create CLI interface in `main()`
- [ ] Test with valid and invalid inputs
- [ ] Document usage
- [ ] Create pull request

### Common Issues & Solutions

**Issue**: "ModuleNotFoundError: No module named 'requests'"
**Solution**: `pip install requests`

**Issue**: "API key not working"
**Solution**: Check environment variable spelling, restart terminal

**Issue**: "Rate limit exceeded"
**Solution**: Add delays between requests, implement caching

**Issue**: "Timeout errors"
**Solution**: Increase timeout value, check internet connection

---

## 🎉 Final Words

You're now equipped to contribute meaningful work to Fincept Terminal! Every API wrapper you build makes the platform more powerful and helps financial professionals worldwide.

**Remember:**
- Start small, learn as you go
- Don't hesitate to ask questions
- Study existing code
- Test thoroughly
- Document well
- Have fun building!

**Your work matters.** Financial professionals will use the data sources you integrate to make informed decisions, conduct research, and analyze markets. You're building infrastructure for the future of open-source financial analysis.

Welcome aboard! 🚀

---

**Questions?** Open an issue or reach out in GitHub Discussions!

**Repository**: https://github.com/Fincept-Corporation/FinceptTerminal/
**Contact**: support@fincept.in
