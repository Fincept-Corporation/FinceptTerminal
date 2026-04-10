#include "screens/markets/MarketsScreen.h"

#include "screens/markets/MarketPanelEditor.h"
#include "screens/markets/MarketPanelStore.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QComboBox>
#include <QDateTime>
#include <QLineEdit>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens {

static QString lbl_ss(const QString& color, bool bold = false) {
    return QString("color:%1;background:transparent;%2").arg(color, bold ? "font-weight:bold;" : "");
}

MarketsScreen::MarketsScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_widget_   = build_header();
    controls_widget_ = build_controls();
    root->addWidget(header_widget_);
    root->addWidget(controls_widget_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:%1;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%2;border-radius:3px;min-height:20px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

    grid_content_ = new QWidget;
    auto* cvl = new QVBoxLayout(grid_content_);
    cvl->setContentsMargins(6, 6, 6, 6);
    cvl->setSpacing(8);

    panel_grid_ = new QGridLayout;
    panel_grid_->setSpacing(6);
    panel_grid_->setContentsMargins(0, 0, 0, 0);
    cvl->addLayout(panel_grid_);
    cvl->addStretch();

    scroll->setWidget(grid_content_);
    root->addWidget(scroll, 1);

    // Load configs and build initial grid
    configs_ = MarketPanelStore::instance().load();
    build_panel_grid();

    auto_refresh_timer_ = new QTimer(this);
    auto_refresh_timer_->setInterval(update_interval_ms_);
    connect(auto_refresh_timer_, &QTimer::timeout, this, &MarketsScreen::refresh_all);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

// ---------------------------------------------------------------------------
// Panel grid management
// ---------------------------------------------------------------------------

void MarketsScreen::clear_panel_grid() {
    for (auto* p : panels_)
        p->deleteLater();
    panels_.clear();

    // Remove all items from the grid layout
    QLayoutItem* item;
    while ((item = panel_grid_->takeAt(0)) != nullptr) {
        delete item;
    }
}

void MarketsScreen::build_panel_grid() {
    clear_panel_grid();

    constexpr int kCols = 3;
    for (int i = 0; i < configs_.size(); ++i) {
        auto* p = new MarketPanel(configs_[i], grid_content_);
        panels_.append(p);
        panel_grid_->addWidget(p, i / kCols, i % kCols);

        connect(p, &MarketPanel::edit_requested,   this, &MarketsScreen::open_editor);
        connect(p, &MarketPanel::delete_requested, this, &MarketsScreen::on_panel_delete);
    }
}

void MarketsScreen::open_editor(const QString& panel_id) {
    MarketPanelConfig cfg;
    if (!panel_id.isEmpty()) {
        for (const auto& c : configs_) {
            if (c.id == panel_id) { cfg = c; break; }
        }
    }

    auto* dlg = new MarketPanelEditor(cfg, this);
    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }
    const MarketPanelConfig updated = dlg->result_config();
    dlg->deleteLater();

    if (panel_id.isEmpty()) {
        // New panel
        configs_.append(updated);
    } else {
        for (auto& c : configs_) {
            if (c.id == panel_id) { c = updated; break; }
        }
    }

    MarketPanelStore::instance().save(configs_);
    build_panel_grid();
    refresh_all();
}

void MarketsScreen::on_panel_delete(const QString& panel_id) {
    QMessageBox mb(this);
    mb.setWindowTitle("Remove Panel");
    mb.setText("Remove this panel?");
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    mb.setDefaultButton(QMessageBox::Cancel);
    mb.setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
    if (mb.exec() != QMessageBox::Yes)
        return;

    configs_.erase(std::remove_if(configs_.begin(), configs_.end(),
                                  [&](const MarketPanelConfig& c) { return c.id == panel_id; }),
                   configs_.end());
    MarketPanelStore::instance().save(configs_);
    build_panel_grid();
}

// ---------------------------------------------------------------------------
// Show / hide — timer lifecycle (P3)
// ---------------------------------------------------------------------------

void MarketsScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (auto_update_ && auto_refresh_timer_)
        auto_refresh_timer_->start();
    if (session_timer_)
        session_timer_->start();
    if (clock_timer_)
        clock_timer_->start();
    bool needs_refresh = !last_refresh_time_.isValid() ||
                         last_refresh_time_.secsTo(QDateTime::currentDateTime()) >= kMinRefreshIntervalSec;
    if (needs_refresh)
        refresh_all();
}

void MarketsScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (auto_refresh_timer_) auto_refresh_timer_->stop();
    if (session_timer_)      session_timer_->stop();
    if (clock_timer_)        clock_timer_->stop();
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

QWidget* MarketsScreen::build_header() {
    auto* w = new QWidget(this);
    w->setFixedHeight(40);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(14, 0, 14, 0);
    h->setSpacing(0);

    auto* brand = new QLabel("FINCEPT");
    brand->setStyleSheet(lbl_ss(ui::colors::AMBER(), true));
    h->addWidget(brand);
    auto* sub = new QLabel("  MARKETS");
    sub->setStyleSheet(lbl_ss(ui::colors::TEXT_TERTIARY()));
    h->addWidget(sub);

    auto* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedSize(1, 20);
    sep1->setStyleSheet(QString("background:%1;margin:0 16px;").arg(ui::colors::BORDER_MED()));
    h->addWidget(sep1);

    auto* session_dot = new QLabel("●");
    auto* session_lbl = new QLabel("NYSE  OPEN");
    session_dot->setStyleSheet(lbl_ss(ui::colors::POSITIVE()));
    session_lbl->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    h->addWidget(session_dot);
    h->addWidget(new QLabel(" "));
    h->addWidget(session_lbl);

    auto update_session = [session_dot, session_lbl]() {
        QDateTime utc = QDateTime::currentDateTimeUtc();
        int day  = utc.date().dayOfWeek();
        QDateTime et = utc.addSecs(-4 * 3600);
        int hhmm = et.time().hour() * 100 + et.time().minute();
        bool weekday = (day >= 1 && day <= 5);
        QString label, color;
        if (!weekday || hhmm < 400 || hhmm >= 2000) {
            label = "NYSE  CLOSED";    color = ui::colors::TEXT_TERTIARY();
        } else if (hhmm < 930) {
            label = "NYSE  PRE-MKT";   color = ui::colors::AMBER();
        } else if (hhmm < 1600) {
            label = "NYSE  OPEN";      color = ui::colors::POSITIVE();
        } else {
            label = "NYSE  AFTER-HRS"; color = ui::colors::INFO();
        }
        session_dot->setStyleSheet(lbl_ss(color));
        session_lbl->setStyleSheet(lbl_ss(color, true));
        session_lbl->setText(label);
    };
    update_session();
    session_timer_ = new QTimer(this);
    session_timer_->setInterval(60000);
    connect(session_timer_, &QTimer::timeout, this, update_session);

    h->addStretch();

    auto* ny_lbl  = new QLabel;
    auto* lon_lbl = new QLabel;
    auto* tok_lbl = new QLabel;
    ny_lbl ->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
    lon_lbl->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
    tok_lbl->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));

    auto update_clocks = [ny_lbl, lon_lbl, tok_lbl]() {
        QDateTime utc = QDateTime::currentDateTimeUtc();
        auto fmt = [](const QDateTime& dt, const QString& name) {
            return QString("%1  %2").arg(name, dt.toString("HH:mm"));
        };
        ny_lbl ->setText(fmt(utc.addSecs(-4 * 3600), "NY"));
        lon_lbl->setText(fmt(utc.addSecs( 1 * 3600), "LON"));
        tok_lbl->setText(fmt(utc.addSecs( 9 * 3600), "TOK"));
    };
    update_clocks();
    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, update_clocks);

    auto* dot1 = new QLabel("   ·   ");
    auto* dot2 = new QLabel("   ·   ");
    dot1->setStyleSheet(lbl_ss(ui::colors::BORDER_MED()));
    dot2->setStyleSheet(lbl_ss(ui::colors::BORDER_MED()));
    h->addWidget(ny_lbl);
    h->addWidget(dot1);
    h->addWidget(lon_lbl);
    h->addWidget(dot2);
    h->addWidget(tok_lbl);

    return w;
}

