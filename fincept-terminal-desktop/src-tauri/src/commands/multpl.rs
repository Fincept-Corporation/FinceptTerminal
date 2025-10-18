// MULTPL (S&P 500 Multiples) data commands based on OpenBB multpl provider
use std::process::Command;

/// Execute MULTPL Python script command
#[tauri::command]
pub async fn execute_multpl_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("multpl_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "MULTPL script not found at: {}",
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
        .map_err(|e| format!("Failed to execute MULTPL command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("MULTPL command failed: {}", stderr))
    }
}

// BASIC SERIES DATA COMMANDS

/// Get data for a specific series
#[tauri::command]
pub async fn get_multpl_series(
    series_name: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![series_name];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_series".to_string(), args).await
}

/// Get data for multiple series
#[tauri::command]
pub async fn get_multpl_multiple_series(
    series_names: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![series_names];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_multiple".to_string(), args).await
}

/// Get list of available series
#[tauri::command]
pub async fn get_multpl_available_series() -> Result<String, String> {
    execute_multpl_command("available_series".to_string(), vec![]).await
}

// CONVENIENCE METHODS FOR POPULAR SERIES

/// Get Shiller P/E ratio data
#[tauri::command]
pub async fn get_multpl_shiller_pe(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_shiller_pe".to_string(), args).await
}

/// Get P/E ratio data
#[tauri::command]
pub async fn get_multpl_pe_ratio(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_pe_ratio".to_string(), args).await
}

/// Get dividend yield data
#[tauri::command]
pub async fn get_multpl_dividend_yield(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_dividend_yield".to_string(), args).await
}

/// Get earnings yield data
#[tauri::command]
pub async fn get_multpl_earnings_yield(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_earnings_yield".to_string(), args).await
}

/// Get price-to-sales ratio data
#[tauri::command]
pub async fn get_multpl_price_to_sales(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_multpl_command("get_price_to_sales".to_string(), args).await
}

// SPECIFIC SERIES CATEGORY COMMANDS

/// Get Shiller P/E data by month
#[tauri::command]
pub async fn get_multpl_shiller_pe_monthly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("shiller_pe_month".to_string(), start_date, end_date).await
}

/// Get Shiller P/E data by year
#[tauri::command]
pub async fn get_multpl_shiller_pe_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("shiller_pe_year".to_string(), start_date, end_date).await
}

/// Get P/E ratio data by year
#[tauri::command]
pub async fn get_multpl_pe_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("pe_year".to_string(), start_date, end_date).await
}

/// Get dividend data by year
#[tauri::command]
pub async fn get_multpl_dividend_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("dividend_year".to_string(), start_date, end_date).await
}

/// Get dividend data by month
#[tauri::command]
pub async fn get_multpl_dividend_monthly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("dividend_month".to_string(), start_date, end_date).await
}

/// Get dividend growth data by year
#[tauri::command]
pub async fn get_multpl_dividend_growth_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("dividend_growth_year".to_string(), start_date, end_date).await
}

/// Get dividend yield data by year
#[tauri::command]
pub async fn get_multpl_dividend_yield_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("dividend_yield_year".to_string(), start_date, end_date).await
}

/// Get earnings data by year
#[tauri::command]
pub async fn get_multpl_earnings_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("earnings_year".to_string(), start_date, end_date).await
}

/// Get earnings data by month
#[tauri::command]
pub async fn get_multpl_earnings_monthly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("earnings_month".to_string(), start_date, end_date).await
}

/// Get earnings growth data by year
#[tauri::command]
pub async fn get_multpl_earnings_growth_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("earnings_growth_year".to_string(), start_date, end_date).await
}

/// Get earnings yield data by year
#[tauri::command]
pub async fn get_multpl_earnings_yield_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("earnings_yield_year".to_string(), start_date, end_date).await
}

/// Get real price data by year
#[tauri::command]
pub async fn get_multpl_real_price_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("real_price_year".to_string(), start_date, end_date).await
}

