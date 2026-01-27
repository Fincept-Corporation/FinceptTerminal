use tauri::command;
use serde::{Deserialize, Serialize};
use crate::python;

#[derive(Debug, Serialize, Deserialize)]
pub struct AKShareResponse {
    pub success: bool,
    pub data: serde_json::Value,
    pub count: Option<usize>,
    pub timestamp: Option<i64>,
    pub error: Option<serde_json::Value>,
}

fn run_python_script(app: &tauri::AppHandle, script: &str, endpoint: &str) -> Result<AKShareResponse, String> {
    let output = python::execute_sync(app, script, vec![endpoint.to_string()])?;
    serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse JSON: {}", e))
}

fn run_python_script_with_args(app: &tauri::AppHandle, script: &str, args: Vec<String>) -> Result<AKShareResponse, String> {
    let output = python::execute_sync(app, script, args)?;
    serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse JSON: {}", e))
}

fn run_python_script_with_symbol(app: &tauri::AppHandle, script: &str, endpoint: &str, symbol: &str) -> Result<AKShareResponse, String> {
    let output = python::execute_sync(app, script, vec![endpoint.to_string(), symbol.to_string()])?;
    serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse JSON: {}", e))
}

// ==================== GENERIC AKSHARE COMMAND ====================
// This allows the frontend to call any endpoint from any script dynamically

#[command]
pub async fn akshare_query(app: tauri::AppHandle, script: String, endpoint: String, args: Option<Vec<String>>) -> Result<AKShareResponse, String> {
    // Validate script name to prevent path traversal
    let valid_scripts = vec![
        "akshare_data.py",
        "akshare_bonds.py",
        "akshare_derivatives.py",
        "akshare_economics_china.py",
        "akshare_economics_global.py",
        "akshare_funds_expanded.py",
        "akshare_alternative.py",
        "akshare_analysis.py",
        "akshare_company_info.py",
        // Market data scripts
        "akshare_index.py",
        "akshare_futures.py",
        "akshare_currency.py",
        "akshare_energy.py",
        "akshare_crypto.py",
        "akshare_news.py",
        "akshare_reits.py",
        // Stock data scripts (8 batches covering 398 endpoints)
        "akshare_stocks_realtime.py",
        "akshare_stocks_historical.py",
        "akshare_stocks_financial.py",
        "akshare_stocks_holders.py",
        "akshare_stocks_funds.py",
        "akshare_stocks_board.py",
        "akshare_stocks_margin.py",
        "akshare_stocks_hot.py",
    ];

    if !valid_scripts.contains(&script.as_str()) {
        return Err(format!("Invalid script: {}. Valid scripts: {:?}", script, valid_scripts));
    }

    let mut cmd_args = vec![endpoint];
    if let Some(extra_args) = args {
        cmd_args.extend(extra_args);
    }

    run_python_script_with_args(&app, &script, cmd_args)
}

// ==================== GET AVAILABLE ENDPOINTS ====================

#[command]
pub async fn akshare_get_endpoints(app: tauri::AppHandle, script: String) -> Result<AKShareResponse, String> {
    let valid_scripts = vec![
        "akshare_data.py",
        "akshare_bonds.py",
        "akshare_derivatives.py",
        "akshare_economics_china.py",
        "akshare_economics_global.py",
        "akshare_funds_expanded.py",
        "akshare_alternative.py",
        "akshare_analysis.py",
        "akshare_company_info.py",
        // Market data scripts
        "akshare_index.py",
        "akshare_futures.py",
        "akshare_currency.py",
        "akshare_energy.py",
        "akshare_crypto.py",
        "akshare_news.py",
        "akshare_reits.py",
        // Stock data scripts (8 batches covering 398 endpoints)
        "akshare_stocks_realtime.py",
        "akshare_stocks_historical.py",
        "akshare_stocks_financial.py",
        "akshare_stocks_holders.py",
        "akshare_stocks_funds.py",
        "akshare_stocks_board.py",
        "akshare_stocks_margin.py",
        "akshare_stocks_hot.py",
    ];

    if !valid_scripts.contains(&script.as_str()) {
        return Err(format!("Invalid script: {}", script));
    }

    run_python_script(&app, &script, "get_all_endpoints")
}

// ==================== STOCK MARKET DATA ====================

