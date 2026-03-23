#include "screens/report_builder/DocumentCanvas.h"

#include "screens/report_builder/ReportBuilderScreen.h"
#include "ui/theme/Theme.h"

#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextTable>
#include <QVBoxLayout>

namespace fincept::screens {

DocumentCanvas::DocumentCanvas(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background: #111111;");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(20, 20, 20, 20);
    vl->setAlignment(Qt::AlignHCenter);

    editor_ = new QTextEdit;
    editor_->setReadOnly(true);
    editor_->setFixedWidth(794); // A4 at 96 dpi
    editor_->setMinimumHeight(1123);
    editor_->setStyleSheet(QString("QTextEdit { background: white; color: #1a1a1a; border: 1px solid %1; "
                                   "padding: 60px; font-family: 'Segoe UI'; font-size: 12pt; }")
                               .arg(ui::colors::BORDER));

    vl->addWidget(editor_);
}

void DocumentCanvas::render(const QVector<ReportComponent>& components, const ReportMetadata& metadata,
                            int selected_index) {
    editor_->clear();
    QTextCursor cursor = editor_->textCursor();

    // Metadata header
    QTextCharFormat title_fmt;
    title_fmt.setFontPointSize(20);
    title_fmt.setFontWeight(QFont::Bold);
    title_fmt.setForeground(QColor("#1a1a1a"));
    cursor.insertText(metadata.title + "\n", title_fmt);

    QTextCharFormat meta_fmt;
    meta_fmt.setFontPointSize(10);
    meta_fmt.setForeground(QColor("#666666"));
    cursor.insertText(metadata.author + " | " + metadata.company + " | " + metadata.date + "\n\n", meta_fmt);

    // Divider
    QTextBlockFormat line_block;
    line_block.setTopMargin(4);
    line_block.setBottomMargin(12);
    cursor.insertBlock(line_block);
    QTextCharFormat line_fmt;
    line_fmt.setForeground(QColor("#cccccc"));
    line_fmt.setFontPointSize(8);
    cursor.insertText(QString(80, QChar(0x2500)), line_fmt);

    // Render each component
    for (int i = 0; i < components.size(); ++i) {
        const auto& comp = components[i];
        cursor.insertBlock();

        if (comp.type == "heading") {
            QTextCharFormat fmt;
            fmt.setFontPointSize(16);
            fmt.setFontWeight(QFont::Bold);
            fmt.setForeground(QColor("#1a1a1a"));
            cursor.insertText(comp.content.isEmpty() ? "Heading" : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "text") {
            QTextBlockFormat bf;
            bf.setLineHeight(140, QTextBlockFormat::ProportionalHeight);
            cursor.setBlockFormat(bf);
            QTextCharFormat fmt;
            fmt.setFontPointSize(12);
            fmt.setForeground(QColor("#333333"));
            cursor.insertText(comp.content.isEmpty() ? "Enter text here..." : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "table") {
            int rows = comp.config.value("rows", "3").toInt();
            int cols = comp.config.value("cols", "3").toInt();

            QTextTableFormat tf;
            tf.setBorderBrush(QColor("#cccccc"));
            tf.setBorder(1);
            tf.setCellPadding(6);
            tf.setCellSpacing(0);
            QVector<QTextLength> widths(cols, QTextLength(QTextLength::PercentageLength, 100.0 / cols));
            tf.setColumnWidthConstraints(widths);

            auto* table = cursor.insertTable(rows, cols, tf);
            QTextCharFormat hdr_fmt;
            hdr_fmt.setFontWeight(QFont::Bold);
            hdr_fmt.setForeground(QColor("#1a1a1a"));

            for (int c = 0; c < cols; ++c) {
                table->cellAt(0, c).firstCursorPosition().insertText(QString("Header %1").arg(c + 1), hdr_fmt);
            }
            cursor.movePosition(QTextCursor::End);
            cursor.insertText("\n");

        } else if (comp.type == "code") {
            QTextBlockFormat bf;
            bf.setBackground(QColor("#f5f5f5"));
            bf.setLeftMargin(16);
            bf.setRightMargin(16);
            cursor.setBlockFormat(bf);
            QTextCharFormat fmt;
            fmt.setFontFamily("Consolas");
            fmt.setFontPointSize(11);
            fmt.setForeground(QColor("#333333"));
            cursor.insertText(comp.content.isEmpty() ? "// code block" : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "divider") {
            QTextCharFormat fmt;
            fmt.setForeground(QColor("#cccccc"));
            fmt.setFontPointSize(8);
            cursor.insertText(QString(80, QChar(0x2500)), fmt);
            cursor.insertText("\n");

        } else if (comp.type == "quote") {
            QTextBlockFormat bf;
            bf.setLeftMargin(24);
            bf.setBackground(QColor("#f9f9f9"));
            cursor.setBlockFormat(bf);
            QTextCharFormat fmt;
            fmt.setFontItalic(true);
            fmt.setForeground(QColor("#555555"));
            fmt.setFontPointSize(12);
            cursor.insertText(comp.content.isEmpty() ? "Block quote..." : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "list") {
            QTextListFormat lf;
            lf.setStyle(QTextListFormat::ListDisc);
            lf.setIndent(1);
            QStringList items = comp.content.split("\n", Qt::SkipEmptyParts);
            if (items.isEmpty())
                items << "Item 1" << "Item 2";
            for (const auto& item : items) {
                cursor.insertBlock();
                cursor.createList(lf);
                cursor.insertText(item);
            }
            cursor.insertText("\n");

        } else if (comp.type == "image") {
            QString path = comp.config.value("path", "");
            if (!path.isEmpty()) {
                QTextImageFormat img_fmt;
                img_fmt.setName(path);
                img_fmt.setWidth(400);
                cursor.insertImage(img_fmt);
            } else {
                QTextCharFormat fmt;
                fmt.setForeground(QColor("#999999"));
                fmt.setFontPointSize(11);
                cursor.insertText("[Image placeholder — select in Properties]", fmt);
            }
            cursor.insertText("\n");
        }
    }

    editor_->setTextCursor(cursor);
}

} // namespace fincept::screens
