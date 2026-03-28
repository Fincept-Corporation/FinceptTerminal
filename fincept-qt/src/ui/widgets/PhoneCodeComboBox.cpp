#include "ui/widgets/PhoneCodeComboBox.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStandardItem>

namespace fincept::ui {

static const struct { const char* flag; const char* name; const char* code; } k_countries[] = {
    {"🇦🇫","Afghanistan","+93"},{"🇦🇱","Albania","+355"},{"🇩🇿","Algeria","+213"},
    {"🇦🇩","Andorra","+376"},{"🇦🇴","Angola","+244"},{"🇦🇬","Antigua & Barbuda","+1-268"},
    {"🇦🇷","Argentina","+54"},{"🇦🇲","Armenia","+374"},{"🇦🇺","Australia","+61"},
    {"🇦🇹","Austria","+43"},{"🇦🇿","Azerbaijan","+994"},{"🇧🇸","Bahamas","+1-242"},
    {"🇧🇭","Bahrain","+973"},{"🇧🇩","Bangladesh","+880"},{"🇧🇧","Barbados","+1-246"},
    {"🇧🇾","Belarus","+375"},{"🇧🇪","Belgium","+32"},{"🇧🇿","Belize","+501"},
    {"🇧🇯","Benin","+229"},{"🇧🇹","Bhutan","+975"},{"🇧🇴","Bolivia","+591"},
    {"🇧🇦","Bosnia & Herzegovina","+387"},{"🇧🇼","Botswana","+267"},{"🇧🇷","Brazil","+55"},
    {"🇧🇳","Brunei","+673"},{"🇧🇬","Bulgaria","+359"},{"🇧🇫","Burkina Faso","+226"},
    {"🇧🇮","Burundi","+257"},{"🇨🇻","Cabo Verde","+238"},{"🇰🇭","Cambodia","+855"},
    {"🇨🇲","Cameroon","+237"},{"🇨🇦","Canada","+1"},{"🇨🇫","Central African Republic","+236"},
    {"🇹🇩","Chad","+235"},{"🇨🇱","Chile","+56"},{"🇨🇳","China","+86"},
    {"🇨🇴","Colombia","+57"},{"🇰🇲","Comoros","+269"},{"🇨🇩","Congo (DRC)","+243"},
    {"🇨🇬","Congo (Republic)","+242"},{"🇨🇷","Costa Rica","+506"},{"🇭🇷","Croatia","+385"},
    {"🇨🇺","Cuba","+53"},{"🇨🇾","Cyprus","+357"},{"🇨🇿","Czech Republic","+420"},
    {"🇩🇰","Denmark","+45"},{"🇩🇯","Djibouti","+253"},{"🇩🇲","Dominica","+1-767"},
    {"🇩🇴","Dominican Republic","+1-809"},{"🇪🇨","Ecuador","+593"},{"🇪🇬","Egypt","+20"},
    {"🇸🇻","El Salvador","+503"},{"🇬🇶","Equatorial Guinea","+240"},{"🇪🇷","Eritrea","+291"},
    {"🇪🇪","Estonia","+372"},{"🇸🇿","Eswatini","+268"},{"🇪🇹","Ethiopia","+251"},
    {"🇫🇯","Fiji","+679"},{"🇫🇮","Finland","+358"},{"🇫🇷","France","+33"},
    {"🇬🇦","Gabon","+241"},{"🇬🇲","Gambia","+220"},{"🇬🇪","Georgia","+995"},
    {"🇩🇪","Germany","+49"},{"🇬🇭","Ghana","+233"},{"🇬🇷","Greece","+30"},
    {"🇬🇩","Grenada","+1-473"},{"🇬🇹","Guatemala","+502"},{"🇬🇳","Guinea","+224"},
    {"🇬🇼","Guinea-Bissau","+245"},{"🇬🇾","Guyana","+592"},{"🇭🇹","Haiti","+509"},
    {"🇭🇳","Honduras","+504"},{"🇭🇺","Hungary","+36"},{"🇮🇸","Iceland","+354"},
    {"🇮🇳","India","+91"},{"🇮🇩","Indonesia","+62"},{"🇮🇷","Iran","+98"},
    {"🇮🇶","Iraq","+964"},{"🇮🇪","Ireland","+353"},{"🇮🇱","Israel","+972"},
    {"🇮🇹","Italy","+39"},{"🇯🇲","Jamaica","+1-876"},{"🇯🇵","Japan","+81"},
    {"🇯🇴","Jordan","+962"},{"🇰🇿","Kazakhstan","+7"},{"🇰🇪","Kenya","+254"},
    {"🇰🇮","Kiribati","+686"},{"🇽🇰","Kosovo","+383"},{"🇰🇼","Kuwait","+965"},
    {"🇰🇬","Kyrgyzstan","+996"},{"🇱🇦","Laos","+856"},{"🇱🇻","Latvia","+371"},
    {"🇱🇧","Lebanon","+961"},{"🇱🇸","Lesotho","+266"},{"🇱🇷","Liberia","+231"},
    {"🇱🇾","Libya","+218"},{"🇱🇮","Liechtenstein","+423"},{"🇱🇹","Lithuania","+370"},
    {"🇱🇺","Luxembourg","+352"},{"🇲🇬","Madagascar","+261"},{"🇲🇼","Malawi","+265"},
    {"🇲🇾","Malaysia","+60"},{"🇲🇻","Maldives","+960"},{"🇲🇱","Mali","+223"},
    {"🇲🇹","Malta","+356"},{"🇲🇭","Marshall Islands","+692"},{"🇲🇷","Mauritania","+222"},
    {"🇲🇺","Mauritius","+230"},{"🇲🇽","Mexico","+52"},{"🇫🇲","Micronesia","+691"},
    {"🇲🇩","Moldova","+373"},{"🇲🇨","Monaco","+377"},{"🇲🇳","Mongolia","+976"},
    {"🇲🇪","Montenegro","+382"},{"🇲🇦","Morocco","+212"},{"🇲🇿","Mozambique","+258"},
    {"🇲🇲","Myanmar","+95"},{"🇳🇦","Namibia","+264"},{"🇳🇷","Nauru","+674"},
    {"🇳🇵","Nepal","+977"},{"🇳🇱","Netherlands","+31"},{"🇳🇿","New Zealand","+64"},
    {"🇳🇮","Nicaragua","+505"},{"🇳🇪","Niger","+227"},{"🇳🇬","Nigeria","+234"},
    {"🇲🇰","North Macedonia","+389"},{"🇳🇴","Norway","+47"},{"🇴🇲","Oman","+968"},
    {"🇵🇰","Pakistan","+92"},{"🇵🇼","Palau","+680"},{"🇵🇦","Panama","+507"},
    {"🇵🇬","Papua New Guinea","+675"},{"🇵🇾","Paraguay","+595"},{"🇵🇪","Peru","+51"},
    {"🇵🇭","Philippines","+63"},{"🇵🇱","Poland","+48"},{"🇵🇹","Portugal","+351"},
    {"🇶🇦","Qatar","+974"},{"🇷🇴","Romania","+40"},{"🇷🇺","Russia","+7"},
    {"🇷🇼","Rwanda","+250"},{"🇰🇳","Saint Kitts & Nevis","+1-869"},{"🇱🇨","Saint Lucia","+1-758"},
    {"🇻🇨","Saint Vincent & Grenadines","+1-784"},{"🇼🇸","Samoa","+685"},
    {"🇸🇲","San Marino","+378"},{"🇸🇹","São Tomé & Príncipe","+239"},
    {"🇸🇦","Saudi Arabia","+966"},{"🇸🇳","Senegal","+221"},{"🇷🇸","Serbia","+381"},
    {"🇸🇨","Seychelles","+248"},{"🇸🇱","Sierra Leone","+232"},{"🇸🇬","Singapore","+65"},
    {"🇸🇰","Slovakia","+421"},{"🇸🇮","Slovenia","+386"},{"🇸🇧","Solomon Islands","+677"},
    {"🇸🇴","Somalia","+252"},{"🇿🇦","South Africa","+27"},{"🇸🇸","South Sudan","+211"},
    {"🇪🇸","Spain","+34"},{"🇱🇰","Sri Lanka","+94"},{"🇸🇩","Sudan","+249"},
    {"🇸🇷","Suriname","+597"},{"🇸🇪","Sweden","+46"},{"🇨🇭","Switzerland","+41"},
    {"🇸🇾","Syria","+963"},{"🇹🇼","Taiwan","+886"},{"🇹🇯","Tajikistan","+992"},
    {"🇹🇿","Tanzania","+255"},{"🇹🇭","Thailand","+66"},{"🇹🇱","Timor-Leste","+670"},
    {"🇹🇬","Togo","+228"},{"🇹🇴","Tonga","+676"},{"🇹🇹","Trinidad & Tobago","+1-868"},
    {"🇹🇳","Tunisia","+216"},{"🇹🇷","Turkey","+90"},{"🇹🇲","Turkmenistan","+993"},
    {"🇹🇻","Tuvalu","+688"},{"🇺🇬","Uganda","+256"},{"🇺🇦","Ukraine","+380"},
    {"🇦🇪","United Arab Emirates","+971"},{"🇬🇧","United Kingdom","+44"},
    {"🇺🇸","United States","+1"},{"🇺🇾","Uruguay","+598"},{"🇺🇿","Uzbekistan","+998"},
    {"🇻🇺","Vanuatu","+678"},{"🇻🇦","Vatican City","+379"},{"🇻🇪","Venezuela","+58"},
    {"🇻🇳","Vietnam","+84"},{"🇾🇪","Yemen","+967"},{"🇿🇲","Zambia","+260"},
    {"🇿🇼","Zimbabwe","+263"},
};

static const char* k_scrollbar_style =
    "QScrollBar:vertical { background:#0a0a0a; width:5px; border:none; }"
    "QScrollBar::handle:vertical { background:#2a2a2a; border-radius:2px; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";

static const char* k_popup_style =
    "QAbstractItemView {"
    "  background:#111111; color:#e5e5e5; border:1px solid #2a2a2a;"
    "  selection-background-color:#1c1400; selection-color:#d97706;"
    "  font-size:13px; font-family:'Consolas',monospace; outline:none; padding:2px; }"
    "QAbstractItemView::item { padding:5px 12px; min-height:28px; border:none; }"
    "QScrollBar:vertical { background:#0a0a0a; width:5px; border:none; }"
    "QScrollBar::handle:vertical { background:#2a2a2a; border-radius:2px; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";

PhoneCodeComboBox::PhoneCodeComboBox(QWidget* parent) : QComboBox(parent) {
    model_ = new QStandardItemModel(this);

    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setMaxVisibleItems(12);

    populate();
    setModel(model_);

    // Wide enough to show full country names without clipping
    view()->setMinimumWidth(320);
    view()->setStyleSheet(k_popup_style);
    view()->verticalScrollBar()->setStyleSheet(k_scrollbar_style);

    // Completer: MatchContains so typing any part of name/code filters the list
    auto* comp = new QCompleter(model_, this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    comp->setFilterMode(Qt::MatchContains);
    comp->setCompletionMode(QCompleter::PopupCompletion);
    comp->setCompletionColumn(0);
    if (auto* popup = comp->popup()) {
        popup->setMinimumWidth(320);
        popup->setStyleSheet(k_popup_style);
        popup->verticalScrollBar()->setStyleSheet(k_scrollbar_style);
    }
    setCompleter(comp);

    // When completer picks a match, sync the index and display
    connect(comp, QOverload<const QModelIndex&>::of(&QCompleter::activated),
            this, [this](const QModelIndex& idx) {
        int row = idx.row();
        setCurrentIndex(row);
        sync_display(row);
    });

    setStyleSheet(
        "QComboBox {"
        "  background:#0a0a0a; color:#e5e5e5; border:1px solid #1a1a1a;"
        "  padding:4px 8px; font-size:14px;"
        "  font-family:'Consolas','Courier New',monospace; }"
        "QComboBox:focus { border:1px solid #333333; }"
        // Reserve space on the right for the custom drop-down button
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding; subcontrol-position: right center;"
        "  width: 28px; border-left: 1px solid #1a1a1a; background: #111111;"
        "  border-radius: 0 2px 2px 0; }"
        // Use the unicode chevron as the arrow via a UTF-8 string trick
        "QComboBox::down-arrow {"
        "  image: none; width:0; height:0; }"
        "QComboBox QAbstractItemView {"
        "  background:#111111; color:#e5e5e5; border:1px solid #2a2a2a;"
        "  selection-background-color:#1c1400; selection-color:#d97706;"
        "  font-size:13px; font-family:'Consolas',monospace; outline:none; padding:2px; }"
        "QComboBox QAbstractItemView::item { padding:5px 12px; min-height:28px; border:none; }"
        "QComboBox QAbstractItemView QScrollBar:vertical { background:#0a0a0a; width:5px; border:none; }"
        "QComboBox QAbstractItemView QScrollBar::handle:vertical { background:#2a2a2a; border-radius:2px; }"
        "QComboBox QAbstractItemView QScrollBar::add-line:vertical,"
        "QComboBox QAbstractItemView QScrollBar::sub-line:vertical { height:0; }");

    // Draw a proper chevron "⌄" in the drop-down button area via a QLabel overlay
    arrow_lbl_ = new QLabel("⌄", this);
    arrow_lbl_->setAlignment(Qt::AlignCenter);
    arrow_lbl_->setStyleSheet(
        "color:#707070; font-size:16px; background:transparent;"
        "font-family:'Consolas',monospace;");
    arrow_lbl_->setAttribute(Qt::WA_TransparentForMouseEvents);
    arrow_lbl_->setGeometry(width() - 28, 0, 28, height());

    if (auto* le = lineEdit()) {
        le->setStyleSheet(
            "QLineEdit { background:transparent; color:#e5e5e5; border:none;"
            "  font-size:14px; font-family:'Consolas','Courier New',monospace; padding:0; }");
        le->setPlaceholderText("Search country…");

        // When user manually edits the field, try to match and update dial code
        connect(le, &QLineEdit::textEdited, this, [this](const QString& text) {
            // Try to find an exact code match first (e.g. user typed "+91")
            for (int i = 0; i < model_->rowCount(); i++) {
                if (model_->item(i)->data(Qt::UserRole).toString().compare(
                        text.trimmed(), Qt::CaseInsensitive) == 0) {
                    setCurrentIndex(i);
                    return;
                }
            }
        });
    }

    // Default to United States
    set_dial_code("+1");

    // On selection from native dropdown: show compact flag + code
    connect(this, &QComboBox::activated, this, &PhoneCodeComboBox::sync_display);
}

void PhoneCodeComboBox::populate() {
    for (const auto& c : k_countries) {
        auto* item = new QStandardItem;
        QString flag = QString::fromUtf8(c.flag);
        QString name = QString::fromUtf8(c.name);
        QString code = QString::fromUtf8(c.code);
        item->setText(QString("%1  %2  %3").arg(flag, name, code));
        item->setData(code, Qt::UserRole);
        item->setData(flag, Qt::UserRole + 1);
        model_->appendRow(item);
    }
}

void PhoneCodeComboBox::sync_display(int index) {
    if (index < 0 || index >= model_->rowCount()) return;
    QString flag = model_->item(index)->data(Qt::UserRole + 1).toString();
    QString code = model_->item(index)->data(Qt::UserRole).toString();
    if (auto* le = lineEdit())
        le->setText(QString("%1  %2").arg(flag, code));
}

QString PhoneCodeComboBox::dial_code() const {
    int idx = currentIndex();
    if (idx >= 0 && idx < model_->rowCount())
        return model_->item(idx)->data(Qt::UserRole).toString();
    return {};
}

void PhoneCodeComboBox::resizeEvent(QResizeEvent* e) {
    QComboBox::resizeEvent(e);
    if (arrow_lbl_)
        arrow_lbl_->setGeometry(width() - 28, 0, 28, height());
}

void PhoneCodeComboBox::set_dial_code(const QString& code) {
    for (int i = 0; i < model_->rowCount(); i++) {
        if (model_->item(i)->data(Qt::UserRole).toString() == code) {
            setCurrentIndex(i);
            sync_display(i);
            return;
        }
    }
}

} // namespace fincept::ui
