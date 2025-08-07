# logger.py - Production-Ready Finance Terminal Logger
"""
Optimized logging module for finance terminal applications.
Focus: Performance, Simplicity, Reliability, Thread Safety.

Key Features:
- Tab-specific prefixes for GUI debugging
- Minimal overhead design
- Thread-safe operations
- Automatic log rotation
- Environment-based configuration
- Memory-efficient buffering
"""

import os
import sys
import logging
import logging.handlers
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Dict, Any, List, Union
from functools import wraps, lru_cache
from collections import deque
from contextlib import contextmanager
import json
import platform


class LogConfig:
    """Simplified configuration with sensible defaults"""

    def __init__(self):
        # Core settings from environment
        self.debug_mode = os.getenv('FINCEPT_DEBUG', 'false').lower() == 'true'
        self.log_level = getattr(logging, os.getenv('FINCEPT_LOG_LEVEL', 'INFO').upper(), logging.INFO)
        self.console_enabled = os.getenv('FINCEPT_CONSOLE_LOG', 'false').lower() == 'true'

        # File rotation settings
        self.max_file_size = int(os.getenv('FINCEPT_MAX_LOG_SIZE', str(50 * 1024 * 1024)))  # 50MB
        self.backup_count = int(os.getenv('FINCEPT_BACKUP_COUNT', '10'))
        self.retention_days = int(os.getenv('FINCEPT_RETENTION_DAYS', '30'))

        # Performance settings
        self.buffer_size = 1000  # Reasonable buffer size
        self.flush_interval = 30.0  # Seconds

        # Tab prefix mapping (simplified)
        self.tab_prefixes = {
            'chat': 'CHAT',
            'forum': 'FORUM',
            'dashboard': 'DASH',
            'market': 'MARKET',
            'analytics': 'ANALYTICS',
            'database': 'DB',
            'api': 'API',
            'session': 'SESSION',
            'theme': 'THEME',
            'main': 'MAIN',
            'logger': 'LOG'
        }

    @lru_cache(maxsize=128)
    def get_tab_prefix(self, module: Optional[str]) -> str:
        """Fast cached prefix lookup"""
        if not module:
            return 'APP'

        module_key = module.lower().split('_')[0]  # Take first part
        return self.tab_prefixes.get(module_key, 'APP')

    def get_logs_dir(self) -> Path:
        """Determine logs directory with fallback chain"""
        # 1. Environment variable (highest priority)
        if env_dir := os.getenv('FINCEPT_LOGS_DIR'):
            return Path(env_dir).expanduser()

        # 2. Application data directory
        system = platform.system()
        if system == 'Windows':
            base = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local'))
            return base / 'FinanceTerminal' / 'logs'
        elif system == 'Darwin':
            return Path.home() / 'Library/Application Support/FinanceTerminal/logs'
        else:  # Linux/Unix
            return Path.home() / '.local/share/finance-terminal/logs'


class PerformanceMetrics:
    """Lightweight metrics collection"""

    def __init__(self):
        self.start_time = time.time()
        self.log_counts = {'DEBUG': 0, 'INFO': 0, 'WARNING': 0, 'ERROR': 0, 'CRITICAL': 0}
        self.recent_errors = deque(maxlen=20)
        self._lock = threading.RLock()

    def record_log(self, level_name: str, message: str = None, module: str = None):
        """Record log entry with minimal overhead"""
        with self._lock:
            self.log_counts[level_name] = self.log_counts.get(level_name, 0) + 1

            if level_name in ('ERROR', 'CRITICAL') and message:
                self.recent_errors.append({
                    'timestamp': time.time(),
                    'level': level_name,
                    'message': message[:200],  # Truncate long messages
                    'module': module or 'unknown'
                })

    def get_summary(self) -> Dict[str, Any]:
        """Get metrics summary"""
        with self._lock:
            return {
                'uptime_seconds': time.time() - self.start_time,
                'log_counts': self.log_counts.copy(),
                'recent_errors': len(self.recent_errors),
                'total_logs': sum(self.log_counts.values())
            }


