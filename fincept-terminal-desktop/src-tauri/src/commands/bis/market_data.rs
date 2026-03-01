// BIS Convenience Market Data Commands

use super::execute_bis_command;

/// Get effective exchange rates data (WS_EER)
#[tauri::command]
pub async fn get_bis_effective_exchange_rates(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_effective_exchange_rates".to_string(), args).await
}

/// Get central bank policy rates (WS_CBPOL)
#[tauri::command]
pub async fn get_bis_central_bank_policy_rates(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_central_bank_policy_rates".to_string(), args).await
}

/// Get long-term interest rates (WS_LTINT)
#[tauri::command]
pub async fn get_bis_long_term_interest_rates(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_long_term_interest_rates".to_string(), args).await
}

/// Get short-term interest rates (WS_STINT)
#[tauri::command]
pub async fn get_bis_short_term_interest_rates(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_short_term_interest_rates".to_string(), args).await
}

/// Get exchange rates (WS_XRU)
#[tauri::command]
pub async fn get_bis_exchange_rates(app: tauri::AppHandle,
    currency_pairs: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = currency_pairs { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_exchange_rates".to_string(), args).await
}

/// Get credit to non-financial sector (WS_CRD)
#[tauri::command]
pub async fn get_bis_credit_to_non_financial_sector(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    sectors: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(l) = sectors { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_credit_to_non_financial_sector".to_string(), args).await
}

/// Get house price indices (WS_HP)
#[tauri::command]
pub async fn get_bis_house_prices(app: tauri::AppHandle,
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(l) = countries { args.extend(l); }
    if let Some(s) = start_period { args.push(s); }
    if let Some(e) = end_period { args.push(e); }
    if let Some(d) = detail { args.push(d); }
    execute_bis_command(app, "get_house_prices".to_string(), args).await
}
