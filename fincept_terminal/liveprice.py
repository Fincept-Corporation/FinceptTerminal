import threading
import sys
import msvcrt  # Windows-specific module for capturing key presses
from fyers_apiv3.FyersWebsocket import data_ws
from rich.console import Console
from rich.live import Live
from rich.table import Table
from rich.text import Text
from rich.style import Style
from datetime import datetime, timezone, timedelta

# Console for displaying the table
console = Console()

# Static table to hold live stock data
stock_data = {
    'NSE:IRFC-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                    'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                    'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:RVNL-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                    'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                    'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:RITES-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                     'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                     'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:SBIN-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                    'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                    'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:SHRIRAMFIN-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                          'ask_price': '-',
                          'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                          'tot_sell_qty': '-',
                          'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:BPCL-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                    'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                    'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:KOTAKBANK-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                         'ask_price': '-',
                         'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                         'tot_sell_qty': '-',
                         'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:INFY-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                    'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                    'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:BAJFINANCE-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                          'ask_price': '-',
                          'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                          'tot_sell_qty': '-',
                          'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:ADANIENT-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                        'ask_price': '-',
                        'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                        'tot_sell_qty': '-',
                        'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:SUNPHARMA-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                         'ask_price': '-',
                         'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                         'tot_sell_qty': '-',
                         'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:JSWSTEEL-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                        'ask_price': '-',
                        'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                        'tot_sell_qty': '-',
                        'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:HDFCBANK-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                        'ask_price': '-',
                        'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                        'tot_sell_qty': '-',
                        'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:TCS-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-', 'ask_price': '-',
                   'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                   'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:ICICIBANK-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                         'ask_price': '-',
                         'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                         'tot_sell_qty': '-',
                         'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:POWERGRID-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                         'ask_price': '-',
                         'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                         'tot_sell_qty': '-',
                         'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:MARUTI-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                      'ask_price': '-',
                      'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-', 'tot_sell_qty': '-',
                      'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:INDUSINDBK-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                          'ask_price': '-',
                          'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                          'tot_sell_qty': '-',
                          'prev_close_price': '-', 'last_traded_time': '-'},
    'NSE:AXISBANK-EQ': {'ltp': '-', 'ltp_prev': None, 'ltp_change': '-', 'volume': '-', 'bid_price': '-',
                        'ask_price': '-',
                        'bid_size': '-', 'ask_size': '-', 'last_traded_qty': '-', 'tot_buy_qty': '-',
                        'tot_sell_qty': '-',
                        'prev_close_price': '-', 'last_traded_time': '-'},
}


def calculate_ltp_change(symbol, new_ltp):
    prev_ltp = stock_data[symbol]['ltp_prev']
    if prev_ltp is not None and prev_ltp != '-':
        change = ((new_ltp - prev_ltp) / prev_ltp) * 100
        stock_data[symbol]['ltp_change'] = f"{change:.2f}%"
    else:
        stock_data[symbol]['ltp_change'] = '0.00%'
    stock_data[symbol]['ltp_prev'] = new_ltp


def unix_to_ist(unix_timestamp):
    ist_offset = timedelta(hours=5, minutes=30)
    ist_time = datetime.fromtimestamp(unix_timestamp, tz=timezone.utc) + ist_offset
    return ist_time.strftime('%H:%M:%S')


def update_table(symbol, data):
    new_ltp = data.get('ltp', '-')
    stock_data[symbol]['ltp'] = new_ltp
    calculate_ltp_change(symbol, new_ltp)
    stock_data[symbol]['volume'] = data.get('vol_traded_today', '-')
    stock_data[symbol]['bid_price'] = data.get('bid_price', '-')
    stock_data[symbol]['ask_price'] = data.get('ask_price', '-')
    stock_data[symbol]['bid_size'] = data.get('bid_size', '-')
    stock_data[symbol]['ask_size'] = data.get('ask_size', '-')
    stock_data[symbol]['last_traded_qty'] = data.get('last_traded_qty', '-')
    stock_data[symbol]['tot_buy_qty'] = data.get('tot_buy_qty', '-')
    stock_data[symbol]['tot_sell_qty'] = data.get('tot_sell_qty', '-')
    stock_data[symbol]['prev_close_price'] = data.get('prev_close_price', '-')
    last_traded_time = data.get('last_traded_time', '-')
    stock_data[symbol]['last_traded_time'] = unix_to_ist(last_traded_time) if last_traded_time != '-' else '-'