/// Get inflation-adjusted price data by year
#[tauri::command]
pub async fn get_multpl_inflation_adjusted_price_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("inflation_adjusted_price_year".to_string(), start_date, end_date).await
}

/// Get sales data by year
#[tauri::command]
pub async fn get_multpl_sales_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("sales_year".to_string(), start_date, end_date).await
}

/// Get sales growth data by year
#[tauri::command]
pub async fn get_multpl_sales_growth_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("sales_growth_year".to_string(), start_date, end_date).await
}

/// Get real sales data by year
#[tauri::command]
pub async fn get_multpl_real_sales_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("real_sales_year".to_string(), start_date, end_date).await
}

/// Get price-to-sales data by year
#[tauri::command]
pub async fn get_multpl_price_to_sales_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("price_to_sales_year".to_string(), start_date, end_date).await
}

/// Get price-to-book value data by year
#[tauri::command]
pub async fn get_multpl_price_to_book_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("price_to_book_value_year".to_string(), start_date, end_date).await
}

/// Get book value data by year
#[tauri::command]
pub async fn get_multpl_book_value_yearly(
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    get_multpl_series("book_value_year".to_string(), start_date, end_date).await
}

// OVERVIEW AND ANALYSIS COMMANDS

/// Get valuation overview with key metrics
#[tauri::command]
pub async fn get_multpl_valuation_overview() -> Result<String, String> {
    execute_multpl_command("get_valuation_overview".to_string(), vec![]).await
}

/// Get dividend overview
#[tauri::command]
pub async fn get_multpl_dividend_overview() -> Result<String, String> {
    execute_multpl_command("get_dividend_overview".to_string(), vec![]).await
}

/// Get earnings overview
#[tauri::command]
pub async fn get_multpl_earnings_overview() -> Result<String, String> {
    execute_multpl_command("get_earnings_overview".to_string(), vec![]).await
}

/// Get comprehensive overview across all categories
#[tauri::command]
pub async fn get_multpl_comprehensive_overview() -> Result<String, String> {
    execute_multpl_command("get_comprehensive_overview".to_string(), vec![]).await
}

// CUSTOM ANALYSIS COMMANDS

