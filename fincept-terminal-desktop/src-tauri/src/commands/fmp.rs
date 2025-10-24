// FMP (Financial Modeling Prep) data commands based on OpenBB fmp provider
use crate::utils::python::execute_python_command;

/// Execute FMP Python script command
#[tauri::command]
pub async fn execute_fmp_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "fmp_data.py", &cmd_args)
}

// EQUITY DATA COMMANDS

/// Get real-time stock quotes for multiple symbols
#[tauri::command]
pub async fn get_fmp_equity_quote(app: tauri::AppHandle, 
    symbols: String,
) -> Result<String, String> {
    let args = vec![symbols];
    execute_fmp_command(app, "equity_quote".to_string(), args).await
}

/// Get company profile and basic information
#[tauri::command]
pub async fn get_fmp_company_profile(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_fmp_command(app, "company_profile".to_string(), args).await
}

/// Get historical price data for a symbol
#[tauri::command]
pub async fn get_fmp_historical_prices(app: tauri::AppHandle, 
    symbol: String,
    start_date: Option<String>,
    end_date: Option<String>,
    interval: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    if let Some(interval) = interval {
        args.push(interval);
    } else {
        args.push("1d".to_string());
    }
    execute_fmp_command(app, "historical_prices".to_string(), args).await
}

// FINANCIAL STATEMENTS COMMANDS

