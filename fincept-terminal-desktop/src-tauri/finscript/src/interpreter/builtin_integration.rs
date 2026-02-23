use crate::ast::*;
use crate::types::*;
use crate::IntegrationAction;
use super::Interpreter;

impl Interpreter {
    /// watchlist_add(symbol, watchlist_name?) — Queue a symbol to be added to a watchlist
    pub(crate) fn eval_watchlist_add(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("watchlist_add() requires at least 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let watchlist_name = if args.len() > 1 {
            match self.evaluate_expr(&args[1])? {
                Value::String(s) => s,
                _ => "Default".to_string(),
            }
        } else {
            "Default".to_string()
        };

        self.integration_actions.push(IntegrationAction {
            action_type: "watchlist_add".to_string(),
            payload: serde_json::json!({
                "symbol": symbol,
                "watchlist_name": watchlist_name,
            }),
        });
        self.output_lines.push(format!("[WATCHLIST] Added {} to '{}'", symbol, watchlist_name));
        Ok(Value::Bool(true))
    }

    /// paper_trade(symbol, side, quantity, price?) — Queue a paper trade order
    pub(crate) fn eval_paper_trade(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 3 {
            return Err("paper_trade() requires 3 arguments: symbol, side, quantity".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let side = match self.evaluate_expr(&args[1])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        if side != "buy" && side != "sell" {
            return Err(format!("paper_trade() side must be 'buy' or 'sell', got '{}'", side));
        }
        let quantity = self.resolve_number(&args[2])?;
        let price = if args.len() > 3 {
            Some(self.resolve_number(&args[3])?)
        } else {
            self.get_last_close_price()
        };

        self.integration_actions.push(IntegrationAction {
            action_type: "paper_trade".to_string(),
            payload: serde_json::json!({
                "symbol": symbol,
                "side": side,
                "quantity": quantity,
                "price": price,
            }),
        });
        self.output_lines.push(format!(
            "[PAPER TRADE] {} {} {} @ {:.2}",
            side.to_uppercase(),
            quantity,
            symbol,
            price.unwrap_or(0.0)
        ));
        Ok(Value::Bool(true))
    }

    /// alert_create(message, type?) — Queue a terminal notification
    pub(crate) fn eval_alert_create(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("alert_create() requires at least 1 argument: message".to_string());
        }
        let message = match self.evaluate_expr(&args[0])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        let alert_type = if args.len() > 1 {
            match self.evaluate_expr(&args[1])? {
                Value::String(s) => s,
                _ => "info".to_string(),
            }
        } else {
            "info".to_string()
        };

        self.integration_actions.push(IntegrationAction {
            action_type: "alert_create".to_string(),
            payload: serde_json::json!({
                "message": message,
                "alert_type": alert_type,
            }),
        });
        self.output_lines.push(format!("[NOTIFY] {}", message));
        Ok(Value::Bool(true))
    }

    /// screener_scan(symbols_array) — Queue a batch screener scan
    pub(crate) fn eval_screener_scan(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("screener_scan() requires an array of symbols".to_string());
        }
        let symbols_val = self.evaluate_expr(&args[0])?;
        let symbols = match symbols_val {
            Value::Array(arr) => arr
                .iter()
                .map(|v| self.value_to_string(v))
                .collect::<Vec<_>>(),
            _ => {
                return Err("screener_scan() expects an array of symbol strings".to_string());
            }
        };

        self.integration_actions.push(IntegrationAction {
            action_type: "screener_run".to_string(),
            payload: serde_json::json!({ "symbols": symbols }),
        });
        self.output_lines.push(format!(
            "[SCREENER] Queued scan for {} symbols",
            symbols.len()
        ));
        Ok(Value::Bool(true))
    }
}
