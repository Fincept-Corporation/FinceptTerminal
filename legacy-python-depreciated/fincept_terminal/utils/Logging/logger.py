# logger.py

import os
import sys
import logging
import logging.handlers
import threading
import time
import inspect
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Dict, Any, List, Union
from functools import wraps, lru_cache
from collections import deque
from contextlib import contextmanager
import json
import platform
import re


class LogConfig:
    """Simplified configuration with automatic class detection"""

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
        self.buffer_size = 1000
        self.flush_interval = 30.0

    @lru_cache(maxsize=256)
    def get_class_prefix(self, class_name: Optional[str]) -> str:
        """Generate intelligent prefix from class name with caching"""
        if not class_name:
            return 'APP'

        # Handle common patterns and convert to readable prefix
        class_name = class_name.strip()

        # Remove common suffixes
        suffixes_to_remove = ['Tab', 'Manager', 'Handler', 'Service', 'Client', 'Controller', 'Helper']
        for suffix in suffixes_to_remove:
            if class_name.endswith(suffix):
                class_name = class_name[:-len(suffix)]
                break

        # Handle camelCase/PascalCase by extracting meaningful parts
        # e.g., "HelpTab" -> "HELP", "MarketDataTab" -> "MARKET_DATA", "SessionManager" -> "SESSION"
        words = re.findall(r'[A-Z][a-z]*', class_name)
        if words:
            if len(words) == 1:
                return words[0].upper()
            elif len(words) <= 3:  # Avoid very long prefixes
                return '_'.join(word.upper() for word in words)
            else:
                # Take first and last word for very long class names
                return f"{words[0].upper()}_{words[-1].upper()}"

        # Fallback for non-standard naming
        return class_name.upper()[:10]  # Limit length

    def get_logs_dir(self) -> Path:
        """Get logs directory - uses .fincept/logs folder"""
        # 1. Environment variable (highest priority)
        if env_dir := os.getenv('FINCEPT_LOGS_DIR'):
            return Path(env_dir).expanduser()

        # 2. Use .fincept/logs folder at home directory for all platforms
        logs_dir = Path.home() / '.fincept' / 'logs'
        return logs_dir


class PerformanceMetrics:
    """Lightweight metrics collection"""

    def __init__(self):
        self.start_time = time.time()
        self.log_counts = {'DEBUG': 0, 'INFO': 0, 'WARNING': 0, 'ERROR': 0, 'CRITICAL': 0}
        self.recent_errors = deque(maxlen=20)
        self.class_usage = {}  # Track which classes are logging
        self._lock = threading.RLock()

    def record_log(self, level_name: str, message: str = None, class_name: str = None):
        """Record log entry with minimal overhead"""
        with self._lock:
            self.log_counts[level_name] = self.log_counts.get(level_name, 0) + 1

            if class_name:
                self.class_usage[class_name] = self.class_usage.get(class_name, 0) + 1

            if level_name in ('ERROR', 'CRITICAL') and message:
                self.recent_errors.append({
                    'timestamp': time.time(),
                    'level': level_name,
                    'message': message[:200],
                    'class': class_name or 'unknown'
                })

    def get_summary(self) -> Dict[str, Any]:
        """Get metrics summary"""
        with self._lock:
            return {
                'uptime_seconds': time.time() - self.start_time,
                'log_counts': self.log_counts.copy(),
                'recent_errors': len(self.recent_errors),
                'total_logs': sum(self.log_counts.values()),
                'active_classes': len(self.class_usage),
                'top_logging_classes': sorted(self.class_usage.items(), key=lambda x: x[1], reverse=True)[:10]
            }


