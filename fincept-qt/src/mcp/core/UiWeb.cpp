// UiWeb.cpp — web pages (QWebEngineView): the page scripts that read and locate
// controls, and running them with a deadline. See UiEngine.h.

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTimer>

#ifdef HAS_QT_WEBENGINE
#    include <QWebEnginePage>
#    include <QWebEngineScript>
#    include <QWebEngineView>
#endif

#include <memory>

namespace fincept::mcp::core::ui {

namespace ui_web_detail {

// One script for every page question; the caller appends "(<spec JSON>)".
// It lists the page's controls the same way each time, so control N of a read
// is control N when it is acted on (and is recognised by role + name if the
// page changed). Password fields are never read: no value, only "secret".
const char* const kWebPageJs = R"JS((function (spec) {
  try {
    var SEL = 'a[href],button,input:not([type=hidden]),select,textarea,summary,[role=button],[role=link],' +
      '[role=tab],[role=menuitem],[role=menuitemcheckbox],[role=menuitemradio],[role=option],[role=checkbox],' +
      '[role=radio],[role=switch],[role=combobox],[role=textbox],[role=searchbox],[role=slider],' +
      '[role=spinbutton],[contenteditable=""],[contenteditable=true],[onclick],[tabindex]:not([tabindex="-1"])';
    var OWNER = 'a[href],button,summary,select,textarea,[role=button],[role=link],[role=tab],[role=menuitem],' +
      '[role=option],[role=checkbox],[role=radio],[role=switch]';
    function clip(s, n) {
      s = String(s == null ? '' : s).replace(/\s+/g, ' ').trim();
      return s.length > n ? s.slice(0, n - 1) + '…' : s;
    }
    function textOf(el) { return el.innerText != null ? el.innerText : el.textContent; }
    function secret(el) { return el.tagName === 'INPUT' && String(el.type).toLowerCase() === 'password'; }
    function roleOf(el) {
      var r = el.getAttribute('role');
      if (r) return r;
      var t = el.tagName;
      if (t === 'A') return 'link';
      if (t === 'BUTTON' || t === 'SUMMARY') return 'button';
      if (t === 'SELECT') return 'combobox';
      if (t === 'TEXTAREA') return 'textarea';
      if (t === 'INPUT') {
        var ty = String(el.type || 'text').toLowerCase();
        if (ty === 'checkbox' || ty === 'radio') return ty;
        if (ty === 'range') return 'slider';
        if (ty === 'number') return 'spinbutton';
        if (/^(button|submit|reset|image)$/.test(ty)) return 'button';
        if (/^(date|datetime-local|month|week|time)$/.test(ty)) return 'date';
        return 'textbox';
      }
      if (el.isContentEditable) return 'textbox';
      return 'clickable';
    }
    function nameOf(el) {
      var n = el.getAttribute('aria-label') || '';
      var lb = el.getAttribute('aria-labelledby');
      if (!n && lb) n = lb.split(/\s+/).map(function (id) {
        var x = document.getElementById(id);
        return x ? textOf(x) : '';
      }).join(' ');
      if (!n && el.labels && el.labels.length) n = textOf(el.labels[0]);
      var t = el.tagName;
      if (!n && t !== 'INPUT' && t !== 'SELECT' && t !== 'TEXTAREA') n = textOf(el);
      if (!n && t === 'INPUT' && /^(button|submit|reset)$/i.test(el.type)) n = el.value;
      if (!n) n = el.getAttribute('title') || el.getAttribute('placeholder') || el.getAttribute('alt') ||
        el.getAttribute('name') || '';
      if (!n && el.querySelector) {
        var img = el.querySelector('img[alt],svg title');
        if (img) n = img.getAttribute('alt') || img.textContent;
      }
      return clip(n, 120);
    }
    function valueOf(el) {
      if (secret(el)) return '';
      var t = el.tagName;
      if (t === 'SELECT') { var o = el.options[el.selectedIndex]; return o ? clip(o.text, 120) : ''; }
      if (t === 'TEXTAREA') return clip(el.value, 200);
      if (t === 'INPUT' && !/^(checkbox|radio|button|submit|reset|image|file)$/i.test(el.type)) return clip(el.value, 200);
      if (el.isContentEditable) return clip(textOf(el), 200);
      return '';
    }
    function box(el) {
      var r = el.getBoundingClientRect();
      if (r.width < 1 || r.height < 1) return null;
      var cs = getComputedStyle(el);
      if (cs.visibility === 'hidden' || cs.visibility === 'collapse' || Number(cs.opacity) === 0) return null;
      return r;
    }
    function statesOf(el, r) {
      var s = [];
      if (!r) s.push('hidden');
      else if (r.bottom <= 0 || r.top >= innerHeight || r.right <= 0 || r.left >= innerWidth) s.push('off-screen');
      if (el.disabled || el.getAttribute('aria-disabled') === 'true') s.push('disabled');
      var ty = String(el.type || '').toLowerCase(), ro = el.getAttribute('role') || '';
      if (el.checked === true || el.getAttribute('aria-checked') === 'true' || el.getAttribute('aria-pressed') === 'true')
        s.push('checked');
      else if (ty === 'checkbox' || ty === 'radio' || /^(checkbox|radio|switch|menuitemcheckbox|menuitemradio)$/.test(ro))
        s.push('unchecked');
      if (el.getAttribute('aria-selected') === 'true') s.push('selected');
      var ex = el.getAttribute('aria-expanded');
      if (ex === 'true') s.push('expanded'); else if (ex === 'false') s.push('collapsed');
      if (el.readOnly) s.push('read-only');
      if (el.required) s.push('required');
      if (typeof el.className === 'string' && /(^|\s)(active|selected)(\s|$)/.test(el.className)) s.push('active');
      if (secret(el)) s.push('secret');
      if (el === document.activeElement) s.push('focused');
      return s;
    }
    function list() {
      var all = document.querySelectorAll(SEL), out = [];
      for (var i = 0; i < all.length && out.length < 3000; ++i) {
        var p = all[i].parentElement;
        if (p && p.closest(OWNER)) continue; // a part of the control around it
        out.push(all[i]);
      }
      return out;
    }
    function info(el, at) {
      var r = box(el), o = {i: at, r: roleOf(el), n: nameOf(el), v: valueOf(el), s: statesOf(el, r)};
      if (r) {
        o.x = Math.round(r.left + r.width / 2); o.y = Math.round(r.top + r.height / 2);
        o.w = Math.round(r.width); o.h = Math.round(r.height);
      }
      return o;
    }
    function key(o) { return o.r + '|' + o.n; }
    function wordsOf(el) {
      var w = [nameOf(el), clip(textOf(el), 200), el.getAttribute('title'), el.getAttribute('aria-label'), el.id,
        typeof el.className === 'string' ? el.className : '', el.getAttribute('href'), el.getAttribute('onclick'),
        el.getAttribute('name')];
      if (el.tagName === 'INPUT' && /^(button|submit|reset)$/i.test(el.type)) w.push(el.value);
      return clip(w.filter(Boolean).join(' '), 600);
    }
    function formWords(el) {
      var f = el.form;
      if (!f) return '';
      var w = [f.id, f.getAttribute('name'), f.getAttribute('action')];
      var b = f.querySelectorAll('button,input[type=submit]');
      for (var j = 0; j < b.length && j < 4; ++j) w.push(wordsOf(b[j]));
      return clip(w.filter(Boolean).join(' '), 600);
    }
)JS"
                               R"JS(
    if (spec.mode === 'text') {
      var tb = document.body;
      return JSON.stringify({text: tb ? String(textOf(tb) || '').slice(0, 2000000) : ''});
    }
    if (spec.mode === 'read') {
      var els = list(), cs = [];
      for (var i = 0; i < els.length; ++i) cs.push(info(els[i], i));
      var b = document.body, d = document.documentElement;
      return JSON.stringify({title: document.title, url: location.href, vw: innerWidth, vh: innerHeight,
        sx: Math.round(scrollX), sy: Math.round(scrollY), sw: d ? d.scrollWidth : 0, sh: d ? d.scrollHeight : 0,
        loading: document.readyState !== 'complete', text: b ? String(textOf(b) || '').slice(0, 2000000) : '',
        controls: cs});
    }
    var el = null, at = -1;
    if (spec.mode === 'control') {
      var all2 = list(), c = all2[spec.i];
      if (c && key(info(c, spec.i)) === spec.k) { el = c; at = spec.i; }
      else {
        var hits = [];
        for (var j = 0; j < all2.length; ++j) if (key(info(all2[j], j)) === spec.k) hits.push(j);
        if (hits.length !== 1) return JSON.stringify({error: hits.length ? 'several' : 'gone', count: hits.length});
        at = hits[0]; el = all2[at];
      }
    } else if (spec.mode === 'point') {
      var h = document.elementFromPoint(spec.x, spec.y);
      if (!h) return JSON.stringify({error: 'nothing'});
      el = h.closest(OWNER) || h.closest(SEL) || h;
    } else if (spec.mode === 'focus') {
      el = document.activeElement;
      if (!el || el === document.body || el === document.documentElement) return JSON.stringify({none: true});
    } else {
      return JSON.stringify({error: 'mode'});
    }
    if (spec.reveal && el.scrollIntoView) {
      var rb = box(el);
      if (rb && (rb.top < 0 || rb.bottom > innerHeight || rb.left < 0 || rb.right > innerWidth))
        el.scrollIntoView({block: 'center', inline: 'center'});
    }
    var out = info(el, at);
    out.tag = el.tagName.toLowerCase();
    out.secret = secret(el);
    out.words = wordsOf(el);
    out.form = formWords(el);
    out.editable = !!(el.isContentEditable || el.tagName === 'TEXTAREA' ||
      (el.tagName === 'INPUT' && !/^(checkbox|radio|button|submit|reset|image|file|range|color)$/i.test(el.type)));
    if (out.x !== undefined) {
      var top = document.elementFromPoint(out.x, out.y);
      if (top && top !== el && !el.contains(top) && !top.contains(el)) {
        out.covered = true;
        out.cover = clip(nameOf(top.closest(SEL) || top) || textOf(top), 80);
      }
    }
    if (el.tagName === 'SELECT') {
      out.options = [];
      for (var k = 0; k < el.options.length && k < 200; ++k) out.options.push(clip(el.options[k].text, 80));
    }
    var op = spec.op || '';
    if (op === 'click') { el.click(); out.done = true; }
    else if (op === 'focus') { el.focus(); out.done = true; }
    else if (op === 'select') {
      var pick = -1, want = String(spec.text || '').trim().toLowerCase();
      if (el.tagName === 'SELECT') {
        for (var m = 0; m < el.options.length && pick < 0; ++m)
          if (el.options[m].text.trim().toLowerCase() === want || el.options[m].value.toLowerCase() === want) pick = m;
        for (var m2 = 0; m2 < el.options.length && pick < 0; ++m2)
          if (want && el.options[m2].text.toLowerCase().indexOf(want) >= 0) pick = m2;
      }
      if (pick < 0) out.error = el.tagName === 'SELECT' ? 'no-option' : 'not-select';
      else {
        out.picked = clip(el.options[pick].text, 80);
        if (!spec.dry) {
          el.selectedIndex = pick;
          el.dispatchEvent(new Event('input', {bubbles: true}));
          el.dispatchEvent(new Event('change', {bubbles: true}));
          out.done = true;
        }
      }
    } else if (op === 'scroll') {
      var sc = el;
      while (sc && sc !== document.body && !(sc.scrollHeight > sc.clientHeight + 4 &&
             /(auto|scroll)/.test(getComputedStyle(sc).overflowY))) sc = sc.parentElement;
      var dy = spec.dy || Math.round(innerHeight * 0.9);
      if (sc && sc !== document.body) sc.scrollBy(0, dy); else window.scrollBy(0, dy);
      out.done = true;
    }
    return JSON.stringify(out);
  } catch (e) {
    return JSON.stringify({error: 'script: ' + (e && e.message ? e.message : String(e))});
  }
}))JS";

