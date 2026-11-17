// AiChatTypes.cpp — see AiChatTypes.h. Pure helpers (QtCore); the only exceptions are
// turn_speaker() (HarnessRegistry display names) and the saved-prompt accessors
// (SettingsRepository), both GUI thread.

#include "screens/ai_chat/AiChatTypes.h"

#include "services/harness/HarnessRegistry.h"
#include "storage/repositories/SettingsRepository.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace fincept::screens::aichat {

namespace {

QString ty_tr(const char* text) {
    return QCoreApplication::translate("AiChat", text);
}

const QString& ty_trailer_prefix() {
    static const QString p = QStringLiteral("<!--fincept-tools:");
    return p;
}

constexpr int kTyMaxPersistedSteps = 300;

QString ty_esc(const QString& s) {
    return s.toHtmlEscaped();
}

// ── Inline Markdown ─────────────────────────────────────────────────────────

struct TyInlineCtx {
    const ParseOptions* opt = nullptr;
    QStringList* tickers = nullptr;
    QVector<SourceLink>* sources = nullptr;
    int depth = 0;
};

void ty_note_ticker(TyInlineCtx& c, const QString& sym) {
    if (c.tickers && !c.tickers->contains(sym))
        c.tickers->append(sym);
}

void ty_note_source(TyInlineCtx& c, const QString& url, const QString& title) {
    if (!c.sources)
        return;
    const QString u = url.trimmed();
    if (!u.startsWith(QLatin1String("http"), Qt::CaseInsensitive))
        return;
    for (const auto& s : std::as_const(*c.sources))
        if (s.url == u)
            return;
    SourceLink l;
    l.number = int(c.sources->size()) + 1;
    l.url = u;
    l.title = title;
    QString host = QUrl(u).host();
    if (host.startsWith(QLatin1String("www.")))
        host = host.mid(4);
    l.host = host.isEmpty() ? u : host;
    c.sources->append(l);
}

QString ty_chip(TyInlineCtx& c, const QString& sym, const QString& shown) {
    ty_note_ticker(c, sym);
    return QStringLiteral("<a class=\"tk\" href=\"fsym:") + ty_esc(sym) + QStringLiteral("\">") + ty_esc(shown) +
           QStringLiteral("</a>");
}

QString ty_link(TyInlineCtx& c, const QString& url, const QString& inner_html, const QString& title) {
    ty_note_source(c, url, title);
    return QStringLiteral("<a class=\"ln\" href=\"") + ty_esc(url) + QStringLiteral("\">") + inner_html +
           QStringLiteral("</a>");
}

bool ty_exchange_suffix(const QString& word) {
    static const QRegularExpression rx(
        QStringLiteral("^[A-Z0-9&\\-]{1,15}\\.(NS|BO|L|T|HK|TO|V|AX|DE|F|PA|SW|MI|AS|BR|MC|ST|OL|CO|HE|KS|KQ|SS|SZ|"
                       "TW|SI|JK|BK|KL|SA|MX|IR|JO|TA|NZ|WA|IS)$"));
    return rx.match(word).hasMatch();
}

// Plain text (no Markdown syntax left): escape it, wrap figures and recognised tickers,
// turn bare URLs into links.
QString ty_plain(TyInlineCtx& c, const QString& raw) {
    static const QRegularExpression rx(QStringLiteral(
        "(?<url>https?://[^\\s<>\"'`]+)"
        "|(?<dollar>\\$(?<dsym>[A-Z]{1,6}(?:\\.[A-Z]{1,3})?)(?![A-Za-z0-9]))"
        "|(?<exch>\\b(?<ex>NASDAQ|NYSE|NYSEARCA|AMEX|NSE|BSE|LSE|TSX|ASX|HKEX|XETRA|TSE|SGX|BATS)\\s?:\\s?"
        "(?<esym>[A-Z][A-Z0-9.\\-]{0,11})(?![A-Za-z0-9]))"
        "|(?<word>(?<![A-Za-z0-9.$^])[A-Z][A-Z0-9&]{0,11}(?:[.\\-][A-Z0-9]{1,4})?(?![A-Za-z0-9]))"
        "|(?<num>(?<![\\w.,])[+\\-\\x{2212}]?[$\\x{20AC}\\x{00A3}\\x{00A5}\\x{20B9}]?(?:\\d{1,3}(?:,\\d{3})+|\\d+)"
        "(?:\\.\\d+)?(?:%|x(?![A-Za-z])|bps(?![A-Za-z])|bp(?![A-Za-z])|[KMBT](?![A-Za-z]))?)"));
    const bool tickers = !c.opt || c.opt->tickers;
    const bool figures = !c.opt || c.opt->figures;
    QString out;
    out.reserve(raw.size() + raw.size() / 4);
    qsizetype pos = 0;
    auto it = rx.globalMatch(raw);
    while (it.hasNext()) {
        const auto m = it.next();
        const qsizetype s = m.capturedStart();
        const qsizetype e = m.capturedEnd();
        QString piece;
        if (m.capturedLength(QStringLiteral("url")) > 0) {
            QString url = m.captured(QStringLiteral("url"));
            qsizetype trail = 0; // trailing sentence punctuation is not part of the link
            while (!url.isEmpty() && QStringLiteral(".,;:!?)]").contains(url.back())) {
                url.chop(1);
                ++trail;
            }
            if (is_safe_link(url))
                piece = ty_link(c, url, ty_esc(url), QString());
            else
                piece = ty_esc(url);
            out += ty_esc(raw.mid(pos, s - pos)) + piece;
            pos = e - trail;
            continue;
        }
        if (tickers && m.capturedLength(QStringLiteral("dollar")) > 0) {
            const QString sym = m.captured(QStringLiteral("dsym"));
            piece = ty_chip(c, sym, sym);
        } else if (tickers && m.capturedLength(QStringLiteral("exch")) > 0) {
            const QString sym = m.captured(QStringLiteral("esym"));
            piece = ty_esc(m.captured(QStringLiteral("ex")) + QStringLiteral(": ")) + ty_chip(c, sym, sym);
        } else if (m.capturedLength(QStringLiteral("word")) > 0) {
            const QString w = m.captured(QStringLiteral("word"));
            const bool known = c.opt && c.opt->known_symbols.contains(w);
            if (tickers && (known || ty_exchange_suffix(w)))
                piece = ty_chip(c, w, w);
            else
                piece = ty_esc(w);
        } else if (figures && m.capturedLength(QStringLiteral("num")) > 0) {
            const QString n = m.captured(QStringLiteral("num"));
            const QChar first = n.at(0);
            const bool signed_move = n.endsWith(QLatin1Char('%')) &&
                                     (first == QLatin1Char('+') || first == QLatin1Char('-') || first == QChar(0x2212));
            const QString cls = !signed_move ? QStringLiteral("n")
                                : first == QLatin1Char('+') ? QStringLiteral("up")
                                                             : QStringLiteral("dn");
            piece = QStringLiteral("<span class=\"") + cls + QStringLiteral("\">") + ty_esc(n) + QStringLiteral("</span>");
        } else {
            piece = ty_esc(m.captured(0));
        }
        out += ty_esc(raw.mid(pos, s - pos)) + piece;
        pos = e;
    }
    out += ty_esc(raw.mid(pos));
    return out;
}

bool ty_is_punct(QChar ch) {
    return ch.isPunct() || ch.isSymbol();
}

// Index of the closing delimiter `d` for an opener at `open` (delimiter already skipped),
// or -1. A closer is preceded by a non-space; '_' closers must not be inside a word.
qsizetype ty_find_closer(const QString& t, qsizetype from, const QString& d) {
    qsizetype i = from;
    while (true) {
        i = t.indexOf(d, i);
        if (i < 0)
            return -1;
        if (i > from && !t.at(i - 1).isSpace()) {
            const qsizetype after = i + d.size();
            const bool word_after = after < t.size() && t.at(after).isLetterOrNumber();
            if (!(d.startsWith(QLatin1Char('_')) && word_after))
                return i;
        }
        i += d.size();
    }
}

QString ty_inline(TyInlineCtx& c, const QString& t);

QString ty_inline(TyInlineCtx& c, const QString& t) {
    if (c.depth > 12)
        return ty_esc(t);
    ++c.depth;
    QString out;
    QString plain; // pending plain text (flushed through ty_plain)
    auto flush = [&]() {
        if (!plain.isEmpty()) {
            out += ty_plain(c, plain);
            plain.clear();
        }
    };
    const qsizetype n = t.size();
    qsizetype i = 0;
    while (i < n) {
        const QChar ch = t.at(i);
        // Line break inside a paragraph / item.
        if (ch == QLatin1Char('\n')) {
            flush();
            out += QStringLiteral("<br>");
            ++i;
            continue;
        }
        // Backslash escape of punctuation.
        if (ch == QLatin1Char('\\') && i + 1 < n) {
            const QChar nx = t.at(i + 1);
            if (nx == QLatin1Char('(')) { // \( inline math \)
                const qsizetype close = t.indexOf(QStringLiteral("\\)"), i + 2);
                if (close > i + 2) {
                    flush();
                    out += QStringLiteral("<span class=\"m\">") + ty_esc(t.mid(i + 2, close - i - 2).trimmed()) +
                           QStringLiteral("</span>");
                    i = close + 2;
                    continue;
                }
            }
            if (ty_is_punct(nx)) {
                plain += nx;
                i += 2;
                continue;
            }
        }
        // Code span.
        if (ch == QLatin1Char('`')) {
            qsizetype run = 0;
            while (i + run < n && t.at(i + run) == QLatin1Char('`'))
                ++run;
            const QString fence(run, QLatin1Char('`'));
            qsizetype close = t.indexOf(fence, i + run);
            while (close >= 0 && close + run < n && t.at(close + run) == QLatin1Char('`'))
                close = t.indexOf(fence, close + run + 1);
            if (close > i) {
                QString code = t.mid(i + run, close - i - run);
                if (code.size() >= 2 && code.startsWith(QLatin1Char(' ')) && code.endsWith(QLatin1Char(' ')))
                    code = code.mid(1, code.size() - 2);
                flush();
                out += QStringLiteral("<code>") + ty_esc(code) + QStringLiteral("</code>");
                i = close + run;
                continue;
            }
            plain += fence;
            i += run;
            continue;
        }
        // $$ inline math $$
        if (ch == QLatin1Char('$') && i + 1 < n && t.at(i + 1) == QLatin1Char('$')) {
            const qsizetype close = t.indexOf(QStringLiteral("$$"), i + 2);
            if (close > i + 2) {
                flush();
                out += QStringLiteral("<span class=\"m\">") + ty_esc(t.mid(i + 2, close - i - 2).trimmed()) +
                       QStringLiteral("</span>");
                i = close + 2;
                continue;
            }
        }
        // Image ![alt](url) — never loaded; shown as a bracketed caption.
        if (ch == QLatin1Char('!') && i + 1 < n && t.at(i + 1) == QLatin1Char('[')) {
            const qsizetype rb = t.indexOf(QLatin1Char(']'), i + 2);
            if (rb > 0 && rb + 1 < n && t.at(rb + 1) == QLatin1Char('(')) {
                const qsizetype rp = t.indexOf(QLatin1Char(')'), rb + 2);
                if (rp > 0) {
                    flush();
                    const QString alt = t.mid(i + 2, rb - i - 2);
                    out += ty_esc(QStringLiteral("[") + ty_tr("image") +
                                  (alt.isEmpty() ? QString() : QStringLiteral(": ") + alt) + QStringLiteral("]"));
                    i = rp + 1;
                    continue;
                }
            }
        }
        // Link [text](url "title")
        if (ch == QLatin1Char('[')) {
            qsizetype depth = 1, j = i + 1;
            while (j < n && depth > 0) {
                if (t.at(j) == QLatin1Char('\\')) {
                    j += 2;
                    continue;
                }
                if (t.at(j) == QLatin1Char('['))
                    ++depth;
                else if (t.at(j) == QLatin1Char(']'))
                    --depth;
                if (depth > 0)
                    ++j;
            }
            if (j < n && depth == 0 && j + 1 < n && t.at(j + 1) == QLatin1Char('(')) {
                qsizetype k = j + 2, pdepth = 1;
                while (k < n && pdepth > 0) {
                    if (t.at(k) == QLatin1Char('('))
                        ++pdepth;
                    else if (t.at(k) == QLatin1Char(')'))
                        --pdepth;
                    if (pdepth > 0)
                        ++k;
                }
                if (k < n && pdepth == 0) {
                    const QString text = t.mid(i + 1, j - i - 1);
                    QString target = t.mid(j + 2, k - j - 2).trimmed();
                    QString title;
                    const qsizetype sp = target.indexOf(QLatin1Char(' '));
                    if (sp > 0) {
                        title = target.mid(sp + 1).trimmed();
                        if (title.size() >= 2 && (title.startsWith(QLatin1Char('"')) || title.startsWith(QLatin1Char('\''))))
                            title = title.mid(1, title.size() - 2);
                        target = target.left(sp);
                    }
                    if (target.startsWith(QLatin1Char('<')) && target.endsWith(QLatin1Char('>')))
                        target = target.mid(1, target.size() - 2);
                    flush();
                    // Link text as plain escaped text: a ticker chip or a bare URL inside it
                    // would nest one anchor in another.
                    const QString label = strip_inline_markdown(text);
                    if (is_safe_link(target))
                        out += ty_link(c, target, ty_esc(label), label);
                    else
                        out += ty_inline(c, text);
                    i = k + 1;
                    continue;
                }
            }
        }
        // Autolink <https://…>
        if (ch == QLatin1Char('<')) {
            const qsizetype close = t.indexOf(QLatin1Char('>'), i + 1);
            if (close > i + 1) {
                const QString inner = t.mid(i + 1, close - i - 1);
                if (!inner.contains(QLatin1Char(' ')) && is_safe_link(inner)) {
                    flush();
                    out += ty_link(c, inner, ty_esc(inner), QString());
                    i = close + 1;
                    continue;
                }
                // "<br>" in model output: a line break; any other tag is shown as text.
                const QString tag = inner.trimmed().toLower();
                if (tag == QLatin1String("br") || tag == QLatin1String("br/") || tag == QLatin1String("br /")) {
                    flush();
                    out += QStringLiteral("<br>");
                    i = close + 1;
                    continue;
                }
            }
        }
        // Emphasis / strike.
        if (ch == QLatin1Char('*') || ch == QLatin1Char('_') || ch == QLatin1Char('~')) {
            const bool dbl = i + 1 < n && t.at(i + 1) == ch;
            if (ch == QLatin1Char('~') && !dbl) {
                plain += ch;
                ++i;
                continue;
            }
            const QString d = dbl ? QString(2, ch) : QString(ch);
            const qsizetype open_end = i + d.size();
            const bool prev_word = i > 0 && t.at(i - 1).isLetterOrNumber();
            const bool opens = open_end < n && !t.at(open_end).isSpace() && !(ch == QLatin1Char('_') && prev_word);
            if (opens) {
                const qsizetype close = ty_find_closer(t, open_end, d);
                if (close > open_end) {
                    flush();
                    const QString inner = ty_inline(c, t.mid(open_end, close - open_end));
                    const QString tag = ch == QLatin1Char('~') ? QStringLiteral("s")
                                        : dbl                  ? QStringLiteral("b")
                                                               : QStringLiteral("i");
                    out += QStringLiteral("<") + tag + QStringLiteral(">") + inner + QStringLiteral("</") + tag +
                           QStringLiteral(">");
                    i = close + d.size();
                    continue;
                }
            }
            plain += d;
            i += d.size();
            continue;
        }
        plain += ch;
        ++i;
    }
    flush();
    --c.depth;
    return out;
}

// ── Block helpers ───────────────────────────────────────────────────────────

struct TyLine {
    QString text;
    int begin = 0; // offset of the line in the source
    int end = 0;   // offset after the line (its '\n' included)
};

bool ty_blank(const QString& l) {
    return l.trimmed().isEmpty();
}

int ty_indent(const QString& l) {
    int n = 0;
    for (const QChar ch : l) {
        if (ch == QLatin1Char(' '))
            ++n;
        else if (ch == QLatin1Char('\t'))
            n += 4;
        else
            break;
    }
    return n;
}

const QRegularExpression& ty_rx_fence() {
    static const QRegularExpression rx(QStringLiteral("^ {0,3}(`{3,}|~{3,})\\s*([^`\\s]*)[^`]*$"));
    return rx;
}
const QRegularExpression& ty_rx_heading() {
    static const QRegularExpression rx(QStringLiteral("^ {0,3}(#{1,6})(?:\\s+(.*?))?\\s*#*\\s*$"));
    return rx;
}
const QRegularExpression& ty_rx_rule() {
    static const QRegularExpression rx(QStringLiteral("^ {0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
    return rx;
}
const QRegularExpression& ty_rx_item() {
    static const QRegularExpression rx(QStringLiteral("^(\\s*)([-*+\\x{2022}]|\\d{1,9}[.)])\\s+(.*)$"));
    return rx;
}
const QRegularExpression& ty_rx_table_sep() {
    static const QRegularExpression rx(
        QStringLiteral("^\\s*\\|?\\s*:?-{2,}:?\\s*(\\|\\s*:?-{2,}:?\\s*)*\\|?\\s*$"));
    return rx;
}
const QRegularExpression& ty_rx_math_open() {
    static const QRegularExpression rx(QStringLiteral("^ {0,3}(\\$\\$|\\\\\\[)\\s*$"));
    return rx;
}

QStringList ty_split_row(const QString& line) {
    QString l = line.trimmed();
    if (l.startsWith(QLatin1Char('|')))
        l.remove(0, 1);
    if (l.endsWith(QLatin1Char('|')) && !l.endsWith(QStringLiteral("\\|")))
        l.chop(1);
    QStringList cells;
    QString cur;
    bool in_code = false;
    for (qsizetype i = 0; i < l.size(); ++i) {
        const QChar ch = l.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < l.size() && l.at(i + 1) == QLatin1Char('|')) {
            cur += QLatin1Char('|');
            ++i;
            continue;
        }
        if (ch == QLatin1Char('`'))
            in_code = !in_code;
        if (ch == QLatin1Char('|') && !in_code) {
            cells << cur.trimmed();
            cur.clear();
            continue;
        }
        cur += ch;
    }
    cells << cur.trimmed();
    return cells;
}

bool ty_starts_block(const QString& l, const QString& next) {
    if (ty_rx_fence().match(l).hasMatch() || ty_rx_heading().match(l).hasMatch() || ty_rx_rule().match(l).hasMatch() ||
        ty_rx_math_open().match(l).hasMatch())
        return true;
    if (l.trimmed().startsWith(QLatin1Char('>')))
        return true;
    if (ty_rx_item().match(l).hasMatch())
        return true;
    if (l.contains(QLatin1Char('|')) && ty_rx_table_sep().match(next).hasMatch())
        return true;
    return false;
}

QString ty_cell_plain(const QString& cell) {
    QString c = cell;
    static const QRegularExpression br(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    c.replace(br, QStringLiteral(" "));
    return strip_inline_markdown(c).simplified();
}

void ty_finish_table(TableData& t) {
    const int cols = int(t.header.size());
    for (auto& r : t.rows) {
        while (r.size() < cols)
            r.append(QString());
        if (r.size() > cols)
            r = r.mid(0, cols);
    }
    t.numeric = QVector<bool>(cols, false);
    if (t.align.size() < cols)
        t.align.resize(cols);
    static const QRegularExpression time_hdr(
        QStringLiteral("\\b(date|time|year|yr|period|quarter|qtr|month|day|week|fy|fiscal)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    for (int c = 0; c < cols; ++c) {
        int filled = 0, figures = 0, periods = 0;
        for (const auto& r : std::as_const(t.rows)) {
            const QString v = r.at(c).trimmed();
            if (v.isEmpty() || v == QLatin1String("-") || v == QStringLiteral("\u2014") ||
                v.compare(QLatin1String("n/a"), Qt::CaseInsensitive) == 0)
                continue;
            ++filled;
            double d = 0;
            if (parse_figure(v, &d))
                ++figures;
            if (looks_like_period(v))
                ++periods;
        }
        const bool periodic = filled > 0 && (periods * 10 >= filled * 7 ||
                                             (time_hdr.match(t.header.value(c)).hasMatch() && periods * 2 >= filled));
        if (periodic && t.time_col < 0) {
            t.time_col = c;
            continue;
        }
        t.numeric[c] = filled > 0 && figures * 10 >= filled * 6;
        if (t.numeric[c] && t.align[c] == 0)
            t.align[c] = 2;
    }
    for (int c = 0; c < cols; ++c) {
        if (!t.numeric[c] && c != t.time_col) {
            t.label_col = c;
            break;
        }
    }
    bool any_numeric = false;
    for (int c = 0; c < cols; ++c)
        any_numeric = any_numeric || (t.numeric[c] && c != t.time_col);
    t.chartable = t.rows.size() >= 3 && (t.label_col >= 0 || t.time_col >= 0) && any_numeric;
}

void ty_detect_series(Block& b, const QStringList& raw_items) {
    if (raw_items.size() < 3 || raw_items.size() > 60)
        return;
    static const QRegularExpression rx(QStringLiteral(
        "^(.{1,48}?)\\s*(?::|\\x{2014}|\\x{2013}|=|\\s-)\\s*([+\\-\\x{2212}]?[$\\x{20AC}\\x{00A3}\\x{00A5}\\x{20B9}]?"
        "[\\d][\\d,]*(?:\\.\\d+)?\\s*(?:%|x|bps|[KMBT]|bn|mn)?)(?:\\s.*)?$"));
    NumericSeries s;
    QString unit;
    for (const QString& raw : raw_items) {
        const auto m = rx.match(strip_inline_markdown(raw).trimmed());
        if (!m.hasMatch())
            return;
        double v = 0;
        const QString fig = m.captured(2).trimmed();
        if (!parse_figure(fig, &v))
            return;
        const QString u = fig.endsWith(QLatin1Char('%')) ? QStringLiteral("%")
                          : fig.contains(QLatin1Char('$')) ? QStringLiteral("$")
                                                           : QString();
        if (s.values.isEmpty())
            unit = u;
        else if (u != unit)
            return; // mixed units are not one series
        s.labels << m.captured(1).trimmed();
        s.values << v;
    }
    s.unit = unit;
    if (s.valid())
        b.series = s;
}

} // namespace

// ── Tool activity ─────────────────────────────────────────────────────────────

QVector<ToolRow> tool_rows(const QVector<ToolStep>& steps) {
    QVector<ToolRow> rows;
    int i = 0;
    const int n = int(steps.size());
    while (i < n) {
        int j = i + 1;
        while (j < n && steps[j].base == steps[i].base)
            ++j;
        ToolRow r;
        r.tool = steps[i].base;
        r.count = j - i;
        r.args = r.count == 1 ? steps[i].label.section(QStringLiteral(" · "), 1).trimmed() : QString();
        QString first_failure, note;
        for (int k = i; k < j; ++k) {
            const auto& s = steps[k];
            switch (s.state) {
                case ToolStep::State::Failed:
                    ++r.failed;
                    if (first_failure.isEmpty())
                        first_failure = s.detail;
                    break;
                case ToolStep::State::Running:
                    ++r.running;
                    break;
                case ToolStep::State::Job:
                    ++r.jobs;
                    r.job_id = s.detail;
                    break;
                case ToolStep::State::Ok:
                    if (!s.detail.isEmpty() && note.isEmpty())
                        note = s.detail;
                    break;
            }
            if (s.elapsed_ms >= 0) {
                r.timed = true;
                r.parallel = r.parallel || s.parallel;
                // Calls of one parallel round each report the round's wall time.
                r.ms = s.parallel ? std::max(r.ms, s.elapsed_ms) : r.ms + s.elapsed_ms;
            }
        }
        r.state = r.running > 0  ? ToolStep::State::Running
                  : r.failed > 0 ? ToolStep::State::Failed
                  : r.jobs > 0   ? ToolStep::State::Job
                                 : ToolStep::State::Ok;
        r.detail = r.failed > 0 ? (first_failure.isEmpty() ? ty_tr("no reason given") : first_failure)
                   : r.jobs > 0 ? r.job_id
                                : note;
        rows.append(r);
        i = j;
    }
    return rows;
}

ToolSummary summarize_tools(const QVector<ToolStep>& steps) {
    ToolSummary s;
    s.calls = int(steps.size());
    qint64 round_max = 0;
    for (const auto& st : steps) {
        if (st.state == ToolStep::State::Failed)
            ++s.failed;
        else if (st.state == ToolStep::State::Job)
            ++s.jobs;
        else if (st.state == ToolStep::State::Running)
            ++s.running;
        if (st.elapsed_ms >= 0) {
            s.timed = true;
            if (st.parallel) {
                round_max = std::max(round_max, st.elapsed_ms);
            } else {
                s.ms += round_max + st.elapsed_ms;
                round_max = 0;
            }
        }
    }
    s.ms += round_max;
    return s;
}

QJsonArray tool_steps_to_json(const QVector<ToolStep>& steps) {
    QJsonArray arr;
    for (const auto& s : steps) {
        QString state = QStringLiteral("done");
        if (s.state == ToolStep::State::Ok)
            state = s.elapsed_ms >= 0 ? QStringLiteral("ok") : QStringLiteral("done");
        else if (s.state == ToolStep::State::Failed)
            state = QStringLiteral("fail");
        else if (s.state == ToolStep::State::Job)
            state = QStringLiteral("job");
        QJsonObject o{{QStringLiteral("l"), s.label}, {QStringLiteral("s"), state}};
        if (s.elapsed_ms >= 0)
            o[QStringLiteral("ms")] = double(s.elapsed_ms);
        if (s.parallel)
            o[QStringLiteral("p")] = true;
        if (!s.detail.isEmpty())
            o[QStringLiteral("d")] = s.detail;
        arr.append(o);
    }
    return arr;
}

QVector<ToolStep> tool_steps_from_json(const QJsonArray& steps) {
    QVector<ToolStep> list;
    list.reserve(steps.size());
    for (const auto& v : steps) {
        const QJsonObject o = v.toObject();
        ToolStep s;
        s.label = o.value(QStringLiteral("l")).toString();
        s.base = s.label.section(QStringLiteral(" · "), 0, 0);
        const QString st = o.value(QStringLiteral("s")).toString();
        s.state = st == QLatin1String("fail")  ? ToolStep::State::Failed
                  : st == QLatin1String("job") ? ToolStep::State::Job
                                               : ToolStep::State::Ok;
        s.elapsed_ms = o.contains(QStringLiteral("ms")) ? qint64(o.value(QStringLiteral("ms")).toDouble()) : -1;
        s.parallel = o.value(QStringLiteral("p")).toBool();
        s.detail = o.value(QStringLiteral("d")).toString();
        list.append(s);
    }
    return list;
}

QString tool_trailer(const QVector<ToolStep>& steps) {
    if (steps.isEmpty())
        return {};
    const int start = std::max(0, int(steps.size()) - kTyMaxPersistedSteps);
    const QJsonArray arr = tool_steps_to_json(steps.mid(start));
    return QStringLiteral("\n\n") + ty_trailer_prefix() +
           QString::fromLatin1(QJsonDocument(arr).toJson(QJsonDocument::Compact).toBase64()) + QStringLiteral("-->");
}

QString strip_tool_trailer(const QString& content, QVector<ToolStep>* steps) {
    static const QRegularExpression rx(QStringLiteral("\\s*<!--fincept-tools:([A-Za-z0-9+/=]*)-->\\s*$"));
    const auto m = rx.match(content);
    if (!m.hasMatch())
        return content;
    if (steps)
        *steps = tool_steps_from_json(QJsonDocument::fromJson(QByteArray::fromBase64(m.captured(1).toLatin1())).array());
    return content.left(m.capturedStart());
}

QString format_ms(qint64 ms) {
    if (ms < 0)
        return {};
    if (ms < 1000)
        return QStringLiteral("%1 ms").arg(ms);
    if (ms < 60000)
        return QStringLiteral("%1 s").arg(double(ms) / 1000.0, 0, 'f', 1);
    const qint64 sec = (ms % 60000) / 1000;
    return QStringLiteral("%1m %2s").arg(ms / 60000).arg(sec, 2, 10, QLatin1Char('0'));
}

QString format_count(qint64 n) {
    if (n < 0)
        return {};
    if (n < 1000)
        return QString::number(n);
    if (n < 1000000)
        return QStringLiteral("%1k").arg(double(n) / 1000.0, 0, 'f', n < 10000 ? 1 : 0);
    return QStringLiteral("%1M").arg(double(n) / 1000000.0, 0, 'f', 1);
}

// ── Envelope ──────────────────────────────────────────────────────────────────

QString context_lines(const QVector<Attachment>& context) {
    QStringList lines;
    for (const auto& a : context) {
        switch (a.kind) {
            case Attachment::Kind::Symbol: {
                const QString sym = a.ref.isEmpty() ? a.name : a.ref;
                if (sym.isEmpty())
                    break;
                const QString cls = a.detail.isEmpty() ? QStringLiteral("equity") : a.detail;
                // Prompt text (kept in the history), not UI — deliberately not tr().
                lines << QStringLiteral("[Context: ") + sym + QStringLiteral(" (") + cls + QStringLiteral(")]");
                break;
            }
            case Attachment::Kind::Portfolio:
                lines << QStringLiteral("[Context: portfolio \"") + a.name + QStringLiteral("\" (id ") + a.ref +
                             QStringLiteral(")]");
                break;
            case Attachment::Kind::Watchlist:
                lines << QStringLiteral("[Context: watchlist \"") + a.name + QStringLiteral("\" (id ") + a.ref +
                             QStringLiteral(")]");
                break;
            case Attachment::Kind::File:
            case Attachment::Kind::Image:
                break;
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString build_user_wire(const QVector<Attachment>& context, const QVector<FilePayload>& files, const QString& typed) {
    QString text;
    for (const auto& f : files) {
        // A silently clipped file reads as the whole file, so the model is told when it is one.
        const QString header =
            f.truncated ? QStringLiteral("[Attached file: %1 — truncated to the first %2 characters]")
                              .arg(f.name, QString::number(kAttachMaxChars)) // one call: "%2" in a name stays
                        : QStringLiteral("[Attached file: ") + f.name + QStringLiteral("]");
        text += header + QStringLiteral("\n\n") + f.content + QStringLiteral("\n\n---\n\n");
    }
    text += typed;
    const QString ctx = context_lines(context);
    if (!ctx.isEmpty())
        text = ctx + QStringLiteral("\n\n") + text;
    return text;
}

UserEnvelope parse_user_wire(const QString& stored) {
    UserEnvelope env;
    QString s = stored;
    // Context lines ("[Context: …]"), one per line, then a blank line.
    while (s.startsWith(QLatin1String("[Context: "))) {
        const qsizetype nl = s.indexOf(QLatin1Char('\n'));
        const QString line = nl < 0 ? s : s.left(nl);
        if (!line.endsWith(QLatin1Char(']')))
            break;
        const QString inner = line.mid(10, line.size() - 11);
        Attachment a;
        static const QRegularExpression ref_rx(QStringLiteral("^(portfolio|watchlist) \"(.*)\" \\(id (.*)\\)$"));
        const auto rm = ref_rx.match(inner);
        if (rm.hasMatch()) {
            a.kind = rm.captured(1) == QLatin1String("portfolio") ? Attachment::Kind::Portfolio
                                                                    : Attachment::Kind::Watchlist;
            a.name = rm.captured(2);
            a.ref = rm.captured(3);
        } else {
            a.kind = Attachment::Kind::Symbol;
            const qsizetype paren = inner.lastIndexOf(QStringLiteral(" ("));
            a.ref = paren > 0 ? inner.left(paren) : inner;
            a.name = a.ref;
            a.detail = paren > 0 ? inner.mid(paren + 2).chopped(inner.endsWith(QLatin1Char(')')) ? 1 : 0) : QString();
        }
        env.chips.append(a);
        s = nl < 0 ? QString() : s.mid(nl + 1);
        while (s.startsWith(QLatin1Char('\n')))
            s.remove(0, 1);
    }
    // Attached files: header, blank line, content, "\n\n---\n\n".
    static const QString sep = QStringLiteral("\n\n---\n\n");
    static const QString hdr = QStringLiteral("[Attached file: ");
    while (s.startsWith(hdr)) {
        const qsizetype end_hdr = s.indexOf(QStringLiteral("]\n\n"));
        if (end_hdr < 0)
            break;
        const QString head = s.mid(hdr.size(), end_hdr - hdr.size());
        Attachment a;
        a.kind = Attachment::Kind::File;
        const qsizetype trunc = head.indexOf(QStringLiteral(" — truncated to the first "));
        a.name = trunc > 0 ? head.left(trunc) : head;
        a.truncated = trunc > 0;
        const QString rest = s.mid(end_hdr + 3);
        // The next file's header after a separator; otherwise the LAST separator ends the
        // final file (its content may contain "---" lines itself).
        qsizetype cut = rest.indexOf(sep + hdr);
        if (cut < 0)
            cut = rest.lastIndexOf(sep);
        if (cut < 0) {
            a.bytes = rest.size();
            env.chips.append(a);
            s.clear();
            break;
        }
        a.bytes = cut; // characters sent
        env.chips.append(a);
        s = rest.mid(cut + sep.size());
    }
    env.typed = s;
    return env;
}

int estimate_tokens(const QString& text) {
    qint64 cjk = 0, other = 0;
    for (const QChar ch : text) {
        const ushort u = ch.unicode();
        if ((u >= 0x3040 && u <= 0x30FF) || (u >= 0x3400 && u <= 0x9FFF) || (u >= 0xAC00 && u <= 0xD7AF))
            ++cjk;
        else
            ++other;
    }
    return int(cjk + (other + 3) / 4);
}

// ── Turns ─────────────────────────────────────────────────────────────────────

QString turn_speaker(const Turn& t) {
    if (t.role == TurnRole::User)
        return ty_tr("You");
    if (t.role == TurnRole::Notice)
        return ty_tr("Notice");
    if (t.provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
        return ty_tr("Fincept LLM");
    if (!t.route_label.isEmpty())
        return t.route_label;
    if (t.provider.startsWith(QLatin1String("harness:"))) {
        // GUI thread: the registry's display names.
        const QString name = harness::HarnessRegistry::instance().display_name(t.provider.mid(8));
        return t.model.isEmpty() ? name : name + QStringLiteral(" · ") + t.model;
    }
    if (t.provider.isEmpty())
        return t.model.isEmpty() ? ty_tr("AI") : t.model;
    return t.model.isEmpty() ? t.provider : t.provider + QStringLiteral(" / ") + t.model;
}

QString turn_markdown(const Turn& t) {
    QString md = t.text;
    if (t.role == TurnRole::Assistant && t.state == TurnState::Failed && !t.error.isEmpty())
        md += (md.isEmpty() ? QString() : QStringLiteral("\n\n")) + QStringLiteral("**") + ty_tr("Error:") +
              QStringLiteral("** ") + t.error;
    return md.trimmed();
}

// ── Markdown ──────────────────────────────────────────────────────────────────

QString inline_markdown_html(const QString& text, const ParseOptions& options, QStringList* tickers,
                             QVector<SourceLink>* sources) {
    TyInlineCtx c;
    c.opt = &options;
    c.tickers = tickers;
    c.sources = sources;
    return ty_inline(c, text);
}

QString strip_inline_markdown(const QString& text) {
    QString s = text;
    static const QRegularExpression img(QStringLiteral("!\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression link(QStringLiteral("\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression code(QStringLiteral("`+([^`]*)`+"));
    static const QRegularExpression emph(QStringLiteral("(\\*\\*|__|~~)(.+?)\\1"));
    static const QRegularExpression em1(QStringLiteral("(?<![\\w*])[*_](?!\\s)(.+?)(?<!\\s)[*_](?![\\w*])"));
    s.replace(img, QStringLiteral("\\1"));
    s.replace(link, QStringLiteral("\\1"));
    s.replace(code, QStringLiteral("\\1"));
    s.replace(emph, QStringLiteral("\\2"));
    s.replace(em1, QStringLiteral("\\1"));
    static const QRegularExpression esc(QStringLiteral("\\\\([\\\\`*_{}\\[\\]()#+\\-.!|>~$])"));
    s.replace(esc, QStringLiteral("\\1"));
    return s;
}

bool parse_figure(const QString& text, double* value) {
    QString s = text.trimmed();
    if (s.isEmpty())
        return false;
    bool negative = false;
    if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        negative = true;
        s = s.mid(1, s.size() - 2).trimmed();
    }
    s.replace(QChar(0x2212), QLatin1Char('-'));
    if (s.startsWith(QLatin1Char('+')))
        s.remove(0, 1);
    if (s.startsWith(QLatin1Char('-'))) {
        negative = !negative;
        s.remove(0, 1);
    }
    static const QString currency = QStringLiteral("$\u20AC\u00A3\u00A5\u20B9");
    while (!s.isEmpty() && currency.contains(s.front()))
        s.remove(0, 1);
    if (s.startsWith(QLatin1Char('-'))) { // "$-12"
        negative = !negative;
        s.remove(0, 1);
    }
    double mult = 1.0;
    static const QRegularExpression suffix(QStringLiteral("\\s*(%|x|bps|bp|bn|mn|[KMBT])$"),
                                           QRegularExpression::CaseInsensitiveOption);
    const auto sm = suffix.match(s);
    if (sm.hasMatch()) {
        const QString u = sm.captured(1).toUpper();
        if (u == QLatin1String("K"))
            mult = 1e3;
        else if (u == QLatin1String("M") || u == QLatin1String("MN"))
            mult = 1e6;
        else if (u == QLatin1String("B") || u == QLatin1String("BN"))
            mult = 1e9;
        else if (u == QLatin1String("T"))
            mult = 1e12;
        s = s.left(sm.capturedStart());
    }
    static const QRegularExpression num(QStringLiteral("^(\\d{1,3}(,\\d{3})+|\\d+)(\\.\\d+)?$|^\\.\\d+$"));
    if (!num.match(s).hasMatch())
        return false;
    s.remove(QLatin1Char(','));
    bool ok = false;
    double v = s.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    v *= mult;
    if (negative)
        v = -v;
    if (value)
        *value = v;
    return true;
}

bool looks_like_period(const QString& text) {
    static const QRegularExpression rx(
        QStringLiteral("^(?:(?:19|20)\\d{2}|(?:19|20)\\d{2}-\\d{2}(?:-\\d{2})?|\\d{1,2}/\\d{1,2}/\\d{2,4}|"
                       "(?:Q[1-4]|H[12])[\\s'\\-]*(?:\\d{2}|\\d{4})|(?:\\d{4})[\\s\\-]*(?:Q[1-4]|H[12])|"
                       "FY\\s?'?\\d{2,4}|(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Sept|Oct|Nov|Dec)[a-z]*\\.?\\s*'?\\d{2,4}|"
                       "\\d{1,2}\\s(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]*\\s\\d{2,4}|"
                       "(?:19|20)\\d{2}-\\d{2}-\\d{2}[T ]\\d{2}:\\d{2}(?::\\d{2})?)$"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(text.trimmed()).hasMatch();
}

QString table_to_csv(const TableData& table) {
    auto field = [](const QString& v) {
        if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"')) || v.contains(QLatin1Char('\n')) ||
            v != v.trimmed()) {
            QString q = v;
            q.replace(QStringLiteral("\""), QStringLiteral("\"\""));
            return QStringLiteral("\"") + q + QStringLiteral("\"");
        }
        return v;
    };
    QStringList lines;
    QStringList h;
    for (const auto& c : table.header)
        h << field(c);
    lines << h.join(QLatin1Char(','));
    for (const auto& r : table.rows) {
        QStringList l;
        for (const auto& c : r)
            l << field(c);
        lines << l.join(QLatin1Char(','));
    }
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

bool is_safe_link(const QString& url) {
    const QUrl u(url.trimmed(), QUrl::StrictMode);
    if (!u.isValid())
        return false;
    const QString scheme = u.scheme().toLower();
    if (scheme == QLatin1String("mailto"))
        return !u.path().isEmpty();
    return (scheme == QLatin1String("http") || scheme == QLatin1String("https")) && !u.host().isEmpty();
}

bool looks_like_ticker(const QString& word) {
    static const QRegularExpression rx(QStringLiteral("^\\^?[A-Z0-9]{1,12}(?:[.\\-=][A-Z0-9]{1,6})?$"));
    if (!rx.match(word).hasMatch())
        return false;
    for (const QChar ch : word)
        if (ch.isLetter())
            return true;
    return false;
}

ParsedDoc parse_markdown(const QString& markdown, const ParseOptions& options, bool streaming) {
    ParsedDoc doc;
    TyInlineCtx ictx;
    ictx.opt = &options;
    ictx.tickers = &doc.tickers;
    ictx.sources = &doc.sources;

    // Lines with their offsets.
    QVector<TyLine> lines;
    {
        int start = 0;
        const int n = int(markdown.size());
        for (int i = 0; i <= n; ++i) {
            if (i == n || markdown.at(i) == QLatin1Char('\n')) {
                TyLine l;
                l.text = markdown.mid(start, i - start);
                if (l.text.endsWith(QLatin1Char('\r')))
                    l.text.chop(1);
                l.begin = start;
                l.end = i < n ? i + 1 : i;
                if (!(i == n && start == n && !lines.isEmpty()))
                    lines.append(l);
                start = i + 1;
            }
        }
    }
    const int nl = int(lines.size());
    auto line = [&](int i) -> const QString& {
        static const QString empty;
        return i < nl ? lines[i].text : empty;
    };
    auto add = [&](Block b, int first, int last) {
        b.src_begin = lines[first].begin;
        b.src_end = lines[last].end;
        b.source = markdown.mid(b.src_begin, b.src_end - b.src_begin);
        if (b.source.endsWith(QLatin1Char('\n')))
            b.source.chop(1);
        doc.blocks.append(b);
    };

    int i = 0;
    while (i < nl) {
        const QString& l = line(i);
        if (ty_blank(l)) {
            ++i;
            continue;
        }
        // Fenced code
        if (const auto fm = ty_rx_fence().match(l); fm.hasMatch()) {
            const QString fence = fm.captured(1);
            Block b;
            b.kind = BlockKind::Code;
            b.lang = fm.captured(2).toLower();
            QStringList body;
            int j = i + 1;
            bool closed = false;
            for (; j < nl; ++j) {
                const QString tt = line(j).trimmed();
                if (tt.size() >= fence.size() && tt.startsWith(fence) &&
                    tt.count(fence.at(0)) == tt.size()) {
                    closed = true;
                    break;
                }
                body << line(j);
            }
            b.code = body.join(QLatin1Char('\n'));
            b.closed = closed;
            add(b, i, closed ? j : nl - 1);
            i = closed ? j + 1 : nl;
            continue;
        }
        // Display math
        if (const auto mm = ty_rx_math_open().match(l); mm.hasMatch()) {
            const QString closer = mm.captured(1) == QLatin1String("$$") ? QStringLiteral("$$") : QStringLiteral("\\]");
            Block b;
            b.kind = BlockKind::Math;
            QStringList body;
            int j = i + 1;
            bool closed = false;
            for (; j < nl; ++j) {
                if (line(j).trimmed() == closer) {
                    closed = true;
                    break;
                }
                body << line(j);
            }
            b.code = body.join(QLatin1Char('\n')).trimmed();
            b.closed = closed;
            add(b, i, closed ? j : nl - 1);
            i = closed ? j + 1 : nl;
            continue;
        }
        // Heading
        if (const auto hm = ty_rx_heading().match(l); hm.hasMatch()) {
            Block b;
            b.kind = BlockKind::Heading;
            b.level = int(hm.captured(1).size());
            b.html = ty_inline(ictx, hm.captured(2).trimmed());
            add(b, i, i);
            ++i;
            continue;
        }
        // Rule
        if (ty_rx_rule().match(l).hasMatch()) {
            Block b;
            b.kind = BlockKind::Rule;
            add(b, i, i);
            ++i;
            continue;
        }
        // Table
        if (l.contains(QLatin1Char('|')) && i + 1 < nl && ty_rx_table_sep().match(line(i + 1)).hasMatch()) {
            Block b;
            b.kind = BlockKind::Table;
            QStringList hdr = ty_split_row(l);
            for (auto& h : hdr)
                h = ty_cell_plain(h);
            b.table.header = hdr;
            const QStringList seps = ty_split_row(line(i + 1));
            b.table.align = QVector<int>(int(hdr.size()), 0);
            for (int c = 0; c < hdr.size() && c < seps.size(); ++c) {
                const QString sp = seps[c].trimmed();
                const bool left = sp.startsWith(QLatin1Char(':'));
                const bool right = sp.endsWith(QLatin1Char(':'));
                b.table.align[c] = left && right ? 1 : right ? 2 : 0;
            }
            int j = i + 2;
            for (; j < nl; ++j) {
                if (ty_blank(line(j)) || !line(j).contains(QLatin1Char('|')))
                    break;
                QStringList cells = ty_split_row(line(j));
                for (auto& c : cells)
                    c = ty_cell_plain(c);
                b.table.rows.append(cells);
            }
            ty_finish_table(b.table);
            add(b, i, j - 1);
            i = j;
            continue;
        }
        // Block quote
        if (l.trimmed().startsWith(QLatin1Char('>'))) {
            QStringList body;
            int j = i;
            for (; j < nl; ++j) {
                const QString t = line(j).trimmed();
                if (!t.startsWith(QLatin1Char('>')))
                    break;
                QString inner = t.mid(1);
                if (inner.startsWith(QLatin1Char(' ')))
                    inner.remove(0, 1);
                body << inner;
            }
            Block b;
            b.kind = BlockKind::Quote;
            b.html = ty_inline(ictx, body.join(QLatin1Char('\n')).trimmed());
            add(b, i, j - 1);
            i = j;
            continue;
        }
        // List
        if (const auto im = ty_rx_item().match(l); im.hasMatch()) {
            Block b;
            b.kind = BlockKind::List;
            const QString marker = im.captured(2);
            b.ordered = marker.at(0).isDigit();
            if (b.ordered)
                b.start = marker.left(marker.size() - 1).toInt();
            const int base_indent = ty_indent(l);
            QStringList raw_items;
            int j = i;
            int last = i;
            while (j < nl) {
                const QString& lj = line(j);
                if (ty_blank(lj)) {
                    // A blank line continues the list only when another item follows.
                    int k = j + 1;
                    while (k < nl && ty_blank(line(k)))
                        ++k;
                    if (k < nl && ty_rx_item().match(line(k)).hasMatch() && ty_indent(line(k)) >= base_indent) {
                        j = k;
                        continue;
                    }
                    break;
                }
                const auto m = ty_rx_item().match(lj);
                if (m.hasMatch() && ty_indent(lj) >= base_indent) {
                    QString itext = m.captured(3);
                    if (itext.startsWith(QLatin1String("[ ] ")))
                        itext = QStringLiteral("\u2610 ") + itext.mid(4);
                    else if (itext.startsWith(QLatin1String("[x] ")) || itext.startsWith(QLatin1String("[X] ")))
                        itext = QStringLiteral("\u2611 ") + itext.mid(4);
                    raw_items << itext;
                    b.item_levels << std::min(4, (ty_indent(lj) - base_indent) / 2);
                    last = j;
                    ++j;
                    continue;
                }
                if (j > i && !raw_items.isEmpty() &&
                    (ty_indent(lj) > base_indent || !ty_starts_block(lj, line(j + 1)))) {
                    raw_items.last() += QLatin1Char('\n') + lj.trimmed(); // continuation line
                    last = j;
                    ++j;
                    continue;
                }
                break;
            }
            for (const QString& it : std::as_const(raw_items))
                b.items << ty_inline(ictx, it);
            ty_detect_series(b, raw_items);
            add(b, i, last);
            i = last + 1;
            continue;
        }
        // Paragraph
        QStringList body;
        int j = i;
        for (; j < nl; ++j) {
            const QString& lj = line(j);
            if (ty_blank(lj))
                break;
            if (j > i && ty_starts_block(lj, line(j + 1)))
                break;
            body << lj.trimmed();
        }
        Block b;
        b.kind = BlockKind::Paragraph;
        b.html = ty_inline(ictx, body.join(QLatin1Char('\n')));
        add(b, i, j - 1);
        i = j;
    }

    if (streaming && !doc.blocks.isEmpty()) {
        Block& last = doc.blocks.last();
        const bool fenced = last.kind == BlockKind::Code || last.kind == BlockKind::Math;
        // A fenced block is closed by its closing fence (set above). Anything else may still
        // grow — unless a blank line already followed it (src_end is past its last '\n', so
        // any further '\n' means an empty line came after the block).
        if (!fenced)
            last.closed = markdown.mid(last.src_end).contains(QLatin1Char('\n'));
    }
    return doc;
}

// ── Sessions ──────────────────────────────────────────────────────────────────

QDateTime db_time_local(const QString& stamp) {
    QDateTime dt = QDateTime::fromString(stamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (dt.isValid())
        return QDateTime(dt.date(), dt.time(), QTimeZone(QTimeZone::UTC)).toLocalTime();
    dt = QDateTime::fromString(stamp, Qt::ISODate);
    return dt.isValid() ? dt.toLocalTime() : QDateTime();
}

DateGroup date_group(const QDateTime& when, const QDate& today) {
    if (!when.isValid())
        return DateGroup::Older;
    const QDate d = when.date();
    if (d >= today)
        return DateGroup::Today;
    if (d == today.addDays(-1))
        return DateGroup::Yesterday;
    if (d > today.addDays(-7))
        return DateGroup::ThisWeek;
    return DateGroup::Older;
}

QString date_group_title(DateGroup g) {
    switch (g) {
        case DateGroup::Pinned:
            return ty_tr("PINNED");
        case DateGroup::Today:
            return ty_tr("TODAY");
        case DateGroup::Yesterday:
            return ty_tr("YESTERDAY");
        case DateGroup::ThisWeek:
            return ty_tr("THIS WEEK");
        case DateGroup::Older:
            break;
    }
    return ty_tr("OLDER");
}

QString short_when(const QDateTime& when, const QDate& today) {
    if (!when.isValid())
        return {};
    const QDate d = when.date();
    if (d == today)
        return when.toString(QStringLiteral("HH:mm"));
    if (d > today.addDays(-7))
        return QLocale().toString(when, QStringLiteral("ddd"));
    if (d.year() == today.year())
        return QLocale().toString(when, QStringLiteral("MMM d"));
    return when.toString(QStringLiteral("yyyy-MM-dd"));
}

QString snippet_of(const QString& role, const QString& stored_content, int max_chars) {
    QString text = role == QLatin1String("user")        ? parse_user_wire(stored_content).typed
                   : role == QLatin1String("assistant") ? strip_tool_trailer(stored_content, nullptr)
                                                        : stored_content;
    QString first;
    bool in_fence = false;
    for (const QString& raw : text.split(QLatin1Char('\n'))) {
        QString l = raw.trimmed();
        if (l.startsWith(QLatin1String("```")) || l.startsWith(QLatin1String("~~~"))) {
            in_fence = !in_fence;
            continue;
        }
        if (in_fence || l.isEmpty() || ty_rx_rule().match(l).hasMatch() || ty_rx_table_sep().match(l).hasMatch())
            continue;
        static const QRegularExpression lead(QStringLiteral("^(#{1,6}\\s+|>\\s*|[-*+]\\s+|\\d{1,3}[.)]\\s+)"));
        l.remove(lead);
        l.remove(QLatin1Char('|'));
        first = strip_inline_markdown(l).simplified();
        if (!first.isEmpty())
            break;
    }
    if (first.size() > max_chars)
        first = first.left(std::max(1, max_chars - 1)).trimmed() + QStringLiteral("\u2026");
    return first;
}

// ── Prompts ───────────────────────────────────────────────────────────────────

QVector<PromptTemplate> builtin_templates() {
    QVector<PromptTemplate> t;
    auto add = [&t](const char* id, const char* group, const char* title, const char* body) {
        PromptTemplate p;
        p.id = QString::fromLatin1(id);
        p.group = ty_tr(group);
        p.title = ty_tr(title);
        p.body = ty_tr(body);
        t.append(p);
    };
    add("tpl.filings", QT_TRANSLATE_NOOP("AiChat", "Research"), QT_TRANSLATE_NOOP("AiChat", "Summarise filings"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Summarise the latest annual and quarterly filings of {SYMBOL}: revenue and margin trends, "
                          "guidance, risk factors that changed, and anything unusual in the notes. Put the key "
                          "figures in a table."));
    add("tpl.peers", QT_TRANSLATE_NOOP("AiChat", "Research"), QT_TRANSLATE_NOOP("AiChat", "Compare peers"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Compare {SYMBOL} with its closest peers on growth, margins, valuation (P/E, EV/EBITDA) "
                          "and balance-sheet strength. One table with the figures, then a short verdict."));
    add("tpl.earnings", QT_TRANSLATE_NOOP("AiChat", "Research"), QT_TRANSLATE_NOOP("AiChat", "Earnings preview"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Preview the next earnings of {SYMBOL}: consensus estimates, what the last quarter showed, "
                          "the metrics to watch and the market's expectations."));
    add("tpl.move", QT_TRANSLATE_NOOP("AiChat", "Markets"), QT_TRANSLATE_NOOP("AiChat", "Explain a move"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Explain the latest move in {SYMBOL}: what happened, the news or data behind it, how it "
                          "compares with its sector and the index, and what to watch next."));
    add("tpl.screen", QT_TRANSLATE_NOOP("AiChat", "Markets"), QT_TRANSLATE_NOOP("AiChat", "Screen for ideas"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Find five large-cap stocks with rising earnings estimates and valuations below their "
                          "sector. A table with the figures and one line of rationale each."));
    add("tpl.risk", QT_TRANSLATE_NOOP("AiChat", "Portfolio"), QT_TRANSLATE_NOOP("AiChat", "Portfolio risk review"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Review the risk of my portfolio: concentration by holding and sector, volatility and "
                          "drawdown, correlation between the largest positions, and the three changes that would "
                          "reduce risk the most."));
    add("tpl.macro", QT_TRANSLATE_NOOP("AiChat", "Macro"), QT_TRANSLATE_NOOP("AiChat", "Macro brief"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Give me a macro brief: rates, inflation, FX, commodities and this week's data calendar, "
                          "with what each means for equities."));
    add("tpl.note", QT_TRANSLATE_NOOP("AiChat", "Writing"), QT_TRANSLATE_NOOP("AiChat", "Draft a note"),
        QT_TRANSLATE_NOOP("AiChat",
                          "Draft a one-page research note on {SYMBOL}: the thesis in three bullets, a key-figures "
                          "table, catalysts, risks and a conclusion. Keep it factual and cite sources."));
    return t;
}

QVector<ExampleCard> example_cards() {
    QVector<ExampleCard> c;
    auto add = [&c](const char* group, const char* title, const char* prompt) {
        c.append({ty_tr(group), ty_tr(title), ty_tr(prompt)});
    };
    add(QT_TRANSLATE_NOOP("AiChat", "Markets"), QT_TRANSLATE_NOOP("AiChat", "Top movers"),
        QT_TRANSLATE_NOOP("AiChat", "Show me today's top market movers across the major US indices and explain the "
                                    "three biggest moves."));
    add(QT_TRANSLATE_NOOP("AiChat", "Markets"), QT_TRANSLATE_NOOP("AiChat", "Sector rotation"),
        QT_TRANSLATE_NOOP("AiChat", "Which sectors outperformed over the last month, and what is driving the rotation?"));
    add(QT_TRANSLATE_NOOP("AiChat", "Markets"), QT_TRANSLATE_NOOP("AiChat", "Volatility check"),
        QT_TRANSLATE_NOOP("AiChat", "How does current market volatility compare with its one-year range? Include the "
                                    "VIX and realised volatility."));
    add(QT_TRANSLATE_NOOP("AiChat", "Company research"), QT_TRANSLATE_NOOP("AiChat", "Valuation snapshot"),
        QT_TRANSLATE_NOOP("AiChat", "Calculate a valuation snapshot for AAPL: P/E, EV/EBITDA and FCF yield versus its "
                                    "five-year average and its peers."));
    add(QT_TRANSLATE_NOOP("AiChat", "Company research"), QT_TRANSLATE_NOOP("AiChat", "Compare peers"),
        QT_TRANSLATE_NOOP("AiChat", "Compare MSFT, GOOGL and AMZN on revenue growth, operating margin and cloud "
                                    "segment performance."));
    add(QT_TRANSLATE_NOOP("AiChat", "Company research"), QT_TRANSLATE_NOOP("AiChat", "Earnings recap"),
        QT_TRANSLATE_NOOP("AiChat", "Summarise NVDA's latest earnings: results versus consensus, guidance and the "
                                    "market reaction."));
    add(QT_TRANSLATE_NOOP("AiChat", "Portfolio"), QT_TRANSLATE_NOOP("AiChat", "Performance review"),
        QT_TRANSLATE_NOOP("AiChat", "Analyse my portfolio's performance this year and attribute it to the largest "
                                    "contributors and detractors."));
    add(QT_TRANSLATE_NOOP("AiChat", "Portfolio"), QT_TRANSLATE_NOOP("AiChat", "Risk review"),
        QT_TRANSLATE_NOOP("AiChat", "Review my portfolio's concentration, volatility and correlation risk, and "
                                    "suggest three ways to reduce it."));
    add(QT_TRANSLATE_NOOP("AiChat", "Portfolio"), QT_TRANSLATE_NOOP("AiChat", "Rebalancing"),
        QT_TRANSLATE_NOOP("AiChat", "Suggest how to rebalance my portfolio toward a 60/40 allocation and list the "
                                    "trades it implies."));
    add(QT_TRANSLATE_NOOP("AiChat", "Macro"), QT_TRANSLATE_NOOP("AiChat", "Growth and inflation"),
        QT_TRANSLATE_NOOP("AiChat", "Summarise the latest GDP and inflation data for the US, the euro area and India."));
    add(QT_TRANSLATE_NOOP("AiChat", "Macro"), QT_TRANSLATE_NOOP("AiChat", "Rates outlook"),
        QT_TRANSLATE_NOOP("AiChat", "What is the market pricing for the Fed's next three meetings, and how has that "
                                    "changed this month?"));
    add(QT_TRANSLATE_NOOP("AiChat", "Macro"), QT_TRANSLATE_NOOP("AiChat", "Dollar drivers"),
        QT_TRANSLATE_NOOP("AiChat", "Explain the main drivers of the US dollar index this quarter."));
    add(QT_TRANSLATE_NOOP("AiChat", "Code / data"), QT_TRANSLATE_NOOP("AiChat", "Python backtest"),
        QT_TRANSLATE_NOOP("AiChat", "Write a Python script that backtests a 50/200-day moving-average crossover on "
                                    "SPY with pandas, and explain what it prints."));
    add(QT_TRANSLATE_NOOP("AiChat", "Code / data"), QT_TRANSLATE_NOOP("AiChat", "Yield history"),
        QT_TRANSLATE_NOOP("AiChat", "Show the 10-year Treasury yield for the last five years as a table by quarter."));
    add(QT_TRANSLATE_NOOP("AiChat", "Code / data"), QT_TRANSLATE_NOOP("AiChat", "Excel formula"),
        QT_TRANSLATE_NOOP("AiChat", "Write an Excel formula that computes the CAGR between two cells and explain it."));
    return c;
}

QVector<PromptTemplate> load_saved_prompts() {
    QVector<PromptTemplate> out;
    const auto r = SettingsRepository::instance().get(QStringLiteral("ai_chat.saved_prompts"));
    if (r.is_err() || r.value().isEmpty())
        return out;
    const QJsonArray arr = QJsonDocument::fromJson(r.value().toUtf8()).array();
    int n = 0;
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        PromptTemplate p;
        p.title = o.value(QStringLiteral("title")).toString().trimmed();
        p.body = o.value(QStringLiteral("body")).toString();
        if (p.body.trimmed().isEmpty())
            continue;
        if (p.title.isEmpty())
            p.title = p.body.simplified().left(40);
        p.id = QStringLiteral("saved.") + QString::number(n++);
        p.group = ty_tr("Saved");
        p.saved = true;
        out.append(p);
    }
    return out;
}

bool store_saved_prompts(const QVector<PromptTemplate>& prompts) {
    QJsonArray arr;
    for (const auto& p : prompts)
        arr.append(QJsonObject{{QStringLiteral("title"), p.title}, {QStringLiteral("body"), p.body}});
    const auto r = SettingsRepository::instance().set(
        QStringLiteral("ai_chat.saved_prompts"), QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
        QStringLiteral("ai_chat"));
    return r.is_ok();
}

QByteArray notebook_json(const QString& code) {
    QJsonArray source;
    const QStringList lines = code.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i)
        source.append(i + 1 < lines.size() ? lines[i] + QLatin1Char('\n') : lines[i]);
    QJsonObject cell{{QStringLiteral("cell_type"), QStringLiteral("code")},
                     {QStringLiteral("execution_count"), QJsonValue()},
                     {QStringLiteral("metadata"), QJsonObject()},
                     {QStringLiteral("outputs"), QJsonArray()},
                     {QStringLiteral("source"), source}};
    QJsonObject meta{{QStringLiteral("kernelspec"),
                      QJsonObject{{QStringLiteral("display_name"), QStringLiteral("Python 3")},
                                  {QStringLiteral("language"), QStringLiteral("python")},
                                  {QStringLiteral("name"), QStringLiteral("python3")}}},
                     {QStringLiteral("language_info"), QJsonObject{{QStringLiteral("name"), QStringLiteral("python")}}}};
    QJsonObject nb{{QStringLiteral("cells"), QJsonArray{cell}},
                   {QStringLiteral("metadata"), meta},
                   {QStringLiteral("nbformat"), 4},
                   {QStringLiteral("nbformat_minor"), 5}};
    return QJsonDocument(nb).toJson(QJsonDocument::Indented);
}

} // namespace fincept::screens::aichat
