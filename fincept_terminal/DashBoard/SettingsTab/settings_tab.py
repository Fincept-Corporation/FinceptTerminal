"""
Settings Tab module for Fincept Terminal
Updated to use new logging system with performance optimizations and enhanced error handling
"""

import sys
import os
import platform
from pathlib import Path
from typing import Optional, List, Dict, Any
from fyers_apiv3.FyersWebsocket import data_ws

# Import new logger module
from fincept_terminal.Utils.Logging.logger import (
    info, debug, warning, error, operation, monitor_performance
)

class SettingsManager:
    """Enhanced Settings Manager with cross-platform support and performance optimization"""

    def __init__(self):
        self.config_dir = self._get_config_directory()
        self.config_dir.mkdir(parents=True, exist_ok=True)
        self._fyers_client = None
        self._connection_status = "disconnected"

        debug("SettingsManager initialized", context={'config_dir': str(self.config_dir)})

    def _get_config_directory(self) -> Path:
        """Get platform-specific config directory"""
        system = platform.system()
        if system == 'Windows':
            base = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local'))
            return base / 'FinanceTerminal' / 'settings'
        elif system == 'Darwin':
            return Path.home() / 'Library/Application Support/FinanceTerminal/settings'
        else:  # Linux/Unix
            return Path.home() / '.local/share/finance-terminal/settings'

@monitor_performance
def load_access_token(log_path: Optional[str] = None) -> str:
    """
    Load access token from file with enhanced error handling and cross-platform support

    Args:
        log_path: Custom path to token file. If None, uses platform-specific location.

    Returns:
        str: The access token

    Raises:
        SystemExit: If token cannot be loaded
    """
    with operation("load_access_token"):
        try:
            # Use platform-specific path if not provided
            if log_path is None:
                settings_manager = SettingsManager()
                token_path = settings_manager.config_dir / "access_token.log"
            else:
                token_path = Path(log_path)

            debug("Loading access token", context={'token_path': str(token_path)})

            if not token_path.exists():
                raise FileNotFoundError(f"Token file not found: {token_path}")

            with open(token_path, "r", encoding='utf-8') as f:
                # Strip out blank lines and pick the last one
                tokens = [line.strip() for line in f if line.strip()]

            if not tokens:
                raise ValueError("No access tokens found in log file")

            access_token = tokens[-1]

            # Validate token format (basic check)
            if not access_token or len(access_token) < 10:
                raise ValueError("Invalid token format")

            info("Access token loaded successfully",
                 context={'token_file': str(token_path), 'token_length': len(access_token)})

            return access_token

        except FileNotFoundError as e:
            error("Token file not found",
                  context={'error': str(e), 'token_path': str(token_path)},
                  exc_info=True)
            sys.exit(1)
        except ValueError as e:
            error("Invalid token data",
                  context={'error': str(e), 'token_path': str(token_path)},
                  exc_info=True)
            sys.exit(1)
        except PermissionError as e:
            error("Permission denied accessing token file",
                  context={'error': str(e), 'token_path': str(token_path)},
                  exc_info=True)
            sys.exit(1)
        except Exception as e:
            error("Unexpected error loading access token",
                  context={'error': str(e), 'token_path': str(token_path)},
                  exc_info=True)
            sys.exit(1)


def onmessage(message: Dict[str, Any]) -> None:
    """
    Enhanced message handler with logging and error handling

    Args:
        message: WebSocket message data
    """
    try:
        # Log message details for debugging
        debug("WebSocket message received",
              context={
                  'message_type': message.get('type', 'unknown'),
                  'symbol': message.get('symbol', 'unknown'),
                  'data_keys': list(message.keys()) if isinstance(message, dict) else 'not_dict'
              })

        # Print response (maintain original functionality)
        print("Response:", message)

        # Additional processing could be added here
        # e.g., data validation, storage, notifications, etc.

    except Exception as e:
        error("Error processing WebSocket message",
              context={'error': str(e), 'message': str(message)[:200]},
              exc_info=True)


def onerror(error_message: Any) -> None:
    """
    Enhanced error handler with logging

    Args:
        error_message: WebSocket error message
    """
    try:
        error_str = str(error_message)

        # Log the error with context
        error("WebSocket error occurred",
              context={'error_message': error_str, 'error_type': type(error_message).__name__})

        # Print error (maintain original functionality)
        print("Error:", error_message)

        # Additional error handling could be added here
        # e.g., reconnection logic, alerting, etc.

    except Exception as e:
        error("Error in WebSocket error handler",
              context={'original_error': str(error_message), 'handler_error': str(e)},
              exc_info=True)


def onclose(close_message: Any) -> None:
    """
    Enhanced close handler with logging

    Args:
        close_message: WebSocket close message
    """
    try:
        close_str = str(close_message)

        # Log the connection closure
        warning("WebSocket connection closed",
                context={'close_message': close_str, 'close_type': type(close_message).__name__})

        # Print close message (maintain original functionality)
        print("Connection closed:", close_message)

        # Additional cleanup could be added here
        # e.g., status updates, reconnection scheduling, etc.

    except Exception as e:
        error("Error in WebSocket close handler",
              context={'original_close': str(close_message), 'handler_error': str(e)},
              exc_info=True)


