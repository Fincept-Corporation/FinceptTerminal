use std::fmt;

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    // Literals
    Number(f64),
    StringLit(String),
    Ident(String),

    // Keywords
    If,
    Else,
    For,
    While,
    In,
    Fn,
    Return,
    Break,
    Continue,
    Buy,
    Sell,
    Plot,
    PlotCandlestick,
    PlotLine,
    PlotHistogram,
    PlotShape,
    Bgcolor,
    Hline,
    And,
    Or,
    Not,
    True,
    False,
    Na,
    Switch,
    Strategy,
    Input,
    Struct,
    Import,
    Export,
    Alert,
    Print,
    Request,
    Color,

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Gt,
    Lt,
    Gte,
    Lte,
    EqEq,
    Neq,
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    DotDot,
    Question,

    // Punctuation
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    Assign,
    Dot,
    Colon,

    // Structure
    Newline,
    Comment(String),
    EOF,
}

impl fmt::Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Token::Number(n) => write!(f, "{}", n),
            Token::StringLit(s) => write!(f, "\"{}\"", s),
            Token::Ident(s) => write!(f, "{}", s),
            Token::If => write!(f, "if"),
            Token::Else => write!(f, "else"),
            Token::For => write!(f, "for"),
            Token::While => write!(f, "while"),
            Token::In => write!(f, "in"),
            Token::Fn => write!(f, "fn"),
            Token::Return => write!(f, "return"),
            Token::Break => write!(f, "break"),
            Token::Continue => write!(f, "continue"),
            Token::Buy => write!(f, "buy"),
            Token::Sell => write!(f, "sell"),
            Token::Plot => write!(f, "plot"),
            Token::PlotCandlestick => write!(f, "plot_candlestick"),
            Token::PlotLine => write!(f, "plot_line"),
            Token::PlotHistogram => write!(f, "plot_histogram"),
            Token::PlotShape => write!(f, "plot_shape"),
            Token::Bgcolor => write!(f, "bgcolor"),
            Token::Hline => write!(f, "hline"),
            Token::And => write!(f, "and"),
            Token::Or => write!(f, "or"),
            Token::Not => write!(f, "not"),
            Token::True => write!(f, "true"),
            Token::False => write!(f, "false"),
            Token::Na => write!(f, "na"),
            Token::Switch => write!(f, "switch"),
            Token::Strategy => write!(f, "strategy"),
            Token::Input => write!(f, "input"),
            Token::Struct => write!(f, "struct"),
            Token::Import => write!(f, "import"),
            Token::Export => write!(f, "export"),
            Token::Alert => write!(f, "alert"),
            Token::Print => write!(f, "print"),
            Token::Request => write!(f, "request"),
            Token::Color => write!(f, "color"),
            Token::Plus => write!(f, "+"),
            Token::Minus => write!(f, "-"),
            Token::Star => write!(f, "*"),
            Token::Slash => write!(f, "/"),
            Token::Percent => write!(f, "%"),
            Token::Gt => write!(f, ">"),
            Token::Lt => write!(f, "<"),
            Token::Gte => write!(f, ">="),
            Token::Lte => write!(f, "<="),
            Token::EqEq => write!(f, "=="),
            Token::Neq => write!(f, "!="),
            Token::PlusAssign => write!(f, "+="),
            Token::MinusAssign => write!(f, "-="),
            Token::StarAssign => write!(f, "*="),
            Token::SlashAssign => write!(f, "/="),
            Token::DotDot => write!(f, ".."),
            Token::Question => write!(f, "?"),
            Token::LParen => write!(f, "("),
            Token::RParen => write!(f, ")"),
            Token::LBrace => write!(f, "{{"),
            Token::RBrace => write!(f, "}}"),
            Token::LBracket => write!(f, "["),
            Token::RBracket => write!(f, "]"),
            Token::Comma => write!(f, ","),
            Token::Assign => write!(f, "="),
            Token::Dot => write!(f, "."),
            Token::Colon => write!(f, ":"),
            Token::Newline => write!(f, "\\n"),
            Token::Comment(s) => write!(f, "//{}", s),
            Token::EOF => write!(f, "EOF"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct TokenWithSpan {
    pub token: Token,
    pub line: usize,
    pub col: usize,
}

pub struct Lexer {
    input: Vec<char>,
    pos: usize,
    line: usize,
    col: usize,
}

impl Lexer {
    pub fn new(input: &str) -> Self {
        Lexer {
            input: input.chars().collect(),
            pos: 0,
            line: 1,
            col: 1,
        }
    }

    fn peek(&self) -> Option<char> {
        self.input.get(self.pos).copied()
    }

    fn peek_next(&self) -> Option<char> {
        self.input.get(self.pos + 1).copied()
    }

    fn advance(&mut self) -> Option<char> {
        let ch = self.input.get(self.pos).copied();
        if let Some(c) = ch {
            self.pos += 1;
            if c == '\n' {
                self.line += 1;
                self.col = 1;
            } else {
                self.col += 1;
            }
        }
        ch
    }

    fn skip_whitespace(&mut self) {
        while let Some(ch) = self.peek() {
            if ch == ' ' || ch == '\t' || ch == '\r' {
                self.advance();
            } else {
                break;
            }
        }
    }

    fn read_number(&mut self) -> f64 {
        let mut num_str = String::new();
        let mut has_dot = false;
        while let Some(ch) = self.peek() {
            if ch.is_ascii_digit() {
                num_str.push(ch);
                self.advance();
            } else if ch == '.' && !has_dot {
                // Check if this is a decimal point or the start of '..' (range operator)
                if self.peek_next() == Some('.') {
                    break; // Don't consume - it's the '..' operator
                }
                has_dot = true;
                num_str.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        num_str.parse::<f64>().unwrap_or(0.0)
    }

    fn read_string(&mut self) -> Result<String, LexerError> {
        let start_line = self.line;
        let start_col = self.col;
        self.advance(); // consume opening quote
        let mut s = String::new();
        loop {
            match self.peek() {
                Some('"') => {
                    self.advance();
                    return Ok(s);
                }
                Some('\\') => {
                    self.advance();
                    match self.peek() {
                        Some('n') => { s.push('\n'); self.advance(); }
                        Some('t') => { s.push('\t'); self.advance(); }
                        Some('\\') => { s.push('\\'); self.advance(); }
                        Some('"') => { s.push('"'); self.advance(); }
                        _ => { s.push('\\'); }
                    }
                }
                Some(ch) => {
                    s.push(ch);
                    self.advance();
                }
                None => {
                    return Err(LexerError {
                        message: "Unterminated string literal".to_string(),
                        line: start_line,
                        col: start_col,
                    });
                }
            }
        }
    }

    fn read_identifier(&mut self) -> String {
        let mut ident = String::new();
        while let Some(ch) = self.peek() {
            if ch.is_alphanumeric() || ch == '_' {
                ident.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        ident
    }

    fn read_comment(&mut self) -> String {
        let mut comment = String::new();
        while let Some(ch) = self.peek() {
            if ch == '\n' {
                break;
            }
            comment.push(ch);
            self.advance();
        }
        comment
    }

    fn ident_to_token(ident: &str) -> Token {
        match ident {
            "if" => Token::If,
            "else" => Token::Else,
            "for" => Token::For,
            "while" => Token::While,
            "in" => Token::In,
            "fn" => Token::Fn,
            "return" => Token::Return,
            "break" => Token::Break,
            "continue" => Token::Continue,
            "buy" => Token::Buy,
            "sell" => Token::Sell,
            "plot" => Token::Plot,
            "plot_candlestick" => Token::PlotCandlestick,
            "plot_line" => Token::PlotLine,
            "plot_histogram" => Token::PlotHistogram,
            "plot_shape" => Token::PlotShape,
            "bgcolor" => Token::Bgcolor,
            "hline" => Token::Hline,
            "and" => Token::And,
            "or" => Token::Or,
            "not" => Token::Not,
            "true" => Token::True,
            "false" => Token::False,
            "na" => Token::Na,
            "switch" => Token::Switch,
            "strategy" => Token::Strategy,
            "input" => Token::Input,
            "struct" => Token::Struct,
            "import" => Token::Import,
            "export" => Token::Export,
            "alert" => Token::Alert,
            "print" => Token::Print,
            "request" => Token::Request,
            "color" => Token::Color,
            _ => Token::Ident(ident.to_string()),
        }
    }

    pub fn tokenize(&mut self) -> Result<Vec<TokenWithSpan>, LexerError> {
        let mut tokens = Vec::new();
        let mut last_was_newline = true;

        loop {
            self.skip_whitespace();

            let line = self.line;
            let col = self.col;

            match self.peek() {
                None => {
                    tokens.push(TokenWithSpan { token: Token::EOF, line, col });
                    break;
                }
                Some('\n') => {
                    self.advance();
                    if !last_was_newline {
                        tokens.push(TokenWithSpan { token: Token::Newline, line, col });
                        last_was_newline = true;
                    }
                    continue;
                }
                Some('/') => {
                    if self.peek_next() == Some('/') {
                        self.advance();
                        self.advance();
                        let comment = self.read_comment();
                        tokens.push(TokenWithSpan { token: Token::Comment(comment), line, col });
                        last_was_newline = false;
                        continue;
                    } else {
                        self.advance();
                        if self.peek() == Some('=') {
                            self.advance();
                            tokens.push(TokenWithSpan { token: Token::SlashAssign, line, col });
                        } else {
                            tokens.push(TokenWithSpan { token: Token::Slash, line, col });
                        }
                    }
                }
                Some('"') => {
                    let s = self.read_string()?;
                    tokens.push(TokenWithSpan { token: Token::StringLit(s), line, col });
                }
                Some(ch) if ch.is_ascii_digit() => {
                    let num = self.read_number();
                    tokens.push(TokenWithSpan { token: Token::Number(num), line, col });
                }
                Some(ch) if ch.is_alphabetic() || ch == '_' => {
                    let ident = self.read_identifier();
                    let token = Self::ident_to_token(&ident);
                    tokens.push(TokenWithSpan { token, line, col });
                }
                Some('+') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::PlusAssign, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Plus, line, col });
                    }
                }
                Some('-') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::MinusAssign, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Minus, line, col });
                    }
                }
                Some('*') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::StarAssign, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Star, line, col });
                    }
                }
                Some('?') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::Question, line, col });
                }
                Some('%') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::Percent, line, col });
                }
                Some('>') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::Gte, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Gt, line, col });
                    }
                }
                Some('<') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::Lte, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Lt, line, col });
                    }
                }
                Some('=') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::EqEq, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Assign, line, col });
                    }
                }
                Some('!') => {
                    self.advance();
                    if self.peek() == Some('=') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::Neq, line, col });
                    } else {
                        return Err(LexerError {
                            message: "Unexpected character '!' (did you mean '!='?)".to_string(),
                            line, col,
                        });
                    }
                }
                Some('.') => {
                    self.advance();
                    if self.peek() == Some('.') {
                        self.advance();
                        tokens.push(TokenWithSpan { token: Token::DotDot, line, col });
                    } else {
                        tokens.push(TokenWithSpan { token: Token::Dot, line, col });
                    }
                }
                Some(':') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::Colon, line, col });
                }
                Some('(') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::LParen, line, col });
                }
                Some(')') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::RParen, line, col });
                }
                Some('{') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::LBrace, line, col });
                }
                Some('}') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::RBrace, line, col });
                }
                Some('[') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::LBracket, line, col });
                }
                Some(']') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::RBracket, line, col });
                }
                Some(',') => {
                    self.advance();
                    tokens.push(TokenWithSpan { token: Token::Comma, line, col });
                }
                Some(ch) => {
                    return Err(LexerError {
                        message: format!("Unexpected character '{}'", ch),
                        line, col,
                    });
                }
            }
            last_was_newline = false;
        }

        Ok(tokens)
    }
}

#[derive(Debug, Clone)]
pub struct LexerError {
    pub message: String,
    pub line: usize,
    pub col: usize,
}

impl fmt::Display for LexerError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Line {}, Col {}: {}", self.line, self.col, self.message)
    }
}

pub fn tokenize(input: &str) -> Result<Vec<TokenWithSpan>, LexerError> {
    let mut lexer = Lexer::new(input);
    lexer.tokenize()
}
