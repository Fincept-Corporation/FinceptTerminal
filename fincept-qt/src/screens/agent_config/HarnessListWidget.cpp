// HarnessListWidget.cpp — the harness table + details (see the header).

#include "screens/agent_config/HarnessListWidget.h"

#include "core/process/HostExec.h"
#include "screens/agent_config/AgentRunCommon.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ModelBadge.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace harness_list_detail {

QLabel* hlw_label(const char* object_name, QWidget* parent, bool wrap = false) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    l->setWordWrap(wrap);
    return l;
}

QPushButton* hlw_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

/// The widget's one stylesheet: object-name rules under #hlwRoot only (it is
/// embedded in two hosts with different sheets), token values of the moment —
/// ThemeManager rewrites them on a theme or font change.
QString hlw_stylesheet() {
    using namespace ui::colors;
    return QString(
               "#hlwRoot { background:%1; }"
               "#hlwRoot QLabel, #hlwRoot QCheckBox { background:transparent; }"
               "#hlwRoot QLabel#hlwTitle { color:%2; font-weight:700; letter-spacing:1px; }"
               "#hlwRoot QLabel#hlwName { color:%3; font-weight:700; font-size:" + ui::fonts::px(1) + "px; }"
               "#hlwRoot QLabel#hlwLabel { color:%4; font-weight:700; letter-spacing:1px; }"
               "#hlwRoot QLabel#hlwHint { color:%5; }"
               "#hlwRoot QFrame#hlwCard { background:%6; border:1px solid %7; }"
               "#hlwRoot QCheckBox { color:%3; spacing:6px; }"
               "#hlwRoot QLineEdit, #hlwRoot QComboBox { background:%8; color:%3; border:1px solid %9; padding:3px 6px; }"
               "#hlwRoot QComboBox QAbstractItemView { background:%8; color:%3; selection-background-color:%10; }"
               "#hlwRoot QPushButton#hlwBtn { background:%8; color:%3; border:1px solid %9; padding:4px 12px; "
               "font-weight:700; letter-spacing:1px; }"
               "#hlwRoot QPushButton#hlwBtn:hover { color:%2; border-color:%2; }"
               "#hlwRoot QPushButton#hlwBtn:disabled { color:%11; border-color:%7; }"
               "#hlwRoot QTableView { background:%1; alternate-background-color:%12; color:%3; border:1px solid %7; "
               "gridline-color:%7; selection-background-color:%13; selection-color:%3; }"
               "#hlwRoot QHeaderView::section { background:%6; color:%4; border:none; border-right:1px solid %7; "
               "border-bottom:1px solid %7; padding:4px 6px; font-weight:700; letter-spacing:1px; }"
               // The table's check marks (TERMINAL AI, DEFAULT) and the check boxes: visible in both themes.
               "#hlwRoot QTableView::indicator, #hlwRoot QCheckBox::indicator { width:0.8em; height:0.8em; "
               "border:1px solid %9; background:%8; }"
               "#hlwRoot QTableView::indicator:checked, #hlwRoot QCheckBox::indicator:checked { background:%2; "
               "border-color:%2; }")
        .arg(BG_BASE(), AMBER(), TEXT_PRIMARY(), TEXT_SECONDARY(), TEXT_TERTIARY(), BG_SURFACE(), BORDER_DIM(),
             BG_RAISED(), BORDER_MED())
        .arg(ACCENT_BG(), TEXT_DIM(), ROW_ALT(), BG_HOVER());
}

} // namespace harness_list_detail

using namespace harness_list_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Model
// ═════════════════════════════════════════════════════════════════════════════

HarnessTableModel::HarnessTableModel(QObject* parent) : QAbstractTableModel(parent) {
    ids_ = harness::HarnessRegistry::instance().ids();
}

int HarnessTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(ids_.size());
}

int HarnessTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant HarnessTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColState:
            return tr("STATE");
        case ColName:
            return tr("HARNESS");
        case ColVersion:
            return tr("VERSION");
        case ColSignIn:
            return tr("SIGN-IN");
        case ColModel:
            return tr("DEFAULT MODEL");
        case ColEffort:
            return tr("EFFORT");
        case ColTerminal:
            return tr("TERMINAL AI");
        case ColDefault:
            return tr("DEFAULT");
        case ColPath:
            return tr("PATH");
        default:
            return {};
    }
}