#[command]
pub async fn get_stock_cn_spot(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "stock_zh_spot")
}

#[command]
pub async fn get_stock_us_spot(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "stock_us_spot")
}

#[command]
pub async fn get_stock_hk_spot(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "stock_hk_spot")
}

#[command]
pub async fn get_stock_hot_rank(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "hot_rank")
}

#[command]
pub async fn get_stock_industry_list(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "industry_list")
}

#[command]
pub async fn get_stock_concept_list(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "concept_list")
}

#[command]
pub async fn get_stock_hsgt_data(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "hsgt")
}

#[command]
pub async fn get_stock_industry_pe(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "industry_pe")
}

// ==================== ECONOMICS ====================

#[command]
pub async fn get_macro_china_gdp(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_economics_china.py", "gdp")
}

#[command]
pub async fn get_macro_china_cpi(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_economics_china.py", "cpi")
}

#[command]
pub async fn get_macro_china_pmi(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_economics_china.py", "pmi")
}

// ==================== BONDS ====================

#[command]
pub async fn get_bond_china_yield(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_bonds.py", "bond_yield")
}

// ==================== FUNDS ====================

#[command]
pub async fn get_fund_etf_spot(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "etf_spot")
}

#[command]
pub async fn get_fund_ranking(app: tauri::AppHandle) -> Result<AKShareResponse, String> {
    run_python_script(&app, "akshare_data.py", "fund_rank")
}

// ==================== COMPANY INFORMATION ====================

#[command]
pub async fn get_stock_cn_info(app: tauri::AppHandle, symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol(&app, "akshare_company_info.py", "cn_basic", &symbol)
}

#[command]
pub async fn get_stock_cn_profile(app: tauri::AppHandle, symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol(&app, "akshare_company_info.py", "cn_profile", &symbol)
}

#[command]
pub async fn get_stock_hk_profile(app: tauri::AppHandle, symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol(&app, "akshare_company_info.py", "hk_profile", &symbol)
}

#[command]
pub async fn get_stock_us_info(app: tauri::AppHandle, symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol(&app, "akshare_company_info.py", "us_info", &symbol)
}

// ==================== DATABASE MANAGEMENT ====================

#[derive(Debug, Serialize, Deserialize)]
pub struct DbStatusResponse {
    pub exists: bool,
    pub last_updated: Option<String>,
}

#[command]
pub async fn check_akshare_db_status() -> Result<DbStatusResponse, String> {
    use std::path::PathBuf;
    use std::fs;

    // Get AppData path
    let appdata = std::env::var("APPDATA")
        .or_else(|_| std::env::var("HOME").map(|h| format!("{}/.config", h)))
        .map_err(|e| format!("Failed to get AppData path: {}", e))?;

    let db_path = PathBuf::from(&appdata)
        .join("fincept-terminal")
        .join("akshare_symbols.db");

    let exists = db_path.exists();
    let last_updated = if exists {
        fs::metadata(&db_path)
            .ok()
            .and_then(|m| m.modified().ok())
            .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
            .map(|d| {
                let secs = d.as_secs();
                chrono::NaiveDateTime::from_timestamp_opt(secs as i64, 0)
                    .map(|dt| dt.format("%Y-%m-%d %H:%M:%S").to_string())
                    .unwrap_or_default()
            })
    } else {
        None
    };

    Ok(DbStatusResponse {
        exists,
        last_updated,
    })
}

#[command]
pub async fn build_akshare_database(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "build_akshare_symbols_db.py", vec![])
        .map(|_| "Database built successfully".to_string())
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TranslateResponse {
    pub success: bool,
    pub translations: Option<Vec<Translation>>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Translation {
    pub original: String,
    pub translated: String,
    pub detected_lang: String,
}

#[command]
pub async fn translate_batch(app: tauri::AppHandle, texts: Vec<String>, target_lang: String) -> Result<TranslateResponse, String> {
    let texts_json = serde_json::to_string(&texts)
        .map_err(|e| format!("Failed to serialize texts: {}", e))?;

    let output = python::execute_sync(
        &app,
        "translate_text.py",
        vec!["batch".to_string(), texts_json, "auto".to_string(), target_lang]
    )?;

    serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse translation response: {}", e))
}
