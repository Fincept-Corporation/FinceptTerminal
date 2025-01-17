use yahoo_finance_api as yahoo;
use std::collections::HashMap;
use std::pin::Pin;
use std::future::Future;
use std::fs::File;
use std::io::{self, Read};
use std::env;
use std::fmt;



// Custom Display implementation for ASTNode
impl fmt::Display for ASTNode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ASTNode::Assignment { variable, value } => {
                write!(f, "{} = {}", variable, value)
            }
            ASTNode::FunctionCall { name, args } => {
                let arg_str: Vec<String> = args.iter().map(|arg| format!("{}", arg)).collect();
                write!(f, "{}({})", name, arg_str.join(", "))
            }
            ASTNode::Conditional { condition, true_block, .. } => {
                write!(f, "if {} then", condition)?;
                for node in true_block {
                    write!(f, "\n  {}", node)?;
                }
                Ok(())
            }
            ASTNode::BuySignal(msg) => write!(f, "Buy Signal: {}", msg),
            ASTNode::SellSignal(msg) => write!(f, "Sell Signal: {}", msg),
            ASTNode::Plot { variable, label } => write!(f, "Plot {}: {}", variable, label),
            ASTNode::Variable(name) => write!(f, "{}", name),
            ASTNode::Number(value) => write!(f, "{}", value), // Replace Literal with Number here
        }
    }
}


#[allow(dead_code)]  // Add this to suppress the warnings about unused variants
#[derive(Debug)]
enum ScriptError {
    ParseError(String),
    RuntimeError(String),
    UndefinedVariable(String),
    InvalidFunctionCall(String),
}

// Interpreter for the scripting language
struct Interpreter {
    variables: HashMap<String, f64>,   // Variable store
    price_cache: HashMap<String, Vec<f64>>, // Cached prices
}

impl Interpreter {
    // Interpreter's variables and price cache
    fn new() -> Self {
        Interpreter {
            variables: HashMap::new(),
            price_cache: HashMap::new(),
        }
    }

    // Fetch stock data from Yahoo Finance API
    async fn fetch_yahoo_finance_data(&self, symbol: &str) -> Result<Vec<f64>, ScriptError> {
        let provider = yahoo::YahooConnector::new().map_err(|e| ScriptError::RuntimeError(format!("Failed to create YahooConnector: {}", e)))?;
        let response = provider.get_latest_quotes(symbol, "1d").await.map_err(|e| ScriptError::RuntimeError(e.to_string()))?;
        let close_prices: Vec<f64> = response
            .quotes()
            .map_err(|e| ScriptError::RuntimeError(e.to_string()))?
            .iter()
            .map(|q| q.close)
            .collect();
        if close_prices.is_empty() {
            return Err(ScriptError::RuntimeError(format!("No valid data found for ticker: {}", symbol)));
        }
        Ok(close_prices)
    }

    // Get prices with caching
    async fn get_prices(&mut self, symbol: &str) -> Vec<f64> {
        if let Some(cached_prices) = self.price_cache.get(symbol) {
            return cached_prices.clone();
        }
        match self.fetch_yahoo_finance_data(symbol).await {
            Ok(prices) => {
                self.price_cache.insert(symbol.to_string(), prices.clone());
                prices
            }
            Err(e) => {
                println!("Error fetching data for {}: {:?}. Using default data.", symbol, e);
                vec![100.0, 102.0, 104.0, 106.0, 108.0]
            }
        }
    }

    // Calculate SMA
    fn calculate_sma(&self, prices: &[f64], period: usize) -> f64 {
        if prices.len() < period {
            return 0.0;
        }
        prices[prices.len() - period..].iter().sum::<f64>() / period as f64
    }

    // Calculate EMA
    fn calculate_ema(&self, prices: &[f64], period: usize) -> f64 {
        let mut ema = prices[0]; // Start with the first price as the initial EMA
        let multiplier = 2.0 / (period as f64 + 1.0);
        for &price in &prices[1..] {
            ema = (price - ema) * multiplier + ema;
        }
        ema
    }

