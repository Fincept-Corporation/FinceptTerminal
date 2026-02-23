use crate::ast::*;
use crate::types::*;
use super::Interpreter;

impl Interpreter {
    /// sharpe(series, rf_rate?) — Annualized Sharpe ratio
    pub(crate) fn eval_sharpe(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("sharpe() requires 1 argument: price series".to_string());
        }
        let val = self.evaluate_expr(&args[0])?;
        let rf_rate = if args.len() > 1 {
            self.resolve_number(&args[1])?
        } else {
            0.0
        };
        match val {
            Value::Series(s) => {
                let returns = daily_returns(&s.values);
                if returns.is_empty() {
                    return Ok(Value::Na);
                }
                let mean = mean_val(&returns);
                let std = std_val(&returns);
                if std == 0.0 {
                    return Ok(Value::Na);
                }
                let daily_rf = rf_rate / 252.0;
                let sharpe = (mean - daily_rf) / std * (252.0_f64).sqrt();
                Ok(Value::Number(sharpe))
            }
            _ => Err("sharpe() expects a price Series".to_string()),
        }
    }

    /// sortino(series, rf_rate?) — Annualized Sortino ratio (downside deviation only)
    pub(crate) fn eval_sortino(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("sortino() requires 1 argument: price series".to_string());
        }
        let val = self.evaluate_expr(&args[0])?;
        let rf_rate = if args.len() > 1 {
            self.resolve_number(&args[1])?
        } else {
            0.0
        };
        match val {
            Value::Series(s) => {
                let returns = daily_returns(&s.values);
                if returns.is_empty() {
                    return Ok(Value::Na);
                }
                let mean = mean_val(&returns);
                let daily_rf = rf_rate / 252.0;
                let downside: Vec<f64> = returns
                    .iter()
                    .filter(|&&r| r < daily_rf)
                    .map(|&r| (r - daily_rf).powi(2))
                    .collect();
                if downside.is_empty() {
                    return Ok(Value::Na);
                }
                let dd_std =
                    (downside.iter().sum::<f64>() / downside.len() as f64).sqrt();
                if dd_std == 0.0 {
                    return Ok(Value::Na);
                }
                Ok(Value::Number(
                    (mean - daily_rf) / dd_std * (252.0_f64).sqrt(),
                ))
            }
            _ => Err("sortino() expects a price Series".to_string()),
        }
    }

    /// max_drawdown(series) — Maximum drawdown as a positive fraction (0.0 to 1.0)
    pub(crate) fn eval_max_drawdown(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("max_drawdown() requires 1 argument: price series".to_string());
        }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let mut peak = f64::NEG_INFINITY;
                let mut max_dd = 0.0_f64;
                for &price in &s.values {
                    if price.is_nan() {
                        continue;
                    }
                    if price > peak {
                        peak = price;
                    }
                    if peak > 0.0 {
                        let dd = (peak - price) / peak;
                        if dd > max_dd {
                            max_dd = dd;
                        }
                    }
                }
                Ok(Value::Number(max_dd))
            }
            _ => Err("max_drawdown() expects a price Series".to_string()),
        }
    }

    /// correlation(series_a, series_b) — Pearson correlation of daily returns
    pub(crate) fn eval_correlation(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("correlation() requires 2 arguments: two series".to_string());
        }
        let a = self.evaluate_expr(&args[0])?;
        let b = self.evaluate_expr(&args[1])?;
        match (a, b) {
            (Value::Series(sa), Value::Series(sb)) => {
                let ra = daily_returns(&sa.values);
                let rb = daily_returns(&sb.values);
                let len = ra.len().min(rb.len());
                if len < 2 {
                    return Ok(Value::Na);
                }
                let ma = mean_val(&ra[..len]);
                let mb = mean_val(&rb[..len]);
                let mut cov = 0.0;
                let mut va = 0.0;
                let mut vb = 0.0;
                for i in 0..len {
                    let da = ra[i] - ma;
                    let db = rb[i] - mb;
                    cov += da * db;
                    va += da * da;
                    vb += db * db;
                }
                let denom = (va * vb).sqrt();
                if denom == 0.0 {
                    return Ok(Value::Na);
                }
                Ok(Value::Number(cov / denom))
            }
            _ => Err("correlation() expects two Series".to_string()),
        }
    }

    /// beta(asset_series, benchmark_series) — Beta coefficient
    pub(crate) fn eval_beta(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err(
                "beta() requires 2 arguments: asset series and benchmark series".to_string(),
            );
        }
        let asset = self.evaluate_expr(&args[0])?;
        let benchmark = self.evaluate_expr(&args[1])?;
        match (asset, benchmark) {
            (Value::Series(sa), Value::Series(sb)) => {
                let ra = daily_returns(&sa.values);
                let rb = daily_returns(&sb.values);
                let len = ra.len().min(rb.len());
                if len < 2 {
                    return Ok(Value::Na);
                }
                let ma = mean_val(&ra[..len]);
                let mb = mean_val(&rb[..len]);
                let mut cov = 0.0;
                let mut var_b = 0.0;
                for i in 0..len {
                    let da = ra[i] - ma;
                    let db = rb[i] - mb;
                    cov += da * db;
                    var_b += db * db;
                }
                if var_b == 0.0 {
                    return Ok(Value::Na);
                }
                Ok(Value::Number(cov / var_b))
            }
            _ => Err("beta() expects two Series".to_string()),
        }
    }

    /// returns(series) — Convert price series to daily return series
    pub(crate) fn eval_returns(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("returns() requires 1 argument: price series".to_string());
        }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let rets = daily_returns(&s.values);
                let ts = if s.timestamps.len() > 1 {
                    s.timestamps[1..].to_vec()
                } else {
                    vec![]
                };
                Ok(Value::Series(SeriesData {
                    values: rets,
                    timestamps: ts,
                }))
            }
            _ => Err("returns() expects a price Series".to_string()),
        }
    }
}

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

fn daily_returns(prices: &[f64]) -> Vec<f64> {
    if prices.len() < 2 {
        return vec![];
    }
    prices
        .windows(2)
        .map(|w| {
            if w[0] != 0.0 && !w[0].is_nan() && !w[1].is_nan() {
                (w[1] - w[0]) / w[0]
            } else {
                0.0
            }
        })
        .collect()
}

fn mean_val(data: &[f64]) -> f64 {
    if data.is_empty() {
        return 0.0;
    }
    data.iter().sum::<f64>() / data.len() as f64
}

fn std_val(data: &[f64]) -> f64 {
    if data.len() < 2 {
        return 0.0;
    }
    let m = mean_val(data);
    let variance = data.iter().map(|x| (x - m).powi(2)).sum::<f64>() / (data.len() - 1) as f64;
    variance.sqrt()
}