// A KLineChart page's data, read through its own API (the page world).
const char* const kWebChartJs = R"JS((function (off, lim) {
  try {
    var c = window.chart;
    if (!c || typeof c.getDataList !== 'function') return '';
    var d = c.getDataList() || [], n = d.length, res = {count: n};
    if (n) {
      var lo = Infinity, hi = -Infinity;
      for (var i = 0; i < n; ++i) { if (d[i].low < lo) lo = d[i].low; if (d[i].high > hi) hi = d[i].high; }
      res.low = lo; res.high = hi; res.first = d[0].timestamp;
      var l = d[n - 1];
      res.last = [l.timestamp, l.open, l.high, l.low, l.close, l.volume];
    }
    try {
      var vr = c.getVisibleRange();
      res.from = vr.realFrom != null ? vr.realFrom : vr.from;
      res.to = vr.realTo != null ? vr.realTo : vr.to;
    } catch (e1) {}
    try { res.type = c.getStyles().candle.type; } catch (e2) {}
    var inds = [];
    try {
      var all = c.getIndicatorByPaneId();
      if (all && all.forEach) all.forEach(function (m, pane) {
        if (m && m.forEach) m.forEach(function (ind, name) {
          var r = ind.result || [], last = r.length ? r[r.length - 1] : null, vals = [];
          if (last) (ind.figures || []).forEach(function (f) {
            var v = last[f.key];
            if (typeof v === 'number' && isFinite(v)) vals.push([String(f.title || f.key).replace(/[:\s]+$/, ''), v]);
          });
          inds.push({pane: pane, name: name, params: ind.calcParams || [], last: vals});
        });
      });
    } catch (e3) {}
    res.indicators = inds;
    if (lim > 0 && n) {
      var start = off < 0 ? Math.max(0, n - lim) : Math.min(off, n);
      res.start = start;
      res.rows = d.slice(start, start + lim).map(function (k) {
        return [k.timestamp, k.open, k.high, k.low, k.close, k.volume];
      });
    }
    return JSON.stringify(res);
  } catch (e) {
    return JSON.stringify({error: String(e && e.message ? e.message : e)});
  }
}))JS";

} // namespace ui_web_detail

