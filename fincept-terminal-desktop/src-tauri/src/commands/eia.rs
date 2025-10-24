// EIA (U.S. Energy Information Administration) data commands based on OpenBB eia provider
use crate::utils::python::execute_python_command;

/// Execute EIA Python script command
#[tauri::command]
pub async fn execute_eia_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "eia_data.py", &cmd_args)
}

// PETROLEUM STATUS REPORT COMMANDS

/// Get petroleum status report data
#[tauri::command]
pub async fn get_eia_petroleum_status_report(app: tauri::AppHandle, 
    category: String,
    tables: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![category];
    if let Some(t) = tables {
        args.push(t);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get available petroleum categories
#[tauri::command]
pub async fn get_eia_petroleum_categories(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_eia_command(app, "available_categories".to_string(), vec![]).await
}

/// Get crude oil stocks
#[tauri::command]
pub async fn get_eia_crude_stocks(app: tauri::AppHandle, 
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
    execute_eia_command(app, "get_crude_stocks".to_string(), args).await
}

/// Get gasoline stocks
#[tauri::command]
pub async fn get_eia_gasoline_stocks(app: tauri::AppHandle, 
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
    execute_eia_command(app, "get_gasoline_stocks".to_string(), args).await
}

/// Get petroleum imports
#[tauri::command]
pub async fn get_eia_petroleum_imports(app: tauri::AppHandle, 
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
    execute_eia_command(app, "get_petroleum_imports".to_string(), args).await
}

/// Get spot prices
#[tauri::command]
pub async fn get_eia_spot_prices(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_eia_command(app, "get_spot_prices".to_string(), vec![]).await
}

// SHORT TERM ENERGY OUTLOOK (STEO) COMMANDS

/// Get short term energy outlook data
#[tauri::command]
pub async fn get_eia_short_term_energy_outlook(app: tauri::AppHandle, 
    table: String,
    symbols: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![table];
    if let Some(s) = symbols {
        args.push(s);
    }
    if let Some(f) = frequency {
        args.push(f);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_steo".to_string(), args).await
}

/// Get available STEO tables
#[tauri::command]
pub async fn get_eia_steo_tables(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_eia_command(app, "available_steo_tables".to_string(), vec![]).await
}

/// Get energy markets summary
#[tauri::command]
pub async fn get_eia_energy_markets_summary(app: tauri::AppHandle, 
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
    execute_eia_command(app, "get_energy_markets_summary".to_string(), args).await
}

/// Get natural gas summary
#[tauri::command]
pub async fn get_eia_natural_gas_summary(app: tauri::AppHandle, 
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
    execute_eia_command(app, "get_natural_gas_summary".to_string(), args).await
}

// SPECIFIC PETROLEUM DATA COMMANDS

/// Get balance sheet data
#[tauri::command]
pub async fn get_eia_balance_sheet(app: tauri::AppHandle, 
    tables: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["balance_sheet".to_string()];
    if let Some(t) = tables {
        args.push(t);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get inputs and production data
#[tauri::command]
pub async fn get_eia_inputs_production(app: tauri::AppHandle, 
    tables: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["inputs_and_production".to_string()];
    if let Some(t) = tables {
        args.push(t);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get refiner blender net production
#[tauri::command]
pub async fn get_eia_refiner_production(app: tauri::AppHandle, 
    tables: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["refiner_blender_net_production".to_string()];
    if let Some(t) = tables {
        args.push(t);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get distillate fuel oil stocks
#[tauri::command]
pub async fn get_eia_distillate_stocks(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["distillate_fuel_oil_stocks".to_string()];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get imports by country
#[tauri::command]
pub async fn get_eia_imports_by_country(app: tauri::AppHandle, 
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["imports_by_country".to_string()];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get weekly estimates
#[tauri::command]
pub async fn get_eia_weekly_estimates(app: tauri::AppHandle, 
    tables: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["weekly_estimates".to_string()];
    if let Some(t) = tables {
        args.push(t);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

/// Get retail prices
#[tauri::command]
pub async fn get_eia_retail_prices(app: tauri::AppHandle, 
    tables: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["retail_prices".to_string()];
    if let Some(t) = tables {
        args.push(t);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_petroleum".to_string(), args).await
}

// SPECIFIC STEO DATA COMMANDS

/// Get nominal energy prices
#[tauri::command]
pub async fn get_eia_nominal_prices(app: tauri::AppHandle, 
    symbols: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["02".to_string()];
    if let Some(s) = symbols {
        args.push(s);
    }
    if let Some(f) = frequency {
        args.push(f);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_steo".to_string(), args).await
}

/// Get US petroleum supply data
#[tauri::command]
pub async fn get_eia_petroleum_supply(app: tauri::AppHandle, 
    symbols: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["04a".to_string()];
    if let Some(s) = symbols {
        args.push(s);
    }
    if let Some(f) = frequency {
        args.push(f);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_steo".to_string(), args).await
}

/// Get electricity industry overview
#[tauri::command]
pub async fn get_eia_electricity_overview(app: tauri::AppHandle, 
    symbols: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["07a".to_string()];
    if let Some(s) = symbols {
        args.push(s);
    }
    if let Some(f) = frequency {
        args.push(f);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_steo".to_string(), args).await
}

/// Get macroeconomic indicators
#[tauri::command]
pub async fn get_eia_macroeconomic_indicators(app: tauri::AppHandle, 
    symbols: Option<String>,
    frequency: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["09a".to_string()];
    if let Some(s) = symbols {
        args.push(s);
    }
    if let Some(f) = frequency {
        args.push(f);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_eia_command(app, "get_steo".to_string(), args).await
}

// OVERVIEW AND ANALYSIS COMMANDS

/// Get comprehensive energy overview
#[tauri::command]
pub async fn get_eia_energy_overview(app: tauri::AppHandle, 
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(lim) = limit {
        args.push(lim.to_string());
    }
    execute_eia_command(app, "energy_overview".to_string(), args).await
}

/// Get petroleum markets overview
#[tauri::command]
pub async fn get_eia_petroleum_markets_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get key petroleum data
    let categories = vec![
        ("balance_sheet", "stocks,supply"),
        ("crude_petroleum_stocks", "stocks"),
        ("gasoline_fuel_stocks", "stocks"),
        ("imports", "imports"),
        ("spot_prices_crude_gas_heating", "crude,conventional_gas")
    ];

    let categories_len = categories.len();

    for (category, tables) in categories {
        match get_eia_petroleum_status_report(app.clone(), 
            category.to_string(),
            Some(tables.to_string()),
            None,
            None
        ).await {
            Ok(data) => results.push((category.to_string(), data)),
            Err(e) => results.push((category.to_string(), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "petroleum_markets_overview": results,
        "categories_analyzed": categories_len
    }).to_string())
}

/// Get natural gas markets overview
#[tauri::command]
pub async fn get_eia_natural_gas_markets_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get natural gas data from STEO
    let gas_tables = vec![
        ("05a", "US Natural Gas Supply, Consumption, and Inventories"),
        ("05b", "US Regional Natural Gas Prices")
    ];

    for (table, _description) in gas_tables {
        match get_eia_short_term_energy_outlook(app.clone(), 
            table.to_string(),
            None,
            Some("month".to_string()),
            None,
            None
        ).await {
            Ok(data) => results.push((format!("steo_{}", table), data)),
            Err(e) => results.push((format!("steo_{}", table), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "natural_gas_markets_overview": results,
        "description": "Natural gas market overview from STEO data"
    }).to_string())
}

/// Get energy price trends
#[tauri::command]
pub async fn get_eia_energy_price_trends(app: tauri::AppHandle, 
    frequency: Option<String>,
) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "month".to_string());
    let mut results = Vec::new();

    // Get price data from different tables
    let price_tables = vec![
        ("02", "WTIPUUS,BREPUUS,NGHHUUS,CLEUDUS"), // Nominal energy prices
    ];

    for (table, symbols) in price_tables {
        match get_eia_short_term_energy_outlook(app.clone(), 
            table.to_string(),
            Some(symbols.to_string()),
            Some(freq.clone()),
            None,
            None
        ).await {
            Ok(data) => results.push((format!("prices_{}", table), data)),
            Err(e) => results.push((format!("prices_{}", table), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "energy_price_trends": results,
        "frequency": freq,
        "description": "Energy price trends from multiple sources"
    }).to_string())
}

/// Get energy supply analysis
#[tauri::command]
pub async fn get_eia_energy_supply_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get supply data from petroleum status report
    let supply_categories = vec![
        ("balance_sheet", "supply"),
        ("inputs_and_production", "product_by_region"),
        ("refiner_blender_net_production", "net_production")
    ];

    for (category, tables) in supply_categories {
        match get_eia_petroleum_status_report(app.clone(), 
            category.to_string(),
            Some(tables.to_string()),
            None,
            None
        ).await {
            Ok(data) => results.push((format!("petroleum_{}_{}", category, tables), data)),
            Err(e) => results.push((format!("petroleum_{}_{}", category, tables), format!("Error: {}", e))),
        }
    }

    // Get supply data from STEO
    match get_eia_short_term_energy_outlook(app.clone(), 
        "04a".to_string(),
        None,
        Some("month".to_string()),
        None,
        None
    ).await {
        Ok(data) => results.push(("steo_petroleum_supply".to_string(), data)),
        Err(e) => results.push(("steo_petroleum_supply".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "energy_supply_analysis": results,
        "description": "Comprehensive energy supply analysis"
    }).to_string())
}

/// Get energy consumption analysis
#[tauri::command]
pub async fn get_eia_energy_consumption_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get consumption data from STEO
    let consumption_tables = vec![
        ("04a", "PATCPUSX,PAFPPUS,PANIPUS"),  // Petroleum consumption
        ("05a", "NGNWPUS,NGSFPUS,NGNIPUS"),     // Natural gas consumption
        ("07a", "EPEOTWH,INEOTWH,CMEOTWH"),     // Electricity consumption
    ];

    for (table, symbols) in consumption_tables {
        match get_eia_short_term_energy_outlook(app.clone(), 
            table.to_string(),
            Some(symbols.to_string()),
            Some("month".to_string()),
            None,
            None
        ).await {
            Ok(data) => results.push((format!("consumption_{}", table), data)),
            Err(e) => results.push((format!("consumption_{}", table), format!("Error: {}", e))),
        }
    }

    Ok(serde_json::json!({
        "energy_consumption_analysis": results,
        "description": "Energy consumption analysis across sectors"
    }).to_string())
}

/// Get custom energy data analysis
#[tauri::command]
pub async fn get_eia_custom_analysis(app: tauri::AppHandle, 
    data_sources: String, // comma-separated list of data sources
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut results = Vec::new();

    // Parse data sources
    let sources: Vec<&str> = data_sources.split(',').collect();

    for source in sources {
        let source = source.trim();

        if source.starts_with("petroleum_") {
            // Handle petroleum status report requests
            let parts: Vec<&str> = source.split('_').collect();
            if parts.len() >= 3 {
                let category = format!("{}_{}", parts[1], parts[2]);
                let tables = if parts.len() > 3 { parts[3] } else { "all" };

                match get_eia_petroleum_status_report(app.clone(), 
                    category,
                    Some(tables.to_string()),
                    start_date.clone(),
                    end_date.clone()
                ).await {
                    Ok(data) => results.push((source.to_string(), data)),
                    Err(e) => results.push((source.to_string(), format!("Error: {}", e))),
                }
            }
        } else if source.starts_with("steo_") {
            // Handle STEO requests
            let table = source.strip_prefix("steo_").unwrap_or("01");

            match get_eia_short_term_energy_outlook(app.clone(), 
                table.to_string(),
                None,
                Some("month".to_string()),
                start_date.clone(),
                end_date.clone()
            ).await {
                Ok(data) => results.push((source.to_string(), data)),
                Err(e) => results.push((source.to_string(), format!("Error: {}", e))),
            }
        }
    }

    Ok(serde_json::json!({
        "custom_energy_analysis": results,
        "data_sources": data_sources,
        "date_range": {
            "start": start_date,
            "end": end_date
        }
    }).to_string())
}

/// Get current energy market snapshot
#[tauri::command]
pub async fn get_eia_current_market_snapshot(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get most recent petroleum data
    match get_eia_petroleum_status_report(app.clone(), "balance_sheet".to_string(), Some("stocks,supply".to_string()), None, None).await {
        Ok(data) => results.push(("petroleum_balance".to_string(), data)),
        Err(e) => results.push(("petroleum_balance".to_string(), format!("Error: {}", e))),
    }

    // Get most recent spot prices
    match get_eia_spot_prices(app.clone()).await {
        Ok(data) => results.push(("spot_prices".to_string(), data)),
        Err(e) => results.push(("spot_prices".to_string(), format!("Error: {}", e))),
    }

    // Get recent STEO data (if available)
    match get_eia_energy_markets_summary(app, None, None).await {
        Ok(data) => results.push(("energy_markets".to_string(), data)),
        Err(e) => results.push(("energy_markets".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "current_market_snapshot": results,
        "timestamp": chrono::Utc::now().to_rfc3339(),
        "description": "Current energy market snapshot"
    }).to_string())
}