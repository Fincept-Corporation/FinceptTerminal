use crate::ast::*;
use crate::types::*;
use super::Interpreter;

impl Interpreter {
    pub(crate) fn eval_method_call(&mut self, object: &Expr, method: &str, args: &[Expr]) -> Result<Value, String> {
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
}
