use serde::{Deserialize, Serialize};
use std::process::Command;
use std::path::PathBuf;
use tauri::command;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct StockData {
    pub date: String,
    pub open: Option<f64>,
    pub high: Option<f64>,
    pub low: Option<f64>,
    pub close: Option<f64>,
    pub volume: Option<f64>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StockResponse {
    pub symbol: String,
    pub data: Vec<StockData>,
    pub success: bool,
    pub error: Option<String>,
}

#[command]
pub async fn get_stock_data(symbol: String, period: Option<String>) -> Result<StockResponse, String> {
    let period = period.unwrap_or_else(|| "1mo".to_string());
    
    // Get the path to the Python backend
    let python_backend_path = get_python_backend_path()
        .ok_or_else(|| "Failed to get Python backend path".to_string())?;
    
    let python_script_path = python_backend_path.join("yfinance_backend.py");
    
    if !python_script_path.exists() {
        return Err(format!(
            "Python script not found at: {:?}",
            python_script_path
        ));
    }

    // Execute Python script
    let output = Command::new("python")
        .arg(&python_script_path)
        .arg(&symbol)
        .arg(&period)
        .output()
        .map_err(|e| format!("Failed to execute Python script: {}", e))?;

    if !output.status.success() {
        let error_msg = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", error_msg));
    }

    // Parse JSON response
    let json_output = String::from_utf8_lossy(&output.stdout);
    let response: serde_json::Value = serde_json::from_str(&json_output)
        .map_err(|e| format!("Failed to parse JSON response: {}", e))?;

    // Check if the request was successful
    if let Some(success) = response.get("success").and_then(|s| s.as_bool()) {
        if !success {
            let error = response.get("error")
                .and_then(|e| e.as_str())
                .unwrap_or("Unknown error");
            return Err(error.to_string());
        }
    }

    // Extract symbol and data
    let symbol_from_response = response.get("symbol")
        .and_then(|s| s.as_str())
        .unwrap_or(&symbol)
        .to_string();

    let data_array = response.get("data")
        .and_then(|d| d.as_array())
        .ok_or_else(|| "No data array in response".to_string())?;

    let mut stock_data = Vec::new();

    for item in data_array {
        if let Ok(stock_point) = serde_json::from_value::<StockData>(item.clone()) {
            stock_data.push(stock_point);
        }
    }

    Ok(StockResponse {
        symbol: symbol_from_response,
        data: stock_data,
        success: true,
        error: None,
    })
}

fn get_python_backend_path() -> Option<PathBuf> {
    // This assumes the Python backend is in a directory parallel to src-tauri
    let current_dir = std::env::current_dir().ok()?;
    
    // For development: go up from src-tauri/src/datasources to project root, then to python_backend
    let mut path = current_dir
        .parent()?  // sr
        .join("python_backend");
    
    // If we're in a release build, the path might be different
    if !path.exists() {
        // Try alternative path structure
        path = current_dir
            .parent()?  // src
            .parent()?  // src-tauri
            .join("../python_backend")
            .canonicalize()
            .ok()?;
    }
    
    if path.exists() {
        Some(path)
    } else {
        None
    }
}