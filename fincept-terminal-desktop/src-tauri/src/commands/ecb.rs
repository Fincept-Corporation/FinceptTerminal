// ECB (European Central Bank) data commands based on OpenBB ECB provider
use crate::utils::python::execute_python_command;

/// Execute ECB Python script command
#[tauri::command]
pub async fn execute_ecb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "ecb_data.py", &cmd_args)
}

// BASIC DATA ACCESS COMMANDS

/// Get available ECB data categories and options
#[tauri::command]
pub async fn get_ecb_available_categories(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_ecb_command(app, "available_categories".to_string(), vec![]).await
}

/// Get currency reference rates (daily FX rates)
#[tauri::command]
pub async fn get_ecb_currency_reference_rates(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_ecb_command(app, "currency_rates".to_string(), vec![]).await
}

/// Get ECB overview with multiple data sources
#[tauri::command]
pub async fn get_ecb_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_ecb_command(app, "overview".to_string(), vec![]).await
}

// YIELD CURVE COMMANDS

/// Get yield curve data with specified parameters
#[tauri::command]
pub async fn get_ecb_yield_curve(app: tauri::AppHandle, 
    rating: Option<String>,
    curve_type: Option<String>,
    date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(r) = rating {
        args.push("--rating".to_string());
        args.push(r);
    }

    if let Some(ct) = curve_type {
        args.push("--type".to_string());
        args.push(ct);
    }

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

/// Get AAA-rated spot rate yield curve
#[tauri::command]
pub async fn get_ecb_yield_curve_aaa_spot(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--rating".to_string(), "aaa".to_string(), "--type".to_string(), "spot_rate".to_string()];

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

/// Get AAA-rated instantaneous forward yield curve
#[tauri::command]
pub async fn get_ecb_yield_curve_aaa_forward(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--rating".to_string(), "aaa".to_string(), "--type".to_string(), "instantaneous_forward".to_string()];

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

/// Get AAA-rated par yield curve
#[tauri::command]
pub async fn get_ecb_yield_curve_aaa_par_yield(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--rating".to_string(), "aaa".to_string(), "--type".to_string(), "par_yield".to_string()];

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

/// Get all ratings spot rate yield curve
#[tauri::command]
pub async fn get_ecb_yield_curve_all_spot(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--rating".to_string(), "all_ratings".to_string(), "--type".to_string(), "spot_rate".to_string()];

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

/// Get all ratings instantaneous forward yield curve
#[tauri::command]
pub async fn get_ecb_yield_curve_all_forward(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--rating".to_string(), "all_ratings".to_string(), "--type".to_string(), "instantaneous_forward".to_string()];

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

/// Get all ratings par yield curve
#[tauri::command]
pub async fn get_ecb_yield_curve_all_par_yield(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--rating".to_string(), "all_ratings".to_string(), "--type".to_string(), "par_yield".to_string()];

    if let Some(d) = date {
        args.push("--date".to_string());
        args.push(d);
    }

    execute_ecb_command(app, "yield_curve".to_string(), args).await
}

// BALANCE OF PAYMENTS COMMANDS

/// Get balance of payments data with specified parameters
#[tauri::command]
pub async fn get_ecb_balance_of_payments(app: tauri::AppHandle, 
    country: Option<String>,
    frequency: Option<String>,
    report_type: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(c) = country {
        args.push("--country".to_string());
        args.push(c);
    }

    if let Some(f) = frequency {
        args.push("--frequency".to_string());
        args.push(f);
    }

    if let Some(rt) = report_type {
        args.push("--report_type".to_string());
        args.push(rt);
    }

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get main balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_main(app: tauri::AppHandle, 
    country: String,
    frequency: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--country".to_string(), country, "--report_type".to_string(), "main".to_string()];

    if let Some(f) = frequency {
        args.push("--frequency".to_string());
        args.push(f);
    }

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get summary balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_summary(app: tauri::AppHandle, 
    country: String,
    frequency: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["--country".to_string(), country, "--report_type".to_string(), "summary".to_string()];

    if let Some(f) = frequency {
        args.push("--frequency".to_string());
        args.push(f);
    }

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get services balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_services(app: tauri::AppHandle, 
    country: String,
) -> Result<String, String> {
    let args = vec![
        "--country".to_string(),
        country,
        "--report_type".to_string(),
        "services".to_string(),
        "--frequency".to_string(),
        "quarterly".to_string()
    ];

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get investment income balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_investment_income(app: tauri::AppHandle, 
    country: String,
) -> Result<String, String> {
    let args = vec![
        "--country".to_string(),
        country,
        "--report_type".to_string(),
        "investment_income".to_string(),
        "--frequency".to_string(),
        "quarterly".to_string()
    ];

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get direct investment balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_direct_investment(app: tauri::AppHandle, 
    country: String,
) -> Result<String, String> {
    let args = vec![
        "--country".to_string(),
        country,
        "--report_type".to_string(),
        "direct_investment".to_string(),
        "--frequency".to_string(),
        "quarterly".to_string()
    ];

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get portfolio investment balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_portfolio_investment(app: tauri::AppHandle, 
    country: String,
) -> Result<String, String> {
    let args = vec![
        "--country".to_string(),
        country,
        "--report_type".to_string(),
        "portfolio_investment".to_string(),
        "--frequency".to_string(),
        "quarterly".to_string()
    ];

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

/// Get other investment balance of payments data for a country
#[tauri::command]
pub async fn get_ecb_bop_other_investment(app: tauri::AppHandle, 
    country: String,
) -> Result<String, String> {
    let args = vec![
        "--country".to_string(),
        country,
        "--report_type".to_string(),
        "other_investment".to_string(),
        "--frequency".to_string(),
        "quarterly".to_string()
    ];

    execute_ecb_command(app, "balance_of_payments".to_string(), args).await
}

// CONVENIENCE COMMANDS FOR MAJOR ECONOMIES

/// Get US balance of payments data
#[tauri::command]
pub async fn get_ecb_bop_united_states(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    get_ecb_bop_main(app, "US".to_string(), frequency).await
}

/// Get Japan balance of payments data
#[tauri::command]
pub async fn get_ecb_bop_japan(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    get_ecb_bop_main(app, "JP".to_string(), frequency).await
}

/// Get United Kingdom balance of payments data
#[tauri::command]
pub async fn get_ecb_bop_united_kingdom(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    get_ecb_bop_main(app, "GB".to_string(), frequency).await
}

/// Get Switzerland balance of payments data
#[tauri::command]
pub async fn get_ecb_bop_switzerland(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    get_ecb_bop_main(app, "CH".to_string(), frequency).await
}

/// Get Canada balance of payments data
#[tauri::command]
pub async fn get_ecb_bop_canada(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    get_ecb_bop_main(app, "CA".to_string(), frequency).await
}

/// Get Australia balance of payments data
#[tauri::command]
pub async fn get_ecb_bop_australia(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    get_ecb_bop_main(app, "AU".to_string(), frequency).await
}

// ANALYSIS COMMANDS

/// Get comprehensive ECB financial markets analysis
#[tauri::command]
pub async fn get_ecb_financial_markets_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get currency rates
    match get_ecb_currency_reference_rates(app.clone()).await {
        Ok(data) => results.push(("currency_rates".to_string(), data)),
        Err(e) => results.push(("currency_rates".to_string(), format!("Error: {}", e))),
    }

    // Get AAA yield curve
    match get_ecb_yield_curve_aaa_spot(app.clone(), None).await {
        Ok(data) => results.push(("yield_curve_aaa_spot".to_string(), data)),
        Err(e) => results.push(("yield_curve_aaa_spot".to_string(), format!("Error: {}", e))),
    }

    // Get all ratings yield curve
    match get_ecb_yield_curve_all_spot(app.clone(), None).await {
        Ok(data) => results.push(("yield_curve_all_spot".to_string(), data)),
        Err(e) => results.push(("yield_curve_all_spot".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "financial_markets_analysis": results,
        "analysis_type": "ecb_comprehensive_financial_markets",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}

/// Get ECB yield curve analysis comparing different ratings and types
#[tauri::command]
pub async fn get_ecb_yield_curve_analysis(app: tauri::AppHandle, 
    date: Option<String>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get different yield curve types
    let curve_types = vec![
        ("aaa_spot", "aaa", "spot_rate"),
        ("aaa_forward", "aaa", "instantaneous_forward"),
        ("aaa_par_yield", "aaa", "par_yield"),
        ("all_spot", "all_ratings", "spot_rate"),
        ("all_forward", "all_ratings", "instantaneous_forward"),
        ("all_par_yield", "all_ratings", "par_yield"),
    ];

    for (name, rating, curve_type) in curve_types {
        match get_ecb_yield_curve(app.clone(), 
            Some(rating.to_string()),
            Some(curve_type.to_string()),
            date.clone()
        ).await {
            Ok(data) => results.push((name.to_string(), data)),
            Err(e) => results.push((name.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "yield_curve_analysis": results,
        "date_analyzed": date.unwrap_or_else(|| "latest".to_string()),
        "analysis_type": "ecb_yield_curve_comparison",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}

/// Get ECB major economies balance of payments analysis
#[tauri::command]
pub async fn get_ecb_major_economies_bop_analysis(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get BOP data for major economies
    let countries = vec![
        ("US", "United States"),
        ("JP", "Japan"),
        ("GB", "United Kingdom"),
        ("CH", "Switzerland"),
        ("CA", "Canada"),
        ("AU", "Australia"),
        ("DE", "Germany"),
        ("FR", "France"),
    ];

    for (code, _name) in countries {
        match get_ecb_bop_main(app.clone(), code.to_string(), frequency.clone()).await {
            Ok(data) => results.push((format!("bop_{}", code.to_lowercase()), data)),
            Err(e) => results.push((format!("bop_{}", code.to_lowercase()), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "major_economies_bop_analysis": results,
        "frequency": frequency.unwrap_or_else(|| "monthly".to_string()),
        "analysis_type": "ecb_major_economies_balance_of_payments",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}

/// Get custom ECB analysis for specific countries and parameters
#[tauri::command]
pub async fn get_ecb_custom_analysis(app: tauri::AppHandle, 
    countries: String,  // Comma-separated country codes
    include_yield_curve: Option<bool>,
    include_currency_rates: Option<bool>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get currency rates if requested
    if include_yield_curve.unwrap_or(true) {
        match get_ecb_currency_reference_rates(app.clone()).await {
            Ok(data) => results.push(("currency_rates".to_string(), data)),
            Err(e) => results.push(("currency_rates".to_string(), format!("Error: {}", e))),
        }
    }

    // Get yield curve if requested
    if include_yield_curve.unwrap_or(true) {
        match get_ecb_yield_curve_aaa_spot(app.clone(), None).await {
            Ok(data) => results.push(("yield_curve".to_string(), data)),
            Err(e) => results.push(("yield_curve".to_string(), format!("Error: {}", e))),
        }
    }

    // Get BOP data for specified countries
    let country_list: Vec<&str> = countries.split(',').collect();
    for country_code in country_list {
        let trimmed_code = country_code.trim();
        match get_ecb_bop_main(app.clone(), trimmed_code.to_string(), None).await {
            Ok(data) => results.push((format!("bop_{}", trimmed_code.to_lowercase()), data)),
            Err(e) => results.push((format!("bop_{}", trimmed_code.to_lowercase()), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "custom_ecb_analysis": results,
        "countries_requested": countries,
        "included_data": {
            "currency_rates": include_currency_rates.unwrap_or(true),
            "yield_curve": include_yield_curve.unwrap_or(true),
            "balance_of_payments": true
        },
        "analysis_type": "ecb_custom_multi_country_analysis",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}

/// Get current ECB market snapshot
#[tauri::command]
pub async fn get_ecb_current_market_snapshot(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get current currency rates
    match get_ecb_currency_reference_rates(app.clone()).await {
        Ok(data) => results.push(("currency_rates".to_string(), data)),
        Err(e) => results.push(("currency_rates".to_string(), format!("Error: {}", e))),
    }

    // Get current yield curve
    match get_ecb_yield_curve_aaa_spot(app.clone(), None).await {
        Ok(data) => results.push(("yield_curve_current".to_string(), data)),
        Err(e) => results.push(("yield_curve_current".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "current_market_snapshot": results,
        "analysis_type": "ecb_current_market_conditions",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}