// SEC (Securities and Exchange Commission) data commands based on OpenBB sec provider
use crate::utils::python::execute_python_command;

/// Execute SEC Python script command
#[tauri::command]
pub async fn execute_sec_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "sec_data.py", &cmd_args)
}

// COMPANY FILINGS COMMANDS

/// Get company filings from SEC database
#[tauri::command]
pub async fn get_sec_company_filings(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    form_type: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(form_type) = form_type {
        args.push(form_type);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_sec_command(app, "company_filings".to_string(), args).await
}

/// Get CIK (Central Index Key) for a company symbol
#[tauri::command]
pub async fn get_sec_cik_map(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_sec_command(app, "cik_map".to_string(), args).await
}

/// Get symbol mapping for a CIK
#[tauri::command]
pub async fn get_sec_symbol_map(app: tauri::AppHandle, 
    cik: String,
) -> Result<String, String> {
    let args = vec![cik];
    execute_sec_command(app, "symbol_map".to_string(), args).await
}

// FILING CONTENT COMMANDS

/// Get content of a specific SEC filing
#[tauri::command]
pub async fn get_sec_filing_content(app: tauri::AppHandle, 
    filing_url: String,
) -> Result<String, String> {
    let args = vec![filing_url];
    execute_sec_command(app, "filing_content".to_string(), args).await
}

/// Parse HTML content from SEC filing
#[tauri::command]
pub async fn parse_sec_filing_html(app: tauri::AppHandle, 
    html_content: String,
) -> Result<String, String> {
    let args = vec![html_content];
    execute_sec_command(app, "parse_filing_html".to_string(), args).await
}

// INSIDER TRADING COMMANDS

/// Get insider trading data (Form 4 filings)
#[tauri::command]
pub async fn get_sec_insider_trading(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_sec_command(app, "insider_trading".to_string(), args).await
}

// INSTITUTIONAL OWNERSHIP COMMANDS

/// Get institutional ownership data (Form 13F filings)
#[tauri::command]
pub async fn get_sec_institutional_ownership(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("50".to_string());
    }
    execute_sec_command(app, "institutional_ownership".to_string(), args).await
}

// COMPANY SEARCH COMMANDS

/// Search for companies in SEC database
#[tauri::command]
pub async fn search_sec_companies(app: tauri::AppHandle, 
    query: String,
    is_fund: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(is_fund) = is_fund {
        args.push(is_fund.to_string());
    }
    execute_sec_command(app, "search_companies".to_string(), args).await
}

/// Search for ETFs and mutual funds
#[tauri::command]
pub async fn search_sec_etfs_mutual_funds(app: tauri::AppHandle, 
    query: String,
) -> Result<String, String> {
    let args = vec![query];
    execute_sec_command(app, "search_etfs_mutual_funds".to_string(), args).await
}

// FORM TYPE HELPERS

/// Get list of available SEC form types
#[tauri::command]
pub async fn get_sec_available_form_types(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_sec_command(app, "available_form_types".to_string(), vec![]).await
}

// COMPANY FACTS COMMANDS

/// Get company facts data from SEC API
#[tauri::command]
pub async fn get_sec_company_facts(app: tauri::AppHandle, 
    cik: String,
) -> Result<String, String> {
    let args = vec![cik];
    execute_sec_command(app, "company_facts".to_string(), args).await
}

/// Get financial statements data using company facts
#[tauri::command]
pub async fn get_sec_financial_statements(app: tauri::AppHandle, 
    cik: String,
    taxonomy: Option<String>,
    fact_list: Option<String>,
) -> Result<String, String> {
    let mut args = vec![cik];
    if let Some(taxonomy) = taxonomy {
        args.push(taxonomy);
    } else {
        args.push("us-gaap".to_string());
    }
    if let Some(fact_list) = fact_list {
        args.push(fact_list);
    }
    execute_sec_command(app, "financial_statements".to_string(), args).await
}

// COMPREHENSIVE DATA COMMANDS

/// Get comprehensive company overview including filings, facts, and insider data
#[tauri::command]
pub async fn get_sec_company_overview(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    execute_sec_command(app, "company_overview".to_string(), args).await
}

/// Get all filings of a specific form type within date range
#[tauri::command]
pub async fn get_sec_filings_by_form_type(app: tauri::AppHandle, 
    form_type: String,
    start_date: Option<String>,
    end_date: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![form_type];
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_sec_command(app, "filings_by_form_type".to_string(), args).await
}

// SECURITIES SEARCH COMMANDS

/// Search for equity securities
#[tauri::command]
pub async fn search_sec_equities(app: tauri::AppHandle, 
    query: String,
    is_fund: Option<bool>,
    use_cache: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(is_fund) = is_fund {
        args.push(is_fund.to_string());
    }
    if let Some(use_cache) = use_cache {
        args.push(use_cache.to_string());
    }
    execute_sec_command(app, "search_companies".to_string(), args).await
}

/// Search for mutual funds and ETFs
#[tauri::command]
pub async fn search_sec_mutual_funds_etfs(app: tauri::AppHandle, 
    query: String,
    use_cache: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(use_cache) = use_cache {
        args.push(use_cache.to_string());
    }
    execute_sec_command(app, "search_etfs_mutual_funds".to_string(), args).await
}

// ADVANCED SEC DATA COMMANDS

/// Get SEC filing metadata and document URLs
#[tauri::command]
pub async fn get_sec_filing_metadata(app: tauri::AppHandle, 
    filing_url: String,
    use_cache: Option<bool>,
) -> Result<String, String> {
    let mut args = vec![filing_url];
    if let Some(use_cache) = use_cache {
        args.push(use_cache.to_string());
    }
    execute_sec_command(app, "filing_content".to_string(), args).await
}

/// Get recent 8-K filings (current reports)
#[tauri::command]
pub async fn get_sec_recent_current_reports(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("20".to_string());
    }
    execute_sec_command(app, "company_filings".to_string(), args).await
}

/// Get annual reports (10-K filings)
#[tauri::command]
pub async fn get_sec_annual_reports(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("5".to_string());
    }
    execute_sec_command(app, "company_filings".to_string(), args).await
}

/// Get quarterly reports (10-Q filings)
#[tauri::command]
pub async fn get_sec_quarterly_reports(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("8".to_string());
    }
    execute_sec_command(app, "company_filings".to_string(), args).await
}

/// Get registration statements (S-1, S-3 filings)
#[tauri::command]
pub async fn get_sec_registration_statements(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    form_types: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(form_types) = form_types {
        args.push(form_types);
    } else {
        args.push("S-1,S-3".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_sec_command(app, "company_filings".to_string(), args).await
}

/// Get beneficial ownership reports (Form 3, 4, 5)
#[tauri::command]
pub async fn get_sec_beneficial_ownership(app: tauri::AppHandle, 
    symbol: Option<String>,
    cik: Option<String>,
    form_types: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(symbol) = symbol {
        args.push(symbol);
    }
    if let Some(cik) = cik {
        args.push(cik);
    }
    if let Some(form_types) = form_types {
        args.push(form_types);
    } else {
        args.push("3,4,5".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("20".to_string());
    }
    execute_sec_command(app, "company_filings".to_string(), args).await
}