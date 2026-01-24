/// Pure Rust implementations of technical indicators.
/// All functions operate on price/volume slices and return computed series.

/// Simple Moving Average
pub fn sma(data: &[f64], period: usize) -> Vec<f64> {
    if data.is_empty() || period == 0 || period > data.len() {
        return vec![f64::NAN; data.len()];
    }

    let mut result = vec![f64::NAN; data.len()];
    let mut sum: f64 = data[..period].iter().sum();
    result[period - 1] = sum / period as f64;

    for i in period..data.len() {
        sum += data[i] - data[i - period];
        result[i] = sum / period as f64;
    }

    result
}

/// Exponential Moving Average
pub fn ema(data: &[f64], period: usize) -> Vec<f64> {
    if data.is_empty() || period == 0 || period > data.len() {
        return vec![f64::NAN; data.len()];
    }

    let mut result = vec![f64::NAN; data.len()];
    let multiplier = 2.0 / (period as f64 + 1.0);

    // First EMA value is the SMA of the first `period` values
    let initial_sma: f64 = data[..period].iter().sum::<f64>() / period as f64;
    result[period - 1] = initial_sma;

    for i in period..data.len() {
        let prev = result[i - 1];
        result[i] = (data[i] - prev) * multiplier + prev;
    }

    result
}

/// Weighted Moving Average
pub fn wma(data: &[f64], period: usize) -> Vec<f64> {
    if data.is_empty() || period == 0 || period > data.len() {
        return vec![f64::NAN; data.len()];
    }

    let mut result = vec![f64::NAN; data.len()];
    let weight_sum: f64 = (1..=period).map(|i| i as f64).sum();

    for i in (period - 1)..data.len() {
        let mut weighted_sum = 0.0;
        for j in 0..period {
            weighted_sum += data[i - period + 1 + j] * (j + 1) as f64;
        }
        result[i] = weighted_sum / weight_sum;
    }

    result
}

/// Relative Strength Index (Wilder's smoothing)
pub fn rsi(data: &[f64], period: usize) -> Vec<f64> {
    if data.len() < period + 1 || period == 0 {
        return vec![f64::NAN; data.len()];
    }

    let mut result = vec![f64::NAN; data.len()];
    let mut gains = Vec::with_capacity(data.len());
    let mut losses = Vec::with_capacity(data.len());

    gains.push(0.0);
    losses.push(0.0);

    for i in 1..data.len() {
        let change = data[i] - data[i - 1];
        if change > 0.0 {
            gains.push(change);
            losses.push(0.0);
        } else {
            gains.push(0.0);
            losses.push(-change);
        }
    }

    // Initial average gain/loss (SMA of first `period` values)
    let mut avg_gain: f64 = gains[1..=period].iter().sum::<f64>() / period as f64;
    let mut avg_loss: f64 = losses[1..=period].iter().sum::<f64>() / period as f64;

    if avg_loss == 0.0 {
        result[period] = 100.0;
    } else {
        let rs = avg_gain / avg_loss;
        result[period] = 100.0 - 100.0 / (1.0 + rs);
    }

    // Subsequent values use Wilder's smoothing
    for i in (period + 1)..data.len() {
        avg_gain = (avg_gain * (period as f64 - 1.0) + gains[i]) / period as f64;
        avg_loss = (avg_loss * (period as f64 - 1.0) + losses[i]) / period as f64;

        if avg_loss == 0.0 {
            result[i] = 100.0;
        } else {
            let rs = avg_gain / avg_loss;
            result[i] = 100.0 - 100.0 / (1.0 + rs);
        }
    }

    result
}

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

