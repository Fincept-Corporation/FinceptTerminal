#include "screens/common/feeds/FeedConfigDialog.h"

#include "services/feeds/FeedMonitor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::feeds {

namespace {

// Best-guess semantic role for a discovered tag path.
QString guess_role(const QString& path) {
    const QString p = path.toLower();
    if (p.contains("title") || p == "headline" || p == "name")
        return "title";
    if (p.contains("desc") || p.contains("summary") || p.contains("abstract") || p.contains("content") ||
        p.contains("encoded"))
        return "summary";
    if (p == "link" || p.contains("url") || p.contains("href") || p == "guid" || p == "id")
        return "link";
    if (p.contains("pubdate") || p.contains("published") || p.contains("updated") || p == "date" ||
        p.contains("time") || p.contains("created"))
        return "time";
    return {};
}

// Human-friendly default column name from a tag path.
QString pretty_label(const QString& path) {
    QString s = path;
    const int at = s.indexOf('@');
    if (at > 0)
        s = s.left(at); // drop attribute suffix for the label
    const int dot = s.lastIndexOf('.');
    if (dot >= 0)
        s = s.mid(dot + 1);
    s.replace('_', ' ').replace('-', ' ');
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

} // namespace

FeedConfigDialog::FeedConfigDialog(const FeedSubscription& initial, QWidget* parent)
    : QDialog(parent), sub_(initial) {
    setWindowTitle(initial.id.isEmpty() ? tr("Add Feed") : tr("Edit Feed"));
    setMinimumWidth(520);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    name_ = new QLineEdit;
    url_ = new QLineEdit;
    url_->setPlaceholderText(QStringLiteral("https://example.com/feed.xml"));
    refresh_ = new QSpinBox;
    refresh_->setRange(15, 86400);
    refresh_->setSuffix(tr(" s"));
    refresh_->setValue(300);
    display_ = new QComboBox;
    display_->addItems({tr("Cards"), tr("Table")});
    parse_ = new QComboBox;
    parse_->addItems({tr("Auto"), tr("Manual")});
    format_ = new QComboBox;
    format_->addItems({"auto", "rss", "atom", "xml", "json", "csv"});
    persist_ = new QCheckBox(tr("Store history (offline cache + past items)"));
    persist_->setChecked(true);
    persist_->setToolTip(tr("Keep fetched items in the local database so they show when the feed is "
                            "unreachable, and can be queried later."));

    form->addRow(tr("Name"), name_);
    form->addRow(tr("URL"), url_);
    form->addRow(tr("Refresh"), refresh_);
    form->addRow(tr("Display"), display_);
    form->addRow(tr("Parse"), parse_);
    form->addRow(tr("Format"), format_);
    form->addRow(QString(), persist_);
    root->addLayout(form);

    // ── Manual-mapping section ───────────────────────────────────────────────
    manual_box_ = new QWidget;
    auto* mv = new QVBoxLayout(manual_box_);
    mv->setContentsMargins(0, 6, 0, 0);
    mv->setSpacing(6);

    auto* recRow = new QHBoxLayout;
    recRow->addWidget(new QLabel(tr("Records")));
    item_combo_ = new QComboBox;
    item_combo_->setEditable(true);
    item_combo_->setToolTip(tr("The repeating tag/element that marks one entry (auto-filled by Discover)."));
    recRow->addWidget(item_combo_, 1);
    mv->addLayout(recRow);

    auto* discRow = new QHBoxLayout;
    auto* discoverBtn = new QPushButton(tr("🔍 Discover fields from feed"));
    discoverBtn->setCursor(Qt::PointingHandCursor);
    discRow->addWidget(discoverBtn);
    discover_status_ = new QLabel;
    discover_status_->setStyleSheet("color:#888;");
    discRow->addWidget(discover_status_, 1);
    mv->addLayout(discRow);

    auto* hint = new QLabel(tr("Pick what each tag is, rename it, or remove it. Add your own with “+ Add field”."));
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#888;font-size:10px;");
    mv->addWidget(hint);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(160);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* fieldsHost = new QWidget;
    fields_layout_ = new QVBoxLayout(fieldsHost);
    fields_layout_->setContentsMargins(4, 4, 4, 4);
    fields_layout_->setSpacing(3);
    fields_layout_->addStretch(); // rows insert before this so they stay top-aligned
    scroll->setWidget(fieldsHost);
    mv->addWidget(scroll, 1);

    auto* addFieldBtn = new QPushButton(tr("+ Add field"));
    addFieldBtn->setCursor(Qt::PointingHandCursor);
    mv->addWidget(addFieldBtn, 0, Qt::AlignLeft);

    root->addWidget(manual_box_);

    // ── Test + buttons ───────────────────────────────────────────────────────
    auto* testBtn = new QPushButton(tr("Test / Preview"));
    test_result_ = new QLabel;
    test_result_->setWordWrap(true);
    root->addWidget(testBtn);
    root->addWidget(test_result_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (!initial.id.isEmpty()) { // existing feed -> offer Delete
        auto* del = bb->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
        del->setCursor(Qt::PointingHandCursor);
        connect(del, &QPushButton::clicked, this, [this]() {
            const auto r = QMessageBox::question(
                this, tr("Delete feed"),
                tr("Delete this feed and its stored history? This cannot be undone."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r == QMessageBox::Yes) {
                delete_requested_ = true;
                accept();
            }
        });
    }
    root->addWidget(bb);

    apply(initial);
    update_manual_visibility();
    connect(parse_, &QComboBox::currentIndexChanged, this, [this]() { update_manual_visibility(); });
    connect(discoverBtn, &QPushButton::clicked, this, &FeedConfigDialog::on_discover);
    connect(addFieldBtn, &QPushButton::clicked, this, [this]() { add_field_row({}); });
    connect(testBtn, &QPushButton::clicked, this, &FeedConfigDialog::on_test);
    connect(bb, &QDialogButtonBox::accepted, this, [this]() {
        sub_ = collect();
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FeedConfigDialog::update_manual_visibility() {
    manual_box_->setVisible(parse_->currentIndex() == 1);
}

QWidget* FeedConfigDialog::add_field_row(const MappedField& f) {
    auto* row = new QWidget;
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(4);

    auto* role = new QComboBox;
    role->setObjectName("role");
    role->addItem(tr("Column"), "");
    role->addItem(tr("Title"), "title");
    role->addItem(tr("Summary"), "summary");
    role->addItem(tr("Link"), "link");
    role->addItem(tr("Time"), "time");
    const int ri = role->findData(f.role);
    role->setCurrentIndex(ri < 0 ? 0 : ri);
    role->setFixedWidth(92);
    role->setToolTip(tr("Title/Summary/Link/Time feed the card view; Column shows only in the table."));
    hl->addWidget(role);

    auto* label = new QLineEdit(f.label);
    label->setObjectName("label");
    label->setPlaceholderText(tr("Display name"));
    hl->addWidget(label, 1);

    auto* source = new QComboBox;
    source->setObjectName("source");
    source->setEditable(true);
    source->addItems(discovered_paths_);
    if (!f.path.isEmpty() && source->findText(f.path) < 0)
        source->insertItem(0, f.path);
    source->setCurrentText(f.path);
    source->setToolTip(tr("Which feed tag to read this from."));
    hl->addWidget(source, 1);

    auto* rm = new QToolButton;
    rm->setText(QStringLiteral("✕"));
    rm->setToolTip(tr("Remove"));
    rm->setAutoRaise(true);
    rm->setCursor(Qt::PointingHandCursor);
    hl->addWidget(rm);

    // Insert before the trailing stretch so rows stay top-aligned.
    fields_layout_->insertWidget(fields_layout_->count() - 1, row);
    field_rows_.append(row);
    connect(rm, &QToolButton::clicked, this, [this, row]() {
        field_rows_.removeOne(row);
        row->deleteLater();
    });
    return row;
}

void FeedConfigDialog::clear_field_rows() {
    for (QWidget* row : field_rows_)
        row->deleteLater();
    field_rows_.clear();
}

void FeedConfigDialog::apply(const FeedSubscription& s) {
    name_->setText(s.name);
    url_->setText(s.url);
    refresh_->setValue(qMax(15, s.refresh_sec));
    display_->setCurrentIndex(s.display_mode == DisplayMode::Table ? 1 : 0);
    parse_->setCurrentIndex(s.parse_mode == ParseMode::Manual ? 1 : 0);
    format_->setCurrentText(to_token(s.format));
    persist_->setChecked(s.persist);
    item_combo_->setCurrentText(s.mapping.item_selector);

    discovered_paths_.clear();
    for (const auto& f : s.mapping.fields)
        if (!f.path.isEmpty() && !discovered_paths_.contains(f.path))
            discovered_paths_ << f.path;
    clear_field_rows();
    for (const auto& f : s.mapping.fields)
        add_field_row(f);
}

FeedSubscription FeedConfigDialog::collect() const {
    FeedSubscription s = sub_;
    if (s.id.isEmpty())
        s.id = "feed-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = name_->text().trimmed();
    s.url = url_->text().trimmed();
    s.refresh_sec = refresh_->value();
    s.display_mode = display_->currentIndex() == 1 ? DisplayMode::Table : DisplayMode::Cards;
    s.parse_mode = parse_->currentIndex() == 1 ? ParseMode::Manual : ParseMode::Auto;
    s.format = feed_format_from(format_->currentText());
    s.persist = persist_->isChecked();

    FieldMapping m;
    m.item_selector = item_combo_->currentText().trimmed();
    for (QWidget* row : field_rows_) {
        auto* source = row->findChild<QComboBox*>("source");
        if (!source)
            continue;
        const QString path = source->currentText().trimmed();
        if (path.isEmpty())
            continue;
        auto* role = row->findChild<QComboBox*>("role");
        auto* label = row->findChild<QLineEdit*>("label");
        MappedField f;
        f.path = path;
        f.role = role ? role->currentData().toString() : QString();
        f.label = label ? label->text().trimmed() : QString();
        if (f.label.isEmpty())
            f.label = pretty_label(path);
        m.fields.append(f);
    }
    s.mapping = m;
    s.enabled = true;
    if (s.name.isEmpty())
        s.name = s.url;
    return s;
}

void FeedConfigDialog::on_discover() {
    discover_status_->setText(tr("Discovering…"));
    QPointer<FeedConfigDialog> self(this);
    FeedMonitor::instance().discover(collect(), [self](bool ok, const QString& msg, const DiscoveredSchema& schema) {
        if (!self)
            return;
        self->discover_status_->setText((ok ? QStringLiteral("✓ ") : QStringLiteral("✗ ")) + msg);
        if (ok)
            self->apply_discovery(schema);
    });
}

void FeedConfigDialog::apply_discovery(const DiscoveredSchema& schema) {
    // Repeating-record selector.
    const QString keepItem = item_combo_->currentText();
    item_combo_->clear();
    item_combo_->addItems(schema.item_options);
    const QString chosen = schema.item_selector.isEmpty() ? keepItem : schema.item_selector;
    if (!chosen.isEmpty()) {
        if (item_combo_->findText(chosen) < 0)
            item_combo_->insertItem(0, chosen);
        item_combo_->setCurrentText(chosen);
    }

    // Available tags for every row's dropdown.
    discovered_paths_.clear();
    for (const auto& d : schema.fields)
        discovered_paths_ << d.path;

    // Rebuild rows with smart defaults; show a sample value as the source tooltip.
    clear_field_rows();
    for (const auto& d : schema.fields) {
        MappedField f;
        f.path = d.path;
        f.role = guess_role(d.path);
        f.label = pretty_label(d.path);
        QWidget* row = add_field_row(f);
        if (auto* src = row->findChild<QComboBox*>("source"))
            if (!d.sample.isEmpty())
                src->setToolTip(tr("e.g. %1").arg(d.sample));
    }

    // Reflect the detected format so parsing matches.
    if (!schema.format.isEmpty()) {
        const int fi = format_->findText(schema.format);
        if (fi >= 0)
            format_->setCurrentIndex(fi);
    }
}

void FeedConfigDialog::on_test() {
    test_result_->setText(tr("Testing…"));
    FeedMonitor::instance().fetch_once(collect(), [this](bool ok, const QString& msg, const QVector<FeedItem>& items) {
        QString detail = msg;
        for (int i = 0; i < items.size() && i < 3; ++i)
            detail += "\n• " + (items[i].title.isEmpty() ? items[i].link : items[i].title);
        test_result_->setText((ok ? QStringLiteral("✓ ") : QStringLiteral("✗ ")) + detail);
    });
}

} // namespace fincept::feeds
