#pragma once
// StorageSqlConsoleGuard.h — the policy behind Settings → Storage → SQL console.
//
// The console is a read-only inspection tool. Three rules, checked before a
// statement is handed to SQLite:
//   1. Read-only shape: one statement, starting with SELECT / WITH / VALUES /
//      EXPLAIN, or a schema PRAGMA from an allow-list (no "= value" form). The
//      statement also runs on a SQLITE_OPEN_READONLY connection with
//      query_only on, so a gap in this text check still cannot write.
//   2. Credential tables are off limits entirely (secure_credentials, the
//      legacy credentials table, ws_provider_configs).
//   3. Tables with a secret COLUMN (llm_profiles.api_key, mcp_servers.env, …)
//      stay queryable, but not with "*" and not by naming that column. The
//      repositories move those values into SecureStorage, yet keep them in the
//      column when the encrypted store fails and on legacy installs.
// Values that slip through as data (key/value rows, JSON blobs) are masked at
// render time by SettingsSecretMask.h.
//
// Pure functions over the SQL text (no widgets, no database) so the policy can
// be read and tested on its own. Every symbol lives in this namespace.

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

namespace fincept::storage_sql_guard {

struct Token {
    enum class Kind { Word, QuotedIdent, String, Number, Punct };
    Kind kind = Kind::Punct;
    QString text; ///< Word: as written. QuotedIdent / String: unescaped content. Punct: the character.
};

inline bool tok_is_punct(const Token& t, QChar c) {
    return t.kind == Token::Kind::Punct && t.text.size() == 1 && t.text.at(0) == c;
}

inline bool ident_start(QChar c) {
    return c.isLetter() || c == QLatin1Char('_') || c.unicode() > 0x7F;
}

inline bool ident_char(QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$') || c.unicode() > 0x7F;
}

/// Read a quoted run starting at `i` (the opening quote). `close` ends it;
/// doubling `close` escapes it (SQLite's only escape inside quotes). Returns the
/// index after the closing quote, or -1 when the run is unterminated.
inline qsizetype read_quoted(const QString& sql, qsizetype i, QChar close, bool doubling, QString& content) {
    const qsizetype n = sql.size();
    ++i; // skip the opening quote
    content.clear();
    while (i < n) {
        const QChar c = sql.at(i);
        if (c == close) {
            if (doubling && i + 1 < n && sql.at(i + 1) == close) {
                content.append(close);
                i += 2;
                continue;
            }
            return i + 1;
        }
        content.append(c);
        ++i;
    }
    return -1;
}

/// Split `sql` into SQLite tokens. Comments and whitespace are dropped (in
/// SQLite a comment is whitespace, so it can never join two halves of a name).
/// Returns false on an unterminated string or quoted identifier.
inline bool tokenize(const QString& sql, QVector<Token>& out) {
    out.clear();
    const qsizetype n = sql.size();
    qsizetype i = 0;
    while (i < n) {
        const QChar c = sql.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('-') && i + 1 < n && sql.at(i + 1) == QLatin1Char('-')) {
            i += 2;
            while (i < n && sql.at(i) != QLatin1Char('\n'))
                ++i;
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < n && sql.at(i + 1) == QLatin1Char('*')) {
            const qsizetype end = sql.indexOf(QLatin1String("*/"), i + 2);
            i = end < 0 ? n : end + 2; // SQLite: an unterminated block comment runs to the end
            continue;
        }
        Token t;
        QString content;
        if ((c == QLatin1Char('x') || c == QLatin1Char('X')) && i + 1 < n && sql.at(i + 1) == QLatin1Char('\'')) {
            const qsizetype next = read_quoted(sql, i + 1, QLatin1Char('\''), true, content); // blob literal
            if (next < 0)
                return false;
            t.kind = Token::Kind::String;
            t.text = content;
            i = next;
        } else if (c == QLatin1Char('\'')) {
            const qsizetype next = read_quoted(sql, i, QLatin1Char('\''), true, content);
            if (next < 0)
                return false;
            t.kind = Token::Kind::String;
            t.text = content;
            i = next;
        } else if (c == QLatin1Char('"') || c == QLatin1Char('`')) {
            const qsizetype next = read_quoted(sql, i, c, true, content);
            if (next < 0)
                return false;
            t.kind = Token::Kind::QuotedIdent;
            t.text = content;
            i = next;
        } else if (c == QLatin1Char('[')) {
            const qsizetype next = read_quoted(sql, i, QLatin1Char(']'), false, content);
            if (next < 0)
                return false;
            t.kind = Token::Kind::QuotedIdent;
            t.text = content;
            i = next;
        } else if (ident_start(c)) {
            const qsizetype start = i;
            while (i < n && ident_char(sql.at(i)))
                ++i;
            t.kind = Token::Kind::Word;
            t.text = sql.mid(start, i - start);
        } else if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && sql.at(i + 1).isDigit())) {
            const qsizetype start = i;
            while (i < n) {
                const QChar d = sql.at(i);
                if (d.isLetterOrNumber() || d == QLatin1Char('.') || d == QLatin1Char('_')) {
                    ++i;
                } else if ((d == QLatin1Char('+') || d == QLatin1Char('-')) && i > start &&
                           (sql.at(i - 1) == QLatin1Char('e') || sql.at(i - 1) == QLatin1Char('E'))) {
                    ++i; // exponent sign: 1e-5
                } else {
                    break;
                }
            }
            t.kind = Token::Kind::Number;
            t.text = sql.mid(start, i - start);
        } else {
            t.kind = Token::Kind::Punct;
            t.text = QString(c);
            ++i;
        }
        out.append(t);
    }
    return true;
}

