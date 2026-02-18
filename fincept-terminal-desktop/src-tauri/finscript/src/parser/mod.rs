mod statements;
mod expressions;
mod plot_strategy;

use crate::ast::*;
use crate::lexer::{Token, TokenWithSpan};
use std::fmt;

#[derive(Debug, Clone)]
pub struct ParseError {
    pub message: String,
    pub line: usize,
    pub col: usize,
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Line {}, Col {}: {}", self.line, self.col, self.message)
    }
}

pub struct Parser {
    tokens: Vec<TokenWithSpan>,
    pos: usize,
}

impl Parser {
    pub fn new(tokens: Vec<TokenWithSpan>) -> Self {
        Parser { tokens, pos: 0 }
    }

    pub(crate) fn peek(&self) -> &Token {
        self.tokens.get(self.pos).map(|t| &t.token).unwrap_or(&Token::EOF)
    }

    pub(crate) fn current_span(&self) -> (usize, usize) {
        self.tokens.get(self.pos).map(|t| (t.line, t.col)).unwrap_or((0, 0))
    }

    pub(crate) fn advance(&mut self) -> &Token {
        let token = self.tokens.get(self.pos).map(|t| &t.token).unwrap_or(&Token::EOF);
        if self.pos < self.tokens.len() {
            self.pos += 1;
        }
        token
    }

    pub(crate) fn expect(&mut self, expected: &Token) -> Result<(), ParseError> {
        let (line, col) = self.current_span();
        let actual = self.peek().clone();
        if std::mem::discriminant(&actual) == std::mem::discriminant(expected) {
            self.advance();
            Ok(())
        } else {
            Err(ParseError {
                message: format!("Expected '{}', found '{}'", expected, actual),
                line, col,
            })
        }
    }

    pub(crate) fn skip_newlines(&mut self) {
        while matches!(self.peek(), Token::Newline | Token::Comment(_)) {
            self.advance();
        }
    }

    pub(crate) fn is_symbol_ident(name: &str) -> bool {
        !name.is_empty()
            && name.chars().all(|c| c.is_uppercase() || c.is_ascii_digit() || c == '_' || c == '.')
            && name.chars().next().map_or(false, |c| c.is_uppercase())
    }

    /// Returns true if the identifier looks like a type/struct name (PascalCase).
    /// Must start with uppercase, contain at least one lowercase letter,
    /// and not be all-uppercase (which is a ticker symbol).
    pub(crate) fn is_type_name(name: &str) -> bool {
        !name.is_empty()
            && name.chars().next().map_or(false, |c| c.is_uppercase())
            && name.chars().any(|c| c.is_lowercase())
    }

    pub fn parse(&mut self) -> Result<Program, ParseError> {
        let mut statements = Vec::new();
        self.skip_newlines();

        while !matches!(self.peek(), Token::EOF) {
            let stmt = self.parse_statement()?;
            statements.push(stmt);
            self.skip_newlines();
        }

        Ok(statements)
    }

