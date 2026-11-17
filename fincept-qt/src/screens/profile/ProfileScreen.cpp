#include "screens/profile/ProfileScreen.h"

#include "auth/AuthManager.h"
#include "auth/UserApi.h"
#include "core/currency/Currency.h"
#include "core/logging/Logger.h"
#include "screens/common/CommandRows.h"
#include "services/billing/PlanAccess.h"
#include "ui/charts/BarChart.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/PlanHint.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>

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

// The digits of a typed phone number: drops one leading '+' and spaces / dashes /
// dots / brackets; returns an empty string when anything else is left, or no
// digit at all (not a phone number). Used to compare numbers; what is SENT keeps
// the '+' (see the SAVE handler in show_edit_profile_dialog()).
static QString profile_phone_digits(const QString& raw) {
    const QString s = raw.trimmed();
    QString digits;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            digits += c;
        else if ((c == QLatin1Char('+') && i == 0) || c.isSpace() || c == QLatin1Char('-') ||
                 c == QLatin1Char('.') || c == QLatin1Char('(') || c == QLatin1Char(')'))
            continue;
        else
            return {};
    }
    return digits;
}

// The API serialises its timestamps as naive UTC; shown in the user's zone.
// Shared with Plans & Pricing (auth::format_local_stamp), so both screens show
// the same date for the same expiry.
static QString profile_local_stamp(const QString& iso, bool with_time) {
    return auth::format_local_stamp(iso, with_time);
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

namespace {

// The Usage / Security / Billing tables sit in a stack: the table itself (page 0)
// or — created on first use — a skeleton while the first rows load and a compact
// designed state (could not load + RETRY / nothing recorded yet). They used to show
// one grey "Loading..." / message row in the table body.
QStackedWidget* profile_wrap_table(QTableWidget* table, QWidget* parent) {
    auto* stack = new QStackedWidget(parent);
    stack->addWidget(table);
    stack->setMinimumHeight(table->minimumHeight());
    return stack;
}

QStackedWidget* profile_table_stack(QTableWidget* table) {
    return table ? qobject_cast<QStackedWidget*>(table->parentWidget()) : nullptr;
}

// The rows are in: show the table.
void profile_table_rows(QTableWidget* table) {
    if (auto* st = profile_table_stack(table))
        st->setCurrentWidget(table);
}

// Skeleton while the table has nothing to show — a refetch keeps the previous
// rows on screen until the new ones arrive (P11).
void profile_table_loading(QTableWidget* table, const QString& caption) {
    auto* st = profile_table_stack(table);
    if (!st)
        return;
    if (st->currentWidget() == table && table->rowCount() > 0)
        return;
    auto* sk = st->findChild<ui::LoadingSkeleton*>(QStringLiteral("profTableSkeleton"), Qt::FindDirectChildrenOnly);
    if (!sk) {
        sk = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, st);
        sk->setObjectName(QStringLiteral("profTableSkeleton"));
        sk->set_row_count(5);
        sk->set_opaque(false);
        st->addWidget(sk);
    }
    sk->set_message(caption);
    st->setCurrentWidget(sk);
}

// A designed state in place of the table (its rows are dropped); `primary` runs on
// the state's primary button (RETRY, VIEW PLANS), none when empty.
void profile_table_state(QTableWidget* table, const ui::EmptyState::Spec& spec,
                         std::function<void()> primary = std::function<void()>()) {
    auto* st = profile_table_stack(table);
    if (!st)
        return;
    table->setSortingEnabled(false);
    table->clearSpans();
    table->setRowCount(0);
    auto* es = st->findChild<ui::EmptyState*>(QStringLiteral("profTableState"), Qt::FindDirectChildrenOnly);
    if (!es) {
        es = new ui::EmptyState(st);
        es->setObjectName(QStringLiteral("profTableState"));
        es->set_compact(true);
        st->addWidget(es);
    }
    QObject::disconnect(es, &ui::EmptyState::primary_clicked, nullptr, nullptr);
    if (primary)
        QObject::connect(es, &ui::EmptyState::primary_clicked, es, std::move(primary));
    es->apply(spec);
    st->setCurrentWidget(es);
}

// A stat box's value while its figure loads: a one-line skeleton in its place
// (never a bare "—", which reads as "no value"). The box's skeleton is created by
// make_stat_box(); sized here to the value's line at the current font.
void profile_stat_loading(QLabel* value, bool loading) {
    if (!value || !value->parentWidget())
        return;
    auto* sk = value->parentWidget()->findChild<ui::LoadingSkeleton*>(QStringLiteral("profStatSkeleton"),
                                                                     Qt::FindDirectChildrenOnly);
    if (!sk)
        return;
    if (loading) {
        // QLabel#profStatValue is 14 px over the base size (apply_theme()).
        sk->setFixedHeight(QFontMetrics(ui::fonts::font(14)).height());
        sk->setMinimumWidth(ui::fonts::font_px(0) * 3);
    }
    value->setVisible(!loading);
    sk->setVisible(loading);
}

// Which account the sections show: the e-mail, else the user id, else "~"
// (signed in, identity not known yet).
QString profile_account_key(const auth::UserProfile& u) {
    const QString email = u.email.trimmed().toLower();
    if (!email.isEmpty())
        return email;
    return u.id > 0 ? QStringLiteral("id:%1").arg(u.id) : QStringLiteral("~");
}

} // namespace

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

