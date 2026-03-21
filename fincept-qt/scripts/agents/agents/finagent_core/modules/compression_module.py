"""
Compression Module - Token optimization for financial agents

Provides:
- Tool result compression
- Large dataset summarization
- Context window management
- Smart truncation strategies
"""

from typing import Dict, Any, Optional, List, Callable
import logging
import json

logger = logging.getLogger(__name__)


class CompressionStrategy:
    """Base class for compression strategies."""

    def compress(self, data: Any, max_tokens: int = 1000) -> str:
        """Compress data to fit within token limit."""
        raise NotImplementedError


class TruncateStrategy(CompressionStrategy):
    """Simple truncation strategy."""

    def compress(self, data: Any, max_tokens: int = 1000) -> str:
        """Truncate data to fit within token limit."""
        text = json.dumps(data) if isinstance(data, (dict, list)) else str(data)

        # Rough token estimate: 1 token ≈ 4 characters
        max_chars = max_tokens * 4

        if len(text) <= max_chars:
            return text

        # Truncate with indicator
        return text[:max_chars - 20] + "\n... [TRUNCATED]"


class SummarizeStrategy(CompressionStrategy):
    """Summarization strategy using patterns."""

    def __init__(self):
        """Initialize summarization strategy."""
        self.patterns = {
            "timeseries": self._summarize_timeseries,
            "table": self._summarize_table,
            "list": self._summarize_list,
            "dict": self._summarize_dict
        }

    def compress(self, data: Any, max_tokens: int = 1000) -> str:
        """Summarize data to fit within token limit."""
        if isinstance(data, str):
            return TruncateStrategy().compress(data, max_tokens)

        # Detect data type and apply appropriate summarization
        data_type = self._detect_type(data)
        summarizer = self.patterns.get(data_type, self._default_summarize)

        return summarizer(data, max_tokens)

    def _detect_type(self, data: Any) -> str:
        """Detect data type for summarization."""
        if isinstance(data, list):
            if len(data) > 0 and isinstance(data[0], dict):
                # Check if it looks like timeseries
                if any(k in data[0] for k in ["date", "timestamp", "time", "Date"]):
                    return "timeseries"
                return "table"
            return "list"
        elif isinstance(data, dict):
            return "dict"
        return "default"

    def _summarize_timeseries(self, data: List[Dict], max_tokens: int) -> str:
        """Summarize timeseries data."""
        if not data:
            return "Empty timeseries"

        # Get key columns
        columns = list(data[0].keys())
        numeric_cols = [c for c in columns if isinstance(data[0].get(c), (int, float))]

        summary = {
            "type": "timeseries",
            "rows": len(data),
            "columns": columns,
            "date_range": {
                "start": data[0].get("date") or data[0].get("timestamp"),
                "end": data[-1].get("date") or data[-1].get("timestamp")
            }
        }

        # Calculate statistics for numeric columns
        stats = {}
        for col in numeric_cols[:5]:  # Limit to 5 columns
            values = [d.get(col) for d in data if d.get(col) is not None]
            if values:
                stats[col] = {
                    "min": min(values),
                    "max": max(values),
                    "avg": sum(values) / len(values),
                    "latest": values[-1]
                }

        summary["statistics"] = stats

        # Include first and last few rows
        summary["first_rows"] = data[:3]
        summary["last_rows"] = data[-3:]

        result = json.dumps(summary, indent=2, default=str)

        # Check if within limit
        if len(result) > max_tokens * 4:
            # Further compress by removing rows
            summary.pop("first_rows", None)
            summary.pop("last_rows", None)
            result = json.dumps(summary, indent=2, default=str)

        return result

    def _summarize_table(self, data: List[Dict], max_tokens: int) -> str:
        """Summarize tabular data."""
        if not data:
            return "Empty table"

        columns = list(data[0].keys()) if data else []

        summary = {
            "type": "table",
            "rows": len(data),
            "columns": columns,
            "sample_rows": data[:5] if len(data) > 5 else data
        }

        # Add column statistics for numeric columns
        numeric_cols = [c for c in columns if isinstance(data[0].get(c), (int, float))]
        for col in numeric_cols[:3]:
            values = [d.get(col) for d in data if d.get(col) is not None]
            if values:
                summary[f"{col}_stats"] = {
                    "min": min(values),
                    "max": max(values),
                    "avg": sum(values) / len(values)
                }

        result = json.dumps(summary, indent=2, default=str)

        if len(result) > max_tokens * 4:
            summary["sample_rows"] = data[:2]
            result = json.dumps(summary, indent=2, default=str)

        return result

    def _summarize_list(self, data: List, max_tokens: int) -> str:
        """Summarize list data."""
        summary = {
            "type": "list",
            "length": len(data),
            "sample": data[:10] if len(data) > 10 else data
        }

        if data and isinstance(data[0], (int, float)):
            summary["statistics"] = {
                "min": min(data),
                "max": max(data),
                "avg": sum(data) / len(data)
            }

        return json.dumps(summary, indent=2, default=str)

    def _summarize_dict(self, data: Dict, max_tokens: int) -> str:
        """Summarize dictionary data."""
        summary = {
            "type": "dict",
            "keys": list(data.keys())[:20],
            "total_keys": len(data)
        }

        # Include small values directly
        for key, value in list(data.items())[:10]:
            if isinstance(value, (str, int, float, bool)) or value is None:
                summary[key] = value
            elif isinstance(value, (list, dict)):
                summary[key] = f"<{type(value).__name__} with {len(value)} items>"

        return json.dumps(summary, indent=2, default=str)

    def _default_summarize(self, data: Any, max_tokens: int) -> str:
        """Default summarization."""
        return TruncateStrategy().compress(data, max_tokens)