enum class Verdict {
    Allowed,
    Empty,
    Unparseable,        ///< unterminated string / quoted identifier
    MultipleStatements, ///< a ';' followed by more SQL
    NotReadOnly,        ///< not SELECT / WITH / VALUES / EXPLAIN / PRAGMA, or a WITH that writes
    PragmaNotAllowed,   ///< a PRAGMA outside the read-only allow-list, or one that assigns
    BlockedFunction,    ///< file / extension / raw-page access
    BlockedTable,       ///< a credential table
    SecretColumn,       ///< "*" or a secret column on a table that has one
};

struct Decision {
    Verdict verdict = Verdict::Allowed;
    QString subject; ///< the keyword / pragma / table / column the verdict is about
};

/// Tables whose every row is a credential.
inline const QSet<QString>& blocked_tables() {
    static const QSet<QString> k = {QStringLiteral("secure_credentials"), QStringLiteral("credentials"),
                                    QStringLiteral("ws_provider_configs")};
    return k;
}

/// Tables with columns that hold secrets (plaintext on legacy installs or when
/// SecureStorage was unavailable at save time — see the owning repositories).
inline const QHash<QString, QStringList>& secret_columns() {
    static const QHash<QString, QStringList> k = {
        {QStringLiteral("llm_configs"), {QStringLiteral("api_key")}},
        {QStringLiteral("llm_model_configs"), {QStringLiteral("api_key")}},
        {QStringLiteral("llm_profiles"), {QStringLiteral("api_key")}},
        {QStringLiteral("mcp_servers"), {QStringLiteral("env")}},
        {QStringLiteral("data_sources"), {QStringLiteral("config")}},
        {QStringLiteral("data_source_connections"), {QStringLiteral("config")}},
        {QStringLiteral("workflow_nodes"), {QStringLiteral("credentials"), QStringLiteral("parameters")}},
        {QStringLiteral("data_mappings"), {QStringLiteral("auth_token"), QStringLiteral("headers")}},
    };
    return k;
}

