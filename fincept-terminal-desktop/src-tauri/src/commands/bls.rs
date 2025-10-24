// BLS (Bureau of Labor Statistics) data commands based on OpenBB bls provider
use crate::utils::python::execute_python_command;

/// Execute BLS Python script command
#[tauri::command]
pub async fn execute_bls_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "bls_data.py", &cmd_args)
}

// SERIES DATA COMMANDS

/// Search for BLS series by category and query
#[tauri::command]
pub async fn search_bls_series(app: tauri::AppHandle, 
    query: String,
    category: Option<String>,
    include_extras: Option<bool>,
    include_code_map: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(category) = category {
        args.push(category);
    } else {
        args.push("cpi".to_string());
    }
    if let Some(include_extras) = include_extras {
        args.push(include_extras.to_string());
    }
    if let Some(include_code_map) = include_code_map {
        args.push(include_code_map.to_string());
    }
    execute_bls_command(app, "search_series".to_string(), args).await
}

/// Get time series data for specific series IDs
#[tauri::command]
pub async fn get_bls_series_data(app: tauri::AppHandle, 
    series_ids: String,
    start_date: Option<String>,
    end_date: Option<String>,
    calculations: Option<bool>,
    annual_average: Option<bool>,
    aspects: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![series_ids];
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(calculations) = calculations {
        args.push((!calculations).to_string()); // Note: Python script expects opposite logic
    }
    if let Some(annual_average) = annual_average {
        args.push(annual_average.to_string());
    }
    if let Some(aspects) = aspects {
        args.push(aspects.to_string());
    }
    execute_bls_command(app, "get_series".to_string(), args).await
}

/// Get data for popular economic series
#[tauri::command]
pub async fn get_bls_popular_series(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bls_command(app, "get_popular".to_string(), vec![]).await
}

// ECONOMIC OVERVIEW COMMANDS

/// Get comprehensive labor market overview
#[tauri::command]
pub async fn get_bls_labor_market_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bls_command(app, "get_labor_overview".to_string(), vec![]).await
}

/// Get comprehensive inflation overview
#[tauri::command]
pub async fn get_bls_inflation_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bls_command(app, "get_inflation_overview".to_string(), vec![]).await
}

/// Get Employment Cost Index data
#[tauri::command]
pub async fn get_bls_employment_cost_index(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bls_command(app, "get_employment_cost_index".to_string(), vec![]).await
}

/// Get Productivity and Costs data
#[tauri::command]
pub async fn get_bls_productivity_costs(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bls_command(app, "get_productivity_costs".to_string(), vec![]).await
}

/// Get available survey categories
#[tauri::command]
pub async fn get_bls_survey_categories(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bls_command(app, "get_categories".to_string(), vec![]).await
}

// SPECIALIZED DATA COMMANDS

/// Get Consumer Price Index data
#[tauri::command]
pub async fn get_bls_cpi_data(app: tauri::AppHandle, 
    series_ids: Option<String>, // If None, use popular CPI series
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let cpi_series = series_ids.unwrap_or_else(|| "CPIAUCSL,CPIAUCNS,CUSR0000SA0".to_string());
    get_bls_series_data(app, cpi_series, start_date, end_date, Some(true), Some(false), Some(false)).await
}

/// Get unemployment rate data
#[tauri::command]
pub async fn get_bls_unemployment_data(app: tauri::AppHandle, 
    series_ids: Option<String>, // If None, use popular unemployment series
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let unemployment_series = series_ids.unwrap_or_else(|| "UNRATE,LNS14000000".to_string());
    get_bls_series_data(app, unemployment_series, start_date, end_date, Some(true), Some(false), Some(false)).await
}

/// Get nonfarm payrolls data
#[tauri::command]
pub async fn get_bls_payrolls_data(app: tauri::AppHandle, 
    series_ids: Option<String>, // If None, use popular payrolls series
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let payrolls_series = series_ids.unwrap_or_else(|| "PAYEMS,CES0000000001".to_string());
    get_bls_series_data(app, payrolls_series, start_date, end_date, Some(true), Some(false), Some(false)).await
}

