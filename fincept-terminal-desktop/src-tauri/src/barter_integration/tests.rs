#[cfg(test)]
mod tests {
    use super::super::*;
    use super::super::strategy::Strategy;
    use chrono::Utc;
    use rust_decimal::Decimal;
    use std::collections::HashMap;

    fn create_demo_candle_data() -> HashMap<String, Vec<Candle>> {
        let mut data = HashMap::new();
        let mut candles = Vec::new();

        // Generate 50 demo candles with upward trend
        let base_time = Utc::now() - chrono::Duration::days(50);
        for i in 0..50 {
            let close = Decimal::new(10000 + (i * 100), 0); // Start at 100.00, increase by 1.00 each candle
            candles.push(Candle {
                timestamp: base_time + chrono::Duration::days(i as i64),
                open: close - Decimal::new(50, 0),
                high: close + Decimal::new(100, 0),
                low: close - Decimal::new(150, 0),
                close,
                volume: Decimal::new(1000000, 0),
            });
        }

        data.insert("BTC-USD".to_string(), candles);
        data
    }

    #[tokio::test]
    async fn test_sma_strategy_initialization() {
        let mut strategy = strategy::SmaStrategy::new();

        let mut params = HashMap::new();
        params.insert("fast_period".to_string(), serde_json::json!(5));
        params.insert("slow_period".to_string(), serde_json::json!(10));
        params.insert("position_size".to_string(), serde_json::json!("1.0"));

        let config = StrategyConfig {
            name: "Test SMA".to_string(),
            mode: TradingMode::Backtest,
            exchanges: vec![Exchange::Binance],
            symbols: vec!["BTC-USD".to_string()],
            parameters: params,
        };

        let result = strategy.initialize(&config);
        assert!(result.is_ok());
        assert_eq!(strategy.name(), "SMA Crossover");
    }

    #[tokio::test]
    async fn test_backtest_engine() {
        let config = BacktestConfig {
            start_date: Utc::now() - chrono::Duration::days(50),
            end_date: Utc::now(),
            initial_capital: Decimal::new(100000, 0), // $1000.00
            commission_rate: Decimal::new(1, 3),      // 0.001 = 0.1%
            strategy: StrategyConfig {
                name: "SMA Crossover".to_string(),
                mode: TradingMode::Backtest,
                exchanges: vec![Exchange::Binance],
                symbols: vec!["BTC-USD".to_string()],
                parameters: {
                    let mut params = HashMap::new();
                    params.insert("fast_period".to_string(), serde_json::json!(5));
                    params.insert("slow_period".to_string(), serde_json::json!(10));
                    params.insert("position_size".to_string(), serde_json::json!("1.0"));
                    params
                },
            },
        };

        let candle_data = create_demo_candle_data();
        let mut strategy = strategy::SmaStrategy::new();

        let result = backtest::BacktestRunner::run_backtest(
            config,
            &mut strategy,
            candle_data,
        )
        .await;

        assert!(result.is_ok());
        let backtest_result = result.unwrap();

        println!("=== Backtest Results ===");
        println!("Total Return: {}%", backtest_result.total_return);
        println!("Sharpe Ratio: {}", backtest_result.sharpe_ratio);
        println!("Max Drawdown: {}%", backtest_result.max_drawdown * Decimal::new(100, 0));
        println!("Win Rate: {}%", backtest_result.win_rate);
        println!("Total Trades: {}", backtest_result.total_trades);
        println!("Final Capital: ${}", backtest_result.final_capital);

        // Basic assertions
        assert!(backtest_result.final_capital > Decimal::ZERO);
        assert!(backtest_result.equity_curve.len() > 0);
    }

    #[tokio::test]
    async fn test_execution_manager() {
        let manager = execution::ExecutionManager::new(TradingMode::Paper);

        let request = OrderRequest {
            exchange: Exchange::Binance,
            symbol: "BTC-USD".to_string(),
            side: OrderSide::Buy,
            order_type: OrderType::Market,
            quantity: Decimal::new(1, 1), // 0.1
            price: None,
            stop_price: None,
            time_in_force: None,
        };

        let result = manager.submit_order(request).await;
        assert!(result.is_ok());

        let order = result.unwrap();
        println!("Order submitted: {:?}", order.order_id);
        assert_eq!(order.status, OrderStatus::Pending);

        // Wait for simulated fill
        tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;

        let filled_order = manager.get_order(&order.order_id);
        assert!(filled_order.is_ok());
        println!("Order status: {:?}", filled_order.unwrap().status);
    }

    #[tokio::test]
    async fn test_market_data_manager() {
        let manager = market_data::MarketDataManager::new();

        let config = MarketStreamConfig {
            exchange: Exchange::Binance,
            symbols: vec!["BTC-USD".to_string(), "ETH-USD".to_string()],
            subscribe_trades: true,
            subscribe_orderbook: false,
            subscribe_candles: false,
            candle_interval: None,
        };

        let result = manager.start_stream(config).await;
        assert!(result.is_ok());

        let stream_id = result.unwrap();
        println!("Market stream started: {}", stream_id);

        let active_streams = manager.get_active_streams();
        assert!(active_streams.contains(&stream_id));

        // Stop stream
        let stop_result = manager.stop_stream(&stream_id).await;
        assert!(stop_result.is_ok());
    }

    #[tokio::test]
    async fn test_strategy_registry() {
        let registry = strategy::StrategyRegistry::default();

        let strategies = registry.list_strategies();
        assert!(strategies.contains(&"SMA Crossover".to_string()));

        let strategy = registry.get("SMA Crossover");
        assert!(strategy.is_some());
        assert_eq!(strategy.unwrap().name(), "SMA Crossover");
    }
}
