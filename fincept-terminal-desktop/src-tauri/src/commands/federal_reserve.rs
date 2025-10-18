// Federal Reserve data commands
use std::process::Command;

/// Execute Federal Reserve Python script command
#[tauri::command]
pub async fn execute_federal_reserve_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("federal_reserve_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Federal Reserve script not found at: {}",
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
        .map_err(|e| format!("Failed to execute Federal Reserve command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Federal Reserve command failed: {}", stderr))
    }
}

/// Get Federal Funds Rate data
#[tauri::command]
pub async fn get_federal_funds_rate(
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
    execute_federal_reserve_command("federal_funds_rate".to_string(), args).await
}

/// Get SOFR Rate data
#[tauri::command]
pub async fn get_sofr_rate(
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
    execute_federal_reserve_command("sofr_rate".to_string(), args).await
}

/// Get Treasury Rates data
#[tauri::command]
pub async fn get_treasury_rates(
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
    execute_federal_reserve_command("treasury_rates".to_string(), args).await
}

/// Get Yield Curve data
#[tauri::command]
pub async fn get_yield_curve(date: Option<String>) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(date) = date {
        args.push(date);
    }
    execute_federal_reserve_command("yield_curve".to_string(), args).await
}

/// Get Money Measures data
#[tauri::command]
pub async fn get_money_measures(
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
    execute_federal_reserve_command("money_measures".to_string(), args).await
}

/// Get Central Bank Holdings data
#[tauri::command]
pub async fn get_central_bank_holdings(
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
    execute_federal_reserve_command("central_bank_holdings".to_string(), args).await
}

/// Get Overnight Bank Funding Rate data
#[tauri::command]
pub async fn get_overnight_bank_funding_rate(
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
    execute_federal_reserve_command("overnight_bank_funding_rate".to_string(), args).await
}

/// Get comprehensive monetary data
#[tauri::command]
pub async fn get_comprehensive_monetary_data(
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
    execute_federal_reserve_command("comprehensive_monetary_data".to_string(), args).await
}

/// Get market overview
#[tauri::command]
pub async fn get_fed_market_overview() -> Result<String, String> {
    execute_federal_reserve_command("market_overview".to_string(), vec![]).await
}