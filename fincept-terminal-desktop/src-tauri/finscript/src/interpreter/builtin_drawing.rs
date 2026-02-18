use crate::ast::*;
use crate::types::*;
use crate::{AlertInfo, DrawingInfo};
use super::Interpreter;

impl Interpreter {
    pub(crate) fn eval_color_rgb(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_line_new(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_label_new(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_box_new(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_table_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        let position = if !args.is_empty() {
            match self.evaluate_expr(&args[0])? { Value::String(s) => s, _ => "top_right".to_string() }
        } else { "top_right".to_string() };
        let rows = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 1 };
        let cols = if args.len() > 2 { self.resolve_number(&args[2])? as usize } else { 1 };
        let cells = vec![vec![TableCell { text: String::new(), bg_color: "#1a1a1a".to_string(), text_color: "#ffffff".to_string() }; cols]; rows];
        let table_id = format!("table_{}", self.plots.len());
        Ok(Value::Table(TableValue { id: table_id, rows, cols, cells, position }))
    }

    pub(crate) fn eval_table_cell(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_map_put(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.len() < 3 { return Err("map.put() requires 3 args: map, key, value".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        let key = match self.evaluate_expr(&args[1])? {
            Value::String(s) => s,
            v => self.value_to_string(&v),
        };
        let val = self.evaluate_expr(&args[2])?;
        if let Value::Map(mut m) = map_val {
            m.insert(key, val);
            if let Expr::Variable(name) = &args[0] {
                self.set_var_in_any_scope(name, Value::Map(m.clone()));
            }
            Ok(Value::Map(m))
        } else {
            Err("map.put() first arg must be a Map".to_string())
        }
    }

    pub(crate) fn eval_map_get(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_map_keys(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("map.keys() requires 1 arg: map".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        if let Value::Map(m) = map_val {
            let keys: Vec<Value> = m.keys().map(|k| Value::String(k.clone())).collect();
            Ok(Value::Array(keys))
        } else {
            Err("map.keys() requires a Map".to_string())
        }
    }

    pub(crate) fn eval_map_size(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("map.size() requires 1 arg: map".to_string()); }
        let map_val = self.evaluate_expr(&args[0])?;
        if let Value::Map(m) = map_val {
            Ok(Value::Number(m.len() as f64))
        } else {
            Err("map.size() requires a Map".to_string())
        }
    }

    pub(crate) fn eval_map_contains(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_matrix_new(&mut self, args: &[Expr]) -> Result<Value, String> {
        let rows = if !args.is_empty() { self.resolve_number(&args[0])? as usize } else { 1 };
        let cols = if args.len() > 1 { self.resolve_number(&args[1])? as usize } else { 1 };
        let init_val = if args.len() > 2 { self.resolve_number(&args[2])? } else { 0.0 };
        let matrix: Vec<Value> = (0..rows).map(|_| {
            Value::Array((0..cols).map(|_| Value::Number(init_val)).collect())
        }).collect();
        Ok(Value::Array(matrix))
    }

    pub(crate) fn eval_matrix_get(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_matrix_set(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_matrix_rows(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("matrix.rows() requires 1 arg".to_string()); }
        let mat = self.evaluate_expr(&args[0])?;
        if let Value::Array(rows) = mat { Ok(Value::Number(rows.len() as f64)) }
        else { Err("matrix.rows() requires a matrix".to_string()) }
    }

    pub(crate) fn eval_matrix_cols(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("matrix.cols() requires 1 arg".to_string()); }
        let mat = self.evaluate_expr(&args[0])?;
        if let Value::Array(rows) = mat {
            let cols = if let Some(Value::Array(first_row)) = rows.first() { first_row.len() } else { 0 };
            Ok(Value::Number(cols as f64))
        } else { Err("matrix.cols() requires a matrix".to_string()) }
    }

    pub(crate) fn eval_request_security(&mut self, args: &[Expr]) -> Result<Value, String> {
        if args.is_empty() { return Err("request.security() requires at least 1 arg: symbol".to_string()); }
        let symbol = self.resolve_symbol(&args[0])?;
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

    pub(crate) fn eval_alert_fn(&mut self, args: &[Expr]) -> Result<Value, String> {
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

    pub(crate) fn eval_alertcondition(&mut self, args: &[Expr]) -> Result<Value, String> {
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
}
