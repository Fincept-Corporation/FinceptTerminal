# notifications.py - Finance Terminal Notification System
"""
Notification wrapper system for finance terminal applications.
Uses notifypy for cross-platform desktop notifications.

Features:
- Tab-specific notification prefixes
- Integration with logger system
- Rate limiting to prevent spam
- Template-based notifications for common scenarios
- Configurable notification levels
- Silent mode for production environments
"""

import os
import time
import threading
from typing import Optional, Dict, Any, Callable, Union
from functools import wraps
from collections import defaultdict, deque
from enum import Enum
from datetime import datetime, timedelta
from pathlib import Path

try:
    from notifypy import Notify

    NOTIFYPY_AVAILABLE = True
except ImportError:
    NOTIFYPY_AVAILABLE = False

# Import our logger (adjust import as needed)
try:
    from logger import info, warning, error, debug

    LOGGER_AVAILABLE = True
except ImportError:
    LOGGER_AVAILABLE = False


    # Fallback to print if logger not available
    def info(msg, **kwargs):
        print(f"INFO: {msg}")


    def warning(msg, **kwargs):
        print(f"WARNING: {msg}")


    def error(msg, **kwargs):
        print(f"ERROR: {msg}")


    def debug(msg, **kwargs):
        print(f"DEBUG: {msg}")


class NotificationLevel(Enum):
    """Notification importance levels"""
    DEBUG = "debug"
    INFO = "info"
    SUCCESS = "success"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


class NotificationConfig:
    """Configuration for notification system"""

    def __init__(self):
        # Basic settings
        self.enabled = os.getenv('FINCEPT_NOTIFICATIONS', 'true').lower() == 'true'
        self.silent_mode = os.getenv('FINCEPT_SILENT_MODE', 'false').lower() == 'true'
        self.debug_notifications = os.getenv('FINCEPT_DEBUG_NOTIFICATIONS', 'false').lower() == 'true'

        # Rate limiting settings
        self.rate_limit_enabled = True
        self.max_notifications_per_minute = int(os.getenv('FINCEPT_MAX_NOTIFICATIONS_PER_MIN', '5'))
        self.duplicate_suppression_window = int(os.getenv('FINCEPT_DUPLICATE_WINDOW', '30'))  # seconds

        # Notification levels to show (can be configured via environment)
        enabled_levels = os.getenv('FINCEPT_NOTIFICATION_LEVELS', 'info,success,warning,error,critical')
        self.enabled_levels = set(level.strip().lower() for level in enabled_levels.split(','))

        # App settings
        self.app_name = os.getenv('FINCEPT_APP_NAME', 'Finance Terminal')
        self.app_icon = self._get_app_icon()

        # Tab prefixes for consistent branding
        self.tab_prefixes = {
            'chat': '💬 Chat',
            'forum': '📋 Forum',
            'dashboard': '📊 Dashboard',
            'market': '📈 Market',
            'analytics': '📉 Analytics',
            'database': '🗄️ Database',
            'api': '🌐 API',
            'session': '🔐 Session',
            'trading': '💰 Trading',
            'portfolio': '📦 Portfolio',
            'alerts': '🚨 Alerts',
            'main': '🏠 Main',
            'logger': '📝 Logger'
        }

    def _get_app_icon(self) -> Optional[str]:
        """Get application icon path"""
        icon_path = os.getenv('FINCEPT_ICON_PATH')
        if icon_path and Path(icon_path).exists():
            return icon_path

        # Look for common icon locations
        possible_paths = [
            Path.cwd() / 'assets' / 'icon.ico',
            Path.cwd() / 'assets' / 'icon.png',
            Path.cwd() / 'icon.ico',
            Path.cwd() / 'icon.png'
        ]

        for path in possible_paths:
            if path.exists():
                return str(path)

        return None

    def get_tab_prefix(self, module: Optional[str]) -> str:
        """Get display prefix for module/tab"""
        if not module:
            return '🏠'

        module_key = module.lower().split('_')[0]
        return self.tab_prefixes.get(module_key, f'📱 {module.title()}')


