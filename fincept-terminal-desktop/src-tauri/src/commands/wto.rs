// WTO (World Trade Organization) data commands based on WTO APIs
use crate::utils::python::execute_python_command;

/// Execute WTO Python script command
#[tauri::command]
pub async fn execute_wto_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "wto_data.py", &cmd_args)
}

// REFERENCE AND OVERVIEW COMMANDS

/// Get available WTO APIs and endpoints
#[tauri::command]
pub async fn get_wto_available_apis(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_wto_command(app, "available_apis".to_string(), vec![]).await
}

/// Get comprehensive WTO overview across all APIs
#[tauri::command]
pub async fn get_wto_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_wto_command(app, "overview".to_string(), vec![]).await
}

// QUANTITATIVE RESTRICTIONS (QR) COMMANDS

/// Get QR members list
#[tauri::command]
pub async fn get_wto_qr_members(app: tauri::AppHandle, 
    member_code: Option<String>,
    name: Option<String>,
    page: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(mc) = member_code {
        args.push("--member_code".to_string());
        args.push(mc);
    }

    if let Some(n) = name {
        args.push("--name".to_string());
        args.push(n);
    }

    if let Some(p) = page {
        args.push("--page".to_string());
        args.push(p.to_string());
    }

    execute_wto_command(app, "qr_members".to_string(), args).await
}

/// Get QR products list
#[tauri::command]
pub async fn get_wto_qr_products(app: tauri::AppHandle, 
    hs_version: String,
    code: Option<String>,
    description: Option<String>,
    page: Option<i32>,
) -> Result<String, String> {
    let mut args = vec!["--hs_version".to_string(), hs_version];

    if let Some(c) = code {
        args.push("--code".to_string());
        args.push(c);
    }

    if let Some(d) = description {
        args.push("--description".to_string());
        args.push(d);
    }

    if let Some(p) = page {
        args.push("--page".to_string());
        args.push(p.to_string());
    }

    execute_wto_command(app, "qr_products".to_string(), args).await
}

/// Get QR notifications list
#[tauri::command]
pub async fn get_wto_qr_notifications(app: tauri::AppHandle, 
    reporter_member_code: Option<String>,
    notification_year: Option<i32>,
    page: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(rmc) = reporter_member_code {
        args.push("--reporter_member_code".to_string());
        args.push(rmc);
    }

    if let Some(ny) = notification_year {
        args.push("--notification_year".to_string());
        args.push(ny.to_string());
    }

    if let Some(p) = page {
        args.push("--page".to_string());
        args.push(p.to_string());
    }

    execute_wto_command(app, "qr_notifications".to_string(), args).await
}

/// Get QR details by ID
#[tauri::command]
pub async fn get_wto_qr_details(app: tauri::AppHandle, 
    qr_id: i32,
) -> Result<String, String> {
    let args = vec!["--qr_id".to_string(), qr_id.to_string()];
    execute_wto_command(app, "qr_details".to_string(), args).await
}

/// Get QR list with filters
#[tauri::command]
pub async fn get_wto_qr_list(app: tauri::AppHandle, 
    reporter_member_code: Option<String>,
    in_force_only: Option<bool>,
    year_of_entry_into_force: Option<i32>,
    product_codes: Option<String>,
    product_ids: Option<String>,
    page: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(rmc) = reporter_member_code {
        args.push("--reporter_member_code".to_string());
        args.push(rmc);
    }

    if let Some(ifo) = in_force_only {
        args.push("--in_force_only".to_string());
        args.push(ifo.to_string());
    }

    if let Some(yeif) = year_of_entry_into_force {
        args.push("--year_of_entry_into_force".to_string());
        args.push(yeif.to_string());
    }

    if let Some(pc) = product_codes {
        args.push("--product_codes".to_string());
        args.push(pc);
    }

    if let Some(pi) = product_ids {
        args.push("--product_ids".to_string());
        args.push(pi);
    }

    if let Some(p) = page {
        args.push("--page".to_string());
        args.push(p.to_string());
    }

    execute_wto_command(app, "qr_list".to_string(), args).await
}

