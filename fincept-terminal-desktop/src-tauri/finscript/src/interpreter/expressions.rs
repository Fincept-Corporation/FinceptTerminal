use std::collections::HashMap;
use crate::ast::*;
use crate::types::*;
use super::{Interpreter, MAX_LOOP_ITERATIONS};

impl Interpreter {
    pub(crate) fn evaluate_expr(&mut self, expr: &Expr) -> Result<Value, String> {
        match expr {
            Expr::Number(n) => Ok(Value::Number(*n)),
            Expr::StringLiteral(s) => Ok(Value::String(s.clone())),
            Expr::Bool(b) => Ok(Value::Bool(*b)),
            Expr::Na => Ok(Value::Na),
            Expr::Symbol(s) => Ok(Value::String(s.clone())),
            Expr::Variable(name) => {
                self.get_var(name)
                    .cloned()
                    .ok_or_else(|| format!("Undefined variable '{}'", name))
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
            Expr::FunctionCall { name, args } => {
                self.eval_function_call(name, args)
            }
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

    pub(crate) fn eval_binary_op(&self, left: &Value, op: &BinOp, right: &Value) -> Result<Value, String> {
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

    pub(crate) fn eval_unary_op(&self, op: &UnaryOp, val: &Value) -> Result<Value, String> {
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
}