// Right-aligned number that header-click sorting orders by `value`, not by its text.
static QTableWidgetItem* profile_num_item(const QString& text, double value) {
    auto* it = new ui::NumericTableWidgetItem(text, value);
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
                "QLabel#profHeaderTitle{color:%4;font-size:" + fincept::ui::fonts::px(0) + "px;font-weight:700;}"
                "QLabel#profHeaderUser{color:%5;font-size:" + fincept::ui::fonts::px(-1) + "px;}"
                "QLabel#profCreditsBadge{color:%6;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:700;}"
                "QLabel#profPlanBadge{color:%4;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:700;}"
                "QWidget#profTabNav{background:%7;border-bottom:1px solid %3;}"
                "QPushButton#profNavBtn{background:transparent;color:%8;border:none;padding:0 14px;"
                "font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:700;letter-spacing:0.5px;}"
                "QPushButton#profNavBtn:hover{color:%5;}"
                "QPushButton#profNavBtn[active=\"true\"]{background:%4;color:%10;}"
                "QWidget#profPanel{background:%1;border:1px solid %3;}"
                "QWidget#profPanelHdr{background:%7;border:none;border-bottom:1px solid %3;}"
                "QLabel#profPanelTitle{color:%4;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:700;letter-spacing:0.5px;}"
                "QWidget#profRow{background:transparent;border:none;border-bottom:1px solid %3;}"
                "QLabel#profRowLabel{color:%9;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#profRowValue{color:%5;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;}")
            .arg(BG_BASE(), BG_SURFACE(), BORDER_DIM(), AMBER(), TEXT_PRIMARY(), CYAN(), BG_RAISED(), TEXT_SECONDARY(),
                 TEXT_SECONDARY(), TEXT_ON_ACCENT()) +
        QString("QLabel#profRowValue[tone=\"pos\"]{color:%1;}"
                "QLabel#profRowValue[tone=\"neg\"]{color:%2;}"
                "QLabel#profRowValue[tone=\"amber\"]{color:%3;}"
                "QLabel#profRowValue[tone=\"dim\"]{color:%4;}"
                "QWidget#profStatBox{background:%5;border:1px solid %6;}"
                "QLabel#profStatValue{color:%7;font-size:" + fincept::ui::fonts::px(14) + "px;font-weight:700;}"
                "QLabel#profStatValue[tone=\"cyan\"]{color:%8;}"
                "QLabel#profStatValue[tone=\"amber\"]{color:%3;}"
                "QLabel#profStatValue[tone=\"warn\"]{color:%9;}"
                "QLabel#profStatLabel{color:%4;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#profCreditsBig{color:%8;font-size:" + fincept::ui::fonts::px(28) + "px;font-weight:700;padding:20px 0 4px 0;}"
                "QLabel#profCreditsCaption{color:%4;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:0.5px;"
                "padding-bottom:12px;}"
                "QFrame#profSep{background:%6;border:none;}"
                "QLabel#profApiKey{color:%7;font-size:" + fincept::ui::fonts::px(-1) + "px;}")
            .arg(POSITIVE(), NEGATIVE(), AMBER(), TEXT_SECONDARY(), BG_RAISED(), BORDER_DIM(), TEXT_PRIMARY(), CYAN(),
                 WARNING()) +
        QString("QLabel#profContactLabel{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:0.5px;}"
                "QLabel#profContactValue{color:%2;font-size:" + fincept::ui::fonts::px(-1) + "px;}"
                "QPushButton#profBtn{background:%3;color:%4;border:1px solid %5;padding:0 12px;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                "font-weight:700;}"
                "QPushButton#profBtn:hover{color:%6;}"
                "QPushButton#profBtnDanger{background:%7;color:%8;border:1px solid %9;padding:0 12px;"
                "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;}"
                "QPushButton#profBtnDanger:hover{background:%8;color:%10;}")
            .arg(TEXT_TERTIARY(), CYAN(), BG_RAISED(), TEXT_SECONDARY(), BORDER_DIM(), TEXT_PRIMARY(), NEGATIVE_BG(),
                 NEGATIVE(), NEGATIVE_DIM(), TEXT_ON_ACCENT()) +
        QString("QPushButton#profBtnAmber{background:%1;color:%2;border:1px solid %3;padding:0 10px;font-size:" + fincept::ui::fonts::px(-4) + "px;"
                "font-weight:700;}"
                "QPushButton#profBtnAmber:hover{background:%2;color:%4;}"
                "QPushButton#profBtnPrimary{background:%2;color:%4;border:none;padding:0 16px;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                "font-weight:700;}"
                "QPushButton#profBtnPrimary:hover{background:%3;}"
                "QPushButton#profBtnPrimary:disabled{background:%5;color:%6;}"
                "QPushButton#profBtnDangerSolid{background:%7;color:%8;border:none;padding:0 14px;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                "font-weight:700;}"
                "QPushButton#profBtnDangerSolid:disabled{background:%9;color:%6;}")
            .arg(ACCENT_BG(), AMBER(), AMBER_DIM(), TEXT_ON_ACCENT(), BG_RAISED(), TEXT_TERTIARY(), NEGATIVE(),
                 TEXT_ON_ACCENT(), NEGATIVE_BG()) +
        QString("QPushButton#profLinkBtn{background:%1;color:%2;border:1px solid %3;padding:0 12px;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                "QPushButton#profLinkBtn:hover{color:%4;background:%5;}"
                "QPushButton#profInlineLink{background:transparent;border:none;color:%6;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                "text-align:left;padding:0;}"
                "QPushButton#profInlineLink:hover{color:%7;text-decoration:underline;}"
                "QDialog#profDialog{background:%8;}"
                "QLabel#profDialogTitle{color:%7;font-size:" + fincept::ui::fonts::px(0) + "px;font-weight:700;}"
                "QLabel#profFieldLabel{color:%9;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;}")
            .arg(BG_RAISED(), CYAN(), BORDER_DIM(), TEXT_PRIMARY(), BG_HOVER(), INFO(), AMBER(), BG_SURFACE(),
                 TEXT_SECONDARY()) +
        QString("QLabel#profDangerLabel{color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;}"
                "QLabel#profMutedLabel{color:%2;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                "QLabel#profMutedLabel[tone=\"pos\"]{color:%3;}"
                "QLabel#profMutedLabel[tone=\"neg\"]{color:%1;}")
            .arg(NEGATIVE(), TEXT_SECONDARY(), POSITIVE());
    // setStyleSheet() re-styles the whole screen even for an identical sheet.
    if (styleSheet() != sheet)
        setStyleSheet(sheet);

    // Failed rows of the login history carry an item brush (marked Qt::UserRole),
    // which the sheet does not reach.
    if (sec_login_hist_) {
        for (int r = 0; r < sec_login_hist_->rowCount(); ++r) {
            auto* item = sec_login_hist_->item(r, 3);
            if (item && item->data(Qt::UserRole).toBool())
                item->setForeground(QColor(NEGATIVE()));
        }
    }
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
    // Stands in for the value while it loads (profile_stat_loading); hidden otherwise.
    auto* sk = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Text, w);
    sk->setObjectName(QStringLiteral("profStatSkeleton"));
    sk->set_row_count(1);
    sk->setAccessibleName(tr("Loading %1").arg(label));
    sk->hide();
    vl->addWidget(sk);
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
    sections_ = new QStackedWidget(this); // in the screen before its pages are added
    sections_->addWidget(build_overview());
    sections_->addWidget(build_usage());
    sections_->addWidget(build_security());
    sections_->addWidget(build_billing());
    sections_->addWidget(build_support());
    build_tab_nav(root);
    root->addWidget(sections_, 1);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this, &ProfileScreen::refresh_all);
    // Balance moved by a billed call elsewhere in the app (no profile re-fetch).
    connect(&auth::AuthManager::instance(), &auth::AuthManager::credit_balance_changed, this,
            [this](double) { refresh_all(); });
    // The saved sign-in being checked → skeleton, not "signed out".
    connect(&auth::AuthManager::instance(), &auth::AuthManager::loading_changed, this,
            [this](bool) { apply_auth_gate(); });
    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled, and the sheet only when its text changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { apply_theme(); });
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

void ProfileScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    apply_auth_gate();
}

void ProfileScreen::apply_auth_gate() {
    const auto& am = auth::AuthManager::instance();
    const ui::RequirementStatus st = ui::check_requirements({ui::Requirement::signed_in()});
    const bool signed_in = st.ok;
    const QString key = signed_in ? profile_account_key(am.session().user_info) : QString();

    if (key != account_key_) {
        const QString previous = account_key_;
        // The identity of the same sign-in arriving later is not a new account.
        const bool same_sign_in = signed_in && previous == QLatin1String("~");
        account_key_ = key; // first: rebuild_sections() → refresh_all() comes back here
        if (!same_sign_in)
            ++account_generation_; // replies requested for the previous account are dropped
        if (!previous.isEmpty() && !same_sign_in) {
            // Signed out, or another account: nothing of the previous one stays on
            // screen (the tables keep their rows during a refetch, by design).
            if (username_header_)
                username_header_->clear();
            if (credits_badge_)
                credits_badge_->clear();
            if (plan_badge_)
                plan_badge_->clear();
            api_key_visible_ = false;
            rebuild_sections(); // fresh, empty pages; fetch stamps reset
        }
        // This account's data for the tab that is open (Usage / Security / Billing).
        if (signed_in && !same_sign_in && sections_)
            on_section_changed(sections_->currentIndex(), /*force=*/true);
    }

    if (tab_nav_)
        tab_nav_->setVisible(signed_in);
    if (sections_)
        sections_->setVisible(signed_in);
    if (header_refresh_btn_)
        header_refresh_btn_->setVisible(signed_in);

    if (signed_in) {
        if (signin_state_)
            signin_state_->hide();
        if (signin_skeleton_)
            signin_skeleton_->hide();
        return;
    }

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if (am.is_loading()) {
        // A saved sign-in is still being checked: not "signed out" yet.
        if (!signin_skeleton_) {
            signin_skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Form, this);
            signin_skeleton_->set_message(tr("Checking your sign-in…"));
            if (root)
                root->addWidget(signin_skeleton_, 1);
        }
        if (signin_state_)
            signin_state_->hide();
        signin_skeleton_->show();
        return;
    }
    if (!signin_state_) {
        signin_state_ = new ui::EmptyState(this);
        // SIGN IN: the sign-in screen (open_setup_link "login"; it does nothing while a
        // sign-in check is still running).
        connect(signin_state_, &ui::EmptyState::primary_clicked, this, [this]() {
            const ui::RequirementStatus now = ui::check_requirements({ui::Requirement::signed_in()});
            if (!now.ok && now.link.valid())
                ui::open_setup_link(now.link, this);
        });
        if (root)
            root->addWidget(signin_state_, 1);
    }
    ui::EmptyState::Spec spec = ui::EmptyState::needs_setup_spec(st);
    spec.message += QLatin1Char(' ') + tr("Your profile, usage, sign-in history and payments load from your Fincept "
                                          "account.");
    signin_state_->apply(spec);
    if (signin_skeleton_)
        signin_skeleton_->hide();
    signin_state_->show();
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
    // The new section widgets start empty: their server data must load again.
    std::fill(std::begin(tab_fetched_ms_), std::end(tab_fetched_ms_), qint64{0});
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
    header_refresh_btn_->setMinimumHeight(22);
    header_refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(header_refresh_btn_, &QPushButton::clicked, this, [this]() {
        auth::AuthManager::instance().refresh_user_data(auth::AuthManager::Refresh::Now);
        // Usage / Security / Billing data is fetched only when its tab is opened, so
        // REFRESH has to re-run the fetch for whichever tab is showing — it used to
        // refresh just the account header and leave those tables stale.
        if (sections_)
            on_section_changed(sections_->currentIndex(), /*force=*/true);
    });
    hl->addWidget(header_refresh_btn_);
    root->addWidget(bar);
}

