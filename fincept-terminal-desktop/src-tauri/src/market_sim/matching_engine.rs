use crate::market_sim::types::*;
use crate::market_sim::orderbook::OrderBook;
use crate::market_sim::events::*;
use std::collections::HashMap;

/// The matching engine processes incoming orders against the order book
/// using price-time priority (FIFO at each price level).
pub struct MatchingEngine {
    books: HashMap<InstrumentId, OrderBook>,
    instruments: HashMap<InstrumentId, Instrument>,
    next_order_id: OrderId,
    next_trade_id: TradeId,
}

impl MatchingEngine {
    pub fn new() -> Self {
        Self {
            books: HashMap::new(),
            instruments: HashMap::new(),
            next_order_id: 1,
            next_trade_id: 1,
        }
    }

    pub fn add_instrument(&mut self, instrument: Instrument) {
        let id = instrument.id;
        self.instruments.insert(id, instrument);
        self.books.insert(id, OrderBook::new(id));
    }

    pub fn get_book(&self, instrument_id: InstrumentId) -> Option<&OrderBook> {
        self.books.get(&instrument_id)
    }

    pub fn get_book_mut(&mut self, instrument_id: InstrumentId) -> Option<&mut OrderBook> {
        self.books.get_mut(&instrument_id)
    }

    pub fn allocate_order_id(&mut self) -> OrderId {
        let id = self.next_order_id;
        self.next_order_id += 1;
        id
    }

    fn allocate_trade_id(&mut self) -> TradeId {
        let id = self.next_trade_id;
        self.next_trade_id += 1;
        id
    }

