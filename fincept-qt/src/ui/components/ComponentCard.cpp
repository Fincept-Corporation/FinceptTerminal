#include "ui/components/ComponentCard.h"

#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QDrag>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QVBoxLayout>

namespace fincept::ui {

namespace {

constexpr const char* kScreenIdMime = "application/x-fincept-screen-id";

/// Category accent: a fixed slot of the theme's categorical palette per
/// category (never a price-direction hue), legible on either theme.
QString category_color(const QString& category) {
    static const QStringList kCategories = {"Core",      "Trading",     "QuantLib", "Tools",    "Economics",
                                            "Community", "Geopolitics", "AI",       "Research"};
    const int slot = kCategories.indexOf(category);
    if (slot < 0)
        return colors::TEXT_SECONDARY();
    // Core amber, Trading sky, QuantLib purple, Tools periwinkle, Economics
    // blue, Community tan, Geopolitics orange, AI magenta, Research cyan.
    static const int kSlots[] = {0, 1, 2, 6, 4, 5, 9, 7, 8};
    return colors::chart_color(kSlots[slot]).name();
}

/// Logarithmic thresholds: one star after a couple visits, five at 150+.
QString popularity_stars(int count) {
    int filled = 0;
    if (count >= 1)
        filled = 1;
    if (count >= 5)
        filled = 2;
    if (count >= 20)
        filled = 3;
    if (count >= 50)
        filled = 4;
    if (count >= 150)
        filled = 5;

    QString out;
    for (int i = 0; i < 5; ++i)
        out += (i < filled) ? QChar(0x2605) : QChar(0x2606); // ★ / ☆
    return out;
}

} // namespace

ComponentCard::ComponentCard(ComponentMeta meta, QWidget* parent) : QFrame(parent), meta_(std::move(meta)) {
    setCursor(Qt::PointingHandCursor);
    setObjectName("componentCard");
    setAttribute(Qt::WA_Hover, true);
    build_ui();
    restyle();
    apply_metrics();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { apply_metrics(); });
}

void ComponentCard::apply_metrics() {
    // 260 x 120 at the default font size, scaled with the user's font; the
    // description keeps room for two lines of its text.
    setFixedSize(metrics::width(260), metrics::width(120));
    if (auto* desc = findChild<QLabel*>(QStringLiteral("componentCardDesc")))
        desc->setMaximumHeight(QFontMetrics(fonts::font(-3)).lineSpacing() * 2 + 4);
}

void ComponentCard::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* strip = new QWidget(this);
    strip->setFixedHeight(6);
    strip->setStyleSheet(QString("background:%1;").arg(category_color(meta_.category)));
    root->addWidget(strip);

    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 8, 12, 8);
    bl->setSpacing(4);

    auto* cat_row = new QHBoxLayout;
    cat_row->setContentsMargins(0, 0, 0, 0);
    cat_row->setSpacing(0);
    auto* cat = new QLabel(meta_.category.toUpper(), body);
    cat->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;letter-spacing:1.5px;font-weight:700;").arg(category_color(meta_.category)));
    cat_row->addWidget(cat);
    cat_row->addStretch(1);
    auto* stars = new QLabel(popularity_stars(meta_.popularity), body);
    stars->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;").arg(colors::AMBER()));
    stars->setToolTip(QString("Use count: %1").arg(meta_.popularity));
    cat_row->addWidget(stars);
    bl->addLayout(cat_row);

    auto* title = new QLabel(meta_.title, body);
    title->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(0) + "px;font-weight:700;").arg(colors::TEXT_PRIMARY()));
    bl->addWidget(title);

    auto* desc = new QLabel(body);
    desc->setObjectName(QStringLiteral("componentCardDesc"));
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(colors::TEXT_SECONDARY()));
    const QString text = meta_.description.isEmpty() ? QStringLiteral(" ") : meta_.description;
    desc->setText(text);
    bl->addWidget(desc);
    bl->addStretch(1);

    root->addWidget(body, 1);
}

void ComponentCard::restyle() {
    QString border = colors::BORDER_MED();
    QString bg = colors::BG_SURFACE();
    if (selected_) {
        border = colors::AMBER();
        bg = colors::BG_HOVER();
    } else if (hovered_) {
        border = colors::BORDER_BRIGHT();
        bg = colors::BG_RAISED();
    }
    setStyleSheet(QString("#componentCard{background:%1;border:1px solid %2;border-radius:2px;}").arg(bg, border));
}

void ComponentCard::set_selected(bool s) {
    if (selected_ == s)
        return;
    selected_ = s;
    restyle();
}

void ComponentCard::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        press_pos_ = e->pos();
        pressed_ = true;
        set_selected(true);
        emit selected_changed(meta_.id);
        e->accept();
        return;
    }
    QFrame::mousePressEvent(e);
}

void ComponentCard::mouseMoveEvent(QMouseEvent* e) {
    if (!pressed_ || !(e->buttons() & Qt::LeftButton))
        return;
    if ((e->pos() - press_pos_).manhattanLength() < QApplication::startDragDistance())
        return;
    pressed_ = false;

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData;
    mime->setData(kScreenIdMime, meta_.id.toUtf8());
    mime->setText(meta_.title); // fallback for generic text targets
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

void ComponentCard::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        emit activated(meta_.id);
        e->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(e);
}

void ComponentCard::enterEvent(QEnterEvent* e) {
    hovered_ = true;
    restyle();
    QFrame::enterEvent(e);
}

void ComponentCard::leaveEvent(QEvent* e) {
    hovered_ = false;
    restyle();
    QFrame::leaveEvent(e);
}

} // namespace fincept::ui