inline Decision check(const QString& sql) {
    QVector<Token> toks;
    if (!tokenize(sql, toks))
        return {Verdict::Unparseable, {}};
    while (!toks.isEmpty() && tok_is_punct(toks.last(), QLatin1Char(';')))
        toks.removeLast(); // trailing semicolons are harmless
    if (toks.isEmpty())
        return {Verdict::Empty, {}};
    for (const auto& t : toks) {
        if (tok_is_punct(t, QLatin1Char(';')))
            return {Verdict::MultipleStatements, {}};
    }

    auto word_upper = [](const Token& t) { return t.kind == Token::Kind::Word ? t.text.toUpper() : QString(); };
    const QString first = word_upper(toks.first());

    static const QSet<QString> kLeading = {QStringLiteral("SELECT"), QStringLiteral("WITH"), QStringLiteral("VALUES"),
                                           QStringLiteral("EXPLAIN"), QStringLiteral("PRAGMA")};
    if (!kLeading.contains(first))
        return {Verdict::NotReadOnly, first.isEmpty() ? toks.first().text : first};

    if (first == QLatin1String("PRAGMA")) {
        // PRAGMA [schema.]name [(argument)] — never "= value", and the
        // parenthesised form only where the argument is a table/index name
        // (PRAGMA journal_mode(DELETE) is a setter).
        static const QSet<QString> kWithArgument = {
            QStringLiteral("table_info"),  QStringLiteral("table_xinfo"),       QStringLiteral("table_list"),
            QStringLiteral("index_list"),  QStringLiteral("index_info"),        QStringLiteral("index_xinfo"),
            QStringLiteral("foreign_key_list"), QStringLiteral("foreign_key_check"), QStringLiteral("integrity_check"),
            QStringLiteral("quick_check")};
        static const QSet<QString> kNoArgument = {
            QStringLiteral("database_list"), QStringLiteral("page_count"),      QStringLiteral("page_size"),
            QStringLiteral("freelist_count"), QStringLiteral("compile_options"), QStringLiteral("collation_list"),
            QStringLiteral("function_list"), QStringLiteral("module_list"),     QStringLiteral("pragma_list"),
            QStringLiteral("encoding"),      QStringLiteral("user_version"),    QStringLiteral("schema_version"),
            QStringLiteral("journal_mode"),  QStringLiteral("data_version")};
        qsizetype k = 1;
        if (k + 2 < toks.size() && tok_is_punct(toks.at(k + 1), QLatin1Char('.')))
            k += 2; // schema prefix: PRAGMA main.table_info(x)
        if (k >= toks.size() || (toks.at(k).kind != Token::Kind::Word && toks.at(k).kind != Token::Kind::QuotedIdent))
            return {Verdict::PragmaNotAllowed, {}};
        const QString name = toks.at(k).text.toLower();
        for (const auto& t : toks) {
            if (tok_is_punct(t, QLatin1Char('=')))
                return {Verdict::PragmaNotAllowed, name};
        }
        const bool has_more = k + 1 < toks.size();
        if (kNoArgument.contains(name)) {
            if (has_more)
                return {Verdict::PragmaNotAllowed, name};
        } else if (kWithArgument.contains(name)) {
            if (has_more && !tok_is_punct(toks.at(k + 1), QLatin1Char('(')))
                return {Verdict::PragmaNotAllowed, name};
        } else {
            return {Verdict::PragmaNotAllowed, name};
        }
    }

    if (first == QLatin1String("WITH")) {
        // A CTE can front a DELETE / INSERT / UPDATE / REPLACE. replace() as a
        // function call is fine.
        for (qsizetype k = 0; k < toks.size(); ++k) {
            const QString w = word_upper(toks.at(k));
            if (w == QLatin1String("INSERT") || w == QLatin1String("UPDATE") || w == QLatin1String("DELETE"))
                return {Verdict::NotReadOnly, w};
            if (w == QLatin1String("REPLACE") &&
                !(k + 1 < toks.size() && tok_is_punct(toks.at(k + 1), QLatin1Char('('))))
                return {Verdict::NotReadOnly, w};
        }
    }

    static const QSet<QString> kBlockedFunctions = {
        QStringLiteral("load_extension"), QStringLiteral("writefile"),    QStringLiteral("readfile"),
        QStringLiteral("edit"),           QStringLiteral("fts3_tokenizer"), QStringLiteral("sqlite_dbpage"),
        QStringLiteral("sqlite_dbdata"),  QStringLiteral("sqlite_dbptr"), QStringLiteral("zipfile"),
        QStringLiteral("fsdir")};

    const auto& blocked = blocked_tables();
    const auto& secret_cols = secret_columns();
    QStringList referenced; // tables with secret columns that the statement names
    for (const auto& t : toks) {
        if (t.kind != Token::Kind::Word && t.kind != Token::Kind::QuotedIdent && t.kind != Token::Kind::String)
            continue;
        const QString lower = t.text.toLower();
        if (t.kind != Token::Kind::String && kBlockedFunctions.contains(lower))
            return {Verdict::BlockedFunction, t.text};
        // String literals count too: SQLite accepts 'name' where only an
        // identifier is valid (FROM 'secure_credentials' reads the table).
        if (blocked.contains(lower))
            return {Verdict::BlockedTable, lower};
        if (secret_cols.contains(lower) && !referenced.contains(lower))
            referenced.append(lower);
    }
    if (referenced.isEmpty())
        return {Verdict::Allowed, {}};

    // "*" selects the secret column without naming it. count(*) is harmless.
    for (qsizetype k = 0; k < toks.size(); ++k) {
        if (!tok_is_punct(toks.at(k), QLatin1Char('*')))
            continue;
        const bool count_star = k >= 2 && tok_is_punct(toks.at(k - 1), QLatin1Char('(')) &&
                                toks.at(k - 2).kind == Token::Kind::Word &&
                                toks.at(k - 2).text.compare(QLatin1String("count"), Qt::CaseInsensitive) == 0;
        if (!count_star)
            return {Verdict::SecretColumn, referenced.first() + QStringLiteral(".*")};
    }
    for (const auto& table : std::as_const(referenced)) {
        const QStringList& cols = secret_cols.value(table);
        for (const auto& t : toks) {
            if (t.kind == Token::Kind::Number || t.kind == Token::Kind::Punct)
                continue;
            if (cols.contains(t.text.toLower()))
                return {Verdict::SecretColumn, table + QLatin1Char('.') + t.text.toLower()};
        }
    }
    return {Verdict::Allowed, {}};
}

} // namespace fincept::storage_sql_guard
