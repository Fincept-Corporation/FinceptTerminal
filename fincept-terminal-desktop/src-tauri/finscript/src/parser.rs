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

    fn peek(&self) -> &Token {
        self.tokens.get(self.pos).map(|t| &t.token).unwrap_or(&Token::EOF)
    }

    fn current_span(&self) -> (usize, usize) {
        self.tokens.get(self.pos).map(|t| (t.line, t.col)).unwrap_or((0, 0))
    }

    fn advance(&mut self) -> &Token {
        let token = self.tokens.get(self.pos).map(|t| &t.token).unwrap_or(&Token::EOF);
        if self.pos < self.tokens.len() {
            self.pos += 1;
        }
        token
    }

    fn expect(&mut self, expected: &Token) -> Result<(), ParseError> {
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

    fn skip_newlines(&mut self) {
        while matches!(self.peek(), Token::Newline | Token::Comment(_)) {
            self.advance();
        }
    }

    fn is_symbol_ident(name: &str) -> bool {
        !name.is_empty()
            && name.chars().all(|c| c.is_uppercase() || c.is_ascii_digit() || c == '_' || c == '.')
            && name.chars().next().map_or(false, |c| c.is_uppercase())
    }

    /// Returns true if the identifier looks like a type/struct name (PascalCase).
    /// Must start with uppercase, contain at least one lowercase letter,
    /// and not be all-uppercase (which is a ticker symbol).
    fn is_type_name(name: &str) -> bool {
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

    fn parse_statement(&mut self) -> Result<Statement, ParseError> {
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

    fn parse_assignment(&mut self) -> Result<Statement, ParseError> {
        let name = match self.peek().clone() {
            Token::Ident(name) => { self.advance(); name }
            _ => unreachable!(),
        };
        self.expect(&Token::Assign)?;
        let value = self.parse_expression()?;
        Ok(Statement::Assignment { name, value })
    }

    fn parse_compound_assign(&mut self) -> Result<Statement, ParseError> {
        let name = match self.peek().clone() {
            Token::Ident(name) => { self.advance(); name }
            _ => unreachable!(),
        };
        let op = match self.peek() {
            Token::PlusAssign => { self.advance(); BinOp::Add }
            Token::MinusAssign => { self.advance(); BinOp::Sub }
            Token::StarAssign => { self.advance(); BinOp::Mul }
            Token::SlashAssign => { self.advance(); BinOp::Div }
            _ => unreachable!(),
        };
        let value = self.parse_expression()?;
        Ok(Statement::CompoundAssign { name, op, value })
    }

    fn parse_index_assign(&mut self) -> Result<Statement, ParseError> {
        let object = match self.peek().clone() {
            Token::Ident(name) => { self.advance(); name }
            _ => unreachable!(),
        };
        self.expect(&Token::LBracket)?;
        let index = self.parse_expression()?;
        self.expect(&Token::RBracket)?;
        self.expect(&Token::Assign)?;
        let value = self.parse_expression()?;
        Ok(Statement::IndexAssign { object, index, value })
    }

    fn parse_if_block(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'if'
        let condition = self.parse_expression()?;

        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        let body = self.parse_block_body()?;
        self.expect(&Token::RBrace)?;

        // Parse else if / else chains
        let mut else_if_blocks = Vec::new();
        let mut else_body = None;

        loop {
            self.skip_newlines();
            if !matches!(self.peek(), Token::Else) {
                break;
            }
            self.advance(); // consume 'else'

            if matches!(self.peek(), Token::If) {
                // else if
                self.advance(); // consume 'if'
                let elif_cond = self.parse_expression()?;
                self.skip_newlines();
                self.expect(&Token::LBrace)?;
                let elif_body = self.parse_block_body()?;
                self.expect(&Token::RBrace)?;
                else_if_blocks.push((elif_cond, elif_body));
            } else {
                // else
                self.skip_newlines();
                self.expect(&Token::LBrace)?;
                let eb = self.parse_block_body()?;
                self.expect(&Token::RBrace)?;
                else_body = Some(eb);
                break;
            }
        }

        Ok(Statement::IfBlock { condition, body, else_if_blocks, else_body })
    }

    fn parse_for_loop(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'for'
        let var_name = match self.peek().clone() {
            Token::Ident(name) => { self.advance(); name }
            other => {
                let (line, col) = self.current_span();
                return Err(ParseError {
                    message: format!("Expected variable name after 'for', found '{}'", other),
                    line, col,
                });
            }
        };
        self.expect(&Token::In)?;
        let iterable = self.parse_expression()?;
        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        let body = self.parse_block_body()?;
        self.expect(&Token::RBrace)?;

        Ok(Statement::ForLoop { var_name, iterable, body })
    }

    fn parse_while_loop(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'while'
        let condition = self.parse_expression()?;
        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        let body = self.parse_block_body()?;
        self.expect(&Token::RBrace)?;

        Ok(Statement::WhileLoop { condition, body })
    }

    fn parse_fn_def(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'fn'
        let name = match self.peek().clone() {
            Token::Ident(name) => { self.advance(); name }
            other => {
                let (line, col) = self.current_span();
                return Err(ParseError {
                    message: format!("Expected function name, found '{}'", other),
                    line, col,
                });
            }
        };
        self.expect(&Token::LParen)?;
        let mut params = Vec::new();
        if !matches!(self.peek(), Token::RParen) {
            match self.peek().clone() {
                Token::Ident(p) => { self.advance(); params.push(p); }
                _ => {}
            }
            while matches!(self.peek(), Token::Comma) {
                self.advance();
                match self.peek().clone() {
                    Token::Ident(p) => { self.advance(); params.push(p); }
                    _ => break,
                }
            }
        }
        self.expect(&Token::RParen)?;
        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        let body = self.parse_block_body()?;
        self.expect(&Token::RBrace)?;

        Ok(Statement::FnDef { name, params, body })
    }

    fn parse_return(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'return'
        // Check if there's a value on the same line
        if matches!(self.peek(), Token::Newline | Token::RBrace | Token::EOF) {
            Ok(Statement::Return { value: None })
        } else {
            let expr = self.parse_expression()?;
            Ok(Statement::Return { value: Some(expr) })
        }
    }

    fn parse_block_body(&mut self) -> Result<Vec<Statement>, ParseError> {
        self.skip_newlines();
        let mut body = Vec::new();
        while !matches!(self.peek(), Token::RBrace | Token::EOF) {
            let stmt = self.parse_statement()?;
            body.push(stmt);
            self.skip_newlines();
        }
        Ok(body)
    }

    fn parse_buy(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let message = self.parse_expression()?;
        Ok(Statement::Buy { message })
    }

    fn parse_sell(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let message = self.parse_expression()?;
        Ok(Statement::Sell { message })
    }

    fn parse_plot(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let expr = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let label = self.parse_expression()?;
        Ok(Statement::Plot { expr, label })
    }

    fn parse_plot_candlestick(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let symbol = self.parse_primary()?;
        self.expect(&Token::Comma)?;
        let title = self.parse_expression()?;
        Ok(Statement::PlotCandlestick { symbol, title })
    }

    fn parse_plot_line(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let value = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let label = self.parse_expression()?;
        let color = if matches!(self.peek(), Token::Comma) {
            self.advance();
            Some(self.parse_expression()?)
        } else {
            None
        };
        Ok(Statement::PlotLine { value, label, color })
    }

    fn parse_plot_histogram(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let value = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let label = self.parse_expression()?;
        let color_up = if matches!(self.peek(), Token::Comma) {
            self.advance();
            Some(self.parse_expression()?)
        } else { None };
        let color_down = if matches!(self.peek(), Token::Comma) {
            self.advance();
            Some(self.parse_expression()?)
        } else { None };
        Ok(Statement::PlotHistogram { value, label, color_up, color_down })
    }

    fn parse_plot_shape(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let condition = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let shape = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let location = self.parse_expression()?;
        let color = if matches!(self.peek(), Token::Comma) {
            self.advance(); Some(self.parse_expression()?)
        } else { None };
        let text = if matches!(self.peek(), Token::Comma) {
            self.advance(); Some(self.parse_expression()?)
        } else { None };
        Ok(Statement::PlotShape { condition, shape, location, color, text })
    }

    fn parse_bgcolor(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let color = self.parse_expression()?;
        let condition = if matches!(self.peek(), Token::Comma) {
            self.advance(); Some(self.parse_expression()?)
        } else { None };
        Ok(Statement::Bgcolor { color, condition })
    }

    fn parse_hline(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let value = self.parse_expression()?;
        let label = if matches!(self.peek(), Token::Comma) {
            self.advance(); Some(self.parse_expression()?)
        } else { None };
        let color = if matches!(self.peek(), Token::Comma) {
            self.advance(); Some(self.parse_expression()?)
        } else { None };
        Ok(Statement::Hline { value, label, color })
    }

    fn parse_switch(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'switch'
        // Optional switch expression
        let expr = if !matches!(self.peek(), Token::LBrace | Token::Newline) {
            Some(self.parse_expression()?)
        } else { None };
        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        self.skip_newlines();
        let mut cases = Vec::new();
        let mut default = None;
        while !matches!(self.peek(), Token::RBrace | Token::EOF) {
            // Check for "else" (default case)
            if matches!(self.peek(), Token::Else) {
                self.advance();
                self.skip_newlines();
                self.expect(&Token::LBrace)?;
                let body = self.parse_block_body()?;
                self.expect(&Token::RBrace)?;
                default = Some(body);
                self.skip_newlines();
                break;
            }
            let case_expr = self.parse_expression()?;
            self.skip_newlines();
            self.expect(&Token::LBrace)?;
            let body = self.parse_block_body()?;
            self.expect(&Token::RBrace)?;
            cases.push((case_expr, body));
            self.skip_newlines();
        }
        self.expect(&Token::RBrace)?;
        Ok(Statement::SwitchBlock { expr, cases, default })
    }

    fn parse_strategy_entry(&mut self) -> Result<Statement, ParseError> {
        // strategy.entry(id, direction, qty?, price?, stop?, limit?)
        self.expect(&Token::LParen)?;
        let id = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let direction = self.parse_expression()?;
        let qty = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let price = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let stop = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let limit = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        self.expect(&Token::RParen)?;
        Ok(Statement::StrategyEntry { id, direction, qty, price, stop, limit })
    }

    fn parse_strategy_exit(&mut self) -> Result<Statement, ParseError> {
        self.expect(&Token::LParen)?;
        let id = self.parse_expression()?;
        let from_entry = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let qty = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let stop = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let limit = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let trail_points = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        let trail_offset = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
        self.expect(&Token::RParen)?;
        Ok(Statement::StrategyExit { id, from_entry, qty, stop, limit, trail_points, trail_offset })
    }

    fn parse_strategy_close(&mut self) -> Result<Statement, ParseError> {
        self.expect(&Token::LParen)?;
        let id = self.parse_expression()?;
        self.expect(&Token::RParen)?;
        Ok(Statement::StrategyClose { id })
    }

    fn parse_input_decl(&mut self) -> Result<Statement, ParseError> {
        // input name = input.type(default, title?)
        // or: name = input(default, title?)  - handled in assignment
        // Standalone: input.int(...), etc - parse as function call expr statement
        self.advance(); // consume 'input'
        if matches!(self.peek(), Token::Dot) {
            self.advance(); // consume '.'
            let type_name = match self.peek().clone() {
                Token::Ident(t) => { self.advance(); t }
                _ => "float".to_string(),
            };
            self.expect(&Token::LParen)?;
            let default_value = self.parse_expression()?;
            let title = if matches!(self.peek(), Token::Comma) { self.advance(); Some(self.parse_expression()?) } else { None };
            self.expect(&Token::RParen)?;
            // We'll treat this as an expression that returns the default
            Ok(Statement::InputDecl { name: String::new(), input_type: type_name, default_value, title })
        } else {
            let (line, col) = self.current_span();
            Err(ParseError { message: "Expected '.' after 'input' (e.g., input.int, input.float)".to_string(), line, col })
        }
    }

    fn parse_struct_def(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'struct'
        let name = match self.peek().clone() {
            Token::Ident(n) => { self.advance(); n }
            other => {
                let (line, col) = self.current_span();
                return Err(ParseError { message: format!("Expected struct name, found '{}'", other), line, col });
            }
        };
        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        self.skip_newlines();
        let mut fields = Vec::new();
        while !matches!(self.peek(), Token::RBrace | Token::EOF) {
            let field_name = match self.peek().clone() {
                Token::Ident(f) => { self.advance(); f }
                _ => break,
            };
            // Optional type hint: field_name: type
            let type_hint = if matches!(self.peek(), Token::Colon) {
                self.advance();
                match self.peek().clone() {
                    Token::Ident(t) => { self.advance(); t }
                    _ => "any".to_string(),
                }
            } else {
                "any".to_string()
            };
            fields.push((field_name, type_hint));
            // Skip comma or newline
            if matches!(self.peek(), Token::Comma) { self.advance(); }
            self.skip_newlines();
        }
        self.expect(&Token::RBrace)?;
        Ok(Statement::StructDef { name, fields })
    }

    fn parse_print(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'print'
        let mut args = Vec::new();
        // Parse comma-separated expressions until newline/EOF
        if !matches!(self.peek(), Token::Newline | Token::EOF | Token::RBrace) {
            args.push(self.parse_expression()?);
            while matches!(self.peek(), Token::Comma) {
                self.advance();
                args.push(self.parse_expression()?);
            }
        }
        Ok(Statement::PrintStatement { args })
    }

    fn parse_alert(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'alert'
        // alert(message) or alert(message, alert_type)
        if matches!(self.peek(), Token::LParen) {
            self.advance();
            let message = self.parse_expression()?;
            let alert_type = if matches!(self.peek(), Token::Comma) {
                self.advance();
                Some(self.parse_expression()?)
            } else { None };
            self.expect(&Token::RParen)?;
            Ok(Statement::AlertStatement { message, alert_type })
        } else {
            let message = self.parse_expression()?;
            Ok(Statement::AlertStatement { message, alert_type: None })
        }
    }

    fn parse_import(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'import'
        let module = match self.peek().clone() {
            Token::Ident(m) => { self.advance(); m }
            Token::StringLit(m) => { self.advance(); m }
            other => {
                let (line, col) = self.current_span();
                return Err(ParseError { message: format!("Expected module name after 'import', found '{}'", other), line, col });
            }
        };
        // Handle dotted module path: import foo.bar.baz
        let mut full_module = module;
        while matches!(self.peek(), Token::Dot) {
            self.advance();
            match self.peek().clone() {
                Token::Ident(part) => { self.advance(); full_module = format!("{}.{}", full_module, part); }
                _ => break,
            }
        }
        // Optional alias: import foo as bar
        let alias = if let Token::Ident(ref s) = self.peek().clone() {
            if s == "as" {
                self.advance();
                match self.peek().clone() {
                    Token::Ident(a) => { self.advance(); Some(a) }
                    _ => None,
                }
            } else { None }
        } else { None };
        Ok(Statement::ImportStatement { module: full_module, alias })
    }

    fn parse_export(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'export'
        let name = match self.peek().clone() {
            Token::Ident(n) => { self.advance(); n }
            Token::Fn => {
                // export fn name(...) { ... }
                let fn_stmt = self.parse_fn_def()?;
                if let Statement::FnDef { name, .. } = &fn_stmt {
                    return Ok(Statement::ExportStatement { name: name.clone() });
                }
                return Ok(fn_stmt);
            }
            other => {
                let (line, col) = self.current_span();
                return Err(ParseError { message: format!("Expected name after 'export', found '{}'", other), line, col });
            }
        };
        Ok(Statement::ExportStatement { name })
    }

    // ─── Expression Parsing ─────────────────────────────────────────────────

    fn parse_expression(&mut self) -> Result<Expr, ParseError> {
        let expr = self.parse_or()?;
        // Ternary: expr ? then : else
        if matches!(self.peek(), Token::Question) {
            self.advance();
            let then_expr = self.parse_expression()?;
            self.expect(&Token::Colon)?;
            let else_expr = self.parse_expression()?;
            Ok(Expr::Ternary {
                condition: Box::new(expr),
                then_expr: Box::new(then_expr),
                else_expr: Box::new(else_expr),
            })
        } else {
            Ok(expr)
        }
    }

    fn parse_or(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_and()?;
        while matches!(self.peek(), Token::Or) {
            self.advance();
            let right = self.parse_and()?;
            left = Expr::BinaryOp { left: Box::new(left), op: BinOp::Or, right: Box::new(right) };
        }
        Ok(left)
    }

    fn parse_and(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_not()?;
        while matches!(self.peek(), Token::And) {
            self.advance();
            let right = self.parse_not()?;
            left = Expr::BinaryOp { left: Box::new(left), op: BinOp::And, right: Box::new(right) };
        }
        Ok(left)
    }

    fn parse_not(&mut self) -> Result<Expr, ParseError> {
        if matches!(self.peek(), Token::Not) {
            self.advance();
            let operand = self.parse_not()?;
            Ok(Expr::UnaryOp { op: UnaryOp::Not, operand: Box::new(operand) })
        } else {
            self.parse_comparison()
        }
    }

    fn parse_comparison(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_range()?;
        loop {
            let op = match self.peek() {
                Token::Gt => BinOp::Gt,
                Token::Lt => BinOp::Lt,
                Token::Gte => BinOp::Gte,
                Token::Lte => BinOp::Lte,
                Token::EqEq => BinOp::Eq,
                Token::Neq => BinOp::Neq,
                _ => break,
            };
            self.advance();
            let right = self.parse_range()?;
            left = Expr::BinaryOp { left: Box::new(left), op, right: Box::new(right) };
        }
        Ok(left)
    }

    fn parse_range(&mut self) -> Result<Expr, ParseError> {
        let left = self.parse_additive()?;
        if matches!(self.peek(), Token::DotDot) {
            self.advance();
            let right = self.parse_additive()?;
            Ok(Expr::Range { start: Box::new(left), end: Box::new(right) })
        } else {
            Ok(left)
        }
    }

    fn parse_additive(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_multiplicative()?;
        loop {
            let op = match self.peek() {
                Token::Plus => BinOp::Add,
                Token::Minus => BinOp::Sub,
                _ => break,
            };
            self.advance();
            let right = self.parse_multiplicative()?;
            left = Expr::BinaryOp { left: Box::new(left), op, right: Box::new(right) };
        }
        Ok(left)
    }

    fn parse_multiplicative(&mut self) -> Result<Expr, ParseError> {
        let mut left = self.parse_unary()?;
        loop {
            let op = match self.peek() {
                Token::Star => BinOp::Mul,
                Token::Slash => BinOp::Div,
                Token::Percent => BinOp::Mod,
                _ => break,
            };
            self.advance();
            let right = self.parse_unary()?;
            left = Expr::BinaryOp { left: Box::new(left), op, right: Box::new(right) };
        }
        Ok(left)
    }

    fn parse_unary(&mut self) -> Result<Expr, ParseError> {
        if matches!(self.peek(), Token::Minus) {
            self.advance();
            let operand = self.parse_unary()?;
            Ok(Expr::UnaryOp { op: UnaryOp::Neg, operand: Box::new(operand) })
        } else {
            self.parse_postfix()
        }
    }

    fn parse_postfix(&mut self) -> Result<Expr, ParseError> {
        let mut expr = self.parse_primary()?;
        loop {
            match self.peek() {
                Token::LBracket => {
                    self.advance();
                    let index = self.parse_expression()?;
                    self.expect(&Token::RBracket)?;
                    expr = Expr::IndexAccess { object: Box::new(expr), index: Box::new(index) };
                }
                Token::Dot => {
                    self.advance();
                    let method = match self.peek().clone() {
                        Token::Ident(m) => { self.advance(); m }
                        _ => break,
                    };
                    if matches!(self.peek(), Token::LParen) {
                        self.advance();
                        let mut args = Vec::new();
                        if !matches!(self.peek(), Token::RParen) {
                            args.push(self.parse_expression()?);
                            while matches!(self.peek(), Token::Comma) {
                                self.advance();
                                args.push(self.parse_expression()?);
                            }
                        }
                        self.expect(&Token::RParen)?;
                        expr = Expr::MethodCall { object: Box::new(expr), method, args };
                    } else {
                        // Property access: treat as method call with no args (like a getter)
                        expr = Expr::MethodCall { object: Box::new(expr), method, args: vec![] };
                    }
                }
                _ => break,
            }
        }
        Ok(expr)
    }

    fn parse_primary(&mut self) -> Result<Expr, ParseError> {
        let (line, col) = self.current_span();

        match self.peek().clone() {
            Token::Number(n) => {
                self.advance();
                Ok(Expr::Number(n))
            }
            Token::True => {
                self.advance();
                Ok(Expr::Bool(true))
            }
            Token::False => {
                self.advance();
                Ok(Expr::Bool(false))
            }
            Token::Na => {
                self.advance();
                // na can be used as a value OR as a function: na(x) to check if x is na
                if matches!(self.peek(), Token::LParen) {
                    self.advance(); // consume '('
                    self.skip_newlines();
                    let mut args = Vec::new();
                    if !matches!(self.peek(), Token::RParen) {
                        args.push(self.parse_expression()?);
                        self.skip_newlines();
                        while matches!(self.peek(), Token::Comma) {
                            self.advance();
                            self.skip_newlines();
                            args.push(self.parse_expression()?);
                            self.skip_newlines();
                        }
                    }
                    self.skip_newlines();
                    self.expect(&Token::RParen)?;
                    Ok(Expr::FunctionCall { name: "na".to_string(), args })
                } else {
                    Ok(Expr::Na)
                }
            }
            Token::StringLit(s) => {
                let val = s.clone();
                self.advance();
                Ok(Expr::StringLiteral(val))
            }
            Token::Color => {
                // color.rgb(...), color.new(...), color.red, etc.
                self.advance();
                if matches!(self.peek(), Token::Dot) {
                    self.advance();
                    let method = match self.peek().clone() {
                        Token::Ident(m) => { self.advance(); m }
                        _ => "white".to_string(),
                    };
                    if matches!(self.peek(), Token::LParen) {
                        self.advance();
                        let mut args = Vec::new();
                        if !matches!(self.peek(), Token::RParen) {
                            args.push(self.parse_expression()?);
                            while matches!(self.peek(), Token::Comma) {
                                self.advance();
                                args.push(self.parse_expression()?);
                            }
                        }
                        self.expect(&Token::RParen)?;
                        Ok(Expr::FunctionCall { name: format!("color_{}", method), args })
                    } else {
                        // color.red, color.green etc. - named color constant
                        Ok(Expr::FunctionCall { name: format!("color_{}", method), args: vec![] })
                    }
                } else {
                    Ok(Expr::Variable("color".to_string()))
                }
            }
            Token::Request => {
                // request.security(...), request.financial(...), etc.
                self.advance();
                if matches!(self.peek(), Token::Dot) {
                    self.advance();
                    let method = match self.peek().clone() {
                        Token::Ident(m) => { self.advance(); m }
                        _ => "security".to_string(),
                    };
                    if matches!(self.peek(), Token::LParen) {
                        self.advance();
                        let mut args = Vec::new();
                        if !matches!(self.peek(), Token::RParen) {
                            args.push(self.parse_expression()?);
                            while matches!(self.peek(), Token::Comma) {
                                self.advance();
                                args.push(self.parse_expression()?);
                            }
                        }
                        self.expect(&Token::RParen)?;
                        Ok(Expr::FunctionCall { name: format!("request_{}", method), args })
                    } else {
                        Ok(Expr::Variable(format!("request_{}", method)))
                    }
                } else {
                    Ok(Expr::Variable("request".to_string()))
                }
            }
            Token::Ident(name) => {
                let name = name.clone();
                self.advance();
                if matches!(self.peek(), Token::LParen) {
                    self.advance(); // consume '('
                    self.skip_newlines();
                    let mut args = Vec::new();
                    if !matches!(self.peek(), Token::RParen) {
                        args.push(self.parse_expression()?);
                        self.skip_newlines();
                        while matches!(self.peek(), Token::Comma) {
                            self.advance();
                            self.skip_newlines();
                            args.push(self.parse_expression()?);
                            self.skip_newlines();
                        }
                    }
                    self.skip_newlines();
                    self.expect(&Token::RParen)?;
                    Ok(Expr::FunctionCall { name, args })
                } else if matches!(self.peek(), Token::LBrace) && Self::is_type_name(&name) {
                    // Struct literal: TypeName { field: value, ... }
                    self.advance(); // consume '{'
                    let mut fields = Vec::new();
                    self.skip_newlines();
                    while !matches!(self.peek(), Token::RBrace | Token::EOF) {
                        let field_name = match self.peek().clone() {
                            Token::Ident(f) => { self.advance(); f }
                            _ => break,
                        };
                        self.expect(&Token::Colon)?;
                        let value = self.parse_expression()?;
                        fields.push((field_name, value));
                        if matches!(self.peek(), Token::Comma) { self.advance(); }
                        self.skip_newlines();
                    }
                    self.expect(&Token::RBrace)?;
                    Ok(Expr::StructLiteral { type_name: name, fields })
                } else if Self::is_symbol_ident(&name) {
                    Ok(Expr::Symbol(name))
                } else {
                    Ok(Expr::Variable(name))
                }
            }
            Token::LParen => {
                self.advance();
                let expr = self.parse_expression()?;
                self.expect(&Token::RParen)?;
                Ok(expr)
            }
            Token::LBracket => {
                self.advance();
                let mut elements = Vec::new();
                if !matches!(self.peek(), Token::RBracket) {
                    elements.push(self.parse_expression()?);
                    while matches!(self.peek(), Token::Comma) {
                        self.advance();
                        if matches!(self.peek(), Token::RBracket) { break; }
                        elements.push(self.parse_expression()?);
                    }
                }
                self.expect(&Token::RBracket)?;
                Ok(Expr::ArrayLiteral(elements))
            }
            other => Err(ParseError {
                message: format!("Unexpected token '{}' in expression", other),
                line, col,
            }),
        }
    }
}

pub fn parse(tokens: Vec<TokenWithSpan>) -> Result<Program, ParseError> {
    let mut parser = Parser::new(tokens);
    parser.parse()
}
