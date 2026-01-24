use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Gt,
    Lt,
    Gte,
    Lte,
    Eq,
    Neq,
    And,
    Or,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum UnaryOp {
    Not,
    Neg,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Expr {
    Number(f64),
    Bool(bool),
    StringLiteral(String),
    Symbol(String),
    Variable(String),
    Na,  // na value (NaN equivalent)
    BinaryOp {
        left: Box<Expr>,
        op: BinOp,
        right: Box<Expr>,
    },
    UnaryOp {
        op: UnaryOp,
        operand: Box<Expr>,
    },
    FunctionCall {
        name: String,
        args: Vec<Expr>,
    },
    // Method call: expr.method(args)
    MethodCall {
        object: Box<Expr>,
        method: String,
        args: Vec<Expr>,
    },
    // Array literal: [expr, expr, ...]
    ArrayLiteral(Vec<Expr>),
    // Index access: expr[expr] — also used for history reference: close[1]
    IndexAccess {
        object: Box<Expr>,
        index: Box<Expr>,
    },
    // Range: start..end
    Range {
        start: Box<Expr>,
        end: Box<Expr>,
    },
    // Ternary: condition ? then_expr : else_expr
    Ternary {
        condition: Box<Expr>,
        then_expr: Box<Expr>,
        else_expr: Box<Expr>,
    },
    // Map literal: { "key": value, ... }
    MapLiteral(Vec<(String, Expr)>),
    // Struct instantiation: TypeName { field: value, ... }
    StructLiteral {
        type_name: String,
        fields: Vec<(String, Expr)>,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Statement {
    Assignment {
        name: String,
        value: Expr,
    },
    CompoundAssign {
        name: String,
        op: BinOp,  // Add or Sub (+=, -=)
        value: Expr,
    },
    IndexAssign {
        object: String,
        index: Expr,
        value: Expr,
    },
    IfBlock {
        condition: Expr,
        body: Vec<Statement>,
        else_if_blocks: Vec<(Expr, Vec<Statement>)>,
        else_body: Option<Vec<Statement>>,
    },
    ForLoop {
        var_name: String,
        iterable: Expr,
        body: Vec<Statement>,
    },
    WhileLoop {
        condition: Expr,
        body: Vec<Statement>,
    },
    FnDef {
        name: String,
        params: Vec<String>,
        body: Vec<Statement>,
    },
    Return {
        value: Option<Expr>,
    },
    Break,
    Continue,
    Buy {
        message: Expr,
    },
    Sell {
        message: Expr,
    },
    Plot {
        expr: Expr,
        label: Expr,
    },
    PlotCandlestick {
        symbol: Expr,
        title: Expr,
    },
    PlotLine {
        value: Expr,
        label: Expr,
        color: Option<Expr>,
    },
    PlotHistogram {
        value: Expr,
        label: Expr,
        color_up: Option<Expr>,
        color_down: Option<Expr>,
    },
    PlotShape {
        condition: Expr,
        shape: Expr,       // "triangleup", "triangledown", "circle", "cross", "diamond"
        location: Expr,    // "abovebar", "belowbar", "absolute"
        color: Option<Expr>,
        text: Option<Expr>,
    },
    Bgcolor {
        color: Expr,
        condition: Option<Expr>,
    },
    Hline {
        value: Expr,
        label: Option<Expr>,
        color: Option<Expr>,
    },
    // Switch statement
    SwitchBlock {
        expr: Option<Expr>,  // optional switch expression
        cases: Vec<(Expr, Vec<Statement>)>,
        default: Option<Vec<Statement>>,
    },
    // Strategy commands
    StrategyEntry {
        id: Expr,
        direction: Expr,   // "long" or "short"
        qty: Option<Expr>,
        price: Option<Expr>,
        stop: Option<Expr>,
        limit: Option<Expr>,
    },
    StrategyExit {
        id: Expr,
        from_entry: Option<Expr>,
        qty: Option<Expr>,
        stop: Option<Expr>,
        limit: Option<Expr>,
        trail_points: Option<Expr>,
        trail_offset: Option<Expr>,
    },
    StrategyClose {
        id: Expr,
    },
    // Input declaration
    InputDecl {
        name: String,
        input_type: String,  // "int", "float", "bool", "string"
        default_value: Expr,
        title: Option<Expr>,
    },
    // Struct definition
    StructDef {
        name: String,
        fields: Vec<(String, String)>,  // (field_name, type_hint)
    },
    // Alert statement
    AlertStatement {
        message: Expr,
        alert_type: Option<Expr>,  // "once", "every_bar", etc.
    },
    // Import statement
    ImportStatement {
        module: String,
        alias: Option<String>,
    },
    // Export statement (marks a function/variable as exported)
    ExportStatement {
        name: String,
    },
    // Print statement: print expr, expr, ...
    PrintStatement {
        args: Vec<Expr>,
    },
    // Expression statement (for bare function calls)
    ExprStatement(Expr),
    Comment(String),
}

pub type Program = Vec<Statement>;
