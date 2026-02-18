use crate::ast::*;
use crate::types::*;
use crate::indicators;
use super::Interpreter;

impl Interpreter {
    pub(crate) fn eval_indicator_fn(
        &mut self,
        name: &str,
        args: &[Expr],
        compute: impl Fn(&[f64], usize) -> Vec<f64>,
    ) -> Result<Value, String> {
        if args.len() < 2 {
            return Err(format!("{}() requires at least 2 arguments: symbol/series and period", name));
        }

        let period = self.resolve_number(&args[1])? as usize;

        // Check if first arg is a series variable or a symbol
        let first = self.evaluate_expr(&args[0])?;
        match first {
            Value::Series(s) => {
                let values = compute(&s.values, period);
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            Value::String(symbol) => {
                let ohlcv = self.symbol_data.get(&symbol)
                    .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
                let values = compute(&ohlcv.close, period);
                Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
            }
            _ => Err(format!("{}() first argument must be a symbol or series", name)),
        }
    }

    pub(crate) fn eval_atr(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("atr() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::atr(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_macd(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (macd_line, _, _) = self.compute_macd(args)?;
        Ok(macd_line)
    }

    pub(crate) fn eval_macd_signal(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, signal_line, _) = self.compute_macd(args)?;
        Ok(signal_line)
    }

    pub(crate) fn eval_macd_hist(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, _, histogram) = self.compute_macd(args)?;
        Ok(histogram)
    }

