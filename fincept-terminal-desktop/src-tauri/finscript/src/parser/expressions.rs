use crate::ast::*;
use crate::lexer::Token;
use super::{Parser, ParseError};

impl Parser {
    pub(crate) fn parse_expression(&mut self) -> Result<Expr, ParseError> {
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

    pub(crate) fn parse_primary(&mut self) -> Result<Expr, ParseError> {
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
