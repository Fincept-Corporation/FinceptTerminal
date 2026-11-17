// tst_font_size_text — src/ui/theme/FontSizeText.h: how a base-relative font
// size is written into a stylesheet and rewritten for another base size by the
// live re-style pass, plus proof that Qt's own stylesheet / HTML parsers read
// the offset marker as whitespace (offscreen QLabel / QTextDocument).
#include "ui/theme/FontSizeText.h"

#include <QApplication>
#include <QLabel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtTest>

using namespace fincept::ui::font_size_text;

namespace {
constexpr int kFloor = 7; // ThemeManager::kFloorFontPx
const QList<int> kBases{9, 10, 12, 14, 18, 24};

QString sheet_at(int base, int offset) {
    return QStringLiteral("QLabel { color:#e5e5e5; font-size:") + encode(base, offset, kFloor) +
           QStringLiteral("px; padding:2px 4px; }");
}
} // namespace

class TestFontSizeText : public QObject {
    Q_OBJECT
  private slots:
    void encode_writes_the_number_and_marks_only_clamped_sizes() {
        QCOMPARE(encode(14, -3, kFloor), QStringLiteral("11"));
        QCOMPARE(encode(14, 6, kFloor), QStringLiteral("20"));
        QCOMPARE(encode(9, -2, kFloor), QStringLiteral("7")); // exactly the floor: not clamped
        QCOMPARE(encode(9, -3, kFloor), QStringLiteral("/*fo:-3*/7"));
        QCOMPARE(encode(10, -4, kFloor), QStringLiteral("/*fo:-4*/7"));
        QCOMPARE(encode(24, -5, kFloor), QStringLiteral("19"));
    }

    void round_trip_9_24_9_and_24_9_24() {
        for (int offset = -6; offset <= 8; ++offset) {
            const QString at9 = sheet_at(9, offset);
            const QString at24 = translate(at9, 9, 24, kFloor);
            QCOMPARE(at24, sheet_at(24, offset));
            QCOMPARE(translate(at24, 24, 9, kFloor), at9);

            const QString from24 = sheet_at(24, offset);
            const QString down = translate(from24, 24, 9, kFloor);
            QCOMPARE(down, sheet_at(9, offset));
            QCOMPARE(translate(down, 9, 24, kFloor), from24);
        }
    }

    void any_chain_of_changes_lands_where_a_fresh_build_would() {
        const QList<int> chain{9, 24, 10, 14, 9, 18, 12, 24, 9};
        for (int offset = -6; offset <= 8; ++offset) {
            QString s = sheet_at(chain.first(), offset);
            for (int i = 1; i < chain.size(); ++i) {
                s = translate(s, chain.at(i - 1), chain.at(i), kFloor);
                QCOMPARE(s, sheet_at(chain.at(i), offset));
            }
        }
        for (int a : kBases)
            for (int b : kBases)
                for (int offset = -6; offset <= 8; ++offset)
                    QCOMPARE(translate(sheet_at(a, offset), a, b, kFloor), sheet_at(b, offset));
    }

    void several_declarations_and_everything_else_untouched() {
        const QString in = QStringLiteral(
            "/* a note, not a marker */ QLabel{font-size:/*fo:-5*/7px;color:#fff} "
            "QPushButton { font-size: 12px; min-height: 22px; } #x { font-size: /*fo:-4*/ 7px; }");
        const QString out = translate(in, 9, 24, kFloor);
        QCOMPARE(out, QStringLiteral("/* a note, not a marker */ QLabel{font-size:19px;color:#fff} "
                                     "QPushButton { font-size: 27px; min-height: 22px; } #x { font-size: 20px; }"));
    }

    void an_unmarked_size_is_taken_as_written_at_the_old_base() {
        // A literal, or a bare int from font_px() — exact unless the floor clamped it.
        QCOMPARE(translate(QStringLiteral("font-size:11px"), 14, 24, kFloor), QStringLiteral("font-size:21px"));
        QCOMPARE(translate(QStringLiteral("font-size:11px"), 14, 9, kFloor), QStringLiteral("font-size:/*fo:-3*/7px"));
        // The documented limit of the int path: a clamped 7 at base 9 reads as -2.
        QCOMPARE(translate(QStringLiteral("font-size:7px"), 9, 24, kFloor), QStringLiteral("font-size:22px"));
    }

    void rewrite_works_inside_a_larger_expression() {
        // ThemeRestyle's Translator puts pattern() after six colour groups and
        // calls rewrite(m, 7, …): the relative group numbering must hold there.
        const QRegularExpression re(QStringLiteral("(a)(b)(c)(d)(e)(f)|") + pattern(),
                                    QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(QStringLiteral("font-size:/*fo:-6*/7px"));
        QVERIFY(m.hasMatch());
        QCOMPARE(rewrite(m, 7, 9, 24, kFloor), QStringLiteral("font-size:18"));
        const QRegularExpressionMatch plain = re.match(QStringLiteral("FONT-SIZE: 13px"));
        QVERIFY(plain.hasMatch());
        QCOMPARE(rewrite(plain, 7, 14, 9, kFloor), QStringLiteral("FONT-SIZE: 8"));
    }

    void qt_stylesheet_reads_the_marker_as_whitespace() {
        QLabel bare;
        bare.setStyleSheet(QStringLiteral("font-size:/*fo:-5*/7px;"));
        bare.ensurePolished();
        QCOMPARE(bare.font().pixelSize(), 7);

        QLabel ruled;
        ruled.setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: /*fo:-4*/ 7px; font-weight: 600; }"));
        ruled.ensurePolished();
        QCOMPARE(ruled.font().pixelSize(), 7);
        QCOMPARE(ruled.font().weight(), QFont::DemiBold); // the declaration after it still applies

        QLabel control; // the same path with a size that is not the floor
        control.setStyleSheet(QStringLiteral("QLabel { font-size: 19px; }"));
        control.ensurePolished();
        QCOMPARE(control.font().pixelSize(), 19);
    }

    void qt_html_reads_the_marker_as_whitespace() {
        QTextDocument doc;
        doc.setHtml(QStringLiteral("<span style=\"font-size:/*fo:-5*/7px; font-weight:700\">x</span>"));
        QTextCursor c(doc.firstBlock());
        c.movePosition(QTextCursor::Right);
        QCOMPARE(c.charFormat().font().pixelSize(), 7);
        QCOMPARE(c.charFormat().fontWeight(), int(QFont::Bold));
    }
};

QTEST_MAIN(TestFontSizeText)
#include "tst_font_size_text.moc"
