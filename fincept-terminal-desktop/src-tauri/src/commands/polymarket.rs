// Polymarket prediction markets data commands
use crate::python;

#[tauri::command]
pub async fn fetch_polymarket_markets(app: tauri::AppHandle, limit: Option<u32>) -> Result<String, String> {
    let limit_str = limit.unwrap_or(10).to_string();
    python::execute(&app, "polymarket.py", vec!["get_markets".to_string(), limit_str]).await
}
