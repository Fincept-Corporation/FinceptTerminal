use crate::ast::*;
use crate::types::*;
use crate::indicators;
use std::collections::HashMap;
use super::{Interpreter, ControlFlow, UserFunction};

impl Interpreter {
    pub(crate) fn eval_function_call(&mut self, name: &str, args: &[Expr]) -> Result<Value, String> {
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
            // Risk & portfolio analytics
            "sharpe" => self.eval_sharpe(args),
            "sortino" => self.eval_sortino(args),
            "max_drawdown" => self.eval_max_drawdown(args),
            "correlation" => self.eval_correlation(args),
            "beta" => self.eval_beta(args),
            "returns" => self.eval_returns(args),
            // Terminal integration functions
            "watchlist_add" => self.eval_watchlist_add(args),
            "paper_trade" => self.eval_paper_trade(args),
            "alert_create" => self.eval_alert_create(args),
            "screener_scan" => self.eval_screener_scan(args),
            _ => Err(format!("Unknown function '{}'", name)),
        }
    }

    pub(crate) fn call_user_function(&mut self, func: &UserFunction, args: &[Expr]) -> Result<Value, String> {
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
}