class AutoDetectFormatter(logging.Formatter):
    """Formatter with automatic class detection"""

    def __init__(self, config: LogConfig):
        self.config = config
        super().__init__(
            fmt='%(asctime)s [%(levelname)7s] [%(class_prefix)s] %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )

    def format(self, record):
        # Get class prefix from the record (set by logger)
        class_name = getattr(record, 'detected_class', None)
        record.class_prefix = self.config.get_class_prefix(class_name)
        return super().format(record)


class StackInspector:
    """Efficient stack inspection for class detection"""

    @staticmethod
    @lru_cache(maxsize=512)
    def get_caller_class_name(skip_frames: int = 3) -> Optional[str]:
        """
        Get the class name of the calling code with caching.
        skip_frames: number of frames to skip (logger internals)
        """
        try:
            # Get the current stack
            frame = inspect.currentframe()

            # Skip logger internal frames
            for _ in range(skip_frames):
                if frame is None:
                    return None
                frame = frame.f_back

            if frame is None:
                return None

            # Look for 'self' in local variables (indicates instance method)
            local_vars = frame.f_locals
            if 'self' in local_vars:
                return local_vars['self'].__class__.__name__

            # Fallback: try to get class from the code object
            code = frame.f_code
            if code.co_name != '<module>':
                # Look for class in global variables
                global_vars = frame.f_globals
                for name, obj in global_vars.items():
                    if inspect.isclass(obj) and hasattr(obj, code.co_name):
                        return name

            # Last resort: use module name
            module_name = frame.f_globals.get('__name__', '')
            if module_name:
                return module_name.split('.')[-1]

            return None

        except Exception:
            return None
        finally:
            del frame  # Prevent reference cycles


class FinanceTerminalLogger:
    """Production-ready logger with automatic class detection"""

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
        self.stack_inspector = StackInspector()

        # Setup logger
        self.logger = logging.getLogger('finance_terminal')
        self.logger.setLevel(logging.DEBUG)
        self.logger.handlers.clear()

        # Setup handlers
        self._setup_file_handler()
        if self.config.console_enabled:
            self._setup_console_handler()

        self.logger.propagate = False
        self._start_maintenance_thread()

        # Log initialization without recursion
        self._direct_log(logging.INFO, "Finance terminal logger initialized", "FinanceTerminalLogger")

    def _setup_file_handler(self):
        """Setup rotating file handler"""
        try:
            logs_dir = self.config.get_logs_dir()
            logs_dir.mkdir(parents=True, exist_ok=True)

            log_file = logs_dir / 'finance_terminal.log'
            file_handler = logging.handlers.RotatingFileHandler(
                log_file,
                maxBytes=self.config.max_file_size,
                backupCount=self.config.backup_count,
                encoding='utf-8'
            )

            file_handler.setFormatter(AutoDetectFormatter(self.config))
            file_handler.setLevel(logging.DEBUG)
            self.logger.addHandler(file_handler)
            self.file_handler = file_handler

        except Exception as e:
            print(f"Failed to setup file logging: {e}")

    def _setup_console_handler(self):
        """Setup console handler"""
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(AutoDetectFormatter(self.config))
        level = logging.DEBUG if self.config.debug_mode else logging.INFO
        console_handler.setLevel(level)
        self.logger.addHandler(console_handler)
        self.console_handler = console_handler

    def _start_maintenance_thread(self):
        """Start background maintenance thread"""

        def maintenance():
            while True:
                try:
                    time.sleep(300)
                    self._cleanup_old_logs()
                    self._flush_handlers()
                except Exception:
                    pass

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

    def _direct_log(self, level: int, message: str, class_name: str,
                    context: Optional[Dict[str, Any]] = None, exc_info: bool = False):
        """Direct logging method without stack inspection"""
        try:
            if context:
                context_str = ' | '.join(f"{k}={v}" for k, v in context.items())
                full_message = f"{message} | {context_str}"
            else:
                full_message = message

            record = self.logger.makeRecord(
                name=self.logger.name,
                level=level,
                fn='', lno=0,
                msg=full_message,
                args=(),
                exc_info=sys.exc_info() if exc_info else None
            )

            record.detected_class = class_name
            self.logger.handle(record)

            level_name = logging.getLevelName(level)
            self.metrics.record_log(level_name, full_message, class_name)

        except Exception:
            pass

    def _log(self, level: int, message: str, module: Optional[str] = None,
             context: Optional[Dict[str, Any]] = None, exc_info: bool = False):
        """Core logging method with automatic class detection"""
        try:
            # Auto-detect class name if not provided
            if module:
                class_name = module
            else:
                class_name = self.stack_inspector.get_caller_class_name()

            self._direct_log(level, message, class_name, context, exc_info)

        except Exception:
            pass

    # Public logging methods - same interface as before!
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
            # Try to get class name from self parameter
            class_name = None
            if args and hasattr(args[0], '__class__'):
                class_name = args[0].__class__.__name__

            with self.operation(func.__name__, module=class_name):
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
            self._direct_log(logging.DEBUG, test_message, "HealthCheck")
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


# Convenience functions - SAME INTERFACE AS BEFORE!
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
    logger._direct_log(logging.INFO, "Development logging configured", "Setup")


def setup_for_production():
    """Configure for production"""
    set_debug_mode(False)
    logger.enable_console(False)
    logger._direct_log(logging.INFO, "Production logging configured", "Setup")


def setup_for_gui():
    """Configure for GUI application"""
    set_debug_mode(False)
    logger.enable_console(False)
    logger._direct_log(logging.INFO, "GUI logging configured", "Setup")


# Export main components
__all__ = [
    'logger', 'debug', 'info', 'warning', 'error', 'critical',
    'operation', 'monitor_performance', 'set_debug_mode',
    'get_stats', 'health_check', 'setup_for_development',
    'setup_for_production', 'setup_for_gui'
]