    // Calculate WMA
    fn calculate_wma(&self, prices: &[f64], period: usize) -> f64 {
        if prices.len() < period {
            return 0.0;
        }
        let mut denominator = 0.0;
        let mut numerator = 0.0;
        for i in 0..period {
            let weight = (i + 1) as f64;
            numerator += prices[prices.len() - period + i] * weight;
            denominator += weight;
        }
        numerator / denominator
    }

    // Evaluate AST nodes asynchronously
    fn eval<'a>(&'a mut self, node: ASTNode) -> Pin<Box<dyn Future<Output = Option<f64>> + 'a>> {
        Box::pin(async move {
            match node {
                ASTNode::CandlestickChart { symbol, label } => {
                    // Fetch stock data (open, high, low, close)
                    let provider = yahoo::YahooConnector::new().unwrap();
                    let response = provider.get_latest_quotes(&symbol, "1d").await.unwrap();
                    let quotes = response.quotes().unwrap();

                    let ohlc: Vec<_> = quotes.iter().map(|q| {
                        (q.timestamp, q.open, q.high, q.low, q.close)
                    }).collect();

                    // Plot candlestick chart
                    plot_candlestick_chart(&symbol, &label, &ohlc);
                    None
                }
                ASTNode::Assignment { variable, value } => {
                    if let Some(val) = self.eval(*value).await {
                        println!("Storing variable: {} = {}", variable, val);
                        self.variables.insert(variable.clone(), val);
                    }
                    None
                }
                ASTNode::FunctionCall { name, args } => {
                    // Handle moving averages dynamically (SMA, EMA, WMA)
                    if name == "sma" || name == "ema" || name == "wma" {
                        if let Some(ASTNode::Variable(symbol)) = args.get(0) {
                            if let Some(ASTNode::Number(period)) = args.get(1) {
                                let prices = self.get_prices(symbol).await;
                                let value = match name.as_str() {
                                    "sma" => self.calculate_sma(&prices, *period as usize),
                                    "ema" => self.calculate_ema(&prices, *period as usize),
                                    "wma" => self.calculate_wma(&prices, *period as usize),
                                    _ => return None,
                                };
                                println!("{} for {} calculated at {:.2}", name.to_uppercase(), symbol, value);
                                return Some(value);
                            }
                        }
                    }

                    // Handle comparison operator `>` dynamically
                    if name == ">" {
                        let left = self.eval(args[0].clone()).await;
                        let right = self.eval(args[1].clone()).await;

                        if let (Some(left_val), Some(right_val)) = (left, right) {
                            println!("Condition met: {} > {} (Triggered at price: {:.2})", args[0], args[1], left_val);
                            if left_val > right_val {
                                return Some(1.0);  // True
                            }
                        }
                        return Some(0.0);  // False
                    }

                    None
                }
                ASTNode::Conditional { condition, true_block, false_block } => {
                    if let Some(result) = self.eval(*condition).await {
                        //println!("Condition evaluated to: {}", result);
                        if result > 0.0 {
                            //println!("Condition is true, executing true block.");
                            for node in true_block {
                                self.eval(node).await;
                            }
                        } else if let Some(false_block) = false_block {
                            //println!("Condition is false, executing false block.");
                            for node in false_block {
                                self.eval(node).await;
                            }
                        } else {
                            println!("Condition is false, no false block to execute.");
                        }
                    }
                    None
                }
                ASTNode::BuySignal(msg) => {
                    if let Some(price) = self.variables.get("sma_value") {

                        println!("Buy Signal: {} triggered at price: {:.2}", msg, price);
                    } else {
                        println!("Buy Signal: {}", msg);
                    }
                    None
                }
                ASTNode::SellSignal(msg) => {
                    if let Some(price) = self.variables.get("sma_value") {
                        println!("Sell Signal: {} triggered at price: {:.2}", msg, price);
                    } else {
                        println!("Sell Signal: {}", msg);
                    }
                    None
                }

                ASTNode::Plot { variable, label } => {
                    if let Some(value) = self.variables.get(&variable) {
                        println!("Plotting {}: {:.2}", label, value);
                    }
                    None
                }
                ASTNode::Variable(name) => self.variables.get(&name).cloned(),
                ASTNode::Number(val) => Some(val),
            }
        })
    }


    // Interpret the entire AST asynchronously
    async fn interpret(&mut self, ast: Vec<ASTNode>) {
        for node in ast {
            self.eval(node).await;
        }
    }
}

