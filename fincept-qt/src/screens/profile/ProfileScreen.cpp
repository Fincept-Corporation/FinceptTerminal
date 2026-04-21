#include "screens/profile/ProfileScreen.h"

#include "auth/AuthManager.h"
#include "auth/UserApi.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>

namespace fincept::screens {

static const char* MF = "font-family:'Consolas',monospace;";
static QString PANEL_SS() {
    return QString("background:%1;border:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM());
}
static QString HDR_SS() {
    return QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM());
}

QWidget* ProfileScreen::make_panel(const QString& title) {
    auto* w = new QWidget(this);
    w->setStyleSheet(PANEL_SS());
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(HDR_SS());
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* t = new QLabel(title);
    t->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;letter-spacing:0.5px;%2")
                         .arg(ui::colors::AMBER(), MF));
    hl->addWidget(t);
    hl->addStretch();
    vl->addWidget(hdr);
    return w;
}

QWidget* ProfileScreen::make_data_row(const QString& label, QLabel*& value_out) {
    auto* row = new QWidget(this);
    row->setStyleSheet(QString("background:transparent;border-bottom:1px solid %1;").arg(ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 6, 12, 6);
    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;background:transparent;letter-spacing:0.5px;%2")
                           .arg(ui::colors::TEXT_SECONDARY(), MF));
    hl->addWidget(lbl);
    hl->addStretch();
    value_out = new QLabel("\xe2\x80\x94");
    value_out->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2").arg(ui::colors::TEXT_PRIMARY(), MF));
    hl->addWidget(value_out);
    return row;
}

QWidget* ProfileScreen::make_stat_box(const QString& label, QLabel*& value_out, const QString& color) {
    auto* w = new QWidget(this);
    w->setStyleSheet(QString("background:%1;border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 14, 12, 14);
    vl->setAlignment(Qt::AlignCenter);
    value_out = new QLabel("0");
    value_out->setAlignment(Qt::AlignCenter);
    value_out->setStyleSheet(
        QString("color:%1;font-size:28px;font-weight:700;background:transparent;%2").arg(color, MF));
    vl->addWidget(value_out);
    auto* l = new QLabel(label);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;background:transparent;letter-spacing:0.5px;%2")
                         .arg(ui::colors::TEXT_SECONDARY(), MF));
    vl->addWidget(l);
    return w;
}

ProfileScreen::ProfileScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    build_header(root);
    sections_ = new QStackedWidget;
    sections_->addWidget(build_overview());
    sections_->addWidget(build_usage());
    sections_->addWidget(build_security());
    sections_->addWidget(build_billing());
    sections_->addWidget(build_support());
    build_tab_nav(root);
    root->addWidget(sections_, 1);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this, &ProfileScreen::refresh_all);
    refresh_all();
    // Style the first tab as active on load
    on_section_changed(0);
}

void ProfileScreen::build_header(QVBoxLayout* root) {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(38);
    bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);
    auto* title = new QLabel("PROFILE & ACCOUNT");
    title->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;background:transparent;%2").arg(ui::colors::AMBER(), MF));
    hl->addWidget(title);
    username_header_ = new QLabel;
    username_header_->setStyleSheet(
        QString("color:%1;font-size:13px;background:transparent;%2").arg(ui::colors::TEXT_PRIMARY(), MF));
    hl->addWidget(username_header_);
    hl->addStretch();
    credits_badge_ = new QLabel;
    credits_badge_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;background:transparent;%2").arg(ui::colors::CYAN(), MF));
    hl->addWidget(credits_badge_);
    plan_badge_ = new QLabel;
    plan_badge_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;background:transparent;%2").arg(ui::colors::AMBER(), MF));
    hl->addWidget(plan_badge_);
    auto* rb = new QPushButton("REFRESH");
    rb->setFixedHeight(22);
    rb->setCursor(Qt::PointingHandCursor);
    rb->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 10px;"
                "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY()));
    connect(rb, &QPushButton::clicked, this, []() { auth::AuthManager::instance().refresh_user_data(); });
    hl->addWidget(rb);
    root->addWidget(bar);
}