class NotificationRateLimiter:
    """Rate limiter to prevent notification spam"""

    def __init__(self, config: NotificationConfig):
        self.config = config
        self.notification_times = deque()
        self.recent_notifications = {}  # Hash -> timestamp for duplicate detection
        self._lock = threading.RLock()

    def should_allow(self, title: str, message: str, level: NotificationLevel) -> bool:
        """Check if notification should be allowed based on rate limiting"""
        if not self.config.rate_limit_enabled:
            return True

        current_time = time.time()

        with self._lock:
            # Clean old timestamps (older than 1 minute)
            cutoff_time = current_time - 60
            while self.notification_times and self.notification_times[0] < cutoff_time:
                self.notification_times.popleft()

            # Check rate limit
            if len(self.notification_times) >= self.config.max_notifications_per_minute:
                return False

            # Check for duplicates
            notification_hash = hash(f"{title}:{message}:{level.value}")
            if notification_hash in self.recent_notifications:
                last_time = self.recent_notifications[notification_hash]
                if current_time - last_time < self.config.duplicate_suppression_window:
                    return False

            # Allow notification
            self.notification_times.append(current_time)
            self.recent_notifications[notification_hash] = current_time

            # Clean old duplicates
            expired_hashes = [
                h for h, t in self.recent_notifications.items()
                if current_time - t > self.config.duplicate_suppression_window
            ]
            for h in expired_hashes:
                del self.recent_notifications[h]

            return True


class NotificationMetrics:
    """Track notification statistics"""

    def __init__(self):
        self.start_time = time.time()
        self.counts = defaultdict(int)
        self.rate_limited = 0
        self.failed_notifications = 0
        self._lock = threading.RLock()

    def record_sent(self, level: NotificationLevel):
        """Record a sent notification"""
        with self._lock:
            self.counts[level.value] += 1

    def record_rate_limited(self):
        """Record a rate-limited notification"""
        with self._lock:
            self.rate_limited += 1

    def record_failed(self):
        """Record a failed notification"""
        with self._lock:
            self.failed_notifications += 1

    def get_stats(self) -> Dict[str, Any]:
        """Get notification statistics"""
        with self._lock:
            return {
                'uptime_seconds': time.time() - self.start_time,
                'notifications_sent': dict(self.counts),
                'total_sent': sum(self.counts.values()),
                'rate_limited': self.rate_limited,
                'failed': self.failed_notifications
            }