class TabFormatter(logging.Formatter):
    """Optimized formatter with tab prefixes"""

    def __init__(self, config: LogConfig):
        self.config = config
        # Pre-compile format string for performance
        super().__init__(
            fmt='%(asctime)s [%(levelname)7s] [%(tab_prefix)s] %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )

    def format(self, record):
        # Add tab prefix to record
        module = getattr(record, 'module', getattr(record, 'name', None))
        record.tab_prefix = self.config.get_tab_prefix(module)
        return super().format(record)


class FinanceTerminalLogger:
    """Production-ready logger for finance terminal"""

    _instance = None
    _lock = threading.RLock()

    def __new__(cls):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self):
        if hasattr(self, '_initialized'):
            return

        self._initialized = True
        self.config = LogConfig()
        self.metrics = PerformanceMetrics()

        # Setup logger
        self.logger = logging.getLogger('finance_terminal')
        self.logger.setLevel(logging.DEBUG)

        # Clear any existing handlers
        self.logger.handlers.clear()

        # Setup handlers
        self._setup_file_handler()
        if self.config.console_enabled:
            self._setup_console_handler()

        # Prevent propagation to root logger
        self.logger.propagate = False

        # Start background maintenance
        self._start_maintenance_thread()

        # Log initialization
        self.info("Finance terminal logger initialized", module='logger')

    def _setup_file_handler(self):
        """Setup rotating file handler"""
        try:
            logs_dir = self.config.get_logs_dir()
            logs_dir.mkdir(parents=True, exist_ok=True)

            log_file = logs_dir / 'finance_terminal.log'

            # Use RotatingFileHandler for automatic rotation
            file_handler = logging.handlers.RotatingFileHandler(
                log_file,
                maxBytes=self.config.max_file_size,
                backupCount=self.config.backup_count,
                encoding='utf-8'
            )

            file_handler.setFormatter(TabFormatter(self.config))
            file_handler.setLevel(logging.DEBUG)

            self.logger.addHandler(file_handler)
            self.file_handler = file_handler

        except Exception as e:
            # Fallback to console only
            print(f"Failed to setup file logging: {e}")

    def _setup_console_handler(self):
        """Setup console handler"""
        console_handler = logging.StreamHandler(sys.stdout)
        console_formatter = TabFormatter(self.config)
        console_handler.setFormatter(console_formatter)

        # Set console level based on config
        level = logging.DEBUG if self.config.debug_mode else logging.INFO
        console_handler.setLevel(level)

        self.logger.addHandler(console_handler)
        self.console_handler = console_handler

    def _start_maintenance_thread(self):
        """Start background maintenance thread"""

        def maintenance():
            while True:
                try:
                    time.sleep(300)  # Run every 5 minutes
                    self._cleanup_old_logs()
                    self._flush_handlers()
                except Exception:
                    pass  # Silent maintenance

        thread = threading.Thread(target=maintenance, daemon=True, name='LogMaintenance')
        thread.start()

    def _cleanup_old_logs(self):
        """Clean up old log files"""
        try:
            logs_dir = self.config.get_logs_dir()
            cutoff = datetime.now() - timedelta(days=self.config.retention_days)

            for log_file in logs_dir.glob('*.log*'):
                if log_file.stat().st_mtime < cutoff.timestamp():
                    log_file.unlink()

        except Exception:
            pass

    def _flush_handlers(self):
        """Flush all handlers"""
        try:
            for handler in self.logger.handlers:
                handler.flush()
        except Exception:
            pass

    def _log(self, level: int, message: str, module: Optional[str] = None,
             context: Optional[Dict[str, Any]] = None, exc_info: bool = False):
        """Core logging method"""
        try:
            # Format message with context
            if context:
                context_str = ' | '.join(f"{k}={v}" for k, v in context.items())
                full_message = f"{message} | {context_str}"
            else:
                full_message = message

            # Create log record
            record = self.logger.makeRecord(
                name=self.logger.name,
                level=level,
                fn='', lno=0,
                msg=full_message,
                args=(),
                exc_info=sys.exc_info() if exc_info else None
            )

            # Add module info
            record.module = module

            # Handle the record
            self.logger.handle(record)

            # Update metrics
            level_name = logging.getLevelName(level)
            self.metrics.record_log(level_name, full_message, module)

        except Exception:
            # Silent failure to prevent logging loops
            pass

    # Public logging methods
    def debug(self, message: str, module: Optional[str] = None,
              context: Optional[Dict[str, Any]] = None):
        """Log debug message"""
        self._log(logging.DEBUG, message, module, context)

    def info(self, message: str, module: Optional[str] = None,
             context: Optional[Dict[str, Any]] = None):
        """Log info message"""
        self._log(logging.INFO, message, module, context)

    def warning(self, message: str, module: Optional[str] = None,
                context: Optional[Dict[str, Any]] = None):
        """Log warning message"""
        self._log(logging.WARNING, message, module, context)

    def error(self, message: str, module: Optional[str] = None,
              context: Optional[Dict[str, Any]] = None, exc_info: bool = False):
        """Log error message"""
        self._log(logging.ERROR, message, module, context, exc_info)

    def critical(self, message: str, module: Optional[str] = None,
                 context: Optional[Dict[str, Any]] = None, exc_info: bool = False):
        """Log critical message"""
        self._log(logging.CRITICAL, message, module, context, exc_info)

    @contextmanager
    def operation(self, name: str, module: Optional[str] = None, **kwargs):
        """Context manager for operation logging"""
        start_time = time.time()
        self.debug(f"Starting: {name}", module=module, context=kwargs)

        try:
            yield
            duration = time.time() - start_time
            self.debug(f"Completed: {name}", module=module,
                       context={'duration_ms': f"{duration * 1000:.1f}"})
        except Exception as e:
            duration = time.time() - start_time
            self.error(f"Failed: {name}", module=module,
                       context={'duration_ms': f"{duration * 1000:.1f}", 'error': str(e)},
                       exc_info=True)
            raise

    def monitor_performance(self, func):
        """Performance monitoring decorator"""

        @wraps(func)
        def wrapper(*args, **kwargs):
            module_name = func.__module__.split('.')[-1] if func.__module__ else 'unknown'
            with self.operation(func.__name__, module=module_name):
                return func(*args, **kwargs)

        return wrapper

    # Configuration methods
    def set_level(self, level: Union[int, str]):
        """Set logging level"""
        if isinstance(level, str):
            level = getattr(logging, level.upper())
        self.logger.setLevel(level)

    def enable_console(self, enable: bool = True):
        """Enable/disable console logging"""
        if enable and not hasattr(self, 'console_handler'):
            self._setup_console_handler()
        elif not enable and hasattr(self, 'console_handler'):
            self.logger.removeHandler(self.console_handler)
            delattr(self, 'console_handler')

    def get_stats(self) -> Dict[str, Any]:
        """Get logging statistics"""
        stats = self.metrics.get_summary()
        stats['config'] = {
            'debug_mode': self.config.debug_mode,
            'console_enabled': self.config.console_enabled,
            'logs_directory': str(self.config.get_logs_dir())
        }
        return stats

    def get_recent_errors(self, limit: int = 10) -> List[Dict[str, Any]]:
        """Get recent error messages"""
        with self.metrics._lock:
            errors = list(self.metrics.recent_errors)[-limit:]
            for error in errors:
                error['timestamp'] = datetime.fromtimestamp(error['timestamp']).isoformat()
            return errors

    def health_check(self) -> Dict[str, Any]:
        """Check logger health"""
        try:
            test_message = f"Health check at {datetime.now().isoformat()}"
            self.debug(test_message, module='logger')

            return {
                'status': 'healthy',
                'logs_directory': str(self.config.get_logs_dir()),
                'stats': self.get_stats()
            }
        except Exception as e:
            return {
                'status': 'unhealthy',
                'error': str(e)
            }


