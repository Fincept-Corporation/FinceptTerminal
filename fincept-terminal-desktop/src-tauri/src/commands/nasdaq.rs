// NASDAQ data commands based on OpenBB nasdaq provider
use crate::utils::python::execute_python_command;
use chrono::Datelike;

/// Execute NASDAQ Python script command
#[tauri::command]
pub async fn execute_nasdaq_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "nasdaq_data.py", &cmd_args)
}

// EQUITY SEARCH AND SCREENER COMMANDS

/// Search for equities in NASDAQ directory
#[tauri::command]
pub async fn search_nasdaq_equities(app: tauri::AppHandle, 
    query: Option<String>,
    is_etf: Option<bool>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(q) = query {
        args.push(q);
    }
    if let Some(etf) = is_etf {
        args.push(etf.to_string());
    }
    execute_nasdaq_command(app, "search_equities".to_string(), args).await
}

/// Get equity screener results with filters
#[tauri::command]
pub async fn get_nasdaq_equity_screener(app: tauri::AppHandle, 
    exchange: Option<String>,
    market_cap: Option<String>,
    sector: Option<String>,
    country: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(ex) = exchange {
        args.push(ex);
    } else {
        args.push("all".to_string());
    }
    if let Some(mc) = market_cap {
        args.push(mc);
    } else {
        args.push("all".to_string());
    }
    if let Some(sec) = sector {
        args.push(sec);
    } else {
        args.push("all".to_string());
    }
    if let Some(c) = country {
        args.push(c);
    } else {
        args.push("all".to_string());
    }
    if let Some(lim) = limit {
        args.push(lim.to_string());
    }
    execute_nasdaq_command(app, "equity_screener".to_string(), args).await
}

/// Get top performing stocks
#[tauri::command]
pub async fn get_nasdaq_top_performers(app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    get_nasdaq_equity_screener(
        app,
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        limit
    ).await
}

/// Get stocks by market cap category
#[tauri::command]
pub async fn get_nasdaq_stocks_by_market_cap(app: tauri::AppHandle,
    market_cap: String,
    limit: Option<i32>,
) -> Result<String, String> {
    get_nasdaq_equity_screener(
        app,
        Some("all".to_string()),
        Some(market_cap),
        Some("all".to_string()),
        Some("all".to_string()),
        limit
    ).await
}

/// Get stocks by sector
#[tauri::command]
pub async fn get_nasdaq_stocks_by_sector(app: tauri::AppHandle,
    sector: String,
    limit: Option<i32>,
) -> Result<String, String> {
    get_nasdaq_equity_screener(
        app,
        Some("all".to_string()),
        Some("all".to_string()),
        Some(sector),
        Some("all".to_string()),
        limit
    ).await
}

/// Get stocks by exchange
#[tauri::command]
pub async fn get_nasdaq_stocks_by_exchange(app: tauri::AppHandle,
    exchange: String,
    limit: Option<i32>,
) -> Result<String, String> {
    get_nasdaq_equity_screener(
        app,
        Some(exchange),
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        limit
    ).await
}

/// Search for ETFs
#[tauri::command]
pub async fn search_nasdaq_etfs(app: tauri::AppHandle,
    query: Option<String>,
) -> Result<String, String> {
    search_nasdaq_equities(app, query, Some(true)).await
}

// CALENDAR COMMANDS

/// Get dividend calendar
#[tauri::command]
pub async fn get_nasdaq_dividend_calendar(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_nasdaq_command(app, "dividend_calendar".to_string(), args).await
}

/// Get upcoming dividends
#[tauri::command]
pub async fn get_nasdaq_upcoming_dividends(app: tauri::AppHandle,
    days: Option<i32>,
) -> Result<String, String> {
    let days_ahead = days.unwrap_or(7);
    let end_date = chrono::Utc::now() + chrono::Duration::days(days_ahead as i64);
    get_nasdaq_dividend_calendar(
        app,
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string()),
        Some(end_date.format("%Y-%m-%d").to_string())
    ).await
}

