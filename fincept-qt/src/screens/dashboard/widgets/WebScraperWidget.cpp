#include "screens/dashboard/widgets/WebScraperWidget.h"

#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

constexpr qsizetype kMaxRowsRendered = 5000; // safety cap for very large tables

QString scraper_size_text(qint64 bytes) {
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    return QStringLiteral("%1 B").arg(bytes);
}

} // namespace

// ─── ctor / dtor ──────────────────────────────────────────────────────────

WebScraperWidget::WebScraperWidget(const QJsonObject& cfg, QWidget* parent) : BaseWidget(tr("WEB SCRAPER"), parent) {
    auto_timer_ = new QTimer(this);
    auto_timer_->setSingleShot(false);
    connect(auto_timer_, &QTimer::timeout, this, &WebScraperWidget::on_auto_refresh_tick);

    build_ui();
    set_configurable(true);
    apply_styles();

    connect(this, &BaseWidget::refresh_requested, this, &WebScraperWidget::on_refresh_clicked);

    apply_config(cfg);
}

WebScraperWidget::~WebScraperWidget() {
    cancel_fetch();
}

// ─── UI ───────────────────────────────────────────────────────────────────

void WebScraperWidget::build_ui() {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 8);
    vl->setSpacing(6);

    // One styled container for the header row, so badge / status colours are
    // object-name + property selectors in a single sheet (no per-update CSS).
    auto* header = new QWidget(this);
    header->setObjectName("scraperHeader");
    auto* header_row = new QHBoxLayout(header);
    header_row->setContentsMargins(0, 0, 0, 0);
    header_row->setSpacing(8);

    format_badge_ = new QLabel(QStringLiteral("—"));
    format_badge_->setObjectName("scraperFormatBadge");
    header_row->addWidget(format_badge_);

    table_combo_ = new QComboBox();
    table_combo_->setObjectName("scraperTableCombo");
    table_combo_->setMinimumContentsLength(12);
    table_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    table_combo_->setEnabled(false);
    connect(table_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WebScraperWidget::on_table_selected);
    header_row->addWidget(table_combo_, 1);

    status_label_ = new QLabel(tr("Configure a URL via the gear icon"));
    status_label_->setObjectName("scraperStatus");
    status_label_->setProperty("error", false);
    status_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header_row->addWidget(status_label_, 2);

    vl->addWidget(header);

    table_ = new ui::DataTable();
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->setSortingEnabled(true);
    vl->addWidget(table_, 1);
}

void WebScraperWidget::apply_styles() {
    auto* header = findChild<QWidget*>(QStringLiteral("scraperHeader"));
    if (!header)
        return;
    header->setStyleSheet(QString("QLabel#scraperFormatBadge{color:%1;background:%2;border-radius:0;"
                                  "padding:2px 6px;font-size:10px;font-weight:700;}"
                                  "QLabel#scraperStatus{color:%3;font-size:11px;background:transparent;}"
                                  "QLabel#scraperStatus[error=\"true\"]{color:%4;}")
                              .arg(ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(),
                                   ui::colors::NEGATIVE()));
}

void WebScraperWidget::on_theme_changed() {
    apply_styles();
}

// ─── Config ───────────────────────────────────────────────────────────────

QJsonObject WebScraperWidget::config() const {
    QJsonObject o;
    o.insert("url", url_);
    o.insert("table_index", table_index_);
    o.insert("refresh_sec", refresh_sec_);
    if (!user_agent_.isEmpty())
        o.insert("user_agent", user_agent_);
    if (!headers_.isEmpty()) {
        QJsonObject h;
        for (auto it = headers_.cbegin(); it != headers_.cend(); ++it)
            h.insert(it.key(), it.value());
        o.insert("headers", h);
    }
    if (!encoding_.isEmpty())
        o.insert("encoding", encoding_);
    if (!json_path_.isEmpty())
        o.insert("json_path", json_path_);
    if (!force_format_.isEmpty())
        o.insert("force_format", force_format_);
    return o;
}

