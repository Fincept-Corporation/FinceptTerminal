// World Bank data commands
use crate::utils::python::execute_python_command;

/// Execute World Bank Python script command
#[tauri::command]
pub async fn execute_worldbank_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "worldbank_data.py", &cmd_args)
}

/// Get World Bank indicator data
#[tauri::command]
pub async fn get_worldbank_indicator(
    app: tauri::AppHandle,
    indicator: String,
    country: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![indicator, country];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_worldbank_command(app, "indicator".to_string(), args).await
}

/// Search World Bank indicators
#[tauri::command]
pub async fn search_worldbank_indicators(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_worldbank_command(app, "search".to_string(), vec![query]).await
}

/// Get country information
#[tauri::command]
pub async fn get_worldbank_country_info(
    app: tauri::AppHandle,
    country_code: String,
) -> Result<String, String> {
    execute_worldbank_command(app, "country_info".to_string(), vec![country_code]).await
}

/// Get list of countries
#[tauri::command]
pub async fn get_worldbank_countries(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_worldbank_command(app, "countries".to_string(), vec![]).await
}

/// Get GDP data for countries
#[tauri::command]
pub async fn get_worldbank_gdp(
    app: tauri::AppHandle,
    country: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![country];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_worldbank_command(app, "gdp".to_string(), args).await
}

/// Get economic overview for a country
#[tauri::command]
pub async fn get_worldbank_economic_overview(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_worldbank_command(app, "economic_overview".to_string(), vec![country]).await
}

/// Get development indicators
#[tauri::command]
pub async fn get_worldbank_development_indicators(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_worldbank_command(app, "development_indicators".to_string(), vec![country]).await
}
