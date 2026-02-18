use std::collections::{BTreeMap, HashMap, VecDeque};
use crate::market_sim::types::*;

/// A single price level in the order book
#[derive(Debug, Clone)]
pub struct BookLevel {
    pub price: Price,
    pub total_quantity: Qty,
    pub visible_quantity: Qty,
    pub order_count: u32,
    pub orders: VecDeque<OrderId>,  // FIFO queue for time priority
}

impl BookLevel {
    pub fn new(price: Price) -> Self {
        Self {
            price,
            total_quantity: 0,
            visible_quantity: 0,
            order_count: 0,
            orders: VecDeque::new(),
        }
    }

    pub fn add_order(&mut self, order_id: OrderId, qty: Qty, display_qty: Qty) {
        self.orders.push_back(order_id);
        self.total_quantity += qty;
        self.visible_quantity += display_qty;
        self.order_count += 1;
    }

    pub fn remove_order(&mut self, order_id: OrderId, qty: Qty, display_qty: Qty) -> bool {
        if let Some(pos) = self.orders.iter().position(|&id| id == order_id) {
            self.orders.remove(pos);
            self.total_quantity -= qty;
            self.visible_quantity -= display_qty;
            self.order_count -= 1;
            true
        } else {
            false
        }
    }

    pub fn is_empty(&self) -> bool {
        self.orders.is_empty()
    }

    pub fn front_order(&self) -> Option<OrderId> {
        self.orders.front().copied()
    }
}

/// High-performance order book for a single instrument
pub struct OrderBook {
    pub instrument_id: InstrumentId,

    // BTreeMap gives us sorted price levels
    // For bids: higher price = better (reverse iteration)
    // For asks: lower price = better (forward iteration)
    bids: BTreeMap<Price, BookLevel>,
    asks: BTreeMap<Price, BookLevel>,

    // Fast order lookup (pub for stop order checking)
    pub orders: HashMap<OrderId, Order>,

    // Market data cache
    pub best_bid: Option<Price>,
    pub best_ask: Option<Price>,
    pub last_trade_price: Price,
    pub last_trade_qty: Qty,
    pub volume: Qty,
    pub trade_count: u64,
    pub vwap_numerator: f64,  // sum(price * qty)
    pub open_price: Price,
    pub high_price: Price,
    pub low_price: Price,
}

impl OrderBook {
    pub fn new(instrument_id: InstrumentId) -> Self {
        Self {
            instrument_id,
            bids: BTreeMap::new(),
            asks: BTreeMap::new(),
            orders: HashMap::new(),
            best_bid: None,
            best_ask: None,
            last_trade_price: 0,
            last_trade_qty: 0,
            volume: 0,
            trade_count: 0,
            vwap_numerator: 0.0,
            open_price: 0,
            high_price: 0,
            low_price: Price::MAX,
        }
    }

    /// Insert an order into the book (resting order, not matched)
    pub fn insert_order(&mut self, mut order: Order) {
        let price = order.price;
        let remaining = order.remaining_quantity;
        let display = if order.hidden {
            0
        } else {
            match order.order_type {
                OrderType::Iceberg => {
                    // Set initial current_display if not already set
                    if order.current_display == 0 || order.current_display > remaining {
                        order.current_display = order.display_quantity.min(remaining);
                    }
                    order.current_display
                }
                _ => remaining,
            }
        };
        let order_id = order.id;
        let side = order.side;

        self.orders.insert(order_id, order);

        let book_side = match side {
            Side::Buy => &mut self.bids,
            Side::Sell => &mut self.asks,
        };

        let level = book_side.entry(price).or_insert_with(|| BookLevel::new(price));
        level.add_order(order_id, remaining, display);

        self.update_bbo();
    }