QWidget* web_view_of(QWidget* w) {
#ifdef HAS_QT_WEBENGINE
    if (!w)
        return nullptr;
    // The view itself, or its render child (what a click at a point lands on).
    for (QWidget* p = w; p; p = p->parentWidget()) {
        if (qobject_cast<QWebEngineView*>(p))
            return p;
        if (p->isWindow())
            break;
    }
    // A host that only wraps a view (KLineChartWidget) — never a panel that
    // merely contains one somewhere deeper.
    auto* view = w->findChild<QWebEngineView*>(QString(), Qt::FindDirectChildrenOnly);
    if (!view)
        return nullptr;
    if (w->inherits("fincept::ui::KLineChartWidget"))
        return view;
    for (QWidget* c : w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (c != view && c->isVisible())
            return nullptr;
    }
    return view;
#else
    Q_UNUSED(w);
    return nullptr;
#endif
}

QWidget* web_input_widget(QWidget* view) {
    if (!view)
        return nullptr;
    if (QWidget* proxy = view->focusProxy())
        return proxy;
    for (QWidget* c : view->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (c->isVisible())
            return c;
    }
    return view;
}

double web_zoom(QWidget* view) {
#ifdef HAS_QT_WEBENGINE
    if (auto* v = qobject_cast<QWebEngineView*>(view)) {
        const double z = v->zoomFactor();
        return z > 0.0 ? z : 1.0;
    }
#else
    Q_UNUSED(view);
#endif
    return 1.0;
}

