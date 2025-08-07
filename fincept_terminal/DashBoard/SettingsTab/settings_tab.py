"""
Settings Tab module for Fincept Terminal
Updated to use centralized logging system
"""

import sys
from fyers_apiv3.FyersWebsocket import data_ws
from fincept_terminal.Utils.Logging.logger import logger, log_operation

def load_access_token(log_path="access_token.log"):
    try:
        with open(log_path, "r") as f:
            # strip out blank lines and pick the last one
            tokens = [line.strip() for line in f if line.strip()]
        if not tokens:
            raise ValueError("No access tokens found in log.")
        return tokens[-1]
    except FileNotFoundError:
        logger.error("Value: %s", module="Settings_Tab", context={"value": f"Error: '{log_path}' not found."})
        sys.exit(1)
    except Exception as e:
        logger.error(f"Error loading access token: {e}", module="Settings_Tab", context={'e': e})
        sys.exit(1)


def onmessage(message):
    print("Response:", message)

def onerror(message):
    print("Error:", message)

def onclose(message):
    print("Connection closed:", message)

def onopen():
    data_type = "SymbolUpdate"
    symbols = ['NSE:20MICRONS-EQ', 'NSE:21STCENMGM-EQ', 'NSE:360ONE-EQ']
    fyers.subscribe(symbols=symbols, data_type=data_type)
    fyers.keep_running()


# Load the latest token from file
access_token = load_access_token()

# Create and connect the socket
fyers = data_ws.FyersDataSocket(
    access_token=access_token,
    log_path="",       # leave blank to auto-create logs
    litemode=False,
    write_to_file=False,
    reconnect=True,
    on_connect=onopen,
    on_close=onclose,
    on_error=onerror,
    on_message=onmessage
)

fyers.connect()