    pub(crate) fn parse_statement(&mut self) -> Result<Statement, ParseError> {
        let (line, col) = self.current_span();

        match self.peek().clone() {
            Token::Comment(text) => {
                let comment = text.clone();
                self.advance();
                Ok(Statement::Comment(comment))
            }
            Token::If => self.parse_if_block(),
            Token::For => self.parse_for_loop(),
            Token::While => self.parse_while_loop(),
            Token::Fn => self.parse_fn_def(),
            Token::Return => self.parse_return(),
            Token::Break => { self.advance(); Ok(Statement::Break) }
            Token::Continue => { self.advance(); Ok(Statement::Continue) }
            Token::Buy => self.parse_buy(),
            Token::Sell => self.parse_sell(),
            Token::Plot => self.parse_plot(),
            Token::PlotCandlestick => self.parse_plot_candlestick(),
            Token::PlotLine => self.parse_plot_line(),
            Token::PlotHistogram => self.parse_plot_histogram(),
            Token::PlotShape => self.parse_plot_shape(),
            Token::Bgcolor => self.parse_bgcolor(),
            Token::Hline => self.parse_hline(),
            Token::Switch => self.parse_switch(),
            Token::Input => self.parse_input_decl(),
            Token::Struct => self.parse_struct_def(),
            Token::Print => self.parse_print(),
            Token::Alert => self.parse_alert(),
            Token::Import => self.parse_import(),
            Token::Export => self.parse_export(),
            Token::Strategy => {
                self.advance(); // consume 'strategy'
                if matches!(self.peek(), Token::Dot) {
                    self.advance(); // consume '.'
                    let method = match self.peek().clone() {
                        Token::Ident(m) => { self.advance(); m }
                        _ => { let (l, c) = self.current_span(); return Err(ParseError { message: "Expected method after 'strategy.'".to_string(), line: l, col: c }); }
                    };
                    match method.as_str() {
                        "entry" => self.parse_strategy_entry(),
                        "exit" => self.parse_strategy_exit(),
                        "close" | "close_all" => self.parse_strategy_close(),
                        _ => {
                            let (l, c) = self.current_span();
                            Err(ParseError { message: format!("Unknown strategy method '{}'", method), line: l, col: c })
                        }
                    }
                } else {
                    // strategy(...) as configuration - treat as expr statement
                    let expr = Expr::FunctionCall { name: "strategy".to_string(), args: vec![] };
                    Ok(Statement::ExprStatement(expr))
                }
            }
            Token::Color | Token::Request => {
                // color.xxx() and request.xxx() are parsed as expressions
                let expr = self.parse_expression()?;
                Ok(Statement::ExprStatement(expr))
            }
            Token::Ident(_) => {
                if self.is_assignment() {
                    self.parse_assignment()
                } else if self.is_compound_assign() {
                    self.parse_compound_assign()
                } else if self.is_index_assign() {
                    self.parse_index_assign()
                } else {
                    let expr = self.parse_expression()?;
                    Ok(Statement::ExprStatement(expr))
                }
            }
            _ => Err(ParseError {
                message: format!("Unexpected token '{}' at start of statement", self.peek()),
                line, col,
            }),
        }
    }

    pub(crate) fn parse_block_body(&mut self) -> Result<Vec<Statement>, ParseError> {
        self.skip_newlines();
        let mut body = Vec::new();
        while !matches!(self.peek(), Token::RBrace | Token::EOF) {
            let stmt = self.parse_statement()?;
            body.push(stmt);
            self.skip_newlines();
        }
        Ok(body)
    }

    fn is_assignment(&self) -> bool {
        if let Some(TokenWithSpan { token: Token::Ident(_), .. }) = self.tokens.get(self.pos) {
            if let Some(TokenWithSpan { token: Token::Assign, .. }) = self.tokens.get(self.pos + 1) {
                // Make sure next is not '=' (i.e. not ==)
                if let Some(TokenWithSpan { token: Token::Assign, .. }) = self.tokens.get(self.pos + 2) {
                    return false;
                }
                return true;
            }
        }
        false
    }

    fn is_compound_assign(&self) -> bool {
        if let Some(TokenWithSpan { token: Token::Ident(_), .. }) = self.tokens.get(self.pos) {
            if let Some(t) = self.tokens.get(self.pos + 1) {
                return matches!(t.token, Token::PlusAssign | Token::MinusAssign | Token::StarAssign | Token::SlashAssign);
            }
        }
        false
    }

    fn is_index_assign(&self) -> bool {
        // ident[expr] = expr
        if let Some(TokenWithSpan { token: Token::Ident(_), .. }) = self.tokens.get(self.pos) {
            if let Some(TokenWithSpan { token: Token::LBracket, .. }) = self.tokens.get(self.pos + 1) {
                // Find matching ] then check for =
                let mut depth = 0;
                let mut i = self.pos + 1;
                while i < self.tokens.len() {
                    match &self.tokens[i].token {
                        Token::LBracket => depth += 1,
                        Token::RBracket => {
                            depth -= 1;
                            if depth == 0 {
                                if let Some(TokenWithSpan { token: Token::Assign, .. }) = self.tokens.get(i + 1) {
                                    // check not ==
                                    if let Some(TokenWithSpan { token: Token::Assign, .. }) = self.tokens.get(i + 2) {
                                        return false;
                                    }
                                    return true;
                                }
                                return false;
                            }
                        }
                        Token::EOF => break,
                        _ => {}
                    }
                    i += 1;
                }
            }
        }
        false
    }
}

pub fn parse(tokens: Vec<TokenWithSpan>) -> Result<Program, ParseError> {
    let mut parser = Parser::new(tokens);
    parser.parse()
}
