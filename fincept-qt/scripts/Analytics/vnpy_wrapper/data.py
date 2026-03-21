from typing import Dict, Optional
from datetime import datetime
from vnpy.trader.object import (
    TickData, BarData, OrderData, TradeData, PositionData, AccountData,
    OrderRequest, CancelRequest, SubscribeRequest, ContractData
)
from vnpy.trader.constant import Direction, OrderType, Offset, Exchange, Interval, Status

def create_tick_data(
    symbol: str,
    exchange: str,
    datetime_str: str,
    last_price: float,
    volume: float = 0.0,
    bid_price_1: float = 0.0,
    ask_price_1: float = 0.0,
    bid_volume_1: float = 0.0,
    ask_volume_1: float = 0.0
) -> Dict:
    tick = TickData(
        symbol=symbol,
        exchange=Exchange[exchange],
        datetime=datetime.fromisoformat(datetime_str),
        name=symbol,
        volume=volume,
        last_price=last_price,
        bid_price_1=bid_price_1,
        ask_price_1=ask_price_1,
        bid_volume_1=bid_volume_1,
        ask_volume_1=ask_volume_1,
        gateway_name="wrapper"
    )

    return tick_to_dict(tick)

def create_bar_data(
    symbol: str,
    exchange: str,
    datetime_str: str,
    interval: str,
    open_price: float,
    high_price: float,
    low_price: float,
    close_price: float,
    volume: float
) -> Dict:
    bar = BarData(
        symbol=symbol,
        exchange=Exchange[exchange],
        datetime=datetime.fromisoformat(datetime_str),
        interval=Interval[interval],
        open_price=open_price,
        high_price=high_price,
        low_price=low_price,
        close_price=close_price,
        volume=volume,
        gateway_name="wrapper"
    )

    return bar_to_dict(bar)

def create_order_request(
    symbol: str,
    exchange: str,
    direction: str,
    order_type: str,
    volume: float,
    price: float = 0.0,
    offset: str = "OPEN"
) -> Dict:
    req = OrderRequest(
        symbol=symbol,
        exchange=Exchange[exchange],
        direction=Direction[direction],
        type=OrderType[order_type],
        volume=volume,
        price=price,
        offset=Offset[offset]
    )

    return {
        'symbol': req.symbol,
        'exchange': req.exchange.value,
        'direction': req.direction.value,
        'type': req.type.value,
        'volume': req.volume,
        'price': req.price,
        'offset': req.offset.value
    }

def create_cancel_request(
    order_id: str,
    symbol: str,
    exchange: str
) -> Dict:
    req = CancelRequest(
        orderid=order_id,
        symbol=symbol,
        exchange=Exchange[exchange]
    )

    return {
        'order_id': req.orderid,
        'symbol': req.symbol,
        'exchange': req.exchange.value
    }

def tick_to_dict(tick: TickData) -> Dict:
    return {
        'symbol': tick.symbol,
        'exchange': tick.exchange.value,
        'datetime': tick.datetime.isoformat(),
        'name': tick.name,
        'volume': tick.volume,
        'last_price': tick.last_price,
        'bid_price_1': tick.bid_price_1,
        'ask_price_1': tick.ask_price_1,
        'bid_volume_1': tick.bid_volume_1,
        'ask_volume_1': tick.ask_volume_1,
        'open_price': tick.open_price,
        'high_price': tick.high_price,
        'low_price': tick.low_price,
        'pre_close': tick.pre_close
    }

def bar_to_dict(bar: BarData) -> Dict:
    return {
        'symbol': bar.symbol,
        'exchange': bar.exchange.value,
        'datetime': bar.datetime.isoformat(),
        'interval': bar.interval.value,
        'open_price': bar.open_price,
        'high_price': bar.high_price,
        'low_price': bar.low_price,
        'close_price': bar.close_price,
        'volume': bar.volume,
        'open_interest': bar.open_interest
    }

def order_to_dict(order: OrderData) -> Dict:
    return {
        'order_id': order.orderid,
        'symbol': order.symbol,
        'exchange': order.exchange.value,
        'direction': order.direction.value,
        'offset': order.offset.value,
        'type': order.type.value,
        'price': order.price,
        'volume': order.volume,
        'traded': order.traded,
        'status': order.status.value
    }

def trade_to_dict(trade: TradeData) -> Dict:
    return {
        'trade_id': trade.tradeid,
        'order_id': trade.orderid,
        'symbol': trade.symbol,
        'exchange': trade.exchange.value,
        'direction': trade.direction.value,
        'offset': trade.offset.value,
        'price': trade.price,
        'volume': trade.volume,
        'datetime': trade.datetime.isoformat()
    }

def position_to_dict(position: PositionData) -> Dict:
    return {
        'symbol': position.symbol,
        'exchange': position.exchange.value,
        'direction': position.direction.value,
        'volume': position.volume,
        'price': position.price,
        'pnl': position.pnl
    }

def account_to_dict(account: AccountData) -> Dict:
    return {
        'account_id': account.accountid,
        'balance': account.balance,
        'frozen': account.frozen,
        'available': account.available
    }

def contract_to_dict(contract: ContractData) -> Dict:
    return {
        'symbol': contract.symbol,
        'exchange': contract.exchange.value,
        'name': contract.name,
        'product': contract.product.value,
        'size': contract.size,
        'pricetick': contract.pricetick,
        'min_volume': contract.min_volume,
        'stop_supported': contract.stop_supported,
        'net_position': contract.net_position
    }

def main():
    print("Testing VNPy Data Wrapper")

    print("\n1. Testing create_tick_data...")
    tick = create_tick_data(
        symbol="AAPL",
        exchange="NASDAQ",
        datetime_str="2024-01-01T10:00:00",
        last_price=150.0,
        volume=100.0,
        bid_price_1=149.99,
        ask_price_1=150.01
    )
    print("Tick:", tick)
    assert tick['symbol'] == "AAPL"
    print("Test 1: PASSED")

    print("\n2. Testing create_bar_data...")
    bar = create_bar_data(
        symbol="AAPL",
        exchange="NASDAQ",
        datetime_str="2024-01-01T10:00:00",
        interval="MINUTE",
        open_price=150.0,
        high_price=150.5,
        low_price=149.8,
        close_price=150.3,
        volume=1000.0
    )
    print("Bar:", bar)
    assert bar['close_price'] == 150.3
    print("Test 2: PASSED")

    print("\n3. Testing create_order_request...")
    order_req = create_order_request(
        symbol="AAPL",
        exchange="NASDAQ",
        direction="LONG",
        order_type="LIMIT",
        volume=100.0,
        price=150.0
    )
    print("Order Request:", order_req)
    assert order_req['direction'] in ["LONG", "Long"]
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
