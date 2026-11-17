// UiActWeb.cpp — ui_act inside web pages: the page script finds the element;
// clicks and keys go in as a person's would (at its centre, through the view),
// and a control the page keeps hidden is clicked through the page.

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiActInternal.h"
#include "mcp/core/UiErrors.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>

namespace fincept::mcp::core::ui::act {

/// What finds the step's element in the page again (by index + key, at a point,
/// or the focused one), for page operations that run when the step performs.
QJsonObject web_element_spec(const Target& t) {
    if (t.web >= 0)
        return {{QStringLiteral("mode"), QStringLiteral("control")},
                {QStringLiteral("i"), t.web_info.value(QStringLiteral("i")).toInt(t.web)},
                {QStringLiteral("k"), t.web_key}};
    if (t.web_point) {
        const double z = web_zoom(t.web_view);
        return {{QStringLiteral("mode"), QStringLiteral("point")},
                {QStringLiteral("x"), t.hit_pos.x() / z},
                {QStringLiteral("y"), t.hit_pos.y() / z}};
    }
    return {{QStringLiteral("mode"), QStringLiteral("focus")}};
}

/// Run a page operation on the step's element; the answer is not needed (the
/// step's report comes from re-reading).
void web_page_op(const QPointer<QWidget>& view, QJsonObject spec, const QJsonObject& op) {
    if (!view)
        return;
    for (auto it = op.constBegin(); it != op.constEnd(); ++it)
        spec.insert(it.key(), it.value());
    run_web_script(view, web_target_script(spec), false, kWebLocateMs, [](const QJsonObject&, const QString&) {});
}

/// Mouse at a point of the page, delivered where a person's would land: the
/// view's render child.
void web_mouse(const QPointer<QWidget>& view, QPoint pos, Qt::MouseButton button, bool dbl, bool move_only = false) {
    if (!view)
        return;
    QWidget* in = web_input_widget(view);
    const QPoint local = in == view.data() ? pos : in->mapFrom(view.data(), pos);
    if (move_only) {
        QMouseEvent move(QEvent::MouseMove, QPointF(local), in->mapTo(in->window(), QPointF(local)),
                         in->mapToGlobal(QPointF(local)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(in, &move);
        return;
    }
    send_mouse(in, local, button, dbl);
}

void web_keys(const QPointer<QWidget>& view, const QString& text) {
    for (const QChar c : text) {
        if (!view)
            return;
        send_key(web_input_widget(view), c.toUpper().unicode(), c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier,
                 QString(c));
    }
}

/// A step on a web page. The page script already found the element
/// (`t.web_info`); clicks and keys go in as a person's would — at its centre,
/// through the view — and a control the page keeps hidden (a closed menu's
/// item) is clicked through the page instead.
std::optional<Prepared> prepare_web(const Step& s, const Target& t, QString* error) {
    QPointer<QWidget> vg(t.web_view);
    const QJsonObject& in = t.web_info;
    const QString what =
        t.web_focus   ? QStringLiteral("the focused element of %1").arg(t.ref)
        : t.web_point ? QStringLiteral("%1 %2 at (%3, %4) of %5")
                            .arg(in.value(QStringLiteral("r")).toString(), common::quoted(common::clip(t.name, 60)))
                            .arg(t.hit_pos.x())
                            .arg(t.hit_pos.y())
                            .arg(t.ref)
                      : describe_target(t);
    if (!vg || !vg->isVisible()) {
        *error = QStringLiteral("%1: the web page is not showing.").arg(what);
        return std::nullopt;
    }
    QStringList states;
    for (const QJsonValue& v : in.value(QStringLiteral("s")).toArray())
        states << v.toString();
    const bool secret = in.value(QStringLiteral("secret")).toBool();
    const bool hidden = t.web >= 0 && !t.has_hit; // no box on screen: a closed menu's item, a collapsed section
    const QPoint pos = t.has_hit ? t.hit_pos : vg->rect().center();
    const QJsonObject spec = web_element_spec(t);
    const QString act = s.action;
    Prepared p;
    p.note = QStringLiteral("%1 %2").arg(act, what);

    if (act == QLatin1String("close")) {
        *error = QStringLiteral("%1 is part of a web page — click its own close button instead.").arg(what);
        return std::nullopt;
    }
    if (act == QLatin1String("scroll")) {
        // A wheel turn at the point (or the page centre), as a person scrolls.
        const int notches = s.amount == 0 ? 5 : s.amount;
        p.perform = [vg, pos, notches]() {
            if (!vg)
                return;
            QWidget* w = web_input_widget(vg);
            const QPoint local = w == vg.data() ? pos : w->mapFrom(vg.data(), pos);
            QWheelEvent wheel(QPointF(local), w->mapToGlobal(QPointF(local)), QPoint(), QPoint(0, -120 * notches),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(w, &wheel);
        };
        return p;
    }
    if (t.web_focus && act != QLatin1String("press") && act != QLatin1String("type") && act != QLatin1String("focus")) {
        *error = QStringLiteral("Say which element of the page: a ref like %1:w3 from ui_read(ref=\"%1\"), or x / y.")
                     .arg(t.ref);
        return std::nullopt;
    }
    const bool needs_enabled = act != QLatin1String("hover") && act != QLatin1String("focus");
    if (needs_enabled && states.contains(QStringLiteral("disabled"))) {
        *error = QStringLiteral("%1 is disabled.").arg(what);
        return std::nullopt;
    }
    if (in.value(QStringLiteral("covered")).toBool() && act != QLatin1String("focus") && !t.web_point) {
        *error = QStringLiteral("%1 is covered by %2 — a click would land on that. Close it first, or look: "
                                "ui_screenshot(target=\"%3\").")
                     .arg(what, common::quoted(in.value(QStringLiteral("cover")).toString()),
                          t.ref.section(QLatin1Char(':'), 0, 0));
        return std::nullopt;
    }

    // ── Typing ──
    if (act == QLatin1String("type") || act == QLatin1String("clear") ||
        (act == QLatin1String("set_value") && in.value(QStringLiteral("editable")).toBool())) {
        if (secret) {
            *error =
                QStringLiteral("%1 is a secret field (password). Ask the user to type it — agents never type secrets.")
                    .arg(what);
            return std::nullopt;
        }
        if (t.web_focus && in.value(QStringLiteral("none")).toBool()) {
            *error = QStringLiteral("Nothing on the page has focus — type with a ref to the field (ui_read(ref=\"%1\") "
                                    "lists them).")
                         .arg(t.ref);
            return std::nullopt;
        }
        if (!in.value(QStringLiteral("editable")).toBool()) {
            *error = QStringLiteral("%1 is not a text field — click or select it instead.").arg(what);
            return std::nullopt;
        }
        if (hidden) {
            *error = QStringLiteral("%1 is hidden on the page — open what holds it first.").arg(what);
            return std::nullopt;
        }
        const QString text =
            act == QLatin1String("clear") ? QString()
            : act == QLatin1String("set_value")
                ? (s.value.isString() ? s.value.toString() : QString::number(s.value.toDouble(), 'g', 15))
                : s.text;
        if (text.size() > 2000) {
            *error = QStringLiteral("At most 2000 characters are typed into a web field per step.");
            return std::nullopt;
        }
        const bool append = s.append && act == QLatin1String("type");
        const bool submit = s.submit && act == QLatin1String("type");
        const bool click_first = !t.web_focus; // a click puts the caret in the field, as a person's does
        p.perform = [vg, pos, text, append, submit, click_first]() {
            if (!vg)
                return;
            if (click_first)
                web_mouse(vg, pos, Qt::LeftButton, false);
            if (!vg)
                return;
            QWidget* w = web_input_widget(vg);
            w->setFocus(Qt::OtherFocusReason);
            if (!append) {
                send_key(w, Qt::Key_A, Qt::ControlModifier, QString()); // select what is there
                send_key(w, Qt::Key_Delete, Qt::NoModifier, QString());
            }
            web_keys(vg, text);
            if (submit && vg)
                send_key(web_input_widget(vg), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        };
        p.note = act == QLatin1String("clear") ? QStringLiteral("clear %1").arg(what)
                                               : QStringLiteral("type %1 into %2%3")
                                                     .arg(common::quoted(common::clip(text, 60)), what,
                                                          submit ? QStringLiteral(" + Enter") : QString());
        return p;
    }

    // ── Keys ──
    if (act == QLatin1String("press")) {
        const auto chord = parse_key(s.text);
        if (!chord) {
            *error = QStringLiteral("\"%1\" is not a key. Examples: Enter, Escape, Tab, Down, PageDown, Ctrl+S, F5.")
                         .arg(s.text);
            return std::nullopt;
        }
        const KeyChord k = *chord;
        const bool ctrl_like = k.mods.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier);
        if ((ctrl_like && k.key == Qt::Key_V) || (k.mods.testFlag(Qt::ShiftModifier) && k.key == Qt::Key_Insert)) {
            *error = QStringLiteral("Pasting is not available to agents (the clipboard is the user's). Type the text "
                                    "with action=\"type\".");
            return std::nullopt;
        }
        if (secret) {
            *error = QStringLiteral("%1 is a secret field (password) — agents never operate it.").arg(what);
            return std::nullopt;
        }
        const bool focus_first = t.web >= 0;
        p.perform = [vg, spec, k, focus_first]() {
            if (!vg)
                return;
            auto send = [vg, k]() {
                if (vg)
                    send_key(web_input_widget(vg), k.key, k.mods, k.text);
            };
            web_input_widget(vg)->setFocus(Qt::OtherFocusReason);
            if (!focus_first) {
                send();
                return;
            }
            // Focus the control through the page, then press once that has landed.
            web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("focus")}});
            QTimer::singleShot(80, vg, send);
        };
        p.note = QStringLiteral("press %1 on %2").arg(s.text, what);
        return p;
    }

    if (act == QLatin1String("focus")) {
        p.perform = [vg, spec]() {
            if (!vg)
                return;
            web_input_widget(vg)->setFocus(Qt::OtherFocusReason);
            web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("focus")}});
        };
        return p;
    }

    // ── Options of a <select> ──
    const bool is_select = in.value(QStringLiteral("tag")).toString() == QLatin1String("select");
    if (act == QLatin1String("select") || (act == QLatin1String("set_value") && is_select)) {
        const QString want = act == QLatin1String("set_value") && s.value.isString() ? s.value.toString() : s.text;
        if (!is_select) {
            const QString role = in.value(QStringLiteral("r")).toString();
            if (role == QLatin1String("option") || role == QLatin1String("tab") || role == QLatin1String("radio") ||
                role.startsWith(QLatin1String("menuitem"))) {
                Step click = s;
                click.action = QStringLiteral("click");
                return prepare_web(click, t, error);
            }
            *error = QStringLiteral("%1 has no options to select — click it, or use type.").arg(what);
            return std::nullopt;
        }
        const QString picked = in.value(QStringLiteral("picked")).toString();
        if (picked.isEmpty()) {
            QStringList options;
            for (const QJsonValue& o : in.value(QStringLiteral("options")).toArray())
                options << o.toString();
            *error = QStringLiteral("%1 has no option \"%2\" — options: %3.")
                         .arg(what, want, common::clip(options.join(QStringLiteral(" | ")), 400));
            return std::nullopt;
        }
        p.perform = [vg, spec, want]() {
            web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("select")}, {QStringLiteral("text"), want}});
        };
        p.note = QStringLiteral("select %1 in %2").arg(common::quoted(picked), what);
        return p;
    }
    if (act == QLatin1String("set_value")) {
        *error = QStringLiteral("%1 does not hold a value — use click / type / select.").arg(what);
        return std::nullopt;
    }

    // ── Check / uncheck: a click unless it already is ──
    if (act == QLatin1String("check") || act == QLatin1String("uncheck")) {
        const bool want = act == QLatin1String("check");
        if (!states.contains(QStringLiteral("checked")) && !states.contains(QStringLiteral("unchecked"))) {
            *error = QStringLiteral("%1 cannot be checked — click it.").arg(what);
            return std::nullopt;
        }
        if (states.contains(QStringLiteral("checked")) == want) {
            p.perform = []() {};
            p.note +=
                QStringLiteral(" (already %1)").arg(want ? QStringLiteral("checked") : QStringLiteral("unchecked"));
            return p;
        }
    }

    // ── Clicks and hover ──
    if (act == QLatin1String("hover")) {
        if (hidden) {
            *error = QStringLiteral("%1 is hidden on the page — nothing to point at.").arg(what);
            return std::nullopt;
        }
        p.perform = [vg, pos]() { web_mouse(vg, pos, Qt::NoButton, false, true); };
        return p;
    }
    if (act == QLatin1String("click") || act == QLatin1String("double_click") || act == QLatin1String("right_click") ||
        act == QLatin1String("expand") || act == QLatin1String("collapse") || act == QLatin1String("check") ||
        act == QLatin1String("uncheck")) {
        if (hidden) {
            if (act == QLatin1String("double_click") || act == QLatin1String("right_click")) {
                *error = QStringLiteral("%1 is hidden on the page — open the menu or section holding it first, or "
                                        "click it (a click goes through the page).")
                             .arg(what);
                return std::nullopt;
            }
            p.perform = [vg, spec]() { web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("click")}}); };
            p.note += QStringLiteral(" (hidden on the page — clicked through it)");
            return p;
        }
        const bool dbl = act == QLatin1String("double_click");
        const Qt::MouseButton button = act == QLatin1String("right_click") ? Qt::RightButton : Qt::LeftButton;
        p.perform = [vg, pos, button, dbl]() { web_mouse(vg, pos, button, dbl); };
        return p;
    }
    *error =
        QStringLiteral("Unknown action \"%1\". Use one of: %2.").arg(act, known_actions().join(QStringLiteral(", ")));
    return std::nullopt;
}