void run_web_script(QWidget* view, const QString& script, bool page_world, int timeout_ms,
                    std::function<void(const QJsonObject& result, const QString& error)> done) {
#ifdef HAS_QT_WEBENGINE
    auto* v = qobject_cast<QWebEngineView*>(view);
    if (!v || !v->page()) {
        done({}, QStringLiteral("there is no web page"));
        return;
    }
    // Answered once: by the page, or by the timer when the page never answers
    // (still loading, its renderer died, or the view went away).
    auto answered = std::make_shared<bool>(false);
    auto finish = std::make_shared<std::function<void(const QJsonObject&, const QString&)>>(std::move(done));
    QTimer::singleShot(timeout_ms, QCoreApplication::instance(), [answered, finish, timeout_ms]() {
        if (*answered)
            return;
        *answered = true;
        common::run_guarded("web script timeout", [&]() {
            (*finish)({}, QStringLiteral("the page did not answer within %1 s (still loading, or busy)")
                              .arg(timeout_ms / 1000.0, 0, 'g', 2));
        });
    });
    v->page()->runJavaScript(script, page_world ? QWebEngineScript::MainWorld : QWebEngineScript::ApplicationWorld,
                             [answered, finish](const QVariant& result) {
                                 if (*answered)
                                     return;
                                 *answered = true;
                                 common::run_guarded("web script answer", [&]() {
                                     const QString text = result.toString();
                                     if (text.isEmpty()) {
                                         (*finish)({}, QString()); // nothing to report (e.g. no chart on the page)
                                         return;
                                     }
                                     const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
                                     if (!doc.isObject()) {
                                         (*finish)({}, QStringLiteral("the page gave an unreadable answer"));
                                         return;
                                     }
                                     (*finish)(doc.object(), QString());
                                 });
                             });
#else
    Q_UNUSED(view);
    Q_UNUSED(script);
    Q_UNUSED(page_world);
    Q_UNUSED(timeout_ms);
    done({}, QStringLiteral("this build has no web engine"));
#endif
}

QString web_read_script() {
    return QString::fromUtf8(ui_web_detail::kWebPageJs) + QStringLiteral("({\"mode\":\"read\"})");
}

QString web_text_script() {
    return QString::fromUtf8(ui_web_detail::kWebPageJs) + QStringLiteral("({\"mode\":\"text\"})");
}

QString web_target_script(const QJsonObject& spec) {
    return QString::fromUtf8(ui_web_detail::kWebPageJs) + QLatin1Char('(') +
           QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact)) + QLatin1Char(')');
}

QString web_chart_script(int offset, int limit) {
    return QString::fromUtf8(ui_web_detail::kWebChartJs) + QStringLiteral("(%1, %2)").arg(offset).arg(limit);
}

QString web_control_key(const QJsonObject& control) {
    return control.value(QStringLiteral("r")).toString() + QLatin1Char('|') +
           control.value(QStringLiteral("n")).toString();
}


} // namespace fincept::mcp::core::ui
