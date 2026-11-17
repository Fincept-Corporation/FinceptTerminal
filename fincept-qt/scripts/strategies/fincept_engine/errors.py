# ============================================================================
# Fincept Terminal - Strategy Engine Errors
# One exception for "the strategy asked for something this engine cannot do".
# ============================================================================


class NotSupportedByEngine(Exception):
    """Raised when a strategy needs data or a feature the Fincept backtest engine
    does not provide (option/future chains, fundamentals, universes, tick data, ...).

    The message is shown to the user as the run's error, so it says what is missing
    in plain words instead of surfacing an AttributeError from a stub. The runner
    recognises this type and reports the run as "not supported" rather than as a crash.
    """

    def __init__(self, feature: str, detail: str = ""):
        self.feature = feature
        msg = f"This strategy needs {feature}, which the Fincept backtest engine does not provide."
        if detail:
            msg += f" {detail}"
        super().__init__(msg)


class ArgumentException(ValueError):
    """System.ArgumentException (e.g. an option price model that does not support the style)."""


class ArgumentOutOfRangeException(ArgumentException):
    pass


class AggregateException(Exception):
    pass


class KeyNotFoundException(KeyError):
    pass


class NotSupportedException(Exception):
    pass


class NullReferenceException(AttributeError):
    pass


def not_supported(feature: str, detail: str = "") -> NotSupportedByEngine:
    """Build the exception (callers `raise not_supported(...)`)."""
    return NotSupportedByEngine(feature, detail)


class _UnsupportedAttribute:
    """Descriptor for an API the engine deliberately does not implement.

    Reading the attribute raises NotSupportedByEngine with a clear message instead
    of an AttributeError (or a stub that silently returns something else).
    """

    def __init__(self, feature: str, detail: str = ""):
        self._feature = feature
        self._detail = detail

    def __set_name__(self, owner, name):
        self._name = name

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        raise NotSupportedByEngine(self._feature, self._detail)


def unsupported_attribute(feature: str, detail: str = "") -> _UnsupportedAttribute:
    return _UnsupportedAttribute(feature, detail)