class FinancialDataCompressor(CompressionStrategy):
    """
    Specialized compressor for financial data.

    Understands financial data structures and preserves
    important information while compressing.
    """

    def __init__(self):
        """Initialize financial data compressor."""
        self.important_fields = {
            "price", "close", "open", "high", "low", "volume",
            "market_cap", "pe_ratio", "eps", "dividend_yield",
            "symbol", "ticker", "date", "timestamp"
        }

    def compress(self, data: Any, max_tokens: int = 1000) -> str:
        """Compress financial data intelligently."""
        if isinstance(data, list) and len(data) > 0:
            return self._compress_financial_list(data, max_tokens)
        elif isinstance(data, dict):
            return self._compress_financial_dict(data, max_tokens)
        return SummarizeStrategy().compress(data, max_tokens)

    def _compress_financial_list(self, data: List, max_tokens: int) -> str:
        """Compress list of financial data."""
        if not isinstance(data[0], dict):
            return SummarizeStrategy().compress(data, max_tokens)

        # Identify important columns
        all_cols = set(data[0].keys())
        important = all_cols & self.important_fields
        other_cols = all_cols - important

        # Always keep important columns
        compressed_data = []
        for row in data:
            compressed_row = {k: row.get(k) for k in important if k in row}
            compressed_data.append(compressed_row)

        # Calculate statistics
        summary = {
            "type": "financial_timeseries",
            "rows": len(data),
            "all_columns": list(all_cols),
            "preserved_columns": list(important),
            "dropped_columns": list(other_cols)
        }

        # Price statistics
        if "close" in important or "price" in important:
            price_col = "close" if "close" in important else "price"
            prices = [d.get(price_col) for d in data if d.get(price_col)]
            if prices:
                summary["price_summary"] = {
                    "start": prices[0],
                    "end": prices[-1],
                    "high": max(prices),
                    "low": min(prices),
                    "change_pct": ((prices[-1] - prices[0]) / prices[0] * 100) if prices[0] else 0
                }

        # Volume statistics
        if "volume" in important:
            volumes = [d.get("volume") for d in data if d.get("volume")]
            if volumes:
                summary["volume_summary"] = {
                    "total": sum(volumes),
                    "avg": sum(volumes) / len(volumes),
                    "max": max(volumes)
                }

        # Date range
        if "date" in important or "timestamp" in important:
            date_col = "date" if "date" in important else "timestamp"
            dates = [d.get(date_col) for d in data if d.get(date_col)]
            if dates:
                summary["date_range"] = {
                    "start": dates[0],
                    "end": dates[-1]
                }

        # Include sample data
        max_sample = max(5, max_tokens // 100)
        summary["sample_data"] = compressed_data[:max_sample]

        return json.dumps(summary, indent=2, default=str)

    def _compress_financial_dict(self, data: Dict, max_tokens: int) -> str:
        """Compress financial dictionary."""
        # Prioritize important fields
        important_data = {k: v for k, v in data.items() if k in self.important_fields}
        other_data = {k: v for k, v in data.items() if k not in self.important_fields}

        summary = {
            "important_fields": important_data,
            "other_fields_count": len(other_data),
            "other_field_names": list(other_data.keys())[:20]
        }

        result = json.dumps(summary, indent=2, default=str)

        if len(result) > max_tokens * 4:
            # Further compress
            summary["other_field_names"] = summary["other_field_names"][:10]
            result = json.dumps(summary, indent=2, default=str)

        return result


class CompressionModule:
    """
    Compression manager for agent tool results.

    Integrates with Agno's compression_manager.
    """

    def __init__(
        self,
        strategy: str = "summarize",
        max_tokens: int = 2000,
        enable_financial: bool = True
    ):
        """
        Initialize compression module.

        Args:
            strategy: Compression strategy (truncate, summarize, financial)
            max_tokens: Maximum tokens for compressed output
            enable_financial: Enable financial data optimization
        """
        self.max_tokens = max_tokens
        self.enable_financial = enable_financial

        # Select strategy
        if strategy == "truncate":
            self.strategy = TruncateStrategy()
        elif strategy == "financial" or enable_financial:
            self.strategy = FinancialDataCompressor()
        else:
            self.strategy = SummarizeStrategy()

    def compress(self, data: Any) -> str:
        """Compress data using configured strategy."""
        return self.strategy.compress(data, self.max_tokens)

    def compress_tool_result(self, tool_name: str, result: Any) -> str:
        """
        Compress a tool result.

        Args:
            tool_name: Name of the tool
            result: Tool result data

        Returns:
            Compressed result string
        """
        # Check if result needs compression
        result_str = json.dumps(result, default=str) if not isinstance(result, str) else result

        # Rough token estimate
        estimated_tokens = len(result_str) // 4

        if estimated_tokens <= self.max_tokens:
            return result_str

        # Compress
        logger.debug(f"Compressing {tool_name} result from ~{estimated_tokens} to {self.max_tokens} tokens")

        compressed = self.compress(result)

        return f"[Compressed from ~{estimated_tokens} tokens]\n{compressed}"

    def to_agno_manager(self):
        """Convert to Agno CompressionManager."""
        try:
            from agno.compression import CompressionManager

            return CompressionManager(
                # Agno compression manager config
            )
        except ImportError:
            return None

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert to Agno agent config.

        Returns config for Agent initialization.
        """
        manager = self.to_agno_manager()

        config = {
            "compress_tool_results": True
        }

        if manager:
            config["compression_manager"] = manager

        return config

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "CompressionModule":
        """Create from configuration."""
        return cls(
            strategy=config.get("strategy", "summarize"),
            max_tokens=config.get("max_tokens", 2000),
            enable_financial=config.get("enable_financial", True)
        )

    @classmethod
    def for_financial_data(cls, max_tokens: int = 2000) -> "CompressionModule":
        """Create optimized for financial data."""
        return cls(
            strategy="financial",
            max_tokens=max_tokens,
            enable_financial=True
        )
