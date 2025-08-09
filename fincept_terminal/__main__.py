#!/usr/bin/env python3
"""
Fincept Terminal - Entry Point
This file serves as the entry point when FinceptTerminal.exe is executed.
"""

import sys
import os

# Add the current directory to Python path for imports
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

# Add parent directory to path (for fincept_terminal imports)
parent_dir = os.path.dirname(current_dir)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)


def main():
    """Main entry point for the Fincept Terminal application"""
    try:
        # Import logger first
        from fincept_terminal.Utils.Logging.logger import logger
        logger.info("Starting Fincept Terminal...", module="__Main__")

        # Import and run the main application
        from FinceptTerminalStart import main as run_main
        run_main()

    except ImportError as ie:
        print(f"Import Error: {ie}")
        print("Please ensure all dependencies are installed.")
        print("Current working directory:", os.getcwd())
        print("Current Python path:", current_dir)
        input("Press Enter to exit...")
        sys.exit(1)

    except KeyboardInterrupt:
        try:
            from fincept_terminal.Utils.Logging.logger import logger
            logger.info("\nGoodbye!", module="__Main__")
        except:
            print("\nGoodbye!")
        sys.exit(0)

    except Exception as e:
        try:
            from fincept_terminal.Utils.Logging.logger import logger
            logger.error(f"Error starting Fincept Terminal: {e}", module="__Main__", context={'error': str(e)})
        except:
            print(f"Error starting Fincept Terminal: {e}")

        print(f"Error details: {e}")
        print("Current working directory:", os.getcwd())
        input("Press Enter to exit...")
        sys.exit(1)


if __name__ == "__main__":
    main()