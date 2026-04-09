#include "screens/report_builder/DocumentCanvas.h"

#include "screens/report_builder/ReportBuilderScreen.h"
#include "ui/theme/Theme.h"

namespace { // forward-declare default theme for constructor use
static fincept::screens::ReportTheme default_canvas_theme() {
    return {"Light Professional", "#1a1a1a", "#d97706", "#ffffff",
            "#333333", "#f0f0f0", "#1a1a1a", "#f5f5f5",
            "#f9f9f9", "#666666", "#cccccc"};
}
} // namespace

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineSeries>
#include <QMimeData>
#include <QPieSeries>
#include <QPieSlice>
#include <QStandardPaths>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextTable>
#include <QUrl>
#include <QValueAxis>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Chart rendering helpers ────────────────────────────────────────────────────

// Parse "10,20,15,30" into a list of doubles, skipping non-numeric tokens
static QVector<double> parse_csv_doubles(const QString& csv) {
    QVector<double> vals;
    for (const QString& tok : csv.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        double v = tok.trimmed().toDouble(&ok);
        if (ok) vals << v;
    }
    return vals;
}

// Parse "Jan,Feb,Mar" into a list of labels
static QStringList parse_csv_labels(const QString& csv) {
    QStringList out;
    for (const QString& tok : csv.split(',', Qt::SkipEmptyParts))
        out << tok.trimmed();
    return out;
}

// Render a QChart to a temp PNG file and return the file path (empty on failure)
static QString render_chart_to_file(const QString& chart_type,
                                    const QString& title,
                                    const QString& data_str,
                                    const QString& labels_str,
                                    const ReportTheme& theme,
                                    int width = 640, int height = 320) {
    QVector<double> values = parse_csv_doubles(data_str);
    if (values.isEmpty()) return {};

    QStringList labels = parse_csv_labels(labels_str);

    // Pad or trim labels to match value count
    while (labels.size() < values.size())
        labels << QString::number(labels.size() + 1);

    QChart* chart = new QChart();
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::NoAnimation);

    // Theme colours
    QColor bg_col(theme.page_bg);
    QColor text_col(theme.text_color);
    QColor accent(theme.accent_color);

    chart->setBackgroundBrush(QBrush(bg_col));
    chart->setTitleBrush(QBrush(QColor(theme.heading_color)));
    QFont title_font;
    title_font.setPointSize(10);
    title_font.setBold(true);
    chart->setTitleFont(title_font);

    if (chart_type == "pie") {
        auto* series = new QPieSeries();
        for (int i = 0; i < values.size(); ++i) {
            auto* slice = series->append(labels.value(i, QString::number(i + 1)), values[i]);
            slice->setLabelVisible(true);
            QFont lf; lf.setPointSize(8);
            slice->setLabelFont(lf);
        }
        // Colour slices with accent + variations
        const QStringList palette = {"#d97706","#3b82f6","#16a34a","#dc2626",
                                     "#7c3aed","#0891b2","#ea580c","#65a30d"};
        for (int i = 0; i < series->slices().size(); ++i) {
            series->slices()[i]->setColor(QColor(palette[i % palette.size()]));
            series->slices()[i]->setLabelColor(text_col);
        }
        chart->addSeries(series);
        chart->legend()->setVisible(true);
        chart->legend()->setLabelColor(text_col);

    } else if (chart_type == "bar") {
        auto* set = new QBarSet("Value");
        set->setColor(accent);
        set->setLabelColor(text_col);
        for (double v : values) *set << v;

        auto* series = new QBarSeries();
        series->append(set);
        chart->addSeries(series);

        auto* axisX = new QBarCategoryAxis();
        axisX->append(labels);
        axisX->setLabelsColor(text_col);
        QFont axf; axf.setPointSize(8);
        axisX->setLabelsFont(axf);
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);

        auto* axisY = new QValueAxis();
        axisY->setLabelsColor(text_col);
        axisY->setLabelsFont(axf);
        axisY->setGridLineColor(QColor(theme.divider_color));
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        chart->legend()->setVisible(false);

    } else {
        // Default: line chart
        auto* series = new QLineSeries();
        series->setColor(accent);
        QPen pen(accent);
        pen.setWidth(2);
        series->setPen(pen);
        for (int i = 0; i < values.size(); ++i)
            series->append(i, values[i]);
        chart->addSeries(series);

        auto* axisX = new QBarCategoryAxis();
        axisX->append(labels);
        axisX->setLabelsColor(text_col);
        QFont axf; axf.setPointSize(8);
        axisX->setLabelsFont(axf);
        axisX->setGridLineColor(QColor(theme.divider_color));
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);

        auto* axisY = new QValueAxis();
        axisY->setLabelsColor(text_col);
        axisY->setLabelsFont(axf);
        axisY->setGridLineColor(QColor(theme.divider_color));
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        chart->legend()->setVisible(false);
    }

    // Style legend
    QPalette legend_pal;
    legend_pal.setColor(QPalette::WindowText, text_col);
    chart->legend()->setPalette(legend_pal);

    // Render to pixmap via QChartView
    QChartView view(chart);
    view.setRenderHint(QPainter::Antialiasing);
    view.resize(width, height);

    QPixmap pix = view.grab();
    delete chart; // view took ownership but we grabbed already

    QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                   + QString("/fincept_chart_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    if (pix.save(path, "PNG")) return path;
    return {};
}

