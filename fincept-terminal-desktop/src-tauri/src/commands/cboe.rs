// CBOE (Chicago Board Options Exchange) data commands based on OpenBB cboe provider
use std::process::Command;

/// Execute CBOE Python script command
#[tauri::command]
pub async fn execute_cboe_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("cboe_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "CBOE script not found at: {}",
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
        .map_err(|e| format!("Failed to execute CBOE command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("CBOE command failed: {}", stderr))
    }
}

// EQUITY DATA COMMANDS

/// Get real-time equity quote with implied volatility data
#[tauri::command]
pub async fn get_cboe_equity_quote(
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_cboe_command("equity_quote".to_string(), args).await
}

/// Get historical equity price data
#[tauri::command]
pub async fn get_cboe_equity_historical(
    symbol: String,
    interval: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(interval) = interval {
        args.push(interval);
    } else {
        args.push("1d".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_cboe_command("equity_historical".to_string(), args).await
}

/// Search for equities in CBOE directory
#[tauri::command]
pub async fn search_cboe_equities(
    query: String,
    is_symbol: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(is_symbol) = is_symbol {
        args.push(is_symbol.to_string());
    }
    execute_cboe_command("equity_search".to_string(), args).await
}

// INDEX DATA COMMANDS

/// Get constituents for European indices
#[tauri::command]
pub async fn get_cboe_index_constituents(
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_cboe_command("index_constituents".to_string(), args).await
}

/// Get historical index data
#[tauri::command]
pub async fn get_cboe_index_historical(
    symbol: String,
    interval: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(interval) = interval {
        args.push(interval);
    } else {
        args.push("1d".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_cboe_command("index_historical".to_string(), args).await
}

/// Search for indices in CBOE directory
#[tauri::command]
pub async fn search_cboe_indices(
    query: String,
    is_symbol: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(is_symbol) = is_symbol {
        args.push(is_symbol.to_string());
    }
    execute_cboe_command("index_search".to_string(), args).await
}

/// Get snapshots for all indices in a region
#[tauri::command]
pub async fn get_cboe_index_snapshots(
    region: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(region) = region {
        args.push(region);
    } else {
        args.push("us".to_string());
    }
    execute_cboe_command("index_snapshots".to_string(), args).await
}

/// Get list of all available indices
#[tauri::command]
pub async fn get_cboe_available_indices() -> Result<String, String> {
    execute_cboe_command("available_indices".to_string(), vec![]).await
}

// FUTURES DATA COMMANDS

/// Get VIX futures curve data
#[tauri::command]
pub async fn get_cboe_futures_curve(
    symbol: Option<String>,
    date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    } else {
        args.push("VX_EOD".to_string());
    }
    if let Some(date) = date {
        args.push(date);
    }
    execute_cboe_command("futures_curve".to_string(), args).await
}

// OPTIONS DATA COMMANDS

/// Get options chains data for a symbol
#[tauri::command]
pub async fn get_cboe_options_chains(
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_cboe_command("options_chains".to_string(), args).await
}

// COMPREHENSIVE DATA COMMANDS

/// Get comprehensive market overview for US indices
#[tauri::command]
pub async fn get_cboe_us_market_overview() -> Result<String, String> {
    get_cboe_index_snapshots(Some("us".to_string())).await
}

/// Get comprehensive market overview for European indices
#[tauri::command]
pub async fn get_cboe_european_market_overview() -> Result<String, String> {
    get_cboe_index_snapshots(Some("eu".to_string())).await
}

/// Get VIX analysis including current levels and futures curve
#[tauri::command]
pub async fn get_cboe_vix_analysis() -> Result<String, String> {
    let mut results = Vec::new();

    // Get VIX quote
    match get_cboe_equity_quote("VIX".to_string()).await {
        Ok(quote_data) => results.push(("quote".to_string(), quote_data)),
        Err(e) => results.push(("quote".to_string(), format!("Error: {}", e))),
    }

    // Get VIX futures curve
    match get_cboe_futures_curve(Some("VX_EOD".to_string()), None).await {
        Ok(futures_data) => results.push(("futures".to_string(), futures_data)),
        Err(e) => results.push(("futures".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "vix_analysis": results
    }).to_string())
}

/// Get comprehensive equity analysis with quote and historical data
#[tauri::command]
pub async fn get_cboe_equity_analysis(
    symbol: String,
    days_back: Option<i32>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get equity quote
    match get_cboe_equity_quote(symbol.clone()).await {
        Ok(quote_data) => results.push(("quote".to_string(), quote_data)),
        Err(e) => results.push(("quote".to_string(), format!("Error: {}", e))),
    }

    // Get historical data (default 30 days)
    let days = days_back.unwrap_or(30);
    let start_date = (chrono::Utc::now() - chrono::Duration::days(days as i64))
        .format("%Y-%m-%d")
        .to_string();

    match get_cboe_equity_historical(symbol.clone(), Some("1d".to_string()), Some(start_date), None).await {
        Ok(historical_data) => results.push(("historical".to_string(), historical_data)),
        Err(e) => results.push(("historical".to_string(), format!("Error: {}", e))),
    }

    // Try to get options chains if available
    match get_cboe_options_chains(symbol.clone()).await {
        Ok(options_data) => results.push(("options".to_string(), options_data)),
        Err(_) => results.push(("options".to_string(), "Options data not available".to_string())),
    }

    Ok(serde_json::json!({
        "equity_analysis": results,
        "symbol": symbol
    }).to_string())
}

/// Get comprehensive index analysis with historical data and constituents
#[tauri::command]
pub async fn get_cboe_index_analysis(
    symbol: String,
    days_back: Option<i32>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get historical data (default 30 days)
    let days = days_back.unwrap_or(30);
    let start_date = (chrono::Utc::now() - chrono::Duration::days(days as i64))
        .format("%Y-%m-%d")
        .to_string();

    match get_cboe_index_historical(symbol.clone(), Some("1d".to_string()), Some(start_date), None).await {
        Ok(historical_data) => results.push(("historical".to_string(), historical_data)),
        Err(e) => results.push(("historical".to_string(), format!("Error: {}", e))),
    }

    // Try to get constituents if it's a European index
    let eu_indices = ["BUK100P", "BEP50P", "BDE40P", "BFR40P", "BIT40P"];
    if eu_indices.contains(&symbol.as_str()) {
        match get_cboe_index_constituents(symbol.clone()).await {
            Ok(constituents_data) => results.push(("constituents".to_string(), constituents_data)),
            Err(e) => results.push(("constituents".to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "index_analysis": results,
        "symbol": symbol
    }).to_string())
}

/// Get CBOE market sentiment analysis using VIX and index data
#[tauri::command]
pub async fn get_cboe_market_sentiment() -> Result<String, String> {
    let mut results = Vec::new();

    // Get VIX data for fear index
    match get_cboe_equity_quote("VIX".to_string()).await {
        Ok(vix_data) => results.push(("vix".to_string(), vix_data)),
        Err(e) => results.push(("vix".to_string(), format!("Error: {}", e))),
    }

    // Get major US indices
    let indices = ["SPX", "NDX", "RUT"];
    for index in &indices {
        match get_cboe_equity_quote(index.to_string()).await {
            Ok(index_data) => results.push((index.to_string(), index_data)),
            Err(e) => results.push((index.to_string(), format!("Error: {}", e))),
        }
    }

    // Get US market snapshots
    match get_cboe_index_snapshots(Some("us".to_string())).await {
        Ok(snapshots_data) => results.push(("market_snapshots".to_string(), snapshots_data)),
        Err(e) => results.push(("market_snapshots".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "market_sentiment": results
    }).to_string())
}

/// Search for symbols across both equities and indices
#[tauri::command]
pub async fn search_cboe_symbols(
    query: String,
    search_type: Option<String>, // "equities", "indices", or "all"
) -> Result<String, String> {
    let search_mode = search_type.unwrap_or_else(|| "all".to_string());
    let mut results = Vec::new();

    if search_mode == "equities" || search_mode == "all" {
        match search_cboe_equities(query.clone(), Some(false)).await {
            Ok(equity_results) => results.push(("equities".to_string(), equity_results)),
            Err(e) => results.push(("equities".to_string(), format!("Error: {}", e))),
        }
    }

    if search_mode == "indices" || search_mode == "all" {
        match search_cboe_indices(query.clone(), Some(false)).await {
            Ok(index_results) => results.push(("indices".to_string(), index_results)),
            Err(e) => results.push(("indices".to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "symbol_search": results,
        "query": query,
        "search_type": search_mode
    }).to_string())
}

/// Get CBOE directory information (companies and indices available)
#[tauri::command]
pub async fn get_cboe_directory_info() -> Result<String, String> {
    let mut results = Vec::new();

    // Get available indices
    match get_cboe_available_indices().await {
        Ok(indices_data) => results.push(("available_indices".to_string(), indices_data)),
        Err(e) => results.push(("available_indices".to_string(), format!("Error: {}", e))),
    }

    // Get US market snapshots for overview
    match get_cboe_index_snapshots(Some("us".to_string())).await {
        Ok(us_snapshots) => results.push(("us_market_overview".to_string(), us_snapshots)),
        Err(e) => results.push(("us_market_overview".to_string(), format!("Error: {}", e))),
    }

    // Get European market snapshots for overview
    match get_cboe_index_snapshots(Some("eu".to_string())).await {
        Ok(eu_snapshots) => results.push(("european_market_overview".to_string(), eu_snapshots)),
        Err(e) => results.push(("european_market_overview".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "directory_info": results
    }).to_string())
}