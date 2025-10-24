use tauri::command;

// ==================== METADATA COMMANDS ====================

#[command]
pub async fn bea_get_dataset_list() -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/bea_data.py")
        .arg("dataset_list")
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_parameter_list(dataset_name: String) -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/bea_data.py")
        .arg("parameter_list")
        .arg(&dataset_name)
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_parameter_values(dataset_name: String, parameter_name: String) -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/bea_data.py")
        .arg("parameter_values")
        .arg(&dataset_name)
        .arg(&parameter_name)
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_parameter_values_filtered(
    dataset_name: String,
    parameter_name: String,
    target_parameter: String
) -> Result<String, String> {
    let output = crate::utils::python::python_command()
        .arg("resources/scripts/bea_data.py")
        .arg("parameter_values_filtered")
        .arg(&dataset_name)
        .arg(&parameter_name)
        .arg(&target_parameter)
        .output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== DATA RETRIEVAL COMMANDS ====================

#[command]
pub async fn bea_get_nipa_data(table_name: String, frequency: Option<String>, year: Option<String>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("nipa")
        .arg(&table_name);

    if let Some(freq) = frequency {
        cmd.arg(&freq);
    } else {
        cmd.arg("A"); // Default to Annual
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_ni_underlying_detail(
    table_name: String,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("ni_underlying")
        .arg(&table_name);

    if let Some(freq) = frequency {
        cmd.arg(&freq);
    } else {
        cmd.arg("A"); // Default to Annual
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_fixed_assets(table_name: String, year: Option<String>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("fixed_assets")
        .arg(&table_name);

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_mne_data(
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
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("mne")
        .arg(&direction);

    if let Some(class) = classification {
        cmd.arg(&class);
    } else {
        cmd.arg("Country"); // Default
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    if let Some(cnt) = country {
        cmd.arg(&cnt);
    }

    if let Some(ind) = industry {
        cmd.arg(&ind);
    }

    if let Some(st) = state {
        cmd.arg(&st);
    }

    if let Some(ol) = ownership_level {
        cmd.arg(&ol);
    }

    if let Some(nba) = nonbank_affiliates_only {
        cmd.arg(&nba);
    }

    if let Some(gf) = get_footnotes {
        cmd.arg(&gf);
    } else {
        cmd.arg("No"); // Default
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_gdp_by_industry(
    table_id: String,
    year: Option<String>,
    frequency: Option<String>,
    industry: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("gdp_by_industry")
        .arg(&table_id);

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    if let Some(freq) = frequency {
        cmd.arg(&freq);
    } else {
        cmd.arg("A"); // Default to Annual
    }

    if let Some(ind) = industry {
        cmd.arg(&ind);
    } else {
        cmd.arg("ALL"); // Default
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_international_transactions(
    indicator: Option<String>,
    area_or_country: Option<String>,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("international_transactions");

    if let Some(ind) = indicator {
        cmd.arg(&ind);
    }

    if let Some(aoc) = area_or_country {
        cmd.arg(&aoc);
    } else {
        cmd.arg("AllCountries"); // Default
    }

    if let Some(freq) = frequency {
        cmd.arg(&freq);
    } else {
        cmd.arg("A"); // Default to Annual
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_international_investment_position(
    type_of_investment: Option<String>,
    component: Option<String>,
    frequency: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("international_investment");

    if let Some(toi) = type_of_investment {
        cmd.arg(&toi);
    }

    if let Some(comp) = component {
        cmd.arg(&comp);
    }

    if let Some(freq) = frequency {
        cmd.arg(&freq);
    } else {
        cmd.arg("A"); // Default to Annual
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_input_output(table_id: String, year: Option<String>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("input_output")
        .arg(&table_id);

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_underlying_gdp_by_industry(
    table_id: String,
    year: Option<String>,
    frequency: Option<String>,
    industry: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("underlying_gdp_industry")
        .arg(&table_id);

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    if let Some(freq) = frequency {
        cmd.arg(&freq);
    } else {
        cmd.arg("A"); // Default to Annual
    }

    if let Some(ind) = industry {
        cmd.arg(&ind);
    } else {
        cmd.arg("ALL"); // Default
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_international_services_trade(
    type_of_service: Option<String>,
    trade_direction: Option<String>,
    affiliation: Option<String>,
    area_or_country: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("international_services");

    if let Some(tos) = type_of_service {
        cmd.arg(&tos);
    }

    if let Some(td) = trade_direction {
        cmd.arg(&td);
    }

    if let Some(aff) = affiliation {
        cmd.arg(&aff);
    }

    if let Some(aoc) = area_or_country {
        cmd.arg(&aoc);
    } else {
        cmd.arg("AllCountries"); // Default
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_regional_data(
    table_name: String,
    line_code: Option<String>,
    geo_fips: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("regional")
        .arg(&table_name);

    if let Some(lc) = line_code {
        cmd.arg(&lc);
    } else {
        cmd.arg("ALL"); // Default
    }

    if let Some(gf) = geo_fips {
        cmd.arg(&gf);
    } else {
        cmd.arg("STATE"); // Default
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

// ==================== COMPOSITE COMMANDS ====================

#[command]
pub async fn bea_get_economic_overview(year: Option<String>) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("economic_overview");

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}

#[command]
pub async fn bea_get_regional_snapshot(
    geo_fips: Option<String>,
    year: Option<String>
) -> Result<String, String> {
    let mut cmd = crate::utils::python::python_command();
    cmd.arg("resources/scripts/bea_data.py")
        .arg("regional_snapshot");

    if let Some(gf) = geo_fips {
        cmd.arg(&gf);
    } else {
        cmd.arg("STATE"); // Default
    }

    if let Some(yr) = year {
        cmd.arg(&yr);
    }

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python command: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}