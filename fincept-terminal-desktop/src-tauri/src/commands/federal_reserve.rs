// Federal Reserve data commands
use crate::utils::python::execute_python_command;

/// Execute Federal Reserve Python script command
#[tauri::command]
pub async fn execute_federal_reserve_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "federal_reserve_data.py", &cmd_args)
}

/// Get Federal Funds Rate data
#[tauri::command]
pub async fn get_federal_funds_rate(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_federal_reserve_command(app, "federal_funds_rate".to_string(), args).await
}

/// Get SOFR Rate data
#[tauri::command]
pub async fn get_sofr_rate(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_federal_reserve_command(app, "sofr_rate".to_string(), args).await
}

/// Get Treasury Rates data
#[tauri::command]
pub async fn get_treasury_rates(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_federal_reserve_command(app, "treasury_rates".to_string(), args).await
}

/// Get Yield Curve data
#[tauri::command]
pub async fn get_yield_curve(app: tauri::AppHandle, date: Option<String>) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(date) = date {
        args.push(date);
    }
    execute_federal_reserve_command(app, "yield_curve".to_string(), args).await
}

/// Get Money Measures data
#[tauri::command]
pub async fn get_money_measures(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
    adjusted: Option<bool>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(adjusted) = adjusted {
        args.push(adjusted.to_string());
    }
    execute_federal_reserve_command(app, "money_measures".to_string(), args).await
}

/// Get Central Bank Holdings data
#[tauri::command]
pub async fn get_central_bank_holdings(app: tauri::AppHandle, 
    holding_type: Option<String>,
    summary: Option<bool>,
    date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(holding_type) = holding_type {
        args.push(holding_type);
    } else {
        args.push("all_treasury".to_string());
    }
    if let Some(summary) = summary {
        args.push(summary.to_string());
    }
    if let Some(date) = date {
        args.push(date);
    }
    execute_federal_reserve_command(app, "central_bank_holdings".to_string(), args).await
}

/// Get Overnight Bank Funding Rate data
#[tauri::command]
pub async fn get_overnight_bank_funding_rate(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_federal_reserve_command(app, "overnight_bank_funding_rate".to_string(), args).await
}

/// Get comprehensive monetary data
#[tauri::command]
pub async fn get_comprehensive_monetary_data(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_federal_reserve_command(app, "comprehensive_monetary_data".to_string(), args).await
}

/// Get market overview
#[tauri::command]
pub async fn get_fed_market_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_federal_reserve_command(app, "market_overview".to_string(), vec![]).await
}