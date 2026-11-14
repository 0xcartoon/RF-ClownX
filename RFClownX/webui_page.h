/* Remote control page - stored in flash (PROGMEM), served from webui.cpp.
   One self-contained document: no external assets, works fully offline. */

#ifndef WEBUI_PAGE_H
#define WEBUI_PAGE_H

#include <Arduino.h>

const char WEBUI_HTML[] PROGMEM = R"HTMLPAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="dark">
<meta name="theme-color" content="#0c0d10">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="RF Clown X">
<link rel="icon" href="data:,">
<title>RF Clown X &middot; Remote</title>
<style>
:root{
  --bg:#0c0d10;--card:#15171c;--card2:#1a1d23;--line:#23262e;
  --text:#eceef2;--muted:#8b91a1;--accent:#6e8bff;--accent-soft:rgba(110,139,255,.13);
  --danger:#e5484d;--ok:#37c98b;
}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0 auto;max-width:680px;padding:16px 16px 104px;background:var(--bg);color:var(--text);
  font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
  overscroll-behavior-y:contain;-webkit-tap-highlight-color:transparent}
h1,h2,h3{margin:0;font-weight:600}
h1{font-size:16px;letter-spacing:-.01em;white-space:nowrap}
h2{font-size:14px}
h3{font-size:12px;font-weight:500;color:var(--muted)}

.top{display:flex;align-items:center;justify-content:space-between;gap:12px;padding-bottom:12px}
.brand{display:flex;align-items:center;gap:9px;min-width:0}
.dot{width:8px;height:8px;border-radius:50%;background:var(--muted);flex:0 0 auto;transition:background .2s}
.dot.ok{background:var(--ok)}
.dot.bad{background:var(--danger)}
.badge{font-size:11px;font-weight:500;color:var(--muted);background:var(--card2);
  border:1px solid var(--line);border-radius:999px;padding:2px 8px}
.link{font-size:12.5px;color:var(--muted);white-space:nowrap}

.stats{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}
.stat{display:flex;flex-direction:column;gap:2px;min-width:0;padding:9px 10px;
  background:var(--card);border:1px solid var(--line);border-radius:12px}
