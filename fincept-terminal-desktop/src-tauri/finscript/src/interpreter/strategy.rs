use crate::ast::*;
use crate::types::*;
use crate::Signal;
use super::Interpreter;

impl Interpreter {
    pub(crate) fn handle_strategy_entry(&mut self, id: &Expr, direction: &Expr, qty: &Option<Expr>, _stop: &Option<Expr>, _limit: &Option<Expr>) {
        let entry_id = match self.evaluate_expr(id) {
            Ok(Value::String(s)) => s,
            Ok(v) => self.value_to_string(&v),
            Err(e) => { self.errors.push(e); return; }
        };
        let dir = match self.evaluate_expr(direction) {
            Ok(Value::String(s)) => s.to_lowercase(),
            Ok(v) => self.value_to_string(&v).to_lowercase(),
            Err(e) => { self.errors.push(e); return; }
        };
        let quantity = if let Some(q) = qty {
            match self.evaluate_expr(q) { Ok(v) => v.as_number().unwrap_or(1.0) as i64, Err(_) => 1 }
        } else { 1 };
        let price = self.get_last_close_price().unwrap_or(0.0);
        if dir == "long" {
            self.strategy_position += quantity;
            self.strategy_entry_price = price;
            self.signals.push(Signal {
                signal_type: "Buy".to_string(),
                message: format!("strategy.entry('{}', long, qty={})", entry_id, quantity),
                timestamp: chrono::Utc::now().to_rfc3339(),
                price: Some(price),
            });
        } else {
            self.strategy_position -= quantity;
            self.strategy_entry_price = price;
            self.signals.push(Signal {
                signal_type: "Sell".to_string(),
                message: format!("strategy.entry('{}', short, qty={})", entry_id, quantity),
                timestamp: chrono::Utc::now().to_rfc3339(),
                price: Some(price),
            });
        }
        self.output_lines.push(format!("[STRATEGY] Entry '{}' {} @ {:.2}", entry_id, dir, price));
    }

    pub(crate) fn handle_strategy_exit(&mut self, id: &Expr, _qty: &Option<Expr>, _stop: &Option<Expr>, _limit: &Option<Expr>) {
        let exit_id = match self.evaluate_expr(id) {
            Ok(Value::String(s)) => s,
            Ok(v) => self.value_to_string(&v),
            Err(e) => { self.errors.push(e); return; }
        };
        let price = self.get_last_close_price().unwrap_or(0.0);
        if self.strategy_position != 0 {
            let pnl = if self.strategy_position > 0 {
                (price - self.strategy_entry_price) * self.strategy_position as f64
            } else {
                (self.strategy_entry_price - price) * (-self.strategy_position) as f64
            };
            self.strategy_equity += pnl;
            let signal_type = if self.strategy_position > 0 { "Sell" } else { "Buy" };
            self.signals.push(Signal {
                signal_type: signal_type.to_string(),
                message: format!("strategy.exit('{}') PnL: {:.2}", exit_id, pnl),
                timestamp: chrono::Utc::now().to_rfc3339(),
                price: Some(price),
            });
            self.output_lines.push(format!("[STRATEGY] Exit '{}' @ {:.2} PnL: {:.2} Equity: {:.2}", exit_id, price, pnl, self.strategy_equity));
            self.strategy_position = 0;
        }
    }

    pub(crate) fn handle_strategy_close(&mut self, id: &Expr) {
        self.handle_strategy_exit(id, &None, &None, &None);
    }
}