/// Get labor force participation rate data
#[tauri::command]
pub async fn get_bls_participation_data(app: tauri::AppHandle, 
    series_ids: Option<String>, // If None, use popular participation series
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let participation_series = series_ids.unwrap_or_else(|| "CIVPART,LNS13000000".to_string());
    get_bls_series_data(app, participation_series, start_date, end_date, Some(true), Some(false), Some(false)).await
}

/// Get Producer Price Index data
#[tauri::command]
pub async fn get_bls_ppi_data(app: tauri::AppHandle, 
    series_ids: Option<String>, // If None, use popular PPI series
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let ppi_series = series_ids.unwrap_or_else(|| "WPSFD4,PPIACO".to_string());
    get_bls_series_data(app, ppi_series, start_date, end_date, Some(true), Some(false), Some(false)).await
}

/// Get JOLTS (Job Openings and Labor Turnover Survey) data
#[tauri::command]
pub async fn get_bls_jolts_data(app: tauri::AppHandle, 
    series_ids: Option<String>, // If None, use popular JOLTS series
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let jolts_series = series_ids.unwrap_or_else(|| "JTSJOL,JTSQUR,JTSHIL,JTSJOR".to_string());
    get_bls_series_data(app, jolts_series, start_date, end_date, Some(true), Some(false), Some(false)).await
}

// COMPREHENSIVE ANALYSIS COMMANDS

/// Get comprehensive economic dashboard with key indicators
#[tauri::command]
pub async fn get_bls_economic_dashboard(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get popular series data
    match get_bls_popular_series(app.clone()).await {
        Ok(data) => results.push(("popular_series".to_string(), data)),
        Err(e) => results.push(("popular_series".to_string(), format!("Error: {}", e))),
    }

    // Get labor market overview
    match get_bls_labor_market_overview(app.clone()).await {
        Ok(data) => results.push(("labor_market".to_string(), data)),
        Err(e) => results.push(("labor_market".to_string(), format!("Error: {}", e))),
    }

    // Get inflation overview
    match get_bls_inflation_overview(app.clone()).await {
        Ok(data) => results.push(("inflation".to_string(), data)),
        Err(e) => results.push(("inflation".to_string(), format!("Error: {}", e))),
    }

    // Get employment cost index
    match get_bls_employment_cost_index(app.clone()).await {
        Ok(data) => results.push(("employment_costs".to_string(), data)),
        Err(e) => results.push(("employment_costs".to_string(), format!("Error: {}", e))),
    }

    // Get productivity and costs
    match get_bls_productivity_costs(app).await {
        Ok(data) => results.push(("productivity_costs".to_string(), data)),
        Err(e) => results.push(("productivity_costs".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "economic_dashboard": results
    }).to_string())
}

/// Get inflation analysis with CPI breakdown
#[tauri::command]
pub async fn get_bls_inflation_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get CPI all items
    match get_bls_cpi_data(app.clone(), Some("CPIAUCSL".to_string()), None, None).await {
        Ok(data) => results.push(("cpi_all_items".to_string(), data)),
        Err(e) => results.push(("cpi_all_items".to_string(), format!("Error: {}", e))),
    }

    // Get core CPI (excluding food and energy)
    match get_bls_cpi_data(app.clone(), Some("CUSR0000SA0L1E".to_string()), None, None).await {
        Ok(data) => results.push(("core_cpi".to_string(), data)),
        Err(e) => results.push(("core_cpi".to_string(), format!("Error: {}", e))),
    }

    // Get energy inflation
    match get_bls_cpi_data(app.clone(), Some("CUSR0000SA0E".to_string()), None, None).await {
        Ok(data) => results.push(("energy_inflation".to_string(), data)),
        Err(e) => results.push(("energy_inflation".to_string(), format!("Error: {}", e))),
    }

    // Get food inflation
    match get_bls_cpi_data(app.clone(), Some("CUSR0000SA0F".to_string()), None, None).await {
        Ok(data) => results.push(("food_inflation".to_string(), data)),
        Err(e) => results.push(("food_inflation".to_string(), format!("Error: {}", e))),
    }

    // Get PPI data
    match get_bls_ppi_data(app, None, None, None).await {
        Ok(data) => results.push(("producer_price_index".to_string(), data)),
        Err(e) => results.push(("producer_price_index".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "inflation_analysis": results
    }).to_string())
}