QString HarnessTableModel::id_at(int row) const {
    return (row >= 0 && row < ids_.size()) ? ids_.at(row) : QString();
}

int HarnessTableModel::row_of(const QString& id) const {
    return static_cast<int>(ids_.indexOf(id));
}

QVariant HarnessTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= ids_.size())
        return {};
    auto& registry = harness::HarnessRegistry::instance();
    const QString& id = ids_[index.row()];
    if (role == kIdRole)
        return id;
    const harness::IHarnessAdapter* adapter = registry.adapter(id);
    if (!adapter)
        return {};
    const harness::HarnessDescriptor& d = adapter->descriptor();
    const harness::HarnessStatus st = registry.status(id);
    const int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
            case ColState:
                return QStringLiteral("● ") + awb_harness_state(id).word;
            case ColName:
                return d.beta ? QStringLiteral("%1  ·  %2").arg(d.display_name, tr("BETA")) : d.display_name;
            case ColVersion:
                return st.installation ? (st.installation->version.isEmpty() ? tr("unknown") : st.installation->version)
                                       : QStringLiteral("—");
            case ColSignIn: {
                if (!st.installation)
                    return QStringLiteral("—");
                QString text = awb_auth_text(st.auth);
                if (st.auth.api_key_env_present && registry.api_key_reaches_child(id) && !d.api_key_only)
                    text += QStringLiteral("  ·  ") + tr("API KEY IN ENV");
                return text;
            }
            case ColModel: {
                const QString m = registry.default_model(id);
                return m.isEmpty() ? tr("CLI default") : m;
            }
            case ColEffort: {
                if (!d.caps.effort)
                    return QStringLiteral("—");
                const QString e = registry.default_effort(id);
                return e.isEmpty() ? tr("default") : e;
            }
            case ColPath:
                return st.installation ? QDir::toNativeSeparators(st.installation->path) : QString();
            default:
                return {};
        }
    }
    if (role == Qt::CheckStateRole) {
        if (col == ColTerminal)
            return registry.enabled_for_terminal(id) ? Qt::Checked : Qt::Unchecked;
        if (col == ColDefault)
            return registry.default_harness() == id ? Qt::Checked : Qt::Unchecked;
    }
    if (role == Qt::ForegroundRole) {
        if (col == ColState)
            return QColor(awb_tone_color(awb_harness_state(id).tone));
        if (col == ColSignIn && st.auth.api_key_env_present && registry.api_key_reaches_child(id) && !d.api_key_only)
            return QColor(ui::colors::WARNING());
        if (col == ColName)
            return QColor(st.installation ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY());
        return QColor(ui::colors::TEXT_SECONDARY());
    }
    if (role == Qt::ToolTipRole) {
        if (col == ColState || col == ColName)
            return awb_harness_state(id).sentence;
        if (col == ColTerminal)
            return tr("Use for terminal AI: AI Chat, Quick Chat, Quick Ask and the workflow tool picker answer with this "
                      "harness by default while it is signed in with an API key (never a personal subscription). "
                      "Switching it on the first time opens a one-time consent naming the vendor that receives your "
                      "data.");
        if (col == ColDefault)
            return tr("The harness new runs and schedules start with");
        if (col == ColPath && st.installation)
            return QDir::toNativeSeparators(st.installation->path);
        if (col == ColSignIn && !st.auth.note.isEmpty())
            return st.auth.note;
    }
    return {};
}

Qt::ItemFlags HarnessTableModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.isValid() && (index.column() == ColTerminal || index.column() == ColDefault))
        f |= Qt::ItemIsUserCheckable;
    return f;
}

bool HarnessTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::CheckStateRole)
        return false;
    auto& registry = harness::HarnessRegistry::instance();
    const QString id = id_at(index.row());
    const bool on = value.toInt() == Qt::Checked;
    if (index.column() == ColTerminal) {
        registry.set_enabled_for_terminal(id, on); // defaults_changed → every view refreshes
        return true;
    }
    if (index.column() == ColDefault) {
        registry.set_default_harness(on ? id : QString());
        return true;
    }
    return false;
}

