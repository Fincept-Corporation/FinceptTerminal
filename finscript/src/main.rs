use yahoo_finance_api as yahoo;
use std::collections::HashMap;
use std::pin::Pin;
use std::future::Future;

struct Interpreter {
    variables: HashMap<String, f64>,  // Symbol table to store variables
}

impl Interpreter {
    fn new() -> Self {
        Interpreter {
            variables: HashMap::new(),
        }
    }

    // Fetch latest stock data using Yahoo Finance API
async fn fetch_yahoo_finance_data(&self, symbol: &str) -> Result<Vec<f64>, Box<dyn std::error::Error>> {
    let provider = yahoo::YahooConnector::new()
        .map_err(|e| format!("Failed to create YahooConnector: {}", e))?;

    // Fetch quotes for the given ticker
    let response = provider.get_latest_quotes(symbol, "1d").await?;

    // Extract the closing prices from the response
    let close_prices: Vec<f64> = response
        .quotes()?
        .iter()
        .map(|q| q.close) // No need to unwrap, just map directly to the close field
        .collect();

    // Handle case if no valid quotes are found
    if close_prices.is_empty() {
        Err(format!("No valid data found for ticker: {}", symbol).into())
    } else {
        Ok(close_prices)
    }
}



    // Get prices (uses the Yahoo Finance API now)
async fn get_prices(&self, symbol: &str) -> Vec<f64> {
    match self.fetch_yahoo_finance_data(symbol).await {
        Ok(prices) => {
            if prices.is_empty() {
                println!("No valid data returned from Yahoo Finance for {}. Using default data.", symbol);
                vec![100.0, 102.0, 104.0, 106.0, 108.0] // Default fallback data
            } else {
                prices
            }
        }
        Err(e) => {
            println!("Error fetching data for {}: {}. Using default data.", symbol, e);
            vec![100.0, 102.0, 104.0, 106.0, 108.0, 110.0, 112.0, 114.0, 116.0, 118.0, 120.0, 122.0, 124.0, 126.0]  // Default data
        }
    }
}


    // SMA Calculation
    fn calculate_sma(&self, prices: &[f64], period: usize) -> f64 {
        let len = prices.len();
        if len < period {
            return 0.0; // Not enough data
        }
        let sum: f64 = prices[len - period..].iter().sum();
        sum / period as f64
    }