// Function to plot candlestick chart using plotters
fn plot_candlestick_chart(symbol: &str, label: &str, ohlc_data: &[(i64, f64, f64, f64, f64)]) {
    use plotters::prelude::*;

    let root = BitMapBackend::new("candlestick_chart.png", (640, 480)).into_drawing_area();
    root.fill(&WHITE).unwrap();

    let max_price = ohlc_data.iter().map(|&(_, _, high, _, _)| high).fold(f64::MIN, f64::max);
    let min_price = ohlc_data.iter().map(|&(_, _, _, low, _)| low).fold(f64::MAX, f64::min);

    let mut chart = ChartBuilder::on(&root)
        .caption(label, ("sans-serif", 40))
        .x_label_area_size(35)
        .y_label_area_size(40)
        .build_cartesian_2d(0..ohlc_data.len(), min_price..max_price)
        .unwrap();

    chart.configure_mesh().draw().unwrap();

    chart.draw_series(ohlc_data.iter().enumerate().map(|(i, &(timestamp, open, high, low, close))| {
        CandleStick::new(
            i, open, close, high, low,
            RGBColor(0, 0, 0).stroke_width(1),
            if close >= open { GREEN.filled() } else { RED.filled() }
        )
    })).unwrap();

    root.present().unwrap();
}


// Updated Abstract Syntax Tree (AST) Nodes
#[derive(Debug, Clone, PartialEq)]
enum ASTNode {
    Assignment { variable: String, value: Box<ASTNode> },
    FunctionCall { name: String, args: Vec<ASTNode> },
    Conditional { condition: Box<ASTNode>, true_block: Vec<ASTNode>, false_block: Option<Vec<ASTNode>> },
    BuySignal(String),
    SellSignal(String),
    Plot { variable: String, label: String },
    CandlestickChart { symbol: String, label: String },  // New variant for candlestick charting
    Variable(String),
    Number(f64),
}


// Updated lexer logic for numbers
fn lex(input: &str) -> Vec<Token> {
    let mut tokens = Vec::new();
    let mut chars = input.chars().peekable();

    while let Some(&ch) = chars.peek() {
        match ch {
            '=' => {
                tokens.push(Token::Assign);
                chars.next();
            }
            '(' => {
                tokens.push(Token::OpenParen);
                chars.next();
            }
            ')' => {
                tokens.push(Token::CloseParen);
                chars.next();
            }
            '{' => {
                tokens.push(Token::OpenBrace);
                chars.next();
            }
            '}' => {
                tokens.push(Token::CloseBrace);
                chars.next();
            }
            ',' => {
                tokens.push(Token::Comma);
                chars.next();
            }
            '>' | '<' => {
                tokens.push(Token::Operator(ch.to_string()));
                chars.next();
            }
            '"' => {
                chars.next(); // Consume opening quote
                let mut string = String::new();
                while let Some(&c) = chars.peek() {
                    if c == '"' {
                        break;
                    }
                    string.push(c);
                    chars.next();
                }
                tokens.push(Token::StringLiteral(string));
                chars.next(); // Consume closing quote
            }
            _ if ch.is_digit(10) || ch == '.' => {
                let mut number = String::new();
                while let Some(&c) = chars.peek() {
                    if c.is_digit(10) || c == '.' {
                        number.push(c);
                        chars.next();
                    } else {
                        break;
                    }
                }

                match number.parse::<f64>() {
                    Ok(num) => tokens.push(Token::Number(num)),
                    Err(_) => {
                        println!("Error: Invalid number format '{}'", number);
                        return tokens;
                    }
                }
            }
            _ if ch.is_alphabetic() => {
                let mut identifier = String::new();
                while let Some(&c) = chars.peek() {
                    if c.is_alphabetic() || c == '.' {
                        identifier.push(c);
                        chars.next();
                    } else {
                        break;
                    }
                }

                match identifier.as_str() {
                    "if" | "buy" | "sell" | "plot" => tokens.push(Token::Keyword(identifier)),
                    _ => tokens.push(Token::Identifier(identifier)),
                }
            }
            _ => {
                chars.next(); // Skip unrecognized characters
            }
        }
    }

    tokens
}