    /// Remove an order from the book
    pub fn cancel_order(&mut self, order_id: OrderId) -> Option<Order> {
        if let Some(mut order) = self.orders.remove(&order_id) {
            let remaining = order.remaining_quantity;
            let display = if order.hidden {
                0
            } else {
                match order.order_type {
                    OrderType::Iceberg => order.current_display,
                    _ => remaining,
                }
            };

            let book_side = match order.side {
                Side::Buy => &mut self.bids,
                Side::Sell => &mut self.asks,
            };

            if let Some(level) = book_side.get_mut(&order.price) {
                level.remove_order(order_id, remaining, display);
                if level.is_empty() {
                    book_side.remove(&order.price);
                }
            }

            order.status = OrderStatus::Cancelled;
            self.update_bbo();
            Some(order)
        } else {
            None
        }
    }

    /// Reduce order quantity after a partial fill
    pub fn reduce_order(&mut self, order_id: OrderId, fill_qty: Qty, timestamp: Nanos) -> bool {
        if let Some(order) = self.orders.get_mut(&order_id) {
            let old_remaining = order.remaining_quantity;
            let old_display = if order.hidden {
                0
            } else {
                match order.order_type {
                    OrderType::Iceberg => order.current_display,
                    _ => old_remaining,
                }
            };

            order.fill(fill_qty, timestamp);
            let new_remaining = order.remaining_quantity;

            if new_remaining <= 0 {
                // Fully filled, remove from book
                let book_side = match order.side {
                    Side::Buy => &mut self.bids,
                    Side::Sell => &mut self.asks,
                };
                let price = order.price;
                if let Some(level) = book_side.get_mut(&price) {
                    level.remove_order(order_id, old_remaining, old_display);
                    if level.is_empty() {
                        book_side.remove(&price);
                    }
                }
                self.orders.remove(&order_id);
            } else {
                // Partial fill — update level quantities
                let new_display = if order.hidden {
                    0
                } else {
                    match order.order_type {
                        OrderType::Iceberg => {
                            // Iceberg: reduce current display by fill amount
                            let remaining_display = old_display.saturating_sub(fill_qty);
                            if remaining_display == 0 {
                                // Reload visible portion from hidden reserve
                                let reload = order.display_quantity.min(new_remaining);
                                order.current_display = reload;
                                reload
                            } else {
                                order.current_display = remaining_display;
                                remaining_display
                            }
                        }
                        _ => new_remaining,
                    }
                };

                let book_side = match order.side {
                    Side::Buy => &mut self.bids,
                    Side::Sell => &mut self.asks,
                };
                if let Some(level) = book_side.get_mut(&order.price) {
                    level.total_quantity -= fill_qty;
                    level.visible_quantity = level.visible_quantity - old_display + new_display;
                }
            }

            self.update_bbo();
            true
        } else {
            false
        }
    }

    /// Record a trade for market data tracking
    pub fn record_trade(&mut self, price: Price, qty: Qty) {
        if self.open_price == 0 {
            self.open_price = price;
        }
        if price > self.high_price {
            self.high_price = price;
        }
        if price < self.low_price {
            self.low_price = price;
        }
        self.last_trade_price = price;
        self.last_trade_qty = qty;
        self.volume += qty;
        self.trade_count += 1;
        self.vwap_numerator += price as f64 * qty as f64;
    }

    pub fn vwap(&self) -> f64 {
        if self.volume > 0 {
            self.vwap_numerator / self.volume as f64
        } else {
            0.0
        }
    }

    fn update_bbo(&mut self) {
        self.best_bid = self.bids.keys().next_back().copied();
        self.best_ask = self.asks.keys().next().copied();
    }

    pub fn spread(&self) -> Option<Price> {
        match (self.best_bid, self.best_ask) {
            (Some(bid), Some(ask)) => Some(ask - bid),
            _ => None,
        }
    }

    pub fn midpoint(&self) -> Option<f64> {
        match (self.best_bid, self.best_ask) {
            (Some(bid), Some(ask)) => Some((bid as f64 + ask as f64) / 2.0),
            _ => None,
        }
    }

