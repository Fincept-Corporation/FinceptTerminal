#include "screens/profile/ProfileScreen.h"

#include "auth/AuthManager.h"
#include "auth/UserApi.h"
#include "core/currency/Currency.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EnterprisePromo.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>

namespace fincept::screens {

// Re-style a widget whose colour follows a dynamic property (the rules live in
// ProfileScreen::apply_theme()). No-op when the value is unchanged.
static void profile_set_prop(QWidget* w, const char* name, const QString& value) {
    if (!w || w->property(name).toString() == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

static void profile_set_tone(QWidget* w, const char* tone) {
    profile_set_prop(w, "tone", QString::fromLatin1(tone));
}

// The API serialises its timestamps as naive UTC ("2026-05-01T12:34:56", no
// offset); QDateTime would read those as LOCAL time. Treat an offset-less value
// as UTC and show it in the user's zone.
static QString profile_local_stamp(const QString& iso, bool with_time) {
    if (iso.isEmpty())
        return QString::fromUtf8("\xe2\x80\x94");
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) {
        QString v = iso.left(with_time ? 16 : 10);
        v.replace('T', ' ');
        return v;
    }
    const QString tail = iso.mid(10);
    const bool has_offset = tail.contains('Z') || tail.contains('+') || tail.contains('-');
    if (!has_offset)
        dt = QDateTime(dt.date(), dt.time(), QTimeZone::utc());
    return dt.toLocalTime().toString(with_time ? QStringLiteral("yyyy-MM-dd HH:mm") : QStringLiteral("yyyy-MM-dd"));
}

// Plan label from an API payload: account_type in caps (FREE when absent), with
// the plan actually in force when a paid pass has lapsed ("PRO (NOW FREE)").
static QString profile_plan_text(const QJsonObject& o) {
    const QString at = o.value("account_type").toString().trimmed();
    const QString label = at.isEmpty() ? QStringLiteral("FREE") : at.toUpper();
    const QString eff = o.value("effective_plan").toString().trimmed();
    if (!eff.isEmpty() && eff.compare(at, Qt::CaseInsensitive) != 0)
        return ProfileScreen::tr("%1 (NOW %2)").arg(label, eff.toUpper());
    return label;
}

// Replace a table's contents with one full-width message row. Used for "loading",
// "failed" and "nothing to show": the Usage / Security / Billing tables used to sit
// completely blank in all three cases, so a failed fetch looked like an empty account.
static void profile_table_message(QTableWidget* table, const QString& text) {
    if (!table)
        return;
    table->clearSpans();
    table->setRowCount(0); // drop old rows' items first so none hide under the span
    table->setRowCount(1);
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignCenter);
    table->setItem(0, 0, item);
    table->setSpan(0, 0, 1, table->columnCount());
}

// Same, but only while the table has nothing yet — a refetch must keep showing the
// previous rows until the new ones arrive (P11).
static void profile_table_loading(QTableWidget* table, const QString& text) {
    if (table && table->rowCount() == 0)
        profile_table_message(table, text);
}

static QTableWidget* profile_make_table(const QStringList& headers, int min_height) {
    auto* t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setShowGrid(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(min_height);
    return t;
}

static QTableWidgetItem* profile_num_item(const QString& text) {
    auto* it = new QTableWidgetItem(text);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

void ProfileScreen::apply_theme() {
    using namespace ui::colors;
    const QString sheet =
        QString("QWidget#profRoot{background:%1;}"
                "QLabel{background:transparent;}"
                "QScrollArea#profScroll{border:none;background:transparent;}"
                "QWidget#profHeaderBar{background:%2;border-bottom:1px solid %3;}"
                "QLabel#profHeaderTitle{color:%4;font-size:14px;font-weight:700;}"
                "QLabel#profHeaderUser{color:%5;font-size:13px;}"
                "QLabel#profCreditsBadge{color:%6;font-size:12px;font-weight:700;}"
                "QLabel#profPlanBadge{color:%4;font-size:12px;font-weight:700;}"
                "QWidget#profTabNav{background:%7;border-bottom:1px solid %3;}"
                "QPushButton#profNavBtn{background:transparent;color:%8;border:none;padding:0 14px;"
                "font-size:12px;font-weight:700;letter-spacing:0.5px;}"
                "QPushButton#profNavBtn:hover{color:%9;}"
                "QPushButton#profNavBtn[active=\"true\"]{background:%4;color:%1;}"
                "QWidget#profPanel{background:%1;border:1px solid %3;}"
                "QWidget#profPanelHdr{background:%7;border:none;border-bottom:1px solid %3;}"
                "QLabel#profPanelTitle{color:%4;font-size:12px;font-weight:700;letter-spacing:0.5px;}"
                "QWidget#profRow{background:transparent;border:none;border-bottom:1px solid %3;}"
                "QLabel#profRowLabel{color:%9;font-size:12px;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#profRowValue{color:%5;font-size:13px;font-weight:700;}")
            .arg(BG_BASE(), BG_SURFACE(), BORDER_DIM(), AMBER(), TEXT_PRIMARY(), CYAN(), BG_RAISED(), TEXT_TERTIARY(),
                 TEXT_SECONDARY()) +
        QString("QLabel#profRowValue[tone=\"pos\"]{color:%1;}"
                "QLabel#profRowValue[tone=\"neg\"]{color:%2;}"
                "QLabel#profRowValue[tone=\"amber\"]{color:%3;}"
                "QLabel#profRowValue[tone=\"dim\"]{color:%4;}"
                "QWidget#profStatBox{background:%5;border:1px solid %6;}"
                "QLabel#profStatValue{color:%7;font-size:28px;font-weight:700;}"
                "QLabel#profStatValue[tone=\"cyan\"]{color:%8;}"
                "QLabel#profStatValue[tone=\"amber\"]{color:%3;}"
                "QLabel#profStatValue[tone=\"warn\"]{color:%9;}"
                "QLabel#profStatLabel{color:%4;font-size:10px;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#profCreditsBig{color:%8;font-size:42px;font-weight:700;padding:20px 0 4px 0;}"
                "QLabel#profCreditsCaption{color:%4;font-size:10px;font-weight:700;letter-spacing:0.5px;"
                "padding-bottom:12px;}"
                "QFrame#profSep{background:%6;border:none;}"
                "QLabel#profApiKey{color:%7;font-size:13px;}")
            .arg(POSITIVE(), NEGATIVE(), AMBER(), TEXT_SECONDARY(), BG_RAISED(), BORDER_DIM(), TEXT_PRIMARY(), CYAN(),
                 WARNING()) +
        QString("QLabel#profContactLabel{color:%1;font-size:10px;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#profContactValue{color:%2;font-size:13px;}"
                "QPushButton#profBtn{background:%3;color:%4;border:1px solid %5;padding:0 12px;font-size:11px;"
                "font-weight:700;}"
                "QPushButton#profBtn:hover{color:%6;}"
                "QPushButton#profBtnDanger{background:%7;color:%8;border:1px solid %9;padding:0 12px;"
                "font-size:11px;font-weight:700;}"
                "QPushButton#profBtnDanger:hover{background:%8;color:%6;}")
            .arg(TEXT_TERTIARY(), CYAN(), BG_RAISED(), TEXT_SECONDARY(), BORDER_DIM(), TEXT_PRIMARY(), NEGATIVE_BG(),
                 NEGATIVE(), NEGATIVE_DIM()) +
        QString("QPushButton#profBtnAmber{background:%1;color:%2;border:1px solid %3;padding:0 10px;font-size:10px;"
                "font-weight:700;}"
                "QPushButton#profBtnAmber:hover{background:%2;color:%4;}"
                "QPushButton#profBtnPrimary{background:%2;color:%4;border:none;padding:0 16px;font-size:11px;"
                "font-weight:700;}"
                "QPushButton#profBtnPrimary:hover{background:%3;}"
                "QPushButton#profBtnPrimary:disabled{background:%5;color:%6;}"
                "QPushButton#profBtnDangerSolid{background:%7;color:%8;border:none;padding:0 14px;font-size:11px;"
                "font-weight:700;}"
                "QPushButton#profBtnDangerSolid:disabled{background:%9;color:%6;}")
            .arg(ACCENT_BG(), AMBER(), AMBER_DIM(), BG_BASE(), BORDER_DIM(), TEXT_DIM(), NEGATIVE(), TEXT_PRIMARY(),
                 NEGATIVE_BG()) +
        QString("QPushButton#profLinkBtn{background:%1;color:%2;border:1px solid %3;padding:0 12px;font-size:11px;}"
                "QPushButton#profLinkBtn:hover{color:%4;background:%5;}"
                "QPushButton#profInlineLink{background:transparent;border:none;color:%6;font-size:11px;"
                "text-align:left;padding:0;}"
                "QPushButton#profInlineLink:hover{color:%7;text-decoration:underline;}"
                "QDialog#profDialog{background:%8;}"
                "QLabel#profDialogTitle{color:%7;font-size:14px;font-weight:700;}"
                "QLabel#profFieldLabel{color:%9;font-size:11px;font-weight:700;}")
            .arg(BG_RAISED(), CYAN(), BORDER_DIM(), TEXT_PRIMARY(), BG_HOVER(), INFO(), AMBER(), BG_SURFACE(),
                 TEXT_SECONDARY()) +
        QString("QLabel#profDangerLabel{color:%1;font-size:11px;font-weight:700;}"
                "QLabel#profMutedLabel{color:%2;font-size:11px;}"
                "QLabel#profMutedLabel[tone=\"pos\"]{color:%3;}"
                "QLabel#profMutedLabel[tone=\"neg\"]{color:%1;}")
            .arg(NEGATIVE(), TEXT_SECONDARY(), POSITIVE());
    setStyleSheet(sheet);
}

QWidget* ProfileScreen::make_panel(const QString& title) {
    auto* w = new QWidget(this);
    w->setObjectName(QStringLiteral("profPanel"));
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    auto* hdr = new QWidget(this);
    hdr->setObjectName(QStringLiteral("profPanelHdr"));
    hdr->setFixedHeight(34);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* t = new QLabel(title);
    t->setObjectName(QStringLiteral("profPanelTitle"));
    hl->addWidget(t);
    hl->addStretch();
    vl->addWidget(hdr);
    return w;
}

QWidget* ProfileScreen::make_data_row(const QString& label, QLabel*& value_out) {
    auto* row = new QWidget(this);
    row->setObjectName(QStringLiteral("profRow"));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 6, 12, 6);
    auto* lbl = new QLabel(label);
    lbl->setObjectName(QStringLiteral("profRowLabel"));
    hl->addWidget(lbl);
    hl->addStretch();
    value_out = new QLabel(QString::fromUtf8("\xe2\x80\x94"));
    value_out->setObjectName(QStringLiteral("profRowValue"));
    hl->addWidget(value_out);
    return row;
}

QWidget* ProfileScreen::make_stat_box(const QString& label, QLabel*& value_out, const char* tone) {
    auto* w = new QWidget(this);
    w->setObjectName(QStringLiteral("profStatBox"));
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 14, 12, 14);
    vl->setAlignment(Qt::AlignCenter);
    value_out = new QLabel(QString::fromUtf8("\xe2\x80\x94"));
    value_out->setObjectName(QStringLiteral("profStatValue"));
    value_out->setProperty("tone", QString::fromLatin1(tone));
    value_out->setAlignment(Qt::AlignCenter);
    vl->addWidget(value_out);
    auto* l = new QLabel(label);
    l->setObjectName(QStringLiteral("profStatLabel"));
    l->setAlignment(Qt::AlignCenter);
    vl->addWidget(l);
    return w;
}

QScrollArea* ProfileScreen::make_scroll_page(QVBoxLayout*& layout_out) {
    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("profScroll"));
    scroll->setWidgetResizable(true);
    auto* page = new QWidget(this);
    layout_out = new QVBoxLayout(page);
    layout_out->setContentsMargins(14, 14, 14, 14);
    layout_out->setSpacing(10);
    scroll->setWidget(page);
    return scroll;
}

