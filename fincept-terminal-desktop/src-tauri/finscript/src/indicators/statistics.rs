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
