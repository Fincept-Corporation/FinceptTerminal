#include "screens/report_builder/ReportBuilderScreen.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTextDocumentWriter>
#include <QVBoxLayout>

namespace fincept::screens {

ReportBuilderScreen::ReportBuilderScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::DARK));
    metadata_.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Top toolbar
    vl->addWidget(build_toolbar());

    // Separator
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER));
    vl->addWidget(sep);

    // 3-panel splitter
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet("QSplitter::handle { background: #1A1A1A; }"
                             "QSplitter::handle:hover { background: #333333; }");

    comp_toolbar_ = new ComponentToolbar;
    canvas_ = new DocumentCanvas;
    properties_ = new PropertiesPanel;

    splitter_->addWidget(comp_toolbar_);
    splitter_->addWidget(canvas_);
    splitter_->addWidget(properties_);
    splitter_->setSizes({240, 700, 260});

    vl->addWidget(splitter_, 1);

    // Undo stack
    undo_stack_ = new QUndoStack(this);

    // Auto-save timer
    autosave_ = new QTimer(this);
    autosave_->setInterval(60000);
    connect(autosave_, &QTimer::timeout, this, &ReportBuilderScreen::on_save);

    // Wire signals
    connect(comp_toolbar_, &ComponentToolbar::add_component, this, &ReportBuilderScreen::add_component);
    connect(comp_toolbar_, &ComponentToolbar::structure_selected, this, &ReportBuilderScreen::select_component);
    connect(comp_toolbar_, &ComponentToolbar::delete_item, this, &ReportBuilderScreen::remove_component);
    connect(comp_toolbar_, &ComponentToolbar::duplicate, this, [this](int idx) {
        if (idx >= 0 && idx < components_.size()) {
            ReportComponent copy = components_[idx];
            copy.id = next_id_++;
            components_.insert(idx + 1, copy);
            refresh_canvas();
            refresh_structure();
        }
    });
    connect(comp_toolbar_, &ComponentToolbar::move_up, this, [this](int idx) {
        if (idx > 0) {
            components_.swapItemsAt(idx, idx - 1);
            selected_ = idx - 1;
            refresh_canvas();
            refresh_structure();
        }
    });
    connect(comp_toolbar_, &ComponentToolbar::move_down, this, [this](int idx) {
        if (idx >= 0 && idx < components_.size() - 1) {
            components_.swapItemsAt(idx, idx + 1);
            selected_ = idx + 1;
            refresh_canvas();
            refresh_structure();
        }
    });

    connect(properties_, &PropertiesPanel::content_changed, this, [this](int idx, const QString& content) {
        if (idx >= 0 && idx < components_.size()) {
            components_[idx].content = content;
            refresh_canvas();
        }
    });
    connect(properties_, &PropertiesPanel::config_changed, this,
            [this](int idx, const QString& key, const QString& val) {
                if (idx >= 0 && idx < components_.size()) {
                    components_[idx].config[key] = val;
                    refresh_canvas();
                }
            });
    connect(properties_, &PropertiesPanel::duplicate_requested, this, [this](int idx) {
        if (idx >= 0 && idx < components_.size()) {
            ReportComponent copy = components_[idx];
            copy.id = next_id_++;
            components_.insert(idx + 1, copy);
            refresh_canvas();
            refresh_structure();
        }
    });
    connect(properties_, &PropertiesPanel::delete_requested, this, &ReportBuilderScreen::remove_component);

    // Initial render
    refresh_canvas();
}

