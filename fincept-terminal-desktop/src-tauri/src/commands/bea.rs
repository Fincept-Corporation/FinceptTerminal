use crate::utils::python::execute_python_script_simple;
use tauri::command;

// ==================== METADATA COMMANDS ====================

#[command]
pub async fn bea_get_dataset_list(app: tauri::AppHandle) -> Result<String, String> {
    execute_python_script_simple(&app, "bea_data.py", &["dataset_list"])
}

#[command]
pub async fn bea_get_parameter_list(app: tauri::AppHandle, dataset_name: String) -> Result<String, String> {
    execute_python_script_simple(&app, "bea_data.py", &["parameter_list", &dataset_name])
}

#[command]
pub async fn bea_get_parameter_values(app: tauri::AppHandle, dataset_name: String, parameter_name: String) -> Result<String, String> {
    execute_python_script_simple(&app, "bea_data.py", &["parameter_values", &dataset_name, &parameter_name])
}

#[command]
pub async fn bea_get_parameter_values_filtered(
    app: tauri::AppHandle,
    dataset_name: String,
    parameter_name: String,
    target_parameter: String
) -> Result<String, String> {
    execute_python_script_simple(&app, "bea_data.py", &["parameter_values_filtered", &dataset_name, &parameter_name, &target_parameter])
}

// ==================== DATA RETRIEVAL COMMANDS ====================

#[command]
pub async fn bea_get_nipa_data(app: tauri::AppHandle, table_name: String, frequency: Option<String>, year: Option<String>) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());
    let mut args = vec!["nipa", &table_name, &freq];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
}

#[command]
pub async fn bea_get_ni_underlying_detail(
    app: tauri::AppHandle,
    table_name: String,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let freq = frequency.unwrap_or_else(|| "A".to_string());
    let mut args = vec!["ni_underlying", &table_name, &freq];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
}

#[command]
pub async fn bea_get_fixed_assets(app: tauri::AppHandle, table_name: String, year: Option<String>) -> Result<String, String> {
    let mut args = vec!["fixed_assets", &table_name];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["mne", &direction, &class];

    let year_str;
    let country_str;
    let industry_str;
    let state_str;
    let ownership_str;
    let nonbank_str;

    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }
    if let Some(cnt) = country {
        country_str = cnt;
        args.push(&country_str);
    }
    if let Some(ind) = industry {
        industry_str = ind;
        args.push(&industry_str);
    }
    if let Some(st) = state {
        state_str = st;
        args.push(&state_str);
    }
    if let Some(ol) = ownership_level {
        ownership_str = ol;
        args.push(&ownership_str);
    }
    if let Some(nba) = nonbank_affiliates_only {
        nonbank_str = nba;
        args.push(&nonbank_str);
    }

    args.push(&footnotes);

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["gdp_by_industry", &table_id];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    args.push(&freq);
    args.push(&ind);

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["international_transactions"];

    let indicator_str;
    let year_str;

    if let Some(ind) = indicator {
        indicator_str = ind;
        args.push(&indicator_str);
    }

    args.push(&aoc);
    args.push(&freq);

    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["international_investment"];

    let toi_str;
    let comp_str;
    let year_str;

    if let Some(toi) = type_of_investment {
        toi_str = toi;
        args.push(&toi_str);
    }

    if let Some(comp) = component {
        comp_str = comp;
        args.push(&comp_str);
    }

    args.push(&freq);

    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
}

#[command]
pub async fn bea_get_input_output(app: tauri::AppHandle, table_id: String, year: Option<String>) -> Result<String, String> {
    let mut args = vec!["input_output", &table_id];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["underlying_gdp_industry", &table_id];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    args.push(&freq);
    args.push(&ind);

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["international_services"];

    let tos_str;
    let td_str;
    let aff_str;
    let year_str;

    if let Some(tos) = type_of_service {
        tos_str = tos;
        args.push(&tos_str);
    }

    if let Some(td) = trade_direction {
        td_str = td;
        args.push(&td_str);
    }

    if let Some(aff) = affiliation {
        aff_str = aff;
        args.push(&aff_str);
    }

    args.push(&aoc);

    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
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

    let mut args = vec!["regional", &table_name, &lc, &gf];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
}

// ==================== COMPOSITE COMMANDS ====================

#[command]
pub async fn bea_get_economic_overview(app: tauri::AppHandle, year: Option<String>) -> Result<String, String> {
    let mut args = vec!["economic_overview"];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
}

#[command]
pub async fn bea_get_regional_snapshot(
    app: tauri::AppHandle,
    geo_fips: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let gf = geo_fips.unwrap_or_else(|| "STATE".to_string());

    let mut args = vec!["regional_snapshot", &gf];

    let year_str;
    if let Some(yr) = year {
        year_str = yr;
        args.push(&year_str);
    }

    execute_python_script_simple(&app, "bea_data.py", &args)
}