    // Evaluate AST Nodes (boxed for async recursion)
    fn eval<'a>(&'a mut self, node: ASTNode) -> Pin<Box<dyn Future<Output = Option<f64>> + 'a>> {
        Box::pin(async move {
            match node {
                ASTNode::Assignment { variable, value } => {
                    if let Some(val) = self.eval(*value).await {
                        self.variables.insert(variable, val);  // Store variable value
                    }
                    None
                }
                ASTNode::FunctionCall { name, args } => {
                    // Simulate SMA calculation with Yahoo Finance data
                    if name == "sma" {
                        if let Some(ASTNode::Variable(symbol)) = args.get(0) {
                            if let Some(ASTNode::Literal(period)) = args.get(1) {
                                let prices = self.get_prices(symbol).await;
                                let sma = self.calculate_sma(&prices, *period as usize);
                                return Some(sma);
                            }
                        }
                    }
                    // Handle other functions (like comparison ">" in if-else)
                    if name == ">" {
                        let left = self.eval(args[0].clone()).await;
                        let right = self.eval(args[1].clone()).await;
                        if left.is_some() && right.is_some() && left.unwrap() > right.unwrap() {
                            return Some(1.0);  // True condition
                        } else {
                            return Some(0.0);  // False condition
                        }
                    }
                    None
                }
                ASTNode::Conditional { condition, true_block } => {
                    // Evaluate condition and execute true block if condition is true
                    if let Some(result) = self.eval(*condition).await {
                        if result > 0.0 {
                            for node in true_block {
                                self.eval(node).await;
                            }
                        }
                    }
                    None
                }
                ASTNode::SellSignal(msg) => {
    if let Some(price) = self.variables.get("sma_value") {
        println!("Executing Sell: {} at price: {}", msg, price);
    } else {
        println!("Executing Sell: {}", msg);
    }
    None
},
                ASTNode::BuySignal(msg) => {
    if let Some(price) = self.variables.get("sma_value") {
        println!("Executing Buy: {} at price: {}", msg, price);
    } else {
        println!("Executing Buy: {}", msg);
    }
    None
},


                ASTNode::Plot { variable, label } => {
                    if let Some(value) = self.variables.get(&variable) {
                        println!("Plotting {}: {}", label, value);
                    }
                    None
                }
                ASTNode::Variable(name) => self.variables.get(&name).cloned(),
                ASTNode::Literal(value) => Some(value),
            }
        })
    }

    // Interpret the AST (async)
    async fn interpret(&mut self, ast: Vec<ASTNode>) {
        for node in ast {
            self.eval(node).await;  // Evaluate each node asynchronously
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
enum ASTNode {
    Assignment { variable: String, value: Box<ASTNode> },
    FunctionCall { name: String, args: Vec<ASTNode> },
    Conditional { condition: Box<ASTNode>, true_block: Vec<ASTNode> },
    BuySignal(String),
    SellSignal(String),
    Plot { variable: String, label: String },
    Variable(String),
    Literal(f64),
}

// Lexer and Parser logic remain the same
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

                // Gracefully handle invalid number parsing
                match number.parse::<f64>() {
                    Ok(num) => tokens.push(Token::Number(num)),
                    Err(_) => {
                        println!("Error: Invalid number format '{}'", number);
                        return tokens; // Stop lexing when an invalid number is encountered
                    }
                }
            }
            _ if ch.is_alphabetic() => {
                let mut identifier = String::new();
                while let Some(&c) = chars.peek() {
                    if c.is_alphabetic() || c == '.' { // Handle ticker symbols with periods
                        identifier.push(c);
                        chars.next();
                    } else {
                        break;
                    }
                }

                // Handle keywords
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
            Token::Identifier(variable_part1) => {
                // Handle multi-part identifiers like "sma_value"
                if let Some(Token::Identifier(variable_part2)) = iter.peek() {
                    let variable = format!("{}_{}", variable_part1, variable_part2);
                    iter.next(); // Consume the second part

                    if let Some(Token::Assign) = iter.next() {
                        let value = match iter.next() {
                            Some(Token::Identifier(func_name)) if func_name == "sma" => {
                                let mut args = Vec::new();
                                if let Some(Token::OpenParen) = iter.next() {
                                    while let Some(arg_token) = iter.next() {
                                        match arg_token {
                                            Token::Identifier(arg) => args.push(ASTNode::Variable(arg)),
                                            Token::Number(num) => args.push(ASTNode::Literal(num)),
                                            Token::CloseParen => break,
                                            _ => {}
                                        }
                                    }
                                }
                                ASTNode::FunctionCall { name: func_name, args }
                            }
                            _ => continue,
                        };

                        nodes.push(ASTNode::Assignment {
                            variable: variable.clone(),
                            value: Box::new(value),
                        });
                    }
                }
            }
            Token::Keyword(ref keyword) if keyword == "if" => {
                // Parse condition in 'if sma_value > 50'
                let condition = match iter.next() {
                    Some(Token::Identifier(var_part1)) => {
                        if let Some(Token::Identifier(var_part2)) = iter.peek() {
                            let variable = format!("{}_{}", var_part1, var_part2);
                            iter.next(); // Consume second part of variable

                            if let Some(Token::Operator(op)) = iter.next() {
                                if let Some(Token::Number(num)) = iter.next() {
                                    ASTNode::FunctionCall {
                                        name: op,
                                        args: vec![ASTNode::Variable(variable), ASTNode::Literal(num)],
                                    }
                                } else {
                                    continue;
                                }
                            } else {
                                continue;
                            }
                        } else {
                            continue;
                        }
                    }
                    _ => continue,
                };

                // Parse the true block (inside '{ }')
                let mut true_block = Vec::new();
                if let Some(Token::OpenBrace) = iter.next() {
                    while let Some(token) = iter.next() {
                        match token {
                            Token::Keyword(ref kw) if kw == "buy" => {
                                if let Some(Token::StringLiteral(msg)) = iter.next() {
                                    true_block.push(ASTNode::BuySignal(msg));
                                }
                            }
                            Token::Keyword(ref kw) if kw == "sell" => {
                                if let Some(Token::StringLiteral(msg)) = iter.next() {
                                    true_block.push(ASTNode::SellSignal(msg));
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
                });
            }
            Token::Keyword(ref keyword) if keyword == "plot" => {
                // Parse plot (e.g., plot sma_value, "SMA (14)")
                if let Some(Token::Identifier(var_part1)) = iter.next() {
                    if let Some(Token::Identifier(var_part2)) = iter.peek() {
                        let variable = format!("{}_{}", var_part1, var_part2);
                        iter.next(); // Consume second part of variable

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

#[tokio::main]
async fn main() {
    let script = r#"
        sma_value = sma(RITES.NS, 14)
        sma_value = sma(SUZLON.NS, 14)
        if sma_value > 50 {
            buy "SMA Crossover Buy"
        }
    "#;

    let tokens = lex(script);
    println!("Tokens: {:?}", tokens); // Debugging: Print tokens

    let ast = parse(tokens);
    println!("AST: {:#?}", ast);  // Debugging: Print the AST

    let mut interpreter = Interpreter::new();
    interpreter.interpret(ast).await;  // Interpret the AST asynchronously
}