    /// Process a new order. Returns events generated.
    pub fn process_order(&mut self, mut order: Order, timestamp: Nanos) -> Vec<SimEvent> {
        let mut events = Vec::new();

        // Validate instrument exists
        let instrument = match self.instruments.get(&order.instrument_id) {
            Some(i) => i.clone(),
            None => {
                events.push(SimEvent::OrderRejected(OrderRejectedEvent {
                    order_id: order.id,
                    participant_id: order.participant_id,
                    reason: RejectReason::InvalidPrice,
                    timestamp,
                }));
                return events;
            }
        };

        // Validate price for limit orders
        if matches!(order.order_type, OrderType::Limit | OrderType::StopLimit | OrderType::Iceberg) {
            if !instrument.validate_price(order.price) {
                events.push(SimEvent::OrderRejected(OrderRejectedEvent {
                    order_id: order.id,
                    participant_id: order.participant_id,
                    reason: RejectReason::InvalidPrice,
                    timestamp,
                }));
                return events;
            }
        }

        // Validate quantity
        if !instrument.validate_quantity(order.quantity) {
            events.push(SimEvent::OrderRejected(OrderRejectedEvent {
                order_id: order.id,
                participant_id: order.participant_id,
                reason: RejectReason::InvalidQuantity,
                timestamp,
            }));
            return events;
        }

        // Accept the order
        events.push(SimEvent::OrderAccepted(OrderAcceptedEvent {
            order_id: order.id,
            participant_id: order.participant_id,
            side: order.side,
            timestamp,
        }));

        // Handle different order types
        match order.order_type {
            OrderType::Market => {
                self.match_aggressive_order(&mut order, timestamp, &mut events);
                // Market orders: any unfilled remainder is cancelled (IOC behavior)
                if order.remaining_quantity > 0 {
                    order.status = OrderStatus::Cancelled;
                    events.push(SimEvent::OrderCancelled(OrderCancelledEvent {
                        order_id: order.id,
                        participant_id: order.participant_id,
                        remaining_quantity: order.remaining_quantity,
                        timestamp,
                    }));
                }
            }
            OrderType::Limit => {
                self.match_aggressive_order(&mut order, timestamp, &mut events);
                // Rest any remaining quantity
                if order.remaining_quantity > 0 {
                    self.handle_tif_and_rest(&mut order, timestamp, &mut events);
                }
            }
            OrderType::Iceberg => {
                self.match_aggressive_order(&mut order, timestamp, &mut events);
                if order.remaining_quantity > 0 {
                    self.handle_tif_and_rest(&mut order, timestamp, &mut events);
                }
            }
            OrderType::Stop => {
                // Stop orders rest until trigger price is hit
                // They are tracked separately and converted to market when triggered
                if self.is_stop_triggered(&order) {
                    order.order_type = OrderType::Market;
                    self.match_aggressive_order(&mut order, timestamp, &mut events);
                    if order.remaining_quantity > 0 {
                        order.status = OrderStatus::Cancelled;
                    }
                } else {
                    // Store as pending stop (in real impl, separate stop book)
                    if let Some(book) = self.books.get_mut(&order.instrument_id) {
                        book.insert_order(order);
                    }
                }
            }
            OrderType::StopLimit => {
                if self.is_stop_triggered(&order) {
                    order.order_type = OrderType::Limit;
                    self.match_aggressive_order(&mut order, timestamp, &mut events);
                    if order.remaining_quantity > 0 {
                        self.handle_tif_and_rest(&mut order, timestamp, &mut events);
                    }
                } else {
                    if let Some(book) = self.books.get_mut(&order.instrument_id) {
                        book.insert_order(order);
                    }
                }
            }
            OrderType::MarketToLimit => {
                // Execute at best available, then rest remainder as limit at last fill price
                let pre_fill = order.filled_quantity;
                self.match_aggressive_order(&mut order, timestamp, &mut events);
                if order.remaining_quantity > 0 {
                    if order.filled_quantity > pre_fill {
                        // Got fills: set limit price to the last fill price (set during matching)
                        order.order_type = OrderType::Limit;
                        if let Some(book) = self.books.get_mut(&order.instrument_id) {
                            book.insert_order(order);
                        }
                    } else {
                        // No fills: cancel the order (no liquidity at any price)
                        order.status = OrderStatus::Cancelled;
                        events.push(SimEvent::OrderCancelled(OrderCancelledEvent {
                            order_id: order.id,
                            participant_id: order.participant_id,
                            remaining_quantity: order.remaining_quantity,
                            timestamp,
                        }));
                    }
                }
            }
            OrderType::Pegged(_) => {
                // Pegged orders: price derived from BBO
                self.repeg_order(&mut order);
                self.match_aggressive_order(&mut order, timestamp, &mut events);
                if order.remaining_quantity > 0 {
                    if let Some(book) = self.books.get_mut(&order.instrument_id) {
                        book.insert_order(order);
                    }
                }
            }
            OrderType::TrailingStop => {
                // Similar to stop but trigger moves with price
                if let Some(book) = self.books.get_mut(&order.instrument_id) {
                    book.insert_order(order);
                }
            }
        }

        events
    }

