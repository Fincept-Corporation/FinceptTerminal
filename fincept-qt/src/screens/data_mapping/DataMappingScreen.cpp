#include "screens/data_mapping/DataMappingScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/data_normalization/DataNormalizationService.h"
#include "storage/repositories/DataMappingRepository.h"
#include "ui/theme/Theme.h"
#include "screens/data_mapping/DataMappingScreen_internal.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>

namespace {
using namespace fincept::ui;
static const QString kStyle =
    QStringLiteral("#dmScreen { background: %1; }"

                   "#dmHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#dmHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#dmHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
                   "#dmHeaderBadge { color: %6; font-size: 8px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

                   "#dmViewBtn { background: transparent; color: %5; border: 1px solid %8; "
                   "  font-size: 9px; font-weight: 700; padding: 4px 12px; }"
                   "#dmViewBtn:hover { color: %4; }"
                   "#dmViewBtn[active=\"true\"] { background: %3; color: %1; border-color: %3; }"

                   "#dmStepBar { background: %7; border-bottom: 1px solid %8; }"
                   "#dmStepBtn { background: transparent; color: %5; border: none; "
                   "  font-size: 9px; font-weight: 700; padding: 6px 14px; border-bottom: 2px solid transparent; }"
                   "#dmStepBtn:hover { color: %4; }"
                   "#dmStepBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"
                   "#dmStepBtn[complete=\"true\"] { color: %6; }"

                   "#dmLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#dmRightPanel { background: %7; border-left: 1px solid %8; }"

                   "#dmPanelTitle { color: %5; font-size: 10px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; padding: 8px; "
                   "  border-bottom: 1px solid %8; }"

                   "#dmPanel { background: %7; border: 1px solid %8; }"
                   "#dmPanelHeader { background: %2; border-bottom: 1px solid %8; }"
                   "#dmPanelHeaderTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#dmPanelHeaderIcon { color: %3; font-size: 13px; background: transparent; }"

                   "#dmLabel { color: %5; font-size: 9px; font-weight: 700; "
                   "  letter-spacing: 0.5px; background: transparent; }"

                   "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QLineEdit:focus { border-color: %9; }"
                   "QPlainTextEdit { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                   "  selection-background-color: %3; }"
                   "QSpinBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "QSpinBox::up-button, QSpinBox::down-button { width: 0; }"

                   "#dmCalcBtn { background: %3; color: %1; border: none; padding: 5px 16px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#dmCalcBtn:hover { background: #FF8800; }"
                   "#dmCalcBtn:disabled { background: %10; color: %11; }"
                   "#dmSecondaryBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 5px 16px; font-size: 9px; font-weight: 700; }"
                   "#dmSecondaryBtn:hover { color: %4; }"
                   "#dmSaveBtn { background: %6; color: %1; border: none; padding: 6px 20px; "
                   "  font-size: 10px; font-weight: 700; }"
                   "#dmSaveBtn:hover { background: %6; }"
                   "#dmSaveBtn:disabled { background: %10; color: %11; }"

                   "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: 11px; }"
                   "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

                   "QTreeWidget { background: %1; color: %4; border: none; font-size: 11px; }"
                   "QTreeWidget::item { padding: 2px 4px; }"
                   "QTreeWidget::item:selected { background: rgba(217,119,6,0.1); color: %3; }"

                   "QListWidget { background: %1; border: none; font-size: 11px; }"
                   "QListWidget::item { color: %5; padding: 6px 10px; border-bottom: 1px solid %8; }"
                   "QListWidget::item:hover { color: %4; background: %12; }"
                   "QListWidget::item:selected { color: %3; background: rgba(217,119,6,0.1); }"

                   "QTextEdit { background: %1; color: %13; border: none; font-size: 11px; }"

                   "#dmInfoLabel { color: %5; font-size: 9px; background: transparent; }"
                   "#dmInfoValue { color: %13; font-size: 11px; font-weight: 700; background: transparent; }"
                   "#dmSuccessBadge { color: %6; font-size: 9px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.15); padding: 2px 6px; }"
                   "#dmErrorBadge { color: %14; font-size: 9px; font-weight: 700; "
                   "  background: rgba(220,38,38,0.15); padding: 2px 6px; }"

                   "#dmNavFooter { background: %2; border-top: 1px solid %8; }"
                   "#dmStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#dmStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#dmStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

                   "#dmEmptyState { color: %11; font-size: 13px; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollArea { background: %1; border: none; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

using namespace fincept::screens::data_mapping_internal;


DataMappingScreen::DataMappingScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("dmScreen");
    setStyleSheet(kStyle);
    setup_ui();
    LOG_INFO("DataMapping", "Screen constructed");
}


void DataMappingScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());
    root->addWidget(create_step_bar());
    root->addWidget(create_main_area(), 1);

    nav_footer_ = create_nav_footer();
    nav_footer_->hide();
    root->addWidget(nav_footer_);

    root->addWidget(create_status_bar());

    on_view_changed(0);
}

