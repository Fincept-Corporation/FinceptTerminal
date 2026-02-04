use crate::market_sim::types::*;

/// Deterministic pseudo-random number generator (xorshift64)
/// Used for reproducible latency jitter
pub struct Rng {
    state: u64,
}

impl Rng {
    pub fn new(seed: u64) -> Self {
        Self { state: if seed == 0 { 1 } else { seed } }
    }

    pub fn next_u64(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.state = x;
        x
    }

    /// Random value in [0, max)
    pub fn next_bounded(&mut self, max: u64) -> u64 {
        if max == 0 {
            return 0;
        }
        self.next_u64() % max
    }

    /// Random f64 in [0.0, 1.0)
    pub fn next_f64(&mut self) -> f64 {
        (self.next_u64() >> 11) as f64 / (1u64 << 53) as f64
    }

    /// Normal distribution using Box-Muller transform
    pub fn next_normal(&mut self, mean: f64, std_dev: f64) -> f64 {
        let u1 = self.next_f64().max(1e-10); // avoid log(0)
        let u2 = self.next_f64();
        let z = (-2.0 * u1.ln()).sqrt() * (2.0 * std::f64::consts::PI * u2).cos();
        mean + z * std_dev
    }
}

/// Latency simulator that models realistic network delays
pub struct LatencySimulator {
    rng: Rng,
    /// Per-tier latency configuration
    tier_configs: Vec<(LatencyTier, LatencyConfig)>,
}

#[derive(Debug, Clone)]
pub struct LatencyConfig {
    pub base_nanos: u64,
    pub jitter_nanos: u64,
    /// Probability of a latency spike (0.0 to 1.0)
    pub spike_probability: f64,
    /// Spike multiplier (e.g., 10x base latency)
    pub spike_multiplier: f64,
}

impl LatencySimulator {
    pub fn new(seed: u64) -> Self {
        let tier_configs = vec![
            (
                LatencyTier::CoLocated,
                LatencyConfig {
                    base_nanos: 1_000,        // 1 microsecond
                    jitter_nanos: 500,
                    spike_probability: 0.001,  // 0.1% chance of spike
                    spike_multiplier: 5.0,
                },
            ),
            (
                LatencyTier::ProximityHosted,
                LatencyConfig {
                    base_nanos: 50_000,       // 50 microseconds
                    jitter_nanos: 20_000,
                    spike_probability: 0.005,
                    spike_multiplier: 8.0,
                },
            ),
            (
                LatencyTier::DirectConnect,
                LatencyConfig {
                    base_nanos: 1_000_000,    // 1 millisecond
                    jitter_nanos: 500_000,
                    spike_probability: 0.01,
                    spike_multiplier: 10.0,
                },
            ),
            (
                LatencyTier::Retail,
                LatencyConfig {
                    base_nanos: 50_000_000,   // 50 milliseconds
                    jitter_nanos: 30_000_000,
                    spike_probability: 0.02,
                    spike_multiplier: 5.0,
                },
            ),
        ];

        Self {
            rng: Rng::new(seed),
            tier_configs,
        }
    }

    /// Calculate the delivery time for a message given the sender's latency tier
    pub fn calculate_latency(&mut self, tier: LatencyTier) -> Nanos {
        let config = self.tier_configs
            .iter()
            .find(|(t, _)| *t == tier)
            .map(|(_, c)| c.clone())
            .unwrap_or(LatencyConfig {
                base_nanos: 1_000_000,
                jitter_nanos: 500_000,
                spike_probability: 0.01,
                spike_multiplier: 5.0,
            });

        let jitter = self.rng.next_bounded(config.jitter_nanos);
        let mut latency = config.base_nanos + jitter;

        // Check for spike
        if self.rng.next_f64() < config.spike_probability {
            latency = (latency as f64 * config.spike_multiplier) as u64;
        }

        latency
    }

    /// Calculate when a participant would receive a market data update
    pub fn data_delivery_time(
        &mut self,
        event_time: Nanos,
        participant_tier: LatencyTier,
    ) -> Nanos {
        event_time + self.calculate_latency(participant_tier)
    }

    /// Calculate when an order from a participant would arrive at the exchange
    pub fn order_arrival_time(
        &mut self,
        submit_time: Nanos,
        participant_tier: LatencyTier,
    ) -> Nanos {
        submit_time + self.calculate_latency(participant_tier)
    }

