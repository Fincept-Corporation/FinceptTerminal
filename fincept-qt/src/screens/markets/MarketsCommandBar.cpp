#include "screens/markets/MarketsCommandBar.h"

#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketsStyle.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"
#include "ui/widgets/ThemeIcon.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>

namespace fincept::screens {

namespace colors = fincept::ui::colors;

namespace {

/// Below this bar width the view switch moves into the overflow menu.
constexpr int kCollapseWidth = 900;

QString mcb_sheet() {
    return QStringLiteral("#mcbRoot{background:%1;border-bottom:1px solid %2;}"
                          "QToolButton#mcbIcon{background:transparent;border:1px solid transparent;padding:3px;}"
                          "QToolButton#mcbIcon:hover{border-color:%3;}"
                          "QToolButton#mcbIcon:disabled{background:transparent;}"
                          "QComboBox#mcbInterval{background:transparent;color:%4;border:1px solid %2;padding:1px 6px;"
                          "font-family:%5;font-size:%6px;font-weight:600;}"
                          "QComboBox#mcbInterval::drop-down{border:none;width:14px;}"
                          "QComboBox#mcbInterval QAbstractItemView{background:%7;color:%8;border:1px solid %3;"
                          "selection-background-color:%9;}")
               .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::BORDER_MED(), colors::TEXT_SECONDARY(),
                    QString(ui::fonts::DATA_FAMILY))
               .arg(ui::fonts::px(-3))
               .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BG_HOVER()) +
           markets_bar_buttons_css();
}

struct IntervalChoice {
    int ms; ///< -1 = OFF
    const char* label;
};
constexpr IntervalChoice kIntervals[] = {
    {0, QT_TRANSLATE_NOOP("MarketsCommandBar", "LIVE")},     {60000, QT_TRANSLATE_NOOP("MarketsCommandBar", "1M")},
    {300000, QT_TRANSLATE_NOOP("MarketsCommandBar", "5M")},  {600000, QT_TRANSLATE_NOOP("MarketsCommandBar", "10M")},
    {900000, QT_TRANSLATE_NOOP("MarketsCommandBar", "15M")}, {1800000, QT_TRANSLATE_NOOP("MarketsCommandBar", "30M")},
    {3600000, QT_TRANSLATE_NOOP("MarketsCommandBar", "1H")}, {-1, QT_TRANSLATE_NOOP("MarketsCommandBar", "OFF")},
};

} // namespace

MarketsCommandBar::MarketsCommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("mcbRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, mcb_sheet()); // before any child exists
    setMinimumHeight(ui::metrics::height(36));

    row_ = new QHBoxLayout(this);
    row_->setContentsMargins(12, 0, 10, 0);
    row_->setSpacing(8);

    // View switch (exclusive): PANELS / HEATMAP / MOVERS.
    views_ = new QButtonGroup(this);
    views_->setExclusive(true);
    auto make_view = [&](View v) {
        auto* b = new QPushButton(this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(ui::metrics::height(24));
        views_->addButton(b, static_cast<int>(v));
        row_->addWidget(b);
        return b;
    };
    panels_btn_ = make_view(View::Panels);
    heatmap_btn_ = make_view(View::Heatmap);
    movers_btn_ = make_view(View::Movers);
    panels_btn_->setChecked(true);
    connect(views_, &QButtonGroup::idClicked, this, [this](int id) { emit view_selected(static_cast<View>(id)); });

    row_->addStretch(1);

    layout_btn_ = new QToolButton(this);
    layout_btn_->setCursor(Qt::PointingHandCursor);
    layout_btn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    layout_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::ChevronDown));
    layout_btn_->setLayoutDirection(Qt::RightToLeft); // chevron after the text
    layout_btn_->setMinimumHeight(ui::metrics::height(24));
    connect(layout_btn_, &QToolButton::clicked, this, &MarketsCommandBar::show_layout_menu);
    row_->addWidget(layout_btn_);

    detail_btn_ = new QPushButton(this);
    detail_btn_->setCheckable(true);
    detail_btn_->setCursor(Qt::PointingHandCursor);
    detail_btn_->setMinimumHeight(ui::metrics::height(24));
    connect(detail_btn_, &QPushButton::toggled, this, [this](bool on) {
        if (!updating_)
            emit detail_toggled(on);
    });
    row_->addWidget(detail_btn_);

    refresh_btn_ = new QToolButton(this);
    refresh_btn_->setObjectName(QStringLiteral("mcbIcon"));
    refresh_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::Refresh));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setFixedSize(ui::metrics::height(24), ui::metrics::height(24));
    connect(refresh_btn_, &QToolButton::clicked, this, &MarketsCommandBar::refresh_requested);
    row_->addWidget(refresh_btn_);

    interval_ = new QComboBox(this);
    interval_->setObjectName(QStringLiteral("mcbInterval"));
    for (const auto& c : kIntervals)
        interval_->addItem(QCoreApplication::translate("MarketsCommandBar", c.label), c.ms);
    interval_->setFixedWidth(QFontMetrics(ui::fonts::font(-3, QFont::DemiBold)).horizontalAdvance(QStringLiteral("LIVE")) +
                             ui::metrics::width(40));
    interval_->setMinimumHeight(ui::metrics::height(24));
    connect(interval_, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (!updating_ && i >= 0)
            emit interval_changed(interval_->itemData(i).toInt());
    });
    row_->addWidget(interval_);

    overflow_btn_ = new QToolButton(this);
    overflow_btn_->setObjectName(QStringLiteral("mcbIcon"));
    overflow_btn_->setIcon(ui::theme_icon(ui::ThemeGlyph::Menu));
    overflow_btn_->setCursor(Qt::PointingHandCursor);
    overflow_btn_->setFixedSize(ui::metrics::height(24), ui::metrics::height(24));
    connect(overflow_btn_, &QToolButton::clicked, this, &MarketsCommandBar::show_overflow_menu);
    row_->addWidget(overflow_btn_);

    retranslate();
}

