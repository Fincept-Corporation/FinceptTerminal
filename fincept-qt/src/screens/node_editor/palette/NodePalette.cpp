#include "screens/node_editor/palette/NodePalette.h"

#include "services/workflow/NodeRegistry.h"
#include "ui/theme/Theme.h"

#include <QDrag>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>

namespace fincept::workflow {

NodePalette::NodePalette(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240);
    setObjectName("nodePalette");
    build_ui();
}

void NodePalette::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ─────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(34);
    header->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_HOVER));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 10, 0);
    auto* title = new QLabel("NODES");
    title->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 12px;"
                                 "font-weight: bold; letter-spacing: 0.5px;")
                             .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();
    root->addWidget(header);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED));
    root->addWidget(sep);

    // ── Search bar ─────────────────────────────────────────────────
    auto* search_wrap = new QWidget(this);
    search_wrap->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_SURFACE));
    auto* sl = new QHBoxLayout(search_wrap);
    sl->setContentsMargins(8, 6, 8, 6);

    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search nodes...");
    search_input_->setStyleSheet(
        QString("QLineEdit {"
                "  background: %1; color: %2; border: 1px solid %3;"
                "  font-family: Consolas; font-size: 12px; padding: 4px 8px;"
                "}"
                "QLineEdit:focus { border: 1px solid %4; }")
            .arg(ui::colors::BG_HOVER, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED, ui::colors::AMBER));
    sl->addWidget(search_input_);
    root->addWidget(search_wrap);

    connect(search_input_, &QLineEdit::textChanged, this, &NodePalette::rebuild_categories);

    // ── Scrollable categories ──────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }"
                                  "QScrollBar:vertical {"
                                  "  background: %1; width: 6px; margin: 0;"
                                  "}"
                                  "QScrollBar::handle:vertical {"
                                  "  background: %2; min-height: 20px;"
                                  "}"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                                  "  height: 0; background: none;"
                                  "}")
                              .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_DIM));

    categories_container_ = new QWidget(this);
    categories_layout_ = new QVBoxLayout(categories_container_);
    categories_layout_->setContentsMargins(0, 0, 0, 0);
    categories_layout_->setSpacing(0);
    categories_layout_->addStretch();

    scroll->setWidget(categories_container_);
    root->addWidget(scroll, 1);

    rebuild_categories();
}

void NodePalette::rebuild_categories(const QString& filter) {
    // Remove existing category widgets (keep the trailing stretch)
    while (categories_layout_->count() > 1) {
        auto* item = categories_layout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    auto& registry = NodeRegistry::instance();
    QStringList cats = registry.categories();

    for (const auto& cat : cats) {
        auto nodes = registry.by_category(cat);

        // Filter nodes by search text
        QVector<NodeTypeDef> filtered;
        for (const auto& n : nodes) {
            if (filter.isEmpty() || n.display_name.contains(filter, Qt::CaseInsensitive) ||
                n.description.contains(filter, Qt::CaseInsensitive) ||
                n.type_id.contains(filter, Qt::CaseInsensitive)) {
                filtered.append(n);
            }
        }

        if (filtered.isEmpty())
            continue;

        // Category header
        auto* cat_header = new QWidget(this);
        cat_header->setFixedHeight(26);
        cat_header->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_HOVER));
        auto* chl = new QHBoxLayout(cat_header);
        chl->setContentsMargins(10, 0, 10, 0);
        auto* cat_label = new QLabel(cat.toUpper());
        QString accent = filtered.first().accent_color;
        cat_label->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px;"
                                         "font-weight: bold; letter-spacing: 0.5px;")
                                     .arg(accent));
        chl->addWidget(cat_label);

        auto* count_label = new QLabel(QString::number(filtered.size()));
        count_label->setStyleSheet(
            QString("color: %1; font-family: Consolas; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY));
        chl->addStretch();
        chl->addWidget(count_label);

        int insert_pos = categories_layout_->count() - 1; // before stretch
        categories_layout_->insertWidget(insert_pos, cat_header);

        // Node buttons
        for (const auto& n : filtered) {
            auto* btn = new QPushButton;
            btn->setFixedHeight(32);
            btn->setCursor(Qt::OpenHandCursor);
            btn->setStyleSheet(
                QString("QPushButton {"
                        "  background: %1; color: %2; border: none;"
                        "  border-bottom: 1px solid %3; font-family: Consolas;"
                        "  font-size: 11px; text-align: left; padding: 0 10px;"
                        "}"
                        "QPushButton:hover {"
                        "  background: %4; color: %2;"
                        "}")
                    .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BG_HOVER, ui::colors::BG_HOVER));

            btn->setToolTip(n.description);

            // Layout inside button: icon + name
            auto* bl = new QHBoxLayout(btn);
            bl->setContentsMargins(10, 0, 8, 0);
            bl->setSpacing(6);

            auto* icon = new QLabel(n.icon_text);
            icon->setFixedWidth(20);
            icon->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;"
                                        "font-weight: bold;")
                                    .arg(accent));
            icon->setAttribute(Qt::WA_TransparentForMouseEvents);
            bl->addWidget(icon);

            auto* name = new QLabel(n.display_name);
            name->setStyleSheet(
                QString("color: %1; font-family: Consolas; font-size: 11px;").arg(ui::colors::TEXT_PRIMARY));
            name->setAttribute(Qt::WA_TransparentForMouseEvents);
            bl->addWidget(name, 1);

            QString type_id = n.type_id;
            connect(btn, &QPushButton::pressed, this, [this, type_id]() { start_drag(type_id); });

            categories_layout_->insertWidget(categories_layout_->count() - 1, btn);
        }
    }
}

void NodePalette::start_drag(const QString& type_id) {
    auto& registry = NodeRegistry::instance();
    const auto* type_def = registry.find(type_id);

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData;
    mime->setData("application/x-fincept-node-type", type_id.toUtf8());
    drag->setMimeData(mime);

    // Create drag preview pixmap
    QString label = type_def ? type_def->display_name : type_id;
    QFont font("Consolas", 10);
    font.setBold(true);
    QFontMetrics fm(font);
    int text_w = fm.horizontalAdvance(label) + 20;
    int h = 24;

    QPixmap pixmap(text_w, h);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(ui::colors::BG_HOVER.get()));
    painter.drawRect(0, 0, text_w, h);

    // Accent left bar
    QString accent = type_def ? type_def->accent_color : QString(ui::colors::AMBER.get());
    painter.setBrush(QColor(accent));
    painter.drawRect(0, 0, 3, h);

    // Text
    painter.setFont(font);
    painter.setPen(QColor(ui::colors::TEXT_PRIMARY.get()));
    painter.drawText(QRect(8, 0, text_w - 8, h), Qt::AlignVCenter, label);

    // Border
    painter.setPen(QPen(QColor(ui::colors::BORDER_MED.get()), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(0, 0, text_w - 1, h - 1);
    painter.end();

    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(text_w / 2, h / 2));
    drag->exec(Qt::CopyAction);
}

} // namespace fincept::workflow
