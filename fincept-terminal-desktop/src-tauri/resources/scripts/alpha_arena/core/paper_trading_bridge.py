"""
Paper Trading Bridge for Alpha Arena

This module bridges Alpha Arena's Python competition system with the main Rust paper trading module.
It directly accesses the SQLite database used by Rust to ensure data persistence.
"""

import sqlite3
import uuid
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass
import os
import json

from alpha_arena.utils.logging import get_logger
from alpha_arena.types.models import (
    MarketData,
    ModelDecision,
    TradeAction,
    TradeResult,
    PolymarketDecision,
    PolymarketAction,
    PolymarketTradeResult,
    PolymarketInfo,
)

logger = get_logger("paper_trading_bridge")


@dataclass
class Portfolio:
    """Paper trading portfolio."""
    id: str
    name: str
    initial_balance: float
    balance: float
    currency: str
    leverage: float
    margin_mode: str
    fee_rate: float
    created_at: str


@dataclass
class Position:
    """Paper trading position."""
    id: str
    portfolio_id: str
    symbol: str
    side: str  # "long" | "short"
    quantity: float
    entry_price: float
    current_price: float
    unrealized_pnl: float
    realized_pnl: float
    leverage: float
    liquidation_price: Optional[float]
    opened_at: str


@dataclass
class Order:
    """Paper trading order."""
    id: str
    portfolio_id: str
    symbol: str
    side: str  # "buy" | "sell"
    order_type: str  # "market" | "limit" | "stop" | "stop_limit"
    quantity: float
    price: Optional[float]
    stop_price: Optional[float]
    filled_qty: float
    avg_price: Optional[float]
    status: str  # "pending" | "filled" | "partial" | "cancelled"
    reduce_only: bool
    created_at: str
    filled_at: Optional[str]


@dataclass
class Trade:
    """Paper trading trade (execution)."""
    id: str
    portfolio_id: str
    order_id: str
    symbol: str
    side: str
    price: float
    quantity: float
    fee: float
    pnl: float
    timestamp: str


@dataclass
class BridgePortfolioState:
    """Portfolio state with calculated values (bridge-specific format)."""
    model_name: str
    portfolio_value: float
    cash: float
    total_pnl: float
    unrealized_pnl: float
    trades_count: int
    positions: List[Dict[str, Any]]  # List format for easy serialization


def get_db_path() -> Path:
    """Get the path to the Fincept database."""
    # Try different possible locations for the database
    possible_paths = [
        # Windows AppData
        Path(os.environ.get("APPDATA", "")) / "fincept-terminal" / "fincept_terminal.db",
        Path(os.environ.get("LOCALAPPDATA", "")) / "fincept-terminal" / "fincept_terminal.db",
        # Development paths
        Path(__file__).parent.parent.parent.parent.parent.parent / "fincept_terminal.db",
        # Linux/Mac
        Path.home() / ".config" / "fincept-terminal" / "fincept_terminal.db",
        Path.home() / ".local" / "share" / "fincept-terminal" / "fincept_terminal.db",
    ]

    for path in possible_paths:
        if path.exists():
            logger.info(f"Found database at: {path}")
            return path

    # Default to Windows AppData
    default_path = Path(os.environ.get("APPDATA", "C:/")) / "fincept-terminal" / "fincept_terminal.db"
    logger.warning(f"Database not found, using default: {default_path}")
    return default_path


def get_connection() -> sqlite3.Connection:
    """Get SQLite connection with proper settings."""
    db_path = get_db_path()

    # Ensure directory exists
    db_path.parent.mkdir(parents=True, exist_ok=True)

    conn = sqlite3.connect(str(db_path), timeout=30.0)
    conn.row_factory = sqlite3.Row
    # Enable foreign keys and WAL mode for better concurrency
    conn.execute("PRAGMA foreign_keys = ON")
    conn.execute("PRAGMA journal_mode = WAL")
    return conn


class ConnectionManager:
    """Context manager for database connections."""

    def __init__(self):
        self.conn: Optional[sqlite3.Connection] = None

    def __enter__(self) -> sqlite3.Connection:
        self.conn = get_connection()
        return self.conn

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.conn:
            if exc_type is None:
                self.conn.commit()
            else:
                self.conn.rollback()
            self.conn.close()
        return False  # Don't suppress exceptions


