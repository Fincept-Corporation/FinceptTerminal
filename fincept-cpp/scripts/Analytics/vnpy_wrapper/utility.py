from typing import Dict, Optional
from datetime import datetime, timedelta
from vnpy.trader.constant import Direction, OrderType, Offset, Exchange

def convert_direction(direction_str: str) -> str:
    direction_map = {
        'buy': 'LONG',
        'long': 'LONG',
        'sell': 'SHORT',
        'short': 'SHORT',
        'net': 'NET'
    }
    return direction_map.get(direction_str.lower(), direction_str.upper())

def convert_order_type(order_type_str: str) -> str:
    type_map = {
        'limit': 'LIMIT',
        'market': 'MARKET',
        'stop': 'STOP',
        'fak': 'FAK',
        'fok': 'FOK'
    }
    return type_map.get(order_type_str.lower(), order_type_str.upper())

def convert_offset(offset_str: str) -> str:
    offset_map = {
        'open': 'OPEN',
        'close': 'CLOSE',
        'closetoday': 'CLOSETODAY',
        'closeyesterday': 'CLOSEYESTERDAY'
    }
    return offset_map.get(offset_str.lower(), offset_str.upper())

def convert_exchange(exchange_str: str) -> str:
    exchange_map = {
        'binance': 'BINANCE',
        'okex': 'OKEX',
        'huobi': 'HUOBI',
        'bybit': 'BYBIT',
        'ctp': 'CTP',
        'sse': 'SSE',
        'szse': 'SZSE',
        'shfe': 'SHFE',
        'dce': 'DCE',
        'czce': 'CZCE',
        'ine': 'INE',
        'cffex': 'CFFEX'
    }
    return exchange_map.get(exchange_str.lower(), exchange_str.upper())

def get_trading_date(dt: Optional[str] = None) -> str:
    if dt:
        target_dt = datetime.fromisoformat(dt)
    else:
        target_dt = datetime.now()

    if target_dt.hour < 8:
        target_dt = target_dt - timedelta(days=1)

    while target_dt.weekday() >= 5:
        target_dt = target_dt - timedelta(days=1)

    return target_dt.strftime('%Y-%m-%d')

def calculate_pnl(
    direction: str,
    entry_price: float,
    exit_price: float,
    volume: float,
    contract_size: float = 1.0
) -> Dict:
    dir_multiplier = 1 if direction.upper() in ['LONG', 'BUY'] else -1

    pnl = (exit_price - entry_price) * volume * contract_size * dir_multiplier
    pnl_pct = ((exit_price - entry_price) / entry_price) * 100 * dir_multiplier

    return {
        'pnl': pnl,
        'pnl_pct': pnl_pct,
        'entry_price': entry_price,
        'exit_price': exit_price,
        'volume': volume,
        'direction': direction
    }

def format_contract_symbol(symbol: str, exchange: str) -> str:
    return "{}.{}".format(symbol, exchange)

def parse_contract_symbol(vt_symbol: str) -> Dict:
    parts = vt_symbol.split('.')
    if len(parts) == 2:
        return {
            'symbol': parts[0],
            'exchange': parts[1]
        }
    return {
        'symbol': vt_symbol,
        'exchange': ''
    }

def calculate_commission(
    price: float,
    volume: float,
    commission_rate: float,
    contract_size: float = 1.0
) -> float:
    return price * volume * contract_size * commission_rate

def calculate_margin(
    price: float,
    volume: float,
    margin_rate: float,
    contract_size: float = 1.0
) -> float:
    return price * volume * contract_size * margin_rate

def main():
    print("Testing VNPy Utility Wrapper")

    print("\n1. Testing convert_direction...")
    result = convert_direction('buy')
    print("Result:", result)
    assert result == 'LONG'
    print("Test 1: PASSED")

    print("\n2. Testing convert_order_type...")
    result = convert_order_type('limit')
    print("Result:", result)
    assert result == 'LIMIT'
    print("Test 2: PASSED")

    print("\n3. Testing calculate_pnl...")
    result = calculate_pnl('LONG', 100.0, 110.0, 10.0)
    print("Result:", result)
    assert result['pnl'] == 100.0
    assert result['pnl_pct'] == 10.0
    print("Test 3: PASSED")

    print("\n4. Testing get_trading_date...")
    result = get_trading_date()
    print("Trading date:", result)
    print("Test 4: PASSED")

    print("\n5. Testing parse_contract_symbol...")
    result = parse_contract_symbol("BTCUSDT.BINANCE")
    print("Result:", result)
    assert result['symbol'] == 'BTCUSDT'
    assert result['exchange'] == 'BINANCE'
    print("Test 5: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