/// Get earnings calendar
#[tauri::command]
pub async fn get_nasdaq_earnings_calendar(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_nasdaq_command(app, "earnings_calendar".to_string(), args).await
}

/// Get upcoming earnings
#[tauri::command]
pub async fn get_nasdaq_upcoming_earnings(app: tauri::AppHandle,
    days: Option<i32>,
) -> Result<String, String> {
    let days_ahead = days.unwrap_or(7);
    let end_date = chrono::Utc::now() + chrono::Duration::days(days_ahead as i64);
    get_nasdaq_earnings_calendar(
        app,
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string()),
        Some(end_date.format("%Y-%m-%d").to_string())
    ).await
}

/// Get IPO calendar
#[tauri::command]
pub async fn get_nasdaq_ipo_calendar(app: tauri::AppHandle, 
    status: Option<String>,
    is_spo: Option<bool>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(s) = status {
        args.push(s);
    } else {
        args.push("priced".to_string());
    }
    if let Some(spo) = is_spo {
        args.push(spo.to_string());
    } else {
        args.push("false".to_string());
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_nasdaq_command(app, "ipo_calendar".to_string(), args).await
}

/// Get upcoming IPOs
#[tauri::command]
pub async fn get_nasdaq_upcoming_ipos(app: tauri::AppHandle, ) -> Result<String, String> {
    get_nasdaq_ipo_calendar(app, Some("upcoming".to_string()), Some(false), None, None).await
}

/// Get recent IPOs
#[tauri::command]
pub async fn get_nasdaq_recent_ipos(app: tauri::AppHandle,
    days: Option<i32>,
) -> Result<String, String> {
    let days_back = days.unwrap_or(30);
    let start_date = chrono::Utc::now() - chrono::Duration::days(days_back as i64);
    get_nasdaq_ipo_calendar(
        app,
        Some("priced".to_string()),
        Some(false),
        Some(start_date.format("%Y-%m-%d").to_string()),
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string())
    ).await
}

/// Get filed IPOs
#[tauri::command]
pub async fn get_nasdaq_filed_ipos(app: tauri::AppHandle, ) -> Result<String, String> {
    get_nasdaq_ipo_calendar(app, Some("filed".to_string()), Some(false), None, None).await
}

/// Get withdrawn IPOs
#[tauri::command]
pub async fn get_nasdaq_withdrawn_ipos(app: tauri::AppHandle, ) -> Result<String, String> {
    get_nasdaq_ipo_calendar(app, Some("withdrawn".to_string()), Some(false), None, None).await
}

/// Get secondary public offerings (SPOs)
#[tauri::command]
pub async fn get_nasdaq_spo_calendar(app: tauri::AppHandle,
    status: Option<String>,
) -> Result<String, String> {
    get_nasdaq_ipo_calendar(app, status.or_else(|| Some("priced".to_string())), Some(true), None, None).await
}

// ECONOMIC CALENDAR COMMANDS

/// Get economic calendar
#[tauri::command]
pub async fn get_nasdaq_economic_calendar(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
    country: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    if let Some(c) = country {
        args.push(c);
    }
    execute_nasdaq_command(app, "economic_calendar".to_string(), args).await
}

/// Get today's economic events
#[tauri::command]
pub async fn get_nasdaq_today_economic_events(app: tauri::AppHandle,
    country: Option<String>,
) -> Result<String, String> {
    get_nasdaq_economic_calendar(
        app,
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string()),
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string()),
        country
    ).await
}

