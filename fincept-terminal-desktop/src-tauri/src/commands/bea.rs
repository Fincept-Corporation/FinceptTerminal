use crate::python;
use tauri::command;

// ==================== METADATA COMMANDS ====================

#[command]
pub async fn bea_get_dataset_list(app: tauri::AppHandle) -> Result<String, String> {
    python::execute_sync(&app, "bea_data.py", vec!["dataset_list".to_string()])
}

#[command]
pub async fn bea_get_parameter_list(app: tauri::AppHandle, dataset_name: String) -> Result<String, String> {
    python::execute_sync(&app, "bea_data.py", vec!["parameter_list".to_string(), dataset_name])
}

#[command]
pub async fn bea_get_parameter_values(app: tauri::AppHandle, dataset_name: String, parameter_name: String) -> Result<String, String> {
    python::execute_sync(&app, "bea_data.py", vec!["parameter_values".to_string(), dataset_name, parameter_name])
}

#[command]
pub async fn bea_get_parameter_values_filtered(
    app: tauri::AppHandle,
    dataset_name: String,
    parameter_name: String,
    target_parameter: String
) -> Result<String, String> {
    python::execute_sync(&app, "bea_data.py", vec!["parameter_values_filtered".to_string(), dataset_name, parameter_name, target_parameter])
}

// ==================== DATA RETRIEVAL COMMANDS ====================

#[command]
pub async fn bea_get_nipa_data(app: tauri::AppHandle, table_name: String, frequency: Option<String>, year: Option<String>) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());
    let mut args = vec!["nipa".to_string(), table_name, freq];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_ni_underlying_detail(
    app: tauri::AppHandle,
    table_name: String,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());
    let mut args = vec!["ni_underlying".to_string(), table_name, freq];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_fixed_assets(app: tauri::AppHandle, table_name: String, year: Option<String>) -> Result<String, String> {
    let mut args = vec!["fixed_assets".to_string(), table_name];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_mne_data(
    app: tauri::AppHandle,
    direction: String,
    classification: Option<String>,
    year: Option<String>,
    country: Option<String>,
    industry: Option<String>,
    state: Option<String>,
    ownership_level: Option<String>,
    nonbank_affiliates_only: Option<String>,
    get_footnotes: Option<String>
) -> Result<String, String> {
    let class = classification.unwrap_or_else(|| "Country".to_string());
    let footnotes = get_footnotes.unwrap_or_else(|| "No".to_string());

    let mut args = vec!["mne".to_string(), direction, class];

    if let Some(yr) = year {
        args.push(yr);
    }
    if let Some(cnt) = country {
        args.push(cnt);
    }
    if let Some(ind) = industry {
        args.push(ind);
    }
    if let Some(st) = state {
        args.push(st);
    }
    if let Some(ol) = ownership_level {
        args.push(ol);
    }
    if let Some(nba) = nonbank_affiliates_only {
        args.push(nba);
    }

    args.push(footnotes);

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_gdp_by_industry(
    app: tauri::AppHandle,
    table_id: String,
    year: Option<String>,
    frequency: Option<String>,
    industry: Option<String>
) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());
    let ind = industry.unwrap_or_else(|| "ALL".to_string());

    let mut args = vec!["gdp_by_industry".to_string(), table_id];

    if let Some(yr) = year {
        args.push(yr);
    }

    args.push(freq);
    args.push(ind);

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_international_transactions(
    app: tauri::AppHandle,
    indicator: Option<String>,
    area_or_country: Option<String>,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let aoc = area_or_country.unwrap_or_else(|| "AllCountries".to_string());
    let freq = frequency.unwrap_or_else(|| "A".to_string());

    let mut args = vec!["international_transactions".to_string()];

    if let Some(ind) = indicator {
        args.push(ind);
    }

    args.push(aoc);
    args.push(freq);

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_international_investment_position(
    app: tauri::AppHandle,
    type_of_investment: Option<String>,
    component: Option<String>,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());

    let mut args = vec!["international_investment".to_string()];

    if let Some(toi) = type_of_investment {
        args.push(toi);
    }

    if let Some(comp) = component {
        args.push(comp);
    }

    args.push(freq);

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_input_output(app: tauri::AppHandle, table_id: String, year: Option<String>) -> Result<String, String> {
    let mut args = vec!["input_output".to_string(), table_id];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_underlying_gdp_by_industry(
    app: tauri::AppHandle,
    table_id: String,
    year: Option<String>,
    frequency: Option<String>,
    industry: Option<String>
) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());
    let ind = industry.unwrap_or_else(|| "ALL".to_string());

    let mut args = vec!["underlying_gdp_industry".to_string(), table_id];

    if let Some(yr) = year {
        args.push(yr);
    }

    args.push(freq);
    args.push(ind);

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_international_services_trade(
    app: tauri::AppHandle,
    type_of_service: Option<String>,
    trade_direction: Option<String>,
    affiliation: Option<String>,
    area_or_country: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let aoc = area_or_country.unwrap_or_else(|| "AllCountries".to_string());

    let mut args = vec!["international_services".to_string()];

    if let Some(tos) = type_of_service {
        args.push(tos);
    }

    if let Some(td) = trade_direction {
        args.push(td);
    }

    if let Some(aff) = affiliation {
        args.push(aff);
    }

    args.push(aoc);

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_regional_data(
    app: tauri::AppHandle,
    table_name: String,
    line_code: Option<String>,
    geo_fips: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let lc = line_code.unwrap_or_else(|| "ALL".to_string());
    let gf = geo_fips.unwrap_or_else(|| "STATE".to_string());

    let mut args = vec!["regional".to_string(), table_name, lc, gf];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

// ==================== COMPOSITE COMMANDS ====================

#[command]
pub async fn bea_get_economic_overview(app: tauri::AppHandle, year: Option<String>) -> Result<String, String> {
    let mut args = vec!["economic_overview".to_string()];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}

#[command]
pub async fn bea_get_regional_snapshot(
    app: tauri::AppHandle,
    geo_fips: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let gf = geo_fips.unwrap_or_else(|| "STATE".to_string());

    let mut args = vec!["regional_snapshot".to_string(), gf];

    if let Some(yr) = year {
        args.push(yr);
    }

    python::execute_sync(&app, "bea_data.py", args)
}