void HarnessTableModel::refresh_row(const QString& id) {
    const int row = row_of(id);
    if (row >= 0)
        emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void HarnessTableModel::refresh_all() {
    if (!ids_.isEmpty())
        emit dataChanged(index(0, 0), index(static_cast<int>(ids_.size()) - 1, ColCount - 1));
}

void HarnessTableModel::retranslate_headers() {
    emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// Widget
// ═════════════════════════════════════════════════════════════════════════════

HarnessListWidget::HarnessListWidget(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("hlwRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(hlw_stylesheet()); // once, before any child exists
    // "Use for terminal AI" without consent only emits terminal_consent_requested:
    // make sure the dialog handler exists even before any model badge was built
    // (otherwise the switch would silently flip back). Idempotent.
    ui::install_harness_consent_handler();
    build();
    retranslate();

    auto& registry = harness::HarnessRegistry::instance();
    connect(&registry, &harness::HarnessRegistry::status_changed, this, [this](const QString& id) {
        model_->refresh_row(id);
        refresh_summary();
        if (id == shown_id_)
            refresh_detail();
    });
    connect(&registry, &harness::HarnessRegistry::defaults_changed, this, [this]() {
        model_->refresh_all();
        {
            const QSignalBlocker b(full_access_check_);
            full_access_check_->setChecked(harness::HarnessRegistry::instance().allow_full_access());
        }
        refresh_detail();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        fit_table();
        refresh_detail(); // rich text with token colours
    });

    fit_table();
    refresh_summary();
    const QString first = registry.preferred_harness();
    select_harness(first.isEmpty() ? model_->id_at(0) : first);
}

void HarnessListWidget::build() {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 12);
    v->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    title_ = hlw_label("hlwTitle", this);
    head->addWidget(title_);
    summary_ = hlw_label("hlwHint", this);
    summary_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    head->addWidget(summary_, 1);
    full_access_check_ = new QCheckBox(this);
    full_access_check_->setChecked(harness::HarnessRegistry::instance().allow_full_access());
    // agents: lets runs choose "Full access" (no sandbox, no approvals) — a permission change
    full_access_check_->setProperty("fincept_agent_risk", "account");
    connect(full_access_check_, &QCheckBox::toggled, this, &HarnessListWidget::on_full_access_toggled);
    head->addWidget(full_access_check_);
    redetect_all_btn_ = hlw_button("hlwBtn", this);
    connect(redetect_all_btn_, &QPushButton::clicked, this,
            []() { harness::HarnessRegistry::instance().refresh({}, harness::RefreshScope::All); });
    head->addWidget(redetect_all_btn_);
    v->addLayout(head);

    model_ = new HarnessTableModel(this);
    table_ = new QTableView(this);
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->verticalHeader()->hide();
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(HarnessTableModel::ColPath, QHeaderView::Stretch);
    hh->setStretchLastSection(true);
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                shown_id_ = model_->id_at(cur.row());
                refresh_detail();
            });
    v->addWidget(table_);

    // ── Details ──────────────────────────────────────────────────────────────
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("hlwCard"));
    auto* cv = new QVBoxLayout(card);
    cv->setContentsMargins(12, 10, 12, 12);
    cv->setSpacing(8);
    name_lbl_ = hlw_label("hlwName", card);
    cv->addWidget(name_lbl_);
    state_lbl_ = hlw_label("hlwHint", card, true);
    state_lbl_->setTextFormat(Qt::RichText);
    state_lbl_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    state_lbl_->setOpenExternalLinks(false);
    connect(state_lbl_, &QLabel::linkActivated, this, &HarnessListWidget::on_link);
    cv->addWidget(state_lbl_);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    int r = 0;
    model_lbl_ = hlw_label("hlwLabel", card);
    model_combo_ = new QComboBox(card);
    model_combo_->setEditable(true);
    model_combo_->setInsertPolicy(QComboBox::NoInsert);
    model_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    model_combo_->setMinimumContentsLength(14);
    connect(model_combo_, &QComboBox::activated, this, [this](int) { on_model_committed(); });
    connect(model_combo_->lineEdit(), &QLineEdit::editingFinished, this, &HarnessListWidget::on_model_committed);
    grid->addWidget(model_lbl_, r, 0);
    grid->addWidget(model_combo_, r++, 1);
    effort_lbl_ = hlw_label("hlwLabel", card);
    effort_combo_ = new QComboBox(card);
    effort_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    effort_combo_->setMinimumContentsLength(10);
    connect(effort_combo_, &QComboBox::activated, this, [this](int index) {
        if (!shown_id_.isEmpty())
            harness::HarnessRegistry::instance().set_default_effort(shown_id_, effort_combo_->itemData(index).toString());
    });
    grid->addWidget(effort_lbl_, r, 0);
    grid->addWidget(effort_combo_, r++, 1);
    keys_lbl_ = hlw_label("hlwLabel", card);
    policy_combo_ = new QComboBox(card);
    policy_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    policy_combo_->setMinimumContentsLength(14);
    policy_combo_->addItem(QString(), QString::fromLatin1(harness::HarnessRegistry::kEnvPolicyInherit));
    policy_combo_->addItem(QString(), QString::fromLatin1(harness::HarnessRegistry::kEnvPolicySubscriptionOnly));
    connect(policy_combo_, &QComboBox::activated, this, [this](int index) {
        if (!shown_id_.isEmpty())
            harness::HarnessRegistry::instance().set_env_policy(shown_id_, policy_combo_->itemData(index).toString());
    });
    grid->addWidget(keys_lbl_, r, 0);
    grid->addWidget(policy_combo_, r++, 1);
    keys_note_ = hlw_label("hlwHint", card, true);
    keys_note_->setTextFormat(Qt::RichText);
    grid->addWidget(keys_note_, r++, 1);
    path_lbl_ = hlw_label("hlwLabel", card);
    auto* path_row = new QHBoxLayout;
    path_row->setSpacing(4);
    path_edit_ = new QLineEdit(card);
    path_edit_->setClearButtonEnabled(true);
    connect(path_edit_, &QLineEdit::returnPressed, this, &HarnessListWidget::on_apply_path);
    path_row->addWidget(path_edit_, 1);
    browse_btn_ = hlw_button("hlwBtn", card);
    connect(browse_btn_, &QPushButton::clicked, this, &HarnessListWidget::on_browse_path);
    path_row->addWidget(browse_btn_);
    apply_path_btn_ = hlw_button("hlwBtn", card);
    apply_path_btn_->setProperty("fincept_agent_risk", "account"); // agents: sets which program the agents run as
    connect(apply_path_btn_, &QPushButton::clicked, this, &HarnessListWidget::on_apply_path);
    path_row->addWidget(apply_path_btn_);
    grid->addWidget(path_lbl_, r, 0);
    grid->addLayout(path_row, r++, 1);
    terminal_check_ = new QCheckBox(card);
    connect(terminal_check_, &QCheckBox::toggled, this, [this](bool on) {
        if (!shown_id_.isEmpty())
            harness::HarnessRegistry::instance().set_enabled_for_terminal(shown_id_, on);
    });
    grid->addWidget(terminal_check_, r++, 1);
    terminal_hint_ = hlw_label("hlwHint", card, true);
    grid->addWidget(terminal_hint_, r++, 1);
    default_check_ = new QCheckBox(card);
    connect(default_check_, &QCheckBox::toggled, this, [this](bool on) {
        if (!shown_id_.isEmpty())
            harness::HarnessRegistry::instance().set_default_harness(on ? shown_id_ : QString());
    });
    grid->addWidget(default_check_, r++, 1);
    grid->setColumnStretch(1, 1);
    cv->addLayout(grid);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    redetect_btn_ = hlw_button("hlwBtn", card);
    connect(redetect_btn_, &QPushButton::clicked, this, [this]() {
        if (!shown_id_.isEmpty())
            harness::HarnessRegistry::instance().refresh(shown_id_, harness::RefreshScope::All);
    });
    install_btn_ = hlw_button("hlwBtn", card);
    install_btn_->setProperty("fincept_agent_risk", "code"); // agents: installs a coding-agent CLI on this computer
    connect(install_btn_, &QPushButton::clicked, this, [this]() {
        if (const auto* a = harness::HarnessRegistry::instance().adapter(shown_id_); a && !a->descriptor().install_url.isEmpty())
            QDesktopServices::openUrl(QUrl(a->descriptor().install_url));
    });
    signin_btn_ = hlw_button("hlwBtn", card);
    connect(signin_btn_, &QPushButton::clicked, this, [this]() {
        if (const auto* a = harness::HarnessRegistry::instance().adapter(shown_id_)) {
            const QString url = a->descriptor().auth_docs_url.isEmpty() ? a->descriptor().docs_url : a->descriptor().auth_docs_url;
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        }
    });
    actions->addWidget(redetect_btn_);
    actions->addWidget(install_btn_);
    actions->addWidget(signin_btn_);
    actions->addStretch(1);
    cv->addLayout(actions);
    notes_lbl_ = hlw_label("hlwHint", card, true);
    notes_lbl_->setTextFormat(Qt::RichText);
    notes_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cv->addWidget(notes_lbl_);
    v->addWidget(card);
    v->addStretch(1);
}