void ProfileScreen::build_tab_nav(QVBoxLayout* root) {
    auto* nav = new QWidget(this);
    nav->setFixedHeight(32);
    nav->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(nav);
    hl->setContentsMargins(4, 0, 4, 0);
    hl->setSpacing(0);
    QStringList tabs = {"OVERVIEW", "USAGE", "SECURITY", "BILLING", "SUPPORT"};
    for (int i = 0; i < tabs.size(); i++) {
        auto* btn = new QPushButton(tabs[i]);
        btn->setFixedHeight(32);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_section_changed(i); });
        hl->addWidget(btn);
        nav_buttons_.append(btn);
    }
    hl->addStretch();
    root->addWidget(nav);
}

void ProfileScreen::on_section_changed(int index) {
    sections_->setCurrentIndex(index);
    for (int i = 0; i < nav_buttons_.size(); ++i) {
        nav_buttons_[i]->setStyleSheet(
            i == index
                ? QString("QPushButton{background:%1;color:%2;border:none;padding:0 14px;"
                          "font-size:12px;font-weight:700;letter-spacing:0.5px;font-family:'Consolas',monospace;}")
                      .arg(ui::colors::AMBER(), ui::colors::BG_BASE())
                : QString("QPushButton{background:transparent;color:%1;border:none;padding:0 14px;"
                          "font-size:12px;font-weight:700;letter-spacing:0.5px;font-family:'Consolas',monospace;}"
                          "QPushButton:hover{color:%2;}")
                      .arg(ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_SECONDARY()));
    }
    if (index == 1)
        fetch_usage_data();
    else if (index == 2)
        fetch_login_history();
    else if (index == 3)
        fetch_billing_data();
}

