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