@monitor_performance
def onopen() -> None:
    """
    Enhanced connection open handler with logging and error handling
    """
    with operation("websocket_connection_open"):
        try:
            info("WebSocket connection opened successfully")

            # Configuration for data subscription
            data_type = "SymbolUpdate"
            symbols = ['NSE:20MICRONS-EQ', 'NSE:21STCENMGM-EQ', 'NSE:360ONE-EQ']

            debug("Subscribing to symbols",
                  context={'data_type': data_type, 'symbols': symbols, 'symbol_count': len(symbols)})

            # Subscribe to symbols
            fyers.subscribe(symbols=symbols, data_type=data_type)

            # Keep connection running
            fyers.keep_running()

            info("Symbol subscription completed and connection maintained",
                 context={'subscribed_symbols': len(symbols)})

        except Exception as e:
            error("Error in WebSocket open handler",
                  context={'error': str(e)},
                  exc_info=True)


@monitor_performance
def create_fyers_connection(access_token: str) -> data_ws.FyersDataSocket:
    """
    Create and configure Fyers WebSocket connection with enhanced error handling

    Args:
        access_token: The access token for authentication

    Returns:
        FyersDataSocket: Configured WebSocket client
    """
    with operation("create_fyers_connection"):
        try:
            debug("Creating Fyers WebSocket connection")

            # Create WebSocket client with optimized configuration
            fyers_client = data_ws.FyersDataSocket(
                access_token=access_token,
                log_path="",           # Leave blank to auto-create logs
                litemode=False,        # Full mode for complete data
                write_to_file=False,   # Don't write to file for performance
                reconnect=True,        # Enable auto-reconnect
                on_connect=onopen,     # Connection handler
                on_close=onclose,      # Close handler
                on_error=onerror,      # Error handler
                on_message=onmessage   # Message handler
            )

            info("Fyers WebSocket client created successfully",
                 context={
                     'access_token_length': len(access_token),
                     'litemode': False,
                     'write_to_file': False,
                     'reconnect_enabled': True
                 })

            return fyers_client

        except Exception as e:
            error("Failed to create Fyers WebSocket connection",
                  context={'error': str(e), 'access_token_length': len(access_token)},
                  exc_info=True)
            raise


@monitor_performance
def establish_connection(access_token_path: Optional[str] = None) -> None:
    """
    Establish WebSocket connection with comprehensive error handling

    Args:
        access_token_path: Optional custom path to access token file
    """
    with operation("establish_websocket_connection"):
        try:
            info("Starting WebSocket connection establishment")

            # Load access token
            access_token = load_access_token(access_token_path)

            # Create WebSocket client
            global fyers  # Make it global to maintain original functionality
            fyers = create_fyers_connection(access_token)

            # Establish connection
            debug("Initiating WebSocket connection")
            fyers.connect()

            info("WebSocket connection establishment completed successfully")

        except SystemExit:
            # Re-raise SystemExit (from load_access_token) without logging again
            raise
        except Exception as e:
            error("Failed to establish WebSocket connection",
                  context={'error': str(e)},
                  exc_info=True)
            sys.exit(1)


def get_connection_status() -> Dict[str, Any]:
    """
    Get current connection status and statistics

    Returns:
        Dict containing connection status information
    """
    try:
        status_info = {
            'connection_active': False,
            'client_exists': False,
            'error': None
        }

        # Check if global fyers client exists
        if 'fyers' in globals():
            status_info['client_exists'] = True

            # Additional status checks could be added here
            # Note: FyersDataSocket may not have direct status methods
            # This would need to be implemented based on available API

        debug("Connection status retrieved", context=status_info)
        return status_info

    except Exception as e:
        error("Error retrieving connection status",
              context={'error': str(e)},
              exc_info=True)
        return {'error': str(e), 'connection_active': False, 'client_exists': False}


def cleanup_connection() -> None:
    """
    Cleanup WebSocket connection and resources
    """
    with operation("cleanup_websocket_connection"):
        try:
            if 'fyers' in globals() and fyers:
                debug("Cleaning up WebSocket connection")

                # Attempt to disconnect gracefully
                try:
                    fyers.disconnect()
                    info("WebSocket disconnected successfully")
                except Exception as e:
                    warning("Error during disconnect", context={'error': str(e)})

                # Clear global reference
                globals()['fyers'] = None

            debug("WebSocket cleanup completed")

        except Exception as e:
            error("Error during WebSocket cleanup",
                  context={'error': str(e)},
                  exc_info=True)


# Main execution with enhanced error handling
def main():
    """
    Main execution function with comprehensive logging and error handling
    """
    with operation("main_websocket_execution"):
        try:
            info("Starting Fyers WebSocket application")

            # Establish connection (this replaces the original direct execution)
            establish_connection()

            info("Fyers WebSocket application started successfully")

        except KeyboardInterrupt:
            info("Application interrupted by user")
            cleanup_connection()
            sys.exit(0)
        except SystemExit as e:
            # Allow clean system exits
            cleanup_connection()
            raise
        except Exception as e:
            error("Unexpected error in main execution",
                  context={'error': str(e)},
                  exc_info=True)
            cleanup_connection()
            sys.exit(1)


# Maintain backward compatibility - original code execution
if __name__ == "__main__":
    # Original execution preserved for compatibility
    try:
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

        info("Starting Fyers WebSocket connection",
             context={'access_token_length': len(access_token)})

        fyers.connect()

    except Exception as e:
        error("Application startup failed",
              context={'error': str(e)},
              exc_info=True)
        sys.exit(1)


# Export main functions for use by other modules
__all__ = [
    'load_access_token',
    'create_fyers_connection',
    'establish_connection',
    'get_connection_status',
    'cleanup_connection',
    'SettingsManager',
    'main'
]