/// Get labor market analysis with employment metrics
#[tauri::command]
pub async fn get_bls_labor_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get unemployment data
    match get_bls_unemployment_data(app.clone(), None, None, None).await {
        Ok(data) => results.push(("unemployment_rate".to_string(), data)),
        Err(e) => results.push(("unemployment_rate".to_string(), format!("Error: {}", e))),
    }

    // Get payrolls data
    match get_bls_payrolls_data(app.clone(), None, None, None).await {
        Ok(data) => results.push(("nonfarm_payrolls".to_string(), data)),
        Err(e) => results.push(("nonfarm_payrolls".to_string(), format!("Error: {}", e))),
    }

    // Get labor force participation
    match get_bls_participation_data(app.clone(), None, None, None).await {
        Ok(data) => results.push(("labor_force_participation".to_string(), data)),
        Err(e) => results.push(("labor_force_participation".to_string(), format!("Error: {}", e))),
    }

    // Get JOLTS data
    match get_bls_jolts_data(app.clone(), None, None, None).await {
        Ok(data) => results.push(("jolts_data".to_string(), data)),
        Err(e) => results.push(("jolts_data".to_string(), format!("Error: {}", e))),
    }

    // Get employment cost index
    match get_bls_employment_cost_index(app).await {
        Ok(data) => results.push(("employment_cost_index".to_string(), data)),
        Err(e) => results.push(("employment_cost_index".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "labor_analysis": results
    }).to_string())
}

/// Get custom series comparison
#[tauri::command]
pub async fn get_bls_series_comparison(app: tauri::AppHandle, 
    series_ids: String,
    start_date: Option<String>,
    end_date: Option<String>,
    normalize: Option<bool>, // If true, normalize all series to base value of 100
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get raw series data
    match get_bls_series_data(app.clone(), series_ids.clone(), start_date.clone(), end_date.clone(), Some(true), Some(false), Some(false)).await {
        Ok(data) => results.push(("raw_data".to_string(), data)),
        Err(e) => results.push(("raw_data".to_string(), format!("Error: {}", e))),
    }

    // If normalization is requested, note it (actual normalization would be done in frontend)
    if normalize.unwrap_or(false) {
        results.push(("normalization_note".to_string(), "Series should be normalized to base value of 100 in frontend processing".to_string()));
    }

    // Get series metadata/search info for context
    let series_list: Vec<&str> = series_ids.split(',').collect();
    for series_id in series_list.iter().take(3) { // Limit to first 3 for performance
        let trimmed_id = series_id.trim();
        match search_bls_series(app.clone(), trimmed_id.to_string(), Some("cpi".to_string()), Some(true), Some(false)).await {
            Ok(search_data) => results.push((format!("search_{}", trimmed_id), search_data)),
            Err(_) => {} // Skip search errors
        }
    }

    Ok(serde_json::json!({
        "series_comparison": results,
        "series_ids": series_ids,
        "normalization_requested": normalize.unwrap_or(false)
    }).to_string())
}

/// Get BLS data directory and available series information
#[tauri::command]
pub async fn get_bls_data_directory(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get survey categories
    match get_bls_survey_categories(app.clone()).await {
        Ok(data) => results.push(("survey_categories".to_string(), data)),
        Err(e) => results.push(("survey_categories".to_string(), format!("Error: {}", e))),
    }

    // Search for series in popular categories
    let popular_searches = vec![
        ("inflation", "cpi"),
        ("employment", "employment"),
        ("wages", "wages"),
        ("productivity", "ip")
    ];

    for (search_term, category) in popular_searches {
        match search_bls_series(app.clone(), search_term.to_string(), Some(category.to_string()), Some(false), Some(false)).await {
            Ok(data) => results.push((format!("sample_{}", category), data)),
            Err(e) => results.push((format!("sample_{}", category), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "data_directory": results
    }).to_string())
}