QWidget* ProfileScreen::build_overview() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);
    auto* grid = new QGridLayout;
    grid->setSpacing(10);

    auto* acct = make_panel("ACCOUNT INFORMATION");
    auto* avl = qobject_cast<QVBoxLayout*>(acct->layout());
    avl->addWidget(make_data_row("USERNAME", ov_username_));
    avl->addWidget(make_data_row("EMAIL", ov_email_));
    avl->addWidget(make_data_row("USER TYPE", ov_user_type_));
    avl->addWidget(make_data_row("ACCOUNT TYPE", ov_account_type_));
    avl->addWidget(make_data_row("PHONE", ov_phone_));
    avl->addWidget(make_data_row("COUNTRY", ov_country_));
    avl->addWidget(make_data_row("EMAIL VERIFIED", ov_verified_));
    avl->addWidget(make_data_row("2FA ENABLED", ov_mfa_));
    auto* eb = new QPushButton("EDIT PROFILE");
    eb->setFixedHeight(26);
    eb->setCursor(Qt::PointingHandCursor);
    eb->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;margin:8px 12px;"
                "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY()));
    connect(eb, &QPushButton::clicked, this, &ProfileScreen::show_edit_profile_dialog);
    avl->addWidget(eb);
    grid->addWidget(acct, 0, 0);

    auto* cred = make_panel("CREDITS & BALANCE");
    auto* cvl2 = qobject_cast<QVBoxLayout*>(cred->layout());
    ov_credits_big_ = new QLabel("0");
    ov_credits_big_->setAlignment(Qt::AlignCenter);
    ov_credits_big_->setStyleSheet(
        QString("color:%1;font-size:42px;font-weight:700;background:transparent;padding:20px 0 4px 0;%2")
            .arg(ui::colors::CYAN(), MF));
    cvl2->addWidget(ov_credits_big_);
    auto* cl = new QLabel("AVAILABLE CREDITS");
    cl->setAlignment(Qt::AlignCenter);
    cl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;background:transparent;letter-spacing:0."
                              "5px;padding-bottom:12px;%2")
                          .arg(ui::colors::TEXT_SECONDARY(), MF));
    cvl2->addWidget(cl);
    auto* sp = new QFrame;
    sp->setFixedHeight(1);
    sp->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    cvl2->addWidget(sp);
    cvl2->addWidget(make_data_row("PLAN", ov_plan_));
    grid->addWidget(cred, 0, 1);

    auto* actions = make_panel("QUICK ACTIONS");
    auto* act_vl = qobject_cast<QVBoxLayout*>(actions->layout());
    auto* ar = new QWidget(this);
    ar->setStyleSheet("background:transparent;");
    auto* arl = new QHBoxLayout(ar);
    arl->setContentsMargins(12, 8, 12, 8);
    arl->setSpacing(10);
    auto* eb2 = new QPushButton("EDIT PROFILE");
    eb2->setFixedHeight(26);
    eb2->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;"
                "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY()));
    connect(eb2, &QPushButton::clicked, this, &ProfileScreen::show_edit_profile_dialog);
    arl->addWidget(eb2);
    auto* lb = new QPushButton("LOGOUT");
    lb->setFixedHeight(26);
    lb->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid #7f1d1d;padding:0 12px;"
                "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{background:%1;"
                "color:%2;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
    connect(lb, &QPushButton::clicked, this, &ProfileScreen::show_logout_confirm);
    arl->addWidget(lb);
    auto* dab = new QPushButton("DELETE ACCOUNT");
    dab->setFixedHeight(26);
    dab->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.15);color:%1;border:1px solid #7f1d1d;padding:0 12px;"
                "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{background:%1;"
                "color:%2;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
    connect(dab, &QPushButton::clicked, this, &ProfileScreen::show_delete_account_dialog);
    arl->addWidget(dab);
    arl->addStretch();
    act_vl->addWidget(ar);
    grid->addWidget(actions, 1, 0, 1, 2);
    vl->addLayout(grid);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* ProfileScreen::build_usage() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);
    auto* sr = new QWidget(this);
    sr->setStyleSheet("background:transparent;");
    auto* srl = new QHBoxLayout(sr);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(8);
    srl->addWidget(make_stat_box("CREDIT BALANCE", usg_credits_, ui::colors::CYAN));
    srl->addWidget(make_stat_box("ACCOUNT TYPE", usg_plan_, ui::colors::AMBER));
    srl->addWidget(make_stat_box("RATE LIMIT/HR", usg_rate_, ui::colors::TEXT_PRIMARY));
    vl->addWidget(sr);
    auto* sp = make_panel("USAGE SUMMARY \xe2\x80\x94 LAST 30 DAYS");
    auto* svl = qobject_cast<QVBoxLayout*>(sp->layout());
    auto* smr = new QWidget(this);
    smr->setStyleSheet("background:transparent;");
    auto* smrl = new QHBoxLayout(smr);
    smrl->setContentsMargins(8, 8, 8, 8);
    smrl->setSpacing(8);
    smrl->addWidget(make_stat_box("TOTAL REQUESTS", usg_total_req_, ui::colors::CYAN));
    smrl->addWidget(make_stat_box("CREDITS USED", usg_cred_used_, ui::colors::WARNING));
    smrl->addWidget(make_stat_box("AVG CR/REQ", usg_avg_cred_, ui::colors::TEXT_PRIMARY));
    smrl->addWidget(make_stat_box("AVG RESP (ms)", usg_avg_resp_, ui::colors::TEXT_PRIMARY));
    svl->addWidget(smr);
    vl->addWidget(sp);
    auto* dp = make_panel("DAILY USAGE");
    auto* dvl = qobject_cast<QVBoxLayout*>(dp->layout());
    usg_daily_table_ = new QTableWidget;
    usg_daily_table_->setColumnCount(3);
    usg_daily_table_->setHorizontalHeaderLabels({"DATE", "REQUESTS", "CREDITS"});
    usg_daily_table_->verticalHeader()->setVisible(false);
    usg_daily_table_->setShowGrid(false);
    usg_daily_table_->horizontalHeader()->setStretchLastSection(true);
    usg_daily_table_->setSelectionMode(QAbstractItemView::NoSelection);
    usg_daily_table_->setMinimumHeight(200);
    dvl->addWidget(usg_daily_table_);
    vl->addWidget(dp);
    auto* ep = make_panel("TOP ENDPOINTS");
    auto* evl = qobject_cast<QVBoxLayout*>(ep->layout());
    usg_endpoint_table_ = new QTableWidget;
    usg_endpoint_table_->setColumnCount(4);
    usg_endpoint_table_->setHorizontalHeaderLabels({"ENDPOINT", "REQUESTS", "CREDITS", "AVG MS"});
    usg_endpoint_table_->verticalHeader()->setVisible(false);
    usg_endpoint_table_->setShowGrid(false);
    usg_endpoint_table_->horizontalHeader()->setStretchLastSection(true);
    usg_endpoint_table_->setSelectionMode(QAbstractItemView::NoSelection);
    usg_endpoint_table_->setMinimumHeight(200);
    evl->addWidget(usg_endpoint_table_);
    vl->addWidget(ep);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* ProfileScreen::build_security() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);
    auto* kp = make_panel("API KEY");
    auto* kvl = qobject_cast<QVBoxLayout*>(kp->layout());
    auto* kr = new QWidget(this);
    kr->setStyleSheet("background:transparent;");
    auto* krl = new QHBoxLayout(kr);
    krl->setContentsMargins(12, 10, 12, 10);
    krl->setSpacing(8);
    sec_api_key_ = new QLabel(QString(20, QChar(0x2022)));
    sec_api_key_->setStyleSheet(
        QString("color:%1;font-size:13px;background:transparent;%2").arg(ui::colors::TEXT_PRIMARY(), MF));
    krl->addWidget(sec_api_key_, 1);
    auto* sb = new QPushButton("SHOW");
    sb->setFixedHeight(22);
    sb->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 10px;"
                "font-size:10px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY()));
    connect(sb, &QPushButton::clicked, this, [this, sb]() {
        api_key_visible_ = !api_key_visible_;
        sb->setText(api_key_visible_ ? "HIDE" : "SHOW");
        sec_api_key_->setText(api_key_visible_ ? auth::AuthManager::instance().session().api_key
                                               : QString(20, QChar(0x2022)));
    });
    krl->addWidget(sb);
    auto* cb = new QPushButton("COPY");
    cb->setFixedHeight(22);
    cb->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 10px;"
                "font-size:10px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY()));
    connect(cb, &QPushButton::clicked, this, [cb]() {
        auto key = auth::AuthManager::instance().session().api_key;
        if (!key.isEmpty()) {
            QApplication::clipboard()->setText(key);
            cb->setText("COPIED");
            QTimer::singleShot(1500, cb, [cb]() { cb->setText("COPY"); });
        }
    });
    krl->addWidget(cb);
    auto* rg = new QPushButton("REGENERATE");
    rg->setFixedHeight(22);
    rg->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;padding:0 10px;"
                              "font-size:10px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{"
                              "background:%1;color:%3;}")
                          .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
    connect(rg, &QPushButton::clicked, this, &ProfileScreen::show_regen_confirm);
    krl->addWidget(rg);
    kvl->addWidget(kr);
    vl->addWidget(kp);
    auto* ssp = make_panel("SECURITY STATUS");
    auto* ssvl = qobject_cast<QVBoxLayout*>(ssp->layout());
    ssvl->addWidget(make_data_row("EMAIL VERIFIED", sec_verified_));
    ssvl->addWidget(make_data_row("2FA (MFA)", sec_mfa_));
    vl->addWidget(ssp);
    auto* hp = make_panel("LOGIN HISTORY");
    auto* hvl = qobject_cast<QVBoxLayout*>(hp->layout());
    sec_login_hist_ = new QTableWidget;
    sec_login_hist_->setColumnCount(3);
    sec_login_hist_->setHorizontalHeaderLabels({"TIMESTAMP", "IP ADDRESS", "STATUS"});
    sec_login_hist_->verticalHeader()->setVisible(false);
    sec_login_hist_->setShowGrid(false);
    sec_login_hist_->horizontalHeader()->setStretchLastSection(true);
    sec_login_hist_->setSelectionMode(QAbstractItemView::NoSelection);
    sec_login_hist_->setMinimumHeight(200);
    hvl->addWidget(sec_login_hist_);
    vl->addWidget(hp);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* ProfileScreen::build_billing() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);
    auto* sp = make_panel("SUBSCRIPTION");
    auto* svl = qobject_cast<QVBoxLayout*>(sp->layout());
    svl->addWidget(make_data_row("PLAN", bill_plan_));
    svl->addWidget(make_data_row("CREDIT BALANCE", bill_credits_));
    svl->addWidget(make_data_row("SUPPORT TYPE", bill_support_));
    vl->addWidget(sp);
    auto* hp = make_panel("PAYMENT HISTORY");
    auto* hvl = qobject_cast<QVBoxLayout*>(hp->layout());
    bill_history_ = new QTableWidget;
    bill_history_->setColumnCount(5);
    bill_history_->setHorizontalHeaderLabels({"DATE", "PLAN", "AMOUNT", "CREDITS", "STATUS"});
    bill_history_->verticalHeader()->setVisible(false);
    bill_history_->setShowGrid(false);
    bill_history_->horizontalHeader()->setStretchLastSection(true);
    bill_history_->setSelectionMode(QAbstractItemView::NoSelection);
    bill_history_->setMinimumHeight(250);
    hvl->addWidget(bill_history_);
    vl->addWidget(hp);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* ProfileScreen::build_support() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);
    auto* cp = make_panel("CONTACT US");
    auto* cvl2 = qobject_cast<QVBoxLayout*>(cp->layout());
    auto* cg = new QGridLayout;
    cg->setSpacing(12);
    cg->setContentsMargins(12, 10, 12, 10);
    auto add_c = [&](const QString& l, const QString& e, int r, int c2) {
        auto* w = new QWidget(this);
        w->setStyleSheet("background:transparent;");
        auto* wl = new QVBoxLayout(w);
        wl->setContentsMargins(0, 0, 0, 0);
        wl->setSpacing(2);
        auto* lb = new QLabel(l);
        lb->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;background:transparent;letter-spacing:0.5px;%2")
                .arg(ui::colors::TEXT_TERTIARY(), MF));
        wl->addWidget(lb);
        auto* em = new QLabel(e);
        em->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;%2").arg(ui::colors::CYAN(), MF));
        wl->addWidget(em);
        cg->addWidget(w, r, c2);
    };
    add_c("GENERAL SUPPORT", "support@fincept.in", 0, 0);
    add_c("COMMERCIAL", "support@fincept.in", 0, 1);
    add_c("SECURITY", "support@fincept.in", 1, 0);
    add_c("LEGAL", "support@fincept.in", 1, 1);
    cvl2->addLayout(cg);
    vl->addWidget(cp);
    auto* lp = make_panel("RESOURCES");
    auto* lvl = qobject_cast<QVBoxLayout*>(lp->layout());
    auto* lr = new QWidget(this);
    lr->setStyleSheet("background:transparent;");
    auto* lrl = new QHBoxLayout(lr);
    lrl->setContentsMargins(12, 10, 12, 10);
    lrl->setSpacing(8);
    auto make_link_btn = [&](const QString& label, const QString& url) {
        auto* b = new QPushButton(label);
        b->setFixedHeight(26);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;"
                    "font-size:11px;font-family:'Consolas',monospace;}QPushButton:hover{color:%4;background:%5;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::CYAN(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                     ui::colors::BG_HOVER()));
        connect(b, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        lrl->addWidget(b);
    };
    make_link_btn("DOCS", "https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs");
    make_link_btn("GITHUB", "https://github.com/Fincept-Corporation/FinceptTerminal");
    make_link_btn("DISCORD", "https://discord.gg/ae87a8ygbN");
    make_link_btn("FAQ", "https://github.com/Fincept-Corporation/FinceptTerminal/wiki");
    lrl->addStretch();
    lvl->addWidget(lr);
    vl->addWidget(lp);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void ProfileScreen::refresh_all() {
    const auto& s = auth::AuthManager::instance().session();
    if (!s.authenticated)
        return;
    username_header_->setText(s.user_info.username.isEmpty() ? s.user_info.email : s.user_info.username);
    credits_badge_->setText(QString("CR %1").arg(s.user_info.credit_balance, 0, 'f', 2));
    plan_badge_->setText(s.account_type().toUpper());
    ov_username_->setText(s.user_info.username.isEmpty() ? "N/A" : s.user_info.username);
    ov_email_->setText(s.user_info.email.isEmpty() ? "N/A" : s.user_info.email);
    ov_user_type_->setText("REGISTERED");
    ov_account_type_->setText(s.account_type().toUpper());
    ov_account_type_->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2").arg(ui::colors::AMBER(), MF));
    ov_phone_->setText(s.user_info.phone.isEmpty() ? "\xe2\x80\x94" : s.user_info.phone);
    ov_country_->setText(s.user_info.country.isEmpty() ? "\xe2\x80\x94" : s.user_info.country);
    ov_verified_->setText(s.user_info.is_verified ? "YES" : "NO");
    ov_verified_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2")
                                    .arg(s.user_info.is_verified ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())
                                    .arg(MF));
    ov_mfa_->setText(s.user_info.mfa_enabled ? "YES" : "NO");
    ov_mfa_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2")
                               .arg(s.user_info.mfa_enabled ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())
                               .arg(MF));
    ov_credits_big_->setText(QString::number(static_cast<int>(s.user_info.credit_balance)));
    ov_plan_->setText(s.account_type().toUpper());
    ov_plan_->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2").arg(ui::colors::AMBER(), MF));
    sec_verified_->setText(s.user_info.is_verified ? "YES" : "NO");
    sec_verified_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2")
                                     .arg(s.user_info.is_verified ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())
                                     .arg(MF));
    sec_mfa_->setText(s.user_info.mfa_enabled ? "ENABLED" : "DISABLED");
    sec_mfa_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;background:transparent;%2")
                                .arg(s.user_info.mfa_enabled ? ui::colors::POSITIVE() : ui::colors::TEXT_SECONDARY())
                                .arg(MF));
    bill_plan_->setText(s.account_type().toUpper());
    bill_credits_->setText(QString::number(s.user_info.credit_balance, 'f', 2));
    // bill_support_ is populated by fetch_billing_data() from the API; leave it as-is here
}