    /// Match an aggressive (incoming) order against resting orders
    fn match_aggressive_order(
        &mut self,
        order: &mut Order,
        timestamp: Nanos,
        events: &mut Vec<SimEvent>,
    ) {
        let instrument_id = order.instrument_id;
        let book = match self.books.get_mut(&instrument_id) {
            Some(b) => b,
            None => return,
        };

        // Collect fills first to avoid borrow issues
        let mut fills: Vec<(OrderId, Price, Qty)> = Vec::new();

        // For FOK, check if entire quantity can be filled
        if matches!(order.time_in_force, TimeInForce::FOK) {
            let available = match order.side {
                Side::Buy => {
                    if order.price > 0 {
                        book.available_liquidity(order.side, order.price)
                    } else {
                        // Market order: all ask liquidity
                        book.available_liquidity(order.side, Price::MAX)
                    }
                }
                Side::Sell => {
                    if order.price > 0 {
                        book.available_liquidity(order.side, order.price)
                    } else {
                        book.available_liquidity(order.side, 0)
                    }
                }
            };
            if available < order.remaining_quantity {
                order.status = OrderStatus::Cancelled;
                events.push(SimEvent::OrderCancelled(OrderCancelledEvent {
                    order_id: order.id,
                    participant_id: order.participant_id,
                    remaining_quantity: order.remaining_quantity,
                    timestamp,
                }));
                return;
            }
        }

        // Walk through price levels
        let prices_and_orders: Vec<(Price, Vec<OrderId>)> = match order.side {
            Side::Buy => {
                book.best_asks()
                    .take_while(|(&price, _)| {
                        order.price == 0 || price <= order.price // market orders match any price
                    })
                    .map(|(&p, level)| (p, level.orders.iter().copied().collect()))
                    .collect()
            }
            Side::Sell => {
                book.best_bids()
                    .take_while(|(&price, _)| {
                        order.price == 0 || price >= order.price
                    })
                    .map(|(&p, level)| (p, level.orders.iter().copied().collect()))
                    .collect()
            }
        };

        for (match_price, order_ids) in prices_and_orders {
            if order.remaining_quantity <= 0 {
                break;
            }
            for resting_id in order_ids {
                if order.remaining_quantity <= 0 {
                    break;
                }
                if let Some(resting) = book.get_order(resting_id) {
                    // Self-trade prevention
                    if resting.participant_id == order.participant_id {
                        continue;
                    }
                    let fill_qty = order.remaining_quantity.min(resting.remaining_quantity);
                    fills.push((resting_id, match_price, fill_qty));
                    order.fill(fill_qty, timestamp);
                }
            }
        }

        // Pre-allocate trade IDs to avoid borrowing self while book is borrowed
        let num_fills = fills.len();
        let first_trade_id = self.next_trade_id;
        self.next_trade_id += num_fills as u64;

        // Apply fills
        for (idx, (resting_id, match_price, fill_qty)) in fills.iter().enumerate() {
            let resting_order = book.get_order(*resting_id).cloned();
            book.reduce_order(*resting_id, *fill_qty, timestamp);
            book.record_trade(*match_price, *fill_qty);

            // For market-to-limit, track last fill price
            if matches!(order.order_type, OrderType::MarketToLimit) {
                order.price = *match_price;
            }

            if let Some(resting) = resting_order {
                let (buyer_id, seller_id, buyer_order, seller_order) = match order.side {
                    Side::Buy => (order.participant_id, resting.participant_id, order.id, resting.id),
                    Side::Sell => (resting.participant_id, order.participant_id, resting.id, order.id),
                };

                let trade = Trade {
                    id: first_trade_id + idx as u64,
                    instrument_id,
                    price: *match_price,
                    quantity: *fill_qty,
                    aggressor_side: order.side,
                    buyer_id,
                    seller_id,
                    buyer_order_id: buyer_order,
                    seller_order_id: seller_order,
                    timestamp,
                    venue_id: order.venue_id,
                    is_auction_trade: false,
                };

                events.push(SimEvent::TradeExecuted(TradeExecutedEvent { trade }));
            }
        }
    }

    /// Handle time-in-force rules and rest the order if appropriate
    fn handle_tif_and_rest(
        &mut self,
        order: &mut Order,
        timestamp: Nanos,
        events: &mut Vec<SimEvent>,
    ) {
        match order.time_in_force {
            TimeInForce::IOC => {
                // Cancel remaining
                order.status = OrderStatus::Cancelled;
                events.push(SimEvent::OrderCancelled(OrderCancelledEvent {
                    order_id: order.id,
                    participant_id: order.participant_id,
                    remaining_quantity: order.remaining_quantity,
                    timestamp,
                }));
            }
            TimeInForce::FOK => {
                // Should have been fully filled or not at all (handled above)
                order.status = OrderStatus::Cancelled;
                events.push(SimEvent::OrderCancelled(OrderCancelledEvent {
                    order_id: order.id,
                    participant_id: order.participant_id,
                    remaining_quantity: order.remaining_quantity,
                    timestamp,
                }));
            }
            _ => {
                // Rest the order in the book
                if let Some(book) = self.books.get_mut(&order.instrument_id) {
                    book.insert_order(order.clone());
                }
            }
        }
    }

    fn is_stop_triggered(&self, order: &Order) -> bool {
        if let Some(book) = self.books.get(&order.instrument_id) {
            match order.side {
                Side::Buy => book.last_trade_price >= order.stop_price,
                Side::Sell => book.last_trade_price <= order.stop_price,
            }
        } else {
            false
        }
    }

