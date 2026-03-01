use super::Exchange;
use crate::market_sim::types::*;
use crate::market_sim::agents::*;

/// Create a default simulation with pre-configured instruments and agents
pub fn create_default_simulation() -> Exchange {
    let instrument = Instrument {
        id: 1,
        symbol: "SIM-AAPL".to_string(),
        tick_size: 1,        // $0.01
        lot_size: 1,
        min_quantity: 1,
        max_quantity: 1_000_000,
        price_band_pct: 0.20,
        reference_price: 15000,  // $150.00
        maker_fee_bps: -0.2,
        taker_fee_bps: 0.3,
        short_sell_allowed: true,
        tick_size_table: vec![],
    };

    let config = SimulationConfig {
        name: "Default Market Simulation".to_string(),
        instruments: vec![instrument],
        duration_nanos: 23_400_000_000_000,
        tick_interval_nanos: 1_000_000,     // 1ms
        random_seed: 42,
        enable_circuit_breakers: true,
        enable_auctions: true,
        ..Default::default()
    };

    let mut exchange = Exchange::new(config);

    // Register market makers (co-located)
    for i in 0..3 {
        let pid = exchange.register_participant(
            format!("MM-{}", i),
            ParticipantType::MarketMaker,
            10_000_000.0,
            LatencyTier::CoLocated,
        );
        let agent = Box::new(MarketMakerAgent::new(
            format!("MM-{}", i),
            1, // instrument_id
            2, // half_spread in ticks
            500, // quote size
            3,   // levels
            20_000, // max inventory
            1,   // tick size
        ));
        exchange.add_agent(pid, agent);
    }

    // Register HFT agents
    for i in 0..2 {
        let pid = exchange.register_participant(
            format!("HFT-{}", i),
            ParticipantType::HFT,
            5_000_000.0,
            LatencyTier::CoLocated,
        );
        let agent = Box::new(HFTAgent::new(
            format!("HFT-{}", i),
            1, 3, 5000,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register stat arb agents
    for i in 0..2 {
        let pid = exchange.register_participant(
            format!("StatArb-{}", i),
            ParticipantType::StatArb,
            5_000_000.0,
            LatencyTier::ProximityHosted,
        );
        let agent = Box::new(StatArbAgent::new(
            format!("StatArb-{}", i),
            1, 50, 2.0, 10_000, 200,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register momentum agents
    for i in 0..2 {
        let pid = exchange.register_participant(
            format!("Momentum-{}", i),
            ParticipantType::Momentum,
            3_000_000.0,
            LatencyTier::DirectConnect,
        );
        let agent = Box::new(MomentumAgent::new(
            format!("Momentum-{}", i),
            1, 10, 30, 10_000, 300,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register noise traders (retail)
    for i in 0..10 {
        let pid = exchange.register_participant(
            format!("Noise-{}", i),
            ParticipantType::NoiseTrader,
            500_000.0,
            LatencyTier::Retail,
        );
        let agent = Box::new(NoiseTraderAgent::new(
            format!("Noise-{}", i),
            1, 0.005, (10, 200), 2000,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register institutional trader
    {
        let pid = exchange.register_participant(
            "Institution-VWAP".to_string(),
            ParticipantType::Institutional,
            50_000_000.0,
            LatencyTier::DirectConnect,
        );
        let agent = Box::new(InstitutionalAgent::new(
            "Institution-VWAP".to_string(),
            1,
            50_000,
            Side::Buy,
            ExecutionAlgo::VWAP,
            10_000_000_000_000, // execute over full day
        ));
        exchange.add_agent(pid, agent);
    }

    // Register sniper bot
    {
        let pid = exchange.register_participant(
            "Sniper-1".to_string(),
            ParticipantType::SniperBot,
            2_000_000.0,
            LatencyTier::CoLocated,
        );
        let agent = Box::new(SniperAgent::new(
            "Sniper-1".to_string(),
            1, 5000, 100,
        ));
        exchange.add_agent(pid, agent);
    }

    exchange
}
