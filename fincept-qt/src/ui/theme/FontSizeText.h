#pragma once
// FontSizeText — how a base-relative font size is written into a stylesheet,
// and how the live re-style pass (ThemeRestyle.cpp) rewrites it for another
// base size.
//
// A size is written as the plain number of px ("11"), except when the
// legibility floor clamped it: then a CSS comment carrying the offset the
// caller asked for precedes the number ("/*fo:-5*/7"). Without it, "7px"
// written at base 9 could stand for an offset of -2, -3, -4 … and a later
// change to base 24 could only guess: it took -2, so every small label came
// out at 22px instead of 19-21px, and came back wrong after returning to 9.
// With the marker the offset survives any number of changes (9→24→9,
// 24→9→24): the re-style pass writes the marker again whenever its result is
// clamped and drops it when it is not.
//
// Why a comment is safe: Qt's CSS scanner returns a comment as a whitespace
// token (qcssparser.cpp, QCssScanner_Generated::handleCommentStart() returns
// S), and a declaration skips whitespace after its colon
// (Parser::parseNextDeclaration → skipSpace()), so `font-size:/*fo:-5*/7px`
// reads exactly as `font-size:7px` (Qt 6.8.3 source; tst_font_size_text also
// checks it on a live QLabel). HTML style attributes go through the same
// parser, and web CSS allows a comment wherever whitespace may stand. A
// size a caller writes as a bare int (`.arg(font_px(-4))`) carries no marker:
// use ui::fonts::px() in stylesheets.
//
// Header-only, Qt Core only.

#include <QRegularExpression>
#include <QString>

namespace fincept::ui::font_size_text {

/// Text for the size `offset` px from `base`, never below `floor`: the number,
/// with the offset marker in front when the floor clamped it.
inline QString encode(int base, int offset, int floor) {
    const int v = base + offset;
    if (v >= floor)
        return QString::number(v);
    return QStringLiteral("/*fo:%1*/%2").arg(offset).arg(floor);
}

/// The offset a written size stands for: the marker's when there is one,
/// otherwise the number minus the base it was written at (a literal size, or
/// one the floor did not touch, is exact that way).
inline int offset_of(int shown, bool has_marker, int marker_offset, int written_base) {
    return has_marker ? marker_offset : shown - written_base;
}

/// Regular-expression fragment for one `font-size: [marker] N` followed by
/// "px". Groups (relative): 1 = "font-size:" with its spacing, 2 = the marker
/// offset (may be absent), 3 = the number.
inline QString pattern() {
    return QStringLiteral("(font-size\\s*:\\s*)(?:/\\*fo:(-?\\d+)\\*/\\s*)?(\\d+)(?=px)");
}

/// One match of pattern() (groups starting at `first`) rewritten from base
/// `from` to base `to`.
inline QString rewrite(const QRegularExpressionMatch& m, int first, int from, int to, int floor) {
    const int shown = m.captured(first + 2).toInt();
    const bool marked = m.hasCaptured(first + 1);
    const int offset = offset_of(shown, marked, marked ? m.captured(first + 1).toInt() : 0, from);
    return m.captured(first) + encode(to, offset, floor);
}

/// Every font size in `text`, written at base `from`, rewritten for base `to`.
/// Nothing else in the text changes.
inline QString translate(const QString& text, int from, int to, int floor) {
    static const QRegularExpression re(pattern(), QRegularExpression::CaseInsensitiveOption);
    QString out;
    out.reserve(text.size() + 16);
    qsizetype last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += QStringView(text).mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        out += rewrite(m, 1, from, to, floor);
    }
    out += QStringView(text).mid(last);
    return out;
}

} // namespace fincept::ui::font_size_text