void HarnessListWidget::retranslate() {
    title_->setText(tr("AGENT HARNESSES"));
    full_access_check_->setText(tr("Allow full-access runs"));
    full_access_check_->setToolTip(tr("Offer “Full access” (no sandbox, no approvals) in the run options. Off by default."));
    redetect_all_btn_->setText(tr("RE-DETECT ALL"));
    model_lbl_->setText(tr("DEFAULT MODEL"));
    effort_lbl_->setText(tr("DEFAULT EFFORT"));
    keys_lbl_->setText(tr("API KEYS"));
    policy_combo_->setItemText(0, tr("Pass API keys from the environment (billed to the key)"));
    policy_combo_->setItemText(1, tr("Subscription sign-in only (strip the API-key variables)"));
    path_lbl_->setText(tr("PATH"));
    browse_btn_->setText(tr("BROWSE"));
    apply_path_btn_->setText(tr("APPLY"));
    terminal_check_->setText(tr("Use for terminal AI"));
    terminal_hint_->setText(tr("AI Chat, Quick Chat and Quick Ask use this harness while it signs in with an API key. "
                               "The first time, a one-time consent names the vendor that receives your data."));
    default_check_->setText(tr("Default harness"));
    default_check_->setToolTip(tr("New runs and schedules start with this harness"));
    redetect_btn_->setText(tr("RE-DETECT"));
    install_btn_->setText(tr("INSTALL GUIDE"));
    signin_btn_->setText(tr("HOW TO SIGN IN"));
    model_->retranslate_headers();
    refresh_summary();
    refresh_detail();
}

void HarnessListWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void HarnessListWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Stale sign-in state (> 10 min) or model lists (> 24 h / other version) are
    // re-checked whenever harness state is on screen; fresh ones are not.
    harness::HarnessRegistry::instance().revalidate();
    refresh_summary();
    refresh_detail();
}

void HarnessListWidget::fit_table() {
    // Row height and the table's own height from the font: every harness visible
    // at once, no inner scroll bar (the host scrolls).
    const int row_h = qMax(24, QFontMetrics(ui::fonts::font()).height() + 10);
    table_->verticalHeader()->setDefaultSectionSize(row_h);
    const int header_h = table_->horizontalHeader()->sizeHint().height();
    table_->setFixedHeight(header_h + row_h * model_->rowCount() + 2 * table_->frameWidth() + 2);
    const auto col_w = [](int w) { return qMax(w, w * ui::fonts::font_px(0) / ui::ThemeManager::kDefaultFontPx); };
    auto* hh = table_->horizontalHeader();
    hh->resizeSection(HarnessTableModel::ColState, col_w(150));
    hh->resizeSection(HarnessTableModel::ColName, col_w(200));
    hh->resizeSection(HarnessTableModel::ColVersion, col_w(80));
    hh->resizeSection(HarnessTableModel::ColSignIn, col_w(220));
    hh->resizeSection(HarnessTableModel::ColModel, col_w(150));
    hh->resizeSection(HarnessTableModel::ColEffort, col_w(80));
    hh->resizeSection(HarnessTableModel::ColTerminal, col_w(130));
    hh->resizeSection(HarnessTableModel::ColDefault, col_w(90));
}

