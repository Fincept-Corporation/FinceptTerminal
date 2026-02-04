use crate::market_sim::types::*;
use crate::market_sim::events::*;
use std::collections::BTreeMap;

/// Auction engine for opening, closing, and volatility auctions
/// Uses uncrossing algorithm to find the price that maximizes volume
pub struct AuctionEngine {
    /// Accumulated buy orders (price → total quantity)
    buy_orders: BTreeMap<Price, Qty>,
    /// Accumulated sell orders (price → total quantity)
    sell_orders: BTreeMap<Price, Qty>,
    /// Individual orders for trade matching
    buy_order_list: Vec<AuctionOrder>,
    sell_order_list: Vec<AuctionOrder>,
}

#[derive(Debug, Clone)]
struct AuctionOrder {
    order_id: OrderId,
    participant_id: ParticipantId,
    price: Price,      // 0 = market order (will match at any price)
    quantity: Qty,
    remaining: Qty,
}

impl AuctionEngine {
    pub fn new() -> Self {
        Self {
            buy_orders: BTreeMap::new(),
            sell_orders: BTreeMap::new(),
            buy_order_list: Vec::new(),
            sell_order_list: Vec::new(),
        }
    }

    /// Add an order to the auction
    pub fn add_order(&mut self, order: &Order) {
        let auction_order = AuctionOrder {
            order_id: order.id,
            participant_id: order.participant_id,
            price: order.price,
            quantity: order.remaining_quantity,
            remaining: order.remaining_quantity,
        };

        match order.side {
            Side::Buy => {
                *self.buy_orders.entry(order.price).or_insert(0) += order.remaining_quantity;
                self.buy_order_list.push(auction_order);
            }
            Side::Sell => {
                *self.sell_orders.entry(order.price).or_insert(0) += order.remaining_quantity;
                self.sell_order_list.push(auction_order);
            }
        }
    }

    /// Calculate indicative auction price and volume
    pub fn indicative(&self) -> (Price, Qty, Qty, Qty) {
        let (price, volume) = self.find_uncrossing_price();
        let buy_surplus = self.total_buy_at_or_above(price).saturating_sub(volume);
        let sell_surplus = self.total_sell_at_or_below(price).saturating_sub(volume);
        (price, volume, buy_surplus, sell_surplus)
    }

    /// Execute the auction: find clearing price and generate trades
    pub fn execute(
        &mut self,
        instrument_id: InstrumentId,
        timestamp: Nanos,
        next_trade_id: &mut TradeId,
    ) -> AuctionResultEvent {
        let (auction_price, _) = self.find_uncrossing_price();
        let mut trades = Vec::new();

        if auction_price <= 0 {
            return AuctionResultEvent {
                instrument_id,
                auction_price: 0,
                auction_volume: 0,
                trades,
                timestamp,
            };
        }

        // Sort buy orders: market orders first, then highest price first
        self.buy_order_list.sort_by(|a, b| {
            if a.price == 0 && b.price == 0 {
                a.order_id.cmp(&b.order_id)
            } else if a.price == 0 {
                std::cmp::Ordering::Less
            } else if b.price == 0 {
                std::cmp::Ordering::Greater
            } else {
                b.price.cmp(&a.price).then(a.order_id.cmp(&b.order_id))
            }
        });

        // Sort sell orders: market orders first, then lowest price first
        self.sell_order_list.sort_by(|a, b| {
            if a.price == 0 && b.price == 0 {
                a.order_id.cmp(&b.order_id)
            } else if a.price == 0 {
                std::cmp::Ordering::Less
            } else if b.price == 0 {
                std::cmp::Ordering::Greater
            } else {
                a.price.cmp(&b.price).then(a.order_id.cmp(&b.order_id))
            }
        });

        // Filter eligible orders
        let eligible_buys: Vec<&mut AuctionOrder> = self.buy_order_list
            .iter_mut()
            .filter(|o| o.price == 0 || o.price >= auction_price)
            .collect();

        let eligible_sells: Vec<&mut AuctionOrder> = self.sell_order_list
            .iter_mut()
            .filter(|o| o.price == 0 || o.price <= auction_price)
            .collect();

        // Match orders
        let mut sell_idx = 0;
        let mut eligible_sells = eligible_sells;
        let mut total_volume: Qty = 0;

        for buy in eligible_buys {
            while sell_idx < eligible_sells.len() && buy.remaining > 0 {
                let sell = &mut eligible_sells[sell_idx];
                if sell.remaining <= 0 {
                    sell_idx += 1;
                    continue;
                }

                // Self-trade prevention
                if buy.participant_id == sell.participant_id {
                    sell_idx += 1;
                    continue;
                }

                let fill_qty = buy.remaining.min(sell.remaining);
                buy.remaining -= fill_qty;
                sell.remaining -= fill_qty;
                total_volume += fill_qty;

                let trade = Trade {
                    id: *next_trade_id,
                    instrument_id,
                    price: auction_price,
                    quantity: fill_qty,
                    aggressor_side: Side::Buy, // auction trades don't have a true aggressor
                    buyer_id: buy.participant_id,
                    seller_id: sell.participant_id,
                    buyer_order_id: buy.order_id,
                    seller_order_id: sell.order_id,
                    timestamp,
                    venue_id: 0,
                    is_auction_trade: true,
                };

                *next_trade_id += 1;
                trades.push(trade);

                if sell.remaining <= 0 {
                    sell_idx += 1;
                }
            }
        }

        AuctionResultEvent {
            instrument_id,
            auction_price,
            auction_volume: total_volume,
            trades,
            timestamp,
        }
    }

    /// Find the price that maximizes executable volume
    /// Ties broken by: minimum surplus, then reference price proximity
    fn find_uncrossing_price(&self) -> (Price, Qty) {
        // Collect all unique price levels
        let mut all_prices: Vec<Price> = self.buy_orders.keys()
            .chain(self.sell_orders.keys())
            .filter(|&&p| p > 0)
            .copied()
            .collect();
        all_prices.sort();
        all_prices.dedup();

        if all_prices.is_empty() {
            return (0, 0);
        }

        let mut best_price: Price = 0;
        let mut best_volume: Qty = 0;
        let mut best_surplus: Qty = Qty::MAX;

        for &price in &all_prices {
            let cum_buy = self.total_buy_at_or_above(price);
            let cum_sell = self.total_sell_at_or_below(price);
            let executable = cum_buy.min(cum_sell);
            let surplus = (cum_buy - cum_sell).abs();

            if executable > best_volume || (executable == best_volume && surplus < best_surplus) {
                best_price = price;
                best_volume = executable;
                best_surplus = surplus;
            }
        }

        (best_price, best_volume)
    }

    /// Total buy quantity at or above a given price (including market orders)
    fn total_buy_at_or_above(&self, price: Price) -> Qty {
        let market_qty: Qty = self.buy_orders.get(&0).copied().unwrap_or(0);
        let limit_qty: Qty = self.buy_orders
            .range(price..)
            .map(|(_, &qty)| qty)
            .sum();
        market_qty + limit_qty
    }

    /// Total sell quantity at or below a given price (including market orders)
    fn total_sell_at_or_below(&self, price: Price) -> Qty {
        let market_qty: Qty = self.sell_orders.get(&0).copied().unwrap_or(0);
        let limit_qty: Qty = self.sell_orders
            .range(..=price)
            .map(|(_, &qty)| qty)
            .sum();
        market_qty + limit_qty
    }

    /// Clear the auction book
    pub fn clear(&mut self) {
        self.buy_orders.clear();
        self.sell_orders.clear();
        self.buy_order_list.clear();
        self.sell_order_list.clear();
    }

    pub fn order_count(&self) -> usize {
        self.buy_order_list.len() + self.sell_order_list.len()
    }
}
