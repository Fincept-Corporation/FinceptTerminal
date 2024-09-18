import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq
import os
import asyncio
from collections import deque
from fyers_apiv3.FyersWebsocket import data_ws, order_ws

# Create a directory to store the Parquet files
output_dir = 'fyers_parquet_data'
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# Batch size for storing data before writing to Parquet
BATCH_SIZE = 1000  # Adjust based on your needs
MAX_QUEUE_SIZE = 10000  # Maximum size of the queue for messages
BATCH_WRITE_INTERVAL = 5  # Time interval (in seconds) to write data even if batch size is not reached

# Initialize a dictionary to store data for each symbol using deque
symbol_data = {}

# Initialize the queue to store incoming messages asynchronously
message_queue = asyncio.Queue(maxsize=MAX_QUEUE_SIZE)

# Store the main event loop globally so it can be accessed in callbacks
main_event_loop = None

# Logging function for clarity
def log(message):
    print(f"[LOG] {message}")

# Function to store data for each stock in Parquet format
def store_in_parquet(symbol, data):
    if symbol not in symbol_data:
        # Use deque to efficiently manage the data for each symbol
        symbol_data[symbol] = deque()

    # Append the data to the deque
    symbol_data[symbol].append(data)
    log(f"Data for {symbol} appended to deque, current length: {len(symbol_data[symbol])}")

    # When deque size reaches the batch size, write the data to a Parquet file
    if len(symbol_data[symbol]) >= BATCH_SIZE:
        df = pd.DataFrame(list(symbol_data[symbol]))

        # Generate a file name based on the symbol and current timestamp
        file_name = os.path.join(output_dir, f'{symbol}_data_{pd.Timestamp.now().strftime("%Y%m%d%H%M%S")}.parquet')

        # Convert the DataFrame to a Parquet file with compression
        table = pa.Table.from_pandas(df)
        pq.write_table(table, file_name, compression='snappy')

        # Clear the deque after writing the data
        symbol_data[symbol].clear()
        log(f"Data for {symbol} written to {file_name}")

# Asynchronous function to handle incoming WebSocket messages and store them
async def process_messages():
    while True:
        try:
            # Wait for a message from the queue
            message = await message_queue.get()

            # Extract the symbol and process the message
            symbol = message.get('symbol')
            if symbol:
                store_in_parquet(symbol, message)

            message_queue.task_done()

        except Exception as e:
            log(f"Error in processing messages: {e}")

# Periodically write any remaining data to Parquet (even if the batch size is not reached)
async def periodic_batch_write():
    while True:
        await asyncio.sleep(BATCH_WRITE_INTERVAL)
        for symbol, data in symbol_data.items():
            if len(data) > 0:
                log(f"Periodic write triggered for {symbol}, current length: {len(data)}")
                store_in_parquet(symbol, list(data))  # Store any remaining data in Parquet
        log("Periodic batch write completed.")

# WebSocket callback to handle incoming messages
def onmessage(message):
    """
    Callback function to handle incoming messages from the FyersDataSocket WebSocket synchronously.
    """
    try:
        # Use the main event loop that we captured at startup
        if main_event_loop:
            asyncio.run_coroutine_threadsafe(process_message(message), main_event_loop)
        else:
            log("Main event loop not found.")
    except RuntimeError as e:
        log(f"Error in onmessage: {e}")

async def process_message(message):
    """
    Asynchronous function to process and enqueue the message.
    """
    await message_queue.put(message)
    log(f"Message enqueued: {message}")

def onerror(message):
    """
    Callback function to handle WebSocket errors.
    """
    log(f"Error: {message}")

def onclose(message):
    """
    Callback function to handle WebSocket connection close events.
    """
    log(f"Connection closed: {message}")