void MarketsCommandBar::set_symbol_entry(QWidget* entry) {
    if (entry_ == entry)
        return;
    entry_ = entry;
    if (!entry)
        return;
    entry->setParent(this);
    row_->insertWidget(0, entry, 0);
    setTabOrder(entry, panels_btn_);
}

void MarketsCommandBar::set_view(View view) {
    if (QAbstractButton* b = views_->button(static_cast<int>(view)))
        b->setChecked(true);
}

void MarketsCommandBar::set_detail_checked(bool on) {
    updating_ = true;
    detail_btn_->setChecked(on);
    updating_ = false;
}

void MarketsCommandBar::set_interval_ms(int ms, bool auto_on) {
    const int idx = interval_->findData(auto_on ? ms : -1);
    if (idx < 0 || idx == interval_->currentIndex())
        return;
    updating_ = true;
    interval_->setCurrentIndex(idx);
    updating_ = false;
}

void MarketsCommandBar::set_refreshing(bool on) {
    if (refresh_btn_->isEnabled() == !on)
        return;
    refresh_btn_->setEnabled(!on); // the status bar shows the progress meanwhile
}

void MarketsCommandBar::set_pulse_checked(bool on) {
    pulse_checked_ = on;
}

void MarketsCommandBar::set_columns_hint(const QStringList& ids) {
    columns_hint_ = ids;
}

void MarketsCommandBar::refresh_theme() {
    // Colours come from the bar's sheet (rewritten by the central re-style pass) and
    // the icons are painted from the theme at paint time: nothing to do here.
    update();
}

void MarketsCommandBar::show_layout_menu() {
    QMenu menu(this);
    markets_set_sheet(&menu, markets_menu_css());
    connect(menu.addAction(tr("Add panel...")), &QAction::triggered, this, &MarketsCommandBar::add_panel_requested);
    connect(menu.addAction(tr("Reset to default panels...")), &QAction::triggered, this,
            &MarketsCommandBar::reset_requested);
    menu.actions().constLast()->setProperty("fincept_agent_risk", "destructive"); // resets the layout
    menu.addSeparator();
    connect(menu.addAction(tr("Equalise panel sizes")), &QAction::triggered, this,
            &MarketsCommandBar::equalise_requested);
    QAction* pulse = menu.addAction(tr("Show market pulse"));
    pulse->setCheckable(true);
    pulse->setChecked(pulse_checked_);
    connect(pulse, &QAction::toggled, this, [this](bool on) {
        pulse_checked_ = on;
        emit pulse_toggled(on);
    });
    menu.exec(layout_btn_->mapToGlobal(QPoint(0, layout_btn_->height())));
}

