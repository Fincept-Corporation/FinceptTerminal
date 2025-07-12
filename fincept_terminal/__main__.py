#!/usr/bin/env python3
"""
Fincept Terminal - Entry Point
This file serves as the entry point when 'fincept' command is executed.
"""

import sys
import os

# Add the current directory to Python path for imports
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

def main():
    """Main entry point for the fincept command"""
    try:
        # Import and run the main application
        from main_dashboard import main as run_main
        run_main()
    except KeyboardInterrupt:
        print("\n👋 Goodbye!")
        sys.exit(0)
    except Exception as e:
        print(f"❌ Error starting Fincept Terminal: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()