def onopen():
    """
    Callback function to subscribe to data type and symbols upon WebSocket connection.
    """
    data_type = "SymbolUpdate"

    # NIFTY 50 Tickers list
    symbols = [
        'NSE:RELIANCE-EQ', 'NSE:HDFCBANK-EQ', 'NSE:INFY-EQ', 'NSE:ICICIBANK-EQ', 'NSE:TCS-EQ',
        'NSE:HINDUNILVR-EQ', 'NSE:ITC-EQ', 'NSE:KOTAKBANK-EQ', 'NSE:LT-EQ', 'NSE:SBIN-EQ',
        'NSE:AXISBANK-EQ', 'NSE:BAJFINANCE-EQ', 'NSE:BHARTIARTL-EQ', 'NSE:M&M-EQ',
        'NSE:MARUTI-EQ', 'NSE:NESTLEIND-EQ', 'NSE:TITAN-EQ', 'NSE:ULTRACEMCO-EQ', 'NSE:WIPRO-EQ',
        'NSE:ADANIENT-EQ', 'NSE:ADANIGREEN-EQ', 'NSE:ADANIPORTS-EQ', 'NSE:APOLLOHOSP-EQ',
        'NSE:ASIANPAINT-EQ', 'NSE:BAJAJFINSV-EQ', 'NSE:BPCL-EQ', 'NSE:BRITANNIA-EQ', 'NSE:CIPLA-EQ',
        'NSE:COALINDIA-EQ', 'NSE:DIVISLAB-EQ', 'NSE:DRREDDY-EQ', 'NSE:EICHERMOT-EQ', 'NSE:GRASIM-EQ',
        'NSE:HCLTECH-EQ', 'NSE:HEROMOTOCO-EQ', 'NSE:HINDALCO-EQ', 'NSE:JSWSTEEL-EQ', 'NSE:NTPC-EQ',
        'NSE:ONGC-EQ', 'NSE:POWERGRID-EQ', 'NSE:SBILIFE-EQ', 'NSE:SHREECEM-EQ', 'NSE:SIEMENS-EQ',
        'NSE:SUNPHARMA-EQ', 'NSE:TATASTEEL-EQ', 'NSE:TATAMOTORS-EQ', 'NSE:TECHM-EQ', 'NSE:VEDL-EQ'
    ]

    fyers.subscribe(symbols=symbols, data_type=data_type)
    fyers.keep_running()

# Replace the sample access token with your actual access token obtained from Fyers
access_token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJhcGkuZnllcnMuaW4iLCJpYXQiOjE3MjY2MzUxMTYsImV4cCI6MTcyNjcwNTg1NiwibmJmIjoxNzI2NjM1MTE2LCJhdWQiOlsieDowIiwieDoxIiwieDoyIiwiZDoxIiwiZDoyIiwieDoxIiwieDowIl0sInN1YiI6ImFjY2Vzc190b2tlbiIsImF0X2hhc2giOiJnQUFBQUFCbTZseHN1SWlha1ZIZ3hfTHZBUV9ibEFhYXd1NGtIRGVkcDVaUVBLUGdaYVlNa2VwVlNhNW9EeVBpYW9DUFNrU2xaWWV1ZnNOUTVGck1oTF9ZSFJIbHUzcExNRThTdDJfTGkwY3hUSm5HOVRRWHZtOD0iLCJkaXNwbGF5X25hbWUiOiJCSUpBTCBOSUtVTCBQQVRFTCIsIm9tcyI6IksxIiwiaHNtX2tleSI6IjQ2YmZiODhmYjhlYjQxMjU0NDNjNWJmZjJmMDg4Y2MyYzY0ZDMwZjU2NTU4NWQzOGQ0ZDVlYzc1IiwiZnlfaWQiOiJYQjAyNTA3IiwiYXBwVHlwZSI6MTAwLCJwb2FfZmxhZyI6Ik4ifQ.Unq7RFy8xBfNFM5ub6--rS9JU4kXICVRqHclZ1Wt_Iw"

# Create a FyersDataSocket instance with the provided parameters
fyers = data_ws.FyersDataSocket(
    access_token=access_token,  # Access token in the format "appid:accesstoken"
    log_path="",  # Path to save logs. Leave empty to auto-create logs in the current directory.
    litemode=False,  # Lite mode disabled. Set to True if you want a lite response.
    write_to_file=False,  # Save response in a log file instead of printing it.
    reconnect=True,  # Enable auto-reconnection to WebSocket on disconnection.
    on_connect=onopen,  # Callback function to subscribe to data upon connection.
    on_close=onclose,  # Callback function to handle WebSocket connection close events.
    on_error=onerror,  # Callback function to handle WebSocket errors.
    on_message=onmessage  # Callback function to handle incoming messages from the WebSocket.
)

# Main function to run WebSocket and asyncio event loop
def run_fyers_socket():
    global main_event_loop  # Declare the main event loop as global
    main_event_loop = asyncio.get_event_loop()  # Get the main event loop and store it globally

    try:
        # Schedule message processing and periodic batch writing tasks
        main_event_loop.create_task(process_messages())
        main_event_loop.create_task(periodic_batch_write())

        # Run WebSocket connection
        fyers.connect()
        main_event_loop.run_forever()
    except Exception as e:
        log(f"Error running the event loop: {e}")

# Start the WebSocket connection and event loop
if __name__ == "__main__":
    run_fyers_socket()

