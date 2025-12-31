/// Risk Management Module Tests
use super::*;
use rust_decimal::Decimal;

#[test]
fn test_fixed_percentage_position_sizing() {
    let config = risk_management::RiskConfig {
        position_sizing: risk_management::PositionSizingMethod::FixedPercentage(
            Decimal::new(5, 2) // 5%
        ),
        ..Default::default()
    };

    let manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));
    let capital = Decimal::new(100000, 0);
    let entry_price = Decimal::new(50, 0);

    let size = manager.calculate_position_size(capital, entry_price, None).unwrap();

    // 5% of $100,000 = $5,000 / $50 = 100 shares
    assert_eq!(size, Decimal::new(100, 0));
    println!("✓ Fixed percentage sizing: {} shares", size);
}

#[test]
fn test_risk_based_position_sizing() {
    let config = risk_management::RiskConfig {
        position_sizing: risk_management::PositionSizingMethod::RiskBased {
            risk_per_trade: Decimal::new(1, 2) // 1% risk per trade
        },
        ..Default::default()
    };

    let manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));
    let capital = Decimal::new(100000, 0);
    let entry_price = Decimal::new(100, 0);
    let stop_loss = Decimal::new(95, 0); // $5 risk per share

    let size = manager.calculate_position_size(capital, entry_price, Some(stop_loss)).unwrap();

    // 1% of $100,000 = $1,000 risk / $5 per share = 200 shares
    // But capped by max_position_size (10% = $10,000 / $100 = 100 shares)
    assert_eq!(size, Decimal::new(100, 0));
    println!("✓ Risk-based sizing: {} shares (capped by max position size)", size);
}

#[test]
fn test_kelly_criterion_position_sizing() {
    let config = risk_management::RiskConfig {
        position_sizing: risk_management::PositionSizingMethod::Kelly {
            win_rate: Decimal::new(60, 2), // 60%
            avg_win: Decimal::new(150, 0),
            avg_loss: Decimal::new(100, 0),
        },
        ..Default::default()
    };

    let manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));
    let capital = Decimal::new(100000, 0);
    let entry_price = Decimal::new(50, 0);

    let size = manager.calculate_position_size(capital, entry_price, None).unwrap();

    // Kelly = (0.6 * 1.5 - 0.4) / 1.5 = 0.333 = 33.3%
    // Capped at 25% = $25,000 / $50 = 500 shares
    println!("✓ Kelly criterion sizing: {} shares", size);
    assert!(size > Decimal::ZERO);
}

#[test]
fn test_fixed_percentage_stop_loss() {
    let config = risk_management::RiskConfig {
        stop_loss: risk_management::StopLossType::FixedPercentage(Decimal::new(2, 2)), // 2%
        ..Default::default()
    };

    let manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));

    // Long position
    let entry_price = Decimal::new(100, 0);
    let stop = manager.calculate_stop_loss(OrderSide::Buy, entry_price, None).unwrap();
    assert_eq!(stop, Decimal::new(98, 0)); // 100 * 0.98
    println!("✓ Long stop-loss: ${}", stop);

    // Short position
    let stop_short = manager.calculate_stop_loss(OrderSide::Sell, entry_price, None).unwrap();
    assert_eq!(stop_short, Decimal::new(102, 0)); // 100 * 1.02
    println!("✓ Short stop-loss: ${}", stop_short);
}

#[test]
fn test_risk_reward_take_profit() {
    let config = risk_management::RiskConfig {
        take_profit: risk_management::TakeProfitType::RiskRewardRatio(Decimal::new(3, 0)), // 3:1
        ..Default::default()
    };

    let manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));

    let entry_price = Decimal::new(100, 0);
    let stop_loss = Decimal::new(95, 0); // $5 risk

    let tp = manager.calculate_take_profit(OrderSide::Buy, entry_price, Some(stop_loss)).unwrap();

    // Risk = $5, Reward = $15 (3:1), TP = $115
    assert_eq!(tp, Decimal::new(115, 0));
    println!("✓ Take-profit at ${} (3:1 R:R)", tp);
}

#[test]
fn test_trailing_stop_update() {
    let config = risk_management::RiskConfig {
        stop_loss: risk_management::StopLossType::Trailing {
            percentage: Decimal::new(5, 2) // 5%
        },
        ..Default::default()
    };

    let mut manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));

    // Register long position
    manager.register_position(
        "AAPL".to_string(),
        OrderSide::Buy,
        Decimal::new(100, 0),
        Decimal::new(100, 0),
        None,
    );

    // Price moves up to $110
    let new_stop = manager.update_trailing_stop("AAPL", Decimal::new(110, 0)).unwrap();
    assert_eq!(new_stop, Decimal::new(1045, 1)); // 110 * 0.95 = 104.5
    println!("✓ Trailing stop updated to ${}", new_stop);

    // Price moves up to $120
    let new_stop2 = manager.update_trailing_stop("AAPL", Decimal::new(120, 0)).unwrap();
    assert_eq!(new_stop2, Decimal::new(114, 0)); // 120 * 0.95 = 114
    println!("✓ Trailing stop updated to ${}", new_stop2);

    // Price drops to $115 - stop should NOT move down
    let unchanged = manager.update_trailing_stop("AAPL", Decimal::new(115, 0));
    assert!(unchanged.is_none());
    println!("✓ Trailing stop stays at $114 (doesn't move down)");
}

