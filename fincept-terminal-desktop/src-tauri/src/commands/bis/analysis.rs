// BIS Analysis and Overview Commands

use super::execute_bis_command;
use super::market_data::{
    get_bis_central_bank_policy_rates, get_bis_short_term_interest_rates,
    get_bis_long_term_interest_rates, get_bis_effective_exchange_rates,
    get_bis_exchange_rates, get_bis_credit_to_non_financial_sector, get_bis_house_prices,
};
use super::structure::{get_bis_data_structures, get_bis_concept_schemes, get_bis_codelists};

/// Get overview of all available datasets
#[tauri::command]
pub async fn get_bis_available_datasets(app: tauri::AppHandle) -> Result<String, String> {
    execute_bis_command(app, "get_available_datasets".to_string(), vec![]).await
}

/// Search for datasets matching a query
#[tauri::command]
pub async fn search_bis_datasets(app: tauri::AppHandle,
    query: String,
    agency_id: Option<String>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(agency) = agency_id { args.push(agency); }
    execute_bis_command(app, "search_datasets".to_string(), args).await
}

/// Get comprehensive economic overview for countries
#[tauri::command]
pub async fn get_bis_economic_overview(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    execute_bis_command(app, "get_economic_overview".to_string(), args).await
}

/// Get comprehensive metadata for a specific dataset
#[tauri::command]
pub async fn get_bis_dataset_metadata(app: tauri::AppHandle, flow: String) -> Result<String, String> {
    execute_bis_command(app, "get_dataset_metadata".to_string(), vec![flow]).await
}

/// Get global monetary policy overview
#[tauri::command]
pub async fn get_bis_monetary_policy_overview(app: tauri::AppHandle) -> Result<String, String> {
    let mut results = Vec::new();

    let major = Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]);

    match get_bis_central_bank_policy_rates(app.clone(), major.clone(), None, None, Some("dataonly".to_string())).await {
        Ok(data) => results.push(("policy_rates".to_string(), data)),
        Err(e) => results.push(("policy_rates".to_string(), format!("Error: {}", e))),
    }

    match get_bis_short_term_interest_rates(app.clone(), major.clone(), None, None, Some("dataonly".to_string())).await {
        Ok(data) => results.push(("short_term_rates".to_string(), data)),
        Err(e) => results.push(("short_term_rates".to_string(), format!("Error: {}", e))),
    }

    match get_bis_long_term_interest_rates(app.clone(), major.clone(), None, None, Some("dataonly".to_string())).await {
        Ok(data) => results.push(("long_term_rates".to_string(), data)),
        Err(e) => results.push(("long_term_rates".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "monetary_policy_overview": results,
        "focus_economies": ["US", "GB", "JP", "EA"],
        "description": "Central bank policy rates and interest rate trends"
    }).to_string())
}

/// Get global exchange rate analysis
#[tauri::command]
pub async fn get_bis_exchange_rate_analysis(app: tauri::AppHandle) -> Result<String, String> {
    let mut results = Vec::new();

    match get_bis_effective_exchange_rates(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None, None, Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("effective_rates".to_string(), data)),
        Err(e) => results.push(("effective_rates".to_string(), format!("Error: {}", e))),
    }

    match get_bis_exchange_rates(
        app.clone(),
        Some(vec!["USD".to_string(), "EUR".to_string(), "GBP".to_string(), "JPY".to_string()]),
        None, None, Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("major_currencies".to_string(), data)),
        Err(e) => results.push(("major_currencies".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "exchange_rate_analysis": results,
        "focus_currencies": ["USD", "EUR", "GBP", "JPY"],
        "description": "Exchange rate movements and trends"
    }).to_string())
}

/// Get global credit conditions overview
#[tauri::command]
pub async fn get_bis_credit_conditions_overview(app: tauri::AppHandle) -> Result<String, String> {
    let mut results = Vec::new();

    match get_bis_credit_to_non_financial_sector(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None, None, None, Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("credit_growth".to_string(), data)),
        Err(e) => results.push(("credit_growth".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "credit_conditions_overview": results,
        "focus_economies": ["US", "GB", "JP", "EA"],
        "description": "Credit growth and conditions analysis"
    }).to_string())
}

/// Get global housing market overview
#[tauri::command]
pub async fn get_bis_housing_market_overview(app: tauri::AppHandle) -> Result<String, String> {
    let mut results = Vec::new();

    match get_bis_house_prices(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "CA".to_string()]),
        None, None, Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("house_prices".to_string(), data)),
        Err(e) => results.push(("house_prices".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "housing_market_overview": results,
        "focus_markets": ["US", "GB", "JP", "CA"],
        "description": "Housing price indices and trends"
    }).to_string())
}

/// Get BIS data directory and available resources
#[tauri::command]
pub async fn get_bis_data_directory(app: tauri::AppHandle) -> Result<String, String> {
    let mut results = Vec::new();

    match get_bis_available_datasets(app.clone()).await {
        Ok(data) => results.push(("available_datasets".to_string(), data)),
        Err(e) => results.push(("available_datasets".to_string(), format!("Error: {}", e))),
    }

    match get_bis_data_structures(app.clone(), Some("BIS".to_string()), None, None, Some("none".to_string()), Some("allstubs".to_string())).await {
        Ok(data) => results.push(("data_structures".to_string(), data)),
        Err(e) => results.push(("data_structures".to_string(), format!("Error: {}", e))),
    }

    match get_bis_concept_schemes(app.clone(), Some("BIS".to_string()), None, None, Some("none".to_string()), Some("allstubs".to_string())).await {
        Ok(data) => results.push(("concept_schemes".to_string(), data)),
        Err(e) => results.push(("concept_schemes".to_string(), format!("Error: {}", e))),
    }

    match get_bis_codelists(app.clone(), Some("BIS".to_string()), None, None, Some("none".to_string()), Some("allstubs".to_string())).await {
        Ok(data) => results.push(("codelists".to_string(), data)),
        Err(e) => results.push(("codelists".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "data_directory": results,
        "description": "BIS statistical data directory and available resources"
    }).to_string())
}

/// Get comprehensive BIS statistics summary
#[tauri::command]
pub async fn get_bis_comprehensive_summary(app: tauri::AppHandle) -> Result<String, String> {
    let mut results = Vec::new();

    match get_bis_monetary_policy_overview(app.clone()).await {
        Ok(data) => results.push(("monetary_policy".to_string(), data)),
        Err(e) => results.push(("monetary_policy".to_string(), format!("Error: {}", e))),
    }

    match get_bis_exchange_rate_analysis(app.clone()).await {
        Ok(data) => results.push(("exchange_rates".to_string(), data)),
        Err(e) => results.push(("exchange_rates".to_string(), format!("Error: {}", e))),
    }

    match get_bis_credit_conditions_overview(app.clone()).await {
        Ok(data) => results.push(("credit_conditions".to_string(), data)),
        Err(e) => results.push(("credit_conditions".to_string(), format!("Error: {}", e))),
    }

    match get_bis_housing_market_overview(app.clone()).await {
        Ok(data) => results.push(("housing_markets".to_string(), data)),
        Err(e) => results.push(("housing_markets".to_string(), format!("Error: {}", e))),
    }

    match get_bis_data_directory(app.clone()).await {
        Ok(data) => results.push(("data_directory".to_string(), data)),
        Err(e) => results.push(("data_directory".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "comprehensive_summary": results,
        "description": "Complete BIS statistics overview covering monetary policy, exchange rates, credit conditions, and housing markets",
        "timestamp": chrono::Utc::now().format("%Y-%m-%d %H:%M:%S UTC").to_string()
    }).to_string())
}