    /// Simulate race condition: given two participants submitting at the same logical time,
    /// determine who arrives first
    pub fn race(
        &mut self,
        time: Nanos,
        tier_a: LatencyTier,
        tier_b: LatencyTier,
    ) -> (Nanos, Nanos) {
        let arrival_a = self.order_arrival_time(time, tier_a);
        let arrival_b = self.order_arrival_time(time, tier_b);
        (arrival_a, arrival_b)
    }

    /// Get the RNG for agent use
    pub fn rng(&mut self) -> &mut Rng {
        &mut self.rng
    }
}

/// Price dynamics generator
/// Generates realistic tick-by-tick price movements
pub struct PriceGenerator {
    rng: Rng,
    /// Current mid price
    mid_price: f64,
    /// Volatility (annualized)
    volatility: f64,
    /// Mean reversion speed
    mean_reversion: f64,
    /// Long-term mean price
    long_term_mean: f64,
    /// Jump intensity (Poisson rate)
    jump_intensity: f64,
    /// Jump size mean
    jump_mean: f64,
    /// Jump size std
    jump_std: f64,
    /// Heston model: variance of variance
    vol_of_vol: f64,
    /// Current instantaneous variance (for Heston)
    current_variance: f64,
}

impl PriceGenerator {
    /// Simple GBM (Geometric Brownian Motion)
    pub fn gbm(seed: u64, initial_price: f64, volatility: f64) -> Self {
        Self {
            rng: Rng::new(seed),
            mid_price: initial_price,
            volatility,
            mean_reversion: 0.0,
            long_term_mean: initial_price,
            jump_intensity: 0.0,
            jump_mean: 0.0,
            jump_std: 0.0,
            vol_of_vol: 0.0,
            current_variance: volatility * volatility,
        }
    }

    /// Jump-diffusion model (Merton)
    pub fn jump_diffusion(
        seed: u64,
        initial_price: f64,
        volatility: f64,
        jump_intensity: f64,
        jump_mean: f64,
        jump_std: f64,
    ) -> Self {
        Self {
            rng: Rng::new(seed),
            mid_price: initial_price,
            volatility,
            mean_reversion: 0.0,
            long_term_mean: initial_price,
            jump_intensity,
            jump_mean,
            jump_std,
            vol_of_vol: 0.0,
            current_variance: volatility * volatility,
        }
    }

    /// Heston stochastic volatility model
    pub fn heston(
        seed: u64,
        initial_price: f64,
        volatility: f64,
        mean_reversion: f64,
        vol_of_vol: f64,
    ) -> Self {
        Self {
            rng: Rng::new(seed),
            mid_price: initial_price,
            volatility,
            mean_reversion,
            long_term_mean: volatility * volatility,
            jump_intensity: 0.0,
            jump_mean: 0.0,
            jump_std: 0.0,
            vol_of_vol,
            current_variance: volatility * volatility,
        }
    }

    /// Generate next price tick
    /// dt is in fractions of a year (e.g., 1 microsecond ≈ 3.17e-14 years)
    pub fn next_price(&mut self, dt: f64) -> f64 {
        let sqrt_dt = dt.sqrt();

        // Update variance (Heston)
        if self.vol_of_vol > 0.0 {
            let z_v = self.rng.next_normal(0.0, 1.0);
            let dv = self.mean_reversion * (self.long_term_mean - self.current_variance) * dt
                + self.vol_of_vol * self.current_variance.sqrt() * sqrt_dt * z_v;
            self.current_variance = (self.current_variance + dv).max(0.0001);
        }

        // GBM component
        let z = self.rng.next_normal(0.0, 1.0);
        let sigma = self.current_variance.sqrt();
        let drift = -0.5 * sigma * sigma * dt;
        let diffusion = sigma * sqrt_dt * z;
        let mut log_return = drift + diffusion;

        // Jump component (Merton)
        if self.jump_intensity > 0.0 {
            let jump_prob = self.jump_intensity * dt;
            if self.rng.next_f64() < jump_prob {
                let jump = self.rng.next_normal(self.jump_mean, self.jump_std);
                log_return += jump;
            }
        }

        self.mid_price *= (log_return).exp();
        self.mid_price
    }

    pub fn current_price(&self) -> f64 {
        self.mid_price
    }

    pub fn current_volatility(&self) -> f64 {
        self.current_variance.sqrt()
    }

    /// Apply a news shock
    pub fn apply_shock(&mut self, magnitude: f64) {
        self.mid_price *= 1.0 + magnitude;
    }
}
