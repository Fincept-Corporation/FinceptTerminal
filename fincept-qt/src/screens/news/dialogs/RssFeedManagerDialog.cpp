#include "screens/news/dialogs/RssFeedManagerDialog.h"

#include "core/logging/Logger.h"
#include "screens/news/dialogs/RssFeedEditDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

constexpr int kColEnabled = 0;
constexpr int kColType = 1;
constexpr int kColSource = 2;
constexpr int kColName = 3;
constexpr int kColUrl = 4;
constexpr int kColCategory = 5;
constexpr int kColRegion = 6;
constexpr int kColTier = 7;

constexpr const char* kBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

} // anonymous namespace

RssFeedManagerDialog::RssFeedManagerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("RSS Feed Sources"));
    setModal(true);
    resize(960, 580);
    build_ui();
    reload_table();
}

void RssFeedManagerDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void RssFeedManagerDialog::retranslateUi() {
    setWindowTitle(tr("RSS Feed Sources"));
    if (header_label_)
        header_label_->setText(
            tr("Manage the RSS feeds the News screen pulls from. Built-in feeds (DEF) "
               "can be disabled or edited; user-added feeds (USR) can be removed."));
    if (table_)
        table_->setHorizontalHeaderLabels(
            {tr("On"), tr("Type"), tr("Source"), tr("Name"), tr("URL"),
             tr("Category"), tr("Region"), tr("Tier")});
    if (add_btn_)   add_btn_->setText(tr("Add Feed"));
    if (edit_btn_)  edit_btn_->setText(tr("Edit"));
    if (test_btn_)  test_btn_->setText(tr("Test URL"));
    if (close_btn_) close_btn_->setText(tr("Close"));
    // toggle_btn_ / delete_btn_ labels + the status label are selection- and
    // count-dependent; re-derive them so the new language is reflected.
    on_selection_changed();
}

void RssFeedManagerDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);

    header_label_ = new QLabel(
        tr("Manage the RSS feeds the News screen pulls from. Built-in feeds (DEF) "
           "can be disabled or edited; user-added feeds (USR) can be removed."),
        this);
    header_label_->setObjectName("rssFeedManagerHeader");
    header_label_->setWordWrap(true);
    root->addWidget(header_label_);

    table_ = new QTableWidget(this);
    table_->setObjectName("rssFeedManagerTable");
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels(
        {tr("On"), tr("Type"), tr("Source"), tr("Name"), tr("URL"),
         tr("Category"), tr("Region"), tr("Tier")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(kColUrl, QHeaderView::Stretch);
    table_->setColumnWidth(kColEnabled, 40);
    table_->setColumnWidth(kColType, 50);
    table_->setColumnWidth(kColSource, 120);
    table_->setColumnWidth(kColName, 180);
    table_->setColumnWidth(kColCategory, 110);
    table_->setColumnWidth(kColRegion, 70);
    table_->setColumnWidth(kColTier, 50);
    root->addWidget(table_, 1);

    auto* btn_row = new QHBoxLayout();
    btn_row->setSpacing(6);

    add_btn_ = new QPushButton(tr("Add Feed"), this);
    edit_btn_ = new QPushButton(tr("Edit"), this);
    toggle_btn_ = new QPushButton(tr("Disable"), this);
    delete_btn_ = new QPushButton(tr("Delete"), this);
    test_btn_ = new QPushButton(tr("Test URL"), this);
    for (auto* b : {add_btn_, edit_btn_, toggle_btn_, delete_btn_, test_btn_}) {
        b->setCursor(Qt::PointingHandCursor);
        btn_row->addWidget(b);
    }
    btn_row->addStretch();

    status_label_ = new QLabel(this);
    status_label_->setObjectName("rssFeedManagerStatus");
    btn_row->addWidget(status_label_);

    close_btn_ = new QPushButton(tr("Close"), this);
    close_btn_->setDefault(true);
    close_btn_->setCursor(Qt::PointingHandCursor);
    btn_row->addWidget(close_btn_);
    root->addLayout(btn_row);

    connect(add_btn_, &QPushButton::clicked, this, &RssFeedManagerDialog::on_add);
    connect(edit_btn_, &QPushButton::clicked, this, &RssFeedManagerDialog::on_edit);
    connect(toggle_btn_, &QPushButton::clicked, this, &RssFeedManagerDialog::on_toggle_enabled);
    connect(delete_btn_, &QPushButton::clicked, this, &RssFeedManagerDialog::on_delete_or_reset);
    connect(test_btn_, &QPushButton::clicked, this, &RssFeedManagerDialog::on_test_url);
    connect(close_btn_, &QPushButton::clicked, this, &QDialog::accept);
    connect(table_, &QTableWidget::itemSelectionChanged, this,
            &RssFeedManagerDialog::on_selection_changed);
    connect(table_, &QTableWidget::itemDoubleClicked, this,
            [this](QTableWidgetItem*) { on_edit(); });
}

void RssFeedManagerDialog::reload_table() {
    rows_ = services::NewsService::instance().list_all_feeds_for_editor();
    table_->setRowCount(rows_.size());

    for (int i = 0; i < rows_.size(); ++i) {
        const auto& r = rows_[i];

        auto make_item = [&](const QString& text) {
            auto* it = new QTableWidgetItem(text);
            if (!r.enabled)
                it->setForeground(QBrush(QColor(148, 163, 184)));
            return it;
        };

        auto* on_item = new QTableWidgetItem(r.enabled ? QStringLiteral("●") : QStringLiteral("○"));
        on_item->setTextAlignment(Qt::AlignCenter);
        on_item->setForeground(r.enabled ? QBrush(QColor(22, 163, 74))
                                          : QBrush(QColor(148, 163, 184)));
        table_->setItem(i, kColEnabled, on_item);

        auto* type_item = new QTableWidgetItem(r.is_builtin ? (r.is_customized ? "DEF*" : "DEF")
                                                              : "USR");
        type_item->setTextAlignment(Qt::AlignCenter);
        type_item->setForeground(r.is_builtin ? QBrush(QColor(59, 130, 246))
                                                : QBrush(QColor(217, 119, 6)));
        type_item->setToolTip(r.is_builtin ? (r.is_customized ? tr("Built-in feed with user edits")
                                                                : tr("Built-in default feed"))
                                            : tr("User-added feed"));
        table_->setItem(i, kColType, type_item);

        table_->setItem(i, kColSource, make_item(r.feed.source));
        table_->setItem(i, kColName, make_item(r.feed.name));
        table_->setItem(i, kColUrl, make_item(r.feed.url));
        table_->setItem(i, kColCategory, make_item(r.feed.category));
        table_->setItem(i, kColRegion, make_item(r.feed.region));
        auto* tier_item = make_item(QString::number(r.feed.tier));
        tier_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, kColTier, tier_item);
    }

    on_selection_changed();
    const int total = rows_.size();
    const int enabled = std::count_if(rows_.begin(), rows_.end(),
                                       [](const auto& r) { return r.enabled; });
    status_label_->setText(tr("%1 feeds (%2 enabled)").arg(total).arg(enabled));
}

int RssFeedManagerDialog::selected_row() const {
    const auto sel = table_->selectionModel()->selectedRows();
    return sel.isEmpty() ? -1 : sel.first().row();
}

void RssFeedManagerDialog::on_selection_changed() {
    const int row = selected_row();
    const bool has = row >= 0 && row < rows_.size();
    edit_btn_->setEnabled(has);
    toggle_btn_->setEnabled(has);
    delete_btn_->setEnabled(has);
    test_btn_->setEnabled(has);
    if (!has) {
        toggle_btn_->setText(tr("Disable"));
        delete_btn_->setText(tr("Delete"));
        return;
    }
    const auto& r = rows_[row];
    toggle_btn_->setText(r.enabled ? tr("Disable") : tr("Enable"));
    delete_btn_->setText(r.is_builtin ? tr("Reset to default") : tr("Delete"));
    delete_btn_->setEnabled(!r.is_builtin || r.is_customized || !r.enabled);
}

