"""
Logging utilities for Alpha Arena using loguru.
"""

import sys
from typing import Optional
from pathlib import Path

try:
    from loguru import logger
    LOGURU_AVAILABLE = True
except ImportError:
    import logging
    logger = logging.getLogger("alpha_arena")
    LOGURU_AVAILABLE = False


def setup_logging(
    level: str = "INFO",
    log_file: Optional[str] = None,
    rotation: str = "10 MB",
    retention: str = "7 days",
) -> None:
    """Setup logging configuration.

    Args:
        level: Log level (DEBUG, INFO, WARNING, ERROR)
        log_file: Optional file path for logging
        rotation: Log rotation size
        retention: Log retention period
    """
    if LOGURU_AVAILABLE:
        # Remove default handler
        logger.remove()

        # Add stderr handler with formatting
        logger.add(
            sys.stderr,
            format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
            level=level,
            colorize=True,
        )

        # Add file handler if specified
        if log_file:
            log_path = Path(log_file)
            log_path.parent.mkdir(parents=True, exist_ok=True)

            logger.add(
                log_file,
                format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {name}:{function}:{line} - {message}",
                level=level,
                rotation=rotation,
                retention=retention,
                compression="gz",
            )
    else:
        # Fallback to standard logging
        logging.basicConfig(
            level=getattr(logging, level),
            format="%(asctime)s | %(levelname)-8s | %(name)s:%(funcName)s:%(lineno)d - %(message)s",
            stream=sys.stderr,
        )


def get_logger(name: str = "alpha_arena"):
    """Get a logger instance.

    Args:
        name: Logger name (used for context in loguru)

    Returns:
        Logger instance
    """
    if LOGURU_AVAILABLE:
        return logger.bind(name=name)
    else:
        return logging.getLogger(name)


# Convenience logging functions
def log_info(message: str, **kwargs):
    """Log info message."""
    if LOGURU_AVAILABLE:
        logger.info(message, **kwargs)
    else:
        logger.info(message % kwargs if kwargs else message)


def log_debug(message: str, **kwargs):
    """Log debug message."""
    if LOGURU_AVAILABLE:
        logger.debug(message, **kwargs)
    else:
        logger.debug(message % kwargs if kwargs else message)


def log_warning(message: str, **kwargs):
    """Log warning message."""
    if LOGURU_AVAILABLE:
        logger.warning(message, **kwargs)
    else:
        logger.warning(message % kwargs if kwargs else message)


def log_error(message: str, **kwargs):
    """Log error message."""
    if LOGURU_AVAILABLE:
        logger.error(message, **kwargs)
    else:
        logger.error(message % kwargs if kwargs else message)


def log_exception(message: str, **kwargs):
    """Log exception with traceback."""
    if LOGURU_AVAILABLE:
        logger.exception(message, **kwargs)
    else:
        logger.exception(message % kwargs if kwargs else message)
