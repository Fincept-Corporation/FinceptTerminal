import sys
import json
import yfinance as yf
import pandas as pd

def get_stock_data_direct(symbol: str, period: str = "1mo"):
    """
    Fetch stock data directly using yfinance
    """
    try:
        print(f"Fetching data for {symbol} for period {period}", file=sys.stderr)
        
        # Create ticker object
        ticker = yf.Ticker(symbol)
        
        # Get historical data
        hist = ticker.history(period=period)
        
        print(f"Received data shape: {hist.shape}", file=sys.stderr)
        
        if hist.empty:
            return {
                'symbol': symbol,
                'data': [],
                'error': 'No data received from yfinance',
                'success': False
            }
        
        # Reset index to make date a column
        hist = hist.reset_index()
        
        # Convert datetime to string for JSON serialization
        hist['Date'] = hist['Date'].dt.strftime('%Y-%m-%d')
        
        # Convert to JSON-serializable format
        result = {
            'symbol': symbol,
            'data': hist.to_dict('records'),
            'columns': list(hist.columns),
            'success': True
        }
        return result
        
    except Exception as e:
        import traceback
        error_details = traceback.format_exc()
        print(f"Error details: {error_details}", file=sys.stderr)
        
        return {
            'symbol': symbol,
            'data': [],
            'error': f"{str(e)}",
            'success': False
        }

def main():
    """Main function to handle command line arguments"""
    if len(sys.argv) < 2:
        error_result = {
            'error': 'No symbol provided. Usage: python yfinance_alternative.py <SYMBOL> [PERIOD]',
            'success': False
        }
        print(json.dumps(error_result))
        return
    
    symbol = sys.argv[1].upper()
    period = sys.argv[2] if len(sys.argv) > 2 else "1mo"
    
    result = get_stock_data_direct(symbol, period)
    print(json.dumps(result))

if __name__ == "__main__":
    main()