/// Get QR HS versions
#[tauri::command]
pub async fn get_wto_qr_hs_versions(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_wto_command(app, "qr_hs_versions".to_string(), vec![]).await
}

// EPING (SPS/TBT NOTIFICATIONS) COMMANDS

/// Get ePing members list
#[tauri::command]
pub async fn get_wto_eping_members(app: tauri::AppHandle, 
    language: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    execute_wto_command(app, "eping_members".to_string(), args).await
}

/// Search ePing notifications
#[tauri::command]
pub async fn search_wto_eping_notifications(app: tauri::AppHandle, 
    language: Option<i32>,
    domain_ids: Option<i32>,
    document_symbol: Option<String>,
    distribution_date_from: Option<String>,
    distribution_date_to: Option<String>,
    country_ids: Option<String>,
    hs_codes: Option<String>,
    ics_codes: Option<String>,
    free_text: Option<String>,
    page: Option<i32>,
    page_size: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    if let Some(di) = domain_ids {
        args.push("--domainIds".to_string());
        args.push(di.to_string());
    }

    if let Some(ds) = document_symbol {
        args.push("--document_symbol".to_string());
        args.push(ds);
    }

    if let Some(ddf) = distribution_date_from {
        args.push("--distribution_date_from".to_string());
        args.push(ddf);
    }

    if let Some(ddt) = distribution_date_to {
        args.push("--distribution_date_to".to_string());
        args.push(ddt);
    }

    if let Some(ci) = country_ids {
        args.push("--country_ids".to_string());
        args.push(ci);
    }

    if let Some(hs) = hs_codes {
        args.push("--hs_codes".to_string());
        args.push(hs);
    }

    if let Some(ics) = ics_codes {
        args.push("--ics_codes".to_string());
        args.push(ics);
    }

    if let Some(ft) = free_text {
        args.push("--free_text".to_string());
        args.push(ft);
    }

    if let Some(p) = page {
        args.push("--page".to_string());
        args.push(p.to_string());
    }

    if let Some(ps) = page_size {
        args.push("--page_size".to_string());
        args.push(ps.to_string());
    }

    execute_wto_command(app, "eping_search".to_string(), args).await
}

// TIMESERIES API COMMANDS

/// Get timeseries topics
#[tauri::command]
pub async fn get_wto_timeseries_topics(app: tauri::AppHandle, 
    language: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    execute_wto_command(app, "timeseries_topics".to_string(), args).await
}

/// Get timeseries indicators
#[tauri::command]
pub async fn get_wto_timeseries_indicators(app: tauri::AppHandle, 
    indicator: Option<String>,
    name: Option<String>,
    topics: Option<String>,
    product_classifications: Option<String>,
    trade_partner: Option<String>,
    frequency: Option<String>,
    language: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(i) = indicator {
        args.push("--indicator".to_string());
        args.push(i);
    }

    if let Some(n) = name {
        args.push("--name".to_string());
        args.push(n);
    }

    if let Some(t) = topics {
        args.push("--topics".to_string());
        args.push(t);
    }

    if let Some(pc) = product_classifications {
        args.push("--product_classifications".to_string());
        args.push(pc);
    }

    if let Some(tp) = trade_partner {
        args.push("--trade_partner".to_string());
        args.push(tp);
    }

    if let Some(f) = frequency {
        args.push("--frequency".to_string());
        args.push(f);
    }

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    execute_wto_command(app, "timeseries_indicators".to_string(), args).await
}