class FinanceNotificationSystem:
    """Main notification system for finance terminal"""

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
        self.config = NotificationConfig()
        self.rate_limiter = NotificationRateLimiter(self.config)
        self.metrics = NotificationMetrics()

        # Check if notifications are available
        self.available = NOTIFYPY_AVAILABLE and self.config.enabled and not self.config.silent_mode

        if LOGGER_AVAILABLE:
            if self.available:
                info("Notification system initialized", module='notifications')
            else:
                warning("Notification system disabled or unavailable", module='notifications')

    def _create_notification(self, title: str, message: str, level: NotificationLevel) -> Optional[Notify]:
        """Create a notification object"""
        if not self.available:
            return None

        try:
            notification = Notify()
            notification.title = title
            notification.message = message
            notification.application_name = self.config.app_name

            if self.config.app_icon:
                notification.icon = self.config.app_icon

            return notification

        except Exception as e:
            if LOGGER_AVAILABLE:
                error(f"Failed to create notification: {e}", module='notifications')
            self.metrics.record_failed()
            return None

    def _send_notification(self, title: str, message: str, level: NotificationLevel,
                           module: Optional[str] = None, **kwargs) -> bool:
        """Core notification sending method"""

        # Check if this level should be shown
        if level.value not in self.config.enabled_levels:
            return False

        # Apply rate limiting
        if not self.rate_limiter.should_allow(title, message, level):
            self.metrics.record_rate_limited()
            if LOGGER_AVAILABLE and self.config.debug_notifications:
                debug(f"Rate limited notification: {title}", module='notifications')
            return False

        # Add tab prefix to title
        if module:
            tab_prefix = self.config.get_tab_prefix(module)
            title = f"{tab_prefix} {title}"

        # Log the notification attempt
        if LOGGER_AVAILABLE:
            info(f"Sending notification: {title}", module='notifications',
                 context={'level': level.value, 'source_module': module})

        # Create and send notification
        notification = self._create_notification(title, message, level)
        if notification:
            try:
                notification.send()
                self.metrics.record_sent(level)
                return True
            except Exception as e:
                if LOGGER_AVAILABLE:
                    error(f"Failed to send notification: {e}", module='notifications')
                self.metrics.record_failed()
                return False

        return False

    # Public notification methods
    def debug(self, title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
        """Send debug notification"""
        if not self.config.debug_notifications:
            return False
        return self._send_notification(title, message, NotificationLevel.DEBUG, module, **kwargs)

    def info(self, title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
        """Send info notification"""
        return self._send_notification(title, message, NotificationLevel.INFO, module, **kwargs)

    def success(self, title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
        """Send success notification"""
        return self._send_notification(title, message, NotificationLevel.SUCCESS, module, **kwargs)

    def warning(self, title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
        """Send warning notification"""
        return self._send_notification(title, message, NotificationLevel.WARNING, module, **kwargs)

    def error(self, title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
        """Send error notification"""
        return self._send_notification(title, message, NotificationLevel.ERROR, module, **kwargs)

    def critical(self, title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
        """Send critical notification"""
        return self._send_notification(title, message, NotificationLevel.CRITICAL, module, **kwargs)

    # Template-based notifications for common scenarios
    def trade_executed(self, symbol: str, action: str, quantity: int, price: float,
                       module: Optional[str] = 'trading') -> bool:
        """Template for trade execution notifications"""
        title = "Trade Executed"
        message = f"{action.upper()} {quantity} {symbol} @ ${price:.2f}"
        return self.success(title, message, module)

    def price_alert(self, symbol: str, current_price: float, target_price: float,
                    condition: str, module: Optional[str] = 'alerts') -> bool:
        """Template for price alert notifications"""
        title = f"Price Alert: {symbol}"
        message = f"Price ${current_price:.2f} {condition} target ${target_price:.2f}"
        return self.warning(title, message, module)

    def connection_status(self, service: str, status: str,
                          module: Optional[str] = 'api') -> bool:
        """Template for connection status notifications"""
        title = f"Connection {status.title()}"
        message = f"{service} connection is now {status.lower()}"

        if status.lower() in ['connected', 'restored']:
            return self.success(title, message, module)
        else:
            return self.error(title, message, module)

    def data_update(self, data_type: str, count: int,
                    module: Optional[str] = 'market') -> bool:
        """Template for data update notifications"""
        title = "Data Updated"
        message = f"{data_type}: {count} items updated"
        return self.info(title, message, module)

    def system_status(self, component: str, status: str, details: str = "",
                      module: Optional[str] = 'main') -> bool:
        """Template for system status notifications"""
        title = f"System {status.title()}"
        message = f"{component}: {details}" if details else component

        if status.lower() in ['started', 'ready', 'healthy']:
            return self.success(title, message, module)
        elif status.lower() in ['warning', 'degraded']:
            return self.warning(title, message, module)
        else:
            return self.error(title, message, module)

    # Configuration methods
    def enable(self, enabled: bool = True):
        """Enable or disable notifications"""
        self.config.enabled = enabled
        self.available = NOTIFYPY_AVAILABLE and enabled and not self.config.silent_mode

        if LOGGER_AVAILABLE:
            status = "enabled" if enabled else "disabled"
            info(f"Notifications {status}", module='notifications')

    def set_silent_mode(self, silent: bool = True):
        """Enable or disable silent mode"""
        self.config.silent_mode = silent
        self.available = NOTIFYPY_AVAILABLE and self.config.enabled and not silent

        if LOGGER_AVAILABLE:
            mode = "silent" if silent else "normal"
            info(f"Notification mode: {mode}", module='notifications')

    def set_debug_notifications(self, enabled: bool = True):
        """Enable or disable debug notifications"""
        self.config.debug_notifications = enabled

    def get_stats(self) -> Dict[str, Any]:
        """Get notification statistics"""
        stats = self.metrics.get_stats()
        stats.update({
            'config': {
                'enabled': self.config.enabled,
                'silent_mode': self.config.silent_mode,
                'available': self.available,
                'rate_limiting': self.config.rate_limit_enabled,
                'enabled_levels': list(self.config.enabled_levels)
            }
        })
        return stats

    def health_check(self) -> Dict[str, Any]:
        """Check notification system health"""
        try:
            if not NOTIFYPY_AVAILABLE:
                return {
                    'status': 'unavailable',
                    'reason': 'notifypy not installed'
                }

            if not self.config.enabled:
                return {
                    'status': 'disabled',
                    'reason': 'notifications disabled in config'
                }

            if self.config.silent_mode:
                return {
                    'status': 'silent',
                    'reason': 'silent mode enabled'
                }

            # Test notification (won't actually send due to rate limiting if called frequently)
            test_title = "Health Check"
            test_message = f"Notification system test at {datetime.now().strftime('%H:%M:%S')}"

            return {
                'status': 'healthy',
                'available': self.available,
                'stats': self.get_stats()
            }

        except Exception as e:
            return {
                'status': 'error',
                'error': str(e)
            }


# Global notification instance
notifier = FinanceNotificationSystem()


# Convenience functions (wrapper interface)
def notify_debug(title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
    """Send debug notification"""
    return notifier.debug(title, message, module, **kwargs)


def notify_info(title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
    """Send info notification"""
    return notifier.info(title, message, module, **kwargs)


def notify_success(title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
    """Send success notification"""
    return notifier.success(title, message, module, **kwargs)


def notify_warning(title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
    """Send warning notification"""
    return notifier.warning(title, message, module, **kwargs)


def notify_error(title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
    """Send error notification"""
    return notifier.error(title, message, module, **kwargs)


def notify_critical(title: str, message: str, module: Optional[str] = None, **kwargs) -> bool:
    """Send critical notification"""
    return notifier.critical(title, message, module, **kwargs)


# Template functions
def notify_trade_executed(symbol: str, action: str, quantity: int, price: float,
                          module: Optional[str] = 'trading') -> bool:
    """Notify trade execution"""
    return notifier.trade_executed(symbol, action, quantity, price, module)


def notify_price_alert(symbol: str, current_price: float, target_price: float, condition: str,
                       module: Optional[str] = 'alerts') -> bool:
    """Notify price alert"""
    return notifier.price_alert(symbol, current_price, target_price, condition, module)


def notify_connection_status(service: str, status: str, module: Optional[str] = 'api') -> bool:
    """Notify connection status"""
    return notifier.connection_status(service, status, module)


def notify_data_update(data_type: str, count: int, module: Optional[str] = 'market') -> bool:
    """Notify data update"""
    return notifier.data_update(data_type, count, module)


def notify_system_status(component: str, status: str, details: str = "", module: Optional[str] = 'main') -> bool:
    """Notify system status"""
    return notifier.system_status(component, status, details, module)


# Decorator for automatic notifications
def notify_on_completion(title_template: str = "Operation Completed",
                         success_message: str = "Operation completed successfully",
                         error_message: str = "Operation failed",
                         module: Optional[str] = None):
    """Decorator to automatically send notifications on function completion"""

    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            func_module = module or func.__module__.split('.')[-1]
            title = title_template.format(func_name=func.__name__)

            try:
                result = func(*args, **kwargs)
                notify_success(title, success_message, func_module)
                return result
            except Exception as e:
                error_msg = f"{error_message}: {str(e)}"
                notify_error(title, error_msg, func_module)
                raise

        return wrapper

    return decorator


def notify_on_error(title_template: str = "Error in {func_name}",
                    message_template: str = "An error occurred: {error}",
                    module: Optional[str] = None):
    """Decorator to send notifications only on errors"""

    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                func_module = module or func.__module__.split('.')[-1]
                title = title_template.format(func_name=func.__name__)
                message = message_template.format(error=str(e))
                notify_error(title, message, func_module)
                raise

        return wrapper

    return decorator


# Configuration functions
def enable_notifications(enabled: bool = True):
    """Enable or disable notifications"""
    notifier.enable(enabled)


def set_silent_mode(silent: bool = True):
    """Set silent mode"""
    notifier.set_silent_mode(silent)


def set_debug_notifications(enabled: bool = True):
    """Enable debug notifications"""
    notifier.set_debug_notifications(enabled)


def get_notification_stats():
    """Get notification statistics"""
    return notifier.get_stats()


def notification_health_check():
    """Check notification system health"""
    return notifier.health_check()


# Export main components
__all__ = [
    # Main notifier
    'notifier',
    # Basic notification functions
    'notify_debug', 'notify_info', 'notify_success', 'notify_warning', 'notify_error', 'notify_critical',
    # Template functions
    'notify_trade_executed', 'notify_price_alert', 'notify_connection_status',
    'notify_data_update', 'notify_system_status',
    # Decorators
    'notify_on_completion', 'notify_on_error',
    # Configuration
    'enable_notifications', 'set_silent_mode', 'set_debug_notifications',
    # Utilities
    'get_notification_stats', 'notification_health_check',
    # Enums
    'NotificationLevel'
]