use crate::ast::*;
use crate::lexer::Token;
use super::{Parser, ParseError};

impl Parser {
    pub(crate) fn parse_plot(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let expr = self.parse_expression()?;
        self.expect(&Token::Comma)?;
        let label = self.parse_expression()?;
        Ok(Statement::Plot { expr, label })
    }

    pub(crate) fn parse_plot_candlestick(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let symbol = self.parse_primary()?;
        self.expect(&Token::Comma)?;
        let title = self.parse_expression()?;
        Ok(Statement::PlotCandlestick { symbol, title })
    }

    pub(crate) fn parse_plot_line(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_plot_histogram(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_plot_shape(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_bgcolor(&mut self) -> Result<Statement, ParseError> {
        self.advance();
        let color = self.parse_expression()?;
        let condition = if matches!(self.peek(), Token::Comma) {
            self.advance(); Some(self.parse_expression()?)
        } else { None };
        Ok(Statement::Bgcolor { color, condition })
    }

    pub(crate) fn parse_hline(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_switch(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_strategy_entry(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_strategy_exit(&mut self) -> Result<Statement, ParseError> {
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

    pub(crate) fn parse_strategy_close(&mut self) -> Result<Statement, ParseError> {
        self.expect(&Token::LParen)?;
        let id = self.parse_expression()?;
        self.expect(&Token::RParen)?;
        Ok(Statement::StrategyClose { id })
    }

    pub(crate) fn parse_input_decl(&mut self) -> Result<Statement, ParseError> {
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
            Ok(Statement::InputDecl { name: String::new(), input_type: type_name, default_value, title })
        } else {
            let (line, col) = self.current_span();
            Err(ParseError { message: "Expected '.' after 'input' (e.g., input.int, input.float)".to_string(), line, col })
        }
    }

    pub(crate) fn parse_struct_def(&mut self) -> Result<Statement, ParseError> {
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
}
