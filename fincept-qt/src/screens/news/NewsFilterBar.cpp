#include "screens/news/NewsFilterBar.h"
#include "ui/theme/Theme.h"
#include <QFrame>

namespace fincept::screens {

static const QString BTN_ACTIVE =
    "QPushButton{background:#b45309;color:#e5e5e5;border:1px solid #4d3300;"
    "padding:0 10px;font-size:12px;font-weight:700;letter-spacing:0.5px;"
    "font-family:'Consolas','Courier New',monospace;}";
static const QString BTN_INACTIVE =
    "QPushButton{background:transparent;color:#525252;border:1px solid #1a1a1a;"
    "padding:0 10px;font-size:12px;letter-spacing:0.5px;"
    "font-family:'Consolas','Courier New',monospace;}"
    "QPushButton:hover{color:#808080;background:#111111;}";

NewsFilterBar::NewsFilterBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
        .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 0, 12, 0);
    root->setSpacing(2);

    // ── Category F-key buttons ──────────────────────────────────────────────
    QVector<QPair<QString,QString>> cats = {
        {"ALL","ALL"}, {"MKT","MKT"}, {"EARN","EARN"}, {"ECO","ECO"},
        {"TECH","TECH"}, {"NRG","NRG"}, {"CRPT","CRPT"}, {"GEO","GEO"}, {"DEF","DEF"},
    };
    for (const auto& [label, id] : cats) {
        auto* btn = make_filter_btn(label, id);
        category_btns_.append(btn);
        root->addWidget(btn);
    }

    // Separator
    auto* sep1 = new QFrame; sep1->setFixedSize(1, 20);
    sep1->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM));
    root->addWidget(sep1);

    // ── Time range buttons ──────────────────────────────────────────────────
    QVector<QPair<QString,QString>> times = {
        {"1H","1H"}, {"6H","6H"}, {"24H","24H"}, {"48H","48H"}, {"7D","7D"},
    };
    for (const auto& [label, id] : times) {
        auto* btn = make_toggle_btn(label, id);
        time_btns_.append(btn);
        root->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, id=id]() {
            active_time_ = id;
            update_btn_styles(time_btns_, active_time_);
            emit time_range_changed(id);
        });
    }

    auto* sep2 = new QFrame; sep2->setFixedSize(1, 20);
    sep2->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM));
    root->addWidget(sep2);

    // ── View mode buttons ───────────────────────────────────────────────────
    QVector<QPair<QString,QString>> views = {
        {"WIRE","WIRE"}, {"CLUSTERED","CLUSTERED"}, {"BY-SOURCE","BY-SOURCE"},
    };
    for (const auto& [label, id] : views) {
        auto* btn = make_toggle_btn(label, id);
        view_btns_.append(btn);
        root->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, id=id]() {
            active_view_ = id;
            update_btn_styles(view_btns_, active_view_);
            emit view_mode_changed(id);
        });
    }

    root->addSpacing(6);

    // ── Search ──────────────────────────────────────────────────────────────
    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search...");
    search_input_->setFixedSize(160, 28);
    search_input_->setStyleSheet(
        "QLineEdit{background:#0a0a0a;color:#e5e5e5;border:1px solid #1a1a1a;"
        "padding:3px 6px;font-size:13px;font-family:'Consolas','Courier New',monospace;}"
        "QLineEdit:focus{border-color:#333333;}"
        "QLineEdit{selection-background-color:#d97706;selection-color:#080808;}");
    connect(search_input_, &QLineEdit::textChanged, this, &NewsFilterBar::search_changed);
    root->addWidget(search_input_);

    root->addStretch();

    // ── Status indicators ───────────────────────────────────────────────────
    count_label_ = new QLabel("0");
    count_label_->setStyleSheet(QString("color:%1;font-size:12px;font-family:'Consolas','Courier New',monospace;"
        "background:transparent;").arg(ui::colors::TEXT_SECONDARY));
    root->addWidget(count_label_);

    alert_label_ = new QLabel;
    alert_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;"
        "font-family:'Consolas','Courier New',monospace;background:transparent;").arg(ui::colors::NEGATIVE));
    alert_label_->hide();
    root->addWidget(alert_label_);

    live_dot_ = new QLabel("\xe2\x97\x8f LIVE");
    live_dot_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:0.5px;"
        "font-family:'Consolas','Courier New',monospace;background:transparent;").arg(ui::colors::POSITIVE));
    root->addWidget(live_dot_);

    // ── Refresh button ──────────────────────────────────────────────────────
    refresh_btn_ = new QPushButton("GO");
    refresh_btn_->setFixedSize(40, 22);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(
        "QPushButton{background:rgba(217,119,6,0.1);color:#d97706;border:1px solid #78350f;"
        "font-size:12px;font-weight:700;font-family:'Consolas','Courier New',monospace;}"
        "QPushButton:hover{background:#d97706;color:#080808;}");
    connect(refresh_btn_, &QPushButton::clicked, this, &NewsFilterBar::refresh_clicked);
    root->addWidget(refresh_btn_);

    // Initial styles
    update_btn_styles(category_btns_, active_category_);
    update_btn_styles(time_btns_, active_time_);
    update_btn_styles(view_btns_, active_view_);
}

QPushButton* NewsFilterBar::make_filter_btn(const QString& label, const QString& id) {
    auto* btn = new QPushButton(label);
    btn->setFixedHeight(22);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("btn_id", id);
    connect(btn, &QPushButton::clicked, this, [this, id]() {
        active_category_ = id;
        update_btn_styles(category_btns_, active_category_);
        emit category_changed(id);
    });
    return btn;
}

QPushButton* NewsFilterBar::make_toggle_btn(const QString& label, const QString& id) {
    auto* btn = new QPushButton(label);
    btn->setFixedHeight(22);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("btn_id", id);
    return btn;
}

void NewsFilterBar::set_active_category(const QString& cat) {
    active_category_ = cat;
    update_btn_styles(category_btns_, active_category_);
}

void NewsFilterBar::set_loading(bool loading) {
    refresh_btn_->setText(loading ? "..." : "GO");
    refresh_btn_->setEnabled(!loading);
}

void NewsFilterBar::set_article_count(int count) {
    count_label_->setText(QString("%1 articles").arg(count));
}

void NewsFilterBar::set_alert_count(int count) {
    if (count > 0) {
        alert_label_->setText(QString("%1 ALERT%2").arg(count).arg(count > 1 ? "S" : ""));
        alert_label_->show();
    } else {
        alert_label_->hide();
    }
}

void NewsFilterBar::update_btn_styles(QVector<QPushButton*>& btns, const QString& active_id) {
    for (auto* btn : btns) {
        btn->setStyleSheet(btn->property("btn_id").toString() == active_id ? BTN_ACTIVE : BTN_INACTIVE);
    }
}

} // namespace fincept::screens