fn parse(tokens: Vec<Token>) -> Vec<ASTNode> {
    let mut nodes = Vec::new();
    let mut iter = tokens.into_iter().peekable();

        while let Some(token) = iter.next() {
        match token {
            Token::Keyword(ref keyword) if keyword == "plot_candlestick" => {
                if let Some(Token::Identifier(symbol)) = iter.next() {
                    if let Some(Token::StringLiteral(label)) = iter.next() {
                        nodes.push(ASTNode::CandlestickChart {
                            symbol,
                            label,
                        });
                    }
                }
            }

    while let Some(token) = iter.next() {
        match token {
            Token::Keyword(ref keyword) if keyword == "if" => {
                // Parse condition where function calls are compared
                let left_expr = parse_expression(&mut iter); // Parse the left-hand side (e.g., ema(...))
                if let Some(Token::Operator(op)) = iter.next() {
                    let right_expr = parse_expression(&mut iter); // Parse the right-hand side (e.g., wma(...))
                    let condition = ASTNode::FunctionCall {
                        name: op,
                        args: vec![left_expr, right_expr],
                    };
                    let mut true_block = Vec::new();
                    if let Some(Token::OpenBrace) = iter.next() {
                        while let Some(token) = iter.next() {
                            match token {
                                Token::Keyword(ref kw) if kw == "buy" => {
                                    if let Some(Token::StringLiteral(msg)) = iter.next() {
                                        true_block.push(ASTNode::BuySignal(msg));
                                    }
                                }
                                Token::CloseBrace => break,
                                _ => {}
                            }
                        }
                    }
                    nodes.push(ASTNode::Conditional {
                        condition: Box::new(condition),
                        true_block,
                        false_block: None,
                    });
                }
            }
            Token::Keyword(ref keyword) if keyword == "plot" => {
                if let Some(Token::Identifier(var_part1)) = iter.next() {
                    if let Some(Token::Identifier(var_part2)) = iter.peek() {
                        let variable = format!("{}_{}", var_part1, var_part2);
                        iter.next();
                        if let Some(Token::StringLiteral(label)) = iter.next() {
                            nodes.push(ASTNode::Plot {
                                variable,
                                label,
                            });
                        }
                    }
                }
            }
            _ => {}
        }
    }
    nodes
}

// Helper function to parse expressions like `ema(...)` or `wma(...)`
fn parse_expression(iter: &mut std::iter::Peekable<std::vec::IntoIter<Token>>) -> ASTNode {
    if let Some(Token::Identifier(func_name)) = iter.next() {
        let mut args = Vec::new();
        if let Some(Token::OpenParen) = iter.next() {
            while let Some(arg_token) = iter.next() {
                match arg_token {
                    Token::Identifier(arg) => args.push(ASTNode::Variable(arg)),
                    Token::Number(num) => args.push(ASTNode::Number(num)),
                    Token::CloseParen => break,
                    _ => {}
                }
            }
        }
        return ASTNode::FunctionCall { name: func_name, args };
    }
    panic!("Expected function call");
}


// Token representation
#[derive(Debug, PartialEq)]
enum Token {
    Identifier(String),
    Number(f64),
    StringLiteral(String),
    Assign,
    Operator(String),
    Keyword(String),
    OpenParen,
    CloseParen,
    OpenBrace,
    CloseBrace,
    Comma,
}



// Main execution
#[tokio::main]
async fn main() -> io::Result<()> {
    let args: Vec<String> = env::args().collect();

    // Check if the first argument is 'run'
    if args.len() < 3 || args[1] != "run" {
        eprintln!("Usage: finscript run <script.fincept>");
        return Ok(());
    }

    let file_path = &args[2];

    // Read the file contents
    let mut file = File::open(file_path)?;
    let mut script = String::new();
    file.read_to_string(&mut script)?;

    println!("Executing script:\n{}", script);

    // Tokenize, parse, and interpret the script
    let tokens = lex(&script);

    // No token printing here

    let ast = parse(tokens);

    // No AST printing here

    let mut interpreter = Interpreter::new();
    interpreter.interpret(ast).await;

    Ok(())
}