void ProfileScreen::fetch_usage_data() {
    // Populate stat boxes with session data as fallback
    const auto& s = auth::AuthManager::instance().session();
    usg_credits_->setText(QString::number(s.user_info.credit_balance, 'f', 0));
    usg_plan_->setText(s.account_type().toUpper());
    usg_rate_->setText("—");

    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().get_user_usage(30, [self](auth::ApiResponse r) {
        if (!self)
            return;
        if (!r.success) {
            LOG_WARN("Profile", "Usage fetch failed: " + r.error);
            return;
        }
        auto data = r.data.contains("data") ? r.data["data"].toObject() : r.data;
        if (data.contains("account")) {
            auto a = data["account"].toObject();
            self->usg_credits_->setText(QString::number(a["credit_balance"].toDouble(), 'f', 0));
            self->usg_plan_->setText(a["account_type"].toString().toUpper());
            self->usg_rate_->setText(QString::number(a["rate_limit_per_hour"].toInt()));
        }
        if (data.contains("summary")) {
            auto s = data["summary"].toObject();
            self->usg_total_req_->setText(QString::number(s["total_requests"].toInt()));
            self->usg_cred_used_->setText(QString::number(s["total_credits_used"].toDouble(), 'f', 0));
            self->usg_avg_cred_->setText(QString::number(s["avg_credits_per_request"].toDouble(), 'f', 2));
            self->usg_avg_resp_->setText(QString::number(s["avg_response_time_ms"].toDouble(), 'f', 0));
        }
        if (data.contains("daily_usage")) {
            auto d = data["daily_usage"].toArray();
            self->usg_daily_table_->setRowCount(0);
            for (int i = d.size() - 1; i >= 0 && i >= d.size() - 10; i--) {
                auto e = d[i].toObject();
                int row = self->usg_daily_table_->rowCount();
                self->usg_daily_table_->insertRow(row);
                self->usg_daily_table_->setItem(row, 0, new QTableWidgetItem(e["date"].toString()));
                self->usg_daily_table_->setItem(row, 1,
                                                new QTableWidgetItem(QString::number(e["request_count"].toInt())));
                self->usg_daily_table_->setItem(
                    row, 2, new QTableWidgetItem(QString::number(e["credits_used"].toDouble(), 'f', 0)));
            }
        }
        if (data.contains("endpoint_breakdown")) {
            auto eps = data["endpoint_breakdown"].toArray();
            self->usg_endpoint_table_->setRowCount(0);
            for (const auto& v : eps) {
                auto e = v.toObject();
                int row = self->usg_endpoint_table_->rowCount();
                self->usg_endpoint_table_->insertRow(row);
                self->usg_endpoint_table_->setItem(row, 0, new QTableWidgetItem(e["endpoint"].toString()));
                self->usg_endpoint_table_->setItem(row, 1,
                                                   new QTableWidgetItem(QString::number(e["request_count"].toInt())));
                self->usg_endpoint_table_->setItem(
                    row, 2, new QTableWidgetItem(QString::number(e["credits_used"].toDouble(), 'f', 0)));
                self->usg_endpoint_table_->setItem(
                    row, 3, new QTableWidgetItem(QString::number(e["avg_response_time_ms"].toDouble(), 'f', 0)));
            }
        }
    });
}

