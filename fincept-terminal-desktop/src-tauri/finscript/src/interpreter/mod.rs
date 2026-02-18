mod expressions;
mod functions;
mod builtin_indicators;
mod builtin_utils;
mod builtin_drawing;
mod strategy;
mod plotting;
mod methods;
mod switch_handler;
mod symbols;
mod helpers;

#[cfg(test)]
mod tests;

pub use symbols::collect_symbols;

use std::collections::HashMap;
use crate::ast::*;
use crate::types::*;
use crate::{Signal, PlotData, AlertInfo, DrawingInfo};

pub struct InterpreterResult {
    pub success: bool,
    pub output: String,
    pub signals: Vec<Signal>,
    pub plots: Vec<PlotData>,
    pub errors: Vec<String>,
    pub alerts: Vec<AlertInfo>,
    pub drawings: Vec<DrawingInfo>,
    pub execution_time_ms: u64,
}

pub(crate) const MAX_LOOP_ITERATIONS: usize = 100_000;

#[derive(Debug, Clone)]
pub(crate) enum ControlFlow {
    None,
    Break,
    Continue,
    Return(Value),
}

#[derive(Debug, Clone)]
pub(crate) struct UserFunction {
    pub params: Vec<String>,
    pub body: Vec<Statement>,
}

pub struct Interpreter {
    pub(crate) scopes: Vec<HashMap<String, Value>>,
    pub(crate) symbol_data: HashMap<String, OhlcvSeries>,
    pub(crate) output_lines: Vec<String>,
    pub(crate) signals: Vec<Signal>,
    pub(crate) plots: Vec<PlotData>,
    pub(crate) errors: Vec<String>,
    pub(crate) user_functions: HashMap<String, UserFunction>,
    pub(crate) alerts: Vec<AlertInfo>,
    pub(crate) drawings: Vec<DrawingInfo>,
    // Strategy state
    pub(crate) strategy_position: i64,
    pub(crate) strategy_entry_price: f64,
    pub(crate) strategy_equity: f64,
}

impl Interpreter {
    pub fn new(symbol_data: HashMap<String, OhlcvSeries>) -> Self {
        Interpreter {
            scopes: vec![HashMap::new()],
            symbol_data,
            output_lines: Vec::new(),
            signals: Vec::new(),
            plots: Vec::new(),
            errors: Vec::new(),
            user_functions: HashMap::new(),
            alerts: Vec::new(),
            drawings: Vec::new(),
            strategy_position: 0,
            strategy_entry_price: 0.0,
            strategy_equity: 10000.0,
        }
    }

    pub fn execute(&mut self, program: &Program) -> InterpreterResult {
        let start = std::time::Instant::now();

        for stmt in program {
            let cf = self.execute_statement(stmt);
            match cf {
                ControlFlow::None => {}
                ControlFlow::Break | ControlFlow::Continue => {}
                ControlFlow::Return(_) => break,
            }
        }

        let execution_time_ms = start.elapsed().as_millis() as u64;

        InterpreterResult {
            success: self.errors.is_empty(),
            output: self.output_lines.join("\n"),
            signals: self.signals.clone(),
            plots: self.plots.clone(),
            errors: self.errors.clone(),
            alerts: self.alerts.clone(),
            drawings: self.drawings.clone(),
            execution_time_ms,
        }
    }

    pub(crate) fn push_scope(&mut self) {
        self.scopes.push(HashMap::new());
    }

    pub(crate) fn pop_scope(&mut self) {
        if self.scopes.len() > 1 {
            self.scopes.pop();
        }
    }

    pub(crate) fn get_var(&self, name: &str) -> Option<&Value> {
        for scope in self.scopes.iter().rev() {
            if let Some(val) = scope.get(name) {
                return Some(val);
            }
        }
        None
    }

    pub(crate) fn set_var(&mut self, name: String, value: Value) {
        if let Some(scope) = self.scopes.last_mut() {
            scope.insert(name, value);
        }
    }

    pub(crate) fn set_var_in_any_scope(&mut self, name: &str, value: Value) {
        for scope in self.scopes.iter_mut().rev() {
            if scope.contains_key(name) {
                scope.insert(name.to_string(), value);
                return;
            }
        }
        // If not found, set in current scope
        if let Some(scope) = self.scopes.last_mut() {
            scope.insert(name.to_string(), value);
        }
    }

