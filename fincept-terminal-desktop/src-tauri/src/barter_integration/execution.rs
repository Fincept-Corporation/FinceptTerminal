use crate::barter_integration::types::*;
use std::collections::HashMap;
use parking_lot::RwLock;
use std::sync::Arc;
use uuid::Uuid;
use chrono::Utc;

/// Order execution manager
pub struct ExecutionManager {
    mode: TradingMode,
    orders: Arc<RwLock<HashMap<String, OrderResponse>>>,
    positions: Arc<RwLock<HashMap<String, Position>>>,
    balances: Arc<RwLock<HashMap<String, Balance>>>,
}

impl ExecutionManager {
    pub fn new(mode: TradingMode) -> Self {
        Self {
            mode,
            orders: Arc::new(RwLock::new(HashMap::new())),
            positions: Arc::new(RwLock::new(HashMap::new())),
            balances: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Submit order
    pub async fn submit_order(&self, request: OrderRequest) -> BarterResult<OrderResponse> {
        let order_id = Uuid::new_v4().to_string();
        let client_order_id = Uuid::new_v4().to_string();

        let order = OrderResponse {
            order_id: order_id.clone(),
            client_order_id,
            exchange: request.exchange,
            symbol: request.symbol,
            side: request.side,
            order_type: request.order_type,
            status: OrderStatus::Pending,
            quantity: request.quantity,
            filled_quantity: rust_decimal::Decimal::ZERO,
            price: request.price,
            average_price: None,
            created_at: Utc::now(),
            updated_at: Utc::now(),
        };

        // Store order
        self.orders.write().insert(order_id.clone(), order.clone());

        // Simulate order processing based on mode
        match self.mode {
            TradingMode::Live => {
                // TODO: Integrate with actual exchange API
                tracing::info!("Live order submitted: {}", order_id);
            }
            TradingMode::Paper => {
                // Simulate order execution
                self.simulate_order_fill(order_id.clone()).await?;
            }
            TradingMode::Backtest => {
                // Backtest mode handled separately
            }
        }

        Ok(order)
    }

    /// Cancel order
    pub async fn cancel_order(&self, order_id: &str) -> BarterResult<OrderResponse> {
        let mut orders = self.orders.write();

        if let Some(order) = orders.get_mut(order_id) {
            if order.status == OrderStatus::Open || order.status == OrderStatus::PartiallyFilled {
                order.status = OrderStatus::Cancelled;
                order.updated_at = Utc::now();
                Ok(order.clone())
            } else {
                Err(BarterError::Order(format!(
                    "Cannot cancel order in status: {:?}",
                    order.status
                )))
            }
        } else {
            Err(BarterError::Order(format!("Order not found: {}", order_id)))
        }
    }

    /// Get order by ID
    pub fn get_order(&self, order_id: &str) -> BarterResult<OrderResponse> {
        self.orders
            .read()
            .get(order_id)
            .cloned()
            .ok_or_else(|| BarterError::Order(format!("Order not found: {}", order_id)))
    }

    /// Get all orders
    pub fn get_all_orders(&self) -> Vec<OrderResponse> {
        self.orders.read().values().cloned().collect()
    }

    /// Get open orders
    pub fn get_open_orders(&self) -> Vec<OrderResponse> {
        self.orders
            .read()
            .values()
            .filter(|o| o.status == OrderStatus::Open || o.status == OrderStatus::PartiallyFilled)
            .cloned()
            .collect()
    }

    /// Get positions
    pub fn get_positions(&self) -> Vec<Position> {
        self.positions.read().values().cloned().collect()
    }

    /// Get position for symbol
    pub fn get_position(&self, exchange: &Exchange, symbol: &str) -> Option<Position> {
        let key = format!("{}_{}", format!("{:?}", exchange), symbol);
        self.positions.read().get(&key).cloned()
    }

    /// Get balances
    pub fn get_balances(&self) -> Vec<Balance> {
        self.balances.read().values().cloned().collect()
    }

    /// Update balance
    pub fn update_balance(&self, asset: String, free: rust_decimal::Decimal, locked: rust_decimal::Decimal) {
        let balance = Balance {
            asset: asset.clone(),
            free,
            locked,
            total: free + locked,
        };
        self.balances.write().insert(asset, balance);
    }

    /// Simulate order fill (paper trading)
    async fn simulate_order_fill(&self, order_id: String) -> BarterResult<()> {
        // Simulate network delay
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

        // Clone order data before updating position
        let order_data = {
            let mut orders = self.orders.write();
            if let Some(order) = orders.get_mut(&order_id) {
                order.status = OrderStatus::Filled;
                order.filled_quantity = order.quantity;
                order.average_price = order.price;
                order.updated_at = Utc::now();

                tracing::info!("Order filled (paper): {}", order_id);
                order.clone()
            } else {
                return Ok(());
            }
        };

        // Update position after releasing orders lock
        self.update_position_from_order(&order_data);

        Ok(())
    }

    /// Update position from filled order
    fn update_position_from_order(&self, order: &OrderResponse) {
        let position_key = format!("{}_{}", format!("{:?}", order.exchange), order.symbol);
        let mut positions = self.positions.write();

        if let Some(existing_pos) = positions.get_mut(&position_key) {
            // Update existing position
            match order.side {
                OrderSide::Buy => {
                    let total_qty = existing_pos.quantity + order.filled_quantity;
                    let total_cost = (existing_pos.entry_price * existing_pos.quantity)
                        + (order.average_price.unwrap_or_default() * order.filled_quantity);
                    existing_pos.entry_price = total_cost / total_qty;
                    existing_pos.quantity = total_qty;
                }
                OrderSide::Sell => {
                    existing_pos.quantity -= order.filled_quantity;
                }
            }
        } else {
            // Create new position
            let position = Position {
                exchange: order.exchange.clone(),
                symbol: order.symbol.clone(),
                side: order.side,
                quantity: order.filled_quantity,
                entry_price: order.average_price.unwrap_or_default(),
                current_price: order.average_price.unwrap_or_default(),
                unrealized_pnl: rust_decimal::Decimal::ZERO,
                realized_pnl: rust_decimal::Decimal::ZERO,
            };
            positions.insert(position_key, position);
        }
    }

    /// Close all positions
    pub async fn close_all_positions(&self) -> BarterResult<Vec<OrderResponse>> {
        let positions = self.get_positions();
        let mut orders = Vec::new();

        for position in positions {
            let opposite_side = match position.side {
                OrderSide::Buy => OrderSide::Sell,
                OrderSide::Sell => OrderSide::Buy,
            };

            let request = OrderRequest {
                exchange: position.exchange,
                symbol: position.symbol,
                side: opposite_side,
                order_type: OrderType::Market,
                quantity: position.quantity,
                price: None,
                stop_price: None,
                time_in_force: None,
            };

            let order = self.submit_order(request).await?;
            orders.push(order);
        }

        Ok(orders)
    }
}