/// Get timeseries data
#[tauri::command]
pub async fn get_wto_timeseries_data(app: tauri::AppHandle, 
    indicator: String,
    reporters: Option<String>,
    partners: Option<String>,
    periods: Option<String>,
    products: Option<String>,
    include_sub_products: Option<bool>,
    format_type: Option<String>,
    mode: Option<String>,
    decimals: Option<String>,
    offset: Option<i32>,
    max_records: Option<i32>,
    heading_style: Option<String>,
    language: Option<i32>,
    include_metadata: Option<bool>,
) -> Result<String, String> {
    let mut args = vec!["--i".to_string(), indicator];

    if let Some(r) = reporters {
        args.push("--reporters".to_string());
        args.push(r);
    }

    if let Some(p) = partners {
        args.push("--partners".to_string());
        args.push(p);
    }

    if let Some(ps) = periods {
        args.push("--periods".to_string());
        args.push(ps);
    }

    if let Some(pc) = products {
        args.push("--products".to_string());
        args.push(pc);
    }

    if let Some(isp) = include_sub_products {
        args.push("--include_sub_products".to_string());
        args.push(isp.to_string());
    }

    if let Some(ft) = format_type {
        args.push("--format_type".to_string());
        args.push(ft);
    }

    if let Some(m) = mode {
        args.push("--mode".to_string());
        args.push(m);
    }

    if let Some(d) = decimals {
        args.push("--decimals".to_string());
        args.push(d);
    }

    if let Some(o) = offset {
        args.push("--offset".to_string());
        args.push(o.to_string());
    }

    if let Some(mr) = max_records {
        args.push("--max_records".to_string());
        args.push(mr.to_string());
    }

    if let Some(hs) = heading_style {
        args.push("--heading_style".to_string());
        args.push(hs);
    }

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    if let Some(im) = include_metadata {
        args.push("--include_metadata".to_string());
        args.push(im.to_string());
    }

    execute_wto_command(app, "timeseries_data".to_string(), args).await
}

/// Get timeseries reporting economies
#[tauri::command]
pub async fn get_wto_timeseries_reporters(app: tauri::AppHandle, 
    name: Option<String>,
    individual_group: Option<String>,
    regions: Option<String>,
    groups: Option<String>,
    language: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(n) = name {
        args.push("--name".to_string());
        args.push(n);
    }

    if let Some(ig) = individual_group {
        args.push("--individual_group".to_string());
        args.push(ig);
    }

    if let Some(r) = regions {
        args.push("--regions".to_string());
        args.push(r);
    }

    if let Some(g) = groups {
        args.push("--groups".to_string());
        args.push(g);
    }

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    execute_wto_command(app, "timeseries_reporters".to_string(), args).await
}

// TFAD (TRADE FACILITATION AGREEMENT DATABASE) COMMANDS

/// Get TFAD data
#[tauri::command]
pub async fn get_wto_tfad_data(app: tauri::AppHandle, 
    countries: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(c) = countries {
        args.push("--countries".to_string());
        args.push(c);
    }

    execute_wto_command(app, "tfad_data".to_string(), args).await
}

// ANALYSIS COMMANDS

/// Get trade restrictions analysis
#[tauri::command]
pub async fn get_wto_trade_restrictions_analysis(app: tauri::AppHandle, 
    member_code: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(mc) = member_code {
        args.push("--member_code".to_string());
        args.push(mc);
    }

    execute_wto_command(app, "trade_restrictions_analysis".to_string(), args).await
}

/// Get notifications analysis
#[tauri::command]
pub async fn get_wto_notifications_analysis(app: tauri::AppHandle, 
    language: Option<i32>,
    domain_ids: Option<i32>,
    date_from: Option<String>,
    date_to: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(l) = language {
        args.push("--language".to_string());
        args.push(l.to_string());
    }

    if let Some(di) = domain_ids {
        args.push("--domain_ids".to_string());
        args.push(di.to_string());
    }

    if let Some(df) = date_from {
        args.push("--date_from".to_string());
        args.push(df);
    }

    if let Some(dt) = date_to {
        args.push("--date_to".to_string());
        args.push(dt);
    }

    execute_wto_command(app, "notifications_analysis".to_string(), args).await
}

