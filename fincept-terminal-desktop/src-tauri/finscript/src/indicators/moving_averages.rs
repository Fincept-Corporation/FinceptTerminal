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
