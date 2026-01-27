// LLM Models Commands - Fetch models from embedded JSON data
// No external Python dependencies required - uses pre-extracted model data
use serde::{Deserialize, Serialize};
use tauri::AppHandle;

use crate::python;

#[derive(Debug, Serialize, Deserialize)]
pub struct LLMProvider {
    pub id: String,
    pub name: String,
    pub model_count: i32,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct LLMModel {
    pub id: String,
    pub name: String,
    pub provider: String,
    #[serde(default)]
    pub description: Option<String>,
    #[serde(default)]
    pub context_window: Option<i64>,
    #[serde(default)]
    pub input_cost_per_token: Option<f64>,
    #[serde(default)]
    pub output_cost_per_token: Option<f64>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct LLMStats {
    pub total_providers: i32,
    pub total_models: i32,
    pub top_providers: Vec<TopProvider>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TopProvider {
    pub provider: String,
    pub model_count: i32,
}

/// Get all available LLM providers from embedded model data
#[tauri::command]
pub fn get_llm_providers(app: AppHandle) -> Result<Vec<LLMProvider>, String> {
    let output = python::execute_sync(
        &app,
        "litellm_models.py",
        vec!["providers".to_string()],
    )?;

    let providers: Vec<LLMProvider> = serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse providers: {} - Output: {}", e, output))?;

    // Check for error in response
    if providers.len() == 1 {
        if let Some(first) = providers.first() {
            if first.id == "error" {
                return Err(first.name.clone());
            }
        }
    }

    Ok(providers)
}

/// Get all models for a specific provider from embedded model data
#[tauri::command]
pub fn get_llm_models_by_provider(app: AppHandle, provider: String) -> Result<Vec<LLMModel>, String> {
    let output = python::execute_sync(
        &app,
        "litellm_models.py",
        vec!["models".to_string(), provider.clone()],
    )?;

    let models: Vec<LLMModel> = serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse models for {}: {} - Output: {}", provider, e, output))?;

    Ok(models)
}

/// Get all models from all providers
#[tauri::command]
pub fn get_all_llm_models(app: AppHandle) -> Result<Vec<LLMModel>, String> {
    let output = python::execute_sync(
        &app,
        "litellm_models.py",
        vec!["models".to_string()],
    )?;

    let models: Vec<LLMModel> = serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse all models: {} - Output: {}", e, output))?;

    Ok(models)
}

/// Get LLM provider statistics
#[tauri::command]
pub fn get_llm_stats(app: AppHandle) -> Result<LLMStats, String> {
    let output = python::execute_sync(
        &app,
        "litellm_models.py",
        vec!["stats".to_string()],
    )?;

    let stats: LLMStats = serde_json::from_str(&output)
        .map_err(|e| format!("Failed to parse stats: {} - Output: {}", e, output))?;

    Ok(stats)
}

/// Search models by name or provider
#[tauri::command]
pub fn search_llm_models(app: AppHandle, query: String) -> Result<Vec<LLMModel>, String> {
    let output = python::execute_sync(
        &app,
        "litellm_models.py",
        vec!["search".to_string(), query.clone()],
    )?;

    let models: Vec<LLMModel> = serde_json::from_str(&output)
        .map_err(|e| format!("Failed to search models for '{}': {} - Output: {}", query, e, output))?;

    Ok(models)
}