/// Get trade statistics analysis
#[tauri::command]
pub async fn get_wto_trade_statistics_analysis(app: tauri::AppHandle, 
    indicator: Option<String>,
    reporter: Option<String>,
    years: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(i) = indicator {
        args.push("--indicator".to_string());
        args.push(i);
    }

    if let Some(r) = reporter {
        args.push("--reporter".to_string());
        args.push(r);
    }

    if let Some(y) = years {
        args.push("--years".to_string());
        args.push(y);
    }

    execute_wto_command(app, "trade_statistics_analysis".to_string(), args).await
}

/// Get comprehensive WTO analysis
#[tauri::command]
pub async fn get_wto_comprehensive_analysis(app: tauri::AppHandle, 
    member_code: Option<String>,
    indicator: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();

    if let Some(mc) = member_code {
        args.push("--member_code".to_string());
        args.push(mc);
    }

    if let Some(i) = indicator {
        args.push("--indicator".to_string());
        args.push(i);
    }

    execute_wto_command(app, "comprehensive_analysis".to_string(), args).await
}

// CONVENIENCE COMMANDS FOR MAJOR ECONOMIES

/// Get US trade data analysis
#[tauri::command]
pub async fn get_wto_us_trade_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    get_wto_trade_statistics_analysis(
        app,
        Some("TP_A_0010".to_string()),
        Some("US".to_string()),
        Some("2020-2023".to_string())
    ).await
}

/// Get China trade data analysis
#[tauri::command]
pub async fn get_wto_china_trade_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    get_wto_trade_statistics_analysis(
        app,
        Some("TP_A_0010".to_string()),
        Some("CN".to_string()),
        Some("2020-2023".to_string())
    ).await
}

/// Get Germany trade data analysis
#[tauri::command]
pub async fn get_wto_germany_trade_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    get_wto_trade_statistics_analysis(
        app,
        Some("TP_A_0010".to_string()),
        Some("DE".to_string()),
        Some("2020-2023".to_string())
    ).await
}

/// Get Japan trade data analysis
#[tauri::command]
pub async fn get_wto_japan_trade_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    get_wto_trade_statistics_analysis(
        app,
        Some("TP_A_0010".to_string()),
        Some("JP".to_string()),
        Some("2020-2023".to_string())
    ).await
}

/// Get UK trade data analysis
#[tauri::command]
pub async fn get_wto_uk_trade_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    get_wto_trade_statistics_analysis(
        app,
        Some("TP_A_0010".to_string()),
        Some("GB".to_string()),
        Some("2020-2023".to_string())
    ).await
}

// SPECIFIC ANALYSIS COMMANDS

/// Get recent TBT notifications
#[tauri::command]
pub async fn get_wto_recent_tbt_notifications(app: tauri::AppHandle, 
    days_back: Option<i32>,
) -> Result<String, String> {
    let days = days_back.unwrap_or(30);
    let date_to = chrono::Utc::now().format("%Y-%m-%d").to_string();
    let date_from = (chrono::Utc::now() - chrono::Duration::days(days as i64)).format("%Y-%m-%d").to_string();

    search_wto_eping_notifications(
        app,
        Some(1),  // English
        Some(1),  // TBT domain
        None,
        Some(date_from),
        Some(date_to),
        None,
        None,
        None,
        None,
        Some(1),
        Some(20)
    ).await
}

/// Get recent SPS notifications
#[tauri::command]
pub async fn get_wto_recent_sps_notifications(app: tauri::AppHandle, 
    days_back: Option<i32>,
) -> Result<String, String> {
    let days = days_back.unwrap_or(30);
    let date_to = chrono::Utc::now().format("%Y-%m-%d").to_string();
    let date_from = (chrono::Utc::now() - chrono::Duration::days(days as i64)).format("%Y-%m-%d").to_string();

    search_wto_eping_notifications(
        app,
        Some(1),  // English
        Some(2),  // SPS domain
        None,
        Some(date_from),
        Some(date_to),
        None,
        None,
        None,
        None,
        Some(1),
        Some(20)
    ).await
}

