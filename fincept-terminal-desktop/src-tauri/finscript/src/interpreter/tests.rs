use super::*;
use crate::lexer::tokenize;
use crate::parser::parse;
use crate::types::OhlcvSeries;
use std::collections::HashMap;

/// Generate test OHLCV data (same logic as finscript_cmd.rs)
fn gen_ohlcv(symbol: &str, days: usize) -> OhlcvSeries {
    let seed: u64 = symbol.bytes().fold(0u64, |acc, b| acc.wrapping_mul(31).wrapping_add(b as u64));
    let base_price = 50.0 + (seed % 450) as f64;
    let volatility = 0.015 + (seed % 20) as f64 * 0.001;

    let mut timestamps = Vec::with_capacity(days);
    let mut open = Vec::with_capacity(days);
    let mut high = Vec::with_capacity(days);
    let mut low = Vec::with_capacity(days);
    let mut close = Vec::with_capacity(days);
    let mut volume = Vec::with_capacity(days);

    let start_ts: i64 = 1719792000;
    let mut price = base_price;
    let mut rng_state = seed;

    for i in 0..days {
        rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let r1 = ((rng_state >> 33) as f64) / (u32::MAX as f64) - 0.5;
        rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let r2 = ((rng_state >> 33) as f64) / (u32::MAX as f64);
        rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let r3 = ((rng_state >> 33) as f64) / (u32::MAX as f64);

        let change = price * volatility * r1 * 2.0;
        let day_open = price;
        let day_close = price + change;
        let spread = (change.abs() + price * volatility * r2 * 0.5).max(price * 0.002);
        let day_high = day_open.max(day_close) + spread * r2;
        let day_low = day_open.min(day_close) - spread * r3;
        let base_vol = 1_000_000.0 + (seed % 9_000_000) as f64;
        let day_volume = base_vol * (1.0 + change.abs() / price * 10.0) * (0.5 + r3);

        timestamps.push(start_ts + (i as i64) * 86400);
        open.push(day_open.max(0.01));
        high.push(day_high.max(day_open.max(day_close)));
        low.push(day_low.min(day_open.min(day_close)).max(0.01));
        close.push(day_close.max(0.01));
        volume.push(day_volume.round());
        price = day_close.max(0.01);
    }

    OhlcvSeries { symbol: symbol.to_string(), timestamps, open, high, low, close, volume }
}

fn run_script(code: &str) -> InterpreterResult {
    let tokens = tokenize(code).expect("Lexer failed");
    let program = parse(tokens).expect("Parser failed");
    let symbols = collect_symbols(&program);
    let mut symbol_data = HashMap::new();
    for s in &symbols {
        symbol_data.insert(s.clone(), gen_ohlcv(s, 180));
    }
    let mut interp = Interpreter::new(symbol_data);
    interp.execute(&program)
}

#[test]
fn test_default_strategy() {
    let code = r#"
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_val = rsi(AAPL, 14)

fast = last(ema_fast)
slow = last(ema_slow)
rsi_now = last(rsi_val)

print "EMA(12):", fast
print "EMA(26):", slow
print "RSI(14):", rsi_now

if fast > slow {
    if rsi_now < 70 {
        buy "Bullish crossover"
    }
}

if fast < slow {
    if rsi_now > 30 {
        sell "Bearish crossover"
    }
}

plot_candlestick AAPL, "AAPL Price"
plot_line ema_fast, "EMA (12)", "cyan"
plot_line ema_slow, "EMA (26)", "orange"
plot rsi_val, "RSI (14)"
"#;
    let result = run_script(code);
    println!("Output:\n{}", result.output);
    println!("Signals: {:?}", result.signals);
    println!("Plots: {} charts", result.plots.len());
    for e in &result.errors {
        println!("ERROR: {}", e);
    }
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(!result.output.is_empty(), "Should have print output");
    assert!(result.plots.len() >= 3, "Should have at least 3 plots");
}

#[test]
fn test_print_statement() {
    let result = run_script(r#"
x = 42
print "Hello", x
print "Sum:", 10 + 20
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("Hello"), "Should contain Hello");
    assert!(result.output.contains("42"), "Should contain 42");
    assert!(result.output.contains("30"), "Should contain 30");
}