void RssFeedManagerDialog::on_add() {
    services::RSSFeed blank;
    blank.tier = 3;
    blank.category = "MARKETS";
    blank.region = "GLOBAL";
    RssFeedEditDialog dlg(blank, /*is_builtin_id=*/false, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto f = dlg.feed();
    if (!services::NewsService::instance().add_user_feed(f)) {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Could not save the feed. See log for details."));
        return;
    }
    dirty_ = true;
    reload_table();
}

void RssFeedManagerDialog::on_edit() {
    const int row = selected_row();
    if (row < 0)
        return;
    const auto& current = rows_[row];
    RssFeedEditDialog dlg(current.feed, /*is_builtin_id=*/current.is_builtin, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    auto f = dlg.feed();
    if (!services::NewsService::instance().update_feed(f, current.enabled)) {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Could not save the feed. See log for details."));
        return;
    }
    dirty_ = true;
    reload_table();
}

void RssFeedManagerDialog::on_delete_or_reset() {
    const int row = selected_row();
    if (row < 0)
        return;
    const auto& r = rows_[row];
    QString prompt;
    if (r.is_builtin) {
        prompt = tr("Reset built-in feed \"%1\" to its default settings? "
                    "Your edits will be lost.")
                     .arg(r.feed.name);
    } else {
        prompt = tr("Delete user feed \"%1\"?").arg(r.feed.name);
    }
    if (QMessageBox::question(this, tr("Confirm"), prompt,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!services::NewsService::instance().remove_or_reset_feed(r.feed.id)) {
        QMessageBox::warning(this, tr("Failed"),
                             tr("Could not remove the feed. See log for details."));
        return;
    }
    dirty_ = true;
    reload_table();
}

void RssFeedManagerDialog::on_toggle_enabled() {
    const int row = selected_row();
    if (row < 0)
        return;
    const auto& r = rows_[row];
    if (!services::NewsService::instance().set_feed_enabled(r.feed.id, !r.enabled)) {
        QMessageBox::warning(this, tr("Failed"),
                             tr("Could not toggle the feed. See log for details."));
        return;
    }
    dirty_ = true;
    reload_table();
}

void RssFeedManagerDialog::on_test_url() {
    const int row = selected_row();
    if (row < 0)
        return;
    const QString url = rows_[row].feed.url;
    if (url.isEmpty() || !QUrl(url).isValid()) {
        QMessageBox::warning(this, tr("Test"), tr("Feed URL is invalid."));
        return;
    }

    test_btn_->setEnabled(false);
    status_label_->setText(tr("Testing %1...").arg(rows_[row].feed.source));

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUserAgent);
    req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, */*");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(6000);

    QPointer<RssFeedManagerDialog> self = this;
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [self, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (!self)
            return;
        self->test_btn_->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QMessageBox::warning(self, tr("Test"),
                                  tr("Request failed: HTTP %1 — %2")
                                      .arg(http)
                                      .arg(reply->errorString()));
            self->status_label_->setText(tr("Test failed."));
            return;
        }
        const QByteArray data = reply->readAll();
        const QByteArray head = data.trimmed().left(256).toLower();
        if (head.startsWith("<?xml") || head.startsWith("<rss") ||
            head.startsWith("<feed") || head.startsWith("<rdf")) {
            QMessageBox::information(
                self, tr("Test"),
                tr("✓ Feed responded with %1 bytes of XML.").arg(data.size()));
            self->status_label_->setText(tr("Test OK."));
        } else if (head.contains("<html") || head.contains("<!doctype html")) {
            QMessageBox::warning(self, tr("Test"),
                                  tr("Server returned HTML — likely a block or login page."));
            self->status_label_->setText(tr("Test returned HTML."));
        } else {
            QMessageBox::warning(
                self, tr("Test"),
                tr("Response doesn't look like RSS/Atom XML (%1 bytes).").arg(data.size()));
            self->status_label_->setText(tr("Test unclear."));
        }
    });
}

} // namespace fincept::screens
