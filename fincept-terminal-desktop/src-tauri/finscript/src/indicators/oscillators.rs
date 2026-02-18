use super::moving_averages::sma;

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
    super::statistics::change(data, period)
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