#[test]
fn test_if_else() {
    let result = run_script(r#"
x = 10
if x > 5 {
    print "greater"
} else {
    print "lesser"
}
"#);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("greater"));
}

#[test]
fn test_for_loop() {
    let result = run_script(r#"
total = 0
for i in 1..6 {
    total = total + i
}
print "total:", total
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("15"), "1+2+3+4+5 = 15");
}

#[test]
fn test_while_loop() {
    let result = run_script(r#"
counter = 0
result = 1
while counter < 10 {
    result = result * 2
    counter = counter + 1
}
print "2^10 =", result
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("1024"));
}

#[test]
fn test_user_function() {
    let result = run_script(r#"
fn add(a, b) {
    return a + b
}
print "result:", add(3, 7)
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("10"));
}

#[test]
fn test_indicators_sma_ema_rsi() {
    let result = run_script(r#"
sma_val = sma(AAPL, 20)
ema_val = ema(AAPL, 12)
rsi_val = rsi(AAPL, 14)
print "SMA:", last(sma_val)
print "EMA:", last(ema_val)
print "RSI:", last(rsi_val)
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("SMA:"));
    assert!(result.output.contains("EMA:"));
    assert!(result.output.contains("RSI:"));
}

#[test]
fn test_macd() {
    let result = run_script(r#"
macd_line = macd(AAPL, 12, 26, 9)
print "MACD:", last(macd_line)
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("MACD:"));
}

#[test]
fn test_bollinger() {
    let result = run_script(r#"
bb = bollinger(AAPL, 20, 2)
print "Bollinger:", last(bb)
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
}

#[test]
fn test_buy_sell_signals() {
    let result = run_script(r#"
buy "test buy signal"
sell "test sell signal"
"#);
    println!("Signals: {:?}", result.signals);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert_eq!(result.signals.len(), 2);
    assert_eq!(result.signals[0].signal_type, "Buy");
    assert_eq!(result.signals[1].signal_type, "Sell");
}

#[test]
fn test_plot_candlestick() {
    let result = run_script(r#"
plot_candlestick AAPL, "Apple"
"#);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert_eq!(result.plots.len(), 1);
    assert_eq!(result.plots[0].plot_type, "candlestick");
    assert!(!result.plots[0].data.is_empty());
}

#[test]
fn test_plot_line() {
    let result = run_script(r#"
data = sma(AAPL, 20)
plot_line data, "SMA 20", "blue"
"#);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert_eq!(result.plots.len(), 1);
    assert_eq!(result.plots[0].plot_type, "line");
}

#[test]
fn test_ternary() {
    let result = run_script(r#"
x = 10
msg = x > 5 ? "big" : "small"
print msg
"#);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("big"));
}

#[test]
fn test_switch() {
    let result = run_script(r#"
val = "a"
switch val {
    "a" {
        print "matched a"
    }
    "b" {
        print "matched b"
    }
    else {
        print "no match"
    }
}
"#);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("matched a"));
}

#[test]
fn test_arrays() {
    let result = run_script(r#"
arr = [10, 20, 30, 40, 50]
print "len:", len(arr)
print "first:", arr[0]
print "last:", arr[4]
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("5"));
    assert!(result.output.contains("10"));
    assert!(result.output.contains("50"));
}

#[test]
fn test_multi_symbol() {
    let result = run_script(r#"
aapl_sma = sma(AAPL, 20)
msft_sma = sma(MSFT, 20)
print "AAPL SMA:", last(aapl_sma)
print "MSFT SMA:", last(msft_sma)
"#);
    println!("Output:\n{}", result.output);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    assert!(result.output.contains("AAPL SMA:"));
    assert!(result.output.contains("MSFT SMA:"));
}

#[test]
fn test_nested_if_buy() {
    let result = run_script(r#"
rsi_val = rsi(AAPL, 14)
rsi_now = last(rsi_val)
if rsi_now > 0 {
    buy "RSI is positive"
}
"#);
    println!("Output:\n{}", result.output);
    println!("Signals: {:?}", result.signals);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
}

#[test]
fn test_hline() {
    let result = run_script(r#"
hline 70, "Overbought", "red"
hline 30, "Oversold", "green"
"#);
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
}

#[test]
fn test_strategy_mode() {
    let result = run_script(r#"
ema_f = ema(AAPL, 12)
ema_s = ema(AAPL, 26)
fast = last(ema_f)
slow = last(ema_s)
if fast > slow {
    strategy.entry("Long", "long")
}
if fast < slow {
    strategy.entry("Short", "short")
}
"#);
    println!("Output:\n{}", result.output);
    for e in &result.errors {
        println!("ERROR: {}", e);
    }
    assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
}