void WebScraperWidget::apply_config(const QJsonObject& cfg) {
    const QString prev_url = url_;
    url_ = cfg.value("url").toString().trimmed();
    table_index_ = std::max(0, cfg.value("table_index").toInt(0));
    // 0 = manual; anything else at least 10 s — a 1 s auto-refresh of a
    // third-party page is a scraping ban waiting to happen.
    const int requested_refresh = cfg.value("refresh_sec").toInt(0);
    refresh_sec_ = requested_refresh <= 0 ? 0 : std::clamp(requested_refresh, 10, 86400);
    user_agent_ = cfg.value("user_agent").toString();
    headers_.clear();
    const QJsonObject h = cfg.value("headers").toObject();
    for (auto it = h.constBegin(); it != h.constEnd(); ++it)
        headers_.insert(it.key(), it.value().toString());
    encoding_ = cfg.value("encoding").toString();
    json_path_ = cfg.value("json_path").toString();
    force_format_ = cfg.value("force_format").toString();

    if (url_ != prev_url)
        has_result_ = false;

    apply_timer_state();

    if (url_.isEmpty()) {
        cancel_fetch();
        set_status(tr("Configure a URL via the gear icon"));
    } else if (isVisible()) {
        start_fetch();
    }
}

void WebScraperWidget::apply_timer_state() {
    if (refresh_sec_ > 0)
        auto_timer_->setInterval(refresh_sec_ * 1000);
    if (!isVisible() || refresh_sec_ <= 0 || url_.isEmpty()) {
        auto_timer_->stop();
    } else if (!auto_timer_->isActive()) {
        auto_timer_->start();
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────

void WebScraperWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    apply_timer_state();
    if (!url_.isEmpty() && !has_result_ && pending_id_ == 0)
        start_fetch();
}

void WebScraperWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    auto_timer_->stop();
}

// ─── Fetch ────────────────────────────────────────────────────────────────

void WebScraperWidget::on_refresh_clicked() {
    if (url_.isEmpty())
        return;
    start_fetch();
}

void WebScraperWidget::on_auto_refresh_tick() {
    if (url_.isEmpty() || pending_id_ != 0)
        return;
    start_fetch();
}

void WebScraperWidget::cancel_fetch() {
    if (pending_id_ == 0)
        return;
    services::WebScrapeService::instance().cancel(pending_id_);
    pending_id_ = 0;
}

void WebScraperWidget::start_fetch() {
    cancel_fetch(); // a refresh supersedes the fetch in flight

    services::WebScrapeRequest req;
    req.url = url_;
    req.user_agent = user_agent_;
    req.headers = headers_;
    req.encoding = encoding_;
    req.json_path = json_path_;
    req.force_format = force_format_;

    set_loading(true);
    set_status(tr("Fetching…"));
    const quint64 id = services::WebScrapeService::instance().scrape(
        req, this, [this](const services::WebScrapeResult& r) { on_result(r); });
    // 0 = rejected synchronously (bad URL); on_result() already ran.
    if (id != 0)
        pending_id_ = id;
}