/// Get complete valuation analysis
#[tauri::command]
pub async fn get_multpl_valuation_analysis() -> Result<String, String> {
    let mut results = Vec::new();

    // Get all valuation metrics
    let valuation_series = vec![
        "shiller_pe_month", "shiller_pe_year", "pe_year", "pe_month",
        "price_to_sales_year", "price_to_sales_quarter",
        "price_to_book_value_year", "price_to_book_value_quarter"
    ];

    for series in valuation_series {
        match get_multpl_series(series.to_string(), None, None).await {
            Ok(data) => results.push((series.to_string(), data)),
            Err(e) => results.push((series.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "valuation_analysis": results,
        "analysis_type": "complete_valuation_metrics"
    }).to_string())
}

/// Get complete dividend analysis
#[tauri::command]
pub async fn get_multpl_dividend_analysis() -> Result<String, String> {
    let mut results = Vec::new();

    // Get all dividend metrics
    let dividend_series = vec![
        "dividend_year", "dividend_month", "dividend_growth_quarter",
        "dividend_growth_year", "dividend_yield_year", "dividend_yield_month"
    ];

    for series in dividend_series {
        match get_multpl_series(series.to_string(), None, None).await {
            Ok(data) => results.push((series.to_string(), data)),
            Err(e) => results.push((series.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "dividend_analysis": results,
        "analysis_type": "complete_dividend_metrics"
    }).to_string())
}

/// Get complete earnings analysis
#[tauri::command]
pub async fn get_multpl_earnings_analysis() -> Result<String, String> {
    let mut results = Vec::new();

    // Get all earnings metrics
    let earnings_series = vec![
        "earnings_year", "earnings_month", "earnings_growth_year",
        "earnings_growth_quarter", "real_earnings_growth_year",
        "real_earnings_growth_quarter", "earnings_yield_year", "earnings_yield_month"
    ];

    for series in earnings_series {
        match get_multpl_series(series.to_string(), None, None).await {
            Ok(data) => results.push((series.to_string(), data)),
            Err(e) => results.push((series.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "earnings_analysis": results,
        "analysis_type": "complete_earnings_metrics"
    }).to_string())
}

/// Get complete sales analysis
#[tauri::command]
pub async fn get_multpl_sales_analysis() -> Result<String, String> {
    let mut results = Vec::new();

    // Get all sales metrics
    let sales_series = vec![
        "sales_year", "sales_quarter", "sales_growth_year", "sales_growth_quarter",
        "real_sales_year", "real_sales_quarter", "real_sales_growth_year",
        "real_sales_growth_quarter"
    ];

    for series in sales_series {
        match get_multpl_series(series.to_string(), None, None).await {
            Ok(data) => results.push((series.to_string(), data)),
            Err(e) => results.push((series.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "sales_analysis": results,
        "analysis_type": "complete_sales_metrics"
    }).to_string())
}

/// Get historical price analysis
#[tauri::command]
pub async fn get_multpl_price_analysis() -> Result<String, String> {
    let mut results = Vec::new();

    // Get all price metrics
    let price_series = vec![
        "real_price_year", "real_price_month",
        "inflation_adjusted_price_year", "inflation_adjusted_price_month"
    ];

    for series in price_series {
        match get_multpl_series(series.to_string(), None, None).await {
            Ok(data) => results.push((series.to_string(), data)),
            Err(e) => results.push((series.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "price_analysis": results,
        "analysis_type": "complete_price_metrics"
    }).to_string())
}

/// Get custom date range analysis for multiple series
#[tauri::command]
pub async fn get_multpl_custom_analysis(
    series_names: String,
    start_date: String,
    end_date: String,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get each series for the custom date range
    let series_list: Vec<&str> = series_names.split(',').collect();

    for series in series_list {
        let trimmed_series = series.trim();
        match get_multpl_series(trimmed_series.to_string(), Some(start_date.clone()), Some(end_date.clone())).await {
            Ok(data) => results.push((trimmed_series.to_string(), data)),
            Err(e) => results.push((trimmed_series.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "custom_analysis": results,
        "series_requested": series_names,
        "date_range": {
            "start": start_date,
            "end": end_date
        }
    }).to_string())
}

/// Get year-over-year comparison for a specific series
#[tauri::command]
pub async fn get_multpl_year_over_year_comparison(
    series_name: String,
    years: Option<i32>, // Number of years to compare, default 10
) -> Result<String, String> {
    let years_to_compare = years.unwrap_or(10);
    let end_date = chrono::Utc::now().format("%Y-%m-%d").to_string();
    let start_date = (chrono::Utc::now() - chrono::Duration::days(years_to_compare as i64 * 365))
        .format("%Y-%m-%d")
        .to_string();

    match get_multpl_series(series_name, Some(start_date), Some(end_date)).await {
        Ok(data) => {
            Ok(serde_json::json!({
                "year_over_year_comparison": data,
                "series_name": series_name,
                "years_compared": years_to_compare
            }).to_string())
        }
        Err(e) => Err(e)
    }
}

/// Get current market valuation summary
#[tauri::command]
pub async fn get_multpl_current_valuation() -> Result<String, String> {
    let mut results = Vec::new();

    // Get current key valuation metrics
    let key_metrics = vec!["shiller_pe_month", "pe_month", "dividend_yield_month", "earnings_yield_month"];

    for metric in key_metrics {
        match get_multpl_series(metric.to_string(), None, None).await {
            Ok(data) => results.push((metric.to_string(), data)),
            Err(e) => results.push((metric.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "current_valuation": results,
        "analysis_type": "current_market_valuation",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}