/// Ask the page what the step's element is now: a listed control found again
/// (and scrolled into view), what is under a point, or the focused element.
void locate_web(const Step& s, const Target& t, Located done) {
    QJsonObject spec;
    if (t.web >= 0) {
        spec = {{QStringLiteral("mode"), QStringLiteral("control")},
                {QStringLiteral("i"), t.web},
                {QStringLiteral("k"), t.web_key},
                {QStringLiteral("reveal"), true}};
    } else {
        spec = web_element_spec(t);
    }
    if (s.action == QLatin1String("select") || s.action == QLatin1String("set_value")) {
        // Find the option now (dry run), so a missing one fails before anything is done.
        spec.insert(QStringLiteral("op"), QStringLiteral("select"));
        spec.insert(QStringLiteral("dry"), true);
        spec.insert(QStringLiteral("text"),
                    s.action == QLatin1String("set_value") && s.value.isString() ? s.value.toString() : s.text);
    }
    const QString what = describe_target(t);
    const QString base = t.ref.section(QLatin1Char(':'), 0, 0);
    QPointer<QWidget> vg(t.web_view);
    run_web_script(
        t.web_view, web_target_script(spec), false, kWebLocateMs,
        [t, vg, what, base, done](const QJsonObject& in, const QString& error) {
            if (!vg) {
                done(std::nullopt, QStringLiteral("%1: the web page closed.").arg(what));
                return;
            }
            if (!error.isEmpty() || in.isEmpty()) {
                done(std::nullopt, QStringLiteral("%1: the web page did not answer (%2) — ui_screenshot(target=\"%3\") "
                                                  "shows it.")
                                       .arg(what, error.isEmpty() ? QStringLiteral("no answer") : error, base));
                return;
            }
            const QString err = in.value(QStringLiteral("error")).toString();
            if (err == QLatin1String("gone")) {
                done(std::nullopt, QStringLiteral("%1 is no longer on the page — it was removed or renamed. Read the "
                                                  "page again: ui_read(ref=\"%2\").")
                                       .arg(what, base));
                return;
            }
            if (err == QLatin1String("several")) {
                done(std::nullopt, QStringLiteral("The page changed: %1 controls are now called %2 — read it again: "
                                                  "ui_read(ref=\"%3\").")
                                       .arg(in.value(QStringLiteral("count")).toInt())
                                       .arg(common::quoted(t.name), base));
                return;
            }
            if (err == QLatin1String("nothing")) {
                done(std::nullopt,
                     QStringLiteral("Nothing on the page at (%1, %2).").arg(t.hit_pos.x()).arg(t.hit_pos.y()));
                return;
            }
            if (err.startsWith(QLatin1String("script")) || err == QLatin1String("mode")) {
                done(std::nullopt, QStringLiteral("%1: the page script failed (%2).").arg(what, err));
                return;
            }
            Target out = t;
            out.web_located = true;
            out.web_info = in;
            if (!in.value(QStringLiteral("none")).toBool()) {
                out.role = in.value(QStringLiteral("r")).toString();
                out.name = in.value(QStringLiteral("n")).toString();
            }
            if (t.web >= 0) {
                // Its centre now (it may have been scrolled into view); no box: hidden.
                out.has_hit = false;
                if (in.contains(QStringLiteral("x"))) {
                    const double z = web_zoom(vg);
                    const QPoint centre(qRound(in.value(QStringLiteral("x")).toDouble() * z),
                                        qRound(in.value(QStringLiteral("y")).toDouble() * z));
                    if (vg->rect().contains(centre)) {
                        out.has_hit = true;
                        out.hit_pos = centre;
                    }
                }
            }
            done(out, QString());
        });
}

} // namespace fincept::mcp::core::ui::act
