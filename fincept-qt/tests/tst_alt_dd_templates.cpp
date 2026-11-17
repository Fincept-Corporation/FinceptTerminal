// tst_alt_dd_templates — the due-diligence templates (src/screens/alt_investments/AltDdTemplates.h):
// saved checklists store each item's status by item id, so ids must be unique within a template,
// stable in form ("section.key") and every template must be findable by its id.

#include "screens/alt_investments/AltDdTemplates.h"

#include <QRegularExpression>
#include <QSet>
#include <QtTest>

using namespace fincept::screens;

class TstAltDdTemplates : public QObject {
    Q_OBJECT
  private slots:
    void ids_unique_and_well_formed() {
        const QRegularExpression form(QStringLiteral("^[a-z]+\\.[a-z_0-9]+$"));
        QSet<QString> template_ids;
        for (const alt_dd::Template& t : alt_dd::templates()) {
            QVERIFY2(!template_ids.contains(QString::fromLatin1(t.id)), t.id);
            template_ids.insert(QString::fromLatin1(t.id));
            QSet<QString> ids;
            for (const alt_dd::Section& s : t.sections) {
                QVERIFY(!s.items.isEmpty());
                for (const alt_dd::Item& it : s.items) {
                    const QString id = QString::fromLatin1(it.id);
                    QVERIFY2(form.match(id).hasMatch(), it.id);
                    QVERIFY2(!ids.contains(id), it.id);
                    ids.insert(id);
                    QVERIFY(QString::fromLatin1(it.text).size() > 10);
                }
            }
            QCOMPARE(alt_dd::item_count(t), int(ids.size()));
            QVERIFY(alt_dd::item_count(t) >= 25);
        }
        QCOMPARE(template_ids.size(), 6);
    }

    void lookup() {
        QVERIFY(alt_dd::find_template(QStringLiteral("private_credit")) != nullptr);
        QVERIFY(alt_dd::find_template(QStringLiteral("nope")) == nullptr);
        QCOMPARE(alt_dd::statuses(), (QStringList{QStringLiteral("open"), QStringLiteral("done"), QStringLiteral("issue"),
                                                  QStringLiteral("na")}));
    }
};

QTEST_GUILESS_MAIN(TstAltDdTemplates)
#include "tst_alt_dd_templates.moc"