void HarnessListWidget::refresh_summary() {
    auto& registry = harness::HarnessRegistry::instance();
    const QStringList installed = registry.installed_ids();
    int signed_in = 0;
    for (const QString& id : installed) {
        if (registry.status(id).auth.state == harness::AuthState::SignedIn)
            ++signed_in;
    }
    QString text = installed.isEmpty()
                       ? tr("No harness installed yet — pick one below for its install guide.")
                       : tr("%1 of %2 installed · %3 signed in").arg(installed.size()).arg(registry.ids().size()).arg(signed_in);
    const QString def = registry.default_harness();
    if (!def.isEmpty())
        text += QStringLiteral(" · ") + tr("default: %1").arg(registry.display_name(def));
    summary_->setText(text);
    summary_->setToolTip(text);
}

QString HarnessListWidget::selected_id() const {
    return shown_id_;
}

void HarnessListWidget::select_harness(const QString& id) {
    const int row = model_->row_of(id);
    if (row < 0)
        return;
    table_->setCurrentIndex(model_->index(row, HarnessTableModel::ColName));
    shown_id_ = id;
    refresh_detail();
}

void HarnessListWidget::refresh_detail() {
    auto& registry = harness::HarnessRegistry::instance();
    const harness::IHarnessAdapter* adapter = registry.adapter(shown_id_);
    const bool have = adapter != nullptr;
    for (QWidget* w : {static_cast<QWidget*>(model_combo_), static_cast<QWidget*>(effort_combo_),
                       static_cast<QWidget*>(policy_combo_), static_cast<QWidget*>(path_edit_),
                       static_cast<QWidget*>(browse_btn_), static_cast<QWidget*>(apply_path_btn_),
                       static_cast<QWidget*>(terminal_check_), static_cast<QWidget*>(default_check_),
                       static_cast<QWidget*>(redetect_btn_)})
        w->setEnabled(have);
    if (!have) {
        name_lbl_->setText(tr("Select a harness"));
        state_lbl_->clear();
        notes_lbl_->clear();
        return;
    }
    const harness::HarnessDescriptor& d = adapter->descriptor();
    const harness::HarnessStatus st = registry.status(shown_id_);
    const AwbHarnessState state = awb_harness_state(shown_id_);
    const QString link = QString(ui::colors::AMBER());
    const QString warn = QString(ui::colors::WARNING());

    name_lbl_->setText(d.beta ? QStringLiteral("%1  ·  %2  ·  %3").arg(d.display_name, d.vendor, tr("BETA"))
                              : QStringLiteral("%1  ·  %2").arg(d.display_name, d.vendor));

    QStringList lines;
    // Paths, commands and detection messages may break at separators: one long
    // path would otherwise set the width of the whole section.
    lines << QStringLiteral("<span style='color:%1'>●</span> <b>%2</b> — %3")
                 .arg(awb_tone_color(state.tone), state.word.toHtmlEscaped(), awb_breakable_html(state.sentence));
    if (st.installation) {
        QString where = awb_breakable_html(QDir::toNativeSeparators(st.installation->path));
        if (st.installation->via == harness::Installation::NodeScript)
            where += QStringLiteral(" · ") + tr("npm launcher, run with node").toHtmlEscaped();
        else if (st.installation->via == harness::Installation::CmdShim)
            where += QStringLiteral(" · ") + tr("npm launcher, run through cmd.exe").toHtmlEscaped();
        if (st.installation->path_overridden)
            where += QStringLiteral(" · ") + tr("path set by you").toHtmlEscaped();
        lines << where;
        // The method and plan; "signed out" / "unknown" are already the state word.
        if (st.auth.state == harness::AuthState::SignedIn)
            lines << awb_auth_text(st.auth).toHtmlEscaped();
    }
    // The commands the user runs themselves — the terminal never signs in or installs.
    QStringList how;
    if (!st.installation && !d.install_command.isEmpty())
        how << QStringLiteral("<a style='color:%1' href='copy:install'>%2</a> %3")
                   .arg(link, tr("COPY INSTALL COMMAND").toHtmlEscaped(), awb_breakable_html(d.install_command));
    if (st.installation && state.tone == AwbHarnessTone::Warning && !d.signin_command.isEmpty())
        how << QStringLiteral("<a style='color:%1' href='copy:signin'>%2</a> %3")
                   .arg(link, tr("COPY SIGN-IN COMMAND").toHtmlEscaped(), awb_breakable_html(d.signin_command));
    if (!how.isEmpty())
        lines << how.join(QStringLiteral(" · "));
    state_lbl_->setText(lines.join(QStringLiteral("<br>")));

    // Default model / effort.
    {
        const QSignalBlocker b(model_combo_);
        if (!model_combo_->lineEdit() || !model_combo_->lineEdit()->hasFocus()) {
            model_combo_->clear();
            model_combo_->addItem(tr("CLI default"), QString());
            for (const harness::ModelInfo& m : st.models)
                model_combo_->addItem(m.label.isEmpty() || m.label == m.id ? m.id : QStringLiteral("%1 (%2)").arg(m.label, m.id),
                                      m.id);
            const QString def = registry.default_model(shown_id_);
            const int idx = def.isEmpty() ? 0 : model_combo_->findData(def);
            if (idx >= 0)
                model_combo_->setCurrentIndex(idx);
            else
                model_combo_->setEditText(def);
            if (QAbstractItemView* list = model_combo_->view())
                list->setMinimumWidth(list->sizeHintForColumn(0) + 2 * list->frameWidth() + 24);
        }
    }
    {
        const QStringList efforts = d.caps.effort ? registry.efforts(shown_id_, registry.default_model(shown_id_)) : QStringList{};
        const QSignalBlocker b(effort_combo_);
        effort_combo_->clear();
        effort_combo_->addItem(tr("Default"), QString());
        for (const QString& e : efforts)
            effort_combo_->addItem(e, e);
        const int idx = effort_combo_->findData(registry.default_effort(shown_id_));
        effort_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
        effort_combo_->setEnabled(!efforts.isEmpty());
        effort_combo_->setToolTip(efforts.isEmpty() ? tr("%1 has no reasoning-effort setting.").arg(d.display_name) : QString());
    }

    // API keys in the environment: names only, never values.
    {
        const bool has_vars = !d.api_key_env_vars.isEmpty();
        keys_lbl_->setEnabled(has_vars && !d.api_key_only);
        policy_combo_->setEnabled(has_vars && !d.api_key_only);
        const QSignalBlocker b(policy_combo_);
        policy_combo_->setCurrentIndex(qMax(0, policy_combo_->findData(registry.env_policy(shown_id_))));
        QString note;
        if (!has_vars) {
            note = tr("%1 has no API-key variables the terminal knows of.").arg(d.display_name).toHtmlEscaped();
        } else if (d.api_key_only) {
            note = tr("%1 runs from the terminal only with an API key (%2) — its vendor does not permit other apps to use "
                      "a personal sign-in.")
                       .arg(d.display_name, d.api_key_env_vars.join(QStringLiteral(", ")))
                       .toHtmlEscaped();
        } else if (st.auth.api_key_env_present) {
            note = registry.api_key_reaches_child(shown_id_)
                       ? QStringLiteral("<span style='color:%1'>%2</span>")
                             .arg(warn, tr("API key in environment (%1): runs are billed to that key even when you are "
                                           "signed in with a subscription.")
                                            .arg(d.api_key_env_vars.join(QStringLiteral(", ")))
                                            .toHtmlEscaped())
                       : tr("An API key is set in the environment but is not passed to runs.").toHtmlEscaped();
        } else {
            note = tr("No API key in the environment (%1).").arg(d.api_key_env_vars.join(QStringLiteral(", "))).toHtmlEscaped();
        }
        keys_note_->setText(note);
    }

    // Path override.
    if (!path_edit_->hasFocus()) {
        path_edit_->setText(QDir::toNativeSeparators(registry.path_override(shown_id_)));
        path_edit_->setPlaceholderText(st.installation && !st.installation->path_overridden
                                           ? tr("Auto-detected: %1").arg(QDir::toNativeSeparators(st.installation->path))
                                           : tr("Auto-detect"));
    }
    {
        const QSignalBlocker b1(terminal_check_);
        terminal_check_->setChecked(registry.enabled_for_terminal(shown_id_));
        const QSignalBlocker b2(default_check_);
        default_check_->setChecked(registry.default_harness() == shown_id_);
    }
    install_btn_->setEnabled(!d.install_url.isEmpty());
    signin_btn_->setEnabled(!(d.auth_docs_url.isEmpty() && d.docs_url.isEmpty()));

    QStringList notes;
    if (process::host::in_flatpak() && !st.host_blocked)
        notes << tr("Runs on your system (outside the sandbox): Fincept starts the copy installed on your computer, "
                    "with your own sign-in and settings.");
    if (d.beta)
        notes << tr("BETA — %1").arg(d.beta_note.isEmpty() ? tr("not yet checked against a recorded run.") : d.beta_note);
    if (!st.detect_error.isEmpty() && st.installation)
        notes << st.detect_error;
    if (!st.models_error.isEmpty())
        notes << st.models_error;
    else if (st.installation && d.caps.model_list)
        notes << (st.models_from_cli ? tr("%n model(s) listed by the CLI.", nullptr, static_cast<int>(st.models.size()))
                                     : tr("Showing the built-in model list until the CLI answers."));
    if (st.installation && !d.tested_version.isEmpty() && !st.installation->version.isEmpty() &&
        services::coding_agents::compare_cli_versions(st.installation->version, d.tested_version) > 0)
        notes << tr("Version %1 is newer than the tested %2 — the output format may have changed.")
                     .arg(st.installation->version, d.tested_version);
    QStringList html;
    for (const QString& n : std::as_const(notes))
        html << awb_breakable_html(n); // detection notes carry paths
    notes_lbl_->setText(html.join(QStringLiteral("<br>")));
}