    pub(crate) fn execute_block(&mut self, block: &[Statement]) -> ControlFlow {
        for stmt in block {
            let cf = self.execute_statement(stmt);
            match cf {
                ControlFlow::None => {}
                other => return other,
            }
        }
        ControlFlow::None
    }

    pub(crate) fn execute_statement(&mut self, stmt: &Statement) -> ControlFlow {
        match stmt {
            Statement::Comment(_) => ControlFlow::None,
            Statement::Assignment { name, value } => {
                match self.evaluate_expr(value) {
                    Ok(val) => self.set_var_in_any_scope(name, val),
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::CompoundAssign { name, op, value } => {
                let current = self.get_var(name).cloned().unwrap_or(Value::Number(0.0));
                match self.evaluate_expr(value) {
                    Ok(rhs) => {
                        match self.eval_binary_op(&current, op, &rhs) {
                            Ok(result) => self.set_var_in_any_scope(name, result),
                            Err(e) => self.errors.push(e),
                        }
                    }
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::IndexAssign { object, index, value } => {
                let idx_val = match self.evaluate_expr(index) {
                    Ok(v) => v,
                    Err(e) => { self.errors.push(e); return ControlFlow::None; }
                };
                let new_val = match self.evaluate_expr(value) {
                    Ok(v) => v,
                    Err(e) => { self.errors.push(e); return ControlFlow::None; }
                };
                let idx = idx_val.as_number().unwrap_or(0.0) as usize;
                let current = self.get_var(object).cloned();
                if let Some(Value::Array(mut arr)) = current {
                    if idx < arr.len() {
                        arr[idx] = new_val;
                        self.set_var_in_any_scope(object, Value::Array(arr));
                    } else {
                        self.errors.push(format!("Index {} out of bounds (len {})", idx, arr.len()));
                    }
                } else if let Some(Value::Map(mut m)) = current {
                    let key = match idx_val {
                        Value::String(s) => s,
                        v => self.value_to_string(&v),
                    };
                    m.insert(key, new_val);
                    self.set_var_in_any_scope(object, Value::Map(m));
                }
                ControlFlow::None
            }
            Statement::IfBlock { condition, body, else_if_blocks, else_body } => {
                match self.evaluate_expr(condition) {
                    Ok(val) => {
                        if val.is_truthy() {
                            self.push_scope();
                            let cf = self.execute_block(body);
                            self.pop_scope();
                            return cf;
                        }
                        for (elif_cond, elif_body) in else_if_blocks {
                            match self.evaluate_expr(elif_cond) {
                                Ok(val) if val.is_truthy() => {
                                    self.push_scope();
                                    let cf = self.execute_block(elif_body);
                                    self.pop_scope();
                                    return cf;
                                }
                                Err(e) => { self.errors.push(e); return ControlFlow::None; }
                                _ => {}
                            }
                        }
                        if let Some(eb) = else_body {
                            self.push_scope();
                            let cf = self.execute_block(eb);
                            self.pop_scope();
                            return cf;
                        }
                    }
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::ForLoop { var_name, iterable, body } => {
                let iter_val = match self.evaluate_expr(iterable) {
                    Ok(v) => v,
                    Err(e) => { self.errors.push(e); return ControlFlow::None; }
                };
                let items = match iter_val {
                    Value::Array(a) => a,
                    Value::Series(s) => s.values.into_iter().map(Value::Number).collect(),
                    _ => { self.errors.push("for loop requires an iterable (array, range, or series)".to_string()); return ControlFlow::None; }
                };
                self.push_scope();
                let mut iteration = 0;
                for item in items {
                    if iteration >= MAX_LOOP_ITERATIONS {
                        self.errors.push("Maximum loop iterations exceeded".to_string());
                        break;
                    }
                    self.set_var(var_name.clone(), item);
                    let cf = self.execute_block(body);
                    match cf {
                        ControlFlow::Break => break,
                        ControlFlow::Continue => { iteration += 1; continue; }
                        ControlFlow::Return(v) => { self.pop_scope(); return ControlFlow::Return(v); }
                        ControlFlow::None => {}
                    }
                    iteration += 1;
                }
                self.pop_scope();
                ControlFlow::None
            }
            Statement::WhileLoop { condition, body } => {
                let mut iteration = 0;
                loop {
                    if iteration >= MAX_LOOP_ITERATIONS {
                        self.errors.push("Maximum loop iterations exceeded".to_string());
                        break;
                    }
                    let cond_val = match self.evaluate_expr(condition) {
                        Ok(v) => v,
                        Err(e) => { self.errors.push(e); break; }
                    };
                    if !cond_val.is_truthy() { break; }
                    self.push_scope();
                    let cf = self.execute_block(body);
                    self.pop_scope();
                    match cf {
                        ControlFlow::Break => break,
                        ControlFlow::Continue => { iteration += 1; continue; }
                        ControlFlow::Return(v) => return ControlFlow::Return(v),
                        ControlFlow::None => {}
                    }
                    iteration += 1;
                }
                ControlFlow::None
            }
            Statement::FnDef { name, params, body } => {
                self.user_functions.insert(name.clone(), UserFunction {
                    params: params.clone(),
                    body: body.clone(),
                });
                ControlFlow::None
            }
            Statement::Return { value } => {
                let val = if let Some(expr) = value {
                    match self.evaluate_expr(expr) {
                        Ok(v) => v,
                        Err(e) => { self.errors.push(e); Value::Void }
                    }
                } else {
                    Value::Void
                };
                ControlFlow::Return(val)
            }
            Statement::Break => ControlFlow::Break,
            Statement::Continue => ControlFlow::Continue,
            Statement::Buy { message } => {
                let msg = match self.evaluate_expr(message) {
                    Ok(Value::String(s)) => s,
                    Ok(v) => self.value_to_string(&v),
                    Err(e) => { self.errors.push(e); return ControlFlow::None; }
                };
                let price = self.get_last_close_price();
                self.signals.push(Signal {
                    signal_type: "Buy".to_string(),
                    message: msg.clone(),
                    timestamp: chrono::Utc::now().to_rfc3339(),
                    price,
                });
                self.output_lines.push(format!("[BUY] {}", msg));
                ControlFlow::None
            }
            Statement::Sell { message } => {
                let msg = match self.evaluate_expr(message) {
                    Ok(Value::String(s)) => s,
                    Ok(v) => self.value_to_string(&v),
                    Err(e) => { self.errors.push(e); return ControlFlow::None; }
                };
                let price = self.get_last_close_price();
                self.signals.push(Signal {
                    signal_type: "Sell".to_string(),
                    message: msg.clone(),
                    timestamp: chrono::Utc::now().to_rfc3339(),
                    price,
                });
                self.output_lines.push(format!("[SELL] {}", msg));
                ControlFlow::None
            }
            Statement::Plot { expr, label } => {
                self.handle_plot(expr, label);
                ControlFlow::None
            }
            Statement::PlotCandlestick { symbol, title } => {
                self.handle_plot_candlestick(symbol, title);
                ControlFlow::None
            }
            Statement::PlotLine { value, label, color } => {
                self.handle_plot_line(value, label, color);
                ControlFlow::None
            }
            Statement::PlotHistogram { value, label, color_up, color_down } => {
                self.handle_plot_histogram(value, label, color_up, color_down);
                ControlFlow::None
            }
            Statement::PlotShape { condition, shape, location, color, text } => {
                self.handle_plot_shape(condition, shape, location, color, text);
                ControlFlow::None
            }
            Statement::Bgcolor { color, condition } => {
                if let Some(cond) = condition {
                    match self.evaluate_expr(cond) {
                        Ok(v) if !v.is_truthy() => return ControlFlow::None,
                        Err(e) => { self.errors.push(e); return ControlFlow::None; }
                        _ => {}
                    }
                }
                match self.evaluate_expr(color) {
                    Ok(Value::String(c)) => self.output_lines.push(format!("[BGCOLOR] {}", c)),
                    Ok(Value::Color(c)) => self.output_lines.push(format!("[BGCOLOR] {}", c.to_hex())),
                    _ => {}
                }
                ControlFlow::None
            }
            Statement::Hline { value, label, color } => {
                self.handle_hline(value, label, color);
                ControlFlow::None
            }
            Statement::SwitchBlock { expr, cases, default } => {
                self.handle_switch(expr, cases, default)
            }
            Statement::StrategyEntry { id, direction, qty, price: _, stop, limit } => {
                self.handle_strategy_entry(id, direction, qty, stop, limit);
                ControlFlow::None
            }
            Statement::StrategyExit { id, from_entry: _, qty, stop, limit, trail_points: _, trail_offset: _ } => {
                self.handle_strategy_exit(id, qty, stop, limit);
                ControlFlow::None
            }
            Statement::StrategyClose { id } => {
                self.handle_strategy_close(id);
                ControlFlow::None
            }
            Statement::InputDecl { name, input_type: _, default_value, title: _ } => {
                match self.evaluate_expr(default_value) {
                    Ok(val) => {
                        if !name.is_empty() {
                            self.set_var(name.clone(), val);
                        }
                    }
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::StructDef { name, fields: _ } => {
                self.output_lines.push(format!("[STRUCT] Defined '{}'", name));
                ControlFlow::None
            }
            Statement::AlertStatement { message, alert_type } => {
                let msg = match self.evaluate_expr(message) {
                    Ok(Value::String(s)) => s,
                    Ok(v) => self.value_to_string(&v),
                    Err(e) => { self.errors.push(e); return ControlFlow::None; }
                };
                let atype = if let Some(at) = alert_type {
                    match self.evaluate_expr(at) {
                        Ok(Value::String(s)) => s,
                        _ => "once".to_string(),
                    }
                } else { "once".to_string() };
                self.output_lines.push(format!("[ALERT] {}", msg));
                self.alerts.push(AlertInfo {
                    message: msg,
                    alert_type: atype,
                    timestamp: chrono::Utc::now().to_rfc3339(),
                });
                ControlFlow::None
            }
            Statement::PrintStatement { args } => {
                let mut parts = Vec::new();
                for arg in args {
                    match self.evaluate_expr(arg) {
                        Ok(val) => parts.push(self.value_to_string(&val)),
                        Err(e) => { self.errors.push(e); return ControlFlow::None; }
                    }
                }
                self.output_lines.push(parts.join(" "));
                ControlFlow::None
            }
            Statement::ImportStatement { module, alias: _ } => {
                self.output_lines.push(format!("[IMPORT] {}", module));
                ControlFlow::None
            }
            Statement::ExportStatement { name } => {
                self.output_lines.push(format!("[EXPORT] {}", name));
                ControlFlow::None
            }
            Statement::ExprStatement(expr) => {
                match self.evaluate_expr(expr) {
                    Ok(_) => {}
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
        }
    }

    pub(crate) fn value_to_string(&self, val: &Value) -> String {
        match val {
            Value::Number(n) => {
                if *n == (*n as i64) as f64 { format!("{}", *n as i64) }
                else { format!("{:.4}", n) }
            }
            Value::Series(s) => {
                let non_nan: Vec<f64> = s.values.iter().filter(|v| !v.is_nan()).copied().collect();
                if non_nan.is_empty() { "Series(empty)".to_string() }
                else { format!("Series({} pts, last: {:.4})", non_nan.len(), non_nan.last().unwrap_or(&0.0)) }
            }
            Value::String(s) => s.clone(),
            Value::Bool(b) => b.to_string(),
            Value::Array(a) => {
                let items: Vec<String> = a.iter().take(5).map(|v| self.value_to_string(v)).collect();
                if a.len() > 5 { format!("[{}, ... ({} items)]", items.join(", "), a.len()) }
                else { format!("[{}]", items.join(", ")) }
            }
            Value::Map(m) => format!("Map({} entries)", m.len()),
            Value::Color(c) => c.to_hex(),
            Value::Struct { type_name, fields } => format!("{}({} fields)", type_name, fields.len()),
            Value::Drawing(d) => format!("Drawing({})", d.drawing_type),
            Value::Table(t) => format!("Table({}x{}, {})", t.rows, t.cols, t.position),
            Value::Na => "na".to_string(),
            Value::Void => "void".to_string(),
        }
    }
}
