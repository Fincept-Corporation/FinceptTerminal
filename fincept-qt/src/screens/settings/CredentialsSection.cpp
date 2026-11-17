// CredentialsSection.cpp — API credential entry cards backed by SecureStorage.

#include "screens/settings/CredentialsSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

using CredDef = QPair<QString, QString>; // {env_key, display_name}
const QList<CredDef> CRED_KEYS = {
    {"ALPHA_VANTAGE_API_KEY", "Alpha Vantage"},
    {"POLYGON_API_KEY", "Polygon.io"},
    {"DATABENTO_API_KEY", "Databento"},
    {"FRED_API_KEY", "FRED (Federal Reserve)"},
    {"NEWSAPI_KEY", "NewsAPI"},
    {"BINANCE_API_KEY", "Binance API Key"},
    {"BINANCE_SECRET_KEY", "Binance Secret Key"},
    {"KRAKEN_API_KEY", "Kraken API Key"},
    {"KRAKEN_SECRET_KEY", "Kraken Secret Key"},
    {"FINNHUB_API_KEY", "Finnhub API Key"},
    {"TIINGO_API_KEY", "Tiingo API Key"},
    {"QUANDL_API_KEY", "Quandl"},
    {"POLYMARKET_API_KEY", "Polymarket API Key"},
    {"POLYMARKET_SECRET", "Polymarket Secret"},
    {"POLYMARKET_PASSPHRASE", "Polymarket Passphrase"},
    {"POLYMARKET_WALLET", "Polymarket Wallet Address"},
// Keyed data-connector provider keys (auto-generated; one entry card each).
// Kept in sync with PythonRunner.cpp kManagedCredentialKeys via the shared
// X-macro include below.
#define FINCEPT_KEYED_CRED(KEY, NAME) {KEY, NAME}, // NOLINT(cppcoreguidelines-macro-usage) — X-macro list expansion
#include "config/KeyedConnectorCredentials.inc"
#undef FINCEPT_KEYED_CRED
};

// One place for the per-key status line's state colour. The handlers used to
// call setStyleSheet() inline in every branch (five copies).
void cred_set_status(QLabel* status, const QString& text, const QString& color) {
    status->setText(text);
    status->setStyleSheet(QString("color:%1;background:transparent;").arg(color));
}

} // namespace

CredentialsSection::CredentialsSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void CredentialsSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void CredentialsSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(0);

    title_ = new QLabel(tr("API CREDENTIALS"));
    title_->setStyleSheet(section_title_ss());
    vl->addWidget(title_);
    vl->addSpacing(4);

    info_ = new QLabel(tr("API keys are encrypted (AES-256-GCM) before they are stored on this machine and are never written "
                          "to disk in plain text."));
    info_->setWordWrap(true);
    info_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info_);
    vl->addSpacing(16);
    vl->addWidget(make_sep());
    vl->addSpacing(16);

    // One page-level stylesheet keyed by objectName instead of ~6 inline
    // setStyleSheet() calls per credential card. With 60+ providers in
    // CRED_KEYS that was 350+ CSS parses every time this section is built —
    // and SettingsScreen builds all 16 sections eagerly. Only the per-key
    // status label keeps an inline style, because its colour is state-driven.
    page->setStyleSheet(QString("QFrame#credCard{background:%1;border:1px solid %2;}"
                                "QWidget#credHdr{background:%3;border-bottom:1px solid %2;}"
                                "QLabel#credName{color:%4;font-weight:600;background:transparent;}"
                                "QWidget#credBody{background:transparent;}"
                                "QLineEdit#credField,QLineEdit#credFilter{background:%3;color:%4;"
                                "border:1px solid %5;padding:6px;}"
                                "QLineEdit#credField:focus,QLineEdit#credFilter:focus{border:1px solid %6;}"
                                "QLabel#credEmpty{color:%9;background:transparent;}"
                                "QPushButton#credSave{background:%6;color:%7;border:none;font-weight:700;"
                                "padding:0 16px;}"
                                "QPushButton#credSave:hover{background:%8;}")
                            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                                 ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER(),
                                 ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(), ui::colors::TEXT_DIM()));

    // ~130 provider cards: finding one by scrolling is hopeless, and the Settings
    // nav search only reaches this section, not a card inside it.
    filter_edit_ = new QLineEdit;
    filter_edit_->setObjectName(QStringLiteral("credFilter"));
    filter_edit_->setPlaceholderText(tr("Filter providers..."));
    filter_edit_->setClearButtonEnabled(true);
    filter_edit_->setAccessibleName(tr("Filter credential providers"));
    connect(filter_edit_, &QLineEdit::textChanged, this, &CredentialsSection::apply_filter);
    vl->addWidget(filter_edit_);
    vl->addSpacing(12);

    no_match_lbl_ = new QLabel(tr("No provider matches the filter."));
    no_match_lbl_->setObjectName(QStringLiteral("credEmpty"));
    no_match_lbl_->hide();
    vl->addWidget(no_match_lbl_);

    // The cards themselves are built a few at a time (build_more_cards()):
    // ~130 cards are ~900 widgets, and building them in one go froze the first
    // visit to this section for 1.5-2.5 s (measured: Qt styles every widget when
    // the page is put in the scroll area, again when the section enters the
    // stack, then lays all of them out). The first screenful is built here, so
    // the section opens complete as far as the eye can tell; the rest follows on
    // the next turns of the event loop.
    page_ = page;
    cards_layout_ = new QVBoxLayout;
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    cards_layout_->setSpacing(0);
    vl->addLayout(cards_layout_);
    for (int i = 0; i < kFirstCards && next_card_ < CRED_KEYS.size(); ++i)
        build_card(next_card_++);

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);

    if (next_card_ < CRED_KEYS.size())
        QTimer::singleShot(0, this, &CredentialsSection::build_more_cards);
}