void ProfileScreen::build_tab_nav(QVBoxLayout* root) {
    auto* nav = new QWidget(this);
    tab_nav_ = nav;
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

void ProfileScreen::on_section_changed(int index, bool force) {
    sections_->setCurrentIndex(index);
    for (int i = 0; i < nav_buttons_.size(); ++i)
        profile_set_prop(nav_buttons_[i], "active", i == index ? QStringLiteral("true") : QStringLiteral("false"));
    if (index < 1 || index > 3)
        return; // Overview / Support show session data only
    // Flicking between tabs used to re-request the tab's tables every time.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && tab_fetched_ms_[index] > 0 && now - tab_fetched_ms_[index] < kTabFreshMs)
        return;
    tab_fetched_ms_[index] = now; // a failed fetch clears it, so the next visit retries
    if (index == 1)
        fetch_usage_data();
    else if (index == 2)
        fetch_login_history();
    else
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
        eb->setMinimumHeight(26);
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
    eb2->setMinimumHeight(26);
    eb2->setCursor(Qt::PointingHandCursor);
    connect(eb2, &QPushButton::clicked, this, &ProfileScreen::show_edit_profile_dialog);
    arl->addWidget(eb2);
    auto* lb = new QPushButton(tr("LOGOUT"));
    lb->setObjectName(QStringLiteral("profBtnDanger"));
    lb->setProperty("fincept_agent_risk", "account");
    lb->setMinimumHeight(26);
    lb->setCursor(Qt::PointingHandCursor);
    connect(lb, &QPushButton::clicked, this, &ProfileScreen::show_logout_confirm);
    arl->addWidget(lb);
    auto* dab = new QPushButton(tr("DELETE ACCOUNT"));
    dab->setObjectName(QStringLiteral("profBtnDanger"));
    dab->setMinimumHeight(26);
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
    // The 30-day credit burn as bars: a spike or a steady climb shows at a glance.
    usg_daily_chart_ = new ui::BarChart(dp);
    usg_daily_chart_->setMinimumHeight(140);
    usg_daily_chart_->setMaximumHeight(180);
    usg_daily_chart_->set_value_formatter([](double v) { return auth::format_credits(v); });
    usg_daily_chart_->set_state(ui::BarChart::Loading);
    qobject_cast<QVBoxLayout*>(dp->layout())->addWidget(usg_daily_chart_);
    usg_daily_table_ = profile_make_table({tr("DATE"), tr("REQUESTS"), tr("CREDITS")}, 200);
    qobject_cast<QVBoxLayout*>(dp->layout())->addWidget(profile_wrap_table(usg_daily_table_, dp));
    vl->addWidget(dp);
    auto* ep = make_panel(tr("TOP ENDPOINTS"));
    usg_endpoint_table_ = profile_make_table({tr("ENDPOINT"), tr("REQUESTS"), tr("CREDITS"), tr("AVG MS")}, 200);
    // "Which endpoint costs me most": credits, largest first, until the user picks a column.
    usg_endpoint_table_->horizontalHeader()->setSortIndicator(2, Qt::DescendingOrder);
    qobject_cast<QVBoxLayout*>(ep->layout())->addWidget(profile_wrap_table(usg_endpoint_table_, ep));
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
    sb->setMinimumHeight(22);
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
    cb->setMinimumHeight(22);
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
    rg->setProperty("fincept_agent_risk", "account"); // invalidates the API key
    rg->setMinimumHeight(22);
    rg->setCursor(Qt::PointingHandCursor);
    connect(rg, &QPushButton::clicked, this, &ProfileScreen::show_regen_confirm);
    krl->addWidget(rg);
    kvl->addLayout(krl);
    vl->addWidget(kp);
    auto* ssp = make_panel(tr("SECURITY STATUS"));
    auto* ssvl = qobject_cast<QVBoxLayout*>(ssp->layout());
    ssvl->addWidget(make_data_row(tr("EMAIL VERIFIED"), sec_verified_));
    {
        // Email 2FA on / off (POST /user/mfa/enable, /user/mfa/disable) — the
        // status used to be read-only with no way to change it anywhere.
        auto* mfa_row = make_data_row(tr("2FA (MFA)"), sec_mfa_);
        sec_mfa_btn_ = new QPushButton;
        sec_mfa_btn_->setObjectName(QStringLiteral("profBtn"));
        sec_mfa_btn_->setMinimumHeight(22);
        sec_mfa_btn_->setCursor(Qt::PointingHandCursor);
        sec_mfa_btn_->setAccessibleName(tr("Turn two-factor authentication on or off"));
        connect(sec_mfa_btn_, &QPushButton::clicked, this, &ProfileScreen::toggle_mfa);
        if (auto* mfa_hl = qobject_cast<QHBoxLayout*>(mfa_row->layout())) {
            mfa_hl->addSpacing(10);
            mfa_hl->addWidget(sec_mfa_btn_);
        }
        ssvl->addWidget(mfa_row);
    }
    vl->addWidget(ssp);
    auto* hp = make_panel(tr("LOGIN HISTORY"));
    sec_login_hist_ =
        profile_make_table({tr("TIME (LOCAL)"), tr("IP ADDRESS"), tr("DEVICE"), tr("STATUS")}, 200);
    qobject_cast<QVBoxLayout*>(hp->layout())->addWidget(profile_wrap_table(sec_login_hist_, hp));
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
    // What the plan is worth: credits saved (or what Pro would save), a rollover
    // waiting for a renewal, what Pro adds and the price of every category on
    // both plans — the server's own table (PlanAccess). The panel is shown only
    // when the server sent one.
    {
        auto* vp = make_panel(tr("PLAN VALUE"));
        auto* value = new ui::PlanBenefitsView(vp);
        value->set_show_upgrade_button(false); // SUBSCRIPTION above carries the button
        qobject_cast<QVBoxLayout*>(vp->layout())->addWidget(value);
        auto& access = billing::PlanAccess::instance();
        auto sync = [vp]() { vp->setVisible(billing::PlanAccess::instance().has_table()); };
        connect(&access, &billing::PlanAccess::changed, vp, sync);
        sync();
        access.refresh();
        vl->addWidget(vp);
    }
    auto* hp = make_panel(tr("PAYMENT HISTORY"));
    bill_history_ = profile_make_table({tr("DATE"), tr("PLAN"), tr("AMOUNT"), tr("CREDITS"), tr("STATUS")}, 250);
    bill_history_->horizontalHeader()->setSortIndicator(0, Qt::DescendingOrder); // newest payment first
    qobject_cast<QVBoxLayout*>(hp->layout())->addWidget(profile_wrap_table(bill_history_, hp));
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
        b->setMinimumHeight(26);
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
    // Signed out → the sign-in requirement page instead of "—" everywhere.
    apply_auth_gate();
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
    if (sec_mfa_btn_) {
        sec_mfa_btn_->setEnabled(!mfa_request_in_flight_);
        sec_mfa_btn_->setText(mfa_request_in_flight_ ? tr("SAVING…")
                                                     : (s.user_info.mfa_enabled ? tr("DISABLE 2FA") : tr("ENABLE 2FA")));
    }
    bill_plan_->setText(s.plan_label());
    bill_credits_->setText(auth::format_credits(s.user_info.credit_balance));
    // Support type: the subscription record AuthManager holds (GET /cashfree/subscription).
    const QString support = s.subscription.support_type.trimmed();
    bill_support_->setText(support.isEmpty() ? QString::fromUtf8("\xe2\x80\x94") : support.toUpper());

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

    profile_table_loading(usg_daily_table_, tr("Loading daily usage…"));
    profile_table_loading(usg_endpoint_table_, tr("Loading endpoint activity…"));
    // Figures not known yet: skeletons, not "—" (a refetch keeps the shown values).
    const QString dash = QString::fromUtf8("\xe2\x80\x94");
    for (QLabel* v : {usg_rate_, usg_total_req_, usg_cred_used_, usg_avg_cred_, usg_avg_resp_})
        if (v && v->text() == dash)
            profile_stat_loading(v, true);
    const auto usage_figures_done = [](ProfileScreen* s) {
        for (QLabel* v : {s->usg_rate_, s->usg_total_req_, s->usg_cred_used_, s->usg_avg_cred_, s->usg_avg_resp_})
            profile_stat_loading(v, false);
    };

    QPointer<ProfileScreen> self = this;
    const quint64 generation = account_generation_;
    auth::UserApi::instance().get_user_usage(
        30,
        [self, generation, usage_figures_done](auth::ApiResponse r) {
            if (!self || self->account_generation_ != generation)
                return; // gone, or asked for an account that is no longer signed in
            usage_figures_done(self.data()); // filled below, or "—" when the answer has none
            if (!r.success) {
                LOG_WARN("Profile", "Usage fetch failed: " + r.error);
                self->tab_fetched_ms_[1] = 0;
                const QString msg = tr("Usage could not be loaded. Check your connection, then retry.");
                const auto retry = [self]() {
                    if (self)
                        self->on_section_changed(1, /*force=*/true);
                };
                profile_table_state(self->usg_daily_table_,
                                    ui::EmptyState::error_spec(msg, tr("Could not load usage"), tr("RETRY"), r.error),
                                    retry);
                profile_table_state(self->usg_endpoint_table_,
                                    ui::EmptyState::error_spec(msg, tr("Could not load endpoint activity"),
                                                               tr("RETRY"), r.error),
                                    retry);
                if (self->usg_daily_chart_)
                    self->usg_daily_chart_->set_state(ui::BarChart::Error, msg);
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
                // there" — otherwise the loading skeleton never goes away.
                auto d = payload["daily_usage"].toArray();
                // Chart: every day of the window, oldest to newest ("MMM d").
                if (self->usg_daily_chart_) {
                    QStringList cats;
                    QVector<double> credits;
                    for (const auto& dv : d) {
                        const auto e = dv.toObject();
                        const QDate day = QDate::fromString(e["date"].toString().left(10), Qt::ISODate);
                        cats << (day.isValid() ? QLocale().toString(day, QStringLiteral("MMM d")) : e["date"].toString());
                        credits << e["credits_used"].toDouble();
                    }
                    if (cats.isEmpty()) {
                        self->usg_daily_chart_->clear();
                        self->usg_daily_chart_->set_state(ui::BarChart::Empty, tr("No usage recorded in the last 30 days."));
                    } else {
                        self->usg_daily_chart_->set_data(cats, credits, tr("Credits"));
                        self->usg_daily_chart_->set_state(ui::BarChart::Ready);
                    }
                }
                self->usg_daily_table_->clearSpans();
                self->usg_daily_table_->setRowCount(0);
                for (int i = d.size() - 1; i >= 0 && i >= d.size() - 10; i--) {
                    auto e = d[i].toObject();
                    int row = self->usg_daily_table_->rowCount();
                    self->usg_daily_table_->insertRow(row);
                    self->usg_daily_table_->setItem(row, 0, new QTableWidgetItem(e["date"].toString()));
                    self->usg_daily_table_->setItem(row, 1,
                                                    profile_num_item(QString::number(e["request_count"].toInt()),
                                                                     e["request_count"].toDouble()));
                    self->usg_daily_table_->setItem(
                        row, 2,
                        profile_num_item(auth::format_credits(e["credits_used"].toDouble()), e["credits_used"].toDouble()));
                }
                if (self->usg_daily_table_->rowCount() == 0)
                    profile_table_state(self->usg_daily_table_,
                                        ui::EmptyState::no_data_spec(
                                            tr("No usage in the last 30 days"),
                                            tr("Calls to Fincept data and AI services appear here day by day, with "
                                               "their credit cost.")));
                else
                    profile_table_rows(self->usg_daily_table_);
            }
            {
                auto eps = payload["endpoint_breakdown"].toArray();
                self->usg_endpoint_table_->setSortingEnabled(false); // rows are inserted by index
                self->usg_endpoint_table_->clearSpans();
                self->usg_endpoint_table_->setRowCount(0);
                for (const auto& v : eps) {
                    auto e = v.toObject();
                    int row = self->usg_endpoint_table_->rowCount();
                    self->usg_endpoint_table_->insertRow(row);
                    self->usg_endpoint_table_->setItem(row, 0, new QTableWidgetItem(e["endpoint"].toString()));
                    self->usg_endpoint_table_->setItem(row, 1,
                                                       profile_num_item(QString::number(e["request_count"].toInt()),
                                                                        e["request_count"].toDouble()));
                    self->usg_endpoint_table_->setItem(
                        row, 2,
                        profile_num_item(auth::format_credits(e["credits_used"].toDouble()), e["credits_used"].toDouble()));
                    self->usg_endpoint_table_->setItem(
                        row, 3,
                        profile_num_item(QString::number(e["avg_response_time_ms"].toDouble(), 'f', 0),
                                         e["avg_response_time_ms"].toDouble()));
                }
                if (self->usg_endpoint_table_->rowCount() == 0) {
                    profile_table_state(self->usg_endpoint_table_,
                                        ui::EmptyState::no_data_spec(
                                            tr("No endpoint activity"),
                                            tr("The services you call most appear here with their requests, credits "
                                               "and response time.")));
                } else {
                    self->usg_endpoint_table_->setSortingEnabled(true);
                    profile_table_rows(self->usg_endpoint_table_);
                }
            }
        },
        this);
}

void ProfileScreen::fetch_billing_data() {
    LOG_DEBUG("Profile", "Fetching billing data...");
    profile_table_loading(bill_history_, tr("Loading payment history…"));
    // Plan, credits and support type are the subscription record AuthManager
    // already holds (GET /cashfree/subscription at sign-in, after REFRESH and on
    // its background refresh); refresh_all() shows them. This tab used to request
    // that record again on every visit — only the payment list is fetched here.
    QPointer<ProfileScreen> self = this;
    const quint64 generation = account_generation_;
    auth::UserApi::instance().get_payment_history(
        1, 20,
        [self, generation](auth::ApiResponse r) {
            if (!self || self->account_generation_ != generation)
                return; // gone, or asked for an account that is no longer signed in
            if (!r.success) {
                LOG_WARN("Profile", "Payment history failed: " + r.error);
                self->tab_fetched_ms_[3] = 0;
                profile_table_state(self->bill_history_,
                                    ui::EmptyState::error_spec(
                                        tr("Payment history could not be loaded. Check your connection, then retry."),
                                        tr("Could not load payments"), tr("RETRY"), r.error),
                                    [self]() {
                                        if (self)
                                            self->on_section_changed(3, /*force=*/true);
                                    });
                return;
            }
            auto d = r.data.contains("data") ? r.data["data"].toObject() : r.data;
            auto p = d["payments"].toArray();
            self->bill_history_->setSortingEnabled(false); // rows are inserted by index
            self->bill_history_->clearSpans();
            self->bill_history_->setRowCount(0);
            const QString dash = QString::fromUtf8("\xe2\x80\x94");
            for (const auto& v : p) {
                auto e = v.toObject();
                int row = self->bill_history_->rowCount();
                self->bill_history_->insertRow(row);
                // Local date shown; sorted by the instant itself.
                const QString created = e["created_at"].toString();
                const QDateTime created_dt = QDateTime::fromString(created, Qt::ISODate);
                auto* date_item = new ui::NumericTableWidgetItem(
                    profile_local_stamp(created, false),
                    created_dt.isValid() ? double(created_dt.toMSecsSinceEpoch()) : 0.0);
                self->bill_history_->setItem(row, 0, date_item);
                const QString plan = e["plan_name"].toString();
                self->bill_history_->setItem(row, 1, new QTableWidgetItem(plan.isEmpty() ? dash : plan));
                // The field is amount_USD: it was charged in dollars, so pin the symbol
                // instead of showing it in whatever display currency is selected (a
                // $49 payment read "EUR 49.00" for a euro user — no conversion happens).
                self->bill_history_->setItem(
                    row, 2,
                    profile_num_item(cur::money(e["amount_usd"].toDouble(), false, QStringLiteral("USD")),
                                     e["amount_usd"].toDouble()));
                const QJsonValue credits = e["credits_purchased"];
                const bool no_credits = credits.isNull() || credits.isUndefined();
                self->bill_history_->setItem(
                    row, 3,
                    profile_num_item(no_credits ? dash : auth::format_credits(credits.toDouble()),
                                     no_credits ? -1.0 : credits.toDouble()));
                self->bill_history_->setItem(row, 4, new QTableWidgetItem(e["status"].toString().toUpper()));
            }
            if (self->bill_history_->rowCount() == 0) {
                profile_table_state(self->bill_history_,
                                    ui::EmptyState::no_data_spec(tr("No payments yet"),
                                                                 tr("Plan and credit-pack payments appear here with "
                                                                    "their amount, credits and status."),
                                                                 tr("VIEW PLANS")),
                                    [self]() {
                                        if (self)
                                            ui::billing::open_plans(self.data());
                                    });
            } else {
                self->bill_history_->setSortingEnabled(true);
                profile_table_rows(self->bill_history_);
            }
            const int total = d["pagination"].toObject()["total_count"].toInt();
            self->bill_history_->setToolTip(total > p.size() ? tr("Showing the latest %1 of %2 payments.")
                                                                    .arg(p.size())
                                                                    .arg(total)
                                                              : QString());
        },
        this);
}

void ProfileScreen::fetch_login_history() {
    LOG_DEBUG("Profile", "Fetching login history...");
    profile_table_loading(sec_login_hist_, tr("Loading sign-in history…"));
    QPointer<ProfileScreen> self = this;
    const quint64 generation = account_generation_;
    auth::UserApi::instance().get_login_history(
        20, 0,
        [self, generation](auth::ApiResponse r) {
            if (!self || self->account_generation_ != generation)
                return; // gone, or asked for an account that is no longer signed in
            if (!r.success) {
                LOG_WARN("Profile", "Login history failed: " + r.error);
                self->tab_fetched_ms_[2] = 0;
                profile_table_state(self->sec_login_hist_,
                                    ui::EmptyState::error_spec(
                                        tr("Sign-in history could not be loaded. Check your connection, then retry."),
                                        tr("Could not load sign-in history"), tr("RETRY"), r.error),
                                    [self]() {
                                        if (self)
                                            self->on_section_changed(2, /*force=*/true);
                                    });
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
                if (failed) {
                    st_item->setForeground(QColor(ui::colors::NEGATIVE()));
                    st_item->setData(Qt::UserRole, true); // apply_theme() re-colours it
                }
                self->sec_login_hist_->setItem(row, 3, st_item);
            }
            if (self->sec_login_hist_->rowCount() == 0)
                profile_table_state(self->sec_login_hist_,
                                    ui::EmptyState::no_data_spec(tr("No sign-ins recorded"),
                                                                 tr("Each sign-in to your account appears here with "
                                                                    "its time, IP address and device.")));
            else
                profile_table_rows(self->sec_login_hist_);
        },
        this);
}

void ProfileScreen::show_edit_profile_dialog() {
    auto* dlg = new QDialog(this);
    dlg->setObjectName(QStringLiteral("profDialog"));
    dlg->setWindowTitle(tr("Edit Profile"));
    dlg->setMinimumSize(420, 300);
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
        i->setMinimumHeight(28);
        vl->addWidget(i);
        return i;
    };
    auto* un = add_f(tr("USERNAME"), s.user_info.username);
    auto* ph = add_f(tr("PHONE (with country code)"), s.user_info.phone);
    auto* co = add_f(tr("COUNTRY"), s.user_info.country);
    // What the dialog opened with: only fields the user actually changed are
    // sent. Re-sending the pre-filled phone ("+<cc><digits>", as registration
    // stores it) made every save fail with "Phone must contain only digits".
    const QString orig_username = s.user_info.username.trimmed();
    const QString orig_phone_digits = profile_phone_digits(s.user_info.phone);
    const QString orig_country = s.user_info.country.trimmed();
    auto* brl = new QHBoxLayout;
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(8);
    brl->addStretch();
    auto* cn = new QPushButton(tr("CANCEL"));
    cn->setObjectName(QStringLiteral("profBtn"));
    cn->setMinimumHeight(26);
    connect(cn, &QPushButton::clicked, dlg, &QDialog::reject);
    brl->addWidget(cn);
    auto* sv = new QPushButton(tr("SAVE"));
    sv->setObjectName(QStringLiteral("profBtnPrimary"));
    sv->setMinimumHeight(26);
    sv->setDefault(true);
    QPointer<ProfileScreen> self = this;
    QPointer<QDialog> dlg_ptr = dlg;
    connect(sv, &QPushButton::clicked, this, [=]() {
        QJsonObject data;
        const QString username = un->text().trimmed();
        if (!username.isEmpty() && username != orig_username)
            data["username"] = username;
        // Compared as digits, so "+91 98765-43210" counts as unchanged when
        // "+919876543210" is stored. A changed number is sent as PUT
        // /user/profile reads it: "+<digits>" is a full international number
        // (stored as is); bare digits are a NATIONAL number the server prefixes
        // with the account's country code. Sending a typed "+65 9876 5432" as
        // bare "6598765432" used to store "+656598765432".
        const QString phone_text = ph->text().trimmed();
        if (!phone_text.isEmpty()) {
            const QString phone_digits = profile_phone_digits(phone_text);
            if (phone_digits.isEmpty()) {
                QMessageBox::warning(dlg_ptr ? static_cast<QWidget*>(dlg_ptr) : nullptr, tr("Edit Profile"),
                                     tr("Phone number may contain only digits, spaces, dashes and a leading +. "
                                        "Start with + and the country code (e.g. +91 98765 43210); a number "
                                        "without + is taken as a national number in your account's country."));
                return;
            }
            if (phone_digits != orig_phone_digits) {
                QString phone_out = phone_digits;
                if (phone_text.startsWith(QLatin1Char('+')))
                    phone_out.prepend(QLatin1Char('+'));
                data["phone"] = phone_out;
            }
        }
        const QString country = co->text().trimmed();
        if (!country.isEmpty() && country != orig_country)
            data["country"] = country;
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
                auth::AuthManager::instance().refresh_user_data(auth::AuthManager::Refresh::Now);
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
    // AuthManager adopts the returned key everywhere the old one lived (session,
    // HttpClient, secure storage) before refreshing — refreshing with the old
    // key, which the server has already invalidated, 401'd and signed the user
    // out. It does so even if this screen is gone by the time the reply lands.
    auth::AuthManager::instance().regenerate_api_key([self](bool ok, const QString& error) {
        // Guarded: regeneration is a network round trip and the screen can be
        // rebuilt (language change) or destroyed while it is in flight.
        if (!self)
            return;
        if (ok) {
            self->api_key_visible_ = false;
            if (self->sec_api_key_)
                self->sec_api_key_->setText(QString(20, QChar(0x2022)));
            LOG_INFO("Profile", "API key regenerated");
            return;
        }
        // Previously a silent no-op — the user could not tell whether their
        // key had been rotated or not.
        LOG_ERROR("Profile", "API key regeneration failed: " + error);
        QMessageBox::warning(self, tr("Regeneration Failed"),
                             error.isEmpty() ? tr("Could not regenerate your API key. Your existing key is "
                                                  "still valid.")
                                             : error);
    });
}

void ProfileScreen::toggle_mfa() {
    if (mfa_request_in_flight_)
        return;
    const auto& s = auth::AuthManager::instance().session();
    const bool enabled = s.user_info.mfa_enabled;
    QString password;
    if (!enabled) {
        if (QMessageBox::question(this, tr("Enable Two-Factor Authentication"),
                                  tr("Every sign-in will then also ask for a 6-digit code that Fincept emails to "
                                     "%1.\n\nTurn on two-factor authentication?")
                                      .arg(s.user_info.email),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    } else {
        // The API refuses to turn 2FA off without the account password.
        bool ok = false;
        password = QInputDialog::getText(this, tr("Disable Two-Factor Authentication"),
                                         tr("Enter your account password to turn off two-factor authentication:"),
                                         QLineEdit::Password, QString(), &ok);
        if (!ok)
            return;
        if (password.isEmpty()) {
            QMessageBox::warning(this, tr("Disable Two-Factor Authentication"),
                                 tr("Your password is required to turn off two-factor authentication."));
            return;
        }
    }

    mfa_request_in_flight_ = true;
    if (sec_mfa_btn_) {
        sec_mfa_btn_->setEnabled(false);
        sec_mfa_btn_->setText(tr("SAVING…"));
    }
    QPointer<ProfileScreen> self = this;
    auto done = [self, enabled](auth::ApiResponse r) {
        if (!self)
            return;
        self->mfa_request_in_flight_ = false;
        if (r.success) {
            LOG_INFO("Profile", enabled ? "Two-factor authentication turned off" : "Two-factor authentication turned on");
            // The row and the button follow when the refreshed profile lands
            // (auth_state_changed → refresh_all()).
            auth::AuthManager::instance().refresh_user_data(auth::AuthManager::Refresh::Now);
            return;
        }
        self->refresh_all(); // puts the button back
        LOG_WARN("Profile", "2FA change failed: " + r.error);
        // The server's own reason — "Invalid password", or for an account that
        // signed up with Google and never set a password, the same refusal.
        QMessageBox::warning(self, enabled ? tr("Disable 2FA Failed") : tr("Enable 2FA Failed"),
                             r.error.isEmpty() ? tr("Could not change two-factor authentication. Please try again.")
                                               : r.error);
    };
    if (enabled)
        auth::UserApi::instance().disable_mfa(password, done, this);
    else
        auth::UserApi::instance().enable_mfa(done, this);
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
    input->setMinimumHeight(28);
    input->setPlaceholderText(email);
    vl->addWidget(input);

    auto* pw_lbl = new QLabel(tr("ENTER YOUR PASSWORD:"));
    pw_lbl->setObjectName(QStringLiteral("profDangerLabel"));
    vl->addWidget(pw_lbl);

    auto* pw_input = new QLineEdit;
    pw_input->setMinimumHeight(28);
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
    code_input->setMinimumHeight(28);
    code_input->setPlaceholderText(tr("Verification code"));
    rl->addWidget(code_input);

    auto* new_pw_lbl = new QLabel(tr("NEW PASSWORD (8+ CHARACTERS WITH UPPER, LOWER CASE AND A DIGIT):"));
    new_pw_lbl->setWordWrap(true);
    new_pw_lbl->setObjectName(QStringLiteral("profDangerLabel"));
    rl->addWidget(new_pw_lbl);

    auto* new_pw = new QLineEdit;
    new_pw->setMinimumHeight(28);
    new_pw->setEchoMode(QLineEdit::Password);
    new_pw->setPlaceholderText(tr("New password"));
    rl->addWidget(new_pw);

    auto* confirm_pw = new QLineEdit;
    confirm_pw->setMinimumHeight(28);
    confirm_pw->setEchoMode(QLineEdit::Password);
    confirm_pw->setPlaceholderText(tr("Confirm new password"));
    rl->addWidget(confirm_pw);

    auto* set_pw_btn = new QPushButton(tr("SET PASSWORD"));
    set_pw_btn->setObjectName(QStringLiteral("profBtnPrimary"));
    set_pw_btn->setProperty("fincept_agent_risk", "account");
    set_pw_btn->setMinimumHeight(26);
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
    cancel->setMinimumHeight(26);
    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);
    brl->addWidget(cancel);

    auto* confirm = new QPushButton(tr("DELETE MY ACCOUNT"));
    confirm->setObjectName(QStringLiteral("profBtnDangerSolid"));
    confirm->setProperty("fincept_agent_risk", "account");
    confirm->setMinimumHeight(26);
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
        // The server's rule: 8+ characters with an upper-case letter, a
        // lower-case letter and a digit (length alone let "password1" through).
        if (!auth::validate_password(new_pw->text()).valid) {
            say(tr("Password needs 8+ characters with upper and lower case letters and a digit"), "neg");
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
            if (r.status_code == 401) {
                // The server's own reason: "Invalid password" or "Email
                // confirmation does not match this account". A session made
                // stale by a password reset no longer shows up here —
                // HttpClient drops the dead token and re-sends the request with
                // the API key alone — so this is not "session expired".
                QMessageBox::warning(self, tr("Delete Failed"),
                                     tr("Account deletion failed: %1\n\nCheck what you entered and try again.")
                                         .arg(r.error.isEmpty() ? tr("the confirmation was not accepted.") : r.error));
                return;
            }
            QMessageBox::critical(self, tr("Delete Failed"),
                                  tr("Account deletion failed: %1\n\nPlease contact support@fincept.in").arg(r.error));
        });
    });
    brl->addWidget(confirm);
    vl->addLayout(brl);

    dlg->exec();
    dlg->deleteLater();
}