// Render a small sparkline (mini line chart) to a temp PNG
static QString render_sparkline_to_file(const QString& data_str,
                                        const ReportTheme& theme,
                                        int width = 200, int height = 60) {
    QVector<double> values = parse_csv_doubles(data_str);
    if (values.size() < 2) return {};

    QChart* chart = new QChart();
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->setBackgroundRoundness(0);
    chart->setBackgroundBrush(QBrush(QColor(theme.page_bg)));
    chart->legend()->setVisible(false);
    chart->setTitle("");
    chart->setAnimationOptions(QChart::NoAnimation);

    auto* series = new QLineSeries();
    QColor accent(theme.accent_color);
    QPen pen(accent);
    pen.setWidth(2);
    series->setPen(pen);
    for (int i = 0; i < values.size(); ++i)
        series->append(i, values[i]);
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->axes(Qt::Horizontal).first()->setVisible(false);
    chart->axes(Qt::Vertical).first()->setVisible(false);

    QChartView view(chart);
    view.setRenderHint(QPainter::Antialiasing);
    view.resize(width, height);
    QPixmap pix = view.grab();
    delete chart;

    QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                   + QString("/fincept_spark_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    if (pix.save(path, "PNG")) return path;
    return {};
}

DocumentCanvas::DocumentCanvas(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }"
                "QScrollBar:vertical { background: %2; width: 10px; }"
                "QScrollBar::handle:vertical { background: %3; border-radius: 5px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                "QScrollBar:horizontal { background: %2; height: 10px; }"
                "QScrollBar::handle:horizontal { background: %3; border-radius: 5px; min-width: 20px; }"
                "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }")
            .arg(ui::colors::BORDER_DIM, ui::colors::BG_RAISED, ui::colors::BORDER_BRIGHT));

    container_ = new QWidget;
    container_->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM));
    pages_layout_ = new QVBoxLayout(container_);
    pages_layout_->setContentsMargins(40, 40, 40, 40);
    pages_layout_->setSpacing(24); // gap between pages
    pages_layout_->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    scroll_->setWidget(container_);
    root->addWidget(scroll_);

    // Always have at least one page so text_edit() never returns nullptr
    new_page(default_canvas_theme());
}

void DocumentCanvas::clear_pages() {
    for (auto* p : pages_) {
        pages_layout_->removeWidget(p);
        delete p;
    }
    pages_.clear();
}