void CredentialsSection::build_more_cards() {
    if (!page_ || !cards_layout_)
        return;
    for (int i = 0; i < kCardsPerTurn && next_card_ < CRED_KEYS.size(); ++i)
        build_card(next_card_++);
    if (next_card_ < CRED_KEYS.size())
        QTimer::singleShot(0, this, &CredentialsSection::build_more_cards);
}

void CredentialsSection::build_card(int index) {
    const auto& def = CRED_KEYS.at(index);
    const QString& key = def.first;
    const QString& name = def.second;

    // Each card sits in a wrapper that carries the 8px gap, so hiding the
    // wrapper (see apply_filter) also hides the gap — a bare addSpacing()
    // would pile up as blank space once most cards are filtered out.
    // Every widget is created under its final parent: a widget that is moved
    // to another parent later is styled a second time.
    auto* wrap = new QWidget(page_);
    auto* wrap_lay = new QVBoxLayout(wrap);
    wrap_lay->setContentsMargins(0, 0, 0, 8);
    wrap_lay->setSpacing(0);

    auto* card = new QFrame(wrap);
    card->setObjectName(QStringLiteral("credCard"));
    auto* cvl = new QVBoxLayout(card);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);

    auto* hdr = new QWidget(card);
    hdr->setObjectName(QStringLiteral("credHdr"));
    hdr->setMinimumHeight(34);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);
    auto* name_lbl = new QLabel(name, hdr);
    name_lbl->setObjectName(QStringLiteral("credName"));
    hhl->addWidget(name_lbl);
    hhl->addStretch();

    auto* status_lbl = new QLabel(hdr);
    hhl->addWidget(status_lbl);
    cred_status_[key] = status_lbl;

    cvl->addWidget(hdr);

    auto* body = new QWidget(card);
    body->setObjectName(QStringLiteral("credBody"));
    auto* bhl = new QHBoxLayout(body);
    bhl->setContentsMargins(12, 10, 12, 10);
    bhl->setSpacing(8);

    auto* field = new QLineEdit(body);
    field->setObjectName(QStringLiteral("credField"));
    field->setEchoMode(QLineEdit::Password);
    // Never let an API key end up in an IME candidate list, a predictive
    // dictionary, or a drag-and-drop payload.
    field->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    field->setDragEnabled(false);
    field->setAcceptDrops(false);
    field->setAccessibleName(tr("%1 API key").arg(name));
    cred_fields_[key] = field;
    bhl->addWidget(field, 1);

    auto* save_btn = new QPushButton(tr("Save"), body);
    save_btn->setProperty("fincept_agent_risk", "account"); // agents: stores API keys
    save_btn->setObjectName(QStringLiteral("credSave"));
    save_btn->setMinimumSize(70, 30);
    save_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    save_btn->setAccessibleName(tr("Save %1 API key").arg(name));
    bhl->addWidget(save_btn);
    cred_save_btns_[key] = save_btn;

    // Stored / not stored, as reload() shows it for the cards already built.
    load_card_state(key);

    // Enter in the field is the same as clicking Save.
    connect(field, &QLineEdit::returnPressed, save_btn, &QPushButton::click);
    setTabOrder(field, save_btn);

    connect(save_btn, &QPushButton::clicked, this, [this, key, name, field, status_lbl]() {
        QString val = field->text().trimmed();
        if (val.isEmpty()) {
            // Saving an empty field deletes the stored key. That is a
            // destructive, silent action when a key is already configured —
            // confirm it rather than shredding a credential on a stray click.
            // A key that could not be read may well exist: confirm then too.
            const SecretRead existing = SecureStorage::instance().read(key);
            const bool may_have_key = existing.failed() || (existing.found() && !existing.value.isEmpty());
            if (may_have_key) {
                const auto reply = QMessageBox::question(
                    this, tr("Remove Credential"),
                    tr("Remove the stored %1 key from encrypted storage?\n\nYou will need to paste it "
                       "again to restore access.")
                        .arg(name),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (reply != QMessageBox::Yes)
                    return;
            }
            const auto removed = SecureStorage::instance().remove(key);
            if (removed.is_err()) {
                // The result was ignored, so "Cleared" was shown even when the
                // key was still stored and still being injected into
                // every Python subprocess.
                cred_set_status(status_lbl, tr("Clear failed"), ui::colors::NEGATIVE());
                LOG_ERROR("Credentials", "Failed to clear " + key);
                return;
            }
            field->setPlaceholderText(tr("Not configured"));
            cred_set_status(status_lbl, tr("Cleared"), ui::colors::TEXT_SECONDARY());
            LOG_AUDIT("Credentials", "Credential removed", {{"name", key}});
        } else {
            auto r = SecureStorage::instance().store(key, val);
            if (r.is_ok()) {
                field->clear();
                field->setPlaceholderText(tr("•••••••• (saved)"));
                cred_set_status(status_lbl, tr("Saved ✓"), ui::colors::POSITIVE());
                LOG_AUDIT("Credentials", "Credential saved", {{"name", key}});
            } else {
                cred_set_status(status_lbl, tr("Save failed"), ui::colors::NEGATIVE());
                LOG_ERROR("Credentials", "Failed to store " + key);
            }
        }
    });

    cvl->addWidget(body);
    wrap_lay->addWidget(card);

    // A card built while a filter is typed in follows that filter.
    if (filter_edit_ && !matches_filter(def, filter_edit_->text().trimmed()))
        wrap->hide();
    cards_layout_->addWidget(wrap);
    cred_cards_[key] = wrap;
}

