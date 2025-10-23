// US Government data commands based on OpenBB government_us provider
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute US Government Python script command
#[tauri::command]
pub async fn execute_government_us_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "government_us_data.py")?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "US Government script not found at: {}",
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
        .map_err(|e| format!("Failed to execute US Government command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("US Government command failed: {}", stderr))
    }
}

/// Get US Treasury prices for a specific date
#[tauri::command]
pub async fn get_treasury_prices(app: tauri::AppHandle, 
    target_date: Option<String>,
    cusip: Option<String>,
    security_type: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(date) = target_date {
        args.push(date);
    }
    if let Some(cusip) = cusip {
        args.push(cusip);
    }
    if let Some(sec_type) = security_type {
        args.push(sec_type);
    }
    execute_government_us_command(app, "treasury_prices".to_string(), args).await
}

/// Get US Treasury auction data
#[tauri::command]
pub async fn get_treasury_auctions(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
    security_type: Option<String>,
    page_size: Option<i32>,
    page_num: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(sec_type) = security_type {
        args.push(sec_type);
    }
    if let Some(page_size) = page_size {
        args.push(page_size.to_string());
    }
    if let Some(page_num) = page_num {
        args.push(page_num.to_string());
    }
    execute_government_us_command(app, "treasury_auctions".to_string(), args).await
}

/// Get comprehensive US Treasury data from multiple endpoints
#[tauri::command]
pub async fn get_comprehensive_treasury_data(app: tauri::AppHandle, 
    target_date: Option<String>,
    security_type: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(date) = target_date {
        args.push(date);
    }
    if let Some(sec_type) = security_type {
        args.push(sec_type);
    }
    execute_government_us_command(app, "comprehensive".to_string(), args).await
}

/// Get US Treasury data summary with key metrics
#[tauri::command]
pub async fn get_treasury_summary(app: tauri::AppHandle, 
    target_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(date) = target_date {
        args.push(date);
    }
    execute_government_us_command(app, "summary".to_string(), args).await
}