void ProfileScreen::fetch_billing_data() {
    LOG_INFO("Profile", "Fetching billing data...");
    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().get_user_subscription([self](auth::ApiResponse r) {
        if (!self)
            return;
        if (!r.success) {
            LOG_WARN("Profile", "Subscription fetch failed: " + r.error);
            return;
        }
        auto d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
        self->bill_plan_->setText(d["account_type"].toString().toUpper());
        self->bill_credits_->setText(QString::number(d["credit_balance"].toDouble(), 'f', 2));
        self->bill_support_->setText(d["support_type"].toString().toUpper());
    });
    auth::UserApi::instance().get_payment_history(1, 20, [self](auth::ApiResponse r) {
        if (!self)
            return;
        if (!r.success) {
            LOG_WARN("Profile", "Payment history failed: " + r.error);
            return;
        }
        auto d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
        auto p = d["payments"].toArray();
        if (p.isEmpty())
            p = d["transactions"].toArray();
        if (p.isEmpty() && d.contains("data"))
            p = d["data"].toArray();
        self->bill_history_->setRowCount(0);
        for (const auto& v : p) {
            auto e = v.toObject();
            int row = self->bill_history_->rowCount();
            self->bill_history_->insertRow(row);
            self->bill_history_->setItem(row, 0, new QTableWidgetItem(e["created_at"].toString().left(10)));
            self->bill_history_->setItem(row, 1, new QTableWidgetItem(e["plan_name"].toString()));
            self->bill_history_->setItem(
                row, 2, new QTableWidgetItem(QString("$%1").arg(e["amount_usd"].toDouble(), 0, 'f', 2)));
            self->bill_history_->setItem(row, 3, new QTableWidgetItem(QString::number(e["credits_purchased"].toInt())));
            self->bill_history_->setItem(row, 4, new QTableWidgetItem(e["status"].toString().toUpper()));
        }
    });
}