/// Stochastic Oscillator - Returns (%K, %D)
pub fn stochastic(
    high: &[f64],
    low: &[f64],
    close: &[f64],
    period: usize,
) -> (Vec<f64>, Vec<f64>) {
    let len = close.len();
    if len < period || period == 0 {
        return (vec![f64::NAN; len], vec![f64::NAN; len]);
    }

    let mut k_values = vec![f64::NAN; len];

    for i in (period - 1)..len {
        let window_high = high[i + 1 - period..=i]
            .iter()
            .cloned()
            .fold(f64::NEG_INFINITY, f64::max);
        let window_low = low[i + 1 - period..=i]
            .iter()
            .cloned()
            .fold(f64::INFINITY, f64::min);

        let range = window_high - window_low;
        if range > 0.0 {
            k_values[i] = ((close[i] - window_low) / range) * 100.0;
        } else {
            k_values[i] = 50.0; // midpoint if no range
        }
    }

    // %D is the 3-period SMA of %K
    let valid_k: Vec<f64> = k_values.iter().filter(|v| !v.is_nan()).copied().collect();
    let d_sma = sma(&valid_k, 3);

    let mut d_values = vec![f64::NAN; len];
    let mut valid_idx = 0;
    for i in 0..len {
        if !k_values[i].is_nan() {
            if valid_idx < d_sma.len() {
                d_values[i] = d_sma[valid_idx];
            }
            valid_idx += 1;
        }
    }

    (k_values, d_values)
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

    let middle = sma(data, period);
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

/// On-Balance Volume
pub fn obv(close: &[f64], volume: &[f64]) -> Vec<f64> {
    let len = close.len().min(volume.len());
    if len == 0 {
        return vec![];
    }

    let mut result = vec![0.0; len];
    result[0] = volume[0];

    for i in 1..len {
        if close[i] > close[i - 1] {
            result[i] = result[i - 1] + volume[i];
        } else if close[i] < close[i - 1] {
            result[i] = result[i - 1] - volume[i];
        } else {
            result[i] = result[i - 1];
        }
    }

    result
}

/// RMA (Running Moving Average / Wilder's smoothing)
pub fn rma(data: &[f64], period: usize) -> Vec<f64> {
    if data.is_empty() || period == 0 || period > data.len() {
        return vec![f64::NAN; data.len()];
    }
    let mut result = vec![f64::NAN; data.len()];
    let initial: f64 = data[..period].iter().sum::<f64>() / period as f64;
    result[period - 1] = initial;
    let alpha = 1.0 / period as f64;
    for i in period..data.len() {
        result[i] = alpha * data[i] + (1.0 - alpha) * result[i - 1];
    }
    result
}

/// Hull Moving Average
pub fn hma(data: &[f64], period: usize) -> Vec<f64> {
    if data.is_empty() || period < 2 || period > data.len() {
        return vec![f64::NAN; data.len()];
    }
    let half = period / 2;
    let sqrt_period = (period as f64).sqrt() as usize;
    let wma_half = wma(data, half);
    let wma_full = wma(data, period);
    let len = data.len();
    let mut diff = vec![f64::NAN; len];
    for i in 0..len {
        if !wma_half[i].is_nan() && !wma_full[i].is_nan() {
            diff[i] = 2.0 * wma_half[i] - wma_full[i];
        }
    }
    let valid: Vec<f64> = diff.iter().filter(|v| !v.is_nan()).copied().collect();
    let hma_vals = wma(&valid, sqrt_period);
    let mut result = vec![f64::NAN; len];
    let mut vi = 0;
    for i in 0..len {
        if !diff[i].is_nan() {
            if vi < hma_vals.len() {
                result[i] = hma_vals[vi];
            }
            vi += 1;
        }
    }
    result
}

/// VWAP (Volume Weighted Average Price)
pub fn vwap(high: &[f64], low: &[f64], close: &[f64], volume: &[f64]) -> Vec<f64> {
    let len = close.len().min(volume.len()).min(high.len()).min(low.len());
    if len == 0 {
        return vec![];
    }
    let mut result = vec![f64::NAN; len];
    let mut cum_vol = 0.0;
    let mut cum_tp_vol = 0.0;
    for i in 0..len {
        let tp = (high[i] + low[i] + close[i]) / 3.0;
        cum_vol += volume[i];
        cum_tp_vol += tp * volume[i];
        if cum_vol > 0.0 {
            result[i] = cum_tp_vol / cum_vol;
        }
    }
    result
}

/// CCI (Commodity Channel Index)
pub fn cci(high: &[f64], low: &[f64], close: &[f64], period: usize) -> Vec<f64> {
    let len = close.len().min(high.len()).min(low.len());
    if len < period || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut tp = vec![0.0; len];
    for i in 0..len {
        tp[i] = (high[i] + low[i] + close[i]) / 3.0;
    }
    let tp_sma = sma(&tp, period);
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let mean = tp_sma[i];
        if mean.is_nan() { continue; }
        let mean_dev: f64 = tp[i + 1 - period..=i].iter()
            .map(|x| (x - mean).abs())
            .sum::<f64>() / period as f64;
        if mean_dev > 0.0 {
            result[i] = (tp[i] - mean) / (0.015 * mean_dev);
        } else {
            result[i] = 0.0;
        }
    }
    result
}

