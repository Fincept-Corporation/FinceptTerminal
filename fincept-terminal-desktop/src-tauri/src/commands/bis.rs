// BIS (Bank for International Settlements) data commands based on SDMX API
use crate::utils::python::execute_python_command;

/// Execute BIS Python script command
#[tauri::command]
pub async fn execute_bis_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "bis_data.py", &cmd_args)
}

// BASIC DATA QUERY COMMANDS

/// Get statistical data for specific flow and key
#[tauri::command]
pub async fn get_bis_data(app: tauri::AppHandle,
    flow: String,
    key: Option<String>,
    start_period: Option<String>,
    end_period: Option<String>,
    first_n_observations: Option<i32>,
    last_n_observations: Option<i32>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = vec![flow];
    if let Some(k) = key {
        args.push(k);
    } else {
        args.push("all".to_string());
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(first_n) = first_n_observations {
        args.push(first_n.to_string());
    }
    if let Some(last_n) = last_n_observations {
        args.push(last_n.to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_data".to_string(), args).await
}

/// Get information about data availability
#[tauri::command]
pub async fn get_bis_available_constraints(app: tauri::AppHandle, 
    flow: String,
    key: String,
    component_id: String,
    mode: Option<String>,
    references: Option<String>,
    start_period: Option<String>,
    end_period: Option<String>,
) -> Result<String, String> {
    let mut args = vec![flow, key, component_id];
    if let Some(m) = mode {
        args.push(m);
    } else {
        args.push("exact".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    execute_bis_command(app, "get_available_constraints".to_string(), args).await
}

// STRUCTURE QUERY COMMANDS

/// Get data structure definitions
#[tauri::command]
pub async fn get_bis_data_structures(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_data_structures".to_string(), args).await
}

/// Get dataflow definitions (available datasets)
#[tauri::command]
pub async fn get_bis_dataflows(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_dataflows".to_string(), args).await
}

/// Get categorisation definitions
#[tauri::command]
pub async fn get_bis_categorisations(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_categorisations".to_string(), args).await
}

/// Get content constraint definitions
#[tauri::command]
pub async fn get_bis_content_constraints(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_content_constraints".to_string(), args).await
}

/// Get actual constraint definitions
#[tauri::command]
pub async fn get_bis_actual_constraints(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_actual_constraints".to_string(), args).await
}

/// Get allowed constraint definitions
#[tauri::command]
pub async fn get_bis_allowed_constraints(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_allowed_constraints".to_string(), args).await
}

/// Get general structure definitions
#[tauri::command]
pub async fn get_bis_structures(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_structures".to_string(), args).await
}

/// Get concept scheme definitions
#[tauri::command]
pub async fn get_bis_concept_schemes(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_concept_schemes".to_string(), args).await
}

/// Get codelist definitions
#[tauri::command]
pub async fn get_bis_codelists(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_codelists".to_string(), args).await
}

/// Get category scheme definitions
#[tauri::command]
pub async fn get_bis_category_schemes(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_category_schemes".to_string(), args).await
}

/// Get hierarchical codelist definitions
#[tauri::command]
pub async fn get_bis_hierarchical_codelists(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_hierarchical_codelists".to_string(), args).await
}

/// Get agency scheme definitions
#[tauri::command]
pub async fn get_bis_agency_schemes(app: tauri::AppHandle, 
    agency_id: Option<String>,
    resource_id: Option<String>,
    version: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(agency) = agency_id {
        args.push(agency);
    } else {
        args.push("all".to_string());
    }
    if let Some(resource) = resource_id {
        args.push(resource);
    } else {
        args.push("all".to_string());
    }
    if let Some(ver) = version {
        args.push(ver);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_agency_schemes".to_string(), args).await
}

// ITEM QUERY COMMANDS

/// Get specific concepts from concept schemes
#[tauri::command]
pub async fn get_bis_concepts(app: tauri::AppHandle, 
    agency_id: String,
    resource_id: String,
    version: String,
    item_id: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = vec![agency_id, resource_id, version];
    if let Some(item) = item_id {
        args.push(item);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_concepts".to_string(), args).await
}

/// Get specific codes from codelists
#[tauri::command]
pub async fn get_bis_codes(app: tauri::AppHandle, 
    agency_id: String,
    resource_id: String,
    version: String,
    item_id: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = vec![agency_id, resource_id, version];
    if let Some(item) = item_id {
        args.push(item);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_codes".to_string(), args).await
}

/// Get specific categories from category schemes
#[tauri::command]
pub async fn get_bis_categories(app: tauri::AppHandle, 
    agency_id: String,
    resource_id: String,
    version: String,
    item_id: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = vec![agency_id, resource_id, version];
    if let Some(item) = item_id {
        args.push(item);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_categories".to_string(), args).await
}

/// Get specific hierarchies from hierarchical codelists
#[tauri::command]
pub async fn get_bis_hierarchies(app: tauri::AppHandle, 
    agency_id: String,
    resource_id: String,
    version: String,
    item_id: Option<String>,
    references: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = vec![agency_id, resource_id, version];
    if let Some(item) = item_id {
        args.push(item);
    } else {
        args.push("all".to_string());
    }
    if let Some(refs) = references {
        args.push(refs);
    } else {
        args.push("none".to_string());
    }
    if let Some(d) = detail {
        args.push(d);
    } else {
        args.push("full".to_string());
    }
    execute_bis_command(app, "get_hierarchies".to_string(), args).await
}

// CONVENIENCE METHODS FOR COMMON DATA TYPES

/// Get effective exchange rates data (WS_EER)
#[tauri::command]
pub async fn get_bis_effective_exchange_rates(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_effective_exchange_rates".to_string(), args).await
}

/// Get central bank policy rates (WS_CBPOL)
#[tauri::command]
pub async fn get_bis_central_bank_policy_rates(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_central_bank_policy_rates".to_string(), args).await
}

/// Get long-term interest rates (WS_LTINT)
#[tauri::command]
pub async fn get_bis_long_term_interest_rates(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_long_term_interest_rates".to_string(), args).await
}

/// Get short-term interest rates (WS_STINT)
#[tauri::command]
pub async fn get_bis_short_term_interest_rates(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_short_term_interest_rates".to_string(), args).await
}

/// Get exchange rates (WS_XRU)
#[tauri::command]
pub async fn get_bis_exchange_rates(app: tauri::AppHandle, 
    currency_pairs: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(currencies_list) = currency_pairs {
        args.extend(currencies_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_exchange_rates".to_string(), args).await
}

/// Get credit to non-financial sector (WS_CRD)
#[tauri::command]
pub async fn get_bis_credit_to_non_financial_sector(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    sectors: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(sectors_list) = sectors {
        args.extend(sectors_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_credit_to_non_financial_sector".to_string(), args).await
}

/// Get house price indices (WS_HP)
#[tauri::command]
pub async fn get_bis_house_prices(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
    detail: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    if let Some(d) = detail {
        args.push(d);
    }
    execute_bis_command(app, "get_house_prices".to_string(), args).await
}

// OVERVIEW AND SEARCH COMMANDS

/// Get overview of all available datasets
#[tauri::command]
pub async fn get_bis_available_datasets(app: tauri::AppHandle, ) -> Result<String, String> {
    execute_bis_command(app, "get_available_datasets".to_string(), vec![]).await
}

/// Search for datasets matching a query
#[tauri::command]
pub async fn search_bis_datasets(app: tauri::AppHandle, 
    query: String,
    agency_id: Option<String>,
) -> Result<String, String> {
    let mut args = vec![query];
    if let Some(agency) = agency_id {
        args.push(agency);
    }
    execute_bis_command(app, "search_datasets".to_string(), args).await
}

/// Get comprehensive economic overview for countries
#[tauri::command]
pub async fn get_bis_economic_overview(app: tauri::AppHandle, 
    countries: Option<Vec<String>>,
    start_period: Option<String>,
    end_period: Option<String>,
) -> Result<String, String> {
    let mut args = Vec::new();
    if let Some(countries_list) = countries {
        args.extend(countries_list);
    }
    if let Some(start) = start_period {
        args.push(start);
    }
    if let Some(end) = end_period {
        args.push(end);
    }
    execute_bis_command(app, "get_economic_overview".to_string(), args).await
}

/// Get comprehensive metadata for a specific dataset
#[tauri::command]
pub async fn get_bis_dataset_metadata(app: tauri::AppHandle, 
    flow: String,
) -> Result<String, String> {
    execute_bis_command(app, "get_dataset_metadata".to_string(), vec![flow]).await
}

// SPECIALIZED ANALYSIS COMMANDS

/// Get global monetary policy overview
#[tauri::command]
pub async fn get_bis_monetary_policy_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get central bank policy rates for major economies
    match get_bis_central_bank_policy_rates(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("policy_rates".to_string(), data)),
        Err(e) => results.push(("policy_rates".to_string(), format!("Error: {}", e))),
    }

    // Get short-term interest rates
    match get_bis_short_term_interest_rates(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("short_term_rates".to_string(), data)),
        Err(e) => results.push(("short_term_rates".to_string(), format!("Error: {}", e))),
    }

    // Get long-term interest rates
    match get_bis_long_term_interest_rates(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("long_term_rates".to_string(), data)),
        Err(e) => results.push(("long_term_rates".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "monetary_policy_overview": results,
        "focus_economies": ["US", "GB", "JP", "EA"],
        "description": "Central bank policy rates and interest rate trends"
    }).to_string())
}

/// Get global exchange rate analysis
#[tauri::command]
pub async fn get_bis_exchange_rate_analysis(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get effective exchange rates
    match get_bis_effective_exchange_rates(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("effective_rates".to_string(), data)),
        Err(e) => results.push(("effective_rates".to_string(), format!("Error: {}", e))),
    }

    // Get major currency exchange rates
    match get_bis_exchange_rates(
        app.clone(),
        Some(vec!["USD".to_string(), "EUR".to_string(), "GBP".to_string(), "JPY".to_string()]),
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("major_currencies".to_string(), data)),
        Err(e) => results.push(("major_currencies".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "exchange_rate_analysis": results,
        "focus_currencies": ["USD", "EUR", "GBP", "JPY"],
        "description": "Exchange rate movements and trends"
    }).to_string())
}

/// Get global credit conditions overview
#[tauri::command]
pub async fn get_bis_credit_conditions_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get credit to non-financial sector for major economies
    match get_bis_credit_to_non_financial_sector(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "EA".to_string()]),
        None,
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("credit_growth".to_string(), data)),
        Err(e) => results.push(("credit_growth".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "credit_conditions_overview": results,
        "focus_economies": ["US", "GB", "JP", "EA"],
        "description": "Credit growth and conditions analysis"
    }).to_string())
}

/// Get global housing market overview
#[tauri::command]
pub async fn get_bis_housing_market_overview(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get house price indices for major economies
    match get_bis_house_prices(
        app.clone(),
        Some(vec!["US".to_string(), "GB".to_string(), "JP".to_string(), "CA".to_string()]),
        None,
        None,
        Some("dataonly".to_string())
    ).await {
        Ok(data) => results.push(("house_prices".to_string(), data)),
        Err(e) => results.push(("house_prices".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "housing_market_overview": results,
        "focus_markets": ["US", "GB", "JP", "CA"],
        "description": "Housing price indices and trends"
    }).to_string())
}

/// Get BIS data directory and available resources
#[tauri::command]
pub async fn get_bis_data_directory(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get available datasets
    match get_bis_available_datasets(app.clone()).await {
        Ok(data) => results.push(("available_datasets".to_string(), data)),
        Err(e) => results.push(("available_datasets".to_string(), format!("Error: {}", e))),
    }

    // Get data structures for BIS
    match get_bis_data_structures(
        app.clone(),
        Some("BIS".to_string()),
        None,
        None,
        Some("none".to_string()),
        Some("allstubs".to_string())
    ).await {
        Ok(data) => results.push(("data_structures".to_string(), data)),
        Err(e) => results.push(("data_structures".to_string(), format!("Error: {}", e))),
    }

    // Get concept schemes
    match get_bis_concept_schemes(
        app.clone(),
        Some("BIS".to_string()),
        None,
        None,
        Some("none".to_string()),
        Some("allstubs".to_string())
    ).await {
        Ok(data) => results.push(("concept_schemes".to_string(), data)),
        Err(e) => results.push(("concept_schemes".to_string(), format!("Error: {}", e))),
    }

    // Get codelists
    match get_bis_codelists(
        app.clone(),
        Some("BIS".to_string()),
        None,
        None,
        Some("none".to_string()),
        Some("allstubs".to_string())
    ).await {
        Ok(data) => results.push(("codelists".to_string(), data)),
        Err(e) => results.push(("codelists".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "data_directory": results,
        "description": "BIS statistical data directory and available resources"
    }).to_string())
}

/// Get comprehensive BIS statistics summary
#[tauri::command]
pub async fn get_bis_comprehensive_summary(app: tauri::AppHandle, ) -> Result<String, String> {
    let mut results = Vec::new();

    // Get monetary policy overview
    match get_bis_monetary_policy_overview(app.clone()).await {
        Ok(data) => results.push(("monetary_policy".to_string(), data)),
        Err(e) => results.push(("monetary_policy".to_string(), format!("Error: {}", e))),
    }

    // Get exchange rate analysis
    match get_bis_exchange_rate_analysis(app.clone()).await {
        Ok(data) => results.push(("exchange_rates".to_string(), data)),
        Err(e) => results.push(("exchange_rates".to_string(), format!("Error: {}", e))),
    }

    // Get credit conditions
    match get_bis_credit_conditions_overview(app.clone()).await {
        Ok(data) => results.push(("credit_conditions".to_string(), data)),
        Err(e) => results.push(("credit_conditions".to_string(), format!("Error: {}", e))),
    }

    // Get housing market overview
    match get_bis_housing_market_overview(app.clone()).await {
        Ok(data) => results.push(("housing_markets".to_string(), data)),
        Err(e) => results.push(("housing_markets".to_string(), format!("Error: {}", e))),
    }

    // Get data directory
    match get_bis_data_directory(app.clone()).await {
        Ok(data) => results.push(("data_directory".to_string(), data)),
        Err(e) => results.push(("data_directory".to_string(), format!("Error: {}", e))),
    }

    Ok(serde_json::json!({
        "comprehensive_summary": results,
        "description": "Complete BIS statistics overview covering monetary policy, exchange rates, credit conditions, and housing markets",
        "timestamp": chrono::Utc::now().format("%Y-%m-%d %H:%M:%S UTC").to_string()
    }).to_string())
}