class PaperTradingBridge:
    """
    Bridge to Rust paper trading module via SQLite.

    This replaces the in-memory PaperTradingEngine with database-backed operations.
    """

    def __init__(
        self,
        competition_id: str,
        model_name: str,
        initial_capital: float = 10000.0,
        fee_rate: float = 0.001,
        currency: str = "USD",
    ):
        self.competition_id = competition_id
        self.model_name = model_name
        self.initial_capital = initial_capital
        self.fee_rate = fee_rate
        self.currency = currency
        self.portfolio_id: Optional[str] = None

        # Initialize on creation
        self._ensure_tables()
        self._initialize_portfolio()

    def _ensure_tables(self):
        """Ensure paper trading tables exist (they should be created by Rust)."""
        conn = get_connection()
        try:
            # Check if pt_portfolios table exists
            cursor = conn.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='pt_portfolios'"
            )
            if not cursor.fetchone():
                logger.warning("Paper trading tables not found, creating them...")
                self._create_tables(conn)
            conn.commit()
        finally:
            conn.close()

    def _create_tables(self, conn: sqlite3.Connection):
        """Create paper trading tables if they don't exist."""
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS pt_portfolios (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                initial_balance REAL NOT NULL,
                balance REAL NOT NULL,
                currency TEXT DEFAULT 'USD',
                leverage REAL DEFAULT 1.0,
                margin_mode TEXT DEFAULT 'cross',
                fee_rate REAL DEFAULT 0.001,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS pt_orders (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                side TEXT NOT NULL,
                order_type TEXT NOT NULL,
                quantity REAL NOT NULL,
                price REAL,
                stop_price REAL,
                filled_qty REAL DEFAULT 0,
                avg_price REAL,
                status TEXT DEFAULT 'pending',
                reduce_only INTEGER DEFAULT 0,
                created_at TEXT NOT NULL,
                filled_at TEXT,
                FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id)
            );

            CREATE TABLE IF NOT EXISTS pt_positions (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                side TEXT NOT NULL,
                quantity REAL NOT NULL,
                entry_price REAL NOT NULL,
                current_price REAL NOT NULL,
                unrealized_pnl REAL DEFAULT 0,
                realized_pnl REAL DEFAULT 0,
                leverage REAL DEFAULT 1.0,
                liquidation_price REAL,
                opened_at TEXT NOT NULL,
                FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
                UNIQUE(portfolio_id, symbol, side)
            );

            CREATE TABLE IF NOT EXISTS pt_trades (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                order_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                side TEXT NOT NULL,
                price REAL NOT NULL,
                quantity REAL NOT NULL,
                fee REAL NOT NULL,
                pnl REAL DEFAULT 0,
                timestamp TEXT NOT NULL,
                FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
                FOREIGN KEY (order_id) REFERENCES pt_orders(id)
            );

            CREATE INDEX IF NOT EXISTS idx_orders_portfolio ON pt_orders(portfolio_id);
            CREATE INDEX IF NOT EXISTS idx_positions_portfolio ON pt_positions(portfolio_id);
            CREATE INDEX IF NOT EXISTS idx_trades_portfolio ON pt_trades(portfolio_id);

            -- Polymarket positions table
            CREATE TABLE IF NOT EXISTS pt_polymarket_positions (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                market_id TEXT NOT NULL,
                market_question TEXT NOT NULL,
                outcome TEXT NOT NULL,
                shares REAL NOT NULL,
                avg_price REAL NOT NULL,
                current_price REAL NOT NULL,
                unrealized_pnl REAL DEFAULT 0,
                realized_pnl REAL DEFAULT 0,
                opened_at TEXT NOT NULL,
                resolved INTEGER DEFAULT 0,
                resolution_outcome TEXT,
                FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
                UNIQUE(portfolio_id, market_id, outcome)
            );

            -- Polymarket trades table
            CREATE TABLE IF NOT EXISTS pt_polymarket_trades (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                market_id TEXT NOT NULL,
                market_question TEXT NOT NULL,
                action TEXT NOT NULL,
                outcome TEXT NOT NULL,
                shares REAL NOT NULL,
                price REAL NOT NULL,
                cost REAL NOT NULL,
                fee REAL NOT NULL,
                pnl REAL DEFAULT 0,
                timestamp TEXT NOT NULL,
                FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id)
            );

            CREATE INDEX IF NOT EXISTS idx_pm_positions_portfolio ON pt_polymarket_positions(portfolio_id);
            CREATE INDEX IF NOT EXISTS idx_pm_trades_portfolio ON pt_polymarket_trades(portfolio_id);
        """)

    def _initialize_portfolio(self):
        """Initialize or get existing portfolio for this competition/model."""
        conn = get_connection()
        try:
            portfolio_name = f"alpha_arena_{self.competition_id}_{self.model_name}"

            # Check for existing portfolio
            cursor = conn.execute(
                "SELECT id FROM pt_portfolios WHERE name = ?",
                (portfolio_name,)
            )
            row = cursor.fetchone()

            if row:
                self.portfolio_id = row["id"]
                logger.info(f"Using existing portfolio: {self.portfolio_id}")
            else:
                # Create new portfolio
                self.portfolio_id = str(uuid.uuid4())
                now = datetime.utcnow().isoformat() + "Z"

                conn.execute(
                    """INSERT INTO pt_portfolios
                       (id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, created_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (
                        self.portfolio_id,
                        portfolio_name,
                        self.initial_capital,
                        self.initial_capital,
                        self.currency,
                        1.0,  # leverage
                        "cross",  # margin_mode
                        self.fee_rate,
                        now,
                    )
                )
                conn.commit()
                logger.info(f"Created new portfolio: {self.portfolio_id}")

                # Also record in alpha_arena_model_portfolios for linking
                self._link_model_portfolio(conn)
        finally:
            conn.close()

    def _link_model_portfolio(self, conn: sqlite3.Connection):
        """Link this portfolio to the Alpha Arena competition."""
        try:
            # Ensure the table exists without foreign key constraints
            # (competition data lives in a separate alpha_arena.db, not in this main db)
            conn.execute("""
                CREATE TABLE IF NOT EXISTS alpha_arena_model_portfolios (
                    id TEXT PRIMARY KEY,
                    competition_id TEXT NOT NULL,
                    model_name TEXT NOT NULL,
                    portfolio_id TEXT NOT NULL,
                    initial_capital REAL NOT NULL,
                    created_at TEXT NOT NULL
                )
            """)
            conn.commit()

            link_id = str(uuid.uuid4())
            now = datetime.utcnow().isoformat() + "Z"

            conn.execute(
                """INSERT OR REPLACE INTO alpha_arena_model_portfolios
                   (id, competition_id, model_name, portfolio_id, initial_capital, created_at)
                   VALUES (?, ?, ?, ?, ?, ?)""",
                (
                    link_id,
                    self.competition_id,
                    self.model_name,
                    self.portfolio_id,
                    self.initial_capital,
                    now,
                )
            )
            conn.commit()
            logger.info(f"Linked model {self.model_name} to portfolio {self.portfolio_id}")
        except sqlite3.Error as e:
            # If foreign key constraint fails due to old table schema, try to recreate
            if "FOREIGN KEY" in str(e):
                try:
                    conn.execute("DROP TABLE IF EXISTS alpha_arena_model_portfolios")
                    conn.execute("""
                        CREATE TABLE alpha_arena_model_portfolios (
                            id TEXT PRIMARY KEY,
                            competition_id TEXT NOT NULL,
                            model_name TEXT NOT NULL,
                            portfolio_id TEXT NOT NULL,
                            initial_capital REAL NOT NULL,
                            created_at TEXT NOT NULL
                        )
                    """)
                    link_id = str(uuid.uuid4())
                    now = datetime.utcnow().isoformat() + "Z"
                    conn.execute(
                        """INSERT INTO alpha_arena_model_portfolios
                           (id, competition_id, model_name, portfolio_id, initial_capital, created_at)
                           VALUES (?, ?, ?, ?, ?, ?)""",
                        (link_id, self.competition_id, self.model_name,
                         self.portfolio_id, self.initial_capital, now)
                    )
                    conn.commit()
                    logger.info(f"Recreated table and linked model {self.model_name} to portfolio {self.portfolio_id}")
                except sqlite3.Error as e2:
                    logger.warning(f"Could not recreate model portfolio link: {e2}")
            else:
                logger.warning(f"Could not link model portfolio: {e}")

    def get_portfolio(self) -> Optional[Portfolio]:
        """Get current portfolio state."""
        if not self.portfolio_id:
            return None

        conn = get_connection()
        try:
            cursor = conn.execute(
                """SELECT id, name, initial_balance, balance, currency, leverage,
                          margin_mode, fee_rate, created_at
                   FROM pt_portfolios WHERE id = ?""",
                (self.portfolio_id,)
            )
            row = cursor.fetchone()

            if row:
                return Portfolio(
                    id=row["id"],
                    name=row["name"],
                    initial_balance=row["initial_balance"],
                    balance=row["balance"],
                    currency=row["currency"],
                    leverage=row["leverage"],
                    margin_mode=row["margin_mode"],
                    fee_rate=row["fee_rate"],
                    created_at=row["created_at"],
                )
            return None
        finally:
            conn.close()

    def get_positions(self) -> List[Position]:
        """Get all open positions."""
        if not self.portfolio_id:
            return []

        conn = get_connection()
        try:
            cursor = conn.execute(
                """SELECT id, portfolio_id, symbol, side, quantity, entry_price,
                          current_price, unrealized_pnl, realized_pnl, leverage,
                          liquidation_price, opened_at
                   FROM pt_positions WHERE portfolio_id = ?""",
                (self.portfolio_id,)
            )

            return [
                Position(
                    id=row["id"],
                    portfolio_id=row["portfolio_id"],
                    symbol=row["symbol"],
                    side=row["side"],
                    quantity=row["quantity"],
                    entry_price=row["entry_price"],
                    current_price=row["current_price"],
                    unrealized_pnl=row["unrealized_pnl"],
                    realized_pnl=row["realized_pnl"],
                    leverage=row["leverage"],
                    liquidation_price=row["liquidation_price"],
                    opened_at=row["opened_at"],
                )
                for row in cursor.fetchall()
            ]
        finally:
            conn.close()

    def get_trades(self, limit: int = 100) -> List[Trade]:
        """Get recent trades."""
        if not self.portfolio_id:
            return []

        conn = get_connection()
        try:
            cursor = conn.execute(
                """SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp
                   FROM pt_trades
                   WHERE portfolio_id = ?
                   ORDER BY timestamp DESC
                   LIMIT ?""",
                (self.portfolio_id, limit)
            )

            return [
                Trade(
                    id=row["id"],
                    portfolio_id=row["portfolio_id"],
                    order_id=row["order_id"],
                    symbol=row["symbol"],
                    side=row["side"],
                    price=row["price"],
                    quantity=row["quantity"],
                    fee=row["fee"],
                    pnl=row["pnl"],
                    timestamp=row["timestamp"],
                )
                for row in cursor.fetchall()
            ]
        finally:
            conn.close()

    def place_order(
        self,
        symbol: str,
        side: str,
        order_type: str,
        quantity: float,
        price: Optional[float] = None,
        reduce_only: bool = False,
    ) -> Optional[Order]:
        """Place a new order."""
        if not self.portfolio_id:
            logger.error("No portfolio initialized")
            return None

        conn = get_connection()
        try:
            # Validate
            if quantity <= 0:
                logger.error("Invalid quantity")
                return None

            if order_type == "limit" and price is None:
                logger.error("Limit order requires price")
                return None

            # Check balance for new positions
            if not reduce_only:
                portfolio = self.get_portfolio()
                if not portfolio:
                    return None

                required_margin = quantity * (price or 0) / portfolio.leverage
                if required_margin > portfolio.balance:
                    logger.error(f"Insufficient margin: need {required_margin}, have {portfolio.balance}")
                    return None

            order_id = str(uuid.uuid4())
            now = datetime.utcnow().isoformat() + "Z"

            conn.execute(
                """INSERT INTO pt_orders
                   (id, portfolio_id, symbol, side, order_type, quantity, price, filled_qty, status, reduce_only, created_at)
                   VALUES (?, ?, ?, ?, ?, ?, ?, 0, 'pending', ?, ?)""",
                (
                    order_id,
                    self.portfolio_id,
                    symbol,
                    side.lower(),
                    order_type,
                    quantity,
                    price,
                    1 if reduce_only else 0,
                    now,
                )
            )
            conn.commit()

            return Order(
                id=order_id,
                portfolio_id=self.portfolio_id,
                symbol=symbol,
                side=side.lower(),
                order_type=order_type,
                quantity=quantity,
                price=price,
                stop_price=None,
                filled_qty=0,
                avg_price=None,
                status="pending",
                reduce_only=reduce_only,
                created_at=now,
                filled_at=None,
            )
        finally:
            conn.close()

    def fill_order(self, order_id: str, fill_price: float, fill_qty: Optional[float] = None) -> Optional[Trade]:
        """Fill an order at the given price."""
        conn = get_connection()
        try:
            # Get order
            cursor = conn.execute(
                """SELECT id, portfolio_id, symbol, side, order_type, quantity, price,
                          filled_qty, avg_price, status, reduce_only
                   FROM pt_orders WHERE id = ?""",
                (order_id,)
            )
            order_row = cursor.fetchone()

            if not order_row:
                logger.error(f"Order not found: {order_id}")
                return None

            if order_row["status"] not in ("pending", "partial"):
                logger.error(f"Order not fillable, status: {order_row['status']}")
                return None

            # Get portfolio
            portfolio = self.get_portfolio()
            if not portfolio:
                return None

            # Calculate fill quantity
            qty = fill_qty or (order_row["quantity"] - order_row["filled_qty"])
            fee = qty * fill_price * portfolio.fee_rate
            now = datetime.utcnow().isoformat() + "Z"

            # Determine position side
            position_side = "long" if order_row["side"] == "buy" else "short"
            opposite_side = "short" if order_row["side"] == "buy" else "long"
            pnl = 0.0

            # Check for closing opposite position
            cursor = conn.execute(
                """SELECT id, quantity, entry_price, realized_pnl
                   FROM pt_positions
                   WHERE portfolio_id = ? AND symbol = ? AND side = ?""",
                (self.portfolio_id, order_row["symbol"], opposite_side)
            )
            opposite_pos = cursor.fetchone()

            if opposite_pos:
                # Closing/reducing existing position
                close_qty = min(qty, opposite_pos["quantity"])

                if opposite_side == "long":
                    pnl = (fill_price - opposite_pos["entry_price"]) * close_qty
                else:
                    pnl = (opposite_pos["entry_price"] - fill_price) * close_qty

                if close_qty >= opposite_pos["quantity"]:
                    # Close entire position
                    conn.execute("DELETE FROM pt_positions WHERE id = ?", (opposite_pos["id"],))
                else:
                    # Reduce position
                    new_qty = opposite_pos["quantity"] - close_qty
                    new_realized = opposite_pos["realized_pnl"] + pnl
                    conn.execute(
                        "UPDATE pt_positions SET quantity = ?, realized_pnl = ? WHERE id = ?",
                        (new_qty, new_realized, opposite_pos["id"])
                    )

                # Open remaining as new position
                remaining_qty = qty - close_qty
                if remaining_qty > 0:
                    self._create_or_add_position(
                        conn, order_row["symbol"], position_side, remaining_qty, fill_price
                    )
            else:
                # Check for same-side position to add to
                cursor = conn.execute(
                    """SELECT id, quantity, entry_price
                       FROM pt_positions
                       WHERE portfolio_id = ? AND symbol = ? AND side = ?""",
                    (self.portfolio_id, order_row["symbol"], position_side)
                )
                same_pos = cursor.fetchone()

                if same_pos:
                    # Average into existing position
                    new_qty = same_pos["quantity"] + qty
                    new_entry = (same_pos["entry_price"] * same_pos["quantity"] + fill_price * qty) / new_qty
                    conn.execute(
                        "UPDATE pt_positions SET quantity = ?, entry_price = ?, current_price = ? WHERE id = ?",
                        (new_qty, new_entry, fill_price, same_pos["id"])
                    )
                else:
                    # New position
                    self._create_or_add_position(
                        conn, order_row["symbol"], position_side, qty, fill_price
                    )

            # Update balance
            balance_change = pnl - fee
            conn.execute(
                "UPDATE pt_portfolios SET balance = balance + ? WHERE id = ?",
                (balance_change, self.portfolio_id)
            )

            # Update order
            new_filled = order_row["filled_qty"] + qty
            new_status = "filled" if new_filled >= order_row["quantity"] else "partial"
            old_avg = order_row["avg_price"] or 0
            new_avg = (old_avg * order_row["filled_qty"] + fill_price * qty) / new_filled if new_filled > 0 else fill_price

            conn.execute(
                """UPDATE pt_orders
                   SET filled_qty = ?, avg_price = ?, status = ?, filled_at = ?
                   WHERE id = ?""",
                (new_filled, new_avg, new_status, now, order_id)
            )

            # Create trade record
            trade_id = str(uuid.uuid4())
            conn.execute(
                """INSERT INTO pt_trades
                   (id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                (
                    trade_id,
                    self.portfolio_id,
                    order_id,
                    order_row["symbol"],
                    order_row["side"],
                    fill_price,
                    qty,
                    fee,
                    pnl,
                    now,
                )
            )

            conn.commit()

            return Trade(
                id=trade_id,
                portfolio_id=self.portfolio_id,
                order_id=order_id,
                symbol=order_row["symbol"],
                side=order_row["side"],
                price=fill_price,
                quantity=qty,
                fee=fee,
                pnl=pnl,
                timestamp=now,
            )
        finally:
            conn.close()

    def _create_or_add_position(
        self,
        conn: sqlite3.Connection,
        symbol: str,
        side: str,
        quantity: float,
        price: float,
    ):
        """Create a new position."""
        pos_id = str(uuid.uuid4())
        now = datetime.utcnow().isoformat() + "Z"

        conn.execute(
            """INSERT INTO pt_positions
               (id, portfolio_id, symbol, side, quantity, entry_price, current_price,
                unrealized_pnl, realized_pnl, leverage, opened_at)
               VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0, 1.0, ?)""",
            (
                pos_id,
                self.portfolio_id,
                symbol,
                side,
                quantity,
                price,
                price,
                now,
            )
        )

    def update_position_price(self, symbol: str, price: float):
        """Update position with current market price."""
        if not self.portfolio_id:
            return

        conn = get_connection()
        try:
            conn.execute(
                """UPDATE pt_positions
                   SET current_price = ?,
                       unrealized_pnl = CASE
                           WHEN side = 'long' THEN (? - entry_price) * quantity
                           ELSE (entry_price - ?) * quantity
                       END
                   WHERE portfolio_id = ? AND symbol = ?""",
                (price, price, price, self.portfolio_id, symbol)
            )
            conn.commit()
        finally:
            conn.close()

    def get_portfolio_state(self, prices: Dict[str, float]) -> BridgePortfolioState:
        """Get current portfolio state with calculated values."""
        portfolio = self.get_portfolio()
        positions = self.get_positions()
        trades = self.get_trades()

        if not portfolio:
            return BridgePortfolioState(
                model_name=self.model_name,
                portfolio_value=self.initial_capital,
                cash=self.initial_capital,
                total_pnl=0.0,
                unrealized_pnl=0.0,
                trades_count=0,
                positions=[],
            )

        # Update position prices and calculate unrealized PnL
        total_unrealized = 0.0
        position_dicts = []

        for pos in positions:
            current_price = prices.get(pos.symbol, pos.current_price)

            # Update price in database
            self.update_position_price(pos.symbol, current_price)

            # Calculate unrealized PnL
            if pos.side == "long":
                unrealized = (current_price - pos.entry_price) * pos.quantity
            else:
                unrealized = (pos.entry_price - current_price) * pos.quantity

            total_unrealized += unrealized

            position_dicts.append({
                "symbol": pos.symbol,
                "side": pos.side,
                "quantity": pos.quantity,
                "entry_price": pos.entry_price,
                "current_price": current_price,
                "unrealized_pnl": unrealized,
            })

        # Calculate total realized PnL from trades
        total_realized = sum(t.pnl for t in trades)

        # Portfolio value = cash + positions value
        positions_value = sum(
            p["quantity"] * p["current_price"] for p in position_dicts
        )
        portfolio_value = portfolio.balance + positions_value

        return BridgePortfolioState(
            model_name=self.model_name,
            portfolio_value=portfolio_value,
            cash=portfolio.balance,
            total_pnl=total_realized + total_unrealized,
            unrealized_pnl=total_unrealized,
            trades_count=len(trades),
            positions=position_dicts,
        )

    def reset(self):
        """Reset portfolio to initial state."""
        if not self.portfolio_id:
            return

        conn = get_connection()
        try:
            # Delete all positions, orders, trades
            conn.execute("DELETE FROM pt_trades WHERE portfolio_id = ?", (self.portfolio_id,))
            conn.execute("DELETE FROM pt_orders WHERE portfolio_id = ?", (self.portfolio_id,))
            conn.execute("DELETE FROM pt_positions WHERE portfolio_id = ?", (self.portfolio_id,))

            # Reset balance
            conn.execute(
                "UPDATE pt_portfolios SET balance = initial_balance WHERE id = ?",
                (self.portfolio_id,)
            )
            conn.commit()
            logger.info(f"Portfolio {self.portfolio_id} reset to initial state")
        finally:
            conn.close()

    def execute_decision(
        self,
        decision: ModelDecision,
        market_data: MarketData,
    ) -> TradeResult:
        """
        Execute a trading decision using the database-backed paper trading.

        This method provides the same interface as the old PaperTradingEngine.

        Args:
            decision: The trading decision to execute
            market_data: Current market data

        Returns:
            TradeResult with execution details
        """
        symbol = decision.symbol
        action = decision.action
        requested_quantity = decision.quantity or 0

        # Validate action
        if isinstance(action, str):
            try:
                action = TradeAction(action.lower())
            except ValueError:
                return TradeResult(
                    status="rejected",
                    action=TradeAction.HOLD,
                    symbol=symbol,
                    quantity=0,
                    price=market_data.price,
                    reason=f"Invalid action: {action}",
                )

        # Handle HOLD
        if action == TradeAction.HOLD:
            return TradeResult(
                status="executed",
                action=TradeAction.HOLD,
                symbol=symbol,
                quantity=0,
                price=market_data.price,
                reason="Hold - no trade",
            )

        # Determine execution price (use ask for buys, bid for sells)
        if action == TradeAction.BUY:
            execution_price = market_data.ask
            side = "buy"
        elif action == TradeAction.SELL:
            execution_price = market_data.bid
            side = "sell"
        elif action == TradeAction.SHORT:
            execution_price = market_data.bid
            side = "sell"  # Short = sell to open
        else:
            return TradeResult(
                status="rejected",
                action=action,
                symbol=symbol,
                quantity=0,
                price=market_data.price,
                reason=f"Unknown action: {action}",
            )

        # Get current portfolio state
        portfolio = self.get_portfolio()
        if not portfolio:
            return TradeResult(
                status="rejected",
                action=action,
                symbol=symbol,
                quantity=0,
                price=execution_price,
                reason="Portfolio not initialized",
            )

        # Adjust quantity based on available funds
        if action == TradeAction.BUY:
            max_cost = portfolio.balance * 0.9  # 90% max position
            max_quantity = max_cost / (execution_price * (1 + self.fee_rate))
            quantity = min(requested_quantity, max_quantity)
        elif action in (TradeAction.SELL, TradeAction.SHORT):
            # Check existing position
            positions = self.get_positions()
            long_position = next(
                (p for p in positions if p.symbol == symbol and p.side == "long"),
                None
            )
            if long_position:
                quantity = min(requested_quantity, long_position.quantity)
            else:
                # For short, use margin
                max_margin = portfolio.balance * 0.9 * 0.3  # 30% margin requirement
                max_quantity = max_margin / (execution_price * 0.3)
                quantity = min(requested_quantity, max_quantity)
        else:
            quantity = requested_quantity

        if quantity <= 0:
            return TradeResult(
                status="rejected",
                action=action,
                symbol=symbol,
                quantity=0,
                price=execution_price,
                reason="Insufficient funds or no position",
            )

        # Place order
        order = self.place_order(
            symbol=symbol,
            side=side,
            order_type="market",
            quantity=quantity,
            price=execution_price,
        )

        if not order:
            return TradeResult(
                status="rejected",
                action=action,
                symbol=symbol,
                quantity=0,
                price=execution_price,
                reason="Failed to place order",
            )

        # Fill order immediately (market order)
        trade = self.fill_order(order.id, execution_price)

        if not trade:
            return TradeResult(
                status="rejected",
                action=action,
                symbol=symbol,
                quantity=0,
                price=execution_price,
                reason="Failed to fill order",
            )

        # Return success result
        logger.info(
            f"{self.model_name}: {action.value.upper()} {quantity} {symbol} @ ${execution_price:.2f} "
            f"(fee: ${trade.fee:.2f}, pnl: ${trade.pnl:+.2f})"
        )

        return TradeResult(
            status="executed",
            action=action,
            symbol=symbol,
            quantity=quantity,
            price=execution_price,
            cost=quantity * execution_price + trade.fee if action == TradeAction.BUY else None,
            proceeds=quantity * execution_price - trade.fee if action == TradeAction.SELL else None,
            pnl=trade.pnl,
        )


    def _ensure_polymarket_tables(self):
        """Ensure Polymarket-specific tables exist."""
        conn = get_connection()
        try:
            cursor = conn.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='pt_polymarket_positions'"
            )
            if not cursor.fetchone():
                conn.executescript("""
                    CREATE TABLE IF NOT EXISTS pt_polymarket_positions (
                        id TEXT PRIMARY KEY,
                        portfolio_id TEXT NOT NULL,
                        market_id TEXT NOT NULL,
                        market_question TEXT NOT NULL,
                        outcome TEXT NOT NULL,
                        shares REAL NOT NULL,
                        avg_price REAL NOT NULL,
                        current_price REAL NOT NULL,
                        unrealized_pnl REAL DEFAULT 0,
                        realized_pnl REAL DEFAULT 0,
                        opened_at TEXT NOT NULL,
                        resolved INTEGER DEFAULT 0,
                        resolution_outcome TEXT,
                        FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
                        UNIQUE(portfolio_id, market_id, outcome)
                    );

                    CREATE TABLE IF NOT EXISTS pt_polymarket_trades (
                        id TEXT PRIMARY KEY,
                        portfolio_id TEXT NOT NULL,
                        market_id TEXT NOT NULL,
                        market_question TEXT NOT NULL,
                        action TEXT NOT NULL,
                        outcome TEXT NOT NULL,
                        shares REAL NOT NULL,
                        price REAL NOT NULL,
                        cost REAL NOT NULL,
                        fee REAL NOT NULL,
                        pnl REAL DEFAULT 0,
                        timestamp TEXT NOT NULL,
                        FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id)
                    );

                    CREATE INDEX IF NOT EXISTS idx_pm_positions_portfolio ON pt_polymarket_positions(portfolio_id);
                    CREATE INDEX IF NOT EXISTS idx_pm_trades_portfolio ON pt_polymarket_trades(portfolio_id);
                """)
            conn.commit()
        finally:
            conn.close()

    def get_polymarket_positions(self) -> List[Dict[str, Any]]:
        """Get all open Polymarket positions."""
        if not self.portfolio_id:
            return []

        self._ensure_polymarket_tables()
        conn = get_connection()
        try:
            cursor = conn.execute(
                """SELECT id, portfolio_id, market_id, market_question, outcome,
                          shares, avg_price, current_price, unrealized_pnl,
                          realized_pnl, opened_at, resolved, resolution_outcome
                   FROM pt_polymarket_positions
                   WHERE portfolio_id = ? AND resolved = 0""",
                (self.portfolio_id,)
            )
            return [dict(row) for row in cursor.fetchall()]
        finally:
            conn.close()

    def get_polymarket_trades(self, limit: int = 100) -> List[Dict[str, Any]]:
        """Get recent Polymarket trades."""
        if not self.portfolio_id:
            return []

        self._ensure_polymarket_tables()
        conn = get_connection()
        try:
            cursor = conn.execute(
                """SELECT id, portfolio_id, market_id, market_question, action,
                          outcome, shares, price, cost, fee, pnl, timestamp
                   FROM pt_polymarket_trades
                   WHERE portfolio_id = ?
                   ORDER BY timestamp DESC
                   LIMIT ?""",
                (self.portfolio_id, limit)
            )
            return [dict(row) for row in cursor.fetchall()]
        finally:
            conn.close()

    def execute_polymarket_decision(
        self,
        decision: PolymarketDecision,
        market: PolymarketInfo,
        max_exposure_pct: float = 0.25,
    ) -> PolymarketTradeResult:
        """
        Execute a Polymarket trading decision.

        Shares are priced between $0 and $1. Buying YES at $0.60 means
        paying $0.60 per share; if the event resolves YES, each share pays $1.

        Args:
            decision: The Polymarket decision to execute
            market: Current market data
            max_exposure_pct: Max portfolio % per market (risk guardrail)

        Returns:
            PolymarketTradeResult with execution details
        """
        self._ensure_polymarket_tables()

        action = decision.action
        if isinstance(action, str):
            try:
                action = PolymarketAction(action.lower())
            except ValueError:
                return PolymarketTradeResult(
                    status="rejected",
                    market_id=decision.market_id,
                    action=PolymarketAction.HOLD,
                    outcome="",
                    shares=0,
                    price=0,
                    cost=0,
                    reason=f"Invalid action: {action}",
                )

        # HOLD
        if action == PolymarketAction.HOLD:
            return PolymarketTradeResult(
                status="executed",
                market_id=decision.market_id,
                action=PolymarketAction.HOLD,
                outcome="",
                shares=0,
                price=0,
                cost=0,
                reason="Hold - no trade",
            )

        portfolio = self.get_portfolio()
        if not portfolio:
            return PolymarketTradeResult(
                status="rejected",
                market_id=decision.market_id,
                action=action,
                outcome="",
                shares=0,
                price=0,
                cost=0,
                reason="Portfolio not initialized",
            )

        # Determine outcome and price
        is_buy = action in (PolymarketAction.BUY_YES, PolymarketAction.BUY_NO)
        if action in (PolymarketAction.BUY_YES, PolymarketAction.SELL_YES):
            outcome = "YES"
            price = market.outcome_prices[0] if market.outcome_prices else 0.5
        else:
            outcome = "NO"
            price = market.outcome_prices[1] if len(market.outcome_prices) > 1 else 0.5

        amount_usd = decision.amount_usd or 0
        if amount_usd <= 0 and is_buy:
            return PolymarketTradeResult(
                status="rejected",
                market_id=decision.market_id,
                action=action,
                outcome=outcome,
                shares=0,
                price=price,
                cost=0,
                reason="Amount must be > 0 for buy",
            )

        # Risk guardrail: max exposure per market
        max_exposure = portfolio.balance * max_exposure_pct
        if is_buy and amount_usd > max_exposure:
            amount_usd = max_exposure
            logger.warning(
                f"Capped {decision.model_name} exposure on {decision.market_id} "
                f"to ${max_exposure:.2f} ({max_exposure_pct:.0%} of portfolio)"
            )

        # Liquidity check
        if market.liquidity > 0 and amount_usd > market.liquidity * 0.1:
            amount_usd = min(amount_usd, market.liquidity * 0.1)
            logger.warning(f"Capped trade to 10% of market liquidity: ${amount_usd:.2f}")

        now = datetime.utcnow().isoformat() + "Z"
        fee = amount_usd * self.fee_rate
        conn = get_connection()

        try:
            if is_buy:
                # Calculate shares: amount / price
                shares = amount_usd / price if price > 0 else 0
                total_cost = amount_usd + fee

                if total_cost > portfolio.balance:
                    return PolymarketTradeResult(
                        status="rejected",
                        market_id=decision.market_id,
                        action=action,
                        outcome=outcome,
                        shares=0,
                        price=price,
                        cost=0,
                        reason=f"Insufficient funds: need ${total_cost:.2f}, have ${portfolio.balance:.2f}",
                    )

                # Update or create position
                cursor = conn.execute(
                    """SELECT id, shares, avg_price FROM pt_polymarket_positions
                       WHERE portfolio_id = ? AND market_id = ? AND outcome = ? AND resolved = 0""",
                    (self.portfolio_id, decision.market_id, outcome)
                )
                existing = cursor.fetchone()

                if existing:
                    new_shares = existing["shares"] + shares
                    new_avg = (existing["avg_price"] * existing["shares"] + price * shares) / new_shares
                    conn.execute(
                        """UPDATE pt_polymarket_positions
                           SET shares = ?, avg_price = ?, current_price = ?
                           WHERE id = ?""",
                        (new_shares, new_avg, price, existing["id"])
                    )
                else:
                    pos_id = str(uuid.uuid4())
                    conn.execute(
                        """INSERT INTO pt_polymarket_positions
                           (id, portfolio_id, market_id, market_question, outcome, shares,
                            avg_price, current_price, unrealized_pnl, realized_pnl, opened_at, resolved)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 0, ?, 0)""",
                        (pos_id, self.portfolio_id, decision.market_id,
                         decision.market_question, outcome, shares, price, price, now)
                    )

                # Deduct from balance
                conn.execute(
                    "UPDATE pt_portfolios SET balance = balance - ? WHERE id = ?",
                    (total_cost, self.portfolio_id)
                )

                # Record trade
                trade_id = str(uuid.uuid4())
                conn.execute(
                    """INSERT INTO pt_polymarket_trades
                       (id, portfolio_id, market_id, market_question, action, outcome,
                        shares, price, cost, fee, pnl, timestamp)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)""",
                    (trade_id, self.portfolio_id, decision.market_id,
                     decision.market_question, action.value, outcome,
                     shares, price, total_cost, fee, now)
                )

                conn.commit()
                logger.info(
                    f"{self.model_name}: {action.value.upper()} {shares:.2f} {outcome} shares "
                    f"@ ${price:.4f} for ${total_cost:.2f} (market: {decision.market_question[:50]})"
                )

                return PolymarketTradeResult(
                    status="executed",
                    market_id=decision.market_id,
                    action=action,
                    outcome=outcome,
                    shares=shares,
                    price=price,
                    cost=total_cost,
                    pnl=0,
                )

            else:
                # SELL - close/reduce position
                cursor = conn.execute(
                    """SELECT id, shares, avg_price FROM pt_polymarket_positions
                       WHERE portfolio_id = ? AND market_id = ? AND outcome = ? AND resolved = 0""",
                    (self.portfolio_id, decision.market_id, outcome)
                )
                existing = cursor.fetchone()

                if not existing or existing["shares"] <= 0:
                    return PolymarketTradeResult(
                        status="rejected",
                        market_id=decision.market_id,
                        action=action,
                        outcome=outcome,
                        shares=0,
                        price=price,
                        cost=0,
                        reason=f"No {outcome} position to sell",
                    )

                # Sell shares based on amount_usd or all
                if amount_usd > 0:
                    sell_shares = min(amount_usd / price, existing["shares"]) if price > 0 else existing["shares"]
                else:
                    sell_shares = existing["shares"]

                proceeds = sell_shares * price
                pnl = (price - existing["avg_price"]) * sell_shares
                net_proceeds = proceeds - fee

                # Update position
                remaining = existing["shares"] - sell_shares
                if remaining <= 0.0001:
                    conn.execute("DELETE FROM pt_polymarket_positions WHERE id = ?", (existing["id"],))
                else:
                    conn.execute(
                        """UPDATE pt_polymarket_positions
                           SET shares = ?, current_price = ?, realized_pnl = realized_pnl + ?
                           WHERE id = ?""",
                        (remaining, price, pnl, existing["id"])
                    )

                # Add proceeds to balance
                conn.execute(
                    "UPDATE pt_portfolios SET balance = balance + ? WHERE id = ?",
                    (net_proceeds, self.portfolio_id)
                )

                # Record trade
                trade_id = str(uuid.uuid4())
                conn.execute(
                    """INSERT INTO pt_polymarket_trades
                       (id, portfolio_id, market_id, market_question, action, outcome,
                        shares, price, cost, fee, pnl, timestamp)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (trade_id, self.portfolio_id, decision.market_id,
                     decision.market_question, action.value, outcome,
                     sell_shares, price, net_proceeds, fee, pnl, now)
                )

                conn.commit()
                logger.info(
                    f"{self.model_name}: {action.value.upper()} {sell_shares:.2f} {outcome} shares "
                    f"@ ${price:.4f} P&L: ${pnl:+.2f}"
                )

                return PolymarketTradeResult(
                    status="executed",
                    market_id=decision.market_id,
                    action=action,
                    outcome=outcome,
                    shares=sell_shares,
                    price=price,
                    cost=net_proceeds,
                    pnl=pnl,
                )

        except Exception as e:
            logger.error(f"Polymarket execution error: {e}")
            conn.rollback()
            return PolymarketTradeResult(
                status="rejected",
                market_id=decision.market_id,
                action=action,
                outcome=outcome,
                shares=0,
                price=price,
                cost=0,
                reason=f"Execution error: {str(e)}",
            )
        finally:
            conn.close()

    def resolve_market(self, market_id: str, winning_outcome: str) -> float:
        """
        Resolve a Polymarket market. Winning shares pay $1 each, losing shares pay $0.

        Args:
            market_id: The market ID to resolve
            winning_outcome: "YES" or "NO"

        Returns:
            Total P&L from resolution
        """
        self._ensure_polymarket_tables()
        conn = get_connection()
        total_pnl = 0.0

        try:
            cursor = conn.execute(
                """SELECT id, outcome, shares, avg_price
                   FROM pt_polymarket_positions
                   WHERE portfolio_id = ? AND market_id = ? AND resolved = 0""",
                (self.portfolio_id, market_id)
            )
            positions = cursor.fetchall()
            now = datetime.utcnow().isoformat() + "Z"

            for pos in positions:
                is_winner = pos["outcome"] == winning_outcome
                settlement_price = 1.0 if is_winner else 0.0
                pnl = (settlement_price - pos["avg_price"]) * pos["shares"]
                total_pnl += pnl

                # Credit winning positions
                if is_winner:
                    proceeds = pos["shares"] * 1.0  # $1 per share
                    conn.execute(
                        "UPDATE pt_portfolios SET balance = balance + ? WHERE id = ?",
                        (proceeds, self.portfolio_id)
                    )

                # Mark as resolved
                conn.execute(
                    """UPDATE pt_polymarket_positions
                       SET resolved = 1, resolution_outcome = ?,
                           current_price = ?, realized_pnl = realized_pnl + ?
                       WHERE id = ?""",
                    (winning_outcome, settlement_price, pnl, pos["id"])
                )

                # Record resolution trade
                trade_id = str(uuid.uuid4())
                conn.execute(
                    """INSERT INTO pt_polymarket_trades
                       (id, portfolio_id, market_id, market_question, action, outcome,
                        shares, price, cost, fee, pnl, timestamp)
                       VALUES (?, ?, ?, 'Resolution', 'resolution', ?, ?, ?, 0, 0, ?, ?)""",
                    (trade_id, self.portfolio_id, market_id,
                     pos["outcome"], pos["shares"], settlement_price, pnl, now)
                )

            conn.commit()
            logger.info(f"Resolved market {market_id}: winner={winning_outcome}, P&L=${total_pnl:+.2f}")

        except Exception as e:
            logger.error(f"Market resolution error: {e}")
            conn.rollback()
        finally:
            conn.close()

        return total_pnl

    def update_polymarket_prices(self, price_updates: Dict[str, Dict[str, float]]):
        """
        Update Polymarket position prices.

        Args:
            price_updates: {market_id: {"YES": price, "NO": price}}
        """
        self._ensure_polymarket_tables()
        if not self.portfolio_id:
            return

        conn = get_connection()
        try:
            for market_id, prices in price_updates.items():
                for outcome, price in prices.items():
                    conn.execute(
                        """UPDATE pt_polymarket_positions
                           SET current_price = ?,
                               unrealized_pnl = (? - avg_price) * shares
                           WHERE portfolio_id = ? AND market_id = ? AND outcome = ? AND resolved = 0""",
                        (price, price, self.portfolio_id, market_id, outcome)
                    )
            conn.commit()
        finally:
            conn.close()

    def get_polymarket_portfolio_state(self) -> Dict[str, Any]:
        """Get portfolio state including Polymarket positions."""
        portfolio = self.get_portfolio()
        pm_positions = self.get_polymarket_positions()
        pm_trades = self.get_polymarket_trades()

        if not portfolio:
            return {
                "model_name": self.model_name,
                "portfolio_value": self.initial_capital,
                "cash": self.initial_capital,
                "total_pnl": 0.0,
                "unrealized_pnl": 0.0,
                "trades_count": 0,
                "positions": [],
            }

        total_unrealized = sum(p.get("unrealized_pnl", 0) for p in pm_positions)
        total_realized = sum(t.get("pnl", 0) for t in pm_trades)
        positions_value = sum(p.get("shares", 0) * p.get("current_price", 0) for p in pm_positions)

        return {
            "model_name": self.model_name,
            "portfolio_value": portfolio.balance + positions_value,
            "cash": portfolio.balance,
            "total_pnl": total_realized + total_unrealized,
            "unrealized_pnl": total_unrealized,
            "realized_pnl": total_realized,
            "trades_count": len(pm_trades),
            "positions": pm_positions,
            "positions_value": positions_value,
        }


# Convenience function to create bridge for a model
def create_paper_trading_bridge(
    competition_id: str,
    model_name: str,
    initial_capital: float = 10000.0,
    fee_rate: float = 0.001,
) -> PaperTradingBridge:
    """Create a paper trading bridge for a competition model."""
    return PaperTradingBridge(
        competition_id=competition_id,
        model_name=model_name,
        initial_capital=initial_capital,
        fee_rate=fee_rate,
    )