namespace {
/// Section id of each tab button, in nav order.
const char* const kProfSectionIds[] = {"overview", "usage", "security", "billing", "support"};
} // namespace

bool ProfileScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus);
    const QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true;
    for (int i = 0; i < int(std::size(kProfSectionIds)); ++i) {
        if (key != QLatin1String(kProfSectionIds[i]))
            continue;
        if (!sections_ || i >= sections_->count())
            return false;
        if (account_key_.isEmpty())
            sections_->setCurrentIndex(i); // signed out: the sign-in gate opens this tab (and loads it)
        else
            on_section_changed(i); // the tab button's click (loads its data unless fresh)
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> prof_command_rows() {
    namespace rows = command_rows;
    const QString where = ProfileScreen::tr("Profile");
    const QString screen = QStringLiteral("profile");
    return {
        rows::section(screen, QStringLiteral("overview"), ProfileScreen::tr("Account overview"),
                      {QStringLiteral("account"), QStringLiteral("profile"), QStringLiteral("my account"),
                       QStringLiteral("username"), QStringLiteral("email")},
                      where),
        rows::section(screen, QStringLiteral("usage"), ProfileScreen::tr("Usage & credits"),
                      {QStringLiteral("usage"), QStringLiteral("credits used"), QStringLiteral("api calls"),
                       QStringLiteral("requests"), QStringLiteral("rate limit")},
                      where),
        rows::section(screen, QStringLiteral("security"), ProfileScreen::tr("Account security"),
                      {QStringLiteral("security"), QStringLiteral("password"), QStringLiteral("2fa"),
                       QStringLiteral("two factor"), QStringLiteral("api key"), QStringLiteral("login history")},
                      where),
        rows::section(screen, QStringLiteral("billing"), ProfileScreen::tr("Billing"),
                      {QStringLiteral("billing"), QStringLiteral("invoices"), QStringLiteral("plan"),
                       QStringLiteral("subscription"), QStringLiteral("payments")},
                      where),
        rows::section(screen, QStringLiteral("support"), ProfileScreen::tr("Account support"),
                      {QStringLiteral("help"), QStringLiteral("contact"), QStringLiteral("support")}, where),
    };
}

void prof_install_commands() {
    command_rows::install(QStringLiteral("profile"), &prof_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(prof_install_commands)

} // namespace fincept::screens