.stat .k{font-size:10.5px;color:var(--muted);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.stat .v{font-size:14.5px;font-weight:600;letter-spacing:-.01em;font-variant-numeric:tabular-nums;
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.stat .v.live{color:var(--accent)}

.card{margin-top:12px;padding:14px;background:var(--card);border:1px solid var(--line);border-radius:16px}
.card-h{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:12px}
.meta{font-size:11.5px;color:var(--muted);font-variant-numeric:tabular-nums}

.screen{position:relative;padding:5px;background:#000;border:1px solid var(--line);border-radius:12px}
canvas{display:block;width:100%;height:auto;aspect-ratio:2/1;border-radius:8px;image-rendering:pixelated}

.soft{aspect-ratio:2/1;display:flex;flex-direction:column;container-type:inline-size;
  background:#000;border-radius:8px;overflow:hidden;color:#eceef2;font-weight:600;letter-spacing:0}
.soft-head{flex:0 0 18.75%;display:flex;align-items:center;justify-content:space-between;gap:6px;
  padding:0 3.2cqw;background:#eceef2;color:#08090b;
  font-size:11px;font-size:clamp(8px,3.4cqw,15px)}
.soft-head span{overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.soft-row{flex:0 0 18.75%;display:flex;align-items:center;justify-content:space-between;gap:6px;
  padding:0 3.2cqw;font-size:12px;font-size:clamp(8px,3.9cqw,18px)}
.soft-row>span:first-child{overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.soft-row.on{background:#eceef2;color:#08090b}
.soft-row .caret{opacity:.5}
.soft-body{flex:1;display:flex;flex-direction:column;justify-content:center;gap:1.6cqw;
  padding:0 3.2cqw 1.6cqw}
.soft-title{margin:0;font-size:13px;font-size:clamp(9px,5.2cqw,21px)}
.soft-line{margin:0;font-weight:500;color:#c9ccd4;font-size:9px;font-size:clamp(7px,3.4cqw,14px)}
.soft-bar{height:5px;height:clamp(3px,1.4cqw,7px);padding:1px;border:1px solid #4a4f5c;border-radius:2px}
.soft-bar i{display:block;height:100%;background:#eceef2;border-radius:1px}

.hint{margin:8px 0 0;font-size:11.5px;color:var(--muted)}
.seg{display:grid;grid-template-columns:repeat(4,1fr);gap:2px;padding:3px;
  background:var(--card2);border:1px solid var(--line);border-radius:10px}
.seg button{appearance:none;border:0;background:transparent;color:var(--muted);font:inherit;
  font-size:13px;font-weight:500;padding:9px 0;border-radius:7px;cursor:pointer;
  touch-action:manipulation;transition:background .15s,color .15s}
.seg button:hover{color:var(--text)}
.seg button.on{background:#2a2f3b;color:var(--text)}
.seg.mini{grid-template-columns:auto auto;padding:2px}
.seg.mini button{padding:4px 10px;font-size:12px}

input[type=range]{appearance:none;-webkit-appearance:none;width:100%;height:22px;margin:0;
  background:transparent;cursor:pointer;touch-action:pan-y}
input[type=range]::-webkit-slider-runnable-track{height:6px;border-radius:99px;background:#2a2f3b}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;margin-top:-6px;
  border:0;border-radius:50%;background:var(--text);box-shadow:0 1px 2px rgba(0,0,0,.6)}
input[type=range]::-moz-range-track{height:6px;border-radius:99px;background:#2a2f3b}
input[type=range]::-moz-range-thumb{width:18px;height:18px;border:0;border-radius:50%;background:var(--text)}

.pad{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:6px;margin-top:12px}
.key{appearance:none;min-height:46px;padding:10px 4px;background:var(--card2);
  border:1px solid var(--line);border-radius:11px;color:var(--text);font:inherit;
  font-size:13.5px;font-weight:500;cursor:pointer;touch-action:manipulation;
  user-select:none;-webkit-user-select:none;transition:background .12s,transform .06s}
.key:hover{background:#20242c}
.key:active{transform:scale(.97);background:#252a34}
.key.back{background:transparent;color:var(--muted)}

.settings{margin-top:10px;border-top:1px solid var(--line)}
.settings summary{display:flex;align-items:center;gap:8px;padding:12px 2px;cursor:pointer;
  list-style:none;font-size:13px;font-weight:500;color:var(--muted);touch-action:manipulation}
.settings summary::-webkit-details-marker{display:none}
.settings summary::before{content:'\203A';font-weight:700;transition:transform .15s}
.settings[open] summary::before{transform:rotate(90deg)}
.settings[open] summary{color:var(--text)}
.settings summary .meta{margin-left:auto;font-weight:400}
.field{display:flex;flex-direction:column;gap:8px;margin-top:6px}
.field .k{font-size:11.5px;color:var(--muted)}
.settings .field:last-of-type{padding-bottom:8px}

.tools{margin-top:12px}
.group+.group{margin-top:16px}
.group h3{margin-bottom:8px}
.items{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px}
.items button{appearance:none;min-height:44px;padding:11px 12px;background:var(--card2);
  border:1px solid var(--line);border-radius:10px;color:var(--text);font:inherit;font-size:13px;
  text-align:left;cursor:pointer;touch-action:manipulation;transition:border-color .15s,background .15s}
.items button:hover{border-color:#353b49}
.items button:active{background:#232833}
.items button.sel{border-color:var(--accent);background:var(--accent-soft)}

#msg{position:fixed;left:50%;bottom:92px;z-index:20;max-width:calc(100% - 32px);
  padding:9px 15px;background:#22262f;border:1px solid var(--line);border-radius:999px;
  font-size:12.5px;text-align:center;opacity:0;pointer-events:none;
  transform:translate(-50%,8px);transition:opacity .18s,transform .18s}
#msg.show{opacity:1;transform:translate(-50%,0)}

.foot{position:fixed;left:0;right:0;bottom:0;z-index:10;padding:12px 16px calc(12px + env(safe-area-inset-bottom));
  background:var(--bg);border-top:1px solid var(--line)}
.foot button{display:block;width:100%;max-width:520px;margin:0 auto;appearance:none;padding:14px;
  border:0;border-radius:12px;background:var(--danger);color:#fff;font:inherit;font-size:14.5px;
  font-weight:600;cursor:pointer;touch-action:manipulation;transition:filter .15s}
.foot button:hover{filter:brightness(1.06)}
.foot button:disabled{opacity:.45;cursor:default}

button:focus-visible,input:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
[hidden]{display:none!important}

@media(max-width:400px){
  .stat{padding:8px}
  .stat .v{font-size:13px}
  .key{font-size:12.5px}
  .badge{display:none}
  h1{font-size:15px}
}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
<body>

<header class="top">
  <div class="brand"><span class="dot" id="dot"></span><h1>RF Clown X</h1><span class="badge">Remote</span></div>
  <span class="link" id="link">Connecting</span>
</header>

<main>
  <section class="stats" aria-label="Device status">
    <div class="stat"><span class="k">State</span><span class="v" id="state">&mdash;</span></div>
    <div class="stat"><span class="k">Uptime</span><span class="v" id="up">&mdash;</span></div>
    <div class="stat"><span class="k">Memory</span><span class="v" id="heap">&mdash;</span></div>
    <div class="stat"><span class="k">Temp</span><span class="v" id="temp">&mdash;</span></div>
  </section>

  <section class="card">
    <div class="card-h">
      <h2>Display</h2>
      <div class="seg mini" role="group" aria-label="Mirror style">
        <button id="mCrisp" class="on">Crisp</button>
        <button id="mPixels">Pixels</button>
      </div>
    </div>
    <div class="screen">
      <div class="soft" id="soft"></div>
      <canvas id="oled" width="128" height="64" aria-label="Device display mirror" hidden></canvas>
    </div>
    <p class="hint" id="cap">Rendered from the live menu</p>

    <div class="pad">
      <button class="key" data-b="left" aria-label="Left">Left</button>
      <button class="key" data-b="select" aria-label="Select">Select</button>
      <button class="key" data-b="right" aria-label="Right">Right</button>
      <button class="key back" data-b="back" aria-label="Back">Back</button>
    </div>

    <details class="settings">
      <summary><span>Settings</span><span class="meta" id="setSum">&mdash;</span></summary>
      <div class="field">
        <span class="k">Auto-stop</span>
        <div class="seg" role="group" aria-label="Auto-stop timer">
          <button data-as="15000">15s</button>
          <button data-as="45000">45s</button>
          <button data-as="120000">2m</button>
          <button data-as="0">Off</button>
        </div>
        <p class="hint" id="fs">&mdash;</p>
      </div>
      <div class="field">
        <label class="k" for="bright">Brightness</label>
        <input id="bright" type="range" min="5" max="255" value="120">
      </div>
    </details>
  </section>

  <section class="card tools">
    <div class="card-h"><h2>Tools</h2><span class="meta">Tap to run</span></div>
    <div id="launcher"></div>
  </section>
</main>

<div id="msg" role="status" aria-live="polite"></div>
<div class="foot"><button id="panic" disabled>Stop everything</button></div>

<script>
(function () {
  var tok = '', brInit = false, last = null, msgTimer = null;
  var menu = null, st = null, mode = 'crisp', lastSig = '';

  function $(s) { return document.querySelector(s); }

  function say(t) {
    var m = $('#msg');
    m.textContent = t;
    m.classList.add('show');
    clearTimeout(msgTimer);
    msgTimer = setTimeout(function () { m.classList.remove('show'); }, 2400);
  }

  function dur(s) {
    var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), x = s % 60;
    return (h ? h + ':' + (m < 10 ? '0' : '') : '') + m + ':' + (x < 10 ? '0' : '') + x;
  }

  function get(p) { return fetch(p, { cache: 'no-store' }); }
  function post(p) { return fetch(p, { method: 'POST', headers: { 'X-Token': tok }, cache: 'no-store' }); }

  var cv = document.getElementById('oled');
  var cx = cv.getContext('2d');
  var img = cx.createImageData(128, 64);

  function paint() {
    if (mode !== 'pixels') return;
    if (document.hidden) { setTimeout(paint, 600); return; }
    get('/api/frame').then(function (r) {
      if (!r.ok) throw 0;
      return r.arrayBuffer();
    }).then(function (b) {
      var d = new Uint8Array(b), same = false;
      if (last && d.length === last.length) {
        same = true;
        for (var i = 0; i < 1024; i++) { if (d[i] !== last[i]) { same = false; break; } }
      }
      if (same) { setTimeout(paint, 150); return; }   // static screen: no repaint
      last = d;
      var p = img.data;
      for (var pg = 0; pg < 8; pg++) {
        for (var x = 0; x < 128; x++) {
          var v = d[pg * 128 + x];
          for (var k = 0; k < 8; k++) {
            var on = (v >> k) & 1, o = ((pg * 8 + k) * 128 + x) * 4;
            p[o] = on ? 236 : 8;
            p[o + 1] = on ? 238 : 9;
            p[o + 2] = on ? 242 : 12;
            p[o + 3] = 255;
          }
        }
      }
      cx.putImageData(img, 0, 0);
      setTimeout(paint, 150);
    }).catch(function () { last = null; setTimeout(paint, 700); });
  }

  function seg(v) {
    var b = document.querySelectorAll('[data-as]');
    for (var i = 0; i < b.length; i++) b[i].classList.toggle('on', +b[i].dataset.as === v);
  }

  function markSel(lvl, cat, item) {
    var b = document.querySelectorAll('#launcher button');
    for (var i = 0; i < b.length; i++) {
      b[i].classList.toggle('sel', lvl === 1 && +b[i].dataset.c === cat && +b[i].dataset.i === item);
    }
  }

  function setSum() {
    var b = document.getElementById('bright').value;
    document.getElementById('setSum').textContent =
      'Auto-stop ' + (st && st.as ? Math.round(st.as / 1000) + 's' : 'off') + ' \u00B7 ' + b;
  }

  function status() {
    if (document.hidden) return;
    get('/api/status').then(function (r) { if (!r.ok) throw 0; return r.json(); }).then(function (j) {
      tok = j.tok;
      $('#dot').className = 'dot ok';
      $('#link').textContent = 'Connected \u00B7 ' + j.cl + (j.cl === 1 ? ' client' : ' clients');
      $('#state').textContent = j.act ? j.tool : 'Idle';
      $('#state').className = 'v' + (j.act ? ' live' : '');
      $('#up').textContent = dur(j.up);
      $('#heap').textContent = Math.round(j.heap / 1024) + ' KB';
      $('#temp').textContent = (j.temp || 0).toFixed(1) + ' \u00B0C';
      $('#panic').disabled = false;
      if (!brInit) { $('#bright').value = j.bright; brInit = true; }
      st = j;
      seg(j.as);
      setSum();
      $('#fs').textContent = !j.as ? 'Auto-stop is off'
        : (j.act ? j.left + 's left' : 'Runs for up to ' + Math.round(j.as / 1000) + 's');
      markSel(j.lvl, j.cat, j.item);
      renderSoft();
    }).catch(function () {
      tok = '';
      $('#dot').className = 'dot bad';
      $('#link').textContent = 'Disconnected';
      $('#state').textContent = '\u2014';
      $('#state').className = 'v';
    });
  }

  function exec(c, i, name) {
    if (!tok) { say('Still connecting'); return; }
    post('/api/exec?c=' + c + '&i=' + i).then(function (r) {
      return r.text().then(function (t) { return { ok: r.ok, body: t }; });
    }).then(function (res) {
      if (res.ok) say('Started ' + name);
      else say(res.body === 'busy' ? 'A tool is already running' : 'Could not start ' + name);
    }).catch(function () { say('Connection lost'); });
  }

  function setMode(m) {
    mode = m;
    $('#mCrisp').classList.toggle('on', m === 'crisp');
    $('#mPixels').classList.toggle('on', m === 'pixels');
    $('#soft').hidden = m === 'pixels';
    $('#oled').hidden = m !== 'pixels';
    $('#cap').textContent = m === 'pixels'
      ? 'Exact frame the panel draws, 128 \u00D7 64'
      : 'Rendered from the live menu';
    if (m === 'pixels') { last = null; paint(); } else { lastSig = ''; renderSoft(); }
  }

  // Same layout the firmware draws, rebuilt from menu state at panel size.
  function renderSoft() {
    if (!menu || !st) return;
    if (st.lvl === 1 && !menu.cats[st.cat]) return;
    var sig = [st.lvl, st.cat, st.item, st.tool, st.act, st.ap, st.as, st.left, st.v,
               menu.cats.length].join('|');
    if (sig === lastSig) return;          // nothing changed: keep the DOM as is
    lastSig = sig;

    var host = $('#soft');
    host.textContent = '';

    var head = document.createElement('div');
    head.className = 'soft-head';
    var hl = document.createElement('span'), hr = document.createElement('span');

    if (st.act) {
      hl.textContent = st.tool || 'Running';
      hr.textContent = st.v || '';
      head.appendChild(hl); head.appendChild(hr); host.appendChild(head);

      var body = document.createElement('div');
      body.className = 'soft-body';
      var title = document.createElement('p'), line = document.createElement('p');
      title.className = 'soft-title'; line.className = 'soft-line';
      var bar = document.createElement('div'), fill = document.createElement('i');
      bar.className = 'soft-bar';

      if (st.tool === 'Remote AP') {
        title.textContent = st.ap ? 'On' : 'Off';
        line.textContent = st.ap ? '192.168.4.1' : 'No network';
        fill.style.width = st.ap ? '100%' : '0%';
      } else {
        title.textContent = 'Running';
        line.textContent = st.as ? st.left + 's before auto-stop' : 'No auto-stop';
        fill.style.width = st.as
          ? Math.min(100, Math.max(2, Math.round(100 * (1 - st.left * 1000 / st.as)))) + '%'
          : '100%';
      }
      bar.appendChild(fill);
      body.appendChild(title); body.appendChild(line); body.appendChild(bar);
      host.appendChild(body);
      return;
    }

    var names, sel, hint;
    if (st.lvl === 0) {
      names = menu.cats.map(function (c) { return c.n; });
      sel = st.cat;
      hint = 'S:open';
      hl.textContent = 'RF Clown X';
    } else {
      names = menu.cats[st.cat].i;
      sel = st.item;
      hint = 'hold:back';
      hl.textContent = menu.cats[st.cat].n;
    }
    hr.textContent = hint;
    head.appendChild(hl); head.appendChild(hr); host.appendChild(head);

    var start = Math.floor(sel / 4) * 4;
    for (var r = 0; r < 4; r++) {
      var idx = start + r;
      if (idx >= names.length) break;
      var row = document.createElement('div');
      row.className = 'soft-row' + (idx === sel ? ' on' : '');
      var label = document.createElement('span');
      label.textContent = names[idx];
      row.appendChild(label);
      if (st.lvl === 0) {
        var caret = document.createElement('span');
        caret.className = 'caret';
        caret.textContent = '>';
        row.appendChild(caret);
      }
      host.appendChild(row);
    }
  }

  function build(j) {
    menu = j;
    lastSig = '';
    renderSoft();
    var host = $('#launcher');
    host.innerHTML = '';
    j.cats.forEach(function (cat, ci) {
      var g = document.createElement('div');
      g.className = 'group';
      var h = document.createElement('h3');
      h.textContent = cat.n;
      g.appendChild(h);
      var grid = document.createElement('div');
      grid.className = 'items';
      cat.i.forEach(function (name, ii) {
        var b = document.createElement('button');
        b.textContent = name;
        b.dataset.c = ci;
        b.dataset.i = ii;
        b.onclick = function () { exec(ci, ii, name); };
        grid.appendChild(b);
      });
      g.appendChild(grid);
      host.appendChild(g);
    });
  }

  function loadMenu() {
    get('/api/menu').then(function (r) { if (!r.ok) throw 0; return r.json(); })
      .then(build).catch(function () { setTimeout(loadMenu, 1200); });
  }

  Array.prototype.forEach.call(document.querySelectorAll('[data-b]'), function (b) {
    b.onclick = function () {
      post('/api/input?b=' + b.dataset.b).catch(function () { say('Connection lost'); });
    };
  });

  Array.prototype.forEach.call(document.querySelectorAll('[data-as]'), function (b) {
    b.onclick = function () {
      post('/api/config?autostop=' + b.dataset.as).then(function (r) {
        if (!r.ok) return;
        seg(+b.dataset.as);
        setSum();
        say(b.dataset.as === '0' ? 'Auto-stop off'
          : 'Auto-stop ' + Math.round(+b.dataset.as / 1000) + 's');
      }).catch(function () { say('Connection lost'); });
    };
  });

  var br = document.getElementById('bright');
  br.onchange = function () {
    setSum();
    post('/api/config?bright=' + br.value).catch(function () { say('Connection lost'); });
  };

  document.getElementById('panic').onclick = function () {
    post('/api/stop').then(function (r) { if (r.ok) say('Stopped'); })
      .catch(function () { say('Connection lost'); });
  };

  $('#mCrisp').onclick = function () { setMode('crisp'); };
  $('#mPixels').onclick = function () { setMode('pixels'); };

  document.addEventListener('visibilitychange', function () {
    if (!document.hidden) { last = null; status(); }
  });

  loadMenu();
  status();
  setInterval(status, 1000);
})();
</script>
</body>
</html>)HTMLPAGE";

#endif // WEBUI_PAGE_H