/// Get this week's economic events
#[tauri::command]
pub async fn get_nasdaq_week_economic_events(app: tauri::AppHandle,
    country: Option<String>,
) -> Result<String, String> {
    let start_date = chrono::Utc::now() - chrono::Duration::days(chrono::Utc::now().weekday().num_days_from_monday() as i64);
    let end_date = start_date + chrono::Duration::days(4);
    get_nasdaq_economic_calendar(
        app,
        Some(start_date.format("%Y-%m-%d").to_string()),
        Some(end_date.format("%Y-%m-%d").to_string()),
        country
    ).await
}

// RETAIL ACTIVITY COMMANDS

/// Get top retail activity
#[tauri::command]
pub async fn get_nasdaq_top_retail_activity(app: tauri::AppHandle, 
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(lim) = limit {
        args.push(lim.to_string());
    } else {
        args.push("10".to_string());
    }
    execute_nasdaq_command(app, "top_retail".to_string(), args).await
}

/// Get retail sentiment leaders
#[tauri::command]
pub async fn get_nasdaq_retail_sentiment(app: tauri::AppHandle,
    limit: Option<i32>,
) -> Result<String, String> {
    // This would use top retail activity data and filter by sentiment
    // For now, return the top retail activity
    get_nasdaq_top_retail_activity(app, limit).await
}

// COMPREHENSIVE OVERVIEW COMMANDS

/// Get comprehensive market overview
#[tauri::command]
pub async fn get_nasdaq_market_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_nasdaq_command(app, "market_overview".to_string(), vec![]).await
}

/// Get dividend-focused overview
#[tauri::command]
pub async fn get_nasdaq_dividend_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get upcoming dividends
    match get_nasdaq_upcoming_dividends(app.clone(), Some(14)).await {
        Ok(data) => results.push(("upcoming_dividends".to_string(), data)),
        Err(e) => results.push(("upcoming_dividends".to_string(), format!("Error: {}", e))),
    }

    // Get top dividend stocks
    match get_nasdaq_equity_screener(
        app,
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        Some(25)
    ).await {
        Ok(data) => results.push(("top_stocks".to_string(), data)),
        Err(e) => results.push(("top_stocks".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "dividend_overview": results
    }).to_string())
}

/// Get IPO-focused overview
#[tauri::command]
pub async fn get_nasdaq_ipo_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get upcoming IPOs
    match get_nasdaq_upcoming_ipos(app.clone()).await {
        Ok(data) => results.push(("upcoming_ipos".to_string(), data)),
        Err(e) => results.push(("upcoming_ipos".to_string(), format!("Error: {}", e))),
    }

    // Get recent IPOs
    match get_nasdaq_recent_ipos(app.clone(), Some(30)).await {
        Ok(data) => results.push(("recent_ipos".to_string(), data)),
        Err(e) => results.push(("recent_ipos".to_string(), format!("Error: {}", e))),
    }

    // Get filed IPOs
    match get_nasdaq_filed_ipos(app).await {
        Ok(data) => results.push(("filed_ipos".to_string(), data)),
        Err(e) => results.push(("filed_ipos".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "ipo_overview": results
    }).to_string())
}

/// Get earnings-focused overview
#[tauri::command]
pub async fn get_nasdaq_earnings_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get upcoming earnings
    match get_nasdaq_upcoming_earnings(app.clone(), Some(7)).await {
        Ok(data) => results.push(("upcoming_earnings".to_string(), data)),
        Err(e) => results.push(("upcoming_earnings".to_string(), format!("Error: {}", e))),
    }

    // Get today's earnings
    match get_nasdaq_earnings_calendar(
        app,
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string()),
        Some(chrono::Utc::now().format("%Y-%m-%d").to_string())
    ).await {
        Ok(data) => results.push(("todays_earnings".to_string(), data)),
        Err(e) => results.push(("todays_earnings".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "earnings_overview": results
    }).to_string())
}

