use std::collections::HashMap;

use crate::ast::*;
use crate::indicators;
use crate::types::*;
use crate::PlotData;
use crate::PlotPoint;
use crate::Signal;
use crate::AlertInfo;
use crate::DrawingInfo;

const MAX_LOOP_ITERATIONS: usize = 100_000;

#[derive(Debug, Clone)]
enum ControlFlow {
    None,
    Break,
    Continue,
    Return(Value),
}

#[derive(Debug, Clone)]
struct UserFunction {
    params: Vec<String>,
    body: Vec<Statement>,
}

pub struct InterpreterResult {
    pub output: String,
    pub signals: Vec<Signal>,
    pub plots: Vec<PlotData>,
    pub errors: Vec<String>,
    pub alerts: Vec<AlertInfo>,
    pub drawings: Vec<DrawingInfo>,
}

pub struct Interpreter {
    env: Vec<HashMap<String, Value>>,  // scope stack
    symbol_data: SymbolDataMap,
    user_functions: HashMap<String, UserFunction>,
    struct_defs: HashMap<String, Vec<(String, String)>>,  // struct definitions
    signals: Vec<Signal>,
    plots: Vec<PlotData>,
    errors: Vec<String>,
    output_lines: Vec<String>,
    alerts: Vec<AlertInfo>,
    drawings: Vec<DrawingInfo>,
    exports: HashMap<String, Value>,
    // Strategy state
    strategy_position: i64,     // +ve = long, -ve = short, 0 = flat
    strategy_equity: f64,
    strategy_entry_price: f64,
    // Input defaults
    inputs: HashMap<String, Value>,
}

impl Interpreter {
    pub fn new(symbol_data: SymbolDataMap) -> Self {
        Interpreter {
            env: vec![HashMap::new()],  // global scope
            symbol_data,
            user_functions: HashMap::new(),
            struct_defs: HashMap::new(),
            signals: Vec::new(),
            plots: Vec::new(),
            errors: Vec::new(),
            output_lines: Vec::new(),
            alerts: Vec::new(),
            drawings: Vec::new(),
            exports: HashMap::new(),
            strategy_position: 0,
            strategy_equity: 100000.0,
            strategy_entry_price: 0.0,
            inputs: HashMap::new(),
        }
    }

    fn push_scope(&mut self) {
        self.env.push(HashMap::new());
    }

    fn pop_scope(&mut self) {
        if self.env.len() > 1 {
            self.env.pop();
        }
    }

    fn set_var(&mut self, name: String, val: Value) {
        if let Some(scope) = self.env.last_mut() {
            scope.insert(name, val);
        }
    }

    fn get_var(&self, name: &str) -> Option<&Value> {
        // Search from innermost to outermost scope
        for scope in self.env.iter().rev() {
            if let Some(val) = scope.get(name) {
                return Some(val);
            }
        }
        None
    }

    fn set_var_in_any_scope(&mut self, name: &str, val: Value) {
        // Try to update in existing scope first
        for scope in self.env.iter_mut().rev() {
            if scope.contains_key(name) {
                scope.insert(name.to_string(), val);
                return;
            }
        }
        // Otherwise set in current scope
        self.set_var(name.to_string(), val);
    }

    pub fn execute(&mut self, program: &Program) -> InterpreterResult {
        for stmt in program {
            let cf = self.execute_statement(stmt);
            if matches!(cf, ControlFlow::Return(_)) {
                break;
            }
        }

        InterpreterResult {
            output: self.output_lines.join("\n"),
            signals: self.signals.clone(),
            plots: self.plots.clone(),
            errors: self.errors.clone(),
            alerts: self.alerts.clone(),
            drawings: self.drawings.clone(),
        }
    }