/// Get member specific comprehensive analysis
#[tauri::command]
pub async fn get_wto_member_analysis(app: tauri::AppHandle, 
    member_code: String,
) -> Result<String, String> {
    get_wto_comprehensive_analysis(
        app,
        Some(member_code),
        Some("TP_A_0010".to_string())
    ).await
}

/// Get global trade overview
#[tauri::command]
pub async fn get_wto_global_trade_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get timeseries data for global trade
    match get_wto_timeseries_data(
        app.clone(),
        "TP_A_0010".to_string(),  // Merchandise trade value
        Some("all".to_string()),
        Some("all".to_string()),
        Some("2022-2023".to_string()),
        None,
        None,
        None,
        None,
        None,
        None,
        Some(100),
        None,
        Some(1),
        None
    ).await {
        Ok(data) => results.push(("global_trade_data".to_string(), data)),
        Err(e) => results.push(("global_trade_data".to_string(), format!("Error: {}", e))),
    }

    // Get recent notifications
    match get_wto_recent_tbt_notifications(app.clone(), Some(7)).await {
        Ok(data) => results.push(("recent_tbt_notifications".to_string(), data)),
        Err(e) => results.push(("recent_tbt_notifications".to_string(), format!("Error: {}", e))),
    }

    match get_wto_recent_sps_notifications(app.clone(), Some(7)).await {
        Ok(data) => results.push(("recent_sps_notifications".to_string(), data)),
        Err(e) => results.push(("recent_sps_notifications".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "global_trade_overview": results,
        "analysis_type": "wto_global_trade_summary",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}

/// Get sector-specific trade analysis
#[tauri::command]
pub async fn get_wto_sector_trade_analysis(app: tauri::AppHandle, 
    sector_codes: String,
    reporter: Option<String>,
    years: Option<String>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Get data for each sector
    let sectors: Vec<&str> = sector_codes.split(',').collect();

    for sector in sectors {
        let trimmed_sector = sector.trim();
        match get_wto_timeseries_data(
            app.clone(),
            format!("TP_S_{}", trimmed_sector),
            reporter.clone(),
            Some("default".to_string()),
            years.clone(),
            Some(trimmed_sector.to_string()),
            None,
            None,
            None,
            None,
            None,
            Some(100),
            None,
            Some(1),
            None
        ).await {
            Ok(data) => results.push((format!("sector_{}", trimmed_sector), data)),
            Err(e) => results.push((format!("sector_{}", trimmed_sector), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "sector_trade_analysis": results,
        "sectors_analyzed": sector_codes,
        "reporter": reporter.unwrap_or_else(|| "all".to_string()),
        "years": years.unwrap_or_else(|| "default".to_string()),
        "analysis_type": "wto_sector_trade_summary",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}

/// Get comparative trade analysis between economies
#[tauri::command]
pub async fn get_wto_comparative_trade_analysis(app: tauri::AppHandle, 
    economies: String,
    indicator: Option<String>,
    years: Option<String>,
) -> Result<String, String> {
    let mut results = Vec::new();

    let indicator_code = indicator.unwrap_or_else(|| "TP_A_0010".to_string());
    let years_analysis = years.unwrap_or_else(|| "2020-2023".to_string());

    // Get data for each economy
    let economy_list: Vec<&str> = economies.split(',').collect();

    for economy in economy_list {
        let trimmed_economy = economy.trim();
        match get_wto_timeseries_data(
            app.clone(),
            indicator_code.clone(),
            Some(trimmed_economy.to_string()),
            Some("all".to_string()),
            Some(years_analysis.clone()),
            None,
            None,
            None,
            None,
            None,
            None,
            Some(100),
            None,
            Some(1),
            None
        ).await {
            Ok(data) => results.push((format!("economy_{}", trimmed_economy), data)),
            Err(e) => results.push((format!("economy_{}", trimmed_economy), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "comparative_trade_analysis": results,
        "economies_compared": economies,
        "indicator": indicator_code,
        "years": years_analysis,
        "analysis_type": "wto_comparative_trade_summary",
        "timestamp": chrono::Utc::now().to_rfc3339()
    }).to_string())
}