use crate::ast::*;
use crate::types::*;
use super::Interpreter;

impl Interpreter {
    pub(crate) fn resolve_symbol(&mut self, expr: &Expr) -> Result<String, String> {
        match expr {
            Expr::Symbol(s) => Ok(s.clone()),
            Expr::Variable(name) => {
                if self.symbol_data.contains_key(name) {
                    return Ok(name.clone());
                }
                if let Some(val) = self.get_var(name).cloned() {
                    if let Value::String(s) = val {
                        return Ok(s);
                    }
                }
                let upper = name.to_uppercase();
                if self.symbol_data.contains_key(&upper) {
                    Ok(upper)
                } else {
                    Ok(name.clone())
                }
            }
            Expr::StringLiteral(s) => Ok(s.clone()),
            _ => {
                let val = self.evaluate_expr(expr)?;
                match val {
                    Value::String(s) => Ok(s),
                    _ => Err("Expected a symbol name".to_string()),
                }
            }
        }
    }

    pub(crate) fn resolve_number(&mut self, expr: &Expr) -> Result<f64, String> {
        let val = self.evaluate_expr(expr)?;
        val.as_number().ok_or_else(|| format!("Expected a number, got {}", val.type_name()))
    }

    pub(crate) fn resolve_color_string(&mut self, expr: &Expr) -> Result<String, String> {
        let val = self.evaluate_expr(expr)?;
        match val {
            Value::Color(c) => Ok(c.to_hex()),
            Value::String(s) => {
                if let Some(c) = ColorValue::from_name(&s) {
                    Ok(c.to_hex())
                } else {
                    Ok(s) // assume it's already a hex/rgba string
                }
            }
            _ => Ok("#2196F3".to_string()),
        }
    }

    pub(crate) fn get_last_close_price(&self) -> Option<f64> {
        for ohlcv in self.symbol_data.values() {
            if let Some(price) = ohlcv.close.last() {
                return Some(*price);
            }
        }
        None
    }

    pub(crate) fn get_any_timestamps(&self) -> Vec<i64> {
        for ohlcv in self.symbol_data.values() {
            if !ohlcv.timestamps.is_empty() {
                return ohlcv.timestamps.clone();
            }
        }
        vec![]
    }

    pub(crate) fn first_symbol(&self) -> String {
        self.symbol_data.keys().next().cloned().unwrap_or_default()
    }
}