/// Get income statement data
#[tauri::command]
pub async fn get_fmp_income_statement(app: tauri::AppHandle, 
    symbol: String,
    period: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(period) = period {
        args.push(period);
    } else {
        args.push("annual".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_fmp_command(app, "income_statement".to_string(), args).await
}

/// Get balance sheet data
#[tauri::command]
pub async fn get_fmp_balance_sheet(app: tauri::AppHandle, 
    symbol: String,
    period: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(period) = period {
        args.push(period);
    } else {
        args.push("annual".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_fmp_command(app, "balance_sheet".to_string(), args).await
}

/// Get cash flow statement data
#[tauri::command]
pub async fn get_fmp_cash_flow_statement(app: tauri::AppHandle, 
    symbol: String,
    period: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(period) = period {
        args.push(period);
    } else {
        args.push("annual".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_fmp_command(app, "cash_flow_statement".to_string(), args).await
}

/// Get financial ratios data
#[tauri::command]
pub async fn get_fmp_financial_ratios(app: tauri::AppHandle, 
    symbol: String,
    period: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(period) = period {
        args.push(period);
    } else {
        args.push("annual".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_fmp_command(app, "financial_ratios".to_string(), args).await
}

/// Get key metrics data
#[tauri::command]
pub async fn get_fmp_key_metrics(app: tauri::AppHandle, 
    symbol: String,
    period: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(period) = period {
        args.push(period);
    } else {
        args.push("annual".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_fmp_command(app, "key_metrics".to_string(), args).await
}

// MARKET DATA COMMANDS

/// Get market snapshots and indices
#[tauri::command]
pub async fn get_fmp_market_snapshots(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_fmp_command(app, "market_snapshots".to_string(), vec![]).await
}

/// Get current treasury rates
#[tauri::command]
pub async fn get_fmp_treasury_rates(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_fmp_command(app, "treasury_rates".to_string(), vec![]).await
}

// ETF DATA COMMANDS

/// Get ETF information
#[tauri::command]
pub async fn get_fmp_etf_info(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_fmp_command(app, "etf_info".to_string(), args).await
}

/// Get ETF holdings data
#[tauri::command]
pub async fn get_fmp_etf_holdings(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_fmp_command(app, "etf_holdings".to_string(), args).await
}

// CRYPTO DATA COMMANDS

/// Get list of available cryptocurrencies
#[tauri::command]
pub async fn get_fmp_crypto_list(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_fmp_command(app, "crypto_list".to_string(), vec![]).await
}

/// Get historical cryptocurrency prices
#[tauri::command]
pub async fn get_fmp_crypto_historical(app: tauri::AppHandle, 
    symbol: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(start_date) = start_date {
        args.push(start_date);
    }
    if let Some(end_date) = end_date {
        args.push(end_date);
    }
    execute_fmp_command(app, "crypto_historical".to_string(), args).await
}

// NEWS DATA COMMANDS

/// Get news for a specific company
#[tauri::command]
pub async fn get_fmp_company_news(app: tauri::AppHandle, 
    symbol: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("50".to_string());
    }
    execute_fmp_command(app, "company_news".to_string(), args).await
}

/// Get general financial news
#[tauri::command]
pub async fn get_fmp_general_news(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_fmp_command(app, "general_news".to_string(), vec![]).await
}

// ECONOMIC DATA COMMANDS

/// Get economic calendar data
#[tauri::command]
pub async fn get_fmp_economic_calendar(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_fmp_command(app, "economic_calendar".to_string(), vec![]).await
}

// INSIDER TRADING COMMANDS

/// Get insider trading data for a symbol
#[tauri::command]
pub async fn get_fmp_insider_trading(app: tauri::AppHandle, 
    symbol: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_fmp_command(app, "insider_trading".to_string(), args).await
}

// INSTITUTIONAL OWNERSHIP COMMANDS

/// Get institutional ownership data
#[tauri::command]
pub async fn get_fmp_institutional_ownership(app: tauri::AppHandle, 
    symbol: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("100".to_string());
    }
    execute_fmp_command(app, "institutional_ownership".to_string(), args).await
}

// COMPREHENSIVE DATA COMMANDS

/// Get comprehensive company overview including profile, quotes, and key metrics
#[tauri::command]
pub async fn get_fmp_company_overview(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_fmp_command(app, "company_overview".to_string(), args).await
}

/// Get all financial statements for a company
#[tauri::command]
pub async fn get_fmp_financial_statements(app: tauri::AppHandle, 
    symbol: String,
    period: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(period) = period {
        args.push(period);
    } else {
        args.push("annual".to_string());
    }
    if let Some(limit) = limit {
        args.push(limit.to_string());
    } else {
        args.push("5".to_string());
    }
    execute_fmp_command(app, "financial_statements".to_string(), args).await
}

/// Get multiple stock quotes efficiently
#[tauri::command]
pub async fn get_fmp_market_quotes(app: tauri::AppHandle, 
    symbols: String,
) -> Result<String, String> {
    let args = vec![symbols];
    execute_fmp_command(app, "equity_quote".to_string(), args).await
}

/// Get stock price performance data
#[tauri::command]
pub async fn get_fmp_price_performance(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    // This would use the price_performance endpoint
    // For now, we'll use historical prices as a proxy
    let args = vec![
        symbol,
        (chrono::Utc::now() - chrono::Duration::days(30)).format("%Y-%m-%d").to_string(),
        chrono::Utc::now().format("%Y-%m-%d").to_string(),
        "1d".to_string(),
    ];
    execute_fmp_command(app, "historical_prices".to_string(), args).await
}

/// Get stock screener data
#[tauri::command]
pub async fn get_fmp_stock_screener(app: tauri::AppHandle, 
    market_cap_more_than: Option<f64>,
    market_cap_lower_than: Option<f64>,
    price_more_than: Option<f64>,
    price_lower_than: Option<f64>,
    beta_more_than: Option<f64>,
    beta_lower_than: Option<f64>,
    volume_more_than: Option<i64>,
    volume_lower_than: Option<i64>,
    dividend_more_than: Option<f64>,
    is_etf: Option<bool>,
    is_actively_trading: Option<bool>,
    sector: Option<String>,
    industry: Option<String>,
    country: Option<String>,
    exchange: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(value) = market_cap_more_than {
        args.push(format!("marketCapMoreThan={}", value));
    }
    if let Some(value) = market_cap_lower_than {
        args.push(format!("marketCapLowerThan={}", value));
    }
    if let Some(value) = price_more_than {
        args.push(format!("priceMoreThan={}", value));
    }
    if let Some(value) = price_lower_than {
        args.push(format!("priceLowerThan={}", value));
    }
    if let Some(value) = beta_more_than {
        args.push(format!("betaMoreThan={}", value));
    }
    if let Some(value) = beta_lower_than {
        args.push(format!("betaLowerThan={}", value));
    }
    if let Some(value) = volume_more_than {
        args.push(format!("volumeMoreThan={}", value));
    }
    if let Some(value) = volume_lower_than {
        args.push(format!("volumeLowerThan={}", value));
    }
    if let Some(value) = dividend_more_than {
        args.push(format!("dividendMoreThan={}", value));
    }
    if let Some(value) = is_etf {
        args.push(format!("isEtf={}", value));
    }
    if let Some(value) = is_actively_trading {
        args.push(format!("isActivelyTrading={}", value));
    }
    if let Some(value) = sector {
        args.push(format!("sector={}", value));
    }
    if let Some(value) = industry {
        args.push(format!("industry={}", value));
    }
    if let Some(value) = country {
        args.push(format!("country={}", value));
    }
    if let Some(value) = exchange {
        args.push(format!("exchange={}", value));
    }
    if let Some(value) = limit {
        args.push(format!("limit={}", value));
    }

    if args.is_empty() {
        return Err("At least one screener parameter must be provided".to_string());
    }

    let query_string = args.join("&");
    execute_fmp_command(app, "stock_screener".to_string(), vec![query_string]).await
}

/// Get stock price targets and analyst data
#[tauri::command]
pub async fn get_fmp_price_targets(app: tauri::AppHandle, 
    symbol: String,
) -> Result<String, String> {
    let args = vec![symbol];
    execute_fmp_command(app, "price_targets".to_string(), args).await
}