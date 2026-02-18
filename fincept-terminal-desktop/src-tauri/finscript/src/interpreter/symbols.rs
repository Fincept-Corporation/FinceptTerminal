use crate::ast::*;

/// Collect all unique symbol names from a program's AST
pub fn collect_symbols(program: &[Statement]) -> Vec<String> {
    let mut symbols = Vec::new();
    for stmt in program {
        collect_symbols_from_statement(stmt, &mut symbols);
    }
    symbols.sort();
    symbols.dedup();
    symbols
}

fn collect_symbols_from_statement(stmt: &Statement, symbols: &mut Vec<String>) {
    match stmt {
        Statement::Assignment { value, .. } => collect_symbols_from_expr(value, symbols),
        Statement::CompoundAssign { value, .. } => collect_symbols_from_expr(value, symbols),
        Statement::IndexAssign { index, value, .. } => {
            collect_symbols_from_expr(index, symbols);
            collect_symbols_from_expr(value, symbols);
        }
        Statement::IfBlock { condition, body, else_if_blocks, else_body } => {
            collect_symbols_from_expr(condition, symbols);
            for s in body { collect_symbols_from_statement(s, symbols); }
            for (cond, stmts) in else_if_blocks {
                collect_symbols_from_expr(cond, symbols);
                for s in stmts { collect_symbols_from_statement(s, symbols); }
            }
            if let Some(eb) = else_body {
                for s in eb { collect_symbols_from_statement(s, symbols); }
            }
        }
        Statement::ForLoop { iterable, body, .. } => {
            collect_symbols_from_expr(iterable, symbols);
            for s in body { collect_symbols_from_statement(s, symbols); }
        }
        Statement::WhileLoop { condition, body } => {
            collect_symbols_from_expr(condition, symbols);
            for s in body { collect_symbols_from_statement(s, symbols); }
        }
        Statement::FnDef { body, .. } => {
            for s in body { collect_symbols_from_statement(s, symbols); }
        }
        Statement::Return { value } => {
            if let Some(v) = value { collect_symbols_from_expr(v, symbols); }
        }
        Statement::Buy { message } => collect_symbols_from_expr(message, symbols),
        Statement::Sell { message } => collect_symbols_from_expr(message, symbols),
        Statement::Plot { expr, label } => {
            collect_symbols_from_expr(expr, symbols);
            collect_symbols_from_expr(label, symbols);
        }
        Statement::PlotCandlestick { symbol, title } => {
            collect_symbols_from_expr(symbol, symbols);
            collect_symbols_from_expr(title, symbols);
        }
        Statement::PlotLine { value, label, color } => {
            collect_symbols_from_expr(value, symbols);
            collect_symbols_from_expr(label, symbols);
            if let Some(c) = color { collect_symbols_from_expr(c, symbols); }
        }
        Statement::PlotHistogram { value, label, color_up, color_down } => {
            collect_symbols_from_expr(value, symbols);
            collect_symbols_from_expr(label, symbols);
            if let Some(c) = color_up { collect_symbols_from_expr(c, symbols); }
            if let Some(c) = color_down { collect_symbols_from_expr(c, symbols); }
        }
        Statement::PlotShape { condition, shape, location, color, text } => {
            collect_symbols_from_expr(condition, symbols);
            collect_symbols_from_expr(shape, symbols);
            collect_symbols_from_expr(location, symbols);
            if let Some(c) = color { collect_symbols_from_expr(c, symbols); }
            if let Some(t) = text { collect_symbols_from_expr(t, symbols); }
        }
        Statement::Bgcolor { color, condition } => {
            collect_symbols_from_expr(color, symbols);
            if let Some(c) = condition { collect_symbols_from_expr(c, symbols); }
        }
        Statement::Hline { value, label, color } => {
            collect_symbols_from_expr(value, symbols);
            if let Some(l) = label { collect_symbols_from_expr(l, symbols); }
            if let Some(c) = color { collect_symbols_from_expr(c, symbols); }
        }
        Statement::SwitchBlock { expr, cases, default } => {
            if let Some(e) = expr { collect_symbols_from_expr(e, symbols); }
            for (case_expr, body) in cases {
                collect_symbols_from_expr(case_expr, symbols);
                for s in body { collect_symbols_from_statement(s, symbols); }
            }
            if let Some(def) = default {
                for s in def { collect_symbols_from_statement(s, symbols); }
            }
        }
        Statement::StrategyEntry { id, direction, qty, price, stop, limit } => {
            collect_symbols_from_expr(id, symbols);
            collect_symbols_from_expr(direction, symbols);
            if let Some(q) = qty { collect_symbols_from_expr(q, symbols); }
            if let Some(p) = price { collect_symbols_from_expr(p, symbols); }
            if let Some(s) = stop { collect_symbols_from_expr(s, symbols); }
            if let Some(l) = limit { collect_symbols_from_expr(l, symbols); }
        }
        Statement::StrategyExit { id, from_entry, qty, stop, limit, trail_points, trail_offset } => {
            collect_symbols_from_expr(id, symbols);
            if let Some(f) = from_entry { collect_symbols_from_expr(f, symbols); }
            if let Some(q) = qty { collect_symbols_from_expr(q, symbols); }
            if let Some(s) = stop { collect_symbols_from_expr(s, symbols); }
            if let Some(l) = limit { collect_symbols_from_expr(l, symbols); }
            if let Some(tp) = trail_points { collect_symbols_from_expr(tp, symbols); }
            if let Some(to) = trail_offset { collect_symbols_from_expr(to, symbols); }
        }
        Statement::StrategyClose { id } => {
            collect_symbols_from_expr(id, symbols);
        }
        Statement::InputDecl { default_value, title, .. } => {
            collect_symbols_from_expr(default_value, symbols);
            if let Some(t) = title { collect_symbols_from_expr(t, symbols); }
        }
        Statement::StructDef { .. } => {}
        Statement::AlertStatement { message, alert_type } => {
            collect_symbols_from_expr(message, symbols);
            if let Some(at) = alert_type { collect_symbols_from_expr(at, symbols); }
        }
        Statement::PrintStatement { args } => {
            for arg in args { collect_symbols_from_expr(arg, symbols); }
        }
        Statement::ImportStatement { .. } => {}
        Statement::ExportStatement { .. } => {}
        Statement::ExprStatement(expr) => collect_symbols_from_expr(expr, symbols),
        Statement::Comment(_) | Statement::Break | Statement::Continue => {}
    }
}

