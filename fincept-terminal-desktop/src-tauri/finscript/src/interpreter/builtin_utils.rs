use crate::ast::*;
use crate::types::*;
use super::{Interpreter, MAX_LOOP_ITERATIONS};

impl Interpreter {
    pub(crate) fn eval_last(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_first(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_len(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("len() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Series(s) => Ok(Value::Number(s.values.len() as f64)),
            Value::Array(a) => Ok(Value::Number(a.len() as f64)),
            Value::String(s) => Ok(Value::Number(s.len() as f64)),
            _ => Err(format!("len() expects Series, Array, or String, got {}", val.type_name())),
        }
    }

    pub(crate) fn eval_abs(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_max(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_min(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_sum(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_avg(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_sqrt(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("sqrt() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        match val {
            Value::Number(n) => Ok(Value::Number(n.sqrt())),
            _ => Err(format!("sqrt() expects Number")),
        }
    }

    pub(crate) fn eval_pow(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("pow() requires 2 arguments: base, exponent".to_string()); }
        let base = self.evaluate_expr(&args[0])?.as_number().ok_or("pow() base must be number")?;
        let exp = self.evaluate_expr(&args[1])?.as_number().ok_or("pow() exponent must be number")?;
        Ok(Value::Number(base.powf(exp)))
    }

    pub(crate) fn eval_log(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("log() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("log() expects Number")?;
        Ok(Value::Number(val.ln()))
    }

    pub(crate) fn eval_log10(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("log10() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("log10() expects Number")?;
        Ok(Value::Number(val.log10()))
    }

    pub(crate) fn eval_exp(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("exp() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("exp() expects Number")?;
        Ok(Value::Number(val.exp()))
    }

    pub(crate) fn eval_trig(&mut self, args: &[Expr], f: fn(f64) -> f64) -> Result<Value, String> {
        if args.is_empty() { return Err("trig function requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("expects Number")?;
        Ok(Value::Number(f(val)))
    }

    pub(crate) fn eval_round(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("round() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("round() expects Number")?;
        let decimals = if args.len() > 1 {
            self.evaluate_expr(&args[1])?.as_number().unwrap_or(0.0) as i32
        } else { 0 };
        let factor = 10_f64.powi(decimals);
        Ok(Value::Number((val * factor).round() / factor))
    }

    pub(crate) fn eval_floor(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("floor() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("floor() expects Number")?;
        Ok(Value::Number(val.floor()))
    }

    pub(crate) fn eval_ceil(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("ceil() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("ceil() expects Number")?;
        Ok(Value::Number(val.ceil()))
    }

    pub(crate) fn eval_sign(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("sign() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?.as_number().ok_or("sign() expects Number")?;
        Ok(Value::Number(if val > 0.0 { 1.0 } else if val < 0.0 { -1.0 } else { 0.0 }))
    }

    pub(crate) fn eval_push(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 2 { return Err("push() requires 2 arguments: array, value".to_string()); }
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

    pub(crate) fn eval_slice(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_print(&mut self, args: &[Expr]) -> Result<Value, String> {
        let mut parts = Vec::new();
        for arg in args {
            let val = self.evaluate_expr(arg)?;
            parts.push(self.value_to_string(&val));
        }
        self.output_lines.push(parts.join(" "));
        Ok(Value::Void)
    }

    pub(crate) fn eval_str(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("str() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        Ok(Value::String(self.value_to_string(&val)))
    }

    pub(crate) fn eval_num(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_type(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("type() requires 1 argument".to_string()); }
        let val = self.evaluate_expr(&args[0])?;
        Ok(Value::String(val.type_name().to_string()))
    }

    pub(crate) fn eval_range_fn(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("range() requires 1-2 arguments".to_string()); }
        let start = if args.len() > 1 {
            self.evaluate_expr(&args[0])?.as_number().ok_or("range start must be number")? as i64
        } else { 0 };
        let end_idx = if args.len() > 1 { 1 } else { 0 };
        let end = self.evaluate_expr(&args[end_idx])?.as_number().ok_or("range end must be number")? as i64;
        let items: Vec<Value> = (start..end).take(MAX_LOOP_ITERATIONS).map(|i| Value::Number(i as f64)).collect();
        Ok(Value::Array(items))
    }

    pub(crate) fn eval_crossover(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_crossunder(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_na_fn(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Ok(Value::Na); }
        let val = self.evaluate_expr(&args[0])?;
        Ok(Value::Bool(val.is_na()))
    }

    pub(crate) fn eval_nz(&mut self, args: &[Expr]) -> Result<Value, String> {
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
}