void CredentialsSection::load_card_state(const QString& key) {
    auto* field = cred_fields_.value(key, nullptr);
    auto* status = cred_status_.value(key, nullptr);
    if (!field || !status)
        return;
    const SecretRead r = SecureStorage::instance().read(key);
    field->clear();
    if (r.found() && !r.value.isEmpty()) {
        field->setPlaceholderText(tr("•••••••• (saved)"));
        cred_set_status(status, tr("Saved ✓"), ui::colors::POSITIVE());
    } else if (r.failed()) {
        // Not "Not set": a key may be stored and simply unreadable right now.
        field->setPlaceholderText(tr("Stored key could not be read"));
        cred_set_status(status, tr("Could not read: %1").arg(r.reason()), ui::colors::WARNING());
    } else {
        field->setPlaceholderText(tr("Not configured"));
        cred_set_status(status, tr("Not set"), ui::colors::TEXT_SECONDARY());
    }
}

void CredentialsSection::reload() {
    for (auto it = cred_fields_.constBegin(); it != cred_fields_.constEnd(); ++it)
        load_card_state(it.key());
}

void CredentialsSection::set_filter(const QString& text) {
    if (filter_edit_)
        filter_edit_->setText(text); // textChanged → apply_filter()
}

QStringList CredentialsSection::search_terms() {
    QStringList out;
    out.reserve(CRED_KEYS.size() * 2);
    for (const auto& def : CRED_KEYS)
        out << def.second << def.first;
    return out;
}

bool CredentialsSection::matches_filter(const QPair<QString, QString>& def, const QString& needle) {
    return needle.isEmpty() || def.second.contains(needle, Qt::CaseInsensitive) ||
           def.first.contains(needle, Qt::CaseInsensitive);
}

void CredentialsSection::apply_filter(const QString& text) {
    const QString needle = text.trimmed();
    // Counted over every provider, built or not: a match whose card is still to
    // be built (build_more_cards) must not flash "No provider matches".
    int matching = 0;
    for (const auto& def : CRED_KEYS) {
        const bool match = matches_filter(def, needle);
        if (match)
            ++matching;
        if (auto* card = cred_cards_.value(def.first, nullptr))
            card->setVisible(match);
    }
    if (no_match_lbl_)
        no_match_lbl_->setVisible(matching == 0);
}

void CredentialsSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CredentialsSection::retranslateUi() {
    if (title_)
        title_->setText(tr("API CREDENTIALS"));
    if (info_)
        info_->setText(tr("API keys are encrypted (AES-256-GCM) before they are stored on this machine and are never written "
                          "to disk in plain text."));
    if (filter_edit_) {
        filter_edit_->setPlaceholderText(tr("Filter providers..."));
        filter_edit_->setAccessibleName(tr("Filter credential providers"));
    }
    if (no_match_lbl_)
        no_match_lbl_->setText(tr("No provider matches the filter."));

    // Per-credential Save buttons. (Credential names are product/brand names —
    // not translated. Provider name labels stay as-is.)
    for (auto it = cred_save_btns_.constBegin(); it != cred_save_btns_.constEnd(); ++it) {
        if (it.value())
            it.value()->setText(tr("Save"));
    }

    // reload() re-applies state-dependent placeholders ("Not configured" /
    // "•••••••• (saved)") and status labels ("Not set" / "Saved ✓") in the
    // current language.
    reload();
}

} // namespace fincept::screens
