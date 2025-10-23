// World Bank data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute World Bank Python script command
#[tauri::command]
pub async fn execute_worldbank_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "worldbank_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "World Bank script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute World Bank command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("World Bank command failed: {}", stderr))
    }
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