QTextEdit* DocumentCanvas::new_page(const ReportTheme& theme) {
    auto* page = new QTextEdit(container_);
    page->setReadOnly(true);
    page->setFixedWidth(794);   // A4 at 96 dpi
    page->setMinimumHeight(1123); // A4 height
    page->setStyleSheet(
        QString("QTextEdit {"
                "  background: %1;"
                "  color: %2;"
                "  border: none;"
                "  padding: 72px 80px 72px 80px;"
                "  font-family: 'Segoe UI', 'Arial', sans-serif;"
                "  font-size: 12pt;"
                "}"
                "QScrollBar:vertical { width: 0px; }"
                "QScrollBar:horizontal { height: 0px; }")
            .arg(theme.page_bg, theme.text_color));
    // Drop shadow effect via container styling — wrap in a frame
    page->setFrameShape(QFrame::NoFrame);

    pages_layout_->addWidget(page, 0, Qt::AlignHCenter);
    pages_.append(page);
    return page;
}

// ── Drag-and-drop ─────────────────────────────────────────────────────────────

static bool is_image_url(const QUrl& url) {
    if (!url.isLocalFile()) return false;
    static const QStringList exts = {"png", "jpg", "jpeg", "bmp", "svg", "gif", "webp"};
    return exts.contains(QFileInfo(url.toLocalFile()).suffix().toLower());
}

void DocumentCanvas::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        for (const auto& url : e->mimeData()->urls()) {
            if (is_image_url(url)) { e->acceptProposedAction(); return; }
        }
    }
    e->ignore();
}

void DocumentCanvas::dragMoveEvent(QDragMoveEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
    else e->ignore();
}

void DocumentCanvas::dropEvent(QDropEvent* e) {
    if (!e->mimeData()->hasUrls()) { e->ignore(); return; }
    for (const auto& url : e->mimeData()->urls()) {
        if (is_image_url(url)) {
            emit image_dropped(url.toLocalFile());
            e->acceptProposedAction();
            return;
        }
    }
    e->ignore();
}

