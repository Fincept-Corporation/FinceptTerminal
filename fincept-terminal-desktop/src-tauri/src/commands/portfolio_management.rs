// Portfolio Management Commands - CRUD operations for portfolio tracking
use crate::database::operations;
use serde_json::Value;
use uuid::Uuid;

#[tauri::command]
pub async fn portfolio_create(
    name: String,
    owner: String,
    currency: String,
    description: Option<String>,
) -> Result<Value, String> {
    let id = Uuid::new_v4().to_string();

    operations::create_portfolio(
        &id,
        &name,
        &owner,
        &currency,
        description.as_deref(),
    )
    .map_err(|e| e.to_string())?;

    Ok(serde_json::json!({
        "id": id,
        "name": name,
        "owner": owner,
        "currency": currency,
        "description": description,
        "created_at": chrono::Utc::now().to_rfc3339(),
        "updated_at": chrono::Utc::now().to_rfc3339()
    }))
}

#[tauri::command]
pub async fn portfolio_get_all() -> Result<Vec<Value>, String> {
    operations::get_all_portfolios().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn portfolio_get_by_id(portfolio_id: String) -> Result<Option<Value>, String> {
    operations::get_portfolio_by_id(&portfolio_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn portfolio_delete(portfolio_id: String) -> Result<(), String> {
    operations::delete_portfolio(&portfolio_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn portfolio_add_asset(
    portfolio_id: String,
    symbol: String,
    quantity: f64,
    price: f64,
) -> Result<(), String> {
    let asset_id = Uuid::new_v4().to_string();
    let transaction_id = Uuid::new_v4().to_string();

    // Add asset
    operations::add_portfolio_asset(&asset_id, &portfolio_id, &symbol, quantity, price)
        .map_err(|e| e.to_string())?;

    // Record transaction
    operations::add_portfolio_transaction(
        &transaction_id,
        &portfolio_id,
        &symbol,
        "BUY",
        quantity,
        price,
        None,
    )
    .map_err(|e| e.to_string())?;

    Ok(())
}

#[tauri::command]
pub async fn portfolio_sell_asset(
    portfolio_id: String,
    symbol: String,
    quantity: f64,
    price: f64,
) -> Result<(), String> {
    let transaction_id = Uuid::new_v4().to_string();

    // Sell asset
    operations::sell_portfolio_asset(&portfolio_id, &symbol, quantity)
        .map_err(|e| e.to_string())?;

    // Record transaction
    operations::add_portfolio_transaction(
        &transaction_id,
        &portfolio_id,
        &symbol,
        "SELL",
        quantity,
        price,
        None,
    )
    .map_err(|e| e.to_string())?;

    Ok(())
}

#[tauri::command]
pub async fn portfolio_get_assets(portfolio_id: String) -> Result<Vec<Value>, String> {
    operations::get_portfolio_assets(&portfolio_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn portfolio_update_asset_symbol(
    asset_id: String,
    new_symbol: String,
) -> Result<(), String> {
    operations::update_portfolio_asset_symbol(&asset_id, &new_symbol)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn portfolio_get_transactions(
    portfolio_id: String,
    limit: Option<i32>,
) -> Result<Vec<Value>, String> {
    operations::get_portfolio_transactions(&portfolio_id, limit).map_err(|e| e.to_string())
}