def generate_table():
    table = Table(title="Live Stock Prices")

    table.add_column("Symbol", justify="center", style="cyan", no_wrap=True)
    table.add_column("LTP", justify="center", style="green", no_wrap=True)
    table.add_column("Change", justify="center", style="green", no_wrap=True)
    table.add_column("Volume", justify="center", style="magenta", no_wrap=True)
    table.add_column("Bid", justify="center", style="yellow", no_wrap=True)
    table.add_column("Ask", justify="center", style="yellow", no_wrap=True)
    table.add_column("Bid Size", justify="center", style="yellow", no_wrap=True)
    table.add_column("Ask Size", justify="center", style="yellow", no_wrap=True)
    table.add_column("LTD Qty", justify="center", style="blue", no_wrap=True)
    table.add_column("Tot Buy", justify="center", style="blue", no_wrap=True)
    table.add_column("Tot Sell", justify="center", style="blue", no_wrap=True)
    table.add_column("Prev Close", justify="center", style="blue", no_wrap=True)
    table.add_column("LTD Time", justify="center", style="red", no_wrap=True)

    for symbol, values in stock_data.items():
        ticker = symbol.replace('NSE:', '').replace('-EQ', '')

        ltp_style = Style(color="green")
        if values['ltp_prev'] is not None:
            if values['ltp'] > values['ltp_prev']:
                ltp_style = Style(color="green")
            elif values['ltp'] < values['ltp_prev']:
                ltp_style = Style(color="red")

        ltp_text = Text(str(values['ltp']), style=ltp_style)
        ltp_change_text = Text(str(values['ltp_change']), style=ltp_style)

        table.add_row(
            ticker,
            ltp_text,
            ltp_change_text,
            str(values['volume']),
            str(values['bid_price']),
            str(values['ask_price']),
            str(values['bid_size']),
            str(values['ask_size']),
            str(values['last_traded_qty']),
            str(values['tot_buy_qty']),
            str(values['tot_sell_qty']),
            str(values['prev_close_price']),
            str(values['last_traded_time']),
        )
        table.add_row(
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15,
            "-" * 15
        )

    return table


def onmessage(message):
    if 'symbol' in message:
        symbol = message['symbol']
        if symbol in stock_data:
            update_table(symbol, message)


def onerror(message):
    print(f"Error: {message}")


def onclose(message):
    print(f"Connection closed: {message}")


def onopen():
    data_type = "SymbolUpdate"
    symbols = list(stock_data.keys())
    fyers.subscribe(symbols=symbols, data_type=data_type)
    fyers.keep_running()


def start_live_price_menu():
    # Start WebSocket in a separate thread
    websocket_thread = threading.Thread(target=run_websocket)
    websocket_thread.daemon = True
    websocket_thread.start()

    # Use Live to dynamically update the table in the console
    exit_counter = 0
    with Live(generate_table(), refresh_per_second=10, console=console) as live:
        while True:
            live.update(generate_table())

            # Check for user input to exit the program
            if msvcrt.kbhit():  # Check if a key is pressed
                key = msvcrt.getch()  # Get the key press
                if key == b'\r':  # Enter key
                    exit_counter += 1
                    if exit_counter == 3:
                        console.print("[bold green]Exiting Live Price Display...[/bold green]")
                        break
                else:
                    exit_counter = 0  # Reset the counter if a different key is pressed

    # Redirect to the main menu after exiting the live display
    from fincept_terminal.cli import show_main_menu
    show_main_menu()


def run_websocket():
    fyers.connect()


# Replace with your actual access token
access_token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJhcGkuZnllcnMuaW4iLCJpYXQiOjE3MjY4MDY3MDUsImV4cCI6MTcyNjg3ODY0NSwibmJmIjoxNzI2ODA2NzA1LCJhdWQiOlsieDowIiwieDoxIiwieDoyIiwiZDoxIiwiZDoyIiwieDoxIiwieDowIl0sInN1YiI6ImFjY2Vzc190b2tlbiIsImF0X2hhc2giOiJnQUFBQUFCbTdQcXgtYzdCMlgwVnlqX3JfLTI5eTdFcmxSY3pJT2ZLQnJiaS16XzJNN1lkdkFUelV1R3RGWUJlQ3F1VjJxaGw4WExNN1hVczlQUHdKMHpMaWcwVjliZ3hrQ1ZZSW9uQk1wcWhLZDFCRmRKTWhWQT0iLCJkaXNwbGF5X25hbWUiOiJCSUpBTCBOSUtVTCBQQVRFTCIsIm9tcyI6IksxIiwiaHNtX2tleSI6IjQ2YmZiODhmYjhlYjQxMjU0NDNjNWJmZjJmMDg4Y2MyYzY0ZDMwZjU2NTU4NWQzOGQ0ZDVlYzc1IiwiZnlfaWQiOiJYQjAyNTA3IiwiYXBwVHlwZSI6MTAwLCJwb2FfZmxhZyI6Ik4ifQ.fEB3229Pj92MXuCybA0cSzcaF2NH1iTt_VRKdbjdjAA"

# Create a FyersDataSocket instance with the provided parameters
fyers = data_ws.FyersDataSocket(
    access_token=access_token,
    log_path="",
    litemode=False,
    write_to_file=False,
    reconnect=True,
    on_connect=onopen,
    on_close=onclose,
    on_error=onerror,
    on_message=onmessage
)

if __name__ == '__main__':
    start_live_price_menu()