void DocumentCanvas::render(const QVector<ReportComponent>& components, const ReportMetadata& metadata,
                            const ReportTheme& theme, int selected_index) {
    clear_pages();
    QTextEdit* current_page = new_page(theme);
    QTextCursor cursor = current_page->textCursor();

    // ── Metadata header ───────────────────────────────────────────────────────
    QTextCharFormat title_fmt;
    title_fmt.setFontPointSize(20);
    title_fmt.setFontWeight(QFont::Bold);
    title_fmt.setForeground(QColor(theme.heading_color));
    cursor.insertText(metadata.title + "\n", title_fmt);

    QTextCharFormat meta_fmt;
    meta_fmt.setFontPointSize(10);
    meta_fmt.setForeground(QColor(theme.meta_color));
    QString meta_line = metadata.author;
    if (!metadata.company.isEmpty()) meta_line += " | " + metadata.company;
    if (!metadata.date.isEmpty())    meta_line += " | " + metadata.date;
    cursor.insertText(meta_line + "\n\n", meta_fmt);

    // Divider
    QTextBlockFormat line_block;
    line_block.setTopMargin(4);
    line_block.setBottomMargin(12);
    cursor.insertBlock(line_block);
    QTextCharFormat line_fmt;
    line_fmt.setForeground(QColor(theme.divider_color));
    line_fmt.setFontPointSize(8);
    cursor.insertText(QString(80, QChar(0x2500)), line_fmt);

    // ── Collect headings for TOC ──────────────────────────────────────────────
    QStringList toc_headings;
    for (const auto& comp : components) {
        if (comp.type == "heading") {
            toc_headings << (comp.content.isEmpty() ? "Heading" : comp.content);
        }
    }

    // ── Render each component ─────────────────────────────────────────────────
    for (int i = 0; i < components.size(); ++i) {
        const auto& comp = components[i];
        cursor.insertBlock();

        // Highlight selected
        QTextBlockFormat sel_bf;
        if (i == selected_index) {
            sel_bf.setBackground(QColor("#fffbe6"));
            sel_bf.setLeftMargin(4);
            cursor.setBlockFormat(sel_bf);
        }

        if (comp.type == "heading") {
            QTextCharFormat fmt;
            fmt.setFontPointSize(16);
            fmt.setFontWeight(QFont::Bold);
            fmt.setForeground(QColor(theme.heading_color));
            cursor.insertText(comp.content.isEmpty() ? "Heading" : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "text") {
            QTextBlockFormat bf;
            bf.setLineHeight(140, QTextBlockFormat::ProportionalHeight);
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);
            QTextCharFormat fmt;
            fmt.setFontPointSize(12);
            fmt.setForeground(QColor(theme.text_color));
            cursor.insertText(comp.content.isEmpty() ? "Enter text here..." : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "table") {
            int rows = comp.config.value("rows", "3").toInt();
            int cols = comp.config.value("cols", "3").toInt();

            QTextTableFormat tf;
            tf.setBorderBrush(QColor(theme.divider_color));
            tf.setBorder(1);
            tf.setCellPadding(6);
            tf.setCellSpacing(0);
            QVector<QTextLength> widths(cols, QTextLength(QTextLength::PercentageLength, 100.0 / cols));
            tf.setColumnWidthConstraints(widths);

            // Parse cell data from JSON stored in config["data"]
            QJsonObject cell_data;
            if (comp.config.contains("data")) {
                QJsonDocument doc = QJsonDocument::fromJson(comp.config.value("data").toUtf8());
                if (doc.isObject()) cell_data = doc.object();
            }

            auto* table = cursor.insertTable(rows, cols, tf);

            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    QString cell_key = QString("%1_%2").arg(r).arg(c);
                    QString cell_text;

                    if (cell_data.contains(cell_key)) {
                        cell_text = cell_data.value(cell_key).toString();
                    } else if (r == 0) {
                        cell_text = QString("Header %1").arg(c + 1);
                    }

                    QTextCharFormat cell_fmt;
                    if (r == 0) {
                        cell_fmt.setFontWeight(QFont::Bold);
                        cell_fmt.setForeground(QColor(theme.table_header_fg));
                        QTextBlockFormat hdr_block;
                        hdr_block.setBackground(QColor(theme.table_header_bg));
                        auto cell_cursor = table->cellAt(r, c).firstCursorPosition();
                        cell_cursor.setBlockFormat(hdr_block);
                        cell_cursor.insertText(cell_text, cell_fmt);
                    } else {
                        cell_fmt.setForeground(QColor(theme.text_color));
                        table->cellAt(r, c).firstCursorPosition().insertText(cell_text, cell_fmt);
                    }
                }
            }
            cursor.movePosition(QTextCursor::End);
            cursor.insertText("\n");

        } else if (comp.type == "code") {
            QTextBlockFormat bf;
            bf.setBackground(QColor(theme.code_bg));
            bf.setLeftMargin(16);
            bf.setRightMargin(16);
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);
            QTextCharFormat fmt;
            fmt.setFontFamily("Consolas");
            fmt.setFontPointSize(11);
            fmt.setForeground(QColor(theme.text_color));
            cursor.insertText(comp.content.isEmpty() ? "// code block" : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "divider") {
            QTextCharFormat fmt;
            fmt.setForeground(QColor(theme.divider_color));
            fmt.setFontPointSize(8);
            cursor.insertText(QString(80, QChar(0x2500)), fmt);
            cursor.insertText("\n");

        } else if (comp.type == "quote") {
            QTextBlockFormat bf;
            bf.setLeftMargin(24);
            bf.setBackground(QColor(theme.quote_bg));
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);
            QTextCharFormat fmt;
            fmt.setFontItalic(true);
            fmt.setForeground(QColor(theme.meta_color));
            fmt.setFontPointSize(12);
            cursor.insertText(comp.content.isEmpty() ? "Block quote..." : comp.content, fmt);
            cursor.insertText("\n");

        } else if (comp.type == "list") {
            QTextListFormat lf;
            lf.setStyle(QTextListFormat::ListDisc);
            lf.setIndent(1);
            QStringList items = comp.content.split("\n", Qt::SkipEmptyParts);
            if (items.isEmpty()) items << "Item 1" << "Item 2";
            QTextList* list = nullptr;
            QTextCharFormat item_fmt;
            item_fmt.setForeground(QColor(theme.text_color));
            for (const auto& item : items) {
                if (!list) {
                    list = cursor.insertList(lf);
                } else {
                    cursor.insertBlock();
                }
                cursor.insertText(item, item_fmt);
            }
            cursor.insertBlock();
            cursor.setBlockFormat(QTextBlockFormat{});

        } else if (comp.type == "chart") {
            QString chart_type  = comp.config.value("chart_type", "line");
            QString chart_title = comp.config.value("title", "Chart");
            QString data_str    = comp.config.value("data", "");
            QString labels_str  = comp.config.value("labels", "");
            int     chart_w     = comp.config.value("width", "640").toInt();

            QTextBlockFormat bf;
            bf.setTopMargin(8);
            bf.setBottomMargin(8);
            bf.setAlignment(Qt::AlignHCenter);
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);

            if (data_str.isEmpty()) {
                QTextCharFormat fmt;
                fmt.setForeground(QColor(theme.meta_color));
                fmt.setFontPointSize(11);
                cursor.insertText(QString("[%1 Chart — enter data in Properties panel]").arg(chart_title), fmt);
            } else {
                QString img_path = render_chart_to_file(chart_type, chart_title,
                                                        data_str, labels_str, theme, chart_w, chart_w / 2);
                if (!img_path.isEmpty()) {
                    QTextImageFormat img_fmt;
                    img_fmt.setName(img_path);
                    img_fmt.setWidth(chart_w);
                    cursor.insertImage(img_fmt);
                } else {
                    QTextCharFormat fmt;
                    fmt.setForeground(QColor(theme.meta_color));
                    fmt.setFontPointSize(11);
                    cursor.insertText("[Chart render failed]", fmt);
                }
            }
            cursor.insertText("\n");

        } else if (comp.type == "image") {
            QString path    = comp.config.value("path", "");
            QString align   = comp.config.value("align", "left");
            QString caption = comp.config.value("caption", "");
            int     width   = comp.config.value("width", "400").toInt();

            // Apply alignment to the block
            QTextBlockFormat img_bf;
            if (i == selected_index) img_bf.setBackground(QColor("#fffbe6"));
            if (align == "center")     img_bf.setAlignment(Qt::AlignHCenter);
            else if (align == "right") img_bf.setAlignment(Qt::AlignRight);
            else                       img_bf.setAlignment(Qt::AlignLeft);
            cursor.setBlockFormat(img_bf);

            if (!path.isEmpty()) {
                QTextImageFormat img_fmt;
                img_fmt.setName(path);
                img_fmt.setWidth(width);
                cursor.insertImage(img_fmt);
            } else {
                QTextCharFormat fmt;
                fmt.setForeground(QColor(theme.meta_color));
                fmt.setFontPointSize(11);
                cursor.insertText("[Image — browse or paste in Properties panel]", fmt);
            }
            cursor.insertText("\n");

            // Caption
            if (!caption.isEmpty()) {
                cursor.insertBlock();
                QTextBlockFormat cap_bf;
                cap_bf.setAlignment(align == "center" ? Qt::AlignHCenter :
                                    align == "right"  ? Qt::AlignRight : Qt::AlignLeft);
                cap_bf.setTopMargin(2);
                cap_bf.setBottomMargin(8);
                cursor.setBlockFormat(cap_bf);
                QTextCharFormat cap_fmt;
                cap_fmt.setFontItalic(true);
                cap_fmt.setFontPointSize(10);
                cap_fmt.setForeground(QColor(theme.meta_color));
                cursor.insertText(caption, cap_fmt);
                cursor.insertText("\n");
            }

        } else if (comp.type == "market_data") {
            QString symbol   = comp.config.value("symbol", "");
            QString price    = comp.config.value("price", "");
            QString change   = comp.config.value("change", "");
            QString chg_pct  = comp.config.value("change_pct", "");
            QString name     = comp.config.value("name", symbol);
            QString high     = comp.config.value("high", "");
            QString low      = comp.config.value("low", "");
            QString volume   = comp.config.value("volume", "");
            QString status   = comp.config.value("status", "pending");

            QTextBlockFormat bf;
            bf.setBackground(QColor(theme.quote_bg));
            bf.setLeftMargin(12);
            bf.setTopMargin(6);
            bf.setBottomMargin(6);
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);

            if (status == "loading") {
                QTextCharFormat fmt;
                fmt.setForeground(QColor(theme.meta_color));
                fmt.setFontPointSize(11);
                cursor.insertText(QString("[Fetching %1...]").arg(symbol.isEmpty() ? "market data" : symbol), fmt);
            } else if (symbol.isEmpty()) {
                QTextCharFormat fmt;
                fmt.setForeground(QColor(theme.meta_color));
                fmt.setFontPointSize(11);
                cursor.insertText("[Market Data — enter ticker in Properties panel]", fmt);
            } else {
                // Line 1: SYMBOL  Name  Price  Change%
                QTextCharFormat sym_fmt;
                sym_fmt.setFontWeight(QFont::Bold);
                sym_fmt.setForeground(QColor(theme.accent_color));
                sym_fmt.setFontPointSize(13);
                cursor.insertText(symbol + "  ", sym_fmt);

                if (!name.isEmpty() && name != symbol) {
                    QTextCharFormat name_fmt;
                    name_fmt.setForeground(QColor(theme.meta_color));
                    name_fmt.setFontPointSize(11);
                    cursor.insertText(name + "  ", name_fmt);
                }

                if (!price.isEmpty()) {
                    QTextCharFormat price_fmt;
                    price_fmt.setFontWeight(QFont::Bold);
                    price_fmt.setForeground(QColor(theme.text_color));
                    price_fmt.setFontPointSize(13);
                    cursor.insertText(price + "  ", price_fmt);
                }

                if (!chg_pct.isEmpty()) {
                    bool negative = chg_pct.startsWith("-");
                    QTextCharFormat chg_fmt;
                    chg_fmt.setFontWeight(QFont::Bold);
                    chg_fmt.setForeground(QColor(negative ? "#dc2626" : "#16a34a"));
                    chg_fmt.setFontPointSize(12);
                    QString sign = (!negative && !chg_pct.startsWith("+")) ? "+" : "";
                    cursor.insertText(sign + chg_pct + "%", chg_fmt);
                }

                // Line 2: H/L/Vol secondary info (only if fetched)
                if (!high.isEmpty() || !low.isEmpty() || !volume.isEmpty()) {
                    cursor.insertBlock();
                    cursor.setBlockFormat(bf);
                    QTextCharFormat meta_fmt;
                    meta_fmt.setFontPointSize(10);
                    meta_fmt.setForeground(QColor(theme.meta_color));

                    QString detail;
                    if (!high.isEmpty() && !low.isEmpty())
                        detail += "H: " + high + "  L: " + low + "  ";
                    if (!volume.isEmpty()) {
                        double vol = volume.toDouble();
                        QString vol_str;
                        if (vol >= 1e9)      vol_str = QString::number(vol / 1e9, 'f', 2) + "B";
                        else if (vol >= 1e6) vol_str = QString::number(vol / 1e6, 'f', 2) + "M";
                        else if (vol >= 1e3) vol_str = QString::number(vol / 1e3, 'f', 1) + "K";
                        else                 vol_str = volume;
                        detail += "Vol: " + vol_str;
                    }
                    cursor.insertText("    " + detail, meta_fmt);
                }
            }
            cursor.insertText("\n");

        } else if (comp.type == "page_break") {
            // Start a new physical page
            current_page = new_page(theme);
            cursor = current_page->textCursor();

        } else if (comp.type == "toc") {
            QTextCharFormat hdr_fmt;
            hdr_fmt.setFontPointSize(14);
            hdr_fmt.setFontWeight(QFont::Bold);
            hdr_fmt.setForeground(QColor(theme.heading_color));
            cursor.insertText("Table of Contents\n", hdr_fmt);

            if (toc_headings.isEmpty()) {
                QTextCharFormat empty_fmt;
                empty_fmt.setForeground(QColor(theme.meta_color));
                empty_fmt.setFontPointSize(11);
                cursor.insertText("  (No headings found in document)\n", empty_fmt);
            } else {
                int num = 1;
                for (const auto& heading : toc_headings) {
                    cursor.insertBlock();
                    QTextCharFormat entry_fmt;
                    entry_fmt.setFontPointSize(11);
                    entry_fmt.setForeground(QColor(theme.text_color));
                    cursor.insertText(QString("  %1.  %2").arg(num++).arg(heading), entry_fmt);
                }
                cursor.insertText("\n");
            }

        } else if (comp.type == "stats_block") {
            // Key-value stats grid rendered as a 2-column table
            QString block_title = comp.config.value("title", "Key Statistics");
            QString data_str    = comp.config.value("data", "");

            // Title row
            QTextCharFormat title_cf;
            title_cf.setFontWeight(QFont::Bold);
            title_cf.setFontPointSize(12);
            title_cf.setForeground(QColor(theme.heading_color));
            cursor.insertText(block_title + "\n", title_cf);

            // Parse "Label:Value\nLabel:Value" lines
            QStringList lines = data_str.split('\n', Qt::SkipEmptyParts);
            if (lines.isEmpty()) {
                QTextCharFormat empty_fmt;
                empty_fmt.setForeground(QColor(theme.meta_color));
                empty_fmt.setFontPointSize(11);
                cursor.insertText("  (Enter key:value pairs in Properties panel)\n", empty_fmt);
            } else {
                QTextTableFormat tf;
                tf.setBorderBrush(QColor(theme.divider_color));
                tf.setBorder(1);
                tf.setCellPadding(5);
                tf.setCellSpacing(0);
                QVector<QTextLength> widths = {
                    QTextLength(QTextLength::PercentageLength, 50),
                    QTextLength(QTextLength::PercentageLength, 50)
                };
                tf.setColumnWidthConstraints(widths);

                int rows = (lines.size() + 1) / 2; // 2 stats per row
                auto* tbl = cursor.insertTable(rows, 4, tf); // label|value|label|value

                QTextTableFormat tf2 = tf;
                tf2.setColumnWidthConstraints({
                    QTextLength(QTextLength::PercentageLength, 25),
                    QTextLength(QTextLength::PercentageLength, 25),
                    QTextLength(QTextLength::PercentageLength, 25),
                    QTextLength(QTextLength::PercentageLength, 25)
                });
                tbl->setFormat(tf2);

                for (int li = 0; li < lines.size(); ++li) {
                    int row = li / 2;
                    int col_offset = (li % 2) * 2;

                    QString label, value;
                    int colon = lines[li].indexOf(':');
                    if (colon >= 0) {
                        label = lines[li].left(colon).trimmed();
                        value = lines[li].mid(colon + 1).trimmed();
                    } else {
                        label = lines[li].trimmed();
                    }

                    // Label cell
                    QTextCharFormat lbl_fmt;
                    lbl_fmt.setForeground(QColor(theme.meta_color));
                    lbl_fmt.setFontPointSize(10);
                    tbl->cellAt(row, col_offset).firstCursorPosition().insertText(label, lbl_fmt);

                    // Value cell
                    QTextCharFormat val_fmt;
                    val_fmt.setFontWeight(QFont::Bold);
                    val_fmt.setForeground(QColor(theme.text_color));
                    val_fmt.setFontPointSize(11);
                    tbl->cellAt(row, col_offset + 1).firstCursorPosition().insertText(value, val_fmt);
                }
                cursor.movePosition(QTextCursor::End);
                cursor.insertText("\n");
            }

        } else if (comp.type == "callout") {
            // Colored callout/alert box: info (blue), success (green), warning (amber), danger (red)
            QString style   = comp.config.value("style", "info");
            QString heading = comp.config.value("heading", "");

            // Pick colors by style
            QString bg_col, border_col, icon;
            if (style == "success") {
                bg_col = "#f0fdf4"; border_col = "#16a34a"; icon = "✓";
            } else if (style == "warning") {
                bg_col = "#fffbeb"; border_col = "#d97706"; icon = "⚠";
            } else if (style == "danger") {
                bg_col = "#fef2f2"; border_col = "#dc2626"; icon = "✗";
            } else { // info
                bg_col = "#eff6ff"; border_col = "#3b82f6"; icon = "ℹ";
            }
            // For dark themes, darken the box bg
            bool dark_theme = QColor(theme.page_bg).lightness() < 100;
            if (dark_theme) {
                if      (style == "success") bg_col = "#052e16";
                else if (style == "warning") bg_col = "#1c1506";
                else if (style == "danger")  bg_col = "#1f0505";
                else                         bg_col = "#0c1a33";
            }

            QTextBlockFormat bf;
            bf.setBackground(QColor(bg_col));
            bf.setLeftMargin(16);
            bf.setRightMargin(8);
            bf.setTopMargin(8);
            bf.setBottomMargin(8);
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);

            // Icon + optional heading
            if (!heading.isEmpty()) {
                QTextCharFormat hdr_fmt;
                hdr_fmt.setFontWeight(QFont::Bold);
                hdr_fmt.setFontPointSize(12);
                hdr_fmt.setForeground(QColor(border_col));
                cursor.insertText(icon + "  " + heading + "\n", hdr_fmt);
                cursor.insertBlock();
                cursor.setBlockFormat(bf);
            } else {
                QTextCharFormat icon_fmt;
                icon_fmt.setFontPointSize(12);
                icon_fmt.setForeground(QColor(border_col));
                cursor.insertText(icon + "  ", icon_fmt);
            }

            QTextCharFormat body_fmt;
            body_fmt.setFontPointSize(11);
            body_fmt.setForeground(QColor(theme.text_color));
            cursor.insertText(comp.content.isEmpty() ? "Enter callout text..." : comp.content, body_fmt);
            cursor.insertText("\n");

        } else if (comp.type == "sparkline") {
            QString spark_title  = comp.config.value("title", "");
            QString data_str     = comp.config.value("data", "");
            QString current_val  = comp.config.value("current", "");
            QString change_pct   = comp.config.value("change_pct", "");

            QTextBlockFormat bf;
            bf.setTopMargin(4);
            bf.setBottomMargin(4);
            if (i == selected_index) bf.setBackground(QColor("#fffbe6"));
            cursor.setBlockFormat(bf);

            // Title + current value inline
            if (!spark_title.isEmpty()) {
                QTextCharFormat t_fmt;
                t_fmt.setFontWeight(QFont::Bold);
                t_fmt.setFontPointSize(11);
                t_fmt.setForeground(QColor(theme.text_color));
                cursor.insertText(spark_title + "  ", t_fmt);
            }
            if (!current_val.isEmpty()) {
                QTextCharFormat v_fmt;
                v_fmt.setFontPointSize(11);
                v_fmt.setForeground(QColor(theme.accent_color));
                cursor.insertText(current_val + "  ", v_fmt);
            }
            if (!change_pct.isEmpty()) {
                bool neg = change_pct.startsWith('-');
                QTextCharFormat c_fmt;
                c_fmt.setFontPointSize(10);
                c_fmt.setForeground(QColor(neg ? "#dc2626" : "#16a34a"));
                cursor.insertText(change_pct + "%  ", c_fmt);
            }

            // Render mini sparkline chart
            if (!data_str.isEmpty()) {
                QString spark_path = render_sparkline_to_file(data_str, theme);
                if (!spark_path.isEmpty()) {
                    QTextImageFormat img_fmt;
                    img_fmt.setName(spark_path);
                    img_fmt.setWidth(160);
                    img_fmt.setHeight(50);
                    cursor.insertImage(img_fmt);
                }
            } else {
                QTextCharFormat hint_fmt;
                hint_fmt.setForeground(QColor(theme.meta_color));
                hint_fmt.setFontPointSize(10);
                cursor.insertText("[Sparkline — enter CSV data in Properties panel]", hint_fmt);
            }
            cursor.insertText("\n");
        }
    }

    current_page->setTextCursor(cursor);
}

} // namespace fincept::screens
