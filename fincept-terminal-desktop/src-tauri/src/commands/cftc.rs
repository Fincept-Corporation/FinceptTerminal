// CFTC (Commodity Futures Trading Commission) data commands based on OpenBB cftc provider
use std::process::Command;

/// Execute CFTC Python script command
#[tauri::command]
pub async fn execute_cftc_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("cftc_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "CFTC script not found at: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    // Execute Python script
    let output = Command::new("python")
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute CFTC command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("CFTC command failed: {}", stderr))
    }
}

// COT DATA COMMANDS

/// Get Commitment of Traders (COT) data
#[tauri::command]
pub async fn get_cftc_cot_data(
    identifier: String,
    report_type: Option<String>,
    futures_only: Option<bool>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("legacy".to_string());
    }
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("1000".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

/// Search for available COT markets
#[tauri::command]
pub async fn search_cftc_cot_markets(
    query: String,
) -> Result<String, String> {
    let args = vec![query];
    execute_cftc_command("search_cot_markets".to_string(), args).await
}

/// Get information about available COT report types
#[tauri::command]
pub async fn get_cftc_available_report_types() -> Result<String, String> {
    execute_cftc_command("available_report_types".to_string(), vec![]).await
}

// MARKET SENTIMENT ANALYSIS COMMANDS

/// Analyze market sentiment from COT data
#[tauri::command]
pub async fn analyze_cftc_market_sentiment(
    identifier: String,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    execute_cftc_command("market_sentiment".to_string(), args).await
}

/// Get summary of current positions for a market
#[tauri::command]
pub async fn get_cftc_position_summary(
    identifier: String,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    execute_cftc_command("position_summary".to_string(), args).await
}

// COMPREHENSIVE DATA COMMANDS

/// Get comprehensive COT overview for multiple markets
#[tauri::command]
pub async fn get_cftc_comprehensive_cot_overview(
    identifiers: Option<String>,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(identifiers) = identifiers {
        args.push(identifiers);
    }
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    execute_cftc_command("comprehensive_cot_overview".to_string(), args).await
}

/// Get historical COT trend data for analysis
#[tauri::command]
pub async fn get_cftc_cot_historical_trend(
    identifier: String,
    report_type: Option<String>,
    period: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    if let Some(period) = period {
        args.push(period.to_string());
    } else {
        args.push("52".to_string());
    }
    execute_cftc_command("cot_historical_trend".to_string(), args).await
}

// LEGACY COT REPORTS COMMANDS

/// Get legacy COT reports (traditional commercial/non-commercial classification)
#[tauri::command]
pub async fn get_cftc_legacy_cot(
    identifier: String,
    futures_only: Option<bool>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    args.push("legacy".to_string());
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("500".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

// DISAGGREGATED COT REPORTS COMMANDS

/// Get disaggregated COT reports (detailed trader classifications)
#[tauri::command]
pub async fn get_cftc_disaggregated_cot(
    identifier: String,
    futures_only: Option<bool>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    args.push("disaggregated".to_string());
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("500".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

// TRADERS IN FINANCIAL FUTURES (TFF) COMMANDS

/// Get TFF reports for financial futures
#[tauri::command]
pub async fn get_cftc_tff_reports(
    identifier: String,
    futures_only: Option<bool>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    args.push("financial".to_string());
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("200".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

// SUPPLEMENTAL COT REPORTS COMMANDS

/// Get supplemental COT reports
#[tauri::command]
pub async fn get_cftc_supplemental_cot(
    identifier: String,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    args.push("supplemental".to_string());
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

// MARKET SPECIFIC COMMANDS

/// Get COT data for precious metals (gold, silver, platinum, palladium)
#[tauri::command]
pub async fn get_cftc_precious_metals_cot(
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["gold,silver,platinum,palladium"];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("200".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

/// Get COT data for energy markets (crude oil, natural gas, gasoline, heating oil)
#[tauri::command]
pub async fn get_cftc_energy_cot(
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["crude_oil,natural_gas,gasoline,heating_oil"];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("200".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

/// Get COT data for agricultural commodities (corn, wheat, soybeans, cotton, etc.)
#[tauri::command]
pub async fn get_cftc_agricultural_cot(
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["corn,wheat,soybeans,cotton,cocoa,coffee,sugar"];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("300".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

/// Get COT data for financial futures (currencies, stock indices, interest rates)
#[tauri::command]
pub async fn get_cftc_financial_cot(
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["euro,jpy,british_pound,s&p_500,nasdaq_100"];
    args.push("financial".to_string());
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("300".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

/// Get COT data for cryptocurrency futures (bitcoin, ether)
#[tauri::command]
pub async fn get_cftc_crypto_cot(
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["bitcoin,ether"];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    if let Some(futures_only) = futures_only {
        args.push(futures_only.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_cftc_command("cot_data".to_string(), args).await
}

// ADVANCED ANALYSIS COMMANDS

/// Get COT position changes and momentum analysis
#[tauri::command]
pub async fn get_cftc_position_changes(
    identifier: String,
    report_type: Option<String>,
    period: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    if let Some(period) = period {
        args.push(period.to_string());
    } else {
        args.push("12".to_string());
    }
    execute_cftc_command("cot_historical_trend".to_string(), args).await
}

/// Get COT extreme positions analysis (historical extremes)
#[tauri::command]
pub async fn get_cftc_extreme_positions(
    identifier: String,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    // Use historical trend for extremes analysis
    args.push("104".to_string()); // 2 years of data
    execute_cftc_command("cot_historical_trend".to_string(), args).await
}