/// MFI (Money Flow Index)
pub fn mfi(high: &[f64], low: &[f64], close: &[f64], volume: &[f64], period: usize) -> Vec<f64> {
    let len = close.len().min(high.len()).min(low.len()).min(volume.len());
    if len < period + 1 || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut tp = vec![0.0; len];
    for i in 0..len {
        tp[i] = (high[i] + low[i] + close[i]) / 3.0;
    }
    let mut result = vec![f64::NAN; len];
    for i in period..len {
        let mut pos_flow = 0.0;
        let mut neg_flow = 0.0;
        for j in (i + 1 - period)..=i {
            let mf = tp[j] * volume[j];
            if j > 0 && tp[j] > tp[j - 1] {
                pos_flow += mf;
            } else {
                neg_flow += mf;
            }
        }
        if neg_flow > 0.0 {
            let mfr = pos_flow / neg_flow;
            result[i] = 100.0 - 100.0 / (1.0 + mfr);
        } else {
            result[i] = 100.0;
        }
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

/// Highest value over N bars
pub fn highest(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    if len == 0 || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let max_val = data[i + 1 - period..=i].iter()
            .filter(|v| !v.is_nan())
            .copied()
            .fold(f64::NEG_INFINITY, f64::max);
        result[i] = if max_val == f64::NEG_INFINITY { f64::NAN } else { max_val };
    }
    result
}

/// Lowest value over N bars
pub fn lowest(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    if len == 0 || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let min_val = data[i + 1 - period..=i].iter()
            .filter(|v| !v.is_nan())
            .copied()
            .fold(f64::INFINITY, f64::min);
        result[i] = if min_val == f64::INFINITY { f64::NAN } else { min_val };
    }
    result
}

/// Change over N bars (default 1)
pub fn change(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    let mut result = vec![f64::NAN; len];
    for i in period..len {
        if !data[i].is_nan() && !data[i - period].is_nan() {
            result[i] = data[i] - data[i - period];
        }
    }
    result
}

/// Cumulative sum
pub fn cum(data: &[f64]) -> Vec<f64> {
    let mut result = Vec::with_capacity(data.len());
    let mut total = 0.0;
    for &v in data {
        if !v.is_nan() {
            total += v;
        }
        result.push(total);
    }
    result
}

/// Rate of Change
pub fn roc(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    let mut result = vec![f64::NAN; len];
    for i in period..len {
        if data[i - period] != 0.0 && !data[i].is_nan() && !data[i - period].is_nan() {
            result[i] = ((data[i] - data[i - period]) / data[i - period]) * 100.0;
        }
    }
    result
}

/// Momentum
pub fn momentum(data: &[f64], period: usize) -> Vec<f64> {
    change(data, period)
}

/// Standard Deviation over N bars
pub fn stdev(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    if len < period || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let window = &data[i + 1 - period..=i];
        let mean: f64 = window.iter().sum::<f64>() / period as f64;
        let variance: f64 = window.iter().map(|x| (x - mean).powi(2)).sum::<f64>() / period as f64;
        result[i] = variance.sqrt();
    }
    result
}

/// Linear Regression
pub fn linreg(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    if len < period || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let window = &data[i + 1 - period..=i];
        let n = period as f64;
        let sum_x: f64 = (0..period).map(|x| x as f64).sum();
        let sum_y: f64 = window.iter().sum();
        let sum_xy: f64 = window.iter().enumerate().map(|(x, y)| x as f64 * y).sum();
        let sum_x2: f64 = (0..period).map(|x| (x as f64).powi(2)).sum();
        let denom = n * sum_x2 - sum_x * sum_x;
        if denom != 0.0 {
            let slope = (n * sum_xy - sum_x * sum_y) / denom;
            let intercept = (sum_y - slope * sum_x) / n;
            result[i] = intercept + slope * (n - 1.0);
        }
    }
    result
}

/// Williams %R
pub fn williams_r(high: &[f64], low: &[f64], close: &[f64], period: usize) -> Vec<f64> {
    let len = close.len().min(high.len()).min(low.len());
    if len < period || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let hh = high[i + 1 - period..=i].iter().copied().fold(f64::NEG_INFINITY, f64::max);
        let ll = low[i + 1 - period..=i].iter().copied().fold(f64::INFINITY, f64::min);
        let range = hh - ll;
        if range > 0.0 {
            result[i] = ((hh - close[i]) / range) * -100.0;
        } else {
            result[i] = -50.0;
        }
    }
    result
}

