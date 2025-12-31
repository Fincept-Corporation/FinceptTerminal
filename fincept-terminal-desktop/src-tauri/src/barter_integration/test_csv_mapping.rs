/// Test CSV data mapping and full backtest workflow
use super::*;
use std::collections::HashMap;

#[tokio::test]
async fn test_csv_to_backtest_full_workflow() {
    println!("\n========================================");
    println!("  CSV Data Mapping & Backtest Test");
    println!("========================================\n");

    // Step 1: Read CSV file
    let csv_path = "C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/src-tauri/resources/scripts/test_volatile_data.csv";
    let csv_content = std::fs::read_to_string(csv_path)
        .expect("Failed to read CSV file");

    println!("✓ Step 1: CSV file loaded ({} bytes)", csv_content.len());

    // Step 2: Parse CSV into rows
    let lines: Vec<&str> = csv_content.lines().collect();
    let headers: Vec<&str> = lines[0].split(',').collect();

    let mut data_rows = Vec::new();
    for line in lines.iter().skip(1) {
        let values: Vec<&str> = line.split(',').collect();
        let mut row_map = HashMap::new();
        for (i, header) in headers.iter().enumerate() {
            row_map.insert(header.to_string(), values[i].to_string());
        }
        data_rows.push(data_mapper::DataRow { fields: row_map });
    }

    println!("✓ Step 2: Parsed {} data rows", data_rows.len());
    println!("   Columns: {}", headers.join(", "));

    // Step 3: Create mapping configuration
    let mapping = data_mapper::DatasetMapping {
        timestamp_column: "timestamp".to_string(),
        timestamp_format: data_mapper::TimestampFormat::Rfc3339,
        open_column: Some("open".to_string()),
        high_column: Some("high".to_string()),
        low_column: Some("low".to_string()),
        close_column: Some("close".to_string()),
        volume_column: Some("volume".to_string()),
        price_column: None,
        size_column: None,
        symbol_column: None,
        aggregation_interval: None,
    };

    println!("✓ Step 3: Column mapping configured");

    // Step 4: Map to candles
    let mapper = data_mapper::DataMapper::new(mapping);
    let candles = mapper.map_ohlcv_data(data_rows, "TEST_STOCK")
        .expect("Failed to map data");

    println!("✓ Step 4: Converted to {} candles", candles.len());
    println!("\n--- Sample Candles ---");
    println!("First: {:?}", candles.first().unwrap());
    println!("Last:  {:?}", candles.last().unwrap());

    // Step 5: Configure backtest
    let config = BacktestConfig {
        start_date: candles.first().unwrap().timestamp,
        end_date: candles.last().unwrap().timestamp,
        initial_capital: rust_decimal::Decimal::new(100000, 0), // $1,000.00
        commission_rate: rust_decimal::Decimal::new(2, 3),      // 0.002 = 0.2%
        strategy: StrategyConfig {
            name: "SMA Crossover".to_string(),
            mode: TradingMode::Backtest,
            exchanges: vec![Exchange::Binance],
            symbols: vec!["TEST_STOCK".to_string()],
            parameters: {
                let mut params = HashMap::new();
                params.insert("fast_period".to_string(), serde_json::json!(3));
                params.insert("slow_period".to_string(), serde_json::json!(7));
                params.insert("position_size".to_string(), serde_json::json!("100"));
                params
            },
        },
    };

    println!("\n✓ Step 5: Backtest configured");
    println!("   Strategy: SMA Crossover (fast=3, slow=7)");
    println!("   Initial Capital: ${}", config.initial_capital);
    println!("   Commission: {}%", config.commission_rate * rust_decimal::Decimal::new(100, 0));

    // Step 6: Run backtest
    let mut strategy = strategy::SmaStrategy::new();
    let mut candle_data = HashMap::new();
    candle_data.insert("TEST_STOCK".to_string(), candles);

    let result = backtest::BacktestRunner::run_backtest(
        config.clone(),
        &mut strategy,
        candle_data.clone(),
    )
    .await
    .expect("Backtest failed");

    println!("\n✓ Step 6: Backtest completed\n");

    // Step 7: Display results
    println!("========================================");
    println!("         BACKTEST RESULTS");
    println!("========================================");
    println!("Total Return:       {}%", result.total_return);
    println!("Sharpe Ratio:       {}", result.sharpe_ratio);
    println!("Sortino Ratio:      {}", result.sortino_ratio);
    println!("Max Drawdown:       {}%", result.max_drawdown * rust_decimal::Decimal::new(100, 0));
    println!("Win Rate:           {}%", result.win_rate);
    println!("----------------------------------------");
    println!("Total Trades:       {}", result.total_trades);
    println!("Winning Trades:     {}", result.winning_trades);
    println!("Losing Trades:      {}", result.losing_trades);
    println!("Average Win:        ${}", result.average_win);
    println!("Average Loss:       ${}", result.average_loss);
    println!("Profit Factor:      {}", result.profit_factor);
    println!("----------------------------------------");
    println!("Initial Capital:    $100000");
    println!("Final Capital:      ${}", result.final_capital);
    println!("Net Profit/Loss:    ${}", result.final_capital - rust_decimal::Decimal::new(100000, 0));
    println!("========================================\n");

    // Step 8: Show equity curve
    println!("--- Equity Curve (Sample) ---");
    let sample_size = 10.min(result.equity_curve.len());
    for i in 0..sample_size {
        let (timestamp, equity) = &result.equity_curve[i];
        println!("{}: ${}", timestamp.format("%H:%M"), equity);
    }
    if result.equity_curve.len() > sample_size {
        println!("... ({} more points)", result.equity_curve.len() - sample_size);
    }

    // Step 9: Show trades
    if !result.trades.is_empty() {
        println!("\n--- Trade History ---");
        for (i, trade) in result.trades.iter().enumerate() {
            println!("Trade #{}: {} {} @ ${} -> ${} | P&L: ${} ({}%)",
                i + 1,
                match trade.side {
                    OrderSide::Buy => "LONG",
                    OrderSide::Sell => "SHORT",
                },
                trade.symbol,
                trade.entry_price,
                trade.exit_price,
                trade.pnl,
                trade.return_pct * rust_decimal::Decimal::new(100, 0)
            );
        }
    } else {
        println!("\n--- No Trades Executed ---");
        println!("Strategy did not generate any signals with current parameters.");
        println!("Try adjusting fast_period/slow_period for more signals.");
    }

    println!("\n========================================");
    println!("  Test Completed Successfully!");
    println!("========================================\n");

    assert!(result.final_capital > rust_decimal::Decimal::ZERO);
    assert_eq!(result.equity_curve.len(), 50); // 50 candles
}
