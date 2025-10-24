// IMF (International Monetary Fund) data commands based on OpenBB imf provider
use crate::utils::python::execute_python_command;

/// Execute IMF Python script command
#[tauri::command]
pub async fn execute_imf_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "imf_data.py", &cmd_args)
}

/// Get economic indicators data (IRFCL and FSI)
#[tauri::command]
pub async fn get_imf_economic_indicators(app: tauri::AppHandle, 
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
    execute_imf_command(app, "economic_indicators".to_string(), args).await
}

/// Get direction of trade data (exports, imports, balance)
#[tauri::command]
pub async fn get_imf_direction_of_trade(app: tauri::AppHandle, 
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
    execute_imf_command(app, "direction_of_trade".to_string(), args).await
}

/// Get list of available IMF indicators
#[tauri::command]
pub async fn get_imf_available_indicators(app: tauri::AppHandle, 
    query: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(query) = query {
        args.push(query);
    }
    execute_imf_command(app, "available_indicators".to_string(), args).await
}

/// Get comprehensive economic data for a country
#[tauri::command]
pub async fn get_imf_comprehensive_economic_data(app: tauri::AppHandle, 
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
    execute_imf_command(app, "comprehensive_economic_data".to_string(), args).await
}

/// Get IMF reserves data (top line indicators)
#[tauri::command]
pub async fn get_imf_reserves_data(app: tauri::AppHandle, 
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
    execute_imf_command(app, "economic_indicators".to_string(), args).await
}

/// Get IMF trade data (exports, imports, balance)
#[tauri::command]
pub async fn get_imf_trade_summary(app: tauri::AppHandle, 
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
    execute_imf_command(app, "direction_of_trade".to_string(), args).await
}