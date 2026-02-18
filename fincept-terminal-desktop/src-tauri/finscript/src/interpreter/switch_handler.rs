use crate::ast::*;
use crate::types::*;
use super::{Interpreter, ControlFlow};

impl Interpreter {
    pub(crate) fn handle_switch(&mut self, expr: &Option<Expr>, cases: &[(Expr, Vec<Statement>)], default: &Option<Vec<Statement>>) -> ControlFlow {
        if let Some(switch_expr) = expr {
            // Value-based switch
            let switch_val = match self.evaluate_expr(switch_expr) {
                Ok(v) => v,
                Err(e) => { self.errors.push(e); return ControlFlow::None; }
            };
            for (case_expr, body) in cases {
                let case_val = match self.evaluate_expr(case_expr) {
                    Ok(v) => v,
                    Err(e) => { self.errors.push(e); continue; }
                };
                // Simple equality check
                let matches = match (&switch_val, &case_val) {
                    (Value::Number(a), Value::Number(b)) => (a - b).abs() < f64::EPSILON,
                    (Value::String(a), Value::String(b)) => a == b,
                    (Value::Bool(a), Value::Bool(b)) => a == b,
                    _ => false,
                };
                if matches {
                    return self.execute_block(body);
                }
            }
        } else {
            // Condition-based switch (first true case wins)
            for (case_expr, body) in cases {
                let case_val = match self.evaluate_expr(case_expr) {
                    Ok(v) => v,
                    Err(e) => { self.errors.push(e); continue; }
                };
                if case_val.is_truthy() {
                    return self.execute_block(body);
                }
            }
        }
        if let Some(def) = default {
            return self.execute_block(def);
        }
        ControlFlow::None
    }
}
