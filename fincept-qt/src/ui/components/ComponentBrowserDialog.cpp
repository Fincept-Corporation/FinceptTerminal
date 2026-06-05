#include "ui/components/ComponentBrowserDialog.h"

#include "core/components/ComponentCatalog.h"
#include "core/components/PopularityTracker.h"
#include "ui/components/ComponentCard.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
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
    setWindowTitle(tr("Component Browser"));
    setModal(true);
    resize(960, 640);
    setStyleSheet(QString("QDialog{background:%1;font-family:%2;} QLabel{color:%3;}")
                      .arg(colors::BG_BASE())
                      .arg(fonts::DATA_FAMILY())
                      .arg(colors::TEXT_PRIMARY()));
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
    header->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(colors::BG_RAISED())
                              .arg(colors::BORDER_MED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(12);

    title_label_ = new QLabel(tr("COMPONENT BROWSER"), header);
    title_label_->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:700;letter-spacing:2px;").arg(colors::AMBER()));
    hl->addWidget(title_label_);

    hl->addSpacing(24);

    search_ = new QLineEdit(header);
    search_->setPlaceholderText(tr("Search components…"));
    search_->setClearButtonEnabled(true);
    search_->setStyleSheet(
        QString("QLineEdit{background:%1;border:1px solid %2;border-radius:2px;"
                "color:%3;padding:6px 10px;min-width:280px;}"
                "QLineEdit:focus{border-color:%4;}")
            .arg(colors::BG_SURFACE())
            .arg(colors::BORDER_MED())
            .arg(colors::TEXT_PRIMARY())
            .arg(colors::AMBER()));
    connect(search_, &QLineEdit::textChanged, this, &ComponentBrowserDialog::on_search_changed);
    hl->addWidget(search_);

    hl->addStretch(1);

    count_label_ = new QLabel(header);
    count_label_->setStyleSheet(QString("color:%1;font-size:11px;").arg(colors::TEXT_SECONDARY()));
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
        QString("QListWidget{background:%1;border:none;border-right:1px solid %2;color:%3;}"
                "QListWidget::item{padding:8px 14px;border:none;}"
                "QListWidget::item:selected{background:%4;color:%5;border-left:3px solid %5;}"
                "QListWidget::item:hover{background:%6;}")
            .arg(colors::BG_SURFACE())
            .arg(colors::BORDER_MED())
            .arg(colors::TEXT_SECONDARY())
            .arg(colors::BG_HOVER())
            .arg(colors::AMBER())
            .arg(colors::BG_RAISED()));
    // Row 0 is the "All" sentinel: translated display text, empty UserRole.
    // Other rows store the real (non-translated) category id in UserRole so
    // filtering stays stable regardless of the active locale.
    auto* all_item = new QListWidgetItem(tr("All"), category_list_);
    all_item->setData(Qt::UserRole, QString());
    for (const QString& c : ComponentCatalog::instance().categories()) {
        auto* item = new QListWidgetItem(c, category_list_);
        item->setData(Qt::UserRole, c);
    }
    category_list_->setCurrentRow(0);
    connect(category_list_, &QListWidget::currentRowChanged, this,
            &ComponentBrowserDialog::on_category_changed);
    sl->addWidget(category_list_);

    scroll_ = new QScrollArea(split);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));
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
        // Empty active_category_ means "All" — show every category.
        if (!active_category_.isEmpty() && m.category != active_category_)
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

    // %n drives Qt's locale-aware plural handling (singular/plural per language).
    count_label_->setText(tr("%n component(s)", "", shown));
}

void ComponentBrowserDialog::on_search_changed(const QString& query) {
    search_query_ = query.trimmed();
    rebuild_cards();
}

void ComponentBrowserDialog::on_category_changed(int row) {
    if (row < 0)
        return;
    // Read the stable category id from UserRole, not the (translatable) text.
    active_category_ = category_list_->item(row)->data(Qt::UserRole).toString();
    rebuild_cards();
}

void ComponentBrowserDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void ComponentBrowserDialog::retranslateUi() {
    setWindowTitle(tr("Component Browser"));
    if (title_label_) title_label_->setText(tr("COMPONENT BROWSER"));
    if (search_)      search_->setPlaceholderText(tr("Search components…"));
    // Row 0 is the "All" sentinel — only its display text is translatable.
    // Category-id rows carry data values (their UserRole id) and are left as-is.
    if (category_list_ && category_list_->count() > 0)
        category_list_->item(0)->setText(tr("All"));
    // The count label embeds the live result count — rebuild_cards() re-applies
    // it with the active translator.
    rebuild_cards();
}

} // namespace fincept::ui