ProfileScreen::ProfileScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("profRoot"));
    // A QWidget subclass only paints a stylesheet background with this set.
    setAttribute(Qt::WA_StyledBackground, true);
    apply_theme();
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
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });
    refresh_all();
    // Style the first tab as active on load
    on_section_changed(0);
}

void ProfileScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        rebuild_sections();
    }
    QWidget::changeEvent(event);
}

void ProfileScreen::retranslateUi() {
    if (header_title_)
        header_title_->setText(tr("PROFILE & ACCOUNT"));
    if (header_refresh_btn_)
        header_refresh_btn_->setText(tr("REFRESH"));
    for (int i = 0; i < nav_buttons_.size() && i < nav_source_keys_.size(); ++i) {
        if (nav_buttons_[i])
            nav_buttons_[i]->setText(tr(nav_source_keys_[i].toUtf8().constData()));
    }
}

void ProfileScreen::rebuild_sections() {
    if (!sections_)
        return;
    const int current = sections_->currentIndex();

    // Build factories in the same order they were originally added so the
    // index remains stable for restore_state() and on_section_changed().
    QList<QWidget*> fresh;
    fresh << build_overview() << build_usage() << build_security() << build_billing() << build_support();

    for (int i = 0; i < fresh.size(); ++i) {
        sections_->insertWidget(i, fresh[i]);
        QWidget* old = sections_->widget(i + 1);
        if (old) {
            sections_->removeWidget(old);
            old->deleteLater();
        }
    }
    if (current >= 0 && current < sections_->count())
        sections_->setCurrentIndex(current);
    // Refresh dynamic fields against the new section widgets (overview, usage etc.)
    refresh_all();
}

void ProfileScreen::build_header(QVBoxLayout* root) {
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("profHeaderBar"));
    bar->setFixedHeight(38);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);
    header_title_ = new QLabel(tr("PROFILE & ACCOUNT"));
    header_title_->setObjectName(QStringLiteral("profHeaderTitle"));
    hl->addWidget(header_title_);
    username_header_ = new QLabel;
    username_header_->setObjectName(QStringLiteral("profHeaderUser"));
    hl->addWidget(username_header_);
    hl->addStretch();
    credits_badge_ = new QLabel;
    credits_badge_->setObjectName(QStringLiteral("profCreditsBadge"));
    hl->addWidget(credits_badge_);
    plan_badge_ = new QLabel;
    plan_badge_->setObjectName(QStringLiteral("profPlanBadge"));
    hl->addWidget(plan_badge_);
    header_refresh_btn_ = new QPushButton(tr("REFRESH"));
    header_refresh_btn_->setObjectName(QStringLiteral("profBtn"));
    header_refresh_btn_->setFixedHeight(22);
    header_refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(header_refresh_btn_, &QPushButton::clicked, this, [this]() {
        auth::AuthManager::instance().refresh_user_data();
        // Usage / Security / Billing data is fetched only when its tab is opened, so
        // REFRESH has to re-run the fetch for whichever tab is showing — it used to
        // refresh just the account header and leave those tables stale.
        if (sections_)
            on_section_changed(sections_->currentIndex());
    });
    hl->addWidget(header_refresh_btn_);
    root->addWidget(bar);
}