void WebScraperWidget::on_result(const services::WebScrapeResult& result) {
    pending_id_ = 0;
    set_loading(false);

    if (!result.ok) {
        // Keep the last good table on screen; the status line says the refresh failed.
        set_status(has_result_ ? tr("Refresh failed: %1 — showing previous data").arg(result.error)
                               : tr("Fetch failed: %1").arg(result.error),
                   true);
        return;
    }

    tables_ = result.tables;
    last_format_ = result.format;
    has_result_ = true;
    format_badge_->setText(last_format_.isEmpty() ? QStringLiteral("—") : last_format_);

    // Repopulate table-picker combo.
    const int prev_index = table_index_;
    QSignalBlocker block(table_combo_);
    table_combo_->clear();
    for (int i = 0; i < tables_.size(); ++i)
        table_combo_->addItem(tr("%1 (%2 rows)")
                                  .arg(tables_[i].label.isEmpty() ? tr("Table %1").arg(i + 1) : tables_[i].label)
                                  .arg(tables_[i].rows.size()));
    if (tables_.isEmpty()) {
        table_combo_->setEnabled(false);
        table_->clear_data();
        table_->set_headers({});
        set_status(tr("No tabular data found — the site may build its tables with JavaScript. Try its JSON API "
                      "URL instead."),
                   true);
        return;
    }
    table_combo_->setEnabled(tables_.size() > 1);
    const int idx = std::clamp<int>(prev_index, 0, static_cast<int>(tables_.size()) - 1);
    table_index_ = idx;
    table_combo_->setCurrentIndex(idx);
    render_selected_table();
    set_status(tr("%n table(s) · %1 · %2", "", static_cast<int>(tables_.size()))
                   .arg(scraper_size_text(result.bytes), QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void WebScraperWidget::on_table_selected(int index) {
    if (index < 0 || index >= tables_.size())
        return;
    if (index == table_index_)
        return;
    table_index_ = index;
    render_selected_table();
    emit config_changed(config());
}

void WebScraperWidget::render_selected_table() {
    if (table_index_ < 0 || table_index_ >= tables_.size())
        return;
    const services::ScrapedTable& t = tables_[table_index_];
    table_->setSortingEnabled(false);
    table_->set_headers(t.headers);
    const qsizetype n = std::min<qsizetype>(t.rows.size(), kMaxRowsRendered);
    table_->set_data(t.rows.mid(0, n));
    table_->setSortingEnabled(true);
    if (t.rows.size() > kMaxRowsRendered)
        set_status(tr("Showing first %1 of %2 rows").arg(n).arg(t.rows.size()));
}

void WebScraperWidget::set_status(const QString& msg, bool error) {
    if (!status_label_)
        return;
    status_label_->setText(msg);
    status_label_->setToolTip(msg);
    if (status_label_->property("error").toBool() != error) {
        status_label_->setProperty("error", error);
        status_label_->style()->unpolish(status_label_);
        status_label_->style()->polish(status_label_);
    }
}

// ─── Config dialog ────────────────────────────────────────────────────────

QDialog* WebScraperWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Web Scraper"));
    dlg->resize(520, 440);
    auto* form = new QFormLayout(dlg);

    auto* url_edit = new QLineEdit(dlg);
    url_edit->setText(url_);
    url_edit->setPlaceholderText(tr("https://example.com/table-page or API endpoint"));
    form->addRow(tr("URL"), url_edit);

    auto* refresh_spin = new QSpinBox(dlg);
    refresh_spin->setRange(0, 86400);
    refresh_spin->setSuffix(tr(" sec"));
    refresh_spin->setSpecialValueText(tr("manual only"));
    refresh_spin->setToolTip(tr("0 = manual refresh only; otherwise at least 10 seconds"));
    refresh_spin->setValue(refresh_sec_);
    form->addRow(tr("Auto-refresh"), refresh_spin);

    auto* ua_edit = new QLineEdit(dlg);
    ua_edit->setText(user_agent_);
    ua_edit->setPlaceholderText(tr("default Fincept user agent"));
    form->addRow(tr("User-Agent"), ua_edit);

    auto* fmt_combo = new QComboBox(dlg);
    fmt_combo->addItems({tr("Auto-detect"), "HTML", "JSON", "CSV", "TSV", "XML"});
    const QHash<QString, int> fmt_idx = {{"", 0}, {"html", 1}, {"json", 2}, {"csv", 3}, {"tsv", 4}, {"xml", 5}};
    fmt_combo->setCurrentIndex(fmt_idx.value(force_format_.toLower(), 0));
    form->addRow(tr("Format"), fmt_combo);

    auto* enc_edit = new QLineEdit(dlg);
    enc_edit->setText(encoding_);
    enc_edit->setPlaceholderText(tr("auto (from Content-Type / <meta>)"));
    form->addRow(tr("Encoding"), enc_edit);

    auto* json_path_edit = new QLineEdit(dlg);
    json_path_edit->setText(json_path_);
    json_path_edit->setPlaceholderText(tr("e.g. data.items  (JSON only)"));
    form->addRow(tr("JSON path"), json_path_edit);

    auto* headers_edit = new QPlainTextEdit(dlg);
    QString hdr_text;
    for (auto it = headers_.cbegin(); it != headers_.cend(); ++it)
        hdr_text.append(QString("%1: %2\n").arg(it.key(), it.value()));
    headers_edit->setPlainText(hdr_text);
    headers_edit->setPlaceholderText(tr("One per line:\nAuthorization: Bearer abc\nX-API-Key: xyz"));
    headers_edit->setToolTip(tr("Stored in the dashboard layout in plain text — use a read-only key."));
    headers_edit->setMaximumHeight(100);
    form->addRow(tr("Extra headers"), headers_edit);

    auto* help = new QLabel(tr("Auto-detects HTML tables, JSON arrays/objects, CSV/TSV, XML/RSS/Atom. Responses are "
                               "capped at 10 MB and 30 s. If the page renders tables via JavaScript, use its "
                               "underlying API URL instead."));
    help->setWordWrap(true);
    help->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    form->addRow(help);

    // Inline validation — an invalid URL used to be accepted and only failed
    // later on the tile.
    auto* error_lbl = new QLabel(dlg);
    error_lbl->setWordWrap(true);
    error_lbl->setVisible(false);
    error_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;").arg(ui::colors::NEGATIVE()));
    form->addRow(error_lbl);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg,
            [this, dlg, url_edit, refresh_spin, ua_edit, fmt_combo, enc_edit, json_path_edit, headers_edit,
             error_lbl]() {
                const QString url_text = url_edit->text().trimmed();
                if (!url_text.isEmpty()) {
                    const QString why =
                        services::WebScrapeService::validate_url(QUrl::fromUserInput(url_text));
                    if (!why.isEmpty()) {
                        error_lbl->setText(why);
                        error_lbl->setVisible(true);
                        url_edit->setFocus();
                        return;
                    }
                }

                QJsonObject cfg;
                cfg.insert("url", url_text);
                cfg.insert("refresh_sec", refresh_spin->value());
                // Keep the chosen table when only the cadence/headers changed.
                cfg.insert("table_index", url_text == url_ ? table_index_ : 0);
                if (!ua_edit->text().isEmpty())
                    cfg.insert("user_agent", ua_edit->text());

                const QStringList fmts = {"", "html", "json", "csv", "tsv", "xml"};
                const int fi = fmt_combo->currentIndex();
                if (fi > 0 && fi < fmts.size())
                    cfg.insert("force_format", fmts[fi]);

                if (!enc_edit->text().isEmpty())
                    cfg.insert("encoding", enc_edit->text());
                if (!json_path_edit->text().isEmpty())
                    cfg.insert("json_path", json_path_edit->text());

                QJsonObject hdrs;
                const QStringList lines = headers_edit->toPlainText().split('\n', Qt::SkipEmptyParts);
                for (const QString& l : lines) {
                    const int colon = l.indexOf(':');
                    if (colon <= 0)
                        continue;
                    const QString k = l.left(colon).trimmed();
                    const QString v = l.mid(colon + 1).trimmed();
                    if (!k.isEmpty())
                        hdrs.insert(k, v);
                }
                if (!hdrs.isEmpty())
                    cfg.insert("headers", hdrs);

                apply_config(cfg);
                emit config_changed(config()); // normalised (refresh clamp etc.)
                dlg->accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void WebScraperWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("WEB SCRAPER"));
    if (url_.isEmpty())
        set_status(tr("Configure a URL via the gear icon"));
}

} // namespace fincept::screens::widgets