/// Get sector analysis overview
#[tauri::command]
pub async fn get_nasdaq_sector_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get data for major sectors
    let sectors = vec![
        "technology", "health_care", "financial_services", "consumer_discretionary",
        "communication_services", "industrials", "energy", "consumer_staples",
        "utilities", "real_estate", "basic_materials"
    ];

    let sectors_clone = sectors.clone();
    for sector in sectors {
        match get_nasdaq_stocks_by_sector(app.clone(), sector.to_string(), Some(20)).await {
            Ok(data) => results.push((format!("sector_{}", sector), data)),
            Err(e) => results.push((format!("sector_{}", sector), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "sector_analysis": results,
        "sectors_analyzed": sectors_clone
    }).to_string())
}

/// Get market cap analysis overview
#[tauri::command]
pub async fn get_nasdaq_market_cap_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get data for each market cap category
    let market_caps = vec!["mega", "large", "mid", "small", "micro"];

    let market_caps_clone = market_caps.clone();
    for market_cap in market_caps {
        match get_nasdaq_stocks_by_market_cap(app.clone(), market_cap.to_string(), Some(20)).await {
            Ok(data) => results.push((format!("market_cap_{}", market_cap), data)),
            Err(e) => results.push((format!("market_cap_{}", market_cap), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "market_cap_analysis": results,
        "market_caps_analyzed": market_caps_clone
    }).to_string())
}

/// Get exchange comparison overview
#[tauri::command]
pub async fn get_nasdaq_exchange_comparison(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get data for each exchange
    let exchanges = vec!["nasdaq", "nyse", "amex"];

    let exchanges_clone = exchanges.clone();
    for exchange in exchanges {
        match get_nasdaq_stocks_by_exchange(app.clone(), exchange.to_string(), Some(25)).await {
            Ok(data) => results.push((format!("exchange_{}", exchange), data)),
            Err(e) => results.push((format!("exchange_{}", exchange), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "exchange_comparison": results,
        "exchanges_analyzed": exchanges_clone
    }).to_string())
}

/// Get custom date range analysis
#[tauri::command]
pub async fn get_nasdaq_date_range_analysis(app: tauri::AppHandle, 
    start_date: String,
    end_date: String,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get dividends for date range
    match get_nasdaq_dividend_calendar(
        app.clone(),
        Some(start_date.clone()),
        Some(end_date.clone())
    ).await {
        Ok(data) => results.push(("dividends".to_string(), data)),
        Err(e) => results.push(("dividends".to_string(), format!("Error: {}", e))),
    }

    // Get earnings for date range
    match get_nasdaq_earnings_calendar(
        app.clone(),
        Some(start_date.clone()),
        Some(end_date.clone())
    ).await {
        Ok(data) => results.push(("earnings".to_string(), data)),
        Err(e) => results.push(("earnings".to_string(), format!("Error: {}", e))),
    }

    // Get economic events for date range
    match get_nasdaq_economic_calendar(
        app,
        Some(start_date.clone()),
        Some(end_date.clone()),
        None
    ).await {
        Ok(data) => results.push(("economic_events".to_string(), data)),
        Err(e) => results.push(("economic_events".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "date_range_analysis": results,
        "date_range": {
            "start": start_date,
            "end": end_date
        }
    }).to_string())
}

/// Get NASDAQ directory information
#[tauri::command]
pub async fn get_nasdaq_directory_info(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Search for popular stocks
    let popular_searches = vec!["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"];

    let popular_searches_clone = popular_searches.clone();
    for search_term in popular_searches {
        match search_nasdaq_equities(app.clone(), Some(search_term.to_string()), None).await {
            Ok(data) => results.push((format!("search_{}", search_term), data)),
            Err(e) => results.push((format!("search_{}", search_term), format!("Error: {}", e))),
        }
    }

    // Get general screener data
    match get_nasdaq_equity_screener(
        app,
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        Some("all".to_string()),
        Some(100)
    ).await {
        Ok(data) => results.push(("general_screener".to_string(), data)),
        Err(e) => results.push(("general_screener".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "directory_info": results,
        "popular_searches": popular_searches_clone
    }).to_string())
}