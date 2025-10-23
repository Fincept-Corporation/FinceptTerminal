# Add New Python Data Source

## Quick Guide for Adding Financial APIs

### Step 1: Choose Your Data Source

**Good options for beginners:**
- Alpha Vantage (stocks, forex)
- Twelve Data (stocks, crypto)
- Financial Modeling Prep (stocks, financials)
- CoinGecko (cryptocurrency)
- FRED (economic data)

### Step 2: Create Your Data Source File

1. Navigate to: `fincept_terminal/Agents/src/tools/`
2. Create new file: `your_source_name.py`
3. Use this template:

```python
"""
Your Source Name Data Fetcher
Fetches data from [API Name]
Returns structured data for analysis
"""

import requests
import os
from typing import Dict, Any, List
from .api import get_financial_data

# ===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - API_KEY (environment variable)
#   - data_type (string): "quote", "historical", etc.
#
# OUTPUT:
#   - Financial data (prices, volume, etc.)
#   - Analysis results (metrics, indicators)
#
# PARAMETERS:
#   - period: "daily", "weekly", "monthly"
#   - limit: Number of records to fetch

API_KEY = os.environ.get('YOUR_SOURCE_API_KEY', '')
BASE_URL = "https://api.example.com"

def fetch_stock_data(symbol: str, data_type: str = "quote") -> Dict[str, Any]:
    """Fetch stock data from API"""

    if not API_KEY:
        return {"error": "API key not configured"}

    try:
        # Make API request
        url = f"{BASE_URL}/stock/{data_type}"
        params = {
            "symbol": symbol,
            "apikey": API_KEY
        }

        response = requests.get(url, params=params, timeout=10)
        response.raise_for_status()

        data = response.json()

        # Format the response
        return {
            "symbol": symbol,
            "price": data.get("price", 0),
            "volume": data.get("volume", 0),
            "change": data.get("change", 0),
            "timestamp": data.get("timestamp")
        }

    except requests.exceptions.RequestException as e:
        return {"error": f"Network error: {str(e)}"}
    except Exception as e:
        return {"error": str(e)}

def get_multiple_symbols(symbols: List[str]) -> Dict[str, Any]:
    """Fetch data for multiple symbols"""
    results = {}

    for symbol in symbols:
        data = fetch_stock_data(symbol)
        results[symbol] = data

    return results
```

### Step 3: Test Your Code

```python
# Test with a single symbol
result = fetch_stock_data("AAPL")
print(result)

# Test with multiple symbols
symbols = ["AAPL", "GOOGL", "MSFT"]
results = get_multiple_symbols(symbols)
print(results)
```

### Step 4: Add Documentation

Add to your file:
- What API it connects to
- How to get API key
- Available data types
- Example usage

### Step 5: Update Agent Files

Add your new data source to relevant agents:

```python
# In agent files like warren_buffett_agent.py
from .tools.your_source_name import fetch_stock_data

# Use in analysis
data = fetch_stock_data(ticker)
```

### Step 6: Create Pull Request

1. **Commit your changes:**
```bash
git add your_source_name.py
git commit -m "Add [API Name] data source"
```

2. **Push and create PR:**
```bash
git push origin main
# Go to GitHub and create Pull Request
```

### PR Template:

```markdown
## Description
Add [API Name] data source for [type of data]

## Data Source Information
- **API Provider**: [Company Name]
- **Free Tier**: Yes/No (describe limits)
- **Documentation**: [link to docs]
- **Data Types**: [list what you implemented]

## Testing
- [x] Tested with valid symbols
- [x] Tested error handling
- [x] Verified output format

## Usage Example
```python
from your_source_name import fetch_stock_data
result = fetch_stock_data("AAPL")
```
```

## Requirements Checklist

- [ ] File created in `Agents/src/tools/`
- [ ] API key uses environment variable
- [ ] Error handling implemented
- [ ] Data returned in consistent format
- [ ] Documentation added
- [ ] Code tested successfully
- [ ] Pull request created

## Need Help?

- Check existing files for examples
- Ask in GitHub Issues
- Include your code and error messages

## Common API Sources

**Stock Market Data:**
- Alpha Vantage: Free 500 calls/day
- Twelve Data: Free 800 calls/day
- Financial Modeling Prep: Free 250 calls/day

**Cryptocurrency:**
- CoinGecko: No API key needed
- CoinMarketCap: Free tier available

**Economic Data:**
- FRED: Federal Reserve data
- World Bank: Global economic indicators

Start with a simple API that doesn't require complex authentication. Build confidence, then tackle more complex data sources!