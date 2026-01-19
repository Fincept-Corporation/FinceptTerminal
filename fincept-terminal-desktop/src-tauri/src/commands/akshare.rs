use tauri::command;
use serde::{Deserialize, Serialize};
use std::process::Command;

#[derive(Debug, Serialize, Deserialize)]
pub struct AKShareResponse {
    pub success: bool,
    pub data: serde_json::Value,
    pub count: Option<usize>,
    pub timestamp: Option<i64>,
    pub error: Option<serde_json::Value>,
}

fn run_python_script(script: &str, endpoint: &str) -> Result<AKShareResponse, String> {
    let output = Command::new("python")
        .arg(format!("resources/scripts/{}", script))
        .arg(endpoint)
        .output()
        .map_err(|e| format!("Failed to execute Python script: {}", e))?;

    if output.status.success() {
        let result: AKShareResponse = serde_json::from_slice(&output.stdout)
            .map_err(|e| format!("Failed to parse JSON: {}", e))?;
        Ok(result)
    } else {
        let error_msg = String::from_utf8_lossy(&output.stderr).to_string();
        Err(format!("Script error: {}", error_msg))
    }
}

fn run_python_script_with_args(script: &str, args: Vec<&str>) -> Result<AKShareResponse, String> {
    let mut cmd = Command::new("python");
    cmd.arg(format!("resources/scripts/{}", script));
    for arg in args {
        cmd.arg(arg);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python script: {}", e))?;

    if output.status.success() {
        let result: AKShareResponse = serde_json::from_slice(&output.stdout)
            .map_err(|e| format!("Failed to parse JSON: {}", e))?;
        Ok(result)
    } else {
        let error_msg = String::from_utf8_lossy(&output.stderr).to_string();
        Err(format!("Script error: {}", error_msg))
    }
}

fn run_python_script_with_symbol(script: &str, endpoint: &str, symbol: &str) -> Result<AKShareResponse, String> {
    let output = Command::new("python")
        .arg(format!("resources/scripts/{}", script))
        .arg(endpoint)
        .arg(symbol)
        .output()
        .map_err(|e| format!("Failed to execute Python script: {}", e))?;

    if output.status.success() {
        let result: AKShareResponse = serde_json::from_slice(&output.stdout)
            .map_err(|e| format!("Failed to parse JSON: {}", e))?;
        Ok(result)
    } else {
        let error_msg = String::from_utf8_lossy(&output.stderr).to_string();
        Err(format!("Script error: {}", error_msg))
    }
}

// ==================== GENERIC AKSHARE COMMAND ====================
// This allows the frontend to call any endpoint from any script dynamically

#[command]
pub async fn akshare_query(script: String, endpoint: String, args: Option<Vec<String>>) -> Result<AKShareResponse, String> {
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

    let mut cmd_args = vec![endpoint.as_str()];
    let arg_refs: Vec<&str>;
    if let Some(ref extra_args) = args {
        arg_refs = extra_args.iter().map(|s| s.as_str()).collect();
        cmd_args.extend(arg_refs.iter());
    }

    run_python_script_with_args(&script, cmd_args)
}

// ==================== GET AVAILABLE ENDPOINTS ====================

#[command]
pub async fn akshare_get_endpoints(script: String) -> Result<AKShareResponse, String> {
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

    run_python_script(&script, "get_all_endpoints")
}

// ==================== STOCK MARKET DATA ====================

#[command]
pub async fn get_stock_cn_spot() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "stock_zh_spot")
}

#[command]
pub async fn get_stock_us_spot() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "stock_us_spot")
}

#[command]
pub async fn get_stock_hk_spot() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "stock_hk_spot")
}

#[command]
pub async fn get_stock_hot_rank() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "hot_rank")
}

#[command]
pub async fn get_stock_industry_list() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "industry_list")
}

#[command]
pub async fn get_stock_concept_list() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "concept_list")
}

#[command]
pub async fn get_stock_hsgt_data() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "hsgt")
}

#[command]
pub async fn get_stock_industry_pe() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "industry_pe")
}

// ==================== ECONOMICS ====================

#[command]
pub async fn get_macro_china_gdp() -> Result<AKShareResponse, String> {
    run_python_script("akshare_economics_china.py", "gdp")
}

#[command]
pub async fn get_macro_china_cpi() -> Result<AKShareResponse, String> {
    run_python_script("akshare_economics_china.py", "cpi")
}

#[command]
pub async fn get_macro_china_pmi() -> Result<AKShareResponse, String> {
    run_python_script("akshare_economics_china.py", "pmi")
}

// ==================== BONDS ====================

#[command]
pub async fn get_bond_china_yield() -> Result<AKShareResponse, String> {
    run_python_script("akshare_bonds.py", "bond_yield")
}

// ==================== FUNDS ====================

#[command]
pub async fn get_fund_etf_spot() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "etf_spot")
}

#[command]
pub async fn get_fund_ranking() -> Result<AKShareResponse, String> {
    run_python_script("akshare_data.py", "fund_rank")
}

// ==================== COMPANY INFORMATION ====================

#[command]
pub async fn get_stock_cn_info(symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol("akshare_company_info.py", "cn_basic", &symbol)
}

#[command]
pub async fn get_stock_cn_profile(symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol("akshare_company_info.py", "cn_profile", &symbol)
}

#[command]
pub async fn get_stock_hk_profile(symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol("akshare_company_info.py", "hk_profile", &symbol)
}

#[command]
pub async fn get_stock_us_info(symbol: String) -> Result<AKShareResponse, String> {
    run_python_script_with_symbol("akshare_company_info.py", "us_info", &symbol)
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
pub async fn build_akshare_database() -> Result<String, String> {
    let output = Command::new("python")
        .arg("resources/scripts/build_akshare_symbols_db.py")
        .output()
        .map_err(|e| format!("Failed to execute database build script: {}", e))?;

    if output.status.success() {
        Ok("Database built successfully".to_string())
    } else {
        let error_msg = String::from_utf8_lossy(&output.stderr).to_string();
        Err(format!("Database build failed: {}", error_msg))
    }
}