void HarnessListWidget::on_model_committed() {
    if (shown_id_.isEmpty())
        return;
    const QString text = model_combo_->currentText().trimmed();
    QString value = text;
    const int idx = model_combo_->findText(text, Qt::MatchExactly);
    if (idx >= 0)
        value = model_combo_->itemData(idx).toString();
    static const QRegularExpression rx(QStringLiteral(R"(^[A-Za-z0-9._:/@\[\]-]{0,100}$)"));
    if (!rx.match(value).hasMatch())
        return; // the service would refuse it anyway
    auto& registry = harness::HarnessRegistry::instance();
    if (value != registry.default_model(shown_id_))
        registry.set_default_model(shown_id_, value);
}

void HarnessListWidget::on_full_access_toggled(bool on) {
    if (on) {
        const auto answer = QMessageBox::warning(
            this, tr("Allow full-access runs?"),
            tr("Full-access runs have no sandbox and ask no approvals: the agent can run any command and change or "
               "delete any file your account can reach. Each run still has to choose “Full access” and confirm it.\n\n"
               "Offer it in the run options?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker b(full_access_check_);
            full_access_check_->setChecked(false);
            return;
        }
    }
    harness::HarnessRegistry::instance().set_allow_full_access(on);
}

void HarnessListWidget::on_apply_path() {
    if (shown_id_.isEmpty())
        return;
    auto& registry = harness::HarnessRegistry::instance();
    const QString path = QDir::fromNativeSeparators(path_edit_->text().trimmed());
    if (path != registry.path_override(shown_id_))
        registry.set_path_override(shown_id_, path); // re-detects that harness
    else
        registry.refresh(shown_id_, harness::RefreshScope::Detect);
}

void HarnessListWidget::on_browse_path() {
#ifdef Q_OS_WIN
    const QString filter = tr("Executables (*.exe *.cmd *.bat);;All files (*)");
#else
    const QString filter = tr("All files (*)");
#endif
    QString path = QFileDialog::getOpenFileName(this, tr("Select the CLI executable"), QDir::homePath(), filter);
    if (path.isEmpty())
        return;
    // Flatpak: the dialog answers with a document-portal copy path; the CLI is
    // started on the host, at its real path.
    path = process::host::document_host_path(path);
    path_edit_->setText(QDir::toNativeSeparators(path));
    on_apply_path();
}

void HarnessListWidget::on_link(const QString& link) {
    const harness::IHarnessAdapter* adapter = harness::HarnessRegistry::instance().adapter(shown_id_);
    if (!adapter)
        return;
    if (link == QLatin1String("copy:install"))
        QGuiApplication::clipboard()->setText(adapter->descriptor().install_command);
    else if (link == QLatin1String("copy:signin"))
        QGuiApplication::clipboard()->setText(adapter->descriptor().signin_command);
    else if (link.startsWith(QLatin1String("https://")) || link.startsWith(QLatin1String("http://")))
        QDesktopServices::openUrl(QUrl(link));
}

} // namespace fincept::screens
