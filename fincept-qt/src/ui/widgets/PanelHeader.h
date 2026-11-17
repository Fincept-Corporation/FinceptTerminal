// src/ui/widgets/PanelHeader.h
#pragma once
// PanelHeader — the uppercase header strip every dense terminal panel starts with
// (the Portfolio / Watchlist / Markets look, generalised from
// screens/portfolio/PortfolioPanelHeader.h so every screen shares one):
//
//   ▌ TITLE  12   …………………………………………  [caller's controls]
//
//   * BG_SURFACE strip, 1 px BORDER_DIM rule underneath;
//   * an accent tick (3 px wide, as tall as the title's cap height), the title in
//     the user's font at base-3 px, bold, 1.2 px tracking, upper case;
//   * an optional dim count / badge after the title (set_badge);
//   * a right-hand controls slot with its own QHBoxLayout (add_control).
//
// Header-only and not a Q_OBJECT (no moc needed anywhere it is included). Styling
// is one selector stylesheet set before any child exists, built from theme tokens,
// so the central re-style pass keeps it in step with theme and font changes; the
// height and tick follow the font and are re-fitted on
// ThemeManager::appearance_settled (never in a theme_changed handler).
//
// USAGE
//   auto* h = new fincept::ui::PanelHeader(tr("HOLDINGS"), panel);
//   h->set_badge(QString::number(rows));
//   h->add_control(filter_edit);
//   panel_layout->addWidget(h);
// or, as a drop-in for the old Portfolio helper:
//   auto parts = fincept::ui::make_panel_header(tr("HOLDINGS"), panel);
//   parts.controls_slot->layout()->addWidget(btn);

#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QWidget>

#include <algorithm>

namespace fincept::ui {

class PanelHeader : public QWidget {
  public:
    explicit PanelHeader(const QString& title = {}, QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("uiPanelHeader"));
        setAttribute(Qt::WA_StyledBackground, true);
        // One sheet, before any child exists; object-name selectors only.
        setStyleSheet(sheet());
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(12, 0, 10, 0);
        row->setSpacing(8);

        tick_ = new QLabel(this);
        tick_->setObjectName(QStringLiteral("uiPanelHeaderTick"));
        row->addWidget(tick_, 0, Qt::AlignVCenter);

        title_ = new QLabel(this);
        title_->setObjectName(QStringLiteral("uiPanelHeaderTitle"));
        title_->setTextFormat(Qt::PlainText);
        row->addWidget(title_, 0, Qt::AlignVCenter);

        badge_ = new QLabel(this);
        badge_->setObjectName(QStringLiteral("uiPanelHeaderBadge"));
        badge_->setTextFormat(Qt::PlainText);
        badge_->setVisible(false);
        row->addWidget(badge_, 0, Qt::AlignVCenter);

        row->addStretch(1);

        controls_ = new QWidget(this);
        controls_->setObjectName(QStringLiteral("uiPanelHeaderControls"));
        controls_layout_ = new QHBoxLayout(controls_);
        controls_layout_->setContentsMargins(0, 0, 0, 0);
        controls_layout_->setSpacing(6);
        row->addWidget(controls_, 0, Qt::AlignVCenter);

        set_title(title);
        fit();
        // Font size changed: the strip and tick follow (after every widget has its new style).
        QObject::connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { fit(); });
    }

    void set_title(const QString& title) {
        const QString t = title.toUpper();
        if (title_->text() != t)
            title_->setText(t);
        title_->setAccessibleName(title);
    }
    QString title() const { return title_->text(); }

    /// Dim text after the title (a count, "12 / 40", "LIVE"); empty hides it.
    void set_badge(const QString& text) {
        if (badge_->text() != text)
            badge_->setText(text);
        const bool show = !text.isEmpty();
        if (badge_->isVisibleTo(this) != show)
            badge_->setVisible(show);
    }

    QLabel* title_label() const { return title_; }
    QLabel* badge_label() const { return badge_; }
    /// The right-hand slot (already has a QHBoxLayout, spacing 6).
    QWidget* controls() const { return controls_; }
    QHBoxLayout* controls_layout() const { return controls_layout_; }
    void add_control(QWidget* w, int stretch = 0) {
        if (!w)
            return;
        if (w->parentWidget() != controls_)
            w->setParent(controls_);
        controls_layout_->addWidget(w, stretch, Qt::AlignVCenter);
    }

    /// Height of the strip at the current font: 30 px at the default size, growing
    /// with the title font so nothing clips at a large base size.
    static int strip_height() {
        const QFontMetrics fm(fonts::font(-3, QFont::Bold));
        return std::max(30, fm.height() + 16);
    }

  private:
    static QString sheet() {
        return QStringLiteral("QWidget#uiPanelHeader{background:%1;border:none;border-bottom:1px solid %2;}"
                              "QLabel#uiPanelHeaderTick{background:%3;border:none;}"
                              "QLabel#uiPanelHeaderTitle{color:%4;background:transparent;font-family:%5;"
                              "font-size:%6px;font-weight:700;letter-spacing:1.2px;}"
                              "QLabel#uiPanelHeaderBadge{color:%7;background:transparent;font-family:%5;"
                              "font-size:%8px;font-weight:600;}"
                              "QWidget#uiPanelHeaderControls{background:transparent;}")
            .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER(), colors::TEXT_PRIMARY(),
                 QString(fonts::DATA_FAMILY))
            .arg(fonts::px(-3))
            .arg(colors::TEXT_TERTIARY())
            .arg(fonts::px(-4));
    }

    void fit() {
        const int h = strip_height();
        if (minimumHeight() != h)
            setMinimumHeight(h);
        const QFontMetrics fm(fonts::font(-3, QFont::Bold));
        const QSize tick(3, std::max(10, fm.capHeight() + 4));
        if (tick_->size() != tick)
            tick_->setFixedSize(tick);
    }

    QLabel* tick_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* badge_ = nullptr;
    QWidget* controls_ = nullptr;
    QHBoxLayout* controls_layout_ = nullptr;
};

/// Drop-in for screens::make_panel_header (Portfolio): the same three handles.
struct PanelHeaderParts {
    PanelHeader* header = nullptr;
    QWidget* controls_slot = nullptr; ///< has a QHBoxLayout already
    QLabel* title_label = nullptr;
};

inline PanelHeaderParts make_panel_header(const QString& title, QWidget* parent = nullptr) {
    PanelHeaderParts p;
    p.header = new PanelHeader(title, parent);
    p.controls_slot = p.header->controls();
    p.title_label = p.header->title_label();
    return p;
}

} // namespace fincept::ui