// ---------------------------------------------------------------------------
// Controls bar
// ---------------------------------------------------------------------------

QWidget* MarketsScreen::build_controls() {
    auto* w = new QWidget(this);
    w->setFixedHeight(32);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(14, 0, 14, 0);
    h->setSpacing(4);

    auto make_btn = [](const QString& key, const QString& label) {
        auto* btn = new QPushButton(QString("[%1] %2").arg(key, label));
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 8px;font-weight:bold;}"
                    "QPushButton:hover{background:%4;color:%5;border-color:%5;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(),
                     ui::colors::BG_HOVER(), ui::colors::AMBER()));
        return btn;
    };

    auto* refresh_btn = make_btn("F5", "REFRESH");
    connect(refresh_btn, &QPushButton::clicked, this, &MarketsScreen::refresh_all);
    h->addWidget(refresh_btn);

    auto* auto_btn = make_btn("F9", auto_update_ ? "AUTO ON" : "AUTO OFF");
    auto update_auto_style = [this, auto_btn]() {
        auto_btn->setText(QString("[F9] %1").arg(auto_update_ ? "AUTO ON" : "AUTO OFF"));
        auto_btn->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 8px;font-weight:bold;}"
                    "QPushButton:hover{background:%4;border-color:%5;color:%5;}")
                .arg(ui::colors::BG_RAISED(),
                     auto_update_ ? ui::colors::POSITIVE() : ui::colors::TEXT_TERTIARY(),
                     auto_update_ ? ui::colors::POSITIVE() : ui::colors::BORDER_DIM(),
                     ui::colors::BG_HOVER(), ui::colors::AMBER()));
    };
    update_auto_style();
    connect(auto_btn, &QPushButton::clicked, this, [this, update_auto_style]() {
        auto_update_ = !auto_update_;
        update_auto_style();
        // Only touch the timer if the screen is currently visible (P3)
        if (!isVisible()) return;
        if (auto_update_) auto_refresh_timer_->start();
        else              auto_refresh_timer_->stop();
    });
    h->addWidget(auto_btn);

    // Interval preset combo (non-editable, fixed presets)
    auto* iv = new QComboBox;
    iv->setFixedHeight(22);
    iv->setFixedWidth(60);
    iv->addItem("5m",   300000);
    iv->addItem("10m",  600000);
    iv->addItem("15m",  900000);
    iv->addItem("30m",  1800000);
    iv->addItem("1h",   3600000);
    iv->addItem("4h",   14400000);
    iv->addItem("1d",   86400000);
    iv->addItem("custom", -1);
    iv->setCurrentIndex(1); // default 10m
    iv->setStyleSheet(QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:0 6px;}"
                              "QComboBox::drop-down{border:none;width:14px;}"
                              "QComboBox QAbstractItemView{background:%1;color:%2;border:1px solid %3;"
                              "selection-background-color:%4;}")
                          .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                               ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));

    // Custom interval input — shown only when "custom" is selected
    auto* custom_iv = new QLineEdit;
    custom_iv->setFixedHeight(22);
    custom_iv->setFixedWidth(60);
    custom_iv->setPlaceholderText("e.g. 45m");
    custom_iv->setVisible(false);
    custom_iv->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:0 6px;}"
                "QLineEdit:focus{border-color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::BORDER_MED(), ui::colors::AMBER()));

    auto apply_custom = [this, custom_iv]() {
        QString txt = custom_iv->text().trimmed().toLower();
        if (txt.isEmpty()) return;
        bool ok = false; int ms = 0;
        if (txt.endsWith('h')) {
            int hh = txt.chopped(1).toInt(&ok);
            if (ok) ms = hh * 3600000;
        } else {
            QString num = txt.endsWith('m') ? txt.chopped(1) : txt;
            int m = num.toInt(&ok);
            if (ok) ms = m * 60000;
        }
        ms = qBound(60000, ms, 86400000);
        if (ms > 0) {
            update_interval_ms_ = ms;
            auto_refresh_timer_->setInterval(ms);
        }
    };

    connect(iv, &QComboBox::currentIndexChanged, this, [this, iv, custom_iv](int i) {
        int ms = iv->itemData(i).toInt();
        if (ms == -1) {
            custom_iv->setVisible(true);
            custom_iv->setFocus();
        } else {
            custom_iv->setVisible(false);
            update_interval_ms_ = ms;
            auto_refresh_timer_->setInterval(ms);
        }
    });
    connect(custom_iv, &QLineEdit::editingFinished, this, [apply_custom]() { apply_custom(); });

    h->addWidget(iv);
    h->addWidget(custom_iv);

    // Add panel button
    auto* add_btn = new QPushButton("[+] ADD PANEL");
    add_btn->setFixedHeight(22);
    add_btn->setCursor(Qt::PointingHandCursor);
    add_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 8px;font-weight:bold;}"
                "QPushButton:hover{background:%4;color:%5;border-color:%5;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::POSITIVE(), ui::colors::POSITIVE(),
                 ui::colors::BG_HOVER(), ui::colors::AMBER()));
    connect(add_btn, &QPushButton::clicked, this, [this]() { open_editor({}); });
    h->addWidget(add_btn);

    // Reset to defaults button
    auto* reset_btn = new QPushButton("RESET");
    reset_btn->setFixedHeight(22);
    reset_btn->setCursor(Qt::PointingHandCursor);
    reset_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 8px;font-weight:bold;}"
                "QPushButton:hover{background:%4;color:%5;border-color:%5;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BG_HOVER(), ui::colors::AMBER()));
    connect(reset_btn, &QPushButton::clicked, this, [this]() {
        QMessageBox mb(this);
        mb.setWindowTitle("Reset Panels");
        mb.setText("Reset all panels to defaults?");
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        mb.setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
        if (mb.exec() != QMessageBox::Yes) return;
        MarketPanelStore::instance().reset_to_defaults();
        configs_ = MarketPanelStore::instance().load();
        build_panel_grid();
        refresh_all();
    });
    h->addWidget(reset_btn);

    h->addStretch();

    last_update_label_ = new QLabel("LAST UPDATE  --:--:--");
    last_update_label_->setStyleSheet(lbl_ss(ui::colors::TEXT_DIM()));
    h->addWidget(last_update_label_);

    h->addWidget(new QLabel("   "));

    status_label_ = new QLabel("● READY");
    status_label_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
    h->addWidget(status_label_);

    return w;
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void MarketsScreen::refresh_all() {
    if (refresh_in_progress_)
        return;
    refresh_in_progress_ = true;

    if (status_label_) {
        status_label_->setText("● LOADING");
        status_label_->setStyleSheet(lbl_ss(ui::colors::AMBER(), true));
    }

    for (auto* p : panels_)
        p->refresh();

    const int total = panels_.size();
    if (total == 0) {
        refresh_in_progress_ = false;
        return;
    }

    auto* counter   = new QObject(this);
    auto* remaining = new int(total);
    auto finish = [this, counter, remaining]() {
        if (--(*remaining) > 0)
            return;
        refresh_in_progress_ = false;
        last_refresh_time_   = QDateTime::currentDateTime();
        delete remaining;
        counter->deleteLater();
        if (status_label_) {
            status_label_->setText("● READY");
            status_label_->setStyleSheet(lbl_ss(ui::colors::POSITIVE(), true));
        }
        if (last_update_label_) {
            last_update_label_->setText(
                QString("LAST UPDATE  %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            last_update_label_->setStyleSheet(lbl_ss(ui::colors::TEXT_SECONDARY()));
        }
    };
    for (auto* p : panels_)
        connect(p, &MarketPanel::refresh_finished, counter, finish, Qt::SingleShotConnection);
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void MarketsScreen::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (header_widget_)
        header_widget_->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                          .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    if (controls_widget_)
        controls_widget_->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
}

} // namespace fincept::screens