void ProfileScreen::fetch_login_history() {
    LOG_INFO("Profile", "Fetching login history...");
    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().get_login_history(20, 0, [self](auth::ApiResponse r) {
        if (!self)
            return;
        if (!r.success) {
            LOG_WARN("Profile", "Login history failed: " + r.error);
            return;
        }
        auto d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
        auto h = d["login_history"].toArray();
        if (h.isEmpty())
            h = d["history"].toArray();
        self->sec_login_hist_->setRowCount(0);
        for (const auto& v : h) {
            auto e = v.toObject();
            int row = self->sec_login_hist_->rowCount();
            self->sec_login_hist_->insertRow(row);
            self->sec_login_hist_->setItem(row, 0, new QTableWidgetItem(e["timestamp"].toString().left(19)));
            self->sec_login_hist_->setItem(row, 1, new QTableWidgetItem(e["ip_address"].toString()));
            self->sec_login_hist_->setItem(row, 2, new QTableWidgetItem(e["status"].toString().toUpper()));
        }
    });
}

void ProfileScreen::show_edit_profile_dialog() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Edit Profile");
    dlg->setFixedSize(420, 300);
    dlg->setStyleSheet(QString("background:%1;color:%2;font-family:'Consolas',monospace;")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY()));
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);
    auto* t = new QLabel("EDIT PROFILE");
    t->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;background:transparent;").arg(ui::colors::AMBER()));
    vl->addWidget(t);
    const auto& s = auth::AuthManager::instance().session();
    auto add_f = [&](const QString& l, const QString& v) -> QLineEdit* {
        auto* lb = new QLabel(l);
        lb->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        vl->addWidget(lb);
        auto* i = new QLineEdit(v);
        i->setFixedHeight(28);
        vl->addWidget(i);
        return i;
    };
    auto* un = add_f("USERNAME", s.user_info.username);
    auto* ph = add_f("PHONE (with country code)", s.user_info.phone);
    auto* co = add_f("COUNTRY", s.user_info.country);
    auto* br = new QWidget(this);
    br->setStyleSheet("background:transparent;");
    auto* brl = new QHBoxLayout(br);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(8);
    brl->addStretch();
    auto* cn = new QPushButton("CANCEL");
    cn->setFixedHeight(26);
    connect(cn, &QPushButton::clicked, dlg, &QDialog::reject);
    brl->addWidget(cn);
    auto* sv = new QPushButton("SAVE");
    sv->setFixedHeight(26);
    sv->setStyleSheet(
        QString(
            "QPushButton{background:%1;color:%2;border:none;padding:0 16px;"
            "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}QPushButton:hover{background:#b45309;}")
            .arg(ui::colors::AMBER(), ui::colors::BG_BASE()));
    QPointer<ProfileScreen> self = this;
    QPointer<QDialog> dlg_ptr = dlg;
    connect(sv, &QPushButton::clicked, this, [=]() {
        QJsonObject data;
        if (!un->text().trimmed().isEmpty())
            data["username"] = un->text().trimmed();
        if (!ph->text().trimmed().isEmpty())
            data["phone"] = ph->text().trimmed();
        if (!co->text().trimmed().isEmpty())
            data["country"] = co->text().trimmed();
        auth::UserApi::instance().update_user_profile(data, [self, dlg_ptr](auth::ApiResponse r) {
            if (!self)
                return;
            if (r.success) {
                auth::AuthManager::instance().refresh_user_data();
                if (dlg_ptr)
                    dlg_ptr->accept();
            }
        });
    });
    brl->addWidget(sv);
    vl->addWidget(br);
    dlg->exec();
    dlg->deleteLater();
}

