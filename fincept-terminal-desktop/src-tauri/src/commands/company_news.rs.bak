use crate::utils::python::{execute_python_command, get_script_path};
use tauri::AppHandle;

/// Fetch company news using GNews
///
/// # Arguments
/// * `query` - Company name or symbol (e.g., "Apple", "AAPL", "Tesla Inc")
/// * `max_results` - Maximum number of articles to return (default: 10)
/// * `period` - Time period: "7d", "1M", "1y" (default: "7d")
/// * `language` - Language code (default: "en")
/// * `country` - Country code (default: "US")
#[tauri::command]
pub async fn fetch_company_news(
    app: AppHandle,
    query: String,
    max_results: Option<u32>,
    period: Option<String>,
    language: Option<String>,
    country: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["search".to_string(), query.clone()];

    if let Some(max) = max_results {
        args.push(max.to_string());
    }

    if let Some(p) = period {
        args.push(p);
    }

    if let Some(lang) = language {
        args.push(lang);
    }

    if let Some(c) = country {
        args.push(c);
    }

    execute_news_command(app, args).await
}

/// Fetch news by topic
///
/// # Arguments
/// * `topic` - Topic name (WORLD, NATION, BUSINESS, TECHNOLOGY, ENTERTAINMENT, SPORTS, SCIENCE, HEALTH)
/// * `max_results` - Maximum number of articles to return
#[tauri::command]
pub async fn fetch_news_by_topic(
    app: AppHandle,
    topic: String,
    max_results: Option<u32>,
) -> Result<String, String> {
    let mut args = vec!["topic".to_string(), topic];

    if let Some(max) = max_results {
        args.push(max.to_string());
    }

    execute_news_command(app, args).await
}

/// Get full article content from URL
///
/// # Arguments
/// * `url` - Article URL
#[tauri::command]
pub async fn get_full_article(
    app: AppHandle,
    url: String,
) -> Result<String, String> {
    let args = vec!["article".to_string(), url];
    execute_news_command(app, args).await
}

/// Get help information for company news commands
#[tauri::command]
pub async fn get_company_news_help(app: AppHandle) -> Result<String, String> {
    let args = vec!["help".to_string()];
    execute_news_command(app, args).await
}

/// Internal helper function to execute news commands
async fn execute_news_command(app: AppHandle, args: Vec<String>) -> Result<String, String> {
    let script_name = "fetch_company_news.py";
    let script_path = get_script_path(&app, script_name)
        .map_err(|e| format!("Failed to locate script: {}", e))?;

    if !script_path.exists() {
        return Err(format!(
            "Company news script not found at: {}",
            script_path.display()
        ));
    }

    match execute_python_command(&app, script_name, &args) {
        Ok(output) => {
            if output.contains("\"success\":false") || output.contains("\"error\"") {
                return Err(output);
            }
            Ok(output)
        }
        Err(e) => Err(format!("News fetch failed: {}", e)),
    }
}