/// True Range
pub fn true_range(high: &[f64], low: &[f64], close: &[f64]) -> Vec<f64> {
    let len = close.len().min(high.len()).min(low.len());
    if len == 0 {
        return vec![];
    }
    let mut result = vec![0.0; len];
    result[0] = high[0] - low[0];
    for i in 1..len {
        let hl = high[i] - low[i];
        let hc = (high[i] - close[i - 1]).abs();
        let lc = (low[i] - close[i - 1]).abs();
        result[i] = hl.max(hc).max(lc);
    }
    result
}

/// Percent Rank
pub fn percentrank(data: &[f64], period: usize) -> Vec<f64> {
    let len = data.len();
    if len < period || period == 0 {
        return vec![f64::NAN; len];
    }
    let mut result = vec![f64::NAN; len];
    for i in (period - 1)..len {
        let current = data[i];
        if current.is_nan() { continue; }
        let count = data[i + 1 - period..i].iter()
            .filter(|v| !v.is_nan() && **v <= current)
            .count();
        result[i] = (count as f64 / (period - 1) as f64) * 100.0;
    }
    result
}

/// Pivot High detection
pub fn pivothigh(data: &[f64], left_bars: usize, right_bars: usize) -> Vec<f64> {
    let len = data.len();
    let mut result = vec![f64::NAN; len];
    if len < left_bars + right_bars + 1 {
        return result;
    }
    for i in left_bars..(len - right_bars) {
        let pivot = data[i];
        if pivot.is_nan() { continue; }
        let is_pivot = data[i - left_bars..i].iter().all(|v| !v.is_nan() && *v <= pivot)
            && data[i + 1..=i + right_bars].iter().all(|v| !v.is_nan() && *v <= pivot);
        if is_pivot {
            result[i + right_bars] = pivot;
        }
    }
    result
}

/// Pivot Low detection
pub fn pivotlow(data: &[f64], left_bars: usize, right_bars: usize) -> Vec<f64> {
    let len = data.len();
    let mut result = vec![f64::NAN; len];
    if len < left_bars + right_bars + 1 {
        return result;
    }
    for i in left_bars..(len - right_bars) {
        let pivot = data[i];
        if pivot.is_nan() { continue; }
        let is_pivot = data[i - left_bars..i].iter().all(|v| !v.is_nan() && *v >= pivot)
            && data[i + 1..=i + right_bars].iter().all(|v| !v.is_nan() && *v >= pivot);
        if is_pivot {
            result[i + right_bars] = pivot;
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sma_basic() {
        let data = vec![1.0, 2.0, 3.0, 4.0, 5.0];
        let result = sma(&data, 3);
        assert!(result[0].is_nan());
        assert!(result[1].is_nan());
        assert!((result[2] - 2.0).abs() < 1e-10);
        assert!((result[3] - 3.0).abs() < 1e-10);
        assert!((result[4] - 4.0).abs() < 1e-10);
    }

    #[test]
    fn test_ema_basic() {
        let data = vec![10.0, 11.0, 12.0, 13.0, 14.0, 15.0];
        let result = ema(&data, 3);
        assert!(result[0].is_nan());
        assert!(result[1].is_nan());
        assert!((result[2] - 11.0).abs() < 1e-10); // SMA of first 3
    }

    #[test]
    fn test_rsi_basic() {
        let data = vec![44.0, 44.25, 44.5, 43.75, 44.5, 44.25, 43.75, 44.0, 43.5, 44.25, 44.5, 44.0, 43.5, 43.75, 44.25];
        let result = rsi(&data, 14);
        assert!(result[13].is_nan());
        assert!(!result[14].is_nan());
    }

    #[test]
    fn test_obv_basic() {
        let close = vec![10.0, 11.0, 10.5, 11.5, 11.0];
        let volume = vec![100.0, 200.0, 150.0, 300.0, 250.0];
        let result = obv(&close, &volume);
        assert_eq!(result[0], 100.0);
        assert_eq!(result[1], 300.0); // up: +200
        assert_eq!(result[2], 150.0); // down: -150
        assert_eq!(result[3], 450.0); // up: +300
        assert_eq!(result[4], 200.0); // down: -250
    }
}