# Global logger instance
logger = FinanceTerminalLogger()


# Convenience functions
def debug(message: str, module: Optional[str] = None, context: Optional[Dict[str, Any]] = None):
    logger.debug(message, module, context)


def info(message: str, module: Optional[str] = None, context: Optional[Dict[str, Any]] = None):
    logger.info(message, module, context)


def warning(message: str, module: Optional[str] = None, context: Optional[Dict[str, Any]] = None):
    logger.warning(message, module, context)


def error(message: str, module: Optional[str] = None, context: Optional[Dict[str, Any]] = None, exc_info: bool = False):
    logger.error(message, module, context, exc_info)


def critical(message: str, module: Optional[str] = None, context: Optional[Dict[str, Any]] = None,
             exc_info: bool = False):
    logger.critical(message, module, context, exc_info)


def operation(name: str, module: Optional[str] = None, **kwargs):
    return logger.operation(name, module, **kwargs)


def monitor_performance(func):
    return logger.monitor_performance(func)


def set_debug_mode(enable: bool):
    """Enable/disable debug mode"""
    logger.config.debug_mode = enable
    logger.set_level(logging.DEBUG if enable else logging.INFO)
    if hasattr(logger, 'console_handler'):
        logger.console_handler.setLevel(logging.DEBUG if enable else logging.INFO)


def get_stats():
    return logger.get_stats()


def health_check():
    return logger.health_check()


# Quick setup functions
def setup_for_development():
    """Configure for development"""
    set_debug_mode(True)
    logger.enable_console(True)
    info("Development logging configured", module='logger')


def setup_for_production():
    """Configure for production"""
    set_debug_mode(False)
    logger.enable_console(False)
    info("Production logging configured", module='logger')


def setup_for_gui():
    """Configure for GUI application"""
    set_debug_mode(False)
    logger.enable_console(False)  # No console in GUI
    info("GUI logging configured", module='logger')


# Export main components
__all__ = [
    'logger', 'debug', 'info', 'warning', 'error', 'critical',
    'operation', 'monitor_performance', 'set_debug_mode',
    'get_stats', 'health_check', 'setup_for_development',
    'setup_for_production', 'setup_for_gui'
]
