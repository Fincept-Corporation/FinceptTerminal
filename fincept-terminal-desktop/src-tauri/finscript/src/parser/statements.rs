use crate::ast::*;
use crate::lexer::Token;
use super::{Parser, ParseError};

impl Parser {
    pub(crate) fn parse_assignment(&mut self) -> Result<Statement, ParseError> {
        let name = match self.peek().clone() {
            Token::Ident(name) => { self.advance(); name }
            _ => unreachable!(),
        };
        self.expect(&Token::Assign)?;
        let value = self.parse_expression()?;
        Ok(Statement::Assignment { name, value })
    }

    pub(crate) fn parse_compound_assign(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_index_assign(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_if_block(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_for_loop(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_while_loop(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'while'
        let condition = self.parse_expression()?;
        self.skip_newlines();
        self.expect(&Token::LBrace)?;
        let body = self.parse_block_body()?;
        self.expect(&Token::RBrace)?;

        Ok(Statement::WhileLoop { condition, body })
    }

    pub(crate) fn parse_fn_def(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_return(&mut self) -> Result<Statement, ParseError> {
        self.advance(); // consume 'return'
        // Check if there's a value on the same line
        if matches!(self.peek(), Token::Newline | Token::RBrace | Token::EOF) {
            Ok(Statement::Return { value: None })
        } else {
            let expr = self.parse_expression()?;
            Ok(Statement::Return { value: Some(expr) })
        }
    }

    pub(crate) fn parse_buy(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let message = self.parse_expression()?;
        Ok(Statement::Buy { message })
    }

    pub(crate) fn parse_sell(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let message = self.parse_expression()?;
        Ok(Statement::Sell { message })
    }

    pub(crate) fn parse_print(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_alert(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_import(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_export(&mut self) -> Result<Statement, ParseError> {
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
}
