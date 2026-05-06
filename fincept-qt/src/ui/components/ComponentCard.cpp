#include "ui/components/ComponentCard.h"

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

QString category_color(const QString& category) {
    if (category == "Core")        return "#d97706"; // amber
    if (category == "Trading")     return "#16a34a"; // green
    if (category == "Research")    return "#0891b2"; // cyan
    if (category == "QuantLib")    return "#7c3aed"; // purple
    if (category == "AI")          return "#c026d3"; // magenta
    if (category == "Economics")   return "#2563eb"; // blue
    if (category == "Geopolitics") return "#dc2626"; // red
    if (category == "Tools")       return "#64748b"; // slate
    if (category == "Community")   return "#059669"; // emerald
    return "#4b5563";
}

/// Logarithmic thresholds: one star after a couple visits, five at 150+.
QString popularity_stars(int count) {
    int filled = 0;
    if (count >= 1)  filled = 1;
    if (count >= 5)  filled = 2;
    if (count >= 20) filled = 3;
    if (count >= 50) filled = 4;
    if (count >= 150) filled = 5;

    QString out;
    for (int i = 0; i < 5; ++i)
        out += (i < filled) ? QChar(0x2605) : QChar(0x2606); // ★ / ☆
    return out;
}

} // namespace

ComponentCard::ComponentCard(ComponentMeta meta, QWidget* parent)
    : QFrame(parent), meta_(std::move(meta)) {
    setFixedSize(260, 120);
    setCursor(Qt::PointingHandCursor);
    setObjectName("componentCard");
    setAttribute(Qt::WA_Hover, true);
    build_ui();
    restyle();
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
    cat->setStyleSheet(QString("color:%1;font-size:10px;letter-spacing:1.5px;font-weight:700;")
                           .arg(category_color(meta_.category)));
    cat_row->addWidget(cat);
    cat_row->addStretch(1);
    auto* stars = new QLabel(popularity_stars(meta_.popularity), body);
    stars->setStyleSheet("color:#d97706;font-size:12px;");
    stars->setToolTip(QString("Use count: %1").arg(meta_.popularity));
    cat_row->addWidget(stars);
    bl->addLayout(cat_row);

    auto* title = new QLabel(meta_.title, body);
    title->setStyleSheet("color:#f3f4f6;font-size:14px;font-weight:700;");
    bl->addWidget(title);

    auto* desc = new QLabel(body);
    desc->setWordWrap(true);
    desc->setStyleSheet("color:#9ca3af;font-size:11px;");
    const QString text = meta_.description.isEmpty() ? QStringLiteral(" ") : meta_.description;
    desc->setText(text);
    desc->setMaximumHeight(36);
    bl->addWidget(desc);
    bl->addStretch(1);

    root->addWidget(body, 1);
}

void ComponentCard::restyle() {
    QString border = "#374151";
    QString bg = "#111827";
    if (selected_) {
        border = "#d97706";
        bg = "#1f2937";
    } else if (hovered_) {
        border = "#6b7280";
        bg = "#1a2332";
    }
    setStyleSheet(QString("#componentCard{background:%1;border:1px solid %2;border-radius:4px;}")
                      .arg(bg, border));
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