void MarketsCommandBar::show_overflow_menu() {
    QMenu menu(this);
    markets_set_sheet(&menu, markets_menu_css());
    if (collapsed_) {
        // The view switch lives here at narrow widths.
        const struct {
            QPushButton* btn;
            View view;
        } views[] = {{panels_btn_, View::Panels}, {heatmap_btn_, View::Heatmap}, {movers_btn_, View::Movers}};
        for (const auto& v : views) {
            QAction* a = menu.addAction(v.btn->text());
            a->setCheckable(true);
            a->setChecked(v.btn->isChecked());
            const View view = v.view;
            connect(a, &QAction::triggered, this, [this, view]() {
                set_view(view);
                emit view_selected(view);
            });
        }
        menu.addSeparator();
    }
    connect(menu.addAction(tr("Export all panels (CSV)...")), &QAction::triggered, this,
            &MarketsCommandBar::export_all_requested);
    QMenu* cols = menu.addMenu(tr("Columns for all panels"));
    for (int c = 0; c < mcols::count(); ++c) {
        if (mcols::def(c).internal)
            continue;
        QAction* a = cols->addAction(mcols::header(c));
        a->setCheckable(true);
        const QString id = mcols::id_at(c);
        a->setChecked(columns_hint_.contains(id));
        if (mcols::locked(c)) {
            a->setEnabled(false);
            continue;
        }
        connect(a, &QAction::toggled, this, [this, id](bool on) {
            QStringList ids = columns_hint_.isEmpty() ? mcols::default_ids() : columns_hint_;
            if (on && !ids.contains(id))
                ids << id;
            else if (!on)
                ids.removeAll(id);
            columns_hint_ = mcols::sanitize_ids(ids);
            emit columns_for_all_changed(columns_hint_);
        });
    }
    connect(menu.addAction(tr("Reset column widths")), &QAction::triggered, this,
            &MarketsCommandBar::reset_widths_requested);
    menu.exec(overflow_btn_->mapToGlobal(QPoint(0, overflow_btn_->height())));
}

void MarketsCommandBar::apply_collapse() {
    const bool collapse = width() > 0 && width() < kCollapseWidth;
    if (collapse == collapsed_)
        return;
    collapsed_ = collapse;
    for (QPushButton* b : {panels_btn_, heatmap_btn_, movers_btn_})
        b->setVisible(!collapse);
}

QSize MarketsCommandBar::minimumSizeHint() const {
    return {0, QWidget::minimumSizeHint().height()};
}

void MarketsCommandBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_collapse(); // visibility changes here only, never in a theme handler
}

void MarketsCommandBar::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void MarketsCommandBar::retranslate() {
    panels_btn_->setText(tr("PANELS"));
    heatmap_btn_->setText(tr("HEATMAP"));
    movers_btn_->setText(tr("MOVERS"));
    panels_btn_->setToolTip(tr("Market panels (Alt+1)"));
    heatmap_btn_->setToolTip(tr("Heatmap of every panel symbol (Alt+2)"));
    movers_btn_->setToolTip(tr("Top gainers, losers and most extended among your panel symbols (Alt+3)"));
    layout_btn_->setText(tr("LAYOUT"));
    layout_btn_->setToolTip(tr("Add, reset or resize panels"));
    detail_btn_->setText(tr("DETAIL"));
    detail_btn_->setToolTip(tr("Show the chart and key figures of the selected symbol (Ctrl+D)"));
    refresh_btn_->setToolTip(tr("Refresh all quotes (F5)"));
    refresh_btn_->setAccessibleName(tr("Refresh all quotes"));
    interval_->setToolTip(tr("How often panel quotes are refreshed. The market pulse is always live."));
    interval_->setAccessibleName(tr("Panel refresh interval"));
    for (int i = 0; i < interval_->count() && i < static_cast<int>(std::size(kIntervals)); ++i)
        interval_->setItemText(i, QCoreApplication::translate("MarketsCommandBar", kIntervals[i].label));
    overflow_btn_->setToolTip(tr("More: export, columns for all panels"));
    overflow_btn_->setAccessibleName(tr("More actions"));
}

} // namespace fincept::screens
