// IMF (International Monetary Fund) data commands based on OpenBB imf provider
use std::process::Command;

/// Execute IMF Python script command
#[tauri::command]
pub async fn execute_imf_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("imf_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "IMF script not found at: {}",
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
        .map_err(|e| format!("Failed to execute IMF command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("IMF command failed: {}", stderr))
    }
}

/// Get economic indicators data (IRFCL and FSI)
#[tauri::command]
pub async fn get_imf_economic_indicators(
    countries: Option<String>,
    symbols: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
    sector: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    }
    if let Some(symbols) = symbols {
        args.push(symbols);
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
    if let Some(sector) = sector {
        args.push(sector);
    }
    execute_imf_command("economic_indicators".to_string(), args).await
}

/// Get direction of trade data (exports, imports, balance)
#[tauri::command]
pub async fn get_imf_direction_of_trade(
    countries: Option<String>,
    counterparts: Option<String>,
    direction: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    }
    if let Some(counterparts) = counterparts {
        args.push(counterparts);
    }
    if let Some(direction) = direction {
        args.push(direction);
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
    execute_imf_command("direction_of_trade".to_string(), args).await
}

/// Get list of available IMF indicators
#[tauri::command]
pub async fn get_imf_available_indicators(
    query: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(query) = query {
        args.push(query);
    }
    execute_imf_command("available_indicators".to_string(), args).await
}

/// Get comprehensive economic data for a country
#[tauri::command]
pub async fn get_imf_comprehensive_economic_data(
    country: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    args.push(country);
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_imf_command("comprehensive_economic_data".to_string(), args).await
}

/// Get IMF reserves data (top line indicators)
#[tauri::command]
pub async fn get_imf_reserves_data(
    countries: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries) = countries {
        args.push(countries);
    } else {
        args.push("all".to_string());
    }
    args.push("irfcl_top_lines".to_string());
    if let Some(frequency) = frequency {
        args.push(frequency);
    } else {
        args.push("quarter".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_imf_command("economic_indicators".to_string(), args).await
}

/// Get IMF trade data (exports, imports, balance)
#[tauri::command]
pub async fn get_imf_trade_summary(
    country: String,
    counterpart: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    args.push(country);
    if let Some(counterpart) = counterpart {
        args.push(counterpart);
    } else {
        args.push("all".to_string());
    }
    args.push("all".to_string()); // direction
    if let Some(frequency) = frequency {
        args.push(frequency);
    } else {
        args.push("quarter".to_string());
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_imf_command("direction_of_trade".to_string(), args).await
}