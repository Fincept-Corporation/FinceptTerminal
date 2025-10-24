// CFTC (Commodity Futures Trading Commission) data commands based on OpenBB cftc provider
use crate::utils::python::execute_python_command;

/// Execute CFTC Python script command
#[tauri::command]
pub async fn execute_cftc_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "cftc_data.py", &cmd_args)
}

// COT DATA COMMANDS

/// Get Commitment of Traders (COT) data
#[tauri::command]
pub async fn get_cftc_cot_data(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

/// Search for available COT markets
#[tauri::command]
pub async fn search_cftc_cot_markets(app: tauri::AppHandle, 
    query: String,
) -> Result<String, String> {
    let args = vec![query];
    execute_cftc_command(app, "search_cot_markets".to_string(), args).await
}

/// Get information about available COT report types
#[tauri::command]
pub async fn get_cftc_available_report_types(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_cftc_command(app, "available_report_types".to_string(), vec![]).await
}

// MARKET SENTIMENT ANALYSIS COMMANDS

/// Analyze market sentiment from COT data
#[tauri::command]
pub async fn analyze_cftc_market_sentiment(app: tauri::AppHandle, 
    identifier: String,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    execute_cftc_command(app, "market_sentiment".to_string(), args).await
}

/// Get summary of current positions for a market
#[tauri::command]
pub async fn get_cftc_position_summary(app: tauri::AppHandle, 
    identifier: String,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec![identifier];
    if let Some(report_type) = report_type {
        args.push(report_type);
    } else {
        args.push("disaggregated".to_string());
    }
    execute_cftc_command(app, "position_summary".to_string(), args).await
}

// COMPREHENSIVE DATA COMMANDS

/// Get comprehensive COT overview for multiple markets
#[tauri::command]
pub async fn get_cftc_comprehensive_cot_overview(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "comprehensive_cot_overview".to_string(), args).await
}

/// Get historical COT trend data for analysis
#[tauri::command]
pub async fn get_cftc_cot_historical_trend(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_historical_trend".to_string(), args).await
}

// LEGACY COT REPORTS COMMANDS

/// Get legacy COT reports (traditional commercial/non-commercial classification)
#[tauri::command]
pub async fn get_cftc_legacy_cot(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

// DISAGGREGATED COT REPORTS COMMANDS

/// Get disaggregated COT reports (detailed trader classifications)
#[tauri::command]
pub async fn get_cftc_disaggregated_cot(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

// TRADERS IN FINANCIAL FUTURES (TFF) COMMANDS

/// Get TFF reports for financial futures
#[tauri::command]
pub async fn get_cftc_tff_reports(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

// SUPPLEMENTAL COT REPORTS COMMANDS

/// Get supplemental COT reports
#[tauri::command]
pub async fn get_cftc_supplemental_cot(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

// MARKET SPECIFIC COMMANDS

/// Get COT data for precious metals (gold, silver, platinum, palladium)
#[tauri::command]
pub async fn get_cftc_precious_metals_cot(app: tauri::AppHandle, 
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["gold,silver,platinum,palladium".to_string()];
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

/// Get COT data for energy markets (crude oil, natural gas, gasoline, heating oil)
#[tauri::command]
pub async fn get_cftc_energy_cot(app: tauri::AppHandle, 
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["crude_oil,natural_gas,gasoline,heating_oil".to_string()];
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

/// Get COT data for agricultural commodities (corn, wheat, soybeans, cotton, etc.)
#[tauri::command]
pub async fn get_cftc_agricultural_cot(app: tauri::AppHandle, 
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["corn,wheat,soybeans,cotton,cocoa,coffee,sugar".to_string()];
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

/// Get COT data for financial futures (currencies, stock indices, interest rates)
#[tauri::command]
pub async fn get_cftc_financial_cot(app: tauri::AppHandle, 
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["euro,jpy,british_pound,s&p_500,nasdaq_100".to_string()];
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

/// Get COT data for cryptocurrency futures (bitcoin, ether)
#[tauri::command]
pub async fn get_cftc_crypto_cot(app: tauri::AppHandle, 
    report_type: Option<String>,
    futures_only: Option<bool>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["bitcoin,ether".to_string()];
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
    execute_cftc_command(app, "cot_data".to_string(), args).await
}

// ADVANCED ANALYSIS COMMANDS

/// Get COT position changes and momentum analysis
#[tauri::command]
pub async fn get_cftc_position_changes(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_historical_trend".to_string(), args).await
}

/// Get COT extreme positions analysis (historical extremes)
#[tauri::command]
pub async fn get_cftc_extreme_positions(app: tauri::AppHandle, 
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
    execute_cftc_command(app, "cot_historical_trend".to_string(), args).await
}