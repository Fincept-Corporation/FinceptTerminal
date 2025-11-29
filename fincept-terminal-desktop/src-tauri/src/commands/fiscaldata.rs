// U.S. Treasury FiscalData API commands
#![allow(dead_code)]
use crate::utils::python::execute_python_command;

/// Execute FiscalData Python script command
#[tauri::command]
pub async fn execute_fiscaldata_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    execute_python_command(&app, "fiscaldata_data.py", &cmd_args)
}

/// Get Debt to the Penny data - Daily public debt outstanding
#[tauri::command]
pub async fn get_fiscaldata_debt_to_penny(
    app: tauri::AppHandle,
    fields: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
    all_pages: Option<bool>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(f) = fields {
        args.push(format!("--fields={}", f));
    }

    if let Some(sd) = start_date {
        args.push(format!("--start-date={}", sd));
    }

    if let Some(ed) = end_date {
        args.push(format!("--end-date={}", ed));
    }

    if let Some(l) = limit {
        args.push(format!("--limit={}", l));
    }

    if all_pages.unwrap_or(false) {
        args.push("--all".to_string());
    }

    execute_fiscaldata_command(app, "debt-to-penny".to_string(), args).await
}

/// Get Average Interest Rates data - Average interest rates on Treasury securities
#[tauri::command]
pub async fn get_fiscaldata_avg_interest_rates(
    app: tauri::AppHandle,
    start_date: Option<String>,
    end_date: Option<String>,
    security_type: Option<String>,
    limit: Option<i32>,
    all_pages: Option<bool>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(sd) = start_date {
        args.push(format!("--start-date={}", sd));
    }

    if let Some(ed) = end_date {
        args.push(format!("--end-date={}", ed));
    }

    if let Some(st) = security_type {
        args.push(format!("--security-type={}", st));
    }

    if let Some(l) = limit {
        args.push(format!("--limit={}", l));
    }

    if all_pages.unwrap_or(false) {
        args.push("--all".to_string());
    }

    execute_fiscaldata_command(app, "avg-interest-rates".to_string(), args).await
}

/// Get Interest Expense data - Monthly interest expense on Treasury securities
#[tauri::command]
pub async fn get_fiscaldata_interest_expense(
    app: tauri::AppHandle,
    start_date: Option<String>,
    end_date: Option<String>,
    expense_category: Option<String>,
    limit: Option<i32>,
    all_pages: Option<bool>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(sd) = start_date {
        args.push(format!("--start-date={}", sd));
    }

    if let Some(ed) = end_date {
        args.push(format!("--end-date={}", ed));
    }

    if let Some(ec) = expense_category {
        args.push(format!("--expense-category={}", ec));
    }

    if let Some(l) = limit {
        args.push(format!("--limit={}", l));
    }

    if all_pages.unwrap_or(false) {
        args.push("--all".to_string());
    }

    execute_fiscaldata_command(app, "interest-expense".to_string(), args).await
}

/// Get available datasets from FiscalData
#[tauri::command]
pub async fn get_fiscaldata_datasets(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_fiscaldata_command(app, "datasets".to_string(), vec![]).await
}