#[test]
fn test_exit_signals() {
    let config = risk_management::RiskConfig {
        stop_loss: risk_management::StopLossType::FixedPercentage(Decimal::new(5, 2)),
        take_profit: risk_management::TakeProfitType::FixedPercentage(Decimal::new(10, 2)),
        ..Default::default()
    };

    let mut manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));

    manager.register_position(
        "AAPL".to_string(),
        OrderSide::Buy,
        Decimal::new(100, 0),
        Decimal::new(100, 0),
        None,
    );

    // Price hits stop-loss at $95
    let signal = manager.check_exit_signals("AAPL", Decimal::new(95, 0));
    assert_eq!(signal, Some(risk_management::ExitSignal::StopLoss));
    println!("✓ Stop-loss triggered at $95");

    // Price hits take-profit at $110
    let signal_tp = manager.check_exit_signals("AAPL", Decimal::new(110, 0));
    assert_eq!(signal_tp, Some(risk_management::ExitSignal::TakeProfit));
    println!("✓ Take-profit triggered at $110");

    // Price at $105 - no signal
    let no_signal = manager.check_exit_signals("AAPL", Decimal::new(105, 0));
    assert_eq!(no_signal, None);
    println!("✓ No exit signal at $105");
}

#[test]
fn test_max_drawdown_limit() {
    let config = risk_management::RiskConfig {
        limits: risk_management::RiskLimits {
            max_drawdown: Some(Decimal::new(10, 2)), // 10%
            ..Default::default()
        },
        ..Default::default()
    };

    let mut manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));

    // Equity drops to $89,000 (11% drawdown)
    let breaches = manager.check_risk_limits(Decimal::new(89000, 0));
    assert_eq!(breaches.len(), 1);

    if let risk_management::RiskLimitBreach::MaxDrawdown { current, limit } = &breaches[0] {
        println!("✓ Max drawdown breached: {:.2}% (limit: {:.2}%)",
            current * Decimal::new(100, 0),
            limit * Decimal::new(100, 0)
        );
    }

    // Equity recovers to $95,000 (5% drawdown) - no breach
    let no_breach = manager.check_risk_limits(Decimal::new(95000, 0));
    assert_eq!(no_breach.len(), 0);
    println!("✓ No breach at 5% drawdown");
}

#[test]
fn test_max_positions_limit() {
    let config = risk_management::RiskConfig {
        limits: risk_management::RiskLimits {
            max_positions: Some(3),
            ..Default::default()
        },
        ..Default::default()
    };

    let mut manager = risk_management::RiskManager::new(config, Decimal::new(100000, 0));

    // Add 3 positions
    manager.register_position("AAPL".into(), OrderSide::Buy, Decimal::new(100, 0), Decimal::new(100, 0), None);
    manager.register_position("GOOGL".into(), OrderSide::Buy, Decimal::new(200, 0), Decimal::new(50, 0), None);
    manager.register_position("MSFT".into(), OrderSide::Buy, Decimal::new(150, 0), Decimal::new(75, 0), None);

    assert_eq!(manager.active_position_count(), 3);

    let breaches = manager.check_risk_limits(Decimal::new(100000, 0));
    assert_eq!(breaches.len(), 1);

    if let risk_management::RiskLimitBreach::MaxPositions { current, limit } = &breaches[0] {
        println!("✓ Max positions reached: {} (limit: {})", current, limit);
    }

    // Remove one position
    manager.remove_position("AAPL");
    assert_eq!(manager.active_position_count(), 2);

    let no_breach = manager.check_risk_limits(Decimal::new(100000, 0));
    assert_eq!(no_breach.len(), 0);
    println!("✓ Positions under limit: 2/3");
}

#[test]
fn test_full_risk_management_workflow() {
    println!("\n=== Full Risk Management Test ===\n");

    let config = risk_management::RiskConfig {
        position_sizing: risk_management::PositionSizingMethod::RiskBased {
            risk_per_trade: Decimal::new(1, 2) // 1%
        },
        stop_loss: risk_management::StopLossType::FixedPercentage(Decimal::new(2, 2)), // 2%
        take_profit: risk_management::TakeProfitType::RiskRewardRatio(Decimal::new(3, 0)), // 3:1
        limits: risk_management::RiskLimits {
            max_drawdown: Some(Decimal::new(15, 2)),
            max_positions: Some(5),
            ..Default::default()
        },
    };

    let mut manager = risk_management::RiskManager::new(config.clone(), Decimal::new(100000, 0));
    let entry_price = Decimal::new(100, 0);

    // Calculate stop-loss
    let stop_loss = manager.calculate_stop_loss(OrderSide::Buy, entry_price, None).unwrap();
    println!("Entry: ${}, Stop-Loss: ${}", entry_price, stop_loss);

    // Calculate position size
    let size = manager.calculate_position_size(
        Decimal::new(100000, 0),
        entry_price,
        Some(stop_loss)
    ).unwrap();
    println!("Position size: {} shares (risking 1% = $1,000)", size);

    // Calculate take-profit
    let take_profit = manager.calculate_take_profit(OrderSide::Buy, entry_price, Some(stop_loss)).unwrap();
    println!("Take-Profit: ${} (3:1 R:R)\n", take_profit);

    // Register position
    manager.register_position(
        "AAPL".to_string(),
        OrderSide::Buy,
        entry_price,
        size,
        None,
    );

    // Check limits
    let breaches = manager.check_risk_limits(Decimal::new(100000, 0));
    println!("Risk limit breaches: {}", breaches.len());

    // Simulate price movement
    println!("\n--- Price Movement Simulation ---");

    // Price moves to $105
    let signal = manager.check_exit_signals("AAPL", Decimal::new(105, 0));
    println!("Price $105: {:?}", signal);

    // Price moves to $106 (hit take-profit)
    let signal_tp = manager.check_exit_signals("AAPL", Decimal::new(106, 0));
    println!("Price $106: {:?}", signal_tp);
    assert_eq!(signal_tp, Some(risk_management::ExitSignal::TakeProfit));

    println!("\n✓ Full workflow completed successfully");
}