fn collect_symbols_from_expr(expr: &Expr, symbols: &mut Vec<String>) {
    match expr {
        Expr::Symbol(s) => {
            if !symbols.contains(s) { symbols.push(s.clone()); }
        }
        Expr::BinaryOp { left, right, .. } => {
            collect_symbols_from_expr(left, symbols);
            collect_symbols_from_expr(right, symbols);
        }
        Expr::UnaryOp { operand, .. } => {
            collect_symbols_from_expr(operand, symbols);
        }
        Expr::FunctionCall { args, .. } => {
            for arg in args { collect_symbols_from_expr(arg, symbols); }
        }
        Expr::MethodCall { object, args, .. } => {
            collect_symbols_from_expr(object, symbols);
            for arg in args { collect_symbols_from_expr(arg, symbols); }
        }
        Expr::ArrayLiteral(elements) => {
            for el in elements { collect_symbols_from_expr(el, symbols); }
        }
        Expr::IndexAccess { object, index } => {
            collect_symbols_from_expr(object, symbols);
            collect_symbols_from_expr(index, symbols);
        }
        Expr::Range { start, end } => {
            collect_symbols_from_expr(start, symbols);
            collect_symbols_from_expr(end, symbols);
        }
        Expr::Ternary { condition, then_expr, else_expr } => {
            collect_symbols_from_expr(condition, symbols);
            collect_symbols_from_expr(then_expr, symbols);
            collect_symbols_from_expr(else_expr, symbols);
        }
        Expr::MapLiteral(entries) => {
            for (_, val) in entries { collect_symbols_from_expr(val, symbols); }
        }
        Expr::StructLiteral { fields, .. } => {
            for (_, val) in fields { collect_symbols_from_expr(val, symbols); }
        }
        _ => {}
    }
}
