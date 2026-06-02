#include "screens/news/dialogs/RssFeedEditDialog.h"

#include "core/logging/Logger.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

constexpr const char* kBrowserUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

constexpr int kTestTimeoutMs = 6000;

QStringList default_categories() {
    return {"MARKETS", "GEOPOLITICS", "REGULATORY", "ECONOMIC", "ENERGY",
            "CRYPTO",  "TECH",        "DEFENSE",    "EARNINGS"};
}

QStringList default_regions() {
    return {"GLOBAL", "US", "EU", "UK", "ASIA", "INDIA", "CHINA", "JAPAN", "ME"};
}

} // anonymous namespace

RssFeedEditDialog::RssFeedEditDialog(const services::RSSFeed& initial, bool is_builtin_id, QWidget* parent)
    : QDialog(parent), initial_(initial), is_builtin_id_(is_builtin_id) {
    setWindowTitle(initial.id.isEmpty() ? tr("Add RSS Feed") : tr("Edit RSS Feed"));
    setModal(true);
    resize(560, 0);
    build_ui();
}

void RssFeedEditDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void RssFeedEditDialog::retranslateUi() {
    setWindowTitle(initial_.id.isEmpty() ? tr("Add RSS Feed") : tr("Edit RSS Feed"));

    // QFormLayout owns the row caption labels — re-apply via labelForField.
    auto set_label = [this](QWidget* field, const QString& text) {
        if (!form_ || !field)
            return;
        if (auto* lbl = qobject_cast<QLabel*>(form_->labelForField(field)))
            lbl->setText(text);
    };
    set_label(id_input_, tr("ID"));
    set_label(source_input_, tr("Source"));
    set_label(name_input_, tr("Name"));
    set_label(url_input_, tr("URL"));
    set_label(category_combo_, tr("Category"));
    set_label(region_combo_, tr("Region"));
    set_label(tier_spin_, tr("Tier"));

    if (id_input_ && is_builtin_id_)
        id_input_->setToolTip(tr("Built-in feed ID is fixed."));
    if (source_input_) source_input_->setPlaceholderText(tr("e.g. BLOOMBERG"));
    if (name_input_)   name_input_->setPlaceholderText(tr("e.g. Bloomberg Markets"));
    if (tier_spin_)    tier_spin_->setToolTip(tr("1=wire, 2=major, 3=specialty, 4=blog"));

    if (test_btn_)   test_btn_->setText(tr("Test URL"));
    if (cancel_btn_) cancel_btn_->setText(tr("Cancel"));
    if (ok_btn_)     ok_btn_->setText(tr("Save"));
    // test_status_ reflects the last async test result — refreshed on next test.
}

void RssFeedEditDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(10);

    form_ = new QFormLayout();
    QFormLayout* form = form_;
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    id_input_ = new QLineEdit(this);
    id_input_->setObjectName("rssFeedEditId");
    if (initial_.id.isEmpty()) {
        // New feed — auto-generate a user prefix id; user can override.
        const QString uid =
            QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        id_input_->setText("usr-" + uid);
    } else {
        id_input_->setText(initial_.id);
    }
    if (is_builtin_id_) {
        id_input_->setReadOnly(true);
        id_input_->setToolTip(tr("Built-in feed ID is fixed."));
    }
    form->addRow(tr("ID"), id_input_);

    source_input_ = new QLineEdit(initial_.source, this);
    source_input_->setPlaceholderText(tr("e.g. BLOOMBERG"));
    form->addRow(tr("Source"), source_input_);

    name_input_ = new QLineEdit(initial_.name, this);
    name_input_->setPlaceholderText(tr("e.g. Bloomberg Markets"));
    form->addRow(tr("Name"), name_input_);

    url_input_ = new QLineEdit(initial_.url, this);
    url_input_->setPlaceholderText("https://example.com/rss.xml");
    form->addRow(tr("URL"), url_input_);

    category_combo_ = new QComboBox(this);
    category_combo_->setEditable(true);
    category_combo_->addItems(default_categories());
    if (!initial_.category.isEmpty()) {
        int idx = category_combo_->findText(initial_.category);
        if (idx >= 0)
            category_combo_->setCurrentIndex(idx);
        else
            category_combo_->setEditText(initial_.category);
    }
    form->addRow(tr("Category"), category_combo_);

    region_combo_ = new QComboBox(this);
    region_combo_->setEditable(true);
    region_combo_->addItems(default_regions());
    if (!initial_.region.isEmpty()) {
        int idx = region_combo_->findText(initial_.region);
        if (idx >= 0)
            region_combo_->setCurrentIndex(idx);
        else
            region_combo_->setEditText(initial_.region);
    }
    form->addRow(tr("Region"), region_combo_);

    tier_spin_ = new QSpinBox(this);
    tier_spin_->setRange(1, 4);
    tier_spin_->setValue(initial_.tier > 0 ? initial_.tier : 3);
    tier_spin_->setToolTip(tr("1=wire, 2=major, 3=specialty, 4=blog"));
    form->addRow(tr("Tier"), tier_spin_);

    root->addLayout(form);

    // Test status line
    test_status_ = new QLabel(this);
    test_status_->setObjectName("rssFeedEditTestStatus");
    test_status_->setWordWrap(true);
    test_status_->hide();
    root->addWidget(test_status_);

    // Buttons
    auto* btn_row = new QHBoxLayout();
    test_btn_ = new QPushButton(tr("Test URL"), this);
    test_btn_->setCursor(Qt::PointingHandCursor);
    btn_row->addWidget(test_btn_);
    btn_row->addStretch();
    cancel_btn_ = new QPushButton(tr("Cancel"), this);
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    btn_row->addWidget(cancel_btn_);
    ok_btn_ = new QPushButton(tr("Save"), this);
    ok_btn_->setDefault(true);
    ok_btn_->setCursor(Qt::PointingHandCursor);
    btn_row->addWidget(ok_btn_);
    root->addLayout(btn_row);

    connect(test_btn_, &QPushButton::clicked, this, &RssFeedEditDialog::on_test);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok_btn_, &QPushButton::clicked, this, &RssFeedEditDialog::try_accept);
}

services::RSSFeed RssFeedEditDialog::feed() const {
    services::RSSFeed f;
    f.id = id_input_->text().trimmed();
    f.name = name_input_->text().trimmed();
    f.url = url_input_->text().trimmed();
    f.source = source_input_->text().trimmed();
    f.category = category_combo_->currentText().trimmed().toUpper();
    f.region = region_combo_->currentText().trimmed().toUpper();
    f.tier = tier_spin_->value();
    return f;
}

