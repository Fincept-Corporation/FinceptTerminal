use super::moving_averages::{ema, rma};

/// MACD - Returns (macd_line, signal_line, histogram)
pub fn macd(
    data: &[f64],
    fast_period: usize,
    slow_period: usize,
    signal_period: usize,
) -> (Vec<f64>, Vec<f64>, Vec<f64>) {
    let len = data.len();
    if len == 0 {
        return (vec![], vec![], vec![]);
    }

    let fast_ema = ema(data, fast_period);
    let slow_ema = ema(data, slow_period);

    // MACD line = fast EMA - slow EMA
    let mut macd_line = vec![f64::NAN; len];
    for i in 0..len {
        if !fast_ema[i].is_nan() && !slow_ema[i].is_nan() {
            macd_line[i] = fast_ema[i] - slow_ema[i];
        }
    }

    // Signal line = EMA of MACD line (ignoring NaN values)
    let valid_macd: Vec<f64> = macd_line.iter().filter(|v| !v.is_nan()).copied().collect();
    let signal_ema = ema(&valid_macd, signal_period);

    let mut signal_line = vec![f64::NAN; len];
    let mut valid_idx = 0;
    for i in 0..len {
        if !macd_line[i].is_nan() {
            if valid_idx < signal_ema.len() {
                signal_line[i] = signal_ema[valid_idx];
            }
            valid_idx += 1;
        }
    }

    // Histogram = MACD - Signal
    let mut histogram = vec![f64::NAN; len];
    for i in 0..len {
        if !macd_line[i].is_nan() && !signal_line[i].is_nan() {
            histogram[i] = macd_line[i] - signal_line[i];
        }
    }

    (macd_line, signal_line, histogram)
}

/// Average True Range
pub fn atr(high: &[f64], low: &[f64], close: &[f64], period: usize) -> Vec<f64> {
    let len = close.len();
    if len < 2 || period == 0 {
        return vec![f64::NAN; len];
    }

    // Calculate True Range
    let mut tr = vec![0.0; len];
    tr[0] = high[0] - low[0];
    for i in 1..len {
        let hl = high[i] - low[i];
        let hc = (high[i] - close[i - 1]).abs();
        let lc = (low[i] - close[i - 1]).abs();
        tr[i] = hl.max(hc).max(lc);
    }

    // ATR = EMA of True Range (using Wilder's smoothing = period-based EMA)
    if len < period {
        return vec![f64::NAN; len];
    }

    let mut result = vec![f64::NAN; len];
    let initial_atr: f64 = tr[..period].iter().sum::<f64>() / period as f64;
    result[period - 1] = initial_atr;

    for i in period..len {
        result[i] = (result[i - 1] * (period as f64 - 1.0) + tr[i]) / period as f64;
    }

    result
}

/// ADX (Average Directional Index)
pub fn adx(high: &[f64], low: &[f64], close: &[f64], period: usize) -> Vec<f64> {
    let len = close.len().min(high.len()).min(low.len());
    if len < period * 2 || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut plus_dm = vec![0.0; len];
    let mut minus_dm = vec![0.0; len];
    let mut tr_vals = vec![0.0; len];
    for i in 1..len {
        let up = high[i] - high[i - 1];
        let down = low[i - 1] - low[i];
        plus_dm[i] = if up > down && up > 0.0 { up } else { 0.0 };
        minus_dm[i] = if down > up && down > 0.0 { down } else { 0.0 };
        let hl = high[i] - low[i];
        let hc = (high[i] - close[i - 1]).abs();
        let lc = (low[i] - close[i - 1]).abs();
        tr_vals[i] = hl.max(hc).max(lc);
    }
    let smoothed_tr = rma(&tr_vals[1..], period);
    let smoothed_plus = rma(&plus_dm[1..], period);
    let smoothed_minus = rma(&minus_dm[1..], period);
    let slen = smoothed_tr.len();
    let mut dx = vec![f64::NAN; slen];
    for i in 0..slen {
        if smoothed_tr[i].is_nan() || smoothed_tr[i] == 0.0 { continue; }
        let plus_di = 100.0 * smoothed_plus[i] / smoothed_tr[i];
        let minus_di = 100.0 * smoothed_minus[i] / smoothed_tr[i];
        let sum = plus_di + minus_di;
        if sum > 0.0 {
            dx[i] = 100.0 * (plus_di - minus_di).abs() / sum;
        }
    }
    let valid_dx: Vec<f64> = dx.iter().filter(|v| !v.is_nan()).copied().collect();
    let adx_vals = rma(&valid_dx, period);
    let mut result = vec![f64::NAN; len];
    let mut vi = 0;
    let offset = 1; // because we sliced [1..]
    for i in 0..slen {
        if !dx[i].is_nan() {
            if vi < adx_vals.len() && !adx_vals[vi].is_nan() {
                result[i + offset] = adx_vals[vi];
            }
            vi += 1;
        }
    }
    result
}