void ProfileScreen::build_tab_nav(QVBoxLayout* root) {
    auto* nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("profTabNav"));
    nav->setFixedHeight(32);
    auto* hl = new QHBoxLayout(nav);
    hl->setContentsMargins(4, 0, 4, 0);
    hl->setSpacing(0);
    // Keep English source keys aligned with nav_buttons_ so retranslateUi
    // can reapply tr() without rebuilding the nav bar (preserves the
    // currently-active highlight).
    nav_source_keys_ = {QStringLiteral("OVERVIEW"), QStringLiteral("USAGE"), QStringLiteral("SECURITY"),
                        QStringLiteral("BILLING"), QStringLiteral("SUPPORT")};
    for (int i = 0; i < nav_source_keys_.size(); i++) {
        auto* btn = new QPushButton(tr(nav_source_keys_[i].toUtf8().constData()));
        btn->setObjectName(QStringLiteral("profNavBtn"));
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
    for (int i = 0; i < nav_buttons_.size(); ++i)
        profile_set_prop(nav_buttons_[i], "active", i == index ? QStringLiteral("true") : QStringLiteral("false"));
    if (index == 1)
        fetch_usage_data();
    else if (index == 2)
        fetch_login_history();
    else if (index == 3)
        fetch_billing_data();
}

QWidget* ProfileScreen::build_overview() {
    QVBoxLayout* vl = nullptr;
    auto* scroll = make_scroll_page(vl);
    auto* grid = new QGridLayout;
    grid->setSpacing(10);

    auto* acct = make_panel(tr("ACCOUNT INFORMATION"));
    auto* avl = qobject_cast<QVBoxLayout*>(acct->layout());
    avl->addWidget(make_data_row(tr("USERNAME"), ov_username_));
    avl->addWidget(make_data_row(tr("EMAIL"), ov_email_));
    avl->addWidget(make_data_row(tr("USER TYPE"), ov_user_type_));
    avl->addWidget(make_data_row(tr("ACCOUNT TYPE"), ov_account_type_));
    avl->addWidget(make_data_row(tr("PHONE"), ov_phone_));
    avl->addWidget(make_data_row(tr("COUNTRY"), ov_country_));
    avl->addWidget(make_data_row(tr("EMAIL VERIFIED"), ov_verified_));
    avl->addWidget(make_data_row(tr("2FA ENABLED"), ov_mfa_));
    avl->addWidget(make_data_row(tr("MEMBER SINCE"), ov_member_since_));
    avl->addWidget(make_data_row(tr("LAST LOGIN"), ov_last_login_));
    {
        auto* erow = new QHBoxLayout;
        erow->setContentsMargins(12, 8, 12, 8);
        auto* eb = new QPushButton(tr("EDIT PROFILE"));
        eb->setObjectName(QStringLiteral("profBtn"));
        eb->setFixedHeight(26);
        eb->setCursor(Qt::PointingHandCursor);
        connect(eb, &QPushButton::clicked, this, &ProfileScreen::show_edit_profile_dialog);
        erow->addWidget(eb);
        erow->addStretch();
        avl->addLayout(erow);
    }
    grid->addWidget(acct, 0, 0);

    auto* cred = make_panel(tr("CREDITS & BALANCE"));
    auto* cvl2 = qobject_cast<QVBoxLayout*>(cred->layout());
    ov_credits_big_ = new QLabel(QStringLiteral("0"));
    ov_credits_big_->setObjectName(QStringLiteral("profCreditsBig"));
    ov_credits_big_->setAlignment(Qt::AlignCenter);
    cvl2->addWidget(ov_credits_big_);
    auto* cl = new QLabel(tr("AVAILABLE CREDITS"));
    cl->setObjectName(QStringLiteral("profCreditsCaption"));
    cl->setAlignment(Qt::AlignCenter);
    cvl2->addWidget(cl);
    auto* sp = new QFrame;
    sp->setObjectName(QStringLiteral("profSep"));
    sp->setFixedHeight(1);
    cvl2->addWidget(sp);
    cvl2->addWidget(make_data_row(tr("PLAN"), ov_plan_));
    cvl2->addWidget(make_data_row(tr("CREDITS EXPIRE"), ov_credits_expire_));
    grid->addWidget(cred, 0, 1);

    auto* actions = make_panel(tr("QUICK ACTIONS"));
    auto* act_vl = qobject_cast<QVBoxLayout*>(actions->layout());
    auto* arl = new QHBoxLayout;
    arl->setContentsMargins(12, 8, 12, 8);
    arl->setSpacing(10);
    auto* eb2 = new QPushButton(tr("EDIT PROFILE"));
    eb2->setObjectName(QStringLiteral("profBtn"));
    eb2->setFixedHeight(26);
    eb2->setCursor(Qt::PointingHandCursor);
    connect(eb2, &QPushButton::clicked, this, &ProfileScreen::show_edit_profile_dialog);
    arl->addWidget(eb2);
    auto* lb = new QPushButton(tr("LOGOUT"));
    lb->setObjectName(QStringLiteral("profBtnDanger"));
    lb->setFixedHeight(26);
    lb->setCursor(Qt::PointingHandCursor);
    connect(lb, &QPushButton::clicked, this, &ProfileScreen::show_logout_confirm);
    arl->addWidget(lb);
    auto* dab = new QPushButton(tr("DELETE ACCOUNT"));
    dab->setObjectName(QStringLiteral("profBtnDanger"));
    dab->setFixedHeight(26);
    dab->setCursor(Qt::PointingHandCursor);
    connect(dab, &QPushButton::clicked, this, &ProfileScreen::show_delete_account_dialog);
    arl->addWidget(dab);
    arl->addStretch();
    act_vl->addLayout(arl);
    grid->addWidget(actions, 1, 0, 1, 2);
    vl->addLayout(grid);
    vl->addStretch();
    return scroll;
}

QWidget* ProfileScreen::build_usage() {
    QVBoxLayout* vl = nullptr;
    auto* scroll = make_scroll_page(vl);
    auto* srl = new QHBoxLayout;
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(8);
    srl->addWidget(make_stat_box(tr("CREDIT BALANCE"), usg_credits_, "cyan"));
    srl->addWidget(make_stat_box(tr("ACCOUNT TYPE"), usg_plan_, "amber"));
    srl->addWidget(make_stat_box(tr("RATE LIMIT/HR"), usg_rate_, ""));
    vl->addLayout(srl);
    auto* sp = make_panel(tr("USAGE SUMMARY \xe2\x80\x94 LAST 30 DAYS"));
    auto* svl = qobject_cast<QVBoxLayout*>(sp->layout());
    auto* smrl = new QHBoxLayout;
    smrl->setContentsMargins(8, 8, 8, 8);
    smrl->setSpacing(8);
    smrl->addWidget(make_stat_box(tr("TOTAL REQUESTS"), usg_total_req_, "cyan"));
    smrl->addWidget(make_stat_box(tr("CREDITS USED"), usg_cred_used_, "warn"));
    smrl->addWidget(make_stat_box(tr("AVG CR/REQ"), usg_avg_cred_, ""));
    smrl->addWidget(make_stat_box(tr("AVG RESP (ms)"), usg_avg_resp_, ""));
    svl->addLayout(smrl);
    vl->addWidget(sp);
    auto* dp = make_panel(tr("DAILY USAGE"));
    usg_daily_table_ = profile_make_table({tr("DATE"), tr("REQUESTS"), tr("CREDITS")}, 200);
    qobject_cast<QVBoxLayout*>(dp->layout())->addWidget(usg_daily_table_);
    vl->addWidget(dp);
    auto* ep = make_panel(tr("TOP ENDPOINTS"));
    usg_endpoint_table_ = profile_make_table({tr("ENDPOINT"), tr("REQUESTS"), tr("CREDITS"), tr("AVG MS")}, 200);
    qobject_cast<QVBoxLayout*>(ep->layout())->addWidget(usg_endpoint_table_);
    vl->addWidget(ep);
    vl->addStretch();
    return scroll;
}

