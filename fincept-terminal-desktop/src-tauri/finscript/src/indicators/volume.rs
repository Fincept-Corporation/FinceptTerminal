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