    fn compute_macd(&mut self, args: &[Expr]) -> Result<(Value, Value, Value), String> {
        if args.is_empty() {
            return Err("macd() requires at least 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let fast = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 12 };
        let slow = if args.len() > 2 { self.resolve_number(&args[2])? as usize } else { 26 };
        let signal = if args.len() > 3 { self.resolve_number(&args[3])? as usize } else { 9 };

        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (macd_vals, signal_vals, hist_vals) = indicators::macd(&ohlcv.close, fast, slow, signal);
        let ts = ohlcv.timestamps.clone();

        Ok((
            Value::Series(SeriesData { values: macd_vals, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: signal_vals, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: hist_vals, timestamps: ts }),
        ))
    }

    pub(crate) fn eval_stochastic(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("stochastic() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (k_values, _) = indicators::stochastic(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values: k_values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_stochastic_d(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("stochastic_d() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (_, d_values) = indicators::stochastic(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values: d_values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_bollinger(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (upper, _, _) = self.compute_bollinger(args)?;
        Ok(upper)
    }

    pub(crate) fn eval_bollinger_middle(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, middle, _) = self.compute_bollinger(args)?;
        Ok(middle)
    }

    pub(crate) fn eval_bollinger_lower(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, _, lower) = self.compute_bollinger(args)?;
        Ok(lower)
    }

    fn compute_bollinger(&mut self, args: &[Expr]) -> Result<(Value, Value, Value), String> {
        if args.len() < 2 {
            return Err("bollinger() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let std_dev = if args.len() > 2 { self.resolve_number(&args[2])? } else { 2.0 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (upper, middle, lower) = indicators::bollinger(&ohlcv.close, period, std_dev);
        let ts = ohlcv.timestamps.clone();
        Ok((
            Value::Series(SeriesData { values: upper, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: middle, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: lower, timestamps: ts }),
        ))
    }

    pub(crate) fn eval_obv(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("obv() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::obv(&ohlcv.close, &ohlcv.volume);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_price_fn(&mut self, args: &[Expr], field: &str) -> Result<Value, String> {
        if args.is_empty() {
            return Err(format!("{}() requires 1 argument: symbol", field));
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = match field {
            "close" => ohlcv.close.clone(),
            "open" => ohlcv.open.clone(),
            "high" => ohlcv.high.clone(),
            "low" => ohlcv.low.clone(),
            "volume" => ohlcv.volume.clone(),
            _ => return Err(format!("Unknown price field '{}'", field)),
        };
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_hlc_indicator(&mut self, args: &[Expr], compute: impl Fn(&[f64], &[f64], &[f64], usize) -> Vec<f64>) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("Indicator requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = compute(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_mfi(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("mfi() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::mfi(&ohlcv.high, &ohlcv.low, &ohlcv.close, &ohlcv.volume, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_vwap(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("vwap() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::vwap(&ohlcv.high, &ohlcv.low, &ohlcv.close, &ohlcv.volume);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_sar(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("sar() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let af_start = if args.len() > 1 { self.resolve_number(&args[1])? } else { 0.02 };
        let af_step = if args.len() > 2 { self.resolve_number(&args[2])? } else { 0.02 };
        let af_max = if args.len() > 3 { self.resolve_number(&args[3])? } else { 0.2 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::sar(&ohlcv.high, &ohlcv.low, af_start, af_step, af_max);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_supertrend(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("supertrend() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 10 };
        let multiplier = if args.len() > 2 { self.resolve_number(&args[2])? } else { 3.0 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (st, _) = indicators::supertrend(&ohlcv.high, &ohlcv.low, &ohlcv.close, period, multiplier);
        Ok(Value::Series(SeriesData { values: st, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_supertrend_dir(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("supertrend_dir() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 10 };
        let multiplier = if args.len() > 2 { self.resolve_number(&args[2])? } else { 3.0 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (_, dir) = indicators::supertrend(&ohlcv.high, &ohlcv.low, &ohlcv.close, period, multiplier);
        Ok(Value::Series(SeriesData { values: dir, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_williams_r(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("williams_r() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::williams_r(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_change(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("change() requires 1-2 arguments".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        let period = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 1 };
        match val {
            Value::Series(s) => {
                let values = indicators::change(&s.values, period);
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err("change() expects a Series".to_string()),
        }
    }

    pub(crate) fn eval_cum(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("cum() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let values = indicators::cum(&s.values);
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err("cum() expects a Series".to_string()),
        }
    }

    pub(crate) fn eval_true_range(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("tr() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::true_range(&ohlcv.high, &ohlcv.low, &ohlcv.close);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_pivot(&mut self, args: &[Expr], is_high: bool) -> Result<Value, String> {
        if args.is_empty() { return Err("pivot() requires at least 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        let left = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 5 };
        let right = if args.len() > 2 { self.resolve_number(&args[2])? as usize } else { 5 };
        match val {
            Value::Series(s) => {
                let values = if is_high {
                    indicators::pivothigh(&s.values, left, right)
                } else {
                    indicators::pivotlow(&s.values, left, right)
                };
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err("pivot functions expect a Series".to_string()),
        }
    }

    pub(crate) fn eval_bar_index(&mut self, args: &[Expr]) -> Result<Value, String> {
        let symbol = if !args.is_empty() { self.resolve_symbol(&args[0])? } else { self.first_symbol() };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values: Vec<f64> = (0..ohlcv.close.len()).map(|i| i as f64).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_hl2(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("hl2() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values: Vec<f64> = ohlcv.high.iter().zip(ohlcv.low.iter())
            .map(|(h, l)| (h + l) / 2.0).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_hlc3(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("hlc3() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let len = ohlcv.close.len();
        let values: Vec<f64> = (0..len)
            .map(|i| (ohlcv.high[i] + ohlcv.low[i] + ohlcv.close[i]) / 3.0).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_ohlc4(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("ohlc4() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let len = ohlcv.close.len();
        let values: Vec<f64> = (0..len)
            .map(|i| (ohlcv.open[i] + ohlcv.high[i] + ohlcv.low[i] + ohlcv.close[i]) / 4.0).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    pub(crate) fn eval_timestamps(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("timestamps() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values: Vec<f64> = ohlcv.timestamps.iter().map(|t| *t as f64).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }
}
