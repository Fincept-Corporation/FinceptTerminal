from typing import Dict, List, Optional
from datetime import datetime
from vnpy.trader.engine import MainEngine, EventEngine
from vnpy.trader.object import (
    OrderRequest, CancelRequest, SubscribeRequest, HistoryRequest,
    ContractData, PositionData, OrderData, TradeData
)
from vnpy.trader.constant import Exchange, Interval

ENGINE_INSTANCES = {}

def create_main_engine(engine_id: str = "default") -> Dict:
    if engine_id in ENGINE_INSTANCES:
        return {
            'success': True,
            'engine_id': engine_id,
            'message': 'Engine already exists'
        }

    event_engine = EventEngine()
    main_engine = MainEngine(event_engine)

    ENGINE_INSTANCES[engine_id] = {
        'event_engine': event_engine,
        'main_engine': main_engine
    }

    return {
        'success': True,
        'engine_id': engine_id,
        'message': 'Main engine created'
    }

def send_order(
    engine_id: str,
    symbol: str,
    exchange: str,
    direction: str,
    order_type: str,
    volume: float,
    price: float = 0.0,
    offset: str = "OPEN"
) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    from vnpy.trader.constant import Direction, OrderType, Offset, Exchange

    req = OrderRequest(
        symbol=symbol,
        exchange=Exchange[exchange],
        direction=Direction[direction],
        type=OrderType[order_type],
        volume=volume,
        price=price,
        offset=Offset[offset]
    )

    vt_orderid = main_engine.send_order(req, "")

    return {
        'success': True,
        'order_id': vt_orderid
    }

def cancel_order(
    engine_id: str,
    order_id: str
) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    req = CancelRequest(orderid=order_id, symbol="", exchange=Exchange.SSE)
    main_engine.cancel_order(req, "")

    return {
        'success': True,
        'order_id': order_id
    }

def subscribe_market_data(
    engine_id: str,
    symbol: str,
    exchange: str
) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    req = SubscribeRequest(
        symbol=symbol,
        exchange=Exchange[exchange]
    )

    main_engine.subscribe(req, "")

    return {
        'success': True,
        'symbol': symbol,
        'exchange': exchange
    }

def query_history(
    engine_id: str,
    symbol: str,
    exchange: str,
    start: str,
    end: Optional[str] = None,
    interval: str = "1m"
) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    req = HistoryRequest(
        symbol=symbol,
        exchange=Exchange[exchange],
        start=datetime.fromisoformat(start),
        end=datetime.fromisoformat(end) if end else datetime.now(),
        interval=Interval[interval]
    )

    bars = main_engine.query_history(req, "")

    return {
        'success': True,
        'bars': [bar_to_dict(bar) for bar in bars] if bars else [],
        'count': len(bars) if bars else 0
    }

def get_all_contracts(engine_id: str) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    oms_engine = main_engine.get_engine("oms")
    contracts = oms_engine.get_all_contracts() if oms_engine else []

    return {
        'success': True,
        'contracts': [contract_to_dict(c) for c in contracts],
        'count': len(contracts)
    }

def get_all_positions(engine_id: str) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    oms_engine = main_engine.get_engine("oms")
    positions = oms_engine.get_all_positions() if oms_engine else []

    return {
        'success': True,
        'positions': [position_to_dict(p) for p in positions],
        'count': len(positions)
    }

def get_all_orders(engine_id: str) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    oms_engine = main_engine.get_engine("oms")
    orders = oms_engine.get_all_orders() if oms_engine else []

    return {
        'success': True,
        'orders': [order_to_dict(o) for o in orders],
        'count': len(orders)
    }

def get_all_trades(engine_id: str) -> Dict:
    if engine_id not in ENGINE_INSTANCES:
        return {'success': False, 'error': 'Engine not found'}

    main_engine = ENGINE_INSTANCES[engine_id]['main_engine']

    oms_engine = main_engine.get_engine("oms")
    trades = oms_engine.get_all_trades() if oms_engine else []

    return {
        'success': True,
        'trades': [trade_to_dict(t) for t in trades],
        'count': len(trades)
    }

def bar_to_dict(bar):
    return {
        'symbol': bar.symbol,
        'exchange': bar.exchange.value,
        'datetime': bar.datetime.isoformat(),
        'interval': bar.interval.value,
        'open': bar.open_price,
        'high': bar.high_price,
        'low': bar.low_price,
        'close': bar.close_price,
        'volume': bar.volume
    }

def contract_to_dict(contract):
    return {
        'symbol': contract.symbol,
        'exchange': contract.exchange.value,
        'name': contract.name,
        'product': contract.product.value,
        'size': contract.size,
        'pricetick': contract.pricetick
    }

def position_to_dict(position):
    from .data import position_to_dict as pd
    return pd(position)

def order_to_dict(order):
    from .data import order_to_dict as od
    return od(order)

def trade_to_dict(trade):
    from .data import trade_to_dict as td
    return td(trade)

def main():
    print("Testing VNPy Engine Wrapper")

    print("\n1. Testing create_main_engine...")
    result = create_main_engine("test_engine")
    print("Result:", result)
    assert result['success']
    print("Test 1: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