QWidget* ReportBuilderScreen::build_toolbar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(36);
    bar->setStyleSheet(QString("background: %1;").arg(ui::colors::HEADER));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(6);

    auto* back_btn = new QPushButton("< BACK");
    back_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; font-size: 13px; }"
                                    "QPushButton:hover { color: %2; }")
                                .arg(ui::colors::GRAY, ui::colors::WHITE));
    connect(back_btn, &QPushButton::clicked, this, &ReportBuilderScreen::back_to_dashboard);
    hl->addWidget(back_btn);

    auto* title = new QLabel("REPORT BUILDER");
    title->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;").arg(ui::colors::ORANGE));
    hl->addWidget(title);

    hl->addStretch();

    auto make_btn = [&](const char* text) {
        auto* b = new QPushButton(text);
        b->setFixedHeight(26);
        hl->addWidget(b);
        return b;
    };

    auto* undo_btn = make_btn("Undo");
    connect(undo_btn, &QPushButton::clicked, undo_stack_, &QUndoStack::undo);

    auto* redo_btn = make_btn("Redo");
    connect(redo_btn, &QPushButton::clicked, undo_stack_, &QUndoStack::redo);

    make_btn("|")->setEnabled(false);

    auto* save_btn = make_btn("Save");
    connect(save_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_save);

    auto* pdf_btn = make_btn("Export PDF");
    pdf_btn->setStyleSheet(QString("QPushButton { color: %1; font-weight: bold; }").arg(ui::colors::ORANGE));
    connect(pdf_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_export_pdf);

    auto* preview_btn = make_btn("Preview");
    connect(preview_btn, &QPushButton::clicked, this, &ReportBuilderScreen::on_preview);

    return bar;
}

void ReportBuilderScreen::add_component(const QString& type) {
    ReportComponent comp;
    comp.id = next_id_++;
    comp.type = type;

    if (type == "table") {
        comp.config["rows"] = "3";
        comp.config["cols"] = "3";
    }

    if (selected_ >= 0 && selected_ < components_.size()) {
        components_.insert(selected_ + 1, comp);
        selected_ = selected_ + 1;
    } else {
        components_.append(comp);
        selected_ = components_.size() - 1;
    }

    refresh_canvas();
    refresh_structure();
    select_component(selected_);
}

void ReportBuilderScreen::select_component(int index) {
    selected_ = index;
    if (index >= 0 && index < components_.size()) {
        properties_->show_properties(&components_[index], index);
    } else {
        properties_->show_empty();
    }
    refresh_canvas();
}

void ReportBuilderScreen::remove_component(int index) {
    if (index >= 0 && index < components_.size()) {
        components_.removeAt(index);
        if (selected_ >= components_.size()) {
            selected_ = components_.size() - 1;
        }
        refresh_canvas();
        refresh_structure();
        select_component(selected_);
    }
}

void ReportBuilderScreen::update_component(int index, const QString& content, const QMap<QString, QString>& config) {
    if (index >= 0 && index < components_.size()) {
        components_[index].content = content;
        for (auto it = config.begin(); it != config.end(); ++it) {
            components_[index].config[it.key()] = it.value();
        }
        refresh_canvas();
    }
}

void ReportBuilderScreen::refresh_canvas() {
    canvas_->render(components_, metadata_, selected_);
}

void ReportBuilderScreen::refresh_structure() {
    QStringList items;
    for (const auto& c : components_) {
        QString label = c.type.toUpper();
        if (!c.content.isEmpty()) {
            label += ": " + c.content.left(30);
        }
        items << label;
    }
    comp_toolbar_->update_structure(items, selected_);
}

void ReportBuilderScreen::on_save() {
    QString path = QFileDialog::getSaveFileName(this, "Save Report", "", "HTML (*.html);;ODF (*.odt)");
    if (path.isEmpty())
        return;

    QTextDocumentWriter writer(path);
    writer.write(canvas_->text_edit()->document());
}

void ReportBuilderScreen::on_export_pdf() {
    QString path = QFileDialog::getSaveFileName(this, "Export PDF", "", "PDF (*.pdf)");
    if (path.isEmpty())
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    canvas_->text_edit()->document()->print(&printer);
}

void ReportBuilderScreen::on_preview() {
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this,
            [this](QPrinter* p) { canvas_->text_edit()->document()->print(p); });
    preview.exec();
}

} // namespace fincept::screens