    fn repeg_order(&self, order: &mut Order) {
        if let OrderType::Pegged(peg_type) = order.order_type {
            if let Some(book) = self.books.get(&order.instrument_id) {
                order.price = match peg_type {
                    PegType::Midpoint => {
                        book.midpoint().map(|m| m as Price).unwrap_or(0)
                    }
                    PegType::Primary => {
                        match order.side {
                            Side::Buy => book.best_bid.unwrap_or(0),
                            Side::Sell => book.best_ask.unwrap_or(0),
                        }
                    }
                    PegType::Market => {
                        match order.side {
                            Side::Buy => book.best_ask.unwrap_or(0),
                            Side::Sell => book.best_bid.unwrap_or(0),
                        }
                    }
                };
            }
        }
    }

    /// Update all pegged orders when BBO changes. Returns events from any resulting matches.
    pub fn update_pegged_orders(&mut self, instrument_id: InstrumentId, timestamp: Nanos) -> Vec<SimEvent> {
        let mut events = Vec::new();

        let book = match self.books.get(&instrument_id) {
            Some(b) => b,
            None => return events,
        };

        // Collect pegged orders that need repricing
        let mut orders_to_repeg: Vec<Order> = Vec::new();

        for order in book.orders.values() {
            if matches!(order.order_type, OrderType::Pegged(_)) {
                orders_to_repeg.push(order.clone());
            }
        }

        // Reprice each pegged order
        for mut order in orders_to_repeg {
            let old_price = order.price;

            // Calculate new peg price
            if let Some(book) = self.books.get(&instrument_id) {
                if let OrderType::Pegged(peg_type) = order.order_type {
                    let new_price = match peg_type {
                        PegType::Midpoint => {
                            book.midpoint().map(|m| m as Price).unwrap_or(0)
                        }
                        PegType::Primary => {
                            match order.side {
                                Side::Buy => book.best_bid.unwrap_or(0),
                                Side::Sell => book.best_ask.unwrap_or(0),
                            }
                        }
                        PegType::Market => {
                            match order.side {
                                Side::Buy => book.best_ask.unwrap_or(0),
                                Side::Sell => book.best_bid.unwrap_or(0),
                            }
                        }
                    };

                    if new_price != old_price && new_price > 0 {
                        // Remove order and resubmit at new price
                        if let Some(book_mut) = self.books.get_mut(&instrument_id) {
                            book_mut.cancel_order(order.id);
                        }
                        order.price = new_price;
                        order.id = self.allocate_order_id();
                        let repeg_events = self.process_order(order, timestamp);
                        events.extend(repeg_events);
                    }
                }
            }
        }

        events
    }

    /// Cancel an order
    pub fn cancel_order(
        &mut self,
        instrument_id: InstrumentId,
        order_id: OrderId,
        participant_id: ParticipantId,
        timestamp: Nanos,
    ) -> Vec<SimEvent> {
        let mut events = Vec::new();

        if let Some(book) = self.books.get_mut(&instrument_id) {
            // Verify ownership
            if let Some(order) = book.get_order(order_id) {
                if order.participant_id != participant_id {
                    return events; // not your order
                }
            }

            if let Some(cancelled) = book.cancel_order(order_id) {
                events.push(SimEvent::OrderCancelled(OrderCancelledEvent {
                    order_id,
                    participant_id: cancelled.participant_id,
                    remaining_quantity: cancelled.remaining_quantity,
                    timestamp,
                }));
            }
        }

        events
    }

    /// Modify an order (cancel + replace with new priority)
    pub fn modify_order(
        &mut self,
        instrument_id: InstrumentId,
        order_id: OrderId,
        participant_id: ParticipantId,
        new_price: Price,
        new_quantity: Qty,
        timestamp: Nanos,
    ) -> Vec<SimEvent> {
        let mut events = Vec::new();

        if let Some(book) = self.books.get_mut(&instrument_id) {
            if let Some(old_order) = book.cancel_order(order_id) {
                if old_order.participant_id != participant_id {
                    // Re-insert if not owner
                    book.insert_order(old_order);
                    return events;
                }

                events.push(SimEvent::OrderModified(OrderModifiedEvent {
                    order_id,
                    participant_id,
                    new_price,
                    new_quantity,
                    timestamp,
                }));

                // Create replacement order with new ID for proper priority
                let mut new_order = old_order.clone();
                new_order.id = self.allocate_order_id();
                new_order.price = new_price;
                new_order.quantity = new_quantity;
                new_order.remaining_quantity = new_quantity;
                new_order.timestamp = timestamp;
                new_order.last_update = timestamp;
                new_order.status = OrderStatus::New;

                // Process the new order through matching
                let new_events = self.process_order(new_order, timestamp);
                events.extend(new_events);
            }
        }

        events
    }