QWidget* ProfileScreen::build_security() {
    QVBoxLayout* vl = nullptr;
    auto* scroll = make_scroll_page(vl);
    auto* kp = make_panel(tr("API KEY"));
    auto* kvl = qobject_cast<QVBoxLayout*>(kp->layout());
    auto* krl = new QHBoxLayout;
    krl->setContentsMargins(12, 10, 12, 10);
    krl->setSpacing(8);
    sec_api_key_ = new QLabel(QString(20, QChar(0x2022)));
    sec_api_key_->setObjectName(QStringLiteral("profApiKey"));
    krl->addWidget(sec_api_key_, 1);
    auto* sb = new QPushButton(tr("SHOW"));
    sb->setObjectName(QStringLiteral("profBtn"));
    sb->setFixedHeight(22);
    sb->setCursor(Qt::PointingHandCursor);
    connect(sb, &QPushButton::clicked, this, [this, sb]() {
        api_key_visible_ = !api_key_visible_;
        sb->setText(api_key_visible_ ? tr("HIDE") : tr("SHOW"));
        sec_api_key_->setText(api_key_visible_ ? auth::AuthManager::instance().session().api_key
                                               : QString(20, QChar(0x2022)));
        // Auto re-mask. A revealed key otherwise stays on screen for the rest
        // of the session — through screen shares, shoulder-surfing, and the
        // terminal's own auto-lock, which does not repaint this panel.
        if (api_key_visible_) {
            QPointer<ProfileScreen> self = this;
            QTimer::singleShot(30000, sb, [self, sb]() {
                if (!self || !self->api_key_visible_)
                    return;
                self->api_key_visible_ = false;
                sb->setText(tr("SHOW"));
                if (self->sec_api_key_)
                    self->sec_api_key_->setText(QString(20, QChar(0x2022)));
            });
        }
    });
    sb->setAccessibleName(tr("Show or hide the API key"));
    krl->addWidget(sb);
    auto* cb = new QPushButton(tr("COPY"));
    cb->setObjectName(QStringLiteral("profBtn"));
    cb->setFixedHeight(22);
    cb->setCursor(Qt::PointingHandCursor);
    connect(cb, &QPushButton::clicked, this, [cb]() {
        auto key = auth::AuthManager::instance().session().api_key;
        if (key.isEmpty())
            return;
        QApplication::clipboard()->setText(key);
        cb->setText(tr("COPIED"));
        QTimer::singleShot(1500, cb, [cb]() { cb->setText(tr("CLEARS 60s")); });
        // The API key is a bearer credential. Leaving it on the system
        // clipboard indefinitely puts it in Windows clipboard history and in
        // reach of every other process on the machine, so drop it once the
        // user has had time to paste it. Only clear if the clipboard still
        // holds exactly this key — never stomp on something the user copied
        // afterwards.
        QTimer::singleShot(60000, cb, [cb, key]() {
            auto* clip = QApplication::clipboard();
            if (clip && clip->text() == key)
                clip->clear();
            cb->setText(tr("COPY"));
        });
    });
    cb->setAccessibleName(tr("Copy the API key to the clipboard"));
    krl->addWidget(cb);
    auto* rg = new QPushButton(tr("REGENERATE"));
    rg->setObjectName(QStringLiteral("profBtnAmber"));
    rg->setFixedHeight(22);
    rg->setCursor(Qt::PointingHandCursor);
    connect(rg, &QPushButton::clicked, this, &ProfileScreen::show_regen_confirm);
    krl->addWidget(rg);
    kvl->addLayout(krl);
    vl->addWidget(kp);
    auto* ssp = make_panel(tr("SECURITY STATUS"));
    auto* ssvl = qobject_cast<QVBoxLayout*>(ssp->layout());
    ssvl->addWidget(make_data_row(tr("EMAIL VERIFIED"), sec_verified_));
    ssvl->addWidget(make_data_row(tr("2FA (MFA)"), sec_mfa_));
    vl->addWidget(ssp);
    auto* hp = make_panel(tr("LOGIN HISTORY"));
    sec_login_hist_ =
        profile_make_table({tr("TIME (LOCAL)"), tr("IP ADDRESS"), tr("DEVICE"), tr("STATUS")}, 200);
    qobject_cast<QVBoxLayout*>(hp->layout())->addWidget(sec_login_hist_);
    vl->addWidget(hp);
    vl->addStretch();
    return scroll;
}

QWidget* ProfileScreen::build_billing() {
    QVBoxLayout* vl = nullptr;
    auto* scroll = make_scroll_page(vl);
    auto* sp = make_panel(tr("SUBSCRIPTION"));
    auto* svl = qobject_cast<QVBoxLayout*>(sp->layout());
    svl->addWidget(make_data_row(tr("PLAN"), bill_plan_));
    svl->addWidget(make_data_row(tr("CREDIT BALANCE"), bill_credits_));
    svl->addWidget(make_data_row(tr("SUPPORT TYPE"), bill_support_));
    // UPGRADE TO PRO (free) / MANAGE PLAN (paid) → Plans & Pricing. Text and
    // style follow the plan in refresh_all(); the hint and button style
    // themselves (EnterprisePromo helpers).
    {
        auto* prl = new QHBoxLayout;
        prl->setContentsMargins(12, 10, 12, 10);
        prl->setSpacing(12);
        auto* hint = ui::enterprise::make_mention_label(sp);
        hint->setObjectName("profilePlanHint");
        prl->addWidget(hint, 1);
        auto* plan_btn = ui::enterprise::make_cta_button(QString(), QString(), ui::enterprise::CtaStyle::Primary, sp);
        plan_btn->setObjectName("profilePlanBtn");
        connect(plan_btn, &QPushButton::clicked, this, [this]() { ui::billing::open_plans(this); });
        prl->addWidget(plan_btn, 0, Qt::AlignVCenter);
        svl->addLayout(prl);
    }
    vl->addWidget(sp);
    auto* hp = make_panel(tr("PAYMENT HISTORY"));
    bill_history_ = profile_make_table({tr("DATE"), tr("PLAN"), tr("AMOUNT"), tr("CREDITS"), tr("STATUS")}, 250);
    qobject_cast<QVBoxLayout*>(hp->layout())->addWidget(bill_history_);
    vl->addWidget(hp);
    vl->addStretch();
    return scroll;
}