void DataMappingScreen::update_step_indicators() {
    const QStringList labels = {"API CONFIG", "SCHEMA", "FIELD MAPPING", "CACHE", "TEST & SAVE"};
    for (int i = 0; i < step_btns_.size(); ++i) {
        step_btns_[i]->setProperty("active", i == current_step_);
        step_btns_[i]->setProperty("complete", i < current_step_);
        step_btns_[i]->style()->unpolish(step_btns_[i]);
        step_btns_[i]->style()->polish(step_btns_[i]);
    }
    step_label_->setText(QString("Step %1 of 5 — %2").arg(current_step_ + 1).arg(labels[current_step_]));
    prev_btn_->setEnabled(current_step_ > 0);
    next_btn_->setEnabled(current_step_ < 4);
    status_step_->setText(labels[current_step_]);
}

void DataMappingScreen::populate_json_tree(const QJsonValue& val, QTreeWidgetItem* parent, const QString& key) {
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(json_tree_);
    item->setText(0, key);

    if (val.isObject()) {
        item->setText(2, "object");
        auto obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            populate_json_tree(it.value(), item, it.key());
        }
    } else if (val.isArray()) {
        auto arr = val.toArray();
        item->setText(2, QString("array[%1]").arg(arr.size()));
        int limit = qMin(arr.size(), 10); // limit display
        for (int i = 0; i < limit; ++i) {
            populate_json_tree(arr[i], item, QString("[%1]").arg(i));
        }
    } else if (val.isDouble()) {
        item->setText(1, QString::number(val.toDouble(), 'g', 10));
        item->setText(2, "number");
        item->setForeground(1, QColor(colors::CYAN()));
    } else if (val.isBool()) {
        item->setText(1, val.toBool() ? "true" : "false");
        item->setText(2, "boolean");
        item->setForeground(1, QColor(colors::WARNING()));
    } else if (val.isNull()) {
        item->setText(1, "null");
        item->setText(2, "null");
        item->setForeground(1, QColor(colors::TEXT_DIM()));
    } else {
        item->setText(1, val.toString());
        item->setText(2, "string");
    }
}

void DataMappingScreen::populate_mapping_list() {
    int schema_idx = schema_select_->currentIndex();
    if (schema_idx < 0 || schema_idx >= schemas().size())
        return;

    const auto& fields = schemas()[schema_idx].fields;
    mapping_table_->setRowCount(fields.size());
    for (int i = 0; i < fields.size(); ++i) {
        auto* name_item = new QTableWidgetItem(fields[i].name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        mapping_table_->setItem(i, 0, name_item);
        mapping_table_->setItem(i, 1, new QTableWidgetItem("")); // expression
        mapping_table_->setItem(i, 2, new QTableWidgetItem("")); // transform
        mapping_table_->setItem(i, 3, new QTableWidgetItem("")); // default
    }
}


void DataMappingScreen::on_view_changed(int view) {
    current_view_ = view;
    view_stack_->setCurrentIndex(view);

    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }

    const QStringList names = {"MAPPINGS", "TEMPLATES", "CREATE"};
    status_view_->setText("VIEW: " + names[view]);

    // CREATE is index 2 — show step bar + nav footer only there.
    nav_footer_->setVisible(view == 2); // only in create mode

    if (view == 2) {
        update_step_indicators();
    }

    LOG_INFO("DataMapping", "View: " + names[view]);
    ScreenStateManager::instance().notify_changed(this);
}

void DataMappingScreen::on_step_changed(int step) {
    if (step < 0 || step > 4)
        return;
    current_step_ = step;
    step_stack_->setCurrentIndex(step);
    update_step_indicators();

    if (step == 2) {
        populate_mapping_list();
        if (!sample_data_.isNull()) {
            json_tree_->clear();
            populate_json_tree(sample_data_.isObject() ? QJsonValue(sample_data_.object())
                                                       : QJsonValue(sample_data_.array()),
                               nullptr, "root");
            json_tree_->expandToDepth(1);
        }
    }

    LOG_INFO("DataMapping", "Step: " + QString::number(step + 1));
}

void DataMappingScreen::on_next_step() {
    if (current_step_ < 4) {
        on_step_changed(current_step_ + 1);
    }
}

void DataMappingScreen::on_prev_step() {
    if (current_step_ > 0) {
        on_step_changed(current_step_ - 1);
    }
}

void DataMappingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load_mappings_from_db();
}

QVariantMap DataMappingScreen::save_state() const {
    return {
        {"view", current_view_},
    };
}

void DataMappingScreen::restore_state(const QVariantMap& state) {
    const int view = state.value("view", 0).toInt();
    // Restore list/templates view only — never resume a partial wizard.
    if (view == 0 || view == 1)
        on_view_changed(view);
}

} // namespace fincept::screens
