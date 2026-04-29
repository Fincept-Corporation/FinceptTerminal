#include "screens/crypto_center/tabs/ComingSoonTab.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString cs_font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

} // namespace

ComingSoonTab::ComingSoonTab(const QString& tab_name,
                             const QString& phase_label,
                             const QString& description,
                             QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("comingSoonTab"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(0);
    root->addStretch(1);

    auto* center_row = new QHBoxLayout;
    center_row->setSpacing(0);
    center_row->addStretch(1);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("comingSoonCard"));
    card->setFixedWidth(460);
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(24, 22, 24, 22);
    cl->setSpacing(8);

    phase_label_ = new QLabel(phase_label.toUpper(), card);
    phase_label_->setObjectName(QStringLiteral("comingSoonPhase"));
    cl->addWidget(phase_label_);

    tab_name_label_ = new QLabel(tab_name.toUpper(), card);
    tab_name_label_->setObjectName(QStringLiteral("comingSoonTitle"));
    cl->addWidget(tab_name_label_);

    description_label_ = new QLabel(description, card);
    description_label_->setObjectName(QStringLiteral("comingSoonDescription"));
    description_label_->setWordWrap(true);
    cl->addWidget(description_label_);

    auto* status = new QLabel(QStringLiteral("STATUS  ·  COMING SOON"), card);
    status->setObjectName(QStringLiteral("comingSoonStatus"));
    cl->addSpacing(6);
    cl->addWidget(status);

    center_row->addWidget(card);
    center_row->addStretch(1);
    root->addLayout(center_row);
    root->addStretch(2);

    apply_theme();
}

ComingSoonTab::~ComingSoonTab() = default;

void ComingSoonTab::apply_theme() {
    using namespace ui::colors;
    const QString font = cs_font_stack();

    const QString ss = QStringLiteral(
        "QWidget#comingSoonTab { background:%1; }"
        "QFrame#comingSoonCard { background:%2; border:1px solid %3; }"
        "QLabel#comingSoonPhase { color:%4; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.5px; background:transparent; }"
        "QLabel#comingSoonTitle { color:%6; font-family:%5; font-size:18px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#comingSoonDescription { color:%7; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QLabel#comingSoonStatus { color:%8; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             TEXT_PRIMARY(),    // %6
             TEXT_SECONDARY(),  // %7
             TEXT_TERTIARY());  // %8

    setStyleSheet(ss);
}

} // namespace fincept::screens