QWidget* ProfileScreen::build_support() {
    QVBoxLayout* vl = nullptr;
    auto* scroll = make_scroll_page(vl);
    auto* cp = make_panel(tr("CONTACT US"));
    auto* cvl2 = qobject_cast<QVBoxLayout*>(cp->layout());
    auto* cg = new QGridLayout;
    cg->setSpacing(12);
    cg->setContentsMargins(12, 10, 12, 10);
    auto add_c = [&](const QString& l, const QString& e, int r, int c2) {
        auto* wl = new QVBoxLayout;
        wl->setContentsMargins(0, 0, 0, 0);
        wl->setSpacing(2);
        auto* lb = new QLabel(l);
        lb->setObjectName(QStringLiteral("profContactLabel"));
        wl->addWidget(lb);
        auto* em = new QLabel(e);
        em->setObjectName(QStringLiteral("profContactValue"));
        em->setTextInteractionFlags(Qt::TextSelectableByMouse);
        wl->addWidget(em);
        cg->addLayout(wl, r, c2);
    };
    add_c(tr("GENERAL SUPPORT"), "support@fincept.in", 0, 0);
    add_c(tr("COMMERCIAL"), "support@fincept.in", 0, 1);
    add_c(tr("SECURITY"), "support@fincept.in", 1, 0);
    add_c(tr("LEGAL"), "support@fincept.in", 1, 1);
    cvl2->addLayout(cg);
    vl->addWidget(cp);
    auto* lp = make_panel(tr("RESOURCES"));
    auto* lvl = qobject_cast<QVBoxLayout*>(lp->layout());
    auto* lrl = new QHBoxLayout;
    lrl->setContentsMargins(12, 10, 12, 10);
    lrl->setSpacing(8);
    auto make_link_btn = [&](const QString& label, const QString& url) {
        auto* b = new QPushButton(label);
        b->setObjectName(QStringLiteral("profLinkBtn"));
        b->setFixedHeight(26);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(url);
        connect(b, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        lrl->addWidget(b);
    };
    // DOCS / FAQ are localisable; GITHUB / DISCORD are brand names — left raw.
    make_link_btn(tr("DOCS"), "https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs");
    make_link_btn(QStringLiteral("GITHUB"), "https://github.com/Fincept-Corporation/FinceptTerminal");
    make_link_btn(QStringLiteral("DISCORD"), "https://discord.gg/ae87a8ygbN");
    make_link_btn(tr("FAQ"), "https://github.com/Fincept-Corporation/FinceptTerminal/wiki");
    lrl->addStretch();
    lvl->addLayout(lrl);
    vl->addWidget(lp);
    vl->addStretch();
    return scroll;
}

void ProfileScreen::refresh_all() {
    const auto& s = auth::AuthManager::instance().session();
    if (!s.authenticated)
        return;
    username_header_->setText(s.user_info.username.isEmpty() ? s.user_info.email : s.user_info.username);
    credits_badge_->setText(tr("CR %1").arg(auth::format_credits(s.user_info.credit_balance)));
    plan_badge_->setText(s.plan_label());
    ov_username_->setText(s.user_info.username.isEmpty() ? tr("N/A") : s.user_info.username);
    ov_email_->setText(s.user_info.email.isEmpty() ? tr("N/A") : s.user_info.email);
    ov_user_type_->setText(tr("REGISTERED"));
    ov_account_type_->setText(s.plan_label());
    profile_set_tone(ov_account_type_, "amber");
    ov_phone_->setText(s.user_info.phone.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : s.user_info.phone);
    ov_country_->setText(s.user_info.country.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : s.user_info.country);
    ov_verified_->setText(s.user_info.is_verified ? tr("YES") : tr("NO"));
    profile_set_tone(ov_verified_, s.user_info.is_verified ? "pos" : "neg");
    ov_mfa_->setText(s.user_info.mfa_enabled ? tr("YES") : tr("NO"));
    profile_set_tone(ov_mfa_, s.user_info.mfa_enabled ? "pos" : "neg");
    ov_member_since_->setText(profile_local_stamp(s.user_info.created_at, false));
    ov_last_login_->setText(profile_local_stamp(s.user_info.last_login_at, true));
    ov_credits_expire_->setText(profile_local_stamp(s.user_info.credits_expire_at, true));
    ov_credits_big_->setText(auth::format_credits(s.user_info.credit_balance));
    ov_plan_->setText(s.plan_label());
    profile_set_tone(ov_plan_, "amber");
    sec_verified_->setText(s.user_info.is_verified ? tr("YES") : tr("NO"));
    profile_set_tone(sec_verified_, s.user_info.is_verified ? "pos" : "neg");
    sec_mfa_->setText(s.user_info.mfa_enabled ? tr("ENABLED") : tr("DISABLED"));
    profile_set_tone(sec_mfa_, s.user_info.mfa_enabled ? "pos" : "dim");
    bill_plan_->setText(s.plan_label());
    bill_credits_->setText(auth::format_credits(s.user_info.credit_balance));
    // bill_support_ is populated by fetch_billing_data() from the API; leave it as-is here

    // Billing → Plans & Pricing. findChildren: rebuild_sections() can leave a
    // superseded Billing page alive until deleteLater runs — update both.
    const bool paid = s.has_paid_plan();
    for (auto* btn : findChildren<QPushButton*>(QStringLiteral("profilePlanBtn"))) {
        btn->setText(paid ? tr("MANAGE PLAN") : tr("UPGRADE TO PRO"));
        ui::enterprise::set_cta_style(btn, paid ? ui::enterprise::CtaStyle::Outline : ui::enterprise::CtaStyle::Primary);
    }
    for (auto* hint : findChildren<QLabel*>(QStringLiteral("profilePlanHint")))
        hint->setText(paid ? tr("See plans and credits, or compare with Fincept Terminal Enterprise for teams.")
                           : tr("Pro — $10/month: monthly credits, higher limits, every cloud analytics module and "
                                "priority support."));
}

void ProfileScreen::fetch_usage_data() {
    // Populate stat boxes with session data as fallback
    const auto& s = auth::AuthManager::instance().session();
    usg_credits_->setText(auth::format_credits(s.user_info.credit_balance));
    usg_plan_->setText(s.plan_label());
    // /user/profile now returns the rate-limit window directly — show it
    // immediately instead of "—" while /user/usage is in flight.
    const int rl_limit = s.user_info.rate_limit.limit;
    usg_rate_->setText(rl_limit > 0 ? QString::number(rl_limit) : QString::fromUtf8("\xe2\x80\x94"));

    profile_table_loading(usg_daily_table_, tr("Loading..."));
    profile_table_loading(usg_endpoint_table_, tr("Loading..."));

    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().get_user_usage(
        30,
        [self](auth::ApiResponse r) {
            if (!self)
                return;
            if (!r.success) {
                LOG_WARN("Profile", "Usage fetch failed: " + r.error);
                const QString msg = tr("Could not load usage data — press REFRESH to retry.");
                profile_table_message(self->usg_daily_table_, msg);
                profile_table_message(self->usg_endpoint_table_, msg);
                return;
            }
            auto payload = r.data.contains("data") ? r.data["data"].toObject() : r.data;
            if (payload.contains("account")) {
                auto a = payload["account"].toObject();
                self->usg_credits_->setText(auth::format_credits(a["credit_balance"].toDouble()));
                self->usg_plan_->setText(profile_plan_text(a));
                // Keep the session's figure when the payload omits the limit,
                // rather than overwriting it with 0.
                const int per_hour = a["rate_limit_per_hour"].toInt();
                if (per_hour > 0)
                    self->usg_rate_->setText(QString::number(per_hour));
            }
            if (payload.contains("summary")) {
                auto s = payload["summary"].toObject();
                self->usg_total_req_->setText(QLocale(QLocale::English).toString(s["total_requests"].toInt()));
                // Credits are fractional (an AI call can cost 0.35); 'f', 0 rounded
                // a month of small calls down to "0".
                self->usg_cred_used_->setText(auth::format_credits(s["total_credits_used"].toDouble()));
                self->usg_avg_cred_->setText(auth::format_credits(s["avg_credits_per_request"].toDouble()));
                self->usg_avg_resp_->setText(QString::number(s["avg_response_time_ms"].toDouble(), 'f', 0));
            }
            {
                // A payload without the key means "no rows", not "keep whatever was
                // there" — otherwise the Loading... placeholder never goes away.
                auto d = payload["daily_usage"].toArray();
                self->usg_daily_table_->clearSpans();
                self->usg_daily_table_->setRowCount(0);
                for (int i = d.size() - 1; i >= 0 && i >= d.size() - 10; i--) {
                    auto e = d[i].toObject();
                    int row = self->usg_daily_table_->rowCount();
                    self->usg_daily_table_->insertRow(row);
                    self->usg_daily_table_->setItem(row, 0, new QTableWidgetItem(e["date"].toString()));
                    self->usg_daily_table_->setItem(row, 1,
                                                    profile_num_item(QString::number(e["request_count"].toInt())));
                    self->usg_daily_table_->setItem(
                        row, 2, profile_num_item(auth::format_credits(e["credits_used"].toDouble())));
                }
                if (self->usg_daily_table_->rowCount() == 0)
                    profile_table_message(self->usg_daily_table_, tr("No usage recorded in the last 30 days."));
            }
            {
                auto eps = payload["endpoint_breakdown"].toArray();
                self->usg_endpoint_table_->clearSpans();
                self->usg_endpoint_table_->setRowCount(0);
                for (const auto& v : eps) {
                    auto e = v.toObject();
                    int row = self->usg_endpoint_table_->rowCount();
                    self->usg_endpoint_table_->insertRow(row);
                    self->usg_endpoint_table_->setItem(row, 0, new QTableWidgetItem(e["endpoint"].toString()));
                    self->usg_endpoint_table_->setItem(row, 1,
                                                       profile_num_item(QString::number(e["request_count"].toInt())));
                    self->usg_endpoint_table_->setItem(
                        row, 2, profile_num_item(auth::format_credits(e["credits_used"].toDouble())));
                    self->usg_endpoint_table_->setItem(
                        row, 3, profile_num_item(QString::number(e["avg_response_time_ms"].toDouble(), 'f', 0)));
                }
                if (self->usg_endpoint_table_->rowCount() == 0)
                    profile_table_message(self->usg_endpoint_table_, tr("No endpoint activity recorded."));
            }
        },
        this);
}

void ProfileScreen::fetch_billing_data() {
    LOG_INFO("Profile", "Fetching billing data...");
    profile_table_loading(bill_history_, tr("Loading..."));
    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().get_user_subscription(
        [self](auth::ApiResponse r) {
            if (!self)
                return;
            if (!r.success) {
                // refresh_all() already filled plan / credits from the session;
                // only the support type is unknown.
                LOG_WARN("Profile", "Subscription fetch failed: " + r.error);
                return;
            }
            const QJsonObject d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
            // Never blank a field the payload does not carry: this used to read a
            // different endpoint and wiped PLAN to "" and CREDIT BALANCE to 0.00.
            if (d.contains("account_type"))
                self->bill_plan_->setText(profile_plan_text(d));
            if (d.contains("credit_balance"))
                self->bill_credits_->setText(auth::format_credits(d["credit_balance"].toDouble()));
            const QString support = d["support_type"].toString().trimmed();
            self->bill_support_->setText(support.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : support.toUpper());
        },
        this);
    auth::UserApi::instance().get_payment_history(
        1, 20,
        [self](auth::ApiResponse r) {
            if (!self)
                return;
            if (!r.success) {
                LOG_WARN("Profile", "Payment history failed: " + r.error);
                profile_table_message(self->bill_history_,
                                      tr("Could not load payment history — press REFRESH to retry."));
                return;
            }
            auto d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
            auto p = d["payments"].toArray();
            self->bill_history_->clearSpans();
            self->bill_history_->setRowCount(0);
            const QString dash = QString::fromUtf8("\xe2\x80\x94");
            for (const auto& v : p) {
                auto e = v.toObject();
                int row = self->bill_history_->rowCount();
                self->bill_history_->insertRow(row);
                self->bill_history_->setItem(row, 0,
                                             new QTableWidgetItem(profile_local_stamp(e["created_at"].toString(), false)));
                const QString plan = e["plan_name"].toString();
                self->bill_history_->setItem(row, 1, new QTableWidgetItem(plan.isEmpty() ? dash : plan));
                // The field is amount_USD: it was charged in dollars, so pin the symbol
                // instead of showing it in whatever display currency is selected (a
                // $49 payment read "EUR 49.00" for a euro user — no conversion happens).
                self->bill_history_->setItem(
                    row, 2, profile_num_item(cur::money(e["amount_usd"].toDouble(), false, QStringLiteral("USD"))));
                const QJsonValue credits = e["credits_purchased"];
                self->bill_history_->setItem(
                    row, 3,
                    profile_num_item(credits.isNull() || credits.isUndefined() ? dash
                                                                                : auth::format_credits(credits.toDouble())));
                self->bill_history_->setItem(row, 4, new QTableWidgetItem(e["status"].toString().toUpper()));
            }
            if (self->bill_history_->rowCount() == 0)
                profile_table_message(self->bill_history_, tr("No payments yet."));
            const int total = d["pagination"].toObject()["total_count"].toInt();
            self->bill_history_->setToolTip(total > p.size() ? tr("Showing the latest %1 of %2 payments.")
                                                                    .arg(p.size())
                                                                    .arg(total)
                                                              : QString());
        },
        this);
}

void ProfileScreen::fetch_login_history() {
    LOG_INFO("Profile", "Fetching login history...");
    profile_table_loading(sec_login_hist_, tr("Loading..."));
    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().get_login_history(
        20, 0,
        [self](auth::ApiResponse r) {
            if (!self)
                return;
            if (!r.success) {
                LOG_WARN("Profile", "Login history failed: " + r.error);
                profile_table_message(self->sec_login_hist_,
                                      tr("Could not load login history — press REFRESH to retry."));
                return;
            }
            auto d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
            auto h = d["login_history"].toArray();
            if (h.isEmpty())
                h = d["history"].toArray();
            self->sec_login_hist_->clearSpans();
            self->sec_login_hist_->setRowCount(0);
            const QString dash = QString::fromUtf8("\xe2\x80\x94");
            for (const auto& v : h) {
                auto e = v.toObject();
                int row = self->sec_login_hist_->rowCount();
                self->sec_login_hist_->insertRow(row);
                // The API sends created_at / login_successful / failure_reason; this
                // table read "timestamp" and "status", so both columns were blank.
                const QString when = e.contains("created_at") ? e["created_at"].toString() : e["timestamp"].toString();
                self->sec_login_hist_->setItem(row, 0, new QTableWidgetItem(profile_local_stamp(when, true)));
                const QString ip = e["ip_address"].toString();
                self->sec_login_hist_->setItem(row, 1, new QTableWidgetItem(ip.isEmpty() ? dash : ip));
                QString device = e["device_info"].toString();
                if (device.isEmpty())
                    device = e["user_agent"].toString();
                auto* dev_item = new QTableWidgetItem(device.isEmpty() ? dash : device);
                dev_item->setToolTip(e["user_agent"].toString());
                self->sec_login_hist_->setItem(row, 2, dev_item);
                QString status;
                bool failed = false;
                if (e.contains("login_successful")) {
                    failed = !e["login_successful"].toBool();
                    status = failed ? tr("FAILED") : tr("SUCCESS");
                    const QString reason = e["failure_reason"].toString();
                    if (failed && !reason.isEmpty())
                        status += QStringLiteral(" — ") + reason;
                } else {
                    status = e["status"].toString().toUpper();
                }
                auto* st_item = new QTableWidgetItem(status);
                if (failed)
                    st_item->setForeground(QColor(ui::colors::NEGATIVE()));
                self->sec_login_hist_->setItem(row, 3, st_item);
            }
            if (self->sec_login_hist_->rowCount() == 0)
                profile_table_message(self->sec_login_hist_, tr("No login history recorded."));
        },
        this);
}

void ProfileScreen::show_edit_profile_dialog() {
    auto* dlg = new QDialog(this);
    dlg->setObjectName(QStringLiteral("profDialog"));
    dlg->setWindowTitle(tr("Edit Profile"));
    dlg->setFixedSize(420, 300);
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);
    auto* t = new QLabel(tr("EDIT PROFILE"));
    t->setObjectName(QStringLiteral("profDialogTitle"));
    vl->addWidget(t);
    const auto& s = auth::AuthManager::instance().session();
    auto add_f = [&](const QString& l, const QString& v) -> QLineEdit* {
        auto* lb = new QLabel(l);
        lb->setObjectName(QStringLiteral("profFieldLabel"));
        vl->addWidget(lb);
        auto* i = new QLineEdit(v);
        i->setFixedHeight(28);
        vl->addWidget(i);
        return i;
    };
    auto* un = add_f(tr("USERNAME"), s.user_info.username);
    auto* ph = add_f(tr("PHONE (with country code)"), s.user_info.phone);
    auto* co = add_f(tr("COUNTRY"), s.user_info.country);
    auto* brl = new QHBoxLayout;
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(8);
    brl->addStretch();
    auto* cn = new QPushButton(tr("CANCEL"));
    cn->setObjectName(QStringLiteral("profBtn"));
    cn->setFixedHeight(26);
    connect(cn, &QPushButton::clicked, dlg, &QDialog::reject);
    brl->addWidget(cn);
    auto* sv = new QPushButton(tr("SAVE"));
    sv->setObjectName(QStringLiteral("profBtnPrimary"));
    sv->setFixedHeight(26);
    sv->setDefault(true);
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
        if (data.isEmpty()) {
            QMessageBox::information(dlg_ptr ? static_cast<QWidget*>(dlg_ptr) : nullptr, tr("Edit Profile"),
                                     tr("Nothing to save — change at least one field."));
            return;
        }
        // One request at a time: a double-click sent the update twice.
        sv->setEnabled(false);
        sv->setText(tr("SAVING…"));
        auth::UserApi::instance().update_user_profile(data, [self, dlg_ptr, sv](auth::ApiResponse r) {
            if (!self)
                return;
            if (r.success) {
                auth::AuthManager::instance().refresh_user_data();
                if (dlg_ptr)
                    dlg_ptr->accept();
                return;
            }
            if (dlg_ptr) {
                sv->setEnabled(true);
                sv->setText(tr("SAVE"));
            }
            // The failure branch was empty: a rejected update (duplicate
            // username, invalid phone) left the dialog sitting there as if
            // the click had never happened.
            LOG_WARN("Profile", "Profile update failed: " + r.error);
            QMessageBox::warning(dlg_ptr ? static_cast<QWidget*>(dlg_ptr) : static_cast<QWidget*>(self),
                                 tr("Update Failed"),
                                 r.error.isEmpty() ? tr("Could not update your profile. Please try again.") : r.error);
        });
    });
    brl->addWidget(sv);
    vl->addLayout(brl);
    dlg->exec();
    dlg->deleteLater();
}