    /// Get the top N price levels for a side
    pub fn get_depth(&self, side: Side, levels: usize) -> Vec<PriceLevel> {
        let book_side = match side {
            Side::Buy => &self.bids,
            Side::Sell => &self.asks,
        };

        let iter: Box<dyn Iterator<Item = (&Price, &BookLevel)>> = match side {
            Side::Buy => Box::new(book_side.iter().rev()),
            Side::Sell => Box::new(book_side.iter()),
        };

        iter.take(levels)
            .map(|(_, level)| PriceLevel {
                price: level.price,
                quantity: level.visible_quantity,
                order_count: level.order_count,
            })
            .collect()
    }

    /// Get L1 quote
    pub fn l1_quote(&self, timestamp: Nanos) -> L1Quote {
        let (bid_price, bid_size) = self
            .bids
            .iter()
            .next_back()
            .map(|(_, l)| (l.price, l.visible_quantity))
            .unwrap_or((0, 0));

        let (ask_price, ask_size) = self
            .asks
            .iter()
            .next()
            .map(|(_, l)| (l.price, l.visible_quantity))
            .unwrap_or((0, 0));

        L1Quote {
            instrument_id: self.instrument_id,
            bid_price,
            bid_size,
            ask_price,
            ask_size,
            last_price: self.last_trade_price,
            last_size: self.last_trade_qty,
            volume: self.volume,
            vwap: self.vwap(),
            open: self.open_price,
            high: self.high_price,
            low: if self.low_price == Price::MAX { 0 } else { self.low_price },
            timestamp,
        }
    }

    /// Get L2 snapshot
    pub fn l2_snapshot(&self, depth: usize, timestamp: Nanos) -> L2Snapshot {
        L2Snapshot {
            instrument_id: self.instrument_id,
            bids: self.get_depth(Side::Buy, depth),
            asks: self.get_depth(Side::Sell, depth),
            timestamp,
        }
    }

    /// Get all resting orders (for auction processing)
    pub fn all_bid_orders(&self) -> Vec<&Order> {
        self.bids
            .values()
            .rev()
            .flat_map(|level| level.orders.iter().filter_map(|id| self.orders.get(id)))
            .collect()
    }

    pub fn all_ask_orders(&self) -> Vec<&Order> {
        self.asks
            .values()
            .flat_map(|level| level.orders.iter().filter_map(|id| self.orders.get(id)))
            .collect()
    }

    /// Check if a price would cross the book (aggressive order)
    pub fn would_cross(&self, side: Side, price: Price) -> bool {
        match side {
            Side::Buy => self.best_ask.map_or(false, |ask| price >= ask),
            Side::Sell => self.best_bid.map_or(false, |bid| price <= bid),
        }
    }

    /// Available liquidity at or better than a price
    pub fn available_liquidity(&self, side: Side, price_limit: Price) -> Qty {
        match side {
            Side::Buy => {
                // Liquidity on the ask side at or below price_limit
                self.asks
                    .range(..=price_limit)
                    .map(|(_, level)| level.total_quantity)
                    .sum()
            }
            Side::Sell => {
                // Liquidity on the bid side at or above price_limit
                self.bids
                    .range(price_limit..)
                    .map(|(_, level)| level.total_quantity)
                    .sum()
            }
        }
    }

    /// Get order by ID
    pub fn get_order(&self, order_id: OrderId) -> Option<&Order> {
        self.orders.get(&order_id)
    }

    /// Total number of resting orders
    pub fn order_count(&self) -> usize {
        self.orders.len()
    }

    /// Get best N ask levels with their order queues (for matching)
    pub fn best_asks(&self) -> impl Iterator<Item = (&Price, &BookLevel)> {
        self.asks.iter()
    }

    /// Get best N bid levels with their order queues (for matching)
    pub fn best_bids(&self) -> impl Iterator<Item = (&Price, &BookLevel)> {
        self.bids.iter().rev()
    }

    /// Clear all orders (for end of day)
    pub fn clear(&mut self) {
        self.bids.clear();
        self.asks.clear();
        self.orders.clear();
        self.update_bbo();
    }

    /// Reset daily stats
    pub fn reset_daily_stats(&mut self) {
        self.volume = 0;
        self.trade_count = 0;
        self.vwap_numerator = 0.0;
        self.open_price = 0;
        self.high_price = 0;
        self.low_price = Price::MAX;
    }
}
