// PlainTextParser.cpp — plain-text stdout → one accumulating Text message (see
// PlainTextParser.h). QtCore only.

#include "services/harness/parsers/PlainTextParser.h"

#include <QRegularExpression>

namespace fincept::harness {

namespace plain_text_parser_detail {

namespace ptp_cad = fincept::services::coding_agents;
namespace ptp_sps = fincept::harness::stream_support;

/// ANSI / VT escape sequences (colours, cursor moves, OSC titles and links).
QString strip_terminal_escapes(const QString& line) {
    if (!line.contains(QChar(0x1B)))
        return line;
    static const QRegularExpression rx(
        QStringLiteral("\\x1B(?:\\[[0-9;?]*[ -/]*[@-~]|\\][^\\x07\\x1B]*(?:\\x07|\\x1B\\\\)|[@-_])"));
    QString s = line;
    s.remove(rx);
    return s;
}

} // namespace plain_text_parser_detail

using namespace plain_text_parser_detail;

QVector<ptp_cad::AgentEvent> PlainTextParser::parse_line(const QString& line) {
    QVector<ptp_cad::AgentEvent> out;
    QString text = strip_terminal_escapes(line);
    // Trailing whitespace and a carriage return left by a "\r\n" or a progress redraw.
    while (!text.isEmpty() && text.back().isSpace())
        text.chop(1);
    if (text.trimmed().isEmpty()) {
        if (acc_.active())
            ++pending_blank_lines_;
        return out;
    }
    if (ptp_sps::looks_like_error(text))
        facts_.last_error = text.trimmed().left(2000);
    QString delta;
    if (!acc_.active()) {
        acc_.start(ptp_cad::EventKind::Text, QStringLiteral("output"), QStringLiteral("Assistant"));
    } else {
        delta = QStringLiteral("\n");
        if (pending_blank_lines_ > 0)
            delta += QStringLiteral("\n"); // paragraph break (several blank lines fold into one)
    }
    pending_blank_lines_ = 0;
    delta += text;
    if (auto ev = acc_.append(delta))
        out.append(*ev);
    return out;
}

QVector<ptp_cad::AgentEvent> PlainTextParser::finish() {
    QVector<ptp_cad::AgentEvent> out;
    QString full;
    if (auto ev = acc_.finish_message(&full)) {
        out.append(*ev);
        facts_.final_text = full;
    }
    pending_blank_lines_ = 0;
    return out;
}

} // namespace fincept::harness