void ProfileScreen::show_logout_confirm() {
    if (QMessageBox::question(this, tr("Confirm Logout"), tr("Are you sure you want to logout?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        auth::AuthManager::instance().logout();
}

void ProfileScreen::show_regen_confirm() {
    if (QMessageBox::warning(this, tr("Regenerate API Key"),
                             tr("Your current API key will be invalidated immediately. Anything using it — scripts, "
                                "integrations, other machines — stops working until you paste the new key.\n\n"
                                "Continue?"),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    QPointer<ProfileScreen> self = this;
    auth::UserApi::instance().regenerate_api_key([self](auth::ApiResponse r) {
        // Guarded: regeneration is a network round trip and the screen can be
        // rebuilt (language change) or destroyed while it is in flight.
        if (!self)
            return;
        if (r.success) {
            auth::AuthManager::instance().refresh_user_data();
            self->api_key_visible_ = false;
            if (self->sec_api_key_)
                self->sec_api_key_->setText(QString(20, QChar(0x2022)));
            LOG_INFO("Profile", "API key regenerated");
            return;
        }
        // Previously a silent no-op — the user could not tell whether their
        // key had been rotated or not.
        LOG_ERROR("Profile", "API key regeneration failed: " + r.error);
        QMessageBox::warning(self, tr("Regeneration Failed"),
                             r.error.isEmpty() ? tr("Could not regenerate your API key. Your existing key is "
                                                    "still valid.")
                                               : r.error);
    });
}

void ProfileScreen::show_delete_account_dialog() {
    const auto& s = auth::AuthManager::instance().session();
    const QString email = s.user_info.email;

    // First confirmation
    auto first =
        QMessageBox::warning(this, tr("Delete Account"),
                             tr("This will permanently delete your Fincept account (%1) and all associated data.\n\n"
                                "This action CANNOT be undone. Are you sure?")
                                 .arg(email),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (first != QMessageBox::Yes)
        return;

    // Second confirmation — type email + enter password to confirm.
    //
    // The password gate used to be unsatisfiable for anyone who signed up via
    // Google: those accounts have no password, the DELETE button stayed
    // permanently disabled, and the dialog offered no way to set one. The
    // reset flow already existed (ForgotPasswordScreen) but was reachable only
    // from Login and Help — both pre-login surfaces — so a signed-in user
    // could not get to it at all. Hence "no input screen or text field is
    // displayed to enter the code" (issue #378).
    //
    // The fix does not try to detect an OAuth account: it can't. Neither
    // UserProfile nor the session carries an auth-provider or has-password
    // field, so the client has no way to know. Instead the set/reset path is
    // put inline here and offered to everyone — which serves the passwordless
    // Google user and the ordinary "I forgot it" user with one control, and
    // needs no new backend field. /user/forgot-password and
    // /user/reset-password are public, email-keyed endpoints, so they work
    // just as well while signed in.
    auto* dlg = new QDialog(this);
    dlg->setObjectName(QStringLiteral("profDialog"));
    dlg->setWindowTitle(tr("Confirm Account Deletion"));
    dlg->setMinimumWidth(420);
    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(10);

    auto* warn = new QLabel(tr("TYPE YOUR EMAIL ADDRESS TO CONFIRM:"));
    warn->setObjectName(QStringLiteral("profDangerLabel"));
    vl->addWidget(warn);

    auto* hint = new QLabel(email);
    hint->setObjectName(QStringLiteral("profMutedLabel"));
    vl->addWidget(hint);

    auto* input = new QLineEdit;
    input->setFixedHeight(28);
    input->setPlaceholderText(email);
    vl->addWidget(input);

    auto* pw_lbl = new QLabel(tr("ENTER YOUR PASSWORD:"));
    pw_lbl->setObjectName(QStringLiteral("profDangerLabel"));
    vl->addWidget(pw_lbl);

    auto* pw_input = new QLineEdit;
    pw_input->setFixedHeight(28);
    pw_input->setEchoMode(QLineEdit::Password);
    pw_input->setPlaceholderText(tr("Current password"));
    vl->addWidget(pw_input);

    // ── Entry point into the inline set/reset flow ───────────────────────────
    auto* reset_link = new QPushButton(tr("Signed in with Google, or forgot your password? Email me a code"));
    reset_link->setObjectName(QStringLiteral("profInlineLink"));
    reset_link->setFlat(true);
    reset_link->setCursor(Qt::PointingHandCursor);
    vl->addWidget(reset_link);

    // ── Reset panel: hidden until the link above is used ─────────────────────
    auto* reset_box = new QWidget;
    reset_box->setVisible(false);
    auto* rl = new QVBoxLayout(reset_box);
    rl->setContentsMargins(0, 4, 0, 0);
    rl->setSpacing(8);

    auto* sep = new QFrame;
    sep->setObjectName(QStringLiteral("profSep"));
    sep->setFixedHeight(1);
    rl->addWidget(sep);

    auto* code_lbl = new QLabel(tr("6-DIGIT CODE FROM YOUR EMAIL:"));
    code_lbl->setObjectName(QStringLiteral("profDangerLabel"));
    rl->addWidget(code_lbl);

    auto* code_input = new QLineEdit;
    code_input->setFixedHeight(28);
    code_input->setPlaceholderText(tr("Verification code"));
    rl->addWidget(code_input);

    auto* new_pw_lbl = new QLabel(tr("NEW PASSWORD (MIN 8 CHARACTERS):"));
    new_pw_lbl->setObjectName(QStringLiteral("profDangerLabel"));
    rl->addWidget(new_pw_lbl);

    auto* new_pw = new QLineEdit;
    new_pw->setFixedHeight(28);
    new_pw->setEchoMode(QLineEdit::Password);
    new_pw->setPlaceholderText(tr("New password"));
    rl->addWidget(new_pw);

    auto* confirm_pw = new QLineEdit;
    confirm_pw->setFixedHeight(28);
    confirm_pw->setEchoMode(QLineEdit::Password);
    confirm_pw->setPlaceholderText(tr("Confirm new password"));
    rl->addWidget(confirm_pw);

    auto* set_pw_btn = new QPushButton(tr("SET PASSWORD"));
    set_pw_btn->setObjectName(QStringLiteral("profBtnPrimary"));
    set_pw_btn->setFixedHeight(26);
    rl->addWidget(set_pw_btn);
    vl->addWidget(reset_box);

    // Shared status line for both the code request and the reset itself.
    auto* status = new QLabel;
    status->setObjectName(QStringLiteral("profMutedLabel"));
    status->setWordWrap(true);
    status->hide();
    vl->addWidget(status);

    // tone: "" (muted), "pos" or "neg" — colours live in apply_theme().
    auto say = [status, dlg](const QString& msg, const char* tone) {
        profile_set_tone(status, tone);
        status->setText(msg);
        status->show();
        // The dialog grows as the reset panel and status line appear; without
        // this it keeps its original height and clips them.
        dlg->adjustSize();
    };

    auto* brl = new QHBoxLayout;
    brl->addStretch();
    auto* cancel = new QPushButton(tr("CANCEL"));
    cancel->setObjectName(QStringLiteral("profBtn"));
    cancel->setFixedHeight(26);
    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);
    brl->addWidget(cancel);

    auto* confirm = new QPushButton(tr("DELETE MY ACCOUNT"));
    confirm->setObjectName(QStringLiteral("profBtnDangerSolid"));
    confirm->setFixedHeight(26);
    confirm->setEnabled(false);
    auto reeval = [confirm, input, pw_input, email]() {
        const bool email_ok = input->text().trimmed() == email;
        const bool pw_ok = !pw_input->text().isEmpty();
        confirm->setEnabled(email_ok && pw_ok);
    };
    connect(input, &QLineEdit::textChanged, this, [reeval](const QString&) { reeval(); });
    connect(pw_input, &QLineEdit::textChanged, this, [reeval](const QString&) { reeval(); });

    // Deliberately NOT captured as a local reference: the lambdas below
    // outlive this stack frame (they are owned by dlg, and a queued HTTP reply
    // can land after exec() returns but before deleteLater() runs), so they
    // call the singleton accessor themselves rather than hold a reference to
    // a local that has gone away.
    auto* auth_mgr = &auth::AuthManager::instance();

    // Reveal the panel and ask for a code in one click — the reporter's flow
    // was "click reset, then look for somewhere to type the code", so the
    // field has to be on screen by the time the email lands.
    connect(reset_link, &QPushButton::clicked, dlg, [reset_box, reset_link, code_input, say, email]() {
        reset_box->setVisible(true);
        reset_link->setText(tr("Re-send code"));
        code_input->setFocus();
        say(tr("Sending a verification code to %1 ...").arg(email), "");
        auth::AuthManager::instance().forgot_password(email);
    });

    // AuthManager's signals are global to the singleton; scoping every
    // connection to `dlg` means they die with the dialog rather than firing
    // into a destroyed lambda later.
    connect(auth_mgr, &auth::AuthManager::forgot_password_sent, dlg, [say, email]() {
        say(tr("Code sent to %1. Enter it above with a new password.").arg(email), "pos");
    });
    connect(auth_mgr, &auth::AuthManager::forgot_password_failed, dlg,
            [say](const QString& err) { say(tr("Could not send code: %1").arg(err), "neg"); });

    connect(set_pw_btn, &QPushButton::clicked, dlg, [set_pw_btn, code_input, new_pw, confirm_pw, say, email]() {
        // Same rules as ForgotPasswordScreen::on_reset_password, so the
        // two paths can't disagree about what a valid password is.
        const QString otp = code_input->text().trimmed();
        if (otp.isEmpty()) {
            say(tr("Enter the verification code from your email"), "neg");
            code_input->setFocus();
            return;
        }
        if (new_pw->text().length() < 8) {
            say(tr("Password must be at least 8 characters"), "neg");
            new_pw->setFocus();
            return;
        }
        if (new_pw->text() != confirm_pw->text()) {
            confirm_pw->clear();
            say(tr("Passwords do not match"), "neg");
            confirm_pw->setFocus();
            return;
        }
        set_pw_btn->setEnabled(false);
        say(tr("Setting password ..."), "");
        auth::AuthManager::instance().reset_password(email, otp, new_pw->text());
    });

    connect(auth_mgr, &auth::AuthManager::password_reset_succeeded, dlg,
            [reset_box, reset_link, set_pw_btn, pw_input, new_pw, say, reeval]() {
                // Carry the new password straight into the delete field: the
                // user just typed it twice, and asking a third time to satisfy
                // a gate they only opened in order to delete is pure friction.
                pw_input->setText(new_pw->text());
                reset_box->setVisible(false);
                reset_link->hide();
                set_pw_btn->setEnabled(true);
                say(tr("Password set. You can now delete your account."), "pos");
                reeval();
            });
    connect(auth_mgr, &auth::AuthManager::password_reset_failed, dlg, [set_pw_btn, say](const QString& err) {
        set_pw_btn->setEnabled(true);
        say(tr("Could not set password: %1").arg(err), "neg");
    });

    QPointer<ProfileScreen> self = this;
    QPointer<QDialog> dlg_ptr = dlg;
    connect(confirm, &QPushButton::clicked, this, [self, dlg_ptr, email, pw_input]() {
        if (!self)
            return;
        const QString password = pw_input->text();
        if (dlg_ptr)
            dlg_ptr->accept();
        auth::UserApi::instance().delete_user_account(email, password, [self](auth::ApiResponse r) {
            if (!self)
                return;
            if (r.success) {
                LOG_INFO("Profile", "Account deleted successfully");
                auth::AuthManager::instance().logout();
                return;
            }
            // Never log r.error's context alongside the password; the error
            // string itself is server wording and safe.
            LOG_ERROR("Profile", "Account deletion failed: " + r.error);
            QString detail = r.error;
            if (r.status_code == 401) {
                // Resetting the password can invalidate the session that was
                // issued before it, so a 401 here is more likely "your session
                // is stale" than "wrong password" — say so rather than sending
                // the user round the password loop again.
                detail = tr("%1\n\nIf you just set a new password, sign out and back in, then retry.").arg(r.error);
            }
            QMessageBox::critical(self, tr("Delete Failed"),
                                  tr("Account deletion failed: %1\n\nPlease contact support@fincept.in").arg(detail));
        });
    });
    brl->addWidget(confirm);
    vl->addLayout(brl);

    dlg->exec();
    dlg->deleteLater();
}

} // namespace fincept::screens
