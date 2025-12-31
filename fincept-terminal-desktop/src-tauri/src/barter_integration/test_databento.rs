/// Quick test to parse the actual Databento file and run a backtest
use super::*;
use std::collections::HashMap;

#[tokio::test]
async fn test_parse_real_databento_file() {
    let file_path = "C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/src-tauri/resources/scripts/databento_raw_data.bin";

    let result = data_mapper::parse_databento_file(file_path);

    match result {
        Ok(candle_data) => {
            println!("=== Databento Parsing Results ===");
            for (symbol, candles) in &candle_data {
                println!("Symbol: {}", symbol);
                println!("  Total Candles: {}", candles.len());
                if !candles.is_empty() {
                    println!("  First: {:?}", candles.first().unwrap());
                    println!("  Last: {:?}", candles.last().unwrap());
                }
            }

            // Run backtest on ES (E-mini S&P 500) if available
            if let Some(es_candles) = candle_data.get("ES") {
                if es_candles.len() >= 10 {
                    println!("\n=== Running Backtest on ES (E-mini S&P 500) ===");

                    let config = BacktestConfig {
                        start_date: es_candles.first().unwrap().timestamp,
                        end_date: es_candles.last().unwrap().timestamp,
                        initial_capital: rust_decimal::Decimal::new(100000, 0),
                        commission_rate: rust_decimal::Decimal::new(1, 3),
                        strategy: StrategyConfig {
                            name: "SMA Crossover".to_string(),
                            mode: TradingMode::Backtest,
                            exchanges: vec![Exchange::Binance],
                            symbols: vec!["ES".to_string()],
                            parameters: {
                                let mut params = HashMap::new();
                                params.insert("fast_period".to_string(), serde_json::json!(3));
                                params.insert("slow_period".to_string(), serde_json::json!(5));
                                params.insert("position_size".to_string(), serde_json::json!("1"));
                                params
                            },
                        },
                    };

                    let mut strategy = strategy::SmaStrategy::new();
                    let mut backtest_data = HashMap::new();
                    backtest_data.insert("ES".to_string(), es_candles.clone());

                    let backtest_result = backtest::BacktestRunner::run_backtest(
                        config,
                        &mut strategy,
                        backtest_data,
                    ).await;

                    match backtest_result {
                        Ok(result) => {
                            println!("Total Return: {}%", result.total_return);
                            println!("Sharpe Ratio: {}", result.sharpe_ratio);
                            println!("Max Drawdown: {}%", result.max_drawdown * rust_decimal::Decimal::new(100, 0));
                            println!("Win Rate: {}%", result.win_rate);
                            println!("Total Trades: {}", result.total_trades);
                            println!("Final Capital: ${}", result.final_capital);
                        }
                        Err(e) => {
                            println!("Backtest failed: {}", e);
                        }
                    }
                }
            }

            assert!(!candle_data.is_empty());
        }
        Err(e) => {
            println!("Failed to parse file: {}", e);
            panic!("Parse failed");
        }
    }
}
