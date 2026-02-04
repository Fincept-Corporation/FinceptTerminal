// QuantStats Analytics - Portfolio performance analysis via quantstats Python library
use crate::python;

/// Run QuantStats analysis on portfolio holdings
/// Actions: stats, returns, drawdown, rolling, full_report, html_report
#[tauri::command]
pub async fn run_quantstats_analysis(
    app: tauri::AppHandle,
    tickers_json: String,
    action: Option<String>,
    benchmark: Option<String>,
    period: Option<String>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let action = action.unwrap_or_else(|| "full_report".to_string());
    let benchmark = benchmark.unwrap_or_else(|| "SPY".to_string());
    let period = period.unwrap_or_else(|| "1y".to_string());
    let rf = risk_free_rate.unwrap_or(0.02);

    let args = vec![
        action,
        tickers_json,
        benchmark,
        period,
        rf.to_string(),
    ];

    python::execute(&app, "Analytics/quantstats_analytics.py", args).await
}