void ProfileScreen::show_logout_confirm() {
    if (QMessageBox::question(this, "Confirm Logout", "Are you sure you want to logout?",
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        auth::AuthManager::instance().logout();
}

void ProfileScreen::show_regen_confirm() {
    if (QMessageBox::warning(this, "Regenerate API Key", "Your current API key will be invalidated. Continue?",
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        auth::UserApi::instance().regenerate_api_key([this](auth::ApiResponse r) {
            if (r.success) {
                auth::AuthManager::instance().refresh_user_data();
                api_key_visible_ = false;
                sec_api_key_->setText(QString(20, QChar(0x2022)));
            }
        });
    }
}

void ProfileScreen::show_delete_account_dialog() {
    const auto& s = auth::AuthManager::instance().session();
    const QString email = s.user_info.email;

    // First confirmation
    auto first = QMessageBox::warning(
        this, "Delete Account",
        QString("This will permanently delete your Fincept account (%1) and all associated data.\n\n"
                "This action CANNOT be undone. Are you sure?").arg(email),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (first != QMessageBox::Yes)
        return;

    // Second confirmation — type email to confirm
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Confirm Account Deletion");
    dlg->setFixedSize(400, 200);
    dlg->setStyleSheet(QString("background:%1;color:%2;font-family:'Consolas',monospace;")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY()));
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);

    auto* warn = new QLabel("TYPE YOUR EMAIL ADDRESS TO CONFIRM:");
    warn->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;").arg(ui::colors::NEGATIVE()));
    vl->addWidget(warn);

    auto* hint = new QLabel(email);
    hint->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(hint);

    auto* input = new QLineEdit;
    input->setFixedHeight(28);
    input->setPlaceholderText(email);
    vl->addWidget(input);

    auto* brl = new QHBoxLayout;
    brl->addStretch();
    auto* cancel = new QPushButton("CANCEL");
    cancel->setFixedHeight(26);
    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);
    brl->addWidget(cancel);

    auto* confirm = new QPushButton("DELETE MY ACCOUNT");
    confirm->setFixedHeight(26);
    confirm->setEnabled(false);
    confirm->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:0 14px;"
                "font-size:11px;font-weight:700;font-family:'Consolas',monospace;}"
                "QPushButton:disabled{background:#3f1515;color:#7f3333;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
    connect(input, &QLineEdit::textChanged, this, [confirm, email](const QString& text) {
        confirm->setEnabled(text.trimmed() == email);
    });

    QPointer<ProfileScreen> self = this;
    QPointer<QDialog> dlg_ptr = dlg;
    connect(confirm, &QPushButton::clicked, this, [self, dlg_ptr]() {
        if (!self) return;
        if (dlg_ptr) dlg_ptr->accept();
        auth::UserApi::instance().delete_user_account([self](auth::ApiResponse r) {
            if (!self) return;
            if (r.success) {
                LOG_INFO("Profile", "Account deleted successfully");
                auth::AuthManager::instance().logout();
            } else {
                LOG_ERROR("Profile", "Account deletion failed: " + r.error);
                QMessageBox::critical(self, "Delete Failed",
                                      "Account deletion failed: " + r.error + "\n\nPlease contact support@fincept.in");
            }
        });
    });
    brl->addWidget(confirm);
    vl->addLayout(brl);

    dlg->exec();
    dlg->deleteLater();
}

} // namespace fincept::screens
