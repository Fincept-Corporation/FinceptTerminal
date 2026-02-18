use crate::ast::*;
use crate::types::*;
use crate::{Signal, PlotData, PlotPoint};
use super::Interpreter;

impl Interpreter {
    pub(crate) fn handle_plot(&mut self, expr: &Expr, label: &Expr) {
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

    pub(crate) fn handle_plot_candlestick(&mut self, symbol: &Expr, title: &Expr) {
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

    pub(crate) fn handle_plot_line(&mut self, value: &Expr, label: &Expr, color: &Option<Expr>) {
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

    pub(crate) fn handle_plot_histogram(&mut self, value: &Expr, label: &Expr, color_up: &Option<Expr>, _color_down: &Option<Expr>) {
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

    pub(crate) fn handle_plot_shape(&mut self, condition: &Expr, shape: &Expr, _location: &Expr, color: &Option<Expr>, text: &Option<Expr>) {
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

    pub(crate) fn handle_hline(&mut self, value: &Expr, label: &Option<Expr>, color: &Option<Expr>) {
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
        let timestamps = self.get_any_timestamps();
        let data: Vec<PlotPoint> = timestamps.iter()
            .map(|ts| PlotPoint { timestamp: *ts, value: Some(val), open: None, high: None, low: None, close: None, volume: None })
            .collect();
        self.plots.push(PlotData { plot_type: "line".to_string(), label: label_str, data, color: color_str });
    }
}
