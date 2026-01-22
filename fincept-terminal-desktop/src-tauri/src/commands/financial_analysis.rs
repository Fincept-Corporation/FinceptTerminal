// Financial Analysis Module Commands
// CFA-compliant statement analysis via worker pool or subprocess

use crate::utils::python::get_script_path;
use crate::python_runtime;
use serde::{Deserialize, Serialize};

/// Financial data input structure
#[derive(Debug, Serialize, Deserialize)]
pub struct FinancialDataInput {
    pub company: CompanyInput,
    pub period: PeriodInput,
    pub financials: FinancialsInput,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct CompanyInput {
    pub ticker: String,
    pub name: String,
    #[serde(default)]
    pub sector: Option<String>,
    #[serde(default)]
    pub industry: Option<String>,
    #[serde(default)]
    pub country: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PeriodInput {
    pub period_end: String,
    pub fiscal_year: i32,
    #[serde(default)]
    pub period_type: Option<String>,
    #[serde(default)]
    pub fiscal_period: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FinancialsInput {
    // Income Statement
    #[serde(default)]
    pub revenue: Option<f64>,
    #[serde(default)]
    pub cost_of_sales: Option<f64>,
    #[serde(default)]
    pub operating_income: Option<f64>,
    #[serde(default)]
    pub net_income: Option<f64>,
    #[serde(default)]
    pub interest_expense: Option<f64>,
    #[serde(default)]
    pub basic_eps: Option<f64>,
    #[serde(default)]
    pub diluted_eps: Option<f64>,
    
    // Balance Sheet
    #[serde(default)]
    pub total_assets: Option<f64>,
    #[serde(default)]
    pub current_assets: Option<f64>,
    #[serde(default)]
    pub cash_equivalents: Option<f64>,
    #[serde(default)]
    pub accounts_receivable: Option<f64>,
    #[serde(default)]
    pub inventory: Option<f64>,
    #[serde(default)]
    pub total_liabilities: Option<f64>,
    #[serde(default)]
    pub current_liabilities: Option<f64>,
    #[serde(default)]
    pub long_term_debt: Option<f64>,
    #[serde(default)]
    pub short_term_debt: Option<f64>,
    #[serde(default)]
    pub total_equity: Option<f64>,
    
    // Cash Flow
    #[serde(default)]
    pub operating_cash_flow: Option<f64>,
    #[serde(default)]
    pub capex: Option<f64>,
}

/// Analyze income statement
#[tauri::command]
pub async fn analyze_income_statement(
    app: tauri::AppHandle,
    data: FinancialDataInput,
) -> Result<String, String> {
    let json_data = serde_json::to_string(&data)
        .map_err(|e| format!("Failed to serialize data: {}", e))?;
    
    let args = vec!["analyze_income".to_string(), json_data];
    let script_path = get_script_path(&app, "Analytics/financial_analysis_cli.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Analyze balance sheet
#[tauri::command]
pub async fn analyze_balance_sheet(
    app: tauri::AppHandle,
    data: FinancialDataInput,
) -> Result<String, String> {
    let json_data = serde_json::to_string(&data)
        .map_err(|e| format!("Failed to serialize data: {}", e))?;
    
    let args = vec!["analyze_balance".to_string(), json_data];
    let script_path = get_script_path(&app, "Analytics/financial_analysis_cli.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Analyze cash flow statement
#[tauri::command]
pub async fn analyze_cash_flow(
    app: tauri::AppHandle,
    data: FinancialDataInput,
) -> Result<String, String> {
    let json_data = serde_json::to_string(&data)
        .map_err(|e| format!("Failed to serialize data: {}", e))?;
    
    let args = vec!["analyze_cashflow".to_string(), json_data];
    let script_path = get_script_path(&app, "Analytics/financial_analysis_cli.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Run comprehensive financial analysis
#[tauri::command]
pub async fn analyze_financial_statements(
    app: tauri::AppHandle,
    data: FinancialDataInput,
) -> Result<String, String> {
    let json_data = serde_json::to_string(&data)
        .map_err(|e| format!("Failed to serialize data: {}", e))?;
    
    let args = vec!["analyze_comprehensive".to_string(), json_data];
    let script_path = get_script_path(&app, "Analytics/financial_analysis_cli.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Get key financial metrics summary
#[tauri::command]
pub async fn get_financial_key_metrics(
    app: tauri::AppHandle,
    data: FinancialDataInput,
) -> Result<String, String> {
    let json_data = serde_json::to_string(&data)
        .map_err(|e| format!("Failed to serialize data: {}", e))?;
    
    let args = vec!["get_key_metrics".to_string(), json_data];
    let script_path = get_script_path(&app, "Analytics/financial_analysis_cli.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Convenience command for quick analysis with raw JSON
#[tauri::command]
pub async fn analyze_financial_json(
    app: tauri::AppHandle,
    command: String,
    json_data: String,
) -> Result<String, String> {
    let args = vec![command, json_data];
    let script_path = get_script_path(&app, "Analytics/financial_analysis_cli.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