void RssFeedEditDialog::on_test() {
    const QString url = url_input_->text().trimmed();
    if (url.isEmpty() || !QUrl(url).isValid()) {
        test_status_->setText(tr("✗ Enter a valid URL first."));
        test_status_->setStyleSheet("color:#dc2626;");
        test_status_->show();
        return;
    }
    test_btn_->setEnabled(false);
    test_btn_->setText(tr("Testing..."));
    test_status_->setText(tr("Fetching..."));
    test_status_->setStyleSheet("color:#6b7280;");
    test_status_->show();

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUserAgent);
    req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, */*");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(kTestTimeoutMs);

    QPointer<RssFeedEditDialog> self = this;
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [self, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (!self)
            return;

        self->test_btn_->setEnabled(true);
        self->test_btn_->setText(tr("Test URL"));
        self->test_run_ = true;

        if (reply->error() != QNetworkReply::NoError) {
            const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            self->last_test_ok_ = false;
            self->test_status_->setText(
                tr("✗ Request failed: HTTP %1 — %2").arg(http).arg(reply->errorString()));
            self->test_status_->setStyleSheet("color:#dc2626;");
            return;
        }

        const QByteArray data = reply->readAll();
        const QByteArray trimmed = data.trimmed();
        const QByteArray head = trimmed.left(256).toLower();
        const bool is_xml = head.startsWith("<?xml") || head.startsWith("<rss") ||
                            head.startsWith("<feed") || head.startsWith("<rdf");
        const bool is_html =
            head.contains("<html") || head.contains("<!doctype html");

        if (is_xml) {
            self->last_test_ok_ = true;
            self->test_status_->setText(
                tr("✓ Looks like a valid RSS/Atom feed (%1 bytes).").arg(data.size()));
            self->test_status_->setStyleSheet("color:#16a34a;");
        } else if (is_html) {
            self->last_test_ok_ = false;
            self->test_status_->setText(
                tr("✗ Server returned HTML — likely a login or block page, not RSS."));
            self->test_status_->setStyleSheet("color:#dc2626;");
        } else {
            self->last_test_ok_ = false;
            self->test_status_->setText(
                tr("⚠ Response doesn't look like RSS/Atom XML (%1 bytes).").arg(data.size()));
            self->test_status_->setStyleSheet("color:#d97706;");
        }
    });
}

void RssFeedEditDialog::try_accept() {
    auto f = feed();
    QStringList missing;
    if (f.id.isEmpty())
        missing << tr("ID");
    if (f.name.isEmpty())
        missing << tr("Name");
    if (f.url.isEmpty())
        missing << tr("URL");
    if (f.source.isEmpty())
        missing << tr("Source");
    if (!missing.isEmpty()) {
        QMessageBox::warning(this, tr("Missing fields"),
                             tr("Please fill in: %1").arg(missing.join(", ")));
        return;
    }
    if (!QUrl(f.url).isValid() || !(f.url.startsWith("http://") || f.url.startsWith("https://"))) {
        QMessageBox::warning(this, tr("Invalid URL"),
                             tr("URL must start with http:// or https://."));
        return;
    }

    if (!test_run_) {
        // Run a quick blocking-feel test before save (still async — chain to
        // accept on success).
        test_btn_->setEnabled(false);
        ok_btn_->setEnabled(false);
        test_status_->setText(tr("Validating URL before save..."));
        test_status_->setStyleSheet("color:#6b7280;");
        test_status_->show();

        auto* nam = new QNetworkAccessManager(this);
        QNetworkRequest req((QUrl(f.url)));
        req.setHeader(QNetworkRequest::UserAgentHeader, kBrowserUserAgent);
        req.setRawHeader("Accept", "application/rss+xml, application/xml, text/xml, */*");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(kTestTimeoutMs);
        QPointer<RssFeedEditDialog> self = this;
        auto* reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [self, reply, nam]() {
            reply->deleteLater();
            nam->deleteLater();
            if (!self)
                return;
            self->test_btn_->setEnabled(true);
            self->ok_btn_->setEnabled(true);
            self->test_run_ = true;

            bool ok = false;
            QString msg;
            if (reply->error() != QNetworkReply::NoError) {
                msg = tr("Request failed: %1").arg(reply->errorString());
            } else {
                const QByteArray head = reply->readAll().trimmed().left(256).toLower();
                if (head.startsWith("<?xml") || head.startsWith("<rss") ||
                    head.startsWith("<feed") || head.startsWith("<rdf")) {
                    ok = true;
                } else if (head.contains("<html") || head.contains("<!doctype html")) {
                    msg = tr("Server returned HTML, not RSS.");
                } else {
                    msg = tr("Response doesn't look like RSS/Atom XML.");
                }
            }
            self->last_test_ok_ = ok;
            if (ok) {
                self->accept();
                return;
            }
            self->test_status_->setText(tr("⚠ %1 Save anyway?").arg(msg));
            self->test_status_->setStyleSheet("color:#d97706;");
            auto choice = QMessageBox::question(
                self, tr("URL didn't validate"),
                tr("%1\n\nSave the feed anyway?").arg(msg),
                QMessageBox::Save | QMessageBox::Cancel);
            if (choice == QMessageBox::Save)
                self->accept();
        });
        return;
    }

    if (!last_test_ok_) {
        auto choice = QMessageBox::question(
            this, tr("URL didn't validate"),
            tr("The last URL test didn't return valid RSS. Save anyway?"),
            QMessageBox::Save | QMessageBox::Cancel);
        if (choice != QMessageBox::Save)
            return;
    }
    accept();
}

} // namespace fincept::screens
