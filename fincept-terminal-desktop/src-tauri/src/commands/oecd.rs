// OECD data commands based on OpenBB oecd provider
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute OECD Python script command
#[tauri::command]
pub async fn execute_oecd_command(
    app: tauri::AppHandle, 
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "oecd_data.py")?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "OECD script not found at: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    // Execute Python script
    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute OECD command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("OECD command failed: {}", stderr))
    }
}

/// Get real GDP data for specified countries
#[tauri::command]
pub async fn get_oecd_gdp_real(app: tauri::AppHandle, 
    countries: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    }
    if let Some(frequency) = frequency {
        args.push(frequency);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_oecd_command(app, "gdp_real".to_string(), args).await
}

/// Get Consumer Price Index data
#[tauri::command]
pub async fn get_oecd_consumer_price_index(app: tauri::AppHandle, 
    countries: Option<String>,
    expenditure: Option<String>,
    frequency: Option<String>,
    units: Option<String>,
    harmonized: Option<bool>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    }
    if let Some(expenditure) = expenditure {
        args.push(expenditure);
    }
    if let Some(frequency) = frequency {
        args.push(frequency);
    }
    if let Some(units) = units {
        args.push(units);
    }
    if let Some(harmonized) = harmonized {
        args.push(harmonized.to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_oecd_command(app, "cpi".to_string(), args).await
}

/// Get GDP forecast data
#[tauri::command]
pub async fn get_oecd_gdp_forecast(app: tauri::AppHandle, 
    countries: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_oecd_command(app, "gdp_forecast".to_string(), args).await
}

/// Get unemployment rate data
#[tauri::command]
pub async fn get_oecd_unemployment(app: tauri::AppHandle, 
    countries: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    }
    if let Some(frequency) = frequency {
        args.push(frequency);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_oecd_command(app, "unemployment".to_string(), args).await
}

/// Get comprehensive economic summary for a country
#[tauri::command]
pub async fn get_oecd_economic_summary(app: tauri::AppHandle, 
    country: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(country) = country {
        args.push(country);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_oecd_command(app, "economic_summary".to_string(), args).await
}

/// Get list of available countries and data categories
#[tauri::command]
pub async fn get_oecd_country_list(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_oecd_command(app, "country_list".to_string(), vec![]).await
}