    /// Get all instrument IDs
    pub fn instrument_ids(&self) -> Vec<InstrumentId> {
        self.instruments.keys().copied().collect()
    }

    /// Get instrument
    pub fn get_instrument(&self, id: InstrumentId) -> Option<&Instrument> {
        self.instruments.get(&id)
    }

    /// Check and trigger stop orders after a trade. Returns events from triggered orders.
    pub fn check_stop_orders(&mut self, instrument_id: InstrumentId, timestamp: Nanos) -> Vec<SimEvent> {
        let mut events = Vec::new();

        let book = match self.books.get(&instrument_id) {
            Some(b) => b,
            None => return events,
        };

        let last_price = book.last_trade_price;
        if last_price == 0 {
            return events;
        }

        // Collect stop orders that should trigger
        let mut orders_to_trigger: Vec<Order> = Vec::new();

        for order in book.orders.values() {
            let should_trigger = match order.order_type {
                OrderType::Stop | OrderType::StopLimit | OrderType::TrailingStop => {
                    match order.side {
                        Side::Buy => last_price >= order.stop_price,
                        Side::Sell => last_price <= order.stop_price,
                    }
                }
                _ => false,
            };

            if should_trigger {
                orders_to_trigger.push(order.clone());
            }
        }

        // Process triggered orders
        for mut order in orders_to_trigger {
            // Remove from book first
            if let Some(book) = self.books.get_mut(&instrument_id) {
                book.cancel_order(order.id);
            }

            // Convert stop to market/limit and process
            match order.order_type {
                OrderType::Stop | OrderType::TrailingStop => {
                    order.order_type = OrderType::Market;
                    order.price = 0;
                }
                OrderType::StopLimit => {
                    order.order_type = OrderType::Limit;
                    // price already set as limit price
                }
                _ => {}
            }

            let triggered_events = self.process_order(order, timestamp);
            events.extend(triggered_events);
        }

        events
    }

    /// Update trailing stop prices based on market movement
    pub fn update_trailing_stops(&mut self, instrument_id: InstrumentId) {
        let book = match self.books.get_mut(&instrument_id) {
            Some(b) => b,
            None => return,
        };

        let last_price = book.last_trade_price;
        if last_price == 0 {
            return;
        }

        // Collect order IDs that need updating
        let updates: Vec<(OrderId, Price)> = book.orders.iter()
            .filter_map(|(id, order)| {
                if !matches!(order.order_type, OrderType::TrailingStop) {
                    return None;
                }

                let new_stop = match order.side {
                    Side::Buy => {
                        // For buy stops, stop price trails below market
                        // If market moves down, stop moves down (tighter)
                        let potential_stop = last_price + order.trailing_offset;
                        if potential_stop < order.stop_price {
                            Some(potential_stop)
                        } else {
                            None
                        }
                    }
                    Side::Sell => {
                        // For sell stops, stop price trails above market
                        // If market moves up, stop moves up (tighter)
                        let potential_stop = last_price.saturating_sub(order.trailing_offset);
                        if potential_stop > order.stop_price {
                            Some(potential_stop)
                        } else {
                            None
                        }
                    }
                };

                new_stop.map(|s| (*id, s))
            })
            .collect();

        // Apply updates
        for (order_id, new_stop) in updates {
            if let Some(order) = book.orders.get_mut(&order_id) {
                order.stop_price = new_stop;
            }
        }
    }

    /// Reset for new trading day
    pub fn reset_daily(&mut self) {
        for book in self.books.values_mut() {
            book.clear();
            book.reset_daily_stats();
        }
    }
}
