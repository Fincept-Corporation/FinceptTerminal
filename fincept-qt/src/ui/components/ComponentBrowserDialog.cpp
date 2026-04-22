#include "ui/components/ComponentBrowserDialog.h"

#include "core/components/ComponentCatalog.h"
#include "core/components/PopularityTracker.h"
#include "ui/components/ComponentCard.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::ui {

namespace {

constexpr int kCardsPerRow = 3;

bool matches_query(const ComponentMeta& m, const QString& query) {
    if (query.isEmpty())
        return true;
    const QString q = query.toLower();
    if (m.title.toLower().contains(q)) return true;
    if (m.description.toLower().contains(q)) return true;
    if (m.id.toLower().contains(q)) return true;
    for (const QString& tag : m.tags) {
        if (tag.toLower().contains(q)) return true;
    }
    return false;
}

} // namespace

ComponentBrowserDialog::ComponentBrowserDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Component Browser");
    setModal(true);
    resize(960, 640);
    setStyleSheet("QDialog{background:#0f172a;} QLabel{color:#e5e7eb;}");
    build_ui();
    rebuild_cards();
}

void ComponentBrowserDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar — title + search.
    auto* header = new QWidget(this);
    header->setFixedHeight(56);
    header->setStyleSheet("background:#111827;border-bottom:1px solid #374151;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(12);

    auto* title = new QLabel("COMPONENT BROWSER", header);
    title->setStyleSheet("color:#d97706;font-size:13px;font-weight:700;letter-spacing:2px;");
    hl->addWidget(title);

    hl->addSpacing(24);

    search_ = new QLineEdit(header);
    search_->setPlaceholderText("Search components…");
    search_->setClearButtonEnabled(true);
    search_->setStyleSheet(
        "QLineEdit{background:#0b1220;border:1px solid #374151;border-radius:3px;"
        "color:#f3f4f6;padding:6px 10px;min-width:280px;}"
        "QLineEdit:focus{border-color:#d97706;}");
    connect(search_, &QLineEdit::textChanged, this, &ComponentBrowserDialog::on_search_changed);
    hl->addWidget(search_);

    hl->addStretch(1);

    count_label_ = new QLabel(header);
    count_label_->setStyleSheet("color:#9ca3af;font-size:11px;");
    hl->addWidget(count_label_);

    root->addWidget(header);

    // Split: sidebar (categories) + grid.
    auto* split = new QWidget(this);
    auto* sl = new QHBoxLayout(split);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);

    category_list_ = new QListWidget(split);
    category_list_->setFixedWidth(180);
    category_list_->setStyleSheet(
        "QListWidget{background:#0b1220;border:none;border-right:1px solid #374151;color:#e5e7eb;}"
        "QListWidget::item{padding:8px 14px;border:none;}"
        "QListWidget::item:selected{background:#1f2937;color:#d97706;border-left:3px solid #d97706;}"
        "QListWidget::item:hover{background:#111827;}");
    category_list_->addItem("All");
    for (const QString& c : ComponentCatalog::instance().categories())
        category_list_->addItem(c);
    category_list_->setCurrentRow(0);
    connect(category_list_, &QListWidget::currentRowChanged, this,
            &ComponentBrowserDialog::on_category_changed);
    sl->addWidget(category_list_);

    scroll_ = new QScrollArea(split);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setStyleSheet("background:#0f172a;");
    grid_host_ = new QWidget;
    grid_host_->setStyleSheet("background:transparent;");
    grid_layout_ = new QGridLayout(grid_host_);
    grid_layout_->setContentsMargins(16, 16, 16, 16);
    grid_layout_->setSpacing(12);
    scroll_->setWidget(grid_host_);
    sl->addWidget(scroll_, 1);

    root->addWidget(split, 1);
}

void ComponentBrowserDialog::rebuild_cards() {
    // Tear down existing cards.
    while (grid_layout_->count() > 0) {
        QLayoutItem* it = grid_layout_->takeAt(0);
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }

    const auto all = ComponentCatalog::instance().list_for_ui();
    int row = 0, col = 0;
    int shown = 0;
    for (const ComponentMeta& m : all) {
        if (active_category_ != "All" && m.category != active_category_)
            continue;
        if (!matches_query(m, search_query_))
            continue;

        auto* card = new ComponentCard(m, grid_host_);
        connect(card, &ComponentCard::activated, this, [this](const QString& id) {
            emit component_chosen(id);
            accept();
        });
        connect(card, &ComponentCard::selected_changed, this, [this](const QString& id) {
            selected_id_ = id;
        });
        grid_layout_->addWidget(card, row, col);
        if (++col >= kCardsPerRow) {
            col = 0;
            ++row;
        }
        ++shown;
    }

    // Pad the last row so cards don't stretch to fill when there's an
    // awkward remainder (e.g. 7 cards on a 3-wide grid).
    if (col != 0) {
        for (int c = col; c < kCardsPerRow; ++c)
            grid_layout_->addWidget(new QWidget(grid_host_), row, c);
    }
    grid_layout_->setRowStretch(row + 1, 1);

    count_label_->setText(QString("%1 component%2").arg(shown).arg(shown == 1 ? "" : "s"));
}

void ComponentBrowserDialog::on_search_changed(const QString& query) {
    search_query_ = query.trimmed();
    rebuild_cards();
}

void ComponentBrowserDialog::on_category_changed(int row) {
    if (row < 0)
        return;
    active_category_ = category_list_->item(row)->text();
    rebuild_cards();
}

} // namespace fincept::ui