/// Bollinger Bands - Returns (upper, middle, lower)
pub fn bollinger(data: &[f64], period: usize, std_dev: f64) -> (Vec<f64>, Vec<f64>, Vec<f64>) {
    let len = data.len();
    if len < period || period == 0 {
        return (
            vec![f64::NAN; len],
            vec![f64::NAN; len],
            vec![f64::NAN; len],
        );
    }

    let middle = super::moving_averages::sma(data, period);
    let mut upper = vec![f64::NAN; len];
    let mut lower = vec![f64::NAN; len];

    for i in (period - 1)..len {
        let window = &data[i + 1 - period..=i];
        let mean = middle[i];
        if mean.is_nan() {
            continue;
        }
        let variance: f64 =
            window.iter().map(|x| (x - mean).powi(2)).sum::<f64>() / period as f64;
        let std = variance.sqrt();
        upper[i] = mean + std_dev * std;
        lower[i] = mean - std_dev * std;
    }

    (upper, middle, lower)
}

/// Parabolic SAR
pub fn sar(high: &[f64], low: &[f64], af_start: f64, af_step: f64, af_max: f64) -> Vec<f64> {
    let len = high.len().min(low.len());
    if len < 2 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    let mut is_long = true;
    let mut sar_val = low[0];
    let mut ep = high[0];
    let mut af = af_start;
    result[0] = sar_val;
    for i in 1..len {
        let prev_sar = sar_val;
        sar_val = prev_sar + af * (ep - prev_sar);
        if is_long {
            sar_val = sar_val.min(low[i - 1]);
            if i >= 2 { sar_val = sar_val.min(low[i - 2]); }
            if high[i] > ep {
                ep = high[i];
                af = (af + af_step).min(af_max);
            }
            if low[i] < sar_val {
                is_long = false;
                sar_val = ep;
                ep = low[i];
                af = af_start;
            }
        } else {
            sar_val = sar_val.max(high[i - 1]);
            if i >= 2 { sar_val = sar_val.max(high[i - 2]); }
            if low[i] < ep {
                ep = low[i];
                af = (af + af_step).min(af_max);
            }
            if high[i] > sar_val {
                is_long = true;
                sar_val = ep;
                ep = high[i];
                af = af_start;
            }
        }
        result[i] = sar_val;
    }
    result
}

/// SuperTrend - returns (supertrend_line, direction: 1=up, -1=down)
pub fn supertrend(high: &[f64], low: &[f64], close: &[f64], period: usize, multiplier: f64) -> (Vec<f64>, Vec<f64>) {
    let len = close.len().min(high.len()).min(low.len());
    if len < period {
        return (vec![f64::NAN; len], vec![f64::NAN; len]);
    }
    let atr_vals = atr(high, low, close, period);
    let mut upper_band = vec![f64::NAN; len];
    let mut lower_band = vec![f64::NAN; len];
    let mut st = vec![f64::NAN; len];
    let mut direction = vec![1.0_f64; len];
    for i in 0..len {
        if atr_vals[i].is_nan() { continue; }
        let hl2 = (high[i] + low[i]) / 2.0;
        upper_band[i] = hl2 + multiplier * atr_vals[i];
        lower_band[i] = hl2 - multiplier * atr_vals[i];
    }
    // Initialize
    let start = period - 1;
    if start < len {
        st[start] = upper_band[start];
        direction[start] = -1.0;
    }
    for i in (start + 1)..len {
        if upper_band[i].is_nan() || lower_band[i].is_nan() { continue; }
        if !upper_band[i - 1].is_nan() && upper_band[i] > upper_band[i - 1] {
            // keep
        }
        if !lower_band[i - 1].is_nan() && lower_band[i] < lower_band[i - 1] {
            // keep
        }
        if direction[i - 1] == 1.0 {
            if close[i] < lower_band[i] {
                direction[i] = -1.0;
                st[i] = upper_band[i];
            } else {
                direction[i] = 1.0;
                st[i] = lower_band[i];
            }
        } else {
            if close[i] > upper_band[i] {
                direction[i] = 1.0;
                st[i] = lower_band[i];
            } else {
                direction[i] = -1.0;
                st[i] = upper_band[i];
            }
        }
    }
    (st, direction)
}