    fn execute_statement(&mut self, stmt: &Statement) -> ControlFlow {
        match stmt {
            Statement::Comment(_) => ControlFlow::None,
            Statement::PrintStatement { args } => {
                let mut parts = Vec::new();
                for arg in args {
                    match self.evaluate_expr(arg) {
                        Ok(val) => parts.push(self.value_to_string(&val)),
                        Err(e) => parts.push(format!("<error: {}>", e)),
                    }
                }
                self.output_lines.push(parts.join(" "));
                ControlFlow::None
            }
            Statement::Assignment { name, value } => {
                match self.evaluate_expr(value) {
                    Ok(val) => { self.set_var_in_any_scope(name, val); }
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::CompoundAssign { name, op, value } => {
                match self.evaluate_expr(value) {
                    Ok(rval) => {
                        let current = self.get_var(name).cloned();
                        if let Some(lval) = current {
                            match self.eval_binary_op(&lval, op, &rval) {
                                Ok(result) => {
                                    self.set_var_in_any_scope(name, result);
                                }
                                Err(e) => self.errors.push(e),
                            }
                        } else {
                            self.errors.push(format!("Undefined variable '{}'", name));
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
                let idx = match idx_val.as_number() {
                    Some(n) => n as usize,
                    None => { self.errors.push("Array index must be a number".to_string()); return ControlFlow::None; }
                };
                let current = self.get_var(object).cloned();
                if let Some(Value::Array(mut arr)) = current {
                    if idx < arr.len() {
                        arr[idx] = new_val;
                        self.set_var_in_any_scope(object, Value::Array(arr));
                    } else {
                        self.errors.push(format!("Index {} out of bounds (len {})", idx, arr.len()));
                    }
                } else {
                    self.errors.push(format!("'{}' is not an array", object));
                }
                ControlFlow::None
            }
            Statement::IfBlock { condition, body, else_if_blocks, else_body } => {
                match self.evaluate_expr(condition) {
                    Ok(val) => {
                        if val.is_truthy() {
                            return self.execute_block(body);
                        }
                        for (elif_cond, elif_body) in else_if_blocks {
                            match self.evaluate_expr(elif_cond) {
                                Ok(v) if v.is_truthy() => {
                                    return self.execute_block(elif_body);
                                }
                                Err(e) => { self.errors.push(e); return ControlFlow::None; }
                                _ => {}
                            }
                        }
                        if let Some(eb) = else_body {
                            return self.execute_block(eb);
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

                let items: Vec<Value> = match iter_val {
                    Value::Array(arr) => arr,
                    Value::Series(s) => {
                        s.values.iter().map(|v| Value::Number(*v)).collect()
                    }
                    Value::Number(n) => {
                        // Treat as range 0..n
                        let count = n as usize;
                        (0..count.min(MAX_LOOP_ITERATIONS)).map(|i| Value::Number(i as f64)).collect()
                    }
                    _ => {
                        self.errors.push(format!("Cannot iterate over {}", iter_val.type_name()));
                        return ControlFlow::None;
                    }
                };

                self.push_scope();
                for (i, item) in items.into_iter().enumerate() {
                    if i >= MAX_LOOP_ITERATIONS {
                        self.errors.push("Loop iteration limit exceeded".to_string());
                        break;
                    }
                    self.set_var(var_name.clone(), item);
                    let cf = self.execute_block_inner(body);
                    match cf {
                        ControlFlow::Break => break,
                        ControlFlow::Continue => continue,
                        ControlFlow::Return(_) => { self.pop_scope(); return cf; }
                        _ => {}
                    }
                }
                self.pop_scope();
                ControlFlow::None
            }
            Statement::WhileLoop { condition, body } => {
                let mut iterations = 0;
                loop {
                    if iterations >= MAX_LOOP_ITERATIONS {
                        self.errors.push("While loop iteration limit exceeded".to_string());
                        break;
                    }
                    iterations += 1;

                    let cond_val = match self.evaluate_expr(condition) {
                        Ok(v) => v,
                        Err(e) => { self.errors.push(e); break; }
                    };

                    if !cond_val.is_truthy() {
                        break;
                    }

                    let cf = self.execute_block_inner(body);
                    match cf {
                        ControlFlow::Break => break,
                        ControlFlow::Continue => continue,
                        ControlFlow::Return(_) => return cf,
                        _ => {}
                    }
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
            Statement::ExprStatement(expr) => {
                match self.evaluate_expr(expr) {
                    Ok(_) => {} // discard result
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::Buy { message } => {
                match self.evaluate_expr(message) {
                    Ok(val) => {
                        let msg = match val {
                            Value::String(s) => s,
                            _ => self.value_to_string(&val),
                        };
                        let price = self.get_last_close_price();
                        self.output_lines.push(format!("[BUY] {}", msg));
                        self.signals.push(Signal {
                            signal_type: "Buy".to_string(),
                            message: msg,
                            timestamp: chrono::Utc::now().to_rfc3339(),
                            price,
                        });
                    }
                    Err(e) => self.errors.push(e),
                }
                ControlFlow::None
            }
            Statement::Sell { message } => {
                match self.evaluate_expr(message) {
                    Ok(val) => {
                        let msg = match val {
                            Value::String(s) => s,
                            _ => self.value_to_string(&val),
                        };
                        let price = self.get_last_close_price();
                        self.output_lines.push(format!("[SELL] {}", msg));
                        self.signals.push(Signal {
                            signal_type: "Sell".to_string(),
                            message: msg,
                            timestamp: chrono::Utc::now().to_rfc3339(),
                            price,
                        });
                    }
                    Err(e) => self.errors.push(e),
                }
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
                // bgcolor is a visual hint - store as a plot annotation
                let _ = (color, condition); // Acknowledge but don't error
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
                if !name.is_empty() {
                    match self.evaluate_expr(default_value) {
                        Ok(val) => { self.set_var(name.clone(), val.clone()); self.inputs.insert(name.clone(), val); }
                        Err(e) => self.errors.push(e),
                    }
                }
                ControlFlow::None
            }
            Statement::StructDef { name, fields } => {
                self.struct_defs.insert(name.clone(), fields.clone());
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
            Statement::ImportStatement { module, alias } => {
                let display_name = alias.as_deref().unwrap_or(module.as_str());
                self.output_lines.push(format!("[IMPORT] {} as '{}'", module, display_name));
                // Import is symbolic - we store the module reference
                self.set_var(display_name.to_string(), Value::String(format!("module:{}", module)));
                ControlFlow::None
            }
            Statement::ExportStatement { name } => {
                if let Some(val) = self.get_var(name).cloned() {
                    self.exports.insert(name.clone(), val);
                }
                ControlFlow::None
            }
        }
    }

    fn execute_block(&mut self, stmts: &[Statement]) -> ControlFlow {
        self.push_scope();
        let cf = self.execute_block_inner(stmts);
        self.pop_scope();
        cf
    }

    fn execute_block_inner(&mut self, stmts: &[Statement]) -> ControlFlow {
        for stmt in stmts {
            let cf = self.execute_statement(stmt);
            match cf {
                ControlFlow::None => {}
                _ => return cf,
            }
        }
        ControlFlow::None
    }

    // ─── Expression Evaluation ──────────────────────────────────────────────

    fn evaluate_expr(&mut self, expr: &Expr) -> Result<Value, String> {
        match expr {
            Expr::Number(n) => Ok(Value::Number(*n)),
            Expr::Bool(b) => Ok(Value::Bool(*b)),
            Expr::Na => Ok(Value::Na),
            Expr::StringLiteral(s) => Ok(Value::String(s.clone())),
            Expr::Symbol(name) => Ok(Value::String(name.clone())),
            Expr::Variable(name) => {
                if let Some(val) = self.get_var(name) {
                    Ok(val.clone())
                } else {
                    Err(format!("Undefined variable '{}'", name))
                }
            }
            Expr::BinaryOp { left, op, right } => {
                let lval = self.evaluate_expr(left)?;
                let rval = self.evaluate_expr(right)?;
                self.eval_binary_op(&lval, op, &rval)
            }
            Expr::UnaryOp { op, operand } => {
                let val = self.evaluate_expr(operand)?;
                self.eval_unary_op(op, &val)
            }
            Expr::FunctionCall { name, args } => self.eval_function_call(name, args),
            Expr::MethodCall { object, method, args } => {
                self.eval_method_call(object, method, args)
            }
            Expr::Ternary { condition, then_expr, else_expr } => {
                let cond = self.evaluate_expr(condition)?;
                if cond.is_truthy() {
                    self.evaluate_expr(then_expr)
                } else {
                    self.evaluate_expr(else_expr)
                }
            }
            Expr::ArrayLiteral(elements) => {
                let mut values = Vec::new();
                for el in elements {
                    values.push(self.evaluate_expr(el)?);
                }
                Ok(Value::Array(values))
            }
            Expr::IndexAccess { object, index } => {
                let obj_val = self.evaluate_expr(object)?;
                let idx_val = self.evaluate_expr(index)?;
                let idx = idx_val.as_number()
                    .ok_or_else(|| "Index must be a number".to_string())? as i64;

                match obj_val {
                    Value::Array(arr) => {
                        let actual_idx = if idx < 0 {
                            (arr.len() as i64 + idx) as usize
                        } else {
                            idx as usize
                        };
                        arr.get(actual_idx)
                            .cloned()
                            .ok_or_else(|| format!("Index {} out of bounds (len {})", idx, arr.len()))
                    }
                    Value::Series(s) => {
                        let actual_idx = if idx < 0 {
                            (s.values.len() as i64 + idx) as usize
                        } else {
                            idx as usize
                        };
                        s.values.get(actual_idx)
                            .map(|v| Value::Number(*v))
                            .ok_or_else(|| format!("Index {} out of bounds (len {})", idx, s.values.len()))
                    }
                    _ => Err(format!("Cannot index into {}", obj_val.type_name())),
                }
            }
            Expr::Range { start, end } => {
                let s = self.evaluate_expr(start)?.as_number()
                    .ok_or_else(|| "Range start must be a number".to_string())? as i64;
                let e = self.evaluate_expr(end)?.as_number()
                    .ok_or_else(|| "Range end must be a number".to_string())? as i64;
                let items: Vec<Value> = (s..e)
                    .take(MAX_LOOP_ITERATIONS)
                    .map(|i| Value::Number(i as f64))
                    .collect();
                Ok(Value::Array(items))
            }
            Expr::MapLiteral(entries) => {
                let mut map = HashMap::new();
                for (key, val_expr) in entries {
                    let val = self.evaluate_expr(val_expr)?;
                    map.insert(key.clone(), val);
                }
                Ok(Value::Map(map))
            }
            Expr::StructLiteral { type_name, fields } => {
                let mut field_map = HashMap::new();
                for (name, val_expr) in fields {
                    let val = self.evaluate_expr(val_expr)?;
                    field_map.insert(name.clone(), val);
                }
                Ok(Value::Struct { type_name: type_name.clone(), fields: field_map })
            }
        }
    }

    fn eval_binary_op(&self, left: &Value, op: &BinOp, right: &Value) -> Result<Value, String> {
        // Series op Series (element-wise)
        if let (Value::Series(ls), Value::Series(rs)) = (left, right) {
            let len = ls.values.len().min(rs.values.len());
            let timestamps = ls.timestamps[..len].to_vec();
            let values: Vec<f64> = (0..len).map(|i| {
                let l = ls.values[i];
                let r = rs.values[i];
                match op {
                    BinOp::Add => l + r,
                    BinOp::Sub => l - r,
                    BinOp::Mul => l * r,
                    BinOp::Div => if r != 0.0 { l / r } else { f64::NAN },
                    BinOp::Mod => if r != 0.0 { l % r } else { f64::NAN },
                    _ => f64::NAN,
                }
            }).collect();
            return Ok(Value::Series(SeriesData { values, timestamps }));
        }

        // Series op Number (broadcast)
        if let (Value::Series(ls), Some(r)) = (left, right.as_number()) {
            let values: Vec<f64> = ls.values.iter().map(|l| {
                match op {
                    BinOp::Add => l + r,
                    BinOp::Sub => l - r,
                    BinOp::Mul => l * r,
                    BinOp::Div => if r != 0.0 { l / r } else { f64::NAN },
                    BinOp::Mod => if r != 0.0 { l % r } else { f64::NAN },
                    _ => f64::NAN,
                }
            }).collect();
            return Ok(Value::Series(SeriesData { values, timestamps: ls.timestamps.clone() }));
        }

        // Number op Series (broadcast)
        if let (Some(l), Value::Series(rs)) = (left.as_number(), right) {
            let values: Vec<f64> = rs.values.iter().map(|r| {
                match op {
                    BinOp::Add => l + r,
                    BinOp::Sub => l - r,
                    BinOp::Mul => l * r,
                    BinOp::Div => if *r != 0.0 { l / r } else { f64::NAN },
                    BinOp::Mod => if *r != 0.0 { l % r } else { f64::NAN },
                    _ => f64::NAN,
                }
            }).collect();
            return Ok(Value::Series(SeriesData { values, timestamps: rs.timestamps.clone() }));
        }

        // Number op Number
        if let (Some(l), Some(r)) = (left.as_number(), right.as_number()) {
            return Ok(match op {
                BinOp::Add => Value::Number(l + r),
                BinOp::Sub => Value::Number(l - r),
                BinOp::Mul => Value::Number(l * r),
                BinOp::Div => {
                    if r == 0.0 { return Err("Division by zero".to_string()); }
                    Value::Number(l / r)
                }
                BinOp::Mod => {
                    if r == 0.0 { return Err("Modulo by zero".to_string()); }
                    Value::Number(l % r)
                }
                BinOp::Gt => Value::Bool(l > r),
                BinOp::Lt => Value::Bool(l < r),
                BinOp::Gte => Value::Bool(l >= r),
                BinOp::Lte => Value::Bool(l <= r),
                BinOp::Eq => Value::Bool((l - r).abs() < f64::EPSILON),
                BinOp::Neq => Value::Bool((l - r).abs() >= f64::EPSILON),
                BinOp::And => Value::Bool(l != 0.0 && r != 0.0),
                BinOp::Or => Value::Bool(l != 0.0 || r != 0.0),
            });
        }

        // String concatenation
        if let (Value::String(ls), Value::String(rs)) = (left, right) {
            if matches!(op, BinOp::Add) {
                return Ok(Value::String(format!("{}{}", ls, rs)));
            }
        }

        // Bool op Bool
        if let (Value::Bool(l), Value::Bool(r)) = (left, right) {
            return Ok(match op {
                BinOp::And => Value::Bool(*l && *r),
                BinOp::Or => Value::Bool(*l || *r),
                BinOp::Eq => Value::Bool(l == r),
                BinOp::Neq => Value::Bool(l != r),
                _ => return Err(format!("Cannot apply {:?} to Bool values", op)),
            });
        }

        Err(format!(
            "Cannot apply {:?} between {} and {}",
            op, left.type_name(), right.type_name()
        ))
    }

    fn eval_unary_op(&self, op: &UnaryOp, val: &Value) -> Result<Value, String> {
        match op {
            UnaryOp::Neg => match val {
                Value::Number(n) => Ok(Value::Number(-n)),
                Value::Series(s) => {
                    let values: Vec<f64> = s.values.iter().map(|v| -v).collect();
                    Ok(Value::Series(SeriesData { values, timestamps: s.timestamps.clone() }))
                }
                _ => Err(format!("Cannot negate {}", val.type_name())),
            },
            UnaryOp::Not => Ok(Value::Bool(!val.is_truthy())),
        }
    }

    // ─── Function Calls ─────────────────────────────────────────────────────

    fn eval_function_call(&mut self, name: &str, args: &[Expr]) -> Result<Value, String> {
        // Check user-defined functions first
        if let Some(func) = self.user_functions.get(name).cloned() {
            return self.call_user_function(&func, args);
        }

        match name {
            // Technical indicators
            "ema" => self.eval_indicator_fn(name, args, |data, period| indicators::ema(data, period)),
            "sma" => self.eval_indicator_fn(name, args, |data, period| indicators::sma(data, period)),
            "wma" => self.eval_indicator_fn(name, args, |data, period| indicators::wma(data, period)),
            "rma" => self.eval_indicator_fn(name, args, |data, period| indicators::rma(data, period)),
            "hma" => self.eval_indicator_fn(name, args, |data, period| indicators::hma(data, period)),
            "rsi" => self.eval_indicator_fn(name, args, |data, period| indicators::rsi(data, period)),
            "cci" => self.eval_hlc_indicator(args, |h, l, c, p| indicators::cci(h, l, c, p)),
            "mfi" => self.eval_mfi(args),
            "adx" => self.eval_hlc_indicator(args, |h, l, c, p| indicators::adx(h, l, c, p)),
            "atr" => self.eval_atr(args),
            "vwap" => self.eval_vwap(args),
            "sar" | "parabolic_sar" => self.eval_sar(args),
            "supertrend" => self.eval_supertrend(args),
            "supertrend_dir" => self.eval_supertrend_dir(args),
            "macd" => self.eval_macd(args),
            "macd_signal" => self.eval_macd_signal(args),
            "macd_hist" => self.eval_macd_hist(args),
            "stochastic" | "stochastic_k" => self.eval_stochastic(args),
            "stochastic_d" => self.eval_stochastic_d(args),
            "bollinger" | "bollinger_upper" => self.eval_bollinger(args),
            "bollinger_middle" => self.eval_bollinger_middle(args),
            "bollinger_lower" => self.eval_bollinger_lower(args),
            "obv" => self.eval_obv(args),
            "williams_r" | "wpr" => self.eval_williams_r(args),
            "roc" => self.eval_indicator_fn(name, args, |data, period| indicators::roc(data, period)),
            "momentum" | "mom" => self.eval_indicator_fn(name, args, |data, period| indicators::momentum(data, period)),
            "stdev" => self.eval_indicator_fn(name, args, |data, period| indicators::stdev(data, period)),
            "linreg" => self.eval_indicator_fn(name, args, |data, period| indicators::linreg(data, period)),
            "percentrank" => self.eval_indicator_fn(name, args, |data, period| indicators::percentrank(data, period)),
            "highest" => self.eval_indicator_fn(name, args, |data, period| indicators::highest(data, period)),
            "lowest" => self.eval_indicator_fn(name, args, |data, period| indicators::lowest(data, period)),
            "change" => self.eval_change(args),
            "cum" => self.eval_cum(args),
            "tr" | "true_range" => self.eval_true_range(args),
            "pivothigh" => self.eval_pivot(args, true),
            "pivotlow" => self.eval_pivot(args, false),
            // Price functions
            "volume" => self.eval_price_fn(args, "volume"),
            "close" => self.eval_price_fn(args, "close"),
            "open" => self.eval_price_fn(args, "open"),
            "high" => self.eval_price_fn(args, "high"),
            "low" => self.eval_price_fn(args, "low"),
            // Built-in bar variables
            "bar_index" => self.eval_bar_index(args),
            "hl2" => self.eval_hl2(args),
            "hlc3" => self.eval_hlc3(args),
            "ohlc4" => self.eval_ohlc4(args),
            "timestamps" => self.eval_timestamps(args),
            // NA functions
            "na" => self.eval_na_fn(args),
            "nz" => self.eval_nz(args),
            // Utility functions
            "last" => self.eval_last(args),
            "first" => self.eval_first(args),
            "len" => self.eval_len(args),
            "abs" => self.eval_abs(args),
            "max" => self.eval_max(args),
            "min" => self.eval_min(args),
            "sum" => self.eval_sum(args),
            "avg" | "mean" => self.eval_avg(args),
            "sqrt" => self.eval_sqrt(args),
            "pow" => self.eval_pow(args),
            "log" => self.eval_log(args),
            "log10" => self.eval_log10(args),
            "exp" => self.eval_exp(args),
            "sin" => self.eval_trig(args, f64::sin),
            "cos" => self.eval_trig(args, f64::cos),
            "tan" => self.eval_trig(args, f64::tan),
            "round" => self.eval_round(args),
            "floor" => self.eval_floor(args),
            "ceil" => self.eval_ceil(args),
            "sign" => self.eval_sign(args),
            "push" => self.eval_push(args),
            "slice" => self.eval_slice(args),
            "print" => self.eval_print(args),
            "str" => self.eval_str(args),
            "num" => self.eval_num(args),
            "type" => self.eval_type(args),
            "range" => self.eval_range_fn(args),
            "crossover" => self.eval_crossover(args),
            "crossunder" => self.eval_crossunder(args),
            // Strategy info functions
            "strategy_position_size" => Ok(Value::Number(self.strategy_position as f64)),
            "strategy_equity" => Ok(Value::Number(self.strategy_equity)),
            // Color functions
            "color_rgb" | "color_new" => self.eval_color_rgb(args),
            "color_red" => Ok(Value::Color(ColorValue::new(255, 0, 0, 255))),
            "color_green" => Ok(Value::Color(ColorValue::new(0, 128, 0, 255))),
            "color_blue" => Ok(Value::Color(ColorValue::new(0, 0, 255, 255))),
            "color_white" => Ok(Value::Color(ColorValue::new(255, 255, 255, 255))),
            "color_black" => Ok(Value::Color(ColorValue::new(0, 0, 0, 255))),
            "color_yellow" => Ok(Value::Color(ColorValue::new(255, 255, 0, 255))),
            "color_orange" => Ok(Value::Color(ColorValue::new(255, 165, 0, 255))),
            "color_purple" => Ok(Value::Color(ColorValue::new(128, 0, 128, 255))),
            "color_aqua" => Ok(Value::Color(ColorValue::new(0, 255, 255, 255))),
            "color_lime" => Ok(Value::Color(ColorValue::new(0, 255, 0, 255))),
            "color_fuchsia" => Ok(Value::Color(ColorValue::new(255, 0, 255, 255))),
            "color_silver" => Ok(Value::Color(ColorValue::new(192, 192, 192, 255))),
            "color_gray" => Ok(Value::Color(ColorValue::new(128, 128, 128, 255))),
            "color_maroon" => Ok(Value::Color(ColorValue::new(128, 0, 0, 255))),
            "color_olive" => Ok(Value::Color(ColorValue::new(128, 128, 0, 255))),
            "color_teal" => Ok(Value::Color(ColorValue::new(0, 128, 128, 255))),
            "color_navy" => Ok(Value::Color(ColorValue::new(0, 0, 128, 255))),
            // Drawing functions
            "line_new" => self.eval_line_new(args),
            "label_new" => self.eval_label_new(args),
            "box_new" => self.eval_box_new(args),
            // Table functions
            "table_new" => self.eval_table_new(args),
            "table_cell" => self.eval_table_cell(args),
            // Map functions
            "map_new" => Ok(Value::Map(HashMap::new())),
            "map_put" => self.eval_map_put(args),
            "map_get" => self.eval_map_get(args),
            "map_keys" => self.eval_map_keys(args),
            "map_size" => self.eval_map_size(args),
            "map_contains" => self.eval_map_contains(args),
            // Matrix functions
            "matrix_new" => self.eval_matrix_new(args),
            "matrix_get" => self.eval_matrix_get(args),
            "matrix_set" => self.eval_matrix_set(args),
            "matrix_rows" => self.eval_matrix_rows(args),
            "matrix_cols" => self.eval_matrix_cols(args),
            // Request functions
            "request_security" => self.eval_request_security(args),
            // Alert function (also handled as statement, but can be called as function)
            "alert" => self.eval_alert_fn(args),
            "alertcondition" => self.eval_alertcondition(args),
            _ => Err(format!("Unknown function '{}'", name)),
        }
    }

    fn call_user_function(&mut self, func: &UserFunction, args: &[Expr]) -> Result<Value, String> {
        // Evaluate arguments in the current scope
        let mut arg_values = Vec::new();
        for arg in args {
            arg_values.push(self.evaluate_expr(arg)?);
        }

        // Create new scope for function
        self.push_scope();

        // Bind parameters
        for (i, param) in func.params.iter().enumerate() {
            let val = arg_values.get(i).cloned().unwrap_or(Value::Void);
            self.set_var(param.clone(), val);
        }

        // Execute body
        let mut result = Value::Void;
        for stmt in &func.body {
            let cf = self.execute_statement(stmt);
            if let ControlFlow::Return(val) = cf {
                result = val;
                break;
            }
        }

        self.pop_scope();
        Ok(result)
    }

    // ─── Indicator Functions ────────────────────────────────────────────────

    fn eval_indicator_fn(
        &mut self,
        name: &str,
        args: &[Expr],
        compute: impl Fn(&[f64], usize) -> Vec<f64>,
    ) -> Result<Value, String> {
        if args.len() < 2 {
            return Err(format!("{}() requires at least 2 arguments: symbol/series and period", name));
        }

        let period = self.resolve_number(&args[1])? as usize;

        // Check if first arg is a series variable or a symbol
        let first = self.evaluate_expr(&args[0])?;
        match first {
            Value::Series(s) => {
                let values = compute(&s.values, period);
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            Value::String(symbol) => {
                let ohlcv = self.symbol_data.get(&symbol)
                    .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
                let values = compute(&ohlcv.close, period);
                Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
            }
            _ => Err(format!("{}() first argument must be a symbol or series", name)),
        }
    }

    fn eval_atr(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("atr() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::atr(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_macd(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (macd_line, _, _) = self.compute_macd(args)?;
        Ok(macd_line)
    }

    fn eval_macd_signal(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, signal_line, _) = self.compute_macd(args)?;
        Ok(signal_line)
    }

    fn eval_macd_hist(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, _, histogram) = self.compute_macd(args)?;
        Ok(histogram)
    }

    fn compute_macd(&mut self, args: &[Expr]) -> Result<(Value, Value, Value), String> {
        if args.is_empty() {
            return Err("macd() requires at least 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let fast = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 12 };
        let slow = if args.len() > 2 { self.resolve_number(&args[2])? as usize } else { 26 };
        let signal = if args.len() > 3 { self.resolve_number(&args[3])? as usize } else { 9 };

        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (macd_vals, signal_vals, hist_vals) = indicators::macd(&ohlcv.close, fast, slow, signal);
        let ts = ohlcv.timestamps.clone();

        Ok((
            Value::Series(SeriesData { values: macd_vals, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: signal_vals, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: hist_vals, timestamps: ts }),
        ))
    }

    fn eval_stochastic(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("stochastic() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (k_values, _) = indicators::stochastic(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values: k_values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_stochastic_d(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("stochastic_d() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (_, d_values) = indicators::stochastic(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values: d_values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_bollinger(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (upper, _, _) = self.compute_bollinger(args)?;
        Ok(upper)
    }

    fn eval_bollinger_middle(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, middle, _) = self.compute_bollinger(args)?;
        Ok(middle)
    }

    fn eval_bollinger_lower(&mut self, args: &[Expr]) -> Result<Value, String> {
        let (_, _, lower) = self.compute_bollinger(args)?;
        Ok(lower)
    }

    fn compute_bollinger(&mut self, args: &[Expr]) -> Result<(Value, Value, Value), String> {
        if args.len() < 2 {
            return Err("bollinger() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let std_dev = if args.len() > 2 { self.resolve_number(&args[2])? } else { 2.0 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (upper, middle, lower) = indicators::bollinger(&ohlcv.close, period, std_dev);
        let ts = ohlcv.timestamps.clone();
        Ok((
            Value::Series(SeriesData { values: upper, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: middle, timestamps: ts.clone() }),
            Value::Series(SeriesData { values: lower, timestamps: ts }),
        ))
    }

    fn eval_obv(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("obv() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::obv(&ohlcv.close, &ohlcv.volume);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_price_fn(&mut self, args: &[Expr], field: &str) -> Result<Value, String> {
        if args.is_empty() {
            return Err(format!("{}() requires 1 argument: symbol", field));
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = match field {
            "close" => ohlcv.close.clone(),
            "open" => ohlcv.open.clone(),
            "high" => ohlcv.high.clone(),
            "low" => ohlcv.low.clone(),
            "volume" => ohlcv.volume.clone(),
            _ => return Err(format!("Unknown price field '{}'", field)),
        };
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    // ─── Utility Functions ──────────────────────────────────────────────────

    fn eval_last(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("last() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let last_val = s.values.iter().rev().find(|v| !v.is_nan()).copied().unwrap_or(f64::NAN);
                Ok(Value::Number(last_val))
            }
            Value::Array(a) => Ok(a.last().cloned().unwrap_or(Value::Void)),
            Value::Number(n) => Ok(Value::Number(n)),
            _ => Err(format!("last() expects Series, Array, or Number, got {}", val.type_name())),
        }
    }

    fn eval_first(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("first() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let first_val = s.values.iter().find(|v| !v.is_nan()).copied().unwrap_or(f64::NAN);
                Ok(Value::Number(first_val))
            }
            Value::Array(a) => Ok(a.first().cloned().unwrap_or(Value::Void)),
            _ => Err(format!("first() expects Series or Array, got {}", val.type_name())),
        }
    }

    fn eval_len(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("len() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => Ok(Value::Number(s.values.len() as f64)),
            Value::Array(a) => Ok(Value::Number(a.len() as f64)),
            Value::String(s) => Ok(Value::Number(s.len() as f64)),
            _ => Err(format!("len() expects Series, Array, or String, got {}", val.type_name())),
        }
    }

    fn eval_abs(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("abs() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Number(n) => Ok(Value::Number(n.abs())),
            Value::Series(s) => {
                let values: Vec<f64> = s.values.iter().map(|v| v.abs()).collect();
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err(format!("abs() expects Number or Series, got {}", val.type_name())),
        }
    }

    fn eval_max(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("max() requires at least 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let max_val = s.values.iter().filter(|v| !v.is_nan()).copied()
                    .fold(f64::NEG_INFINITY, f64::max);
                Ok(Value::Number(max_val))
            }
            Value::Array(a) => {
                let max_val = a.iter().filter_map(|v| v.as_number())
                    .fold(f64::NEG_INFINITY, f64::max);
                Ok(Value::Number(max_val))
            }
            Value::Number(n) => {
                if args.len() > 1 {
                    let b = self.evaluate_expr(&args[1])?.as_number()
                        .ok_or("max() second arg must be number")?;
                    Ok(Value::Number(n.max(b)))
                } else {
                    Ok(Value::Number(n))
                }
            }
            _ => Err(format!("max() expects Number, Series, or Array")),
        }
    }

    fn eval_min(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("min() requires at least 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let min_val = s.values.iter().filter(|v| !v.is_nan()).copied()
                    .fold(f64::INFINITY, f64::min);
                Ok(Value::Number(min_val))
            }
            Value::Array(a) => {
                let min_val = a.iter().filter_map(|v| v.as_number())
                    .fold(f64::INFINITY, f64::min);
                Ok(Value::Number(min_val))
            }
            Value::Number(n) => {
                if args.len() > 1 {
                    let b = self.evaluate_expr(&args[1])?.as_number()
                        .ok_or("min() second arg must be number")?;
                    Ok(Value::Number(n.min(b)))
                } else {
                    Ok(Value::Number(n))
                }
            }
            _ => Err(format!("min() expects Number, Series, or Array")),
        }
    }

    fn eval_sum(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("sum() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let total: f64 = s.values.iter().filter(|v| !v.is_nan()).sum();
                Ok(Value::Number(total))
            }
            Value::Array(a) => {
                let total: f64 = a.iter().filter_map(|v| v.as_number()).sum();
                Ok(Value::Number(total))
            }
            _ => Err(format!("sum() expects Series or Array")),
        }
    }

    fn eval_avg(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("avg() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let non_nan: Vec<f64> = s.values.iter().filter(|v| !v.is_nan()).copied().collect();
                if non_nan.is_empty() { return Ok(Value::Number(f64::NAN)); }
                Ok(Value::Number(non_nan.iter().sum::<f64>() / non_nan.len() as f64))
            }
            Value::Array(a) => {
                let nums: Vec<f64> = a.iter().filter_map(|v| v.as_number()).collect();
                if nums.is_empty() { return Ok(Value::Number(f64::NAN)); }
                Ok(Value::Number(nums.iter().sum::<f64>() / nums.len() as f64))
            }
            _ => Err(format!("avg() expects Series or Array")),
        }
    }

    fn eval_sqrt(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("sqrt() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Number(n) => Ok(Value::Number(n.sqrt())),
            _ => Err(format!("sqrt() expects Number")),
        }
    }

    fn eval_pow(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("pow() requires 2 arguments: base, exponent".to_string()); }
        let base = self.evaluate_expr(&args[0])?.as_number().ok_or("pow() base must be number")?;
        let exp = self.evaluate_expr(&args[1])?.as_number().ok_or("pow() exponent must be number")?;
        Ok(Value::Number(base.powf(exp)))
    }

    fn eval_log(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("log() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("log() expects Number")?;
        Ok(Value::Number(val.ln()))
    }

    fn eval_round(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("round() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("round() expects Number")?;
        let decimals = if args.len() > 1 {
            self.evaluate_expr(&args[1])?.as_number().unwrap_or(0.0) as i32
        } else { 0 };
        let factor = 10_f64.powi(decimals);
        Ok(Value::Number((val * factor).round() / factor))
    }

    fn eval_floor(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("floor() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("floor() expects Number")?;
        Ok(Value::Number(val.floor()))
    }

    fn eval_ceil(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("ceil() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("ceil() expects Number")?;
        Ok(Value::Number(val.ceil()))
    }

    fn eval_push(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("push() requires 2 arguments: array, value".to_string()); }
        // Get array variable name
        let arr_name = match &args[0] {
            Expr::Variable(name) => name.clone(),
            _ => return Err("push() first arg must be a variable".to_string()),
        };
        let new_val = self.evaluate_expr(&args[1])?;
        let current = self.get_var(&arr_name).cloned();
        if let Some(Value::Array(mut arr)) = current {
            arr.push(new_val);
            let len = arr.len() as f64;
            self.set_var_in_any_scope(&arr_name, Value::Array(arr));
            Ok(Value::Number(len))
        } else {
            Err(format!("'{}' is not an array", arr_name))
        }
    }

    fn eval_slice(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 3 { return Err("slice() requires 3 arguments: array, start, end".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        let start = self.evaluate_expr(&args[1])?.as_number().ok_or("slice start must be number")? as usize;
        let end = self.evaluate_expr(&args[2])?.as_number().ok_or("slice end must be number")? as usize;
        match val {
            Value::Array(a) => {
                let s = start.min(a.len());
                let e = end.min(a.len());
                Ok(Value::Array(a[s..e].to_vec()))
            }
            Value::Series(s) => {
                let sv = start.min(s.values.len());
                let ev = end.min(s.values.len());
                Ok(Value::Series(SeriesData {
                    values: s.values[sv..ev].to_vec(),
                    timestamps: s.timestamps[sv..ev].to_vec(),
                }))
            }
            _ => Err("slice() expects Array or Series".to_string()),
        }
    }

    fn eval_print(&mut self, args: &[Expr]) -> Result<Value, String> {
        let mut parts = Vec::new();
        for arg in args {
            let val = self.evaluate_expr(arg)?;
            parts.push(self.value_to_string(&val));
        }
        self.output_lines.push(parts.join(" "));
        Ok(Value::Void)
    }

    fn eval_str(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("str() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        Ok(Value::String(self.value_to_string(&val)))
    }

    fn eval_num(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("num() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Number(n) => Ok(Value::Number(n)),
            Value::String(s) => {
                let n = s.parse::<f64>().map_err(|_| format!("Cannot convert '{}' to number", s))?;
                Ok(Value::Number(n))
            }
            Value::Bool(b) => Ok(Value::Number(if b { 1.0 } else { 0.0 })),
            _ => Err(format!("Cannot convert {} to number", val.type_name())),
        }
    }

    fn eval_type(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("type() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        Ok(Value::String(val.type_name().to_string()))
    }

    fn eval_range_fn(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("range() requires 1-2 arguments".to_string()); }
        let start = if args.len() > 1 {
            self.evaluate_expr(&args[0])?.as_number().ok_or("range start must be number")? as i64
        } else { 0 };
        let end_idx = if args.len() > 1 { 1 } else { 0 };
        let end = self.evaluate_expr(&args[end_idx])?.as_number().ok_or("range end must be number")? as i64;
        let items: Vec<Value> = (start..end).take(MAX_LOOP_ITERATIONS).map(|i| Value::Number(i as f64)).collect();
        Ok(Value::Array(items))
    }

    fn eval_crossover(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("crossover() requires 2 arguments".to_string()); }
        let a = self.evaluate_expr(&args[0])?;
        let b = self.evaluate_expr(&args[1])?;
        match (a, b) {
            (Value::Series(sa), Value::Series(sb)) => {
                let len = sa.values.len().min(sb.values.len());
                if len < 2 { return Ok(Value::Bool(false)); }
                let prev_a = sa.values[len - 2];
                let curr_a = sa.values[len - 1];
                let prev_b = sb.values[len - 2];
                let curr_b = sb.values[len - 1];
                Ok(Value::Bool(prev_a <= prev_b && curr_a > curr_b))
            }
            _ => Err("crossover() requires two Series arguments".to_string()),
        }
    }

    fn eval_crossunder(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("crossunder() requires 2 arguments".to_string()); }
        let a = self.evaluate_expr(&args[0])?;
        let b = self.evaluate_expr(&args[1])?;
        match (a, b) {
            (Value::Series(sa), Value::Series(sb)) => {
                let len = sa.values.len().min(sb.values.len());
                if len < 2 { return Ok(Value::Bool(false)); }
                let prev_a = sa.values[len - 2];
                let curr_a = sa.values[len - 1];
                let prev_b = sb.values[len - 2];
                let curr_b = sb.values[len - 1];
                Ok(Value::Bool(prev_a >= prev_b && curr_a < curr_b))
            }
            _ => Err("crossunder() requires two Series arguments".to_string()),
        }
    }

    // ─── New Indicator Functions ────────────────────────────────────────────

    fn eval_hlc_indicator(&mut self, args: &[Expr], compute: impl Fn(&[f64], &[f64], &[f64], usize) -> Vec<f64>) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("Indicator requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = compute(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_mfi(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("mfi() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::mfi(&ohlcv.high, &ohlcv.low, &ohlcv.close, &ohlcv.volume, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_vwap(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("vwap() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::vwap(&ohlcv.high, &ohlcv.low, &ohlcv.close, &ohlcv.volume);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_sar(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("sar() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let af_start = if args.len() > 1 { self.resolve_number(&args[1])? } else { 0.02 };
        let af_step = if args.len() > 2 { self.resolve_number(&args[2])? } else { 0.02 };
        let af_max = if args.len() > 3 { self.resolve_number(&args[3])? } else { 0.2 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::sar(&ohlcv.high, &ohlcv.low, af_start, af_step, af_max);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_supertrend(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("supertrend() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 10 };
        let multiplier = if args.len() > 2 { self.resolve_number(&args[2])? } else { 3.0 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (st, _) = indicators::supertrend(&ohlcv.high, &ohlcv.low, &ohlcv.close, period, multiplier);
        Ok(Value::Series(SeriesData { values: st, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_supertrend_dir(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() {
            return Err("supertrend_dir() requires 1 argument: symbol".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 10 };
        let multiplier = if args.len() > 2 { self.resolve_number(&args[2])? } else { 3.0 };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let (_, dir) = indicators::supertrend(&ohlcv.high, &ohlcv.low, &ohlcv.close, period, multiplier);
        Ok(Value::Series(SeriesData { values: dir, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_williams_r(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 {
            return Err("williams_r() requires 2 arguments: symbol and period".to_string());
        }
        let symbol = self.resolve_symbol(&args[0])?;
        let period = self.resolve_number(&args[1])? as usize;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::williams_r(&ohlcv.high, &ohlcv.low, &ohlcv.close, period);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_change(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("change() requires 1-2 arguments".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        let period = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 1 };
        match val {
            Value::Series(s) => {
                let values = indicators::change(&s.values, period);
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err("change() expects a Series".to_string()),
        }
    }

    fn eval_cum(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("cum() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => {
                let values = indicators::cum(&s.values);
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err("cum() expects a Series".to_string()),
        }
    }

    fn eval_true_range(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("tr() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values = indicators::true_range(&ohlcv.high, &ohlcv.low, &ohlcv.close);
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_pivot(&mut self, args: &[Expr], is_high: bool) -> Result<Value, String> {
        if args.is_empty() { return Err("pivot() requires at least 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        let left = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 5 };
        let right = if args.len() > 2 { self.resolve_number(&args[2])? as usize } else { 5 };
        match val {
            Value::Series(s) => {
                let values = if is_high {
                    indicators::pivothigh(&s.values, left, right)
                } else {
                    indicators::pivotlow(&s.values, left, right)
                };
                Ok(Value::Series(SeriesData { values, timestamps: s.timestamps }))
            }
            _ => Err("pivot functions expect a Series".to_string()),
        }
    }

    // ─── Built-in Bar Variables ──────────────────────────────────────────────

    fn eval_bar_index(&mut self, args: &[Expr]) -> Result<Value, String> {
        let symbol = if !args.is_empty() { self.resolve_symbol(&args[0])? } else { self.first_symbol() };
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values: Vec<f64> = (0..ohlcv.close.len()).map(|i| i as f64).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_hl2(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("hl2() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values: Vec<f64> = ohlcv.high.iter().zip(ohlcv.low.iter())
            .map(|(h, l)| (h + l) / 2.0).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_hlc3(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("hlc3() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let len = ohlcv.close.len();
        let values: Vec<f64> = (0..len)
            .map(|i| (ohlcv.high[i] + ohlcv.low[i] + ohlcv.close[i]) / 3.0).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_ohlc4(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("ohlc4() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let len = ohlcv.close.len();
        let values: Vec<f64> = (0..len)
            .map(|i| (ohlcv.open[i] + ohlcv.high[i] + ohlcv.low[i] + ohlcv.close[i]) / 4.0).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_timestamps(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("timestamps() requires 1 argument: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        let ohlcv = self.symbol_data.get(&symbol)
            .ok_or_else(|| format!("No data for symbol '{}'", symbol))?;
        let values: Vec<f64> = ohlcv.timestamps.iter().map(|t| *t as f64).collect();
        Ok(Value::Series(SeriesData { values, timestamps: ohlcv.timestamps.clone() }))
    }

    fn eval_na_fn(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Ok(Value::Na); }
        let val = self.evaluate_expr(&args[0])?;
        Ok(Value::Bool(val.is_na()))
    }

    fn eval_nz(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("nz() requires 1-2 arguments".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        if val.is_na() {
            if args.len() > 1 {
                self.evaluate_expr(&args[1])
            } else {
                Ok(Value::Number(0.0))
            }
        } else {
            Ok(val)
        }
    }

    fn eval_log10(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("log10() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("log10() expects Number")?;
        Ok(Value::Number(val.log10()))
    }

    fn eval_exp(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("exp() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("exp() expects Number")?;
        Ok(Value::Number(val.exp()))
    }

    fn eval_trig(&mut self, args: &[Expr], f: fn(f64) -> f64) -> Result<Value, String> {
        if args.is_empty() { return Err("trig function requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("expects Number")?;
        Ok(Value::Number(f(val)))
    }

    fn eval_sign(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("sign() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("sign() expects Number")?;
        Ok(Value::Number(if val > 0.0 { 1.0 } else if val < 0.0 { -1.0 } else { 0.0 }))
    }

    // ─── Method Call Dispatch ────────────────────────────────────────────────

    fn eval_method_call(&mut self, object: &Expr, method: &str, args: &[Expr]) -> Result<Value, String> {
        let obj_val = self.evaluate_expr(object)?;
        match (&obj_val, method) {
            (Value::Series(s), "last") => {
                let n = if !args.is_empty() { self.resolve_number(&args[0])? as usize } else { 1 };
                let valid: Vec<f64> = s.values.iter().filter(|v| !v.is_nan()).copied().collect();
                if n <= valid.len() {
                    Ok(Value::Number(valid[valid.len() - n]))
                } else {
                    Ok(Value::Na)
                }
            }
            (Value::Series(_), "len") | (Value::Array(_), "len") => {
                Ok(Value::Number(match &obj_val {
                    Value::Series(s) => s.values.len() as f64,
                    Value::Array(a) => a.len() as f64,
                    _ => 0.0,
                }))
            }
            (Value::Array(a), "push") => {
                if args.is_empty() { return Err(".push() requires 1 argument".to_string()); }
                let new_val = self.evaluate_expr(&args[0])?;
                let mut arr = a.clone();
                arr.push(new_val);
                Ok(Value::Array(arr))
            }
            (Value::Array(a), "pop") => {
                let mut arr = a.clone();
                let val = arr.pop().unwrap_or(Value::Void);
                Ok(val)
            }
            (Value::String(s), "len") => Ok(Value::Number(s.len() as f64)),
            (Value::String(s), "upper") => Ok(Value::String(s.to_uppercase())),
            (Value::String(s), "lower") => Ok(Value::String(s.to_lowercase())),
            (Value::String(s), "contains") => {
                if args.is_empty() { return Err(".contains() requires 1 argument".to_string()); }
                let search = self.evaluate_expr(&args[0])?;
                if let Value::String(sub) = search {
                    Ok(Value::Bool(s.contains(&*sub)))
                } else {
                    Err(".contains() argument must be a string".to_string())
                }
            }
            // Map methods
            (Value::Map(m), "get") => {
                if args.is_empty() { return Err(".get() requires a key argument".to_string()); }
                let key = match self.evaluate_expr(&args[0])? {
                    Value::String(s) => s,
                    v => self.value_to_string(&v),
                };
                Ok(m.get(&key).cloned().unwrap_or(Value::Na))
            }
            (Value::Map(m), "put") | (Value::Map(m), "set") => {
                if args.len() < 2 { return Err(".put() requires key and value arguments".to_string()); }
                let key = match self.evaluate_expr(&args[0])? {
                    Value::String(s) => s,
                    v => self.value_to_string(&v),
                };
                let val = self.evaluate_expr(&args[1])?;
                let mut new_map = m.clone();
                new_map.insert(key, val);
                Ok(Value::Map(new_map))
            }
            (Value::Map(m), "keys") => {
                let keys: Vec<Value> = m.keys().map(|k| Value::String(k.clone())).collect();
                Ok(Value::Array(keys))
            }
            (Value::Map(m), "size") | (Value::Map(m), "len") => {
                Ok(Value::Number(m.len() as f64))
            }
            (Value::Map(m), "contains") | (Value::Map(m), "has") => {
                if args.is_empty() { return Err(".contains() requires a key argument".to_string()); }
                let key = match self.evaluate_expr(&args[0])? {
                    Value::String(s) => s,
                    v => self.value_to_string(&v),
                };
                Ok(Value::Bool(m.contains_key(&key)))
            }
            // Struct field access
            (Value::Struct { fields, .. }, field_name) => {
                if let Some(val) = fields.get(field_name) {
                    Ok(val.clone())
                } else {
                    Err(format!("Struct has no field '{}'", field_name))
                }
            }
            // Color methods
            (Value::Color(c), "r") => Ok(Value::Number(c.r as f64)),
            (Value::Color(c), "g") => Ok(Value::Number(c.g as f64)),
            (Value::Color(c), "b") => Ok(Value::Number(c.b as f64)),
            (Value::Color(c), "a") => Ok(Value::Number(c.a as f64)),
            (Value::Color(c), "hex") | (Value::Color(c), "to_string") => Ok(Value::String(c.to_hex())),
            // Drawing methods
            (Value::Drawing(d), "delete") => {
                let _ = d; // acknowledge - drawings are immutable in our model
                Ok(Value::Void)
            }
            _ => Err(format!("No method '{}' on type {}", method, obj_val.type_name())),
        }
    }

    // ─── Strategy Handlers ──────────────────────────────────────────────────

    fn handle_strategy_entry(&mut self, id: &Expr, direction: &Expr, qty: &Option<Expr>, _stop: &Option<Expr>, _limit: &Option<Expr>) {
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

    fn handle_strategy_exit(&mut self, id: &Expr, _qty: &Option<Expr>, _stop: &Option<Expr>, _limit: &Option<Expr>) {
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

    fn handle_strategy_close(&mut self, id: &Expr) {
        self.handle_strategy_exit(id, &None, &None, &None);
    }

    // ─── Switch Handler ─────────────────────────────────────────────────────

    fn handle_switch(&mut self, expr: &Option<Expr>, cases: &[(Expr, Vec<Statement>)], default: &Option<Vec<Statement>>) -> ControlFlow {
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

    // ─── Plot Handlers (New) ────────────────────────────────────────────────

    fn handle_plot_histogram(&mut self, value: &Expr, label: &Expr, color_up: &Option<Expr>, _color_down: &Option<Expr>) {
        let label_str = match self.evaluate_expr(label) {
            Ok(Value::String(s)) => s,
            Ok(v) => self.value_to_string(&v),
            Err(e) => { self.errors.push(e); return; }
        };
        let color = if let Some(c) = color_up {
            match self.evaluate_expr(c) { Ok(Value::String(s)) => Some(s), _ => None }
        } else { None };
        match self.evaluate_expr(value) {
            Ok(Value::Series(series)) => {
                let data: Vec<PlotPoint> = series.timestamps.iter().zip(series.values.iter())
                    .filter(|(_, v)| !v.is_nan())
                    .map(|(ts, val)| PlotPoint { timestamp: *ts, value: Some(*val), open: None, high: None, low: None, close: None, volume: None })
                    .collect();
                self.plots.push(PlotData { plot_type: "histogram".to_string(), label: label_str, data, color });
            }
            Ok(_) => self.errors.push("plot_histogram value must be a Series".to_string()),
            Err(e) => self.errors.push(e),
        }
    }

    fn handle_plot_shape(&mut self, condition: &Expr, shape: &Expr, _location: &Expr, color: &Option<Expr>, text: &Option<Expr>) {
        let cond_val = match self.evaluate_expr(condition) {
            Ok(v) => v,
            Err(e) => { self.errors.push(e); return; }
        };
        if !cond_val.is_truthy() { return; }
        let shape_str = match self.evaluate_expr(shape) {
            Ok(Value::String(s)) => s,
            _ => "circle".to_string(),
        };
        let color_str = if let Some(c) = color {
            match self.evaluate_expr(c) { Ok(Value::String(s)) => Some(s), _ => None }
        } else { None };
        let text_str = if let Some(t) = text {
            match self.evaluate_expr(t) { Ok(Value::String(s)) => Some(s), _ => None }
        } else { None };
        let price = self.get_last_close_price();
        self.signals.push(Signal {
            signal_type: format!("Shape:{}", shape_str),
            message: text_str.unwrap_or_else(|| shape_str.clone()),
            timestamp: chrono::Utc::now().to_rfc3339(),
            price,
        });
        let _ = color_str; // stored for future rendering
    }

    fn handle_hline(&mut self, value: &Expr, label: &Option<Expr>, color: &Option<Expr>) {
        let val = match self.evaluate_expr(value) {
            Ok(v) => v.as_number().unwrap_or(0.0),
            Err(e) => { self.errors.push(e); return; }
        };
        let label_str = if let Some(l) = label {
            match self.evaluate_expr(l) { Ok(Value::String(s)) => s, Ok(v) => self.value_to_string(&v), Err(_) => format!("{}", val) }
        } else { format!("{}", val) };
        let color_str = if let Some(c) = color {
            match self.evaluate_expr(c) { Ok(Value::String(s)) => Some(s), _ => None }
        } else { None };
        // hline is a horizontal line across all timestamps
        let timestamps = self.get_any_timestamps();
        let data: Vec<PlotPoint> = timestamps.iter()
            .map(|ts| PlotPoint { timestamp: *ts, value: Some(val), open: None, high: None, low: None, close: None, volume: None })
            .collect();
        self.plots.push(PlotData { plot_type: "line".to_string(), label: label_str, data, color: color_str });
    }

    fn first_symbol(&self) -> String {
        self.symbol_data.keys().next().cloned().unwrap_or_default()
    }

    // ─── Color Functions ────────────────────────────────────────────────────

    fn eval_color_rgb(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 3 { return Err("color.rgb() requires 3-4 arguments: r, g, b, [transparency]".to_string()); }
        let r = self.resolve_number(&args[0])? as u8;
        let g = self.resolve_number(&args[1])? as u8;
        let b = self.resolve_number(&args[2])? as u8;
        let a = if args.len() > 3 {
            let transp = self.resolve_number(&args[3])?; // 0-100 transparency
            ((100.0 - transp.clamp(0.0, 100.0)) * 2.55) as u8
        } else { 255 };
        Ok(Value::Color(ColorValue::new(r, g, b, a)))
    }

    // ─── Drawing Functions ──────────────────────────────────────────────────

    fn eval_line_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        // line.new(x1, y1, x2, y2, color?, style?, width?)
        if args.len() < 4 { return Err("line.new() requires at least 4 args: x1, y1, x2, y2".to_string()); }
        let x1 = self.resolve_number(&args[0])? as i64;
        let y1 = self.resolve_number(&args[1])?;
        let x2 = self.resolve_number(&args[2])? as i64;
        let y2 = self.resolve_number(&args[3])?;
        let color = if args.len() > 4 { self.resolve_color_string(&args[4])? } else { "#2196F3".to_string() };
        let style = if args.len() > 5 {
            match self.evaluate_expr(&args[5])? { Value::String(s) => s, _ => "solid".to_string() }
        } else { "solid".to_string() };
        let width = if args.len() > 6 { self.resolve_number(&args[6])? } else { 1.0 };
        let drawing = DrawingValue {
            drawing_type: "line".to_string(), x1, y1, x2, y2,
            color: color.clone(), text: String::new(), style, width,
        };
        self.drawings.push(DrawingInfo {
            drawing_type: "line".to_string(), x1, y1, x2, y2,
            color, text: String::new(), style: "solid".to_string(), width,
        });
        Ok(Value::Drawing(drawing))
    }

    fn eval_label_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        // label.new(x, y, text, color?, textcolor?, style?)
        if args.len() < 3 { return Err("label.new() requires at least 3 args: x, y, text".to_string()); }
        let x = self.resolve_number(&args[0])? as i64;
        let y = self.resolve_number(&args[1])?;
        let text = match self.evaluate_expr(&args[2])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        let color = if args.len() > 3 { self.resolve_color_string(&args[3])? } else { "#2196F3".to_string() };
        let drawing = DrawingValue {
            drawing_type: "label".to_string(), x1: x, y1: y, x2: x, y2: y,
            color: color.clone(), text: text.clone(), style: "label_down".to_string(), width: 1.0,
        };
        self.drawings.push(DrawingInfo {
            drawing_type: "label".to_string(), x1: x, y1: y, x2: x, y2: y,
            color, text, style: "label_down".to_string(), width: 1.0,
        });
        Ok(Value::Drawing(drawing))
    }

    fn eval_box_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        // box.new(x1, y1, x2, y2, border_color?, bg_color?, border_width?)
        if args.len() < 4 { return Err("box.new() requires at least 4 args: x1, y1, x2, y2".to_string()); }
        let x1 = self.resolve_number(&args[0])? as i64;
        let y1 = self.resolve_number(&args[1])?;
        let x2 = self.resolve_number(&args[2])? as i64;
        let y2 = self.resolve_number(&args[3])?;
        let color = if args.len() > 4 { self.resolve_color_string(&args[4])? } else { "#2196F3".to_string() };
        let width = if args.len() > 6 { self.resolve_number(&args[6])? } else { 1.0 };
        let drawing = DrawingValue {
            drawing_type: "box".to_string(), x1, y1, x2, y2,
            color: color.clone(), text: String::new(), style: "solid".to_string(), width,
        };
        self.drawings.push(DrawingInfo {
            drawing_type: "box".to_string(), x1, y1, x2, y2,
            color, text: String::new(), style: "solid".to_string(), width,
        });
        Ok(Value::Drawing(drawing))
    }

    // ─── Table Functions ────────────────────────────────────────────────────

    fn eval_table_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        // table.new(position, rows, cols, bgcolor?, border_color?, border_width?)
        let position = if !args.is_empty() {
            match self.evaluate_expr(&args[0])? { Value::String(s) => s, _ => "top_right".to_string() }
        } else { "top_right".to_string() };
        let rows = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 1 };
        let cols = if args.len() > 2 { self.resolve_number(&args[2])? as usize } else { 1 };
        let cells = vec![vec![TableCell { text: String::new(), bg_color: "#1a1a1a".to_string(), text_color: "#ffffff".to_string() }; cols]; rows];
        let table_id = format!("table_{}", self.plots.len());
        Ok(Value::Table(TableValue { id: table_id, rows, cols, cells, position }))
    }

    fn eval_table_cell(&mut self, args: &[Expr]) -> Result<Value, String> {
        // table.cell(table, row, col, text, bg_color?, text_color?)
        if args.len() < 4 { return Err("table.cell() requires at least 4 args: table, row, col, text".to_string()); }
        let table_var = match &args[0] {
            Expr::Variable(name) => name.clone(),
            _ => return Err("table.cell() first arg must be a table variable".to_string()),
        };
        let row = self.resolve_number(&args[1])? as usize;
        let col = self.resolve_number(&args[2])? as usize;
        let text = match self.evaluate_expr(&args[3])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        let bg_color = if args.len() > 4 { self.resolve_color_string(&args[4])? } else { "#1a1a1a".to_string() };
        let text_color = if args.len() > 5 { self.resolve_color_string(&args[5])? } else { "#ffffff".to_string() };

        let current = self.get_var(&table_var).cloned();
        if let Some(Value::Table(mut table)) = current {
            if row < table.rows && col < table.cols {
                table.cells[row][col] = TableCell { text, bg_color, text_color };
            }
            self.set_var_in_any_scope(&table_var, Value::Table(table));
        }
        Ok(Value::Void)
    }

    // ─── Map Functions ──────────────────────────────────────────────────────

    fn eval_map_put(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 3 { return Err("map.put() requires 3 args: map, key, value".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        let key = match self.evaluate_expr(&args[1])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        let val = self.evaluate_expr(&args[2])?;
        if let Value::Map(mut m) = map_val {
            m.insert(key, val);
            // Also update the variable if it was a variable reference
            if let Expr::Variable(name) = &args[0] {
                self.set_var_in_any_scope(name, Value::Map(m.clone()));
            }
            Ok(Value::Map(m))
        } else {
            Err("map.put() first arg must be a Map".to_string())
        }
    }

    fn eval_map_get(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("map.get() requires 2 args: map, key".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        let key = match self.evaluate_expr(&args[1])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        if let Value::Map(m) = map_val {
            Ok(m.get(&key).cloned().unwrap_or(Value::Na))
        } else {
            Err("map.get() first arg must be a Map".to_string())
        }
    }

    fn eval_map_keys(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("map.keys() requires 1 arg: map".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        if let Value::Map(m) = map_val {
            let keys: Vec<Value> = m.keys().map(|k| Value::String(k.clone())).collect();
            Ok(Value::Array(keys))
        } else {
            Err("map.keys() requires a Map".to_string())
        }
    }

    fn eval_map_size(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("map.size() requires 1 arg: map".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        if let Value::Map(m) = map_val {
            Ok(Value::Number(m.len() as f64))
        } else {
            Err("map.size() requires a Map".to_string())
        }
    }

    fn eval_map_contains(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("map.contains() requires 2 args: map, key".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        let key = match self.evaluate_expr(&args[1])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        if let Value::Map(m) = map_val {
            Ok(Value::Bool(m.contains_key(&key)))
        } else {
            Err("map.contains() requires a Map".to_string())
        }
    }

    // ─── Matrix Functions ───────────────────────────────────────────────────

    fn eval_matrix_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        let rows = if !args.is_empty() { self.resolve_number(&args[0])? as usize } else { 1 };
        let cols = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 1 };
        let init_val = if args.len() > 2 { self.resolve_number(&args[2])? } else { 0.0 };
        // Represent matrix as Array of Arrays
        let matrix: Vec<Value> = (0..rows).map(|_| {
            Value::Array((0..cols).map(|_| Value::Number(init_val)).collect())
        }).collect();
        Ok(Value::Array(matrix))
    }

    fn eval_matrix_get(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 3 { return Err("matrix.get() requires 3 args: matrix, row, col".to_string()); }
        let mat = self.evaluate_expr(&args[0])?;
        let row = self.resolve_number(&args[1])? as usize;
        let col = self.resolve_number(&args[2])? as usize;
        if let Value::Array(rows) = mat {
            if let Some(Value::Array(cols)) = rows.get(row) {
                Ok(cols.get(col).cloned().unwrap_or(Value::Na))
            } else { Ok(Value::Na) }
        } else { Err("matrix.get() first arg must be a matrix (Array of Arrays)".to_string()) }
    }

    fn eval_matrix_set(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 4 { return Err("matrix.set() requires 4 args: matrix, row, col, value".to_string()); }
        let mat_name = match &args[0] {
            Expr::Variable(name) => name.clone(),
            _ => return Err("matrix.set() first arg must be a variable".to_string()),
        };
        let row = self.resolve_number(&args[1])? as usize;
        let col = self.resolve_number(&args[2])? as usize;
        let val = self.evaluate_expr(&args[3])?;
        let current = self.get_var(&mat_name).cloned();
        if let Some(Value::Array(mut rows)) = current {
            if let Some(Value::Array(ref mut cols)) = rows.get_mut(row) {
                if col < cols.len() { cols[col] = val; }
            }
            self.set_var_in_any_scope(&mat_name, Value::Array(rows));
        }
        Ok(Value::Void)
    }

    fn eval_matrix_rows(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("matrix.rows() requires 1 arg".to_string()); }
        let mat = self.evaluate_expr(&args[0])?;
        if let Value::Array(rows) = mat { Ok(Value::Number(rows.len() as f64)) }
        else { Err("matrix.rows() requires a matrix".to_string()) }
    }

    fn eval_matrix_cols(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("matrix.cols() requires 1 arg".to_string()); }
        let mat = self.evaluate_expr(&args[0])?;
        if let Value::Array(rows) = mat {
            let cols = if let Some(Value::Array(first_row)) = rows.first() { first_row.len() } else { 0 };
            Ok(Value::Number(cols as f64))
        } else { Err("matrix.cols() requires a matrix".to_string()) }
    }

    // ─── Request Functions ──────────────────────────────────────────────────

    fn eval_request_security(&mut self, args: &[Expr]) -> Result<Value, String> {
        // request.security(symbol, timeframe, expression)
        // In our implementation, we fetch data for the symbol if available
        if args.is_empty() { return Err("request.security() requires at least 1 arg: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
        // If symbol data is available, return close series by default
        if let Some(ohlcv) = self.symbol_data.get(&symbol) {
            Ok(Value::Series(SeriesData {
                values: ohlcv.close.clone(),
                timestamps: ohlcv.timestamps.clone(),
            }))
        } else {
            self.output_lines.push(format!("[REQUEST] Security '{}' - data not pre-loaded", symbol));
            Ok(Value::Na)
        }
    }

    // ─── Alert Functions ────────────────────────────────────────────────────

    fn eval_alert_fn(&mut self, args: &[Expr]) -> Result<Value, String> {
        let msg = if !args.is_empty() {
            match self.evaluate_expr(&args[0])? { Value::String(s) => s, v => self.value_to_string(&v) }
        } else { "Alert triggered".to_string() };
        let atype = if args.len() > 1 {
            match self.evaluate_expr(&args[1])? { Value::String(s) => s, _ => "once".to_string() }
        } else { "once".to_string() };
        self.output_lines.push(format!("[ALERT] {}", msg));
        self.alerts.push(AlertInfo {
            message: msg,
            alert_type: atype,
            timestamp: chrono::Utc::now().to_rfc3339(),
        });
        Ok(Value::Void)
    }

    fn eval_alertcondition(&mut self, args: &[Expr]) -> Result<Value, String> {
        // alertcondition(condition, title?, message?)
        if args.is_empty() { return Err("alertcondition() requires at least 1 arg".to_string()); }
        let cond = self.evaluate_expr(&args[0])?;
        if cond.is_truthy() {
            let title = if args.len() > 1 {
                match self.evaluate_expr(&args[1])? { Value::String(s) => s, v => self.value_to_string(&v) }
            } else { "Alert".to_string() };
            let msg = if args.len() > 2 {
                match self.evaluate_expr(&args[2])? { Value::String(s) => s, v => self.value_to_string(&v) }
            } else { title.clone() };
            self.output_lines.push(format!("[ALERTCONDITION] {}: {}", title, msg));
            self.alerts.push(AlertInfo {
                message: format!("{}: {}", title, msg),
                alert_type: "condition".to_string(),
                timestamp: chrono::Utc::now().to_rfc3339(),
            });
        }
        Ok(Value::Void)
    }

    // ─── Helper: resolve color to string ────────────────────────────────────

    fn resolve_color_string(&mut self, expr: &Expr) -> Result<String, String> {
        let val = self.evaluate_expr(expr)?;
        match val {
            Value::Color(c) => Ok(c.to_hex()),
            Value::String(s) => {
                // Check if it's a named color
                if let Some(c) = ColorValue::from_name(&s) {
                    Ok(c.to_hex())
                } else {
                    Ok(s) // assume it's already a hex/rgba string
                }
            }
            _ => Ok("#2196F3".to_string()),
        }
    }

    // ─── Helper Methods ─────────────────────────────────────────────────────

    fn resolve_symbol(&mut self, expr: &Expr) -> Result<String, String> {
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

    fn resolve_number(&mut self, expr: &Expr) -> Result<f64, String> {
        let val = self.evaluate_expr(expr)?;
        val.as_number().ok_or_else(|| format!("Expected a number, got {}", val.type_name()))
    }

    fn get_last_close_price(&self) -> Option<f64> {
        for ohlcv in self.symbol_data.values() {
            if let Some(price) = ohlcv.close.last() {
                return Some(*price);
            }
        }
        None
    }

    fn get_any_timestamps(&self) -> Vec<i64> {
        for ohlcv in self.symbol_data.values() {
            if !ohlcv.timestamps.is_empty() {
                return ohlcv.timestamps.clone();
            }
        }
        vec![]
    }

    // ─── Plot Handlers ──────────────────────────────────────────────────────

    fn handle_plot(&mut self, expr: &Expr, label: &Expr) {
        let label_str = match self.evaluate_expr(label) {
            Ok(Value::String(s)) => s,
            Ok(v) => self.value_to_string(&v),
            Err(e) => { self.errors.push(e); return; }
        };
        match self.evaluate_expr(expr) {
            Ok(Value::Series(series)) => {
                let data: Vec<PlotPoint> = series.timestamps.iter().zip(series.values.iter())
                    .filter(|(_, v)| !v.is_nan())
                    .map(|(ts, val)| PlotPoint { timestamp: *ts, value: Some(*val), open: None, high: None, low: None, close: None, volume: None })
                    .collect();
                self.plots.push(PlotData { plot_type: "line".to_string(), label: label_str, data, color: None });
            }
            Ok(Value::Number(n)) => {
                let timestamps = self.get_any_timestamps();
                let data: Vec<PlotPoint> = timestamps.iter()
                    .map(|ts| PlotPoint { timestamp: *ts, value: Some(n), open: None, high: None, low: None, close: None, volume: None })
                    .collect();
                self.plots.push(PlotData { plot_type: "line".to_string(), label: label_str, data, color: None });
            }
            Ok(_) => self.errors.push("Cannot plot: value is not Series or Number".to_string()),
            Err(e) => self.errors.push(e),
        }
    }

    fn handle_plot_candlestick(&mut self, symbol: &Expr, title: &Expr) {
        let title_str = match self.evaluate_expr(title) {
            Ok(Value::String(s)) => s,
            Ok(v) => self.value_to_string(&v),
            Err(e) => { self.errors.push(e); return; }
        };
        let symbol_name = match symbol {
            Expr::Symbol(s) => s.clone(),
            Expr::Variable(s) => s.clone(),
            _ => match self.evaluate_expr(symbol) {
                Ok(Value::String(s)) => s,
                _ => { self.errors.push("plot_candlestick requires a symbol".to_string()); return; }
            }
        };
        if let Some(ohlcv) = self.symbol_data.get(&symbol_name) {
            let data: Vec<PlotPoint> = ohlcv.timestamps.iter().enumerate()
                .map(|(i, ts)| PlotPoint {
                    timestamp: *ts, value: None,
                    open: Some(ohlcv.open[i]), high: Some(ohlcv.high[i]),
                    low: Some(ohlcv.low[i]), close: Some(ohlcv.close[i]),
                    volume: Some(ohlcv.volume[i]),
                }).collect();
            self.plots.push(PlotData { plot_type: "candlestick".to_string(), label: title_str, data, color: None });
        } else {
            self.errors.push(format!("No data for symbol '{}'", symbol_name));
        }
    }

    fn handle_plot_line(&mut self, value: &Expr, label: &Expr, color: &Option<Expr>) {
        let label_str = match self.evaluate_expr(label) {
            Ok(Value::String(s)) => s,
            Ok(v) => self.value_to_string(&v),
            Err(e) => { self.errors.push(e); return; }
        };
        let color_str = if let Some(c) = color {
            match self.evaluate_expr(c) { Ok(Value::String(s)) => Some(s), _ => None }
        } else { None };

        match self.evaluate_expr(value) {
            Ok(Value::Series(series)) => {
                let data: Vec<PlotPoint> = series.timestamps.iter().zip(series.values.iter())
                    .filter(|(_, v)| !v.is_nan())
                    .map(|(ts, val)| PlotPoint { timestamp: *ts, value: Some(*val), open: None, high: None, low: None, close: None, volume: None })
                    .collect();
                self.plots.push(PlotData { plot_type: "line".to_string(), label: label_str, data, color: color_str });
            }
            Ok(Value::Number(n)) => {
                let timestamps = self.get_any_timestamps();
                let data: Vec<PlotPoint> = timestamps.iter()
                    .map(|ts| PlotPoint { timestamp: *ts, value: Some(n), open: None, high: None, low: None, close: None, volume: None })
                    .collect();
                self.plots.push(PlotData { plot_type: "line".to_string(), label: label_str, data, color: color_str });
            }
            Ok(_) => self.errors.push("plot_line value must be Series or Number".to_string()),
            Err(e) => self.errors.push(e),
        }
    }

    fn value_to_string(&self, val: &Value) -> String {
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

/// Collect all unique symbol names from a program's AST
pub fn collect_symbols(program: &Program) -> Vec<String> {
    let mut symbols = Vec::new();
    for stmt in program {
        collect_symbols_from_statement(stmt, &mut symbols);
    }
    symbols.sort();
    symbols.dedup();
    symbols
}

fn collect_symbols_from_statement(stmt: &Statement, symbols: &mut Vec<String>) {
    match stmt {
        Statement::Assignment { value, .. } => collect_symbols_from_expr(value, symbols),
        Statement::CompoundAssign { value, .. } => collect_symbols_from_expr(value, symbols),
        Statement::IndexAssign { index, value, .. } => {
            collect_symbols_from_expr(index, symbols);
            collect_symbols_from_expr(value, symbols);
        }
        Statement::IfBlock { condition, body, else_if_blocks, else_body } => {
            collect_symbols_from_expr(condition, symbols);
            for s in body { collect_symbols_from_statement(s, symbols); }
            for (cond, stmts) in else_if_blocks {
                collect_symbols_from_expr(cond, symbols);
                for s in stmts { collect_symbols_from_statement(s, symbols); }
            }
            if let Some(eb) = else_body {
                for s in eb { collect_symbols_from_statement(s, symbols); }
            }
        }
        Statement::ForLoop { iterable, body, .. } => {
            collect_symbols_from_expr(iterable, symbols);
            for s in body { collect_symbols_from_statement(s, symbols); }
        }
        Statement::WhileLoop { condition, body } => {
            collect_symbols_from_expr(condition, symbols);
            for s in body { collect_symbols_from_statement(s, symbols); }
        }
        Statement::FnDef { body, .. } => {
            for s in body { collect_symbols_from_statement(s, symbols); }
        }
        Statement::Return { value } => {
            if let Some(v) = value { collect_symbols_from_expr(v, symbols); }
        }
        Statement::Buy { message } => collect_symbols_from_expr(message, symbols),
        Statement::Sell { message } => collect_symbols_from_expr(message, symbols),
        Statement::Plot { expr, label } => {
            collect_symbols_from_expr(expr, symbols);
            collect_symbols_from_expr(label, symbols);
        }
        Statement::PlotCandlestick { symbol, title } => {
            collect_symbols_from_expr(symbol, symbols);
            collect_symbols_from_expr(title, symbols);
        }
        Statement::PlotLine { value, label, color } => {
            collect_symbols_from_expr(value, symbols);
            collect_symbols_from_expr(label, symbols);
            if let Some(c) = color { collect_symbols_from_expr(c, symbols); }
        }
        Statement::PlotHistogram { value, label, color_up, color_down } => {
            collect_symbols_from_expr(value, symbols);
            collect_symbols_from_expr(label, symbols);
            if let Some(c) = color_up { collect_symbols_from_expr(c, symbols); }
            if let Some(c) = color_down { collect_symbols_from_expr(c, symbols); }
        }
        Statement::PlotShape { condition, shape, location, color, text } => {
            collect_symbols_from_expr(condition, symbols);
            collect_symbols_from_expr(shape, symbols);
            collect_symbols_from_expr(location, symbols);
            if let Some(c) = color { collect_symbols_from_expr(c, symbols); }
            if let Some(t) = text { collect_symbols_from_expr(t, symbols); }
        }
        Statement::Bgcolor { color, condition } => {
            collect_symbols_from_expr(color, symbols);
            if let Some(c) = condition { collect_symbols_from_expr(c, symbols); }
        }
        Statement::Hline { value, label, color } => {
            collect_symbols_from_expr(value, symbols);
            if let Some(l) = label { collect_symbols_from_expr(l, symbols); }
            if let Some(c) = color { collect_symbols_from_expr(c, symbols); }
        }
        Statement::SwitchBlock { expr, cases, default } => {
            if let Some(e) = expr { collect_symbols_from_expr(e, symbols); }
            for (case_expr, body) in cases {
                collect_symbols_from_expr(case_expr, symbols);
                for s in body { collect_symbols_from_statement(s, symbols); }
            }
            if let Some(def) = default {
                for s in def { collect_symbols_from_statement(s, symbols); }
            }
        }
        Statement::StrategyEntry { id, direction, qty, price, stop, limit } => {
            collect_symbols_from_expr(id, symbols);
            collect_symbols_from_expr(direction, symbols);
            if let Some(q) = qty { collect_symbols_from_expr(q, symbols); }
            if let Some(p) = price { collect_symbols_from_expr(p, symbols); }
            if let Some(s) = stop { collect_symbols_from_expr(s, symbols); }
            if let Some(l) = limit { collect_symbols_from_expr(l, symbols); }
        }
        Statement::StrategyExit { id, from_entry, qty, stop, limit, trail_points, trail_offset } => {
            collect_symbols_from_expr(id, symbols);
            if let Some(f) = from_entry { collect_symbols_from_expr(f, symbols); }
            if let Some(q) = qty { collect_symbols_from_expr(q, symbols); }
            if let Some(s) = stop { collect_symbols_from_expr(s, symbols); }
            if let Some(l) = limit { collect_symbols_from_expr(l, symbols); }
            if let Some(tp) = trail_points { collect_symbols_from_expr(tp, symbols); }
            if let Some(to) = trail_offset { collect_symbols_from_expr(to, symbols); }
        }
        Statement::StrategyClose { id } => {
            collect_symbols_from_expr(id, symbols);
        }
        Statement::InputDecl { default_value, title, .. } => {
            collect_symbols_from_expr(default_value, symbols);
            if let Some(t) = title { collect_symbols_from_expr(t, symbols); }
        }
        Statement::StructDef { .. } => {}
        Statement::AlertStatement { message, alert_type } => {
            collect_symbols_from_expr(message, symbols);
            if let Some(at) = alert_type { collect_symbols_from_expr(at, symbols); }
        }
        Statement::PrintStatement { args } => {
            for arg in args { collect_symbols_from_expr(arg, symbols); }
        }
        Statement::ImportStatement { .. } => {}
        Statement::ExportStatement { .. } => {}
        Statement::ExprStatement(expr) => collect_symbols_from_expr(expr, symbols),
        Statement::Comment(_) | Statement::Break | Statement::Continue => {}
    }
}

fn collect_symbols_from_expr(expr: &Expr, symbols: &mut Vec<String>) {
    match expr {
        Expr::Symbol(s) => {
            if !symbols.contains(s) { symbols.push(s.clone()); }
        }
        Expr::BinaryOp { left, right, .. } => {
            collect_symbols_from_expr(left, symbols);
            collect_symbols_from_expr(right, symbols);
        }
        Expr::UnaryOp { operand, .. } => {
            collect_symbols_from_expr(operand, symbols);
        }
        Expr::FunctionCall { args, .. } => {
            for arg in args { collect_symbols_from_expr(arg, symbols); }
        }
        Expr::MethodCall { object, args, .. } => {
            collect_symbols_from_expr(object, symbols);
            for arg in args { collect_symbols_from_expr(arg, symbols); }
        }
        Expr::ArrayLiteral(elements) => {
            for el in elements { collect_symbols_from_expr(el, symbols); }
        }
        Expr::IndexAccess { object, index } => {
            collect_symbols_from_expr(object, symbols);
            collect_symbols_from_expr(index, symbols);
        }
        Expr::Range { start, end } => {
            collect_symbols_from_expr(start, symbols);
            collect_symbols_from_expr(end, symbols);
        }
        Expr::Ternary { condition, then_expr, else_expr } => {
            collect_symbols_from_expr(condition, symbols);
            collect_symbols_from_expr(then_expr, symbols);
            collect_symbols_from_expr(else_expr, symbols);
        }
        Expr::MapLiteral(entries) => {
            for (_, val) in entries { collect_symbols_from_expr(val, symbols); }
        }
        Expr::StructLiteral { fields, .. } => {
            for (_, val) in fields { collect_symbols_from_expr(val, symbols); }
        }
        _ => {}
    }
}

// ─── Tests ──────────────────────────────────────────────────────────────────
#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::tokenize;
    use crate::parser::parse;
    use crate::types::OhlcvSeries;

    /// Generate test OHLCV data (same logic as finscript_cmd.rs)
    fn gen_ohlcv(symbol: &str, days: usize) -> OhlcvSeries {
        let seed: u64 = symbol.bytes().fold(0u64, |acc, b| acc.wrapping_mul(31).wrapping_add(b as u64));
        let base_price = 50.0 + (seed % 450) as f64;
        let volatility = 0.015 + (seed % 20) as f64 * 0.001;

        let mut timestamps = Vec::with_capacity(days);
        let mut open = Vec::with_capacity(days);
        let mut high = Vec::with_capacity(days);
        let mut low = Vec::with_capacity(days);
        let mut close = Vec::with_capacity(days);
        let mut volume = Vec::with_capacity(days);

        let start_ts: i64 = 1719792000;
        let mut price = base_price;
        let mut rng_state = seed;

        for i in 0..days {
            rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
            let r1 = ((rng_state >> 33) as f64) / (u32::MAX as f64) - 0.5;
            rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
            let r2 = ((rng_state >> 33) as f64) / (u32::MAX as f64);
            rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
            let r3 = ((rng_state >> 33) as f64) / (u32::MAX as f64);

            let change = price * volatility * r1 * 2.0;
            let day_open = price;
            let day_close = price + change;
            let spread = (change.abs() + price * volatility * r2 * 0.5).max(price * 0.002);
            let day_high = day_open.max(day_close) + spread * r2;
            let day_low = day_open.min(day_close) - spread * r3;
            let base_vol = 1_000_000.0 + (seed % 9_000_000) as f64;
            let day_volume = base_vol * (1.0 + change.abs() / price * 10.0) * (0.5 + r3);

            timestamps.push(start_ts + (i as i64) * 86400);
            open.push(day_open.max(0.01));
            high.push(day_high.max(day_open.max(day_close)));
            low.push(day_low.min(day_open.min(day_close)).max(0.01));
            close.push(day_close.max(0.01));
            volume.push(day_volume.round());
            price = day_close.max(0.01);
        }

        OhlcvSeries { symbol: symbol.to_string(), timestamps, open, high, low, close, volume }
    }

    fn run_script(code: &str) -> InterpreterResult {
        let tokens = tokenize(code).expect("Lexer failed");
        let program = parse(tokens).expect("Parser failed");
        let symbols = collect_symbols(&program);
        let mut symbol_data = HashMap::new();
        for s in &symbols {
            symbol_data.insert(s.clone(), gen_ohlcv(s, 180));
        }
        let mut interp = Interpreter::new(symbol_data);
        interp.execute(&program)
    }

    #[test]
    fn test_default_strategy() {
        let code = r#"
ema_fast = ema(AAPL, 12)
ema_slow = ema(AAPL, 26)
rsi_val = rsi(AAPL, 14)

fast = last(ema_fast)
slow = last(ema_slow)
rsi_now = last(rsi_val)

print "EMA(12):", fast
print "EMA(26):", slow
print "RSI(14):", rsi_now

if fast > slow {
    if rsi_now < 70 {
        buy "Bullish crossover"
    }
}

if fast < slow {
    if rsi_now > 30 {
        sell "Bearish crossover"
    }
}

plot_candlestick AAPL, "AAPL Price"
plot_line ema_fast, "EMA (12)", "cyan"
plot_line ema_slow, "EMA (26)", "orange"
plot rsi_val, "RSI (14)"
"#;
        let result = run_script(code);
        println!("Output:\n{}", result.output);
        println!("Signals: {:?}", result.signals);
        println!("Plots: {} charts", result.plots.len());
        for e in &result.errors {
            println!("ERROR: {}", e);
        }
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(!result.output.is_empty(), "Should have print output");
        assert!(result.plots.len() >= 3, "Should have at least 3 plots");
    }

    #[test]
    fn test_print_statement() {
        let result = run_script(r#"
x = 42
print "Hello", x
print "Sum:", 10 + 20
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("Hello"), "Should contain Hello");
        assert!(result.output.contains("42"), "Should contain 42");
        assert!(result.output.contains("30"), "Should contain 30");
    }

    #[test]
    fn test_if_else() {
        let result = run_script(r#"
x = 10
if x > 5 {
    print "greater"
} else {
    print "lesser"
}
"#);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("greater"));
    }

    #[test]
    fn test_for_loop() {
        let result = run_script(r#"
total = 0
for i in 1..6 {
    total = total + i
}
print "total:", total
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("15"), "1+2+3+4+5 = 15");
    }

    #[test]
    fn test_while_loop() {
        let result = run_script(r#"
counter = 0
result = 1
while counter < 10 {
    result = result * 2
    counter = counter + 1
}
print "2^10 =", result
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("1024"));
    }

    #[test]
    fn test_user_function() {
        let result = run_script(r#"
fn add(a, b) {
    return a + b
}
print "result:", add(3, 7)
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("10"));
    }

    #[test]
    fn test_indicators_sma_ema_rsi() {
        let result = run_script(r#"
sma_val = sma(AAPL, 20)
ema_val = ema(AAPL, 12)
rsi_val = rsi(AAPL, 14)
print "SMA:", last(sma_val)
print "EMA:", last(ema_val)
print "RSI:", last(rsi_val)
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("SMA:"));
        assert!(result.output.contains("EMA:"));
        assert!(result.output.contains("RSI:"));
    }

    #[test]
    fn test_macd() {
        let result = run_script(r#"
macd_line = macd(AAPL, 12, 26, 9)
print "MACD:", last(macd_line)
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("MACD:"));
    }

    #[test]
    fn test_bollinger() {
        let result = run_script(r#"
bb = bollinger(AAPL, 20, 2)
print "Bollinger:", last(bb)
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    }

    #[test]
    fn test_buy_sell_signals() {
        let result = run_script(r#"
buy "test buy signal"
sell "test sell signal"
"#);
        println!("Signals: {:?}", result.signals);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert_eq!(result.signals.len(), 2);
        assert_eq!(result.signals[0].signal_type, "Buy");
        assert_eq!(result.signals[1].signal_type, "Sell");
    }

    #[test]
    fn test_plot_candlestick() {
        let result = run_script(r#"
plot_candlestick AAPL, "Apple"
"#);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert_eq!(result.plots.len(), 1);
        assert_eq!(result.plots[0].plot_type, "candlestick");
        assert!(!result.plots[0].data.is_empty());
    }

    #[test]
    fn test_plot_line() {
        let result = run_script(r#"
data = sma(AAPL, 20)
plot_line data, "SMA 20", "blue"
"#);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert_eq!(result.plots.len(), 1);
        assert_eq!(result.plots[0].plot_type, "line");
    }

    #[test]
    fn test_ternary() {
        let result = run_script(r#"
x = 10
msg = x > 5 ? "big" : "small"
print msg
"#);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("big"));
    }

    #[test]
    fn test_switch() {
        let result = run_script(r#"
val = "a"
switch val {
    "a" {
        print "matched a"
    }
    "b" {
        print "matched b"
    }
    else {
        print "no match"
    }
}
"#);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("matched a"));
    }

    #[test]
    fn test_arrays() {
        let result = run_script(r#"
arr = [10, 20, 30, 40, 50]
print "len:", len(arr)
print "first:", arr[0]
print "last:", arr[4]
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("5"));
        assert!(result.output.contains("10"));
        assert!(result.output.contains("50"));
    }

    #[test]
    fn test_multi_symbol() {
        let result = run_script(r#"
aapl_sma = sma(AAPL, 20)
msft_sma = sma(MSFT, 20)
print "AAPL SMA:", last(aapl_sma)
print "MSFT SMA:", last(msft_sma)
"#);
        println!("Output:\n{}", result.output);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
        assert!(result.output.contains("AAPL SMA:"));
        assert!(result.output.contains("MSFT SMA:"));
    }

    #[test]
    fn test_nested_if_buy() {
        let result = run_script(r#"
rsi_val = rsi(AAPL, 14)
rsi_now = last(rsi_val)
if rsi_now > 0 {
    buy "RSI is positive"
}
"#);
        println!("Output:\n{}", result.output);
        println!("Signals: {:?}", result.signals);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    }

    #[test]
    fn test_hline() {
        let result = run_script(r#"
hline 70, "Overbought", "red"
hline 30, "Oversold", "green"
"#);
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    }

    #[test]
    fn test_strategy_mode() {
        let result = run_script(r#"
ema_f = ema(AAPL, 12)
ema_s = ema(AAPL, 26)
fast = last(ema_f)
slow = last(ema_s)
if fast > slow {
    strategy.entry("Long", "long")
}
if fast < slow {
    strategy.entry("Short", "short")
}
"#);
        println!("Output:\n{}", result.output);
        for e in &result.errors {
            println!("ERROR: {}", e);
        }
        assert!(result.errors.is_empty(), "Errors: {:?}", result.errors);
    }
}
