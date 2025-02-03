import logging
import sys

logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout),  # Log to terminal
        logging.FileHandler("fincept_terminal.log", mode="a", encoding="utf-8"),  # Log to file
    ]
)


# Example usage
logging.info("🚀 Logging initialized. Saving logs to fincept_terminal.log")