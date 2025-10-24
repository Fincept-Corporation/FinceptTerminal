// US Government data commands based on OpenBB government_us provider
use crate::utils::python::execute_python_command;

/// Execute US Government Python script command
#[tauri::command]
pub async fn execute_government_us_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "government_us_data.py", &cmd_args)
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