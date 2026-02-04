use crate::market_sim::types::*;
use crate::market_sim::orderbook::OrderBook;
use std::collections::HashMap;

/// Market data feed types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FeedType {
    /// Direct exchange feed - fastest, full depth
    DirectFeed,
    /// Consolidated feed (like SIP/NBBO) - slower, aggregated
    ConsolidatedFeed,
}

/// Simulates asymmetric market data delivery
/// Different participants see different data at different times
pub struct MarketDataFeed {
    /// Per-instrument quote cache
    quotes: HashMap<InstrumentId, L1Quote>,
    /// Per-instrument depth cache
    depth_cache: HashMap<InstrumentId, L2Snapshot>,
    /// Feed latency by type (nanos)
    direct_feed_latency: Nanos,
    consolidated_feed_latency: Nanos,
    /// Depth levels for L2
    l2_depth: usize,
}

impl MarketDataFeed {
    pub fn new() -> Self {
        Self {
            quotes: HashMap::new(),
            depth_cache: HashMap::new(),
            direct_feed_latency: 1_000,         // 1 microsecond
            consolidated_feed_latency: 500_000, // 500 microseconds (SIP delay)
            l2_depth: 10,
        }
    }

    pub fn with_latencies(direct_ns: Nanos, consolidated_ns: Nanos) -> Self {
        Self {
            quotes: HashMap::new(),
            depth_cache: HashMap::new(),
            direct_feed_latency: direct_ns,
            consolidated_feed_latency: consolidated_ns,
            l2_depth: 10,
        }
    }

    /// Update market data from order book state
    pub fn update_from_book(&mut self, book: &OrderBook, timestamp: Nanos) {
        let quote = book.l1_quote(timestamp);
        self.quotes.insert(book.instrument_id, quote);

        let depth = book.l2_snapshot(self.l2_depth, timestamp);
        self.depth_cache.insert(book.instrument_id, depth);
    }

    /// Get quote as seen by a participant with given feed type
    /// Returns (quote, effective_timestamp) accounting for feed latency
    pub fn get_quote(
        &self,
        instrument_id: InstrumentId,
        feed_type: FeedType,
        _request_time: Nanos,
    ) -> Option<L1Quote> {
        let latency = match feed_type {
            FeedType::DirectFeed => self.direct_feed_latency,
            FeedType::ConsolidatedFeed => self.consolidated_feed_latency,
        };

        self.quotes.get(&instrument_id).map(|q| {
            let mut stale_quote = q.clone();
            // The quote the participant sees is "latency" old
            stale_quote.timestamp = q.timestamp.saturating_sub(latency);
            stale_quote
        })
    }

    /// Get L2 depth
    pub fn get_depth(
        &self,
        instrument_id: InstrumentId,
        feed_type: FeedType,
    ) -> Option<L2Snapshot> {
        let latency = match feed_type {
            FeedType::DirectFeed => self.direct_feed_latency,
            FeedType::ConsolidatedFeed => self.consolidated_feed_latency,
        };

        self.depth_cache.get(&instrument_id).map(|d| {
            let mut stale_depth = d.clone();
            stale_depth.timestamp = d.timestamp.saturating_sub(latency);
            stale_depth
        })
    }

    /// Get NBBO (National Best Bid/Offer) across venues
    /// In our single-venue sim, this is just the BBO
    pub fn get_nbbo(&self, instrument_id: InstrumentId) -> Option<(Price, Qty, Price, Qty)> {
        self.quotes.get(&instrument_id).map(|q| {
            (q.bid_price, q.bid_size, q.ask_price, q.ask_size)
        })
    }

    /// Calculate information advantage: how much more a direct feed participant
    /// knows compared to consolidated feed
    pub fn information_advantage_nanos(&self) -> Nanos {
        self.consolidated_feed_latency.saturating_sub(self.direct_feed_latency)
    }
}

/// VPIN (Volume-synchronized Probability of Informed Trading)
/// Measures order flow toxicity
pub struct VPINCalculator {
    bucket_size: Qty,
    num_buckets: usize,
    buckets: Vec<VPINBucket>,
    current_bucket: VPINBucket,
}

#[derive(Debug, Clone, Default)]
struct VPINBucket {
    buy_volume: Qty,
    sell_volume: Qty,
    total_volume: Qty,
}

impl VPINCalculator {
    pub fn new(bucket_size: Qty, num_buckets: usize) -> Self {
        Self {
            bucket_size,
            num_buckets,
            buckets: Vec::with_capacity(num_buckets),
            current_bucket: VPINBucket::default(),
        }
    }

    /// Add a trade to the VPIN calculation
    pub fn add_trade(&mut self, side: Side, qty: Qty) {
        match side {
            Side::Buy => self.current_bucket.buy_volume += qty,
            Side::Sell => self.current_bucket.sell_volume += qty,
        }
        self.current_bucket.total_volume += qty;

        // Check if bucket is full
        while self.current_bucket.total_volume >= self.bucket_size {
            let overflow = self.current_bucket.total_volume - self.bucket_size;

            // Save completed bucket
            let mut completed = self.current_bucket.clone();
            completed.total_volume = self.bucket_size;

            // Proportionally split the overflow
            if overflow > 0 {
                let ratio = overflow as f64 / (completed.total_volume + overflow) as f64;
                let buy_overflow = (completed.buy_volume as f64 * ratio) as Qty;
                let sell_overflow = (completed.sell_volume as f64 * ratio) as Qty;
                completed.buy_volume -= buy_overflow;
                completed.sell_volume -= sell_overflow;

                self.current_bucket = VPINBucket {
                    buy_volume: buy_overflow,
                    sell_volume: sell_overflow,
                    total_volume: overflow,
                };
            } else {
                self.current_bucket = VPINBucket::default();
            }

            self.buckets.push(completed);
            if self.buckets.len() > self.num_buckets {
                self.buckets.remove(0);
            }
        }
    }

    /// Calculate VPIN value (0.0 to 1.0, higher = more toxic)
    pub fn vpin(&self) -> f64 {
        if self.buckets.is_empty() {
            return 0.0;
        }

        let sum_abs_imbalance: f64 = self
            .buckets
            .iter()
            .map(|b| (b.buy_volume - b.sell_volume).unsigned_abs() as f64)
            .sum();

        let total_volume: f64 = self.buckets.iter().map(|b| b.total_volume as f64).sum();

        if total_volume > 0.0 {
            sum_abs_imbalance / total_volume
        } else {
            0.0
        }
    }
}

/// Kyle's Lambda - price impact estimator
/// Measures how much prices move per unit of order flow
pub struct KyleLambda {
    trade_prices: Vec<f64>,
    signed_volumes: Vec<f64>,
    window: usize,
}

impl KyleLambda {
    pub fn new(window: usize) -> Self {
        Self {
            trade_prices: Vec::new(),
            signed_volumes: Vec::new(),
            window,
        }
    }

    pub fn add_trade(&mut self, price: Price, qty: Qty, side: Side) {
        self.trade_prices.push(price as f64);
        let signed_vol = match side {
            Side::Buy => qty as f64,
            Side::Sell => -(qty as f64),
        };
        self.signed_volumes.push(signed_vol);

        if self.trade_prices.len() > self.window {
            self.trade_prices.remove(0);
            self.signed_volumes.remove(0);
        }
    }

    /// Estimate Kyle's lambda (price impact coefficient)
    pub fn lambda(&self) -> f64 {
        if self.trade_prices.len() < 2 {
            return 0.0;
        }

        // Simple OLS: ΔP = λ * signed_volume + ε
        let n = self.trade_prices.len() - 1;
        let mut sum_xy = 0.0;
        let mut sum_xx = 0.0;

        for i in 0..n {
            let dp = self.trade_prices[i + 1] - self.trade_prices[i];
            let sv = self.signed_volumes[i + 1];
            sum_xy += dp * sv;
            sum_xx += sv * sv;
        }

        if sum_xx > 0.0 {
            sum_xy / sum_xx
        } else {
            0.0
        }
    }
}
