use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OhlcvSeries {
    pub symbol: String,
    pub timestamps: Vec<i64>,
    pub open: Vec<f64>,
    pub high: Vec<f64>,
    pub low: Vec<f64>,
    pub close: Vec<f64>,
    pub volume: Vec<f64>,
}

#[derive(Debug, Clone)]
pub struct ColorValue {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8, // 0=fully transparent, 255=fully opaque
}

impl ColorValue {
    pub fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        ColorValue { r, g, b, a }
    }
    pub fn to_hex(&self) -> String {
        if self.a == 255 {
            format!("#{:02x}{:02x}{:02x}", self.r, self.g, self.b)
        } else {
            format!("rgba({},{},{},{:.2})", self.r, self.g, self.b, self.a as f64 / 255.0)
        }
    }
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "red" => Some(ColorValue::new(255, 0, 0, 255)),
            "green" => Some(ColorValue::new(0, 128, 0, 255)),
            "blue" => Some(ColorValue::new(0, 0, 255, 255)),
            "white" => Some(ColorValue::new(255, 255, 255, 255)),
            "black" => Some(ColorValue::new(0, 0, 0, 255)),
            "yellow" => Some(ColorValue::new(255, 255, 0, 255)),
            "orange" => Some(ColorValue::new(255, 165, 0, 255)),
            "purple" => Some(ColorValue::new(128, 0, 128, 255)),
            "aqua" | "cyan" => Some(ColorValue::new(0, 255, 255, 255)),
            "lime" => Some(ColorValue::new(0, 255, 0, 255)),
            "fuchsia" | "magenta" => Some(ColorValue::new(255, 0, 255, 255)),
            "silver" => Some(ColorValue::new(192, 192, 192, 255)),
            "gray" | "grey" => Some(ColorValue::new(128, 128, 128, 255)),
            "maroon" => Some(ColorValue::new(128, 0, 0, 255)),
            "olive" => Some(ColorValue::new(128, 128, 0, 255)),
            "teal" => Some(ColorValue::new(0, 128, 128, 255)),
            "navy" => Some(ColorValue::new(0, 0, 128, 255)),
            _ => None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct DrawingValue {
    pub drawing_type: String,  // "line", "label", "box"
    pub x1: i64,
    pub y1: f64,
    pub x2: i64,
    pub y2: f64,
    pub color: String,
    pub text: String,
    pub style: String,
    pub width: f64,
}

#[derive(Debug, Clone)]
pub struct TableValue {
    pub id: String,
    pub rows: usize,
    pub cols: usize,
    pub cells: Vec<Vec<TableCell>>,
    pub position: String,  // "top_left", "top_right", "bottom_left", "bottom_right"
}

#[derive(Debug, Clone)]
pub struct TableCell {
    pub text: String,
    pub bg_color: String,
    pub text_color: String,
}

#[derive(Debug, Clone)]
pub enum Value {
    Number(f64),
    Series(SeriesData),
    String(String),
    Bool(bool),
    Array(Vec<Value>),
    Map(HashMap<String, Value>),
    Color(ColorValue),
    Struct { type_name: String, fields: HashMap<String, Value> },
    Drawing(DrawingValue),
    Table(TableValue),
    Na,  // PineScript-style na (NaN/missing value)
    Void,
}

#[derive(Debug, Clone)]
pub struct SeriesData {
    pub values: Vec<f64>,
    pub timestamps: Vec<i64>,
}

impl Value {
    pub fn is_truthy(&self) -> bool {
        match self {
            Value::Bool(b) => *b,
            Value::Number(n) => *n != 0.0 && !n.is_nan(),
            Value::String(s) => !s.is_empty(),
            Value::Series(s) => !s.values.is_empty(),
            Value::Array(a) => !a.is_empty(),
            Value::Map(m) => !m.is_empty(),
            Value::Color(_) => true,
            Value::Struct { .. } => true,
            Value::Drawing(_) => true,
            Value::Table(_) => true,
            Value::Na => false,
            Value::Void => false,
        }
    }

    pub fn is_na(&self) -> bool {
        matches!(self, Value::Na) || matches!(self, Value::Number(n) if n.is_nan())
    }

    pub fn as_number(&self) -> Option<f64> {
        match self {
            Value::Number(n) => Some(*n),
            Value::Bool(b) => Some(if *b { 1.0 } else { 0.0 }),
            Value::Na => Some(f64::NAN),
            _ => None,
        }
    }

    pub fn as_series(&self) -> Option<&SeriesData> {
        match self {
            Value::Series(s) => Some(s),
            _ => None,
        }
    }

    pub fn as_string(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s),
            _ => None,
        }
    }

    pub fn type_name(&self) -> &'static str {
        match self {
            Value::Number(_) => "Number",
            Value::Series(_) => "Series",
            Value::String(_) => "String",
            Value::Bool(_) => "Bool",
            Value::Array(_) => "Array",
            Value::Map(_) => "Map",
            Value::Color(_) => "Color",
            Value::Struct { .. } => "Struct",
            Value::Drawing(_) => "Drawing",
            Value::Table(_) => "Table",
            Value::Na => "Na",
            Value::Void => "Void",
        }
    }
}

pub type SymbolDataMap = HashMap<String, OhlcvSeries>;
