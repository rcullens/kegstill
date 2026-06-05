// index_html.h - Web UI as a PROGMEM raw string literal.
// Lives in a .h file so the Arduino IDE preprocessor does NOT mangle the
// raw string (.ino files get scanned for function prototypes and break on
// JS keywords like `function`). Header files are passed through untouched.
#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KEG STILL - GLITCH</title>
<script src="https://cdn.tailwindcss.com"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body { font-family: system-ui, sans-serif; }
.big-num { font-size: 3.2rem; line-height: 1; font-weight: 700; }
.section { background-color: #18181b; border: 1px solid #3f3f46; }
.metric { background-color: #27272a; }
.nav-tab.active { border-bottom: 3px solid #f59e0b; color: #f59e0b; }
.stage-pip { background:#27272a; border:1px solid #3f3f46; }
.stage-pip.active { background:#78350f; border-color:#f59e0b; color:#fde68a; }
.stage-pip.done   { background:#064e3b; border-color:#10b981; color:#a7f3d0; }
.modal-bg { background: rgba(0,0,0,0.65); }
input[type=number]::-webkit-inner-spin-button { opacity: 1; }
</style>
</head>
<body class="bg-zinc-950 text-zinc-200">
<div class="max-w-7xl mx-auto p-6">
  <div class="flex items-center justify-between mb-6">
    <div>
      <h1 class="text-5xl font-bold tracking-tighter">KEG STILL</h1>
      <p class="text-zinc-500">GLITCH EDITION - CQ60 BLE - 4-STAGE</p>
    </div>
    <div class="flex items-center gap-x-3">
      <div id="ble-pill" class="px-3 py-1.5 rounded-full text-xs font-semibold bg-zinc-900 border border-zinc-700">BLE: ?</div>
      <div id="status-pill" class="px-4 py-1.5 rounded-full text-sm font-semibold flex items-center gap-x-2 bg-zinc-900 border border-zinc-700">
        <div class="w-2 h-2 rounded-full bg-emerald-500 animate-pulse"></div>
        <span id="status-text">IDLE</span>
      </div>
      <button onclick="doEStop()" class="bg-red-600 hover:bg-red-700 px-8 py-3 rounded-2xl font-bold text-lg">E-STOP</button>
    </div>
  </div>

  <!-- STAGE STRIP -->
  <div class="grid grid-cols-5 gap-2 mb-6 text-center text-xs uppercase tracking-widest">
    <div id="stage-1" class="stage-pip py-3 rounded-2xl">HEATUP</div>
    <div id="stage-2" class="stage-pip py-3 rounded-2xl">HEADS</div>
    <div id="stage-3" class="stage-pip py-3 rounded-2xl">HEARTS</div>
    <div id="stage-4" class="stage-pip py-3 rounded-2xl">TAILS</div>
    <div id="stage-5" class="stage-pip py-3 rounded-2xl">SHUTDOWN</div>
  </div>

  <div class="grid grid-cols-1 lg:grid-cols-12 gap-6">
    <div class="lg:col-span-5 section rounded-3xl p-8">
      <div class="grid grid-cols-2 gap-6">
        <div class="metric rounded-2xl p-6 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">VAPOR TEMP</div>
          <div class="flex items-baseline gap-x-2">
            <span id="temp-value" class="big-num text-white">--</span>
            <span class="text-3xl text-zinc-400">&deg;F</span>
          </div>
          <div id="temp-c-small" class="text-sm text-zinc-500 mt-1">-- &deg;C</div>
          <div class="text-xs text-amber-400 mt-1">Target: <span id="target-temp">180</span>&deg;F</div>
        </div>

        <div class="metric rounded-2xl p-6 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">POWER</div>
          <div class="flex items-baseline gap-x-1">
            <span id="power-value" class="big-num text-white">0</span>
            <span class="text-2xl text-zinc-400">%</span>
          </div>
          <input type="range" id="power-slider" min="0" max="100" step="1" value="0" class="w-full accent-amber-500 mt-4" oninput="setPower(this.value)">
        </div>

        <div class="metric rounded-2xl p-6 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">EST. ABV</div>
          <div class="flex items-baseline gap-x-1">
            <span id="abv-value" class="big-num text-emerald-400">0</span>
            <span class="text-2xl text-zinc-400">%</span>
          </div>
        </div>

        <div class="metric rounded-2xl p-6 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">ELAPSED</div>
          <div id="elapsed-value" class="big-num text-white tabular-nums">00:00:00</div>
        </div>
      </div>

      <div class="mt-6 grid grid-cols-2 gap-4">
        <button onclick="startDistill()" class="bg-emerald-600 hover:bg-emerald-500 text-white font-bold py-6 rounded-3xl text-2xl">DISTILL THIS SHIT!</button>
        <button onclick="stopRun()" class="bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 py-6 rounded-3xl font-semibold">STOP RUN</button>
      </div>
      <div class="mt-3 grid grid-cols-2 gap-4">
        <button onclick="advanceStage()" class="bg-amber-700 hover:bg-amber-600 py-3 rounded-2xl text-sm font-semibold">SKIP TO NEXT STAGE</button>
        <button onclick="resetSession()" class="bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 py-3 rounded-2xl text-sm">RESET SESSION</button>
      </div>
    </div>

    <div class="lg:col-span-7 section rounded-3xl p-6">
      <div class="flex justify-between mb-2 px-2">
        <div class="font-semibold">LIVE DATA</div>
        <div class="text-xs text-zinc-500">Temp &deg;F / Power %</div>
      </div>
      <div style="position: relative; height: 320px; width: 100%; overflow: hidden;">
        <canvas id="runChart"></canvas>
      </div>
    </div>
  </div>

  <div class="mt-8">
    <div class="flex border-b border-zinc-800 text-sm overflow-x-auto">
      <div onclick="switchTab(0)" class="nav-tab active cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-0">PROFILES</div>
      <div onclick="switchTab(1)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-1">BATCH INFO</div>
      <div onclick="switchTab(2)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-2">CONTROLS</div>
      <div onclick="switchTab(3)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-3">SYSTEM</div>
    </div>

    <div id="tab-content-0" class="tab-content section rounded-b-3xl p-8">
      <div class="flex justify-between mb-6">
        <div class="font-semibold text-xl">Distillation Profiles</div>
        <button onclick="openProfileModal()" class="px-5 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-sm">+ NEW PROFILE</button>
      </div>
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4" id="profiles-grid"></div>
    </div>

    <div id="tab-content-1" class="tab-content section rounded-b-3xl p-8 hidden">
      <div class="max-w-2xl">
        <div class="grid grid-cols-1 md:grid-cols-2 gap-x-8 gap-y-6">
          <div>
            <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">WASH ABV %</label>
            <input id="batch-abv" type="number" step="0.1" value="12.5" class="w-full bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3 text-2xl font-semibold">
          </div>
          <div>
            <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">VOLUME</label>
            <div class="flex gap-x-3">
              <input id="batch-volume" type="number" step="0.5" value="12" class="flex-1 bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3 text-2xl font-semibold">
              <select id="batch-unit" class="bg-zinc-900 border border-zinc-700 rounded-2xl px-4 text-sm font-semibold">
                <option value="gal">US GAL</option>
                <option value="L">LITERS</option>
              </select>
            </div>
          </div>
        </div>
        <div class="mt-6">
          <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">INGREDIENTS</label>
          <textarea id="batch-ingredients" rows="2" class="w-full bg-zinc-900 border border-zinc-700 rounded-3xl px-5 py-3 text-sm"></textarea>
        </div>
        <div class="mt-4">
          <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">NOTES</label>
          <textarea id="batch-notes" rows="2" class="w-full bg-zinc-900 border border-zinc-700 rounded-3xl px-5 py-3 text-sm"></textarea>
        </div>
        <div class="mt-6">
          <button onclick="saveBatchInfo()" class="px-8 py-3 bg-emerald-600 hover:bg-emerald-500 rounded-2xl font-bold">SAVE BATCH INFO</button>
        </div>
      </div>
    </div>

    <div id="tab-content-2" class="tab-content section rounded-b-3xl p-8 hidden">
      <label class="flex items-center gap-x-3 cursor-pointer">
        <input type="checkbox" id="auto-toggle" onchange="toggleAutomation()" class="accent-amber-500 w-5 h-5">
        <span class="font-semibold">AUTOMATION ENABLED (4-stage state machine)</span>
      </label>
      <div class="mt-4 text-xs text-zinc-400 space-y-1">
        <div>HEATUP &rarr; max power until target-5&deg;F.</div>
        <div>HEADS &rarr; reduced power for set duration (slow foreshots).</div>
        <div>HEARTS &rarr; P-control around target temp (Kp gain).</div>
        <div>TAILS &rarr; reduced power, monitoring for cut temp.</div>
        <div>SHUTDOWN &rarr; cut temp reached, SSR off, snapshot cleared.</div>
      </div>
      <button onclick="exportData()" class="mt-6 px-6 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-sm">EXPORT SESSION DATA (CSV)</button>
    </div>

    <div id="tab-content-3" class="tab-content section rounded-b-3xl p-8 hidden">
      <div class="font-semibold text-xl mb-4">WiFi Credentials</div>
      <div class="text-xs text-zinc-500 mb-4">Stored in NVS. Device reboots after save.</div>
      <div class="max-w-md space-y-3">
        <input id="wifi-ssid" type="text" placeholder="SSID" class="w-full bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3">
        <input id="wifi-pass" type="text" placeholder="Password" class="w-full bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3">
        <button onclick="saveWifi()" class="px-6 py-3 bg-amber-600 hover:bg-amber-500 rounded-2xl font-bold">SAVE &amp; REBOOT</button>
      </div>
    </div>
  </div>
</div>

<!-- RESUME MODAL -->
<div id="resume-modal" class="hidden fixed inset-0 modal-bg z-50 flex items-center justify-center p-4">
  <div class="bg-zinc-900 border border-amber-600 rounded-3xl p-8 max-w-md w-full">
    <div class="text-2xl font-bold text-amber-400 mb-2">Resume Previous Run?</div>
    <div class="text-sm text-zinc-300 mb-4">A run was active before the device powered down.</div>
    <div class="text-xs text-zinc-400 mb-6" id="resume-info"></div>
    <div class="grid grid-cols-2 gap-3">
      <button onclick="resumeRun()" class="bg-emerald-600 hover:bg-emerald-500 py-3 rounded-2xl font-bold">RESUME</button>
      <button onclick="dismissResume()" class="bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 py-3 rounded-2xl font-semibold">DISCARD</button>
    </div>
  </div>
</div>

<!-- NEW/EDIT PROFILE MODAL -->
<div id="profile-modal" class="hidden fixed inset-0 modal-bg z-50 flex items-center justify-center p-4">
  <div class="bg-zinc-900 border border-zinc-700 rounded-3xl p-6 max-w-2xl w-full max-h-screen overflow-y-auto">
    <div class="text-2xl font-bold mb-4">New Profile</div>
    <div class="grid grid-cols-2 gap-4 text-sm">
      <div class="col-span-2">
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Name</label>
        <input id="pf-name" type="text" value="Custom Run" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Target &deg;F (hearts)</label>
        <input id="pf-target" type="number" step="0.5" value="172" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Max power %</label>
        <input id="pf-max" type="number" step="1" value="80" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Kp gain</label>
        <input id="pf-kp" type="number" step="0.1" value="4" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Cut temp &deg;F</label>
        <input id="pf-cut" type="number" step="0.5" value="195" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Heatup power %</label>
        <input id="pf-heatup" type="number" step="1" value="100" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Heads power %</label>
        <input id="pf-heads" type="number" step="1" value="25" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Heads duration (min)</label>
        <input id="pf-heads-min" type="number" step="1" value="15" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Tails start &deg;F</label>
        <input id="pf-tailst" type="number" step="0.5" value="178" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Tails power %</label>
        <input id="pf-tailsp" type="number" step="1" value="35" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
    </div>
    <div class="mt-6 grid grid-cols-2 gap-3">
      <button onclick="saveNewProfile()" class="bg-emerald-600 hover:bg-emerald-500 py-3 rounded-2xl font-bold">SAVE PROFILE</button>
      <button onclick="closeProfileModal()" class="bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 py-3 rounded-2xl">CANCEL</button>
    </div>
  </div>
</div>

<script>
var chart = null, tempData = [], powerData = [], timeLabels = [];
var currentProfileIndex = 0;

function initChart() {
  var ctx = document.getElementById('runChart');
  chart = new Chart(ctx, {
    type: 'line',
    data: { labels: timeLabels, datasets: [
      { label: 'Temp F', data: tempData,  borderColor: '#f59e0b', borderWidth: 2, tension: 0.3, yAxisID: 'y',  pointRadius: 0 },
      { label: 'Power %', data: powerData, borderColor: '#64748b', borderWidth: 2, tension: 0.3, yAxisID: 'y1', pointRadius: 0 }
    ]},
    options: {
      responsive: true, maintainAspectRatio: false, animation: false,
      scales: {
        y:  { position: 'left',  min: 60, max: 220, grid: { color: '#27272a' }, ticks: { color: '#a1a1aa' } },
        y1: { position: 'right', min: 0,  max: 100, grid: { drawOnChartArea: false }, ticks: { color: '#a1a1aa' } },
        x:  { grid: { color: '#27272a' }, ticks: { color: '#a1a1aa', maxRotation: 0, autoSkip: true } }
      },
      plugins: { legend: { labels: { color: '#a1a1aa' } } }
    }
  });
}

function fmtTime(sec) {
  var h = Math.floor(sec / 3600);
  var m = Math.floor((sec % 3600) / 60);
  var s = sec % 60;
  return String(h).padStart(2,'0') + ':' + String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0');
}

function pushChart(tempF, power, elapsedSec) {
  var mins = Math.floor(elapsedSec / 60);
  var label = mins + ':' + String(elapsedSec % 60).padStart(2,'0');
  timeLabels.push(label); tempData.push(tempF); powerData.push(power);
  if (timeLabels.length > 360) { timeLabels.shift(); tempData.shift(); powerData.shift(); }
  chart.update('none');
}

function updateStagePips(stageIdx) {
  for (var i = 1; i <= 5; i++) {
    var el = document.getElementById('stage-' + i);
    el.classList.remove('active', 'done');
    if (i < stageIdx) el.classList.add('done');
    if (i === stageIdx) el.classList.add('active');
  }
}

function updateDashboard(data) {
  var tempEl = document.getElementById('temp-value');
  var tempCSmall = document.getElementById('temp-c-small');

  if (data.tempF !== undefined && data.tempF !== null) {
    tempEl.innerText = data.tempF;
    tempCSmall.innerText = data.tempC + ' C';
    tempEl.style.color = '#fff';
  } else {
    tempEl.innerText = 'NO';
    tempCSmall.innerText = 'PROBE';
    tempEl.style.color = '#f87171';
  }

  var blePill = document.getElementById('ble-pill');
  if (data.bleOk) { blePill.innerText = 'BLE OK'; blePill.className = 'px-3 py-1.5 rounded-full text-xs font-semibold bg-emerald-950 border border-emerald-600 text-emerald-400'; }
  else { blePill.innerText = 'BLE WAIT'; blePill.className = 'px-3 py-1.5 rounded-full text-xs font-semibold bg-red-950 border border-red-600 text-red-400'; }

  document.getElementById('power-value').innerText = Math.round(data.power);
  document.getElementById('abv-value').innerText = Math.round(data.abv);
  document.getElementById('target-temp').innerText = data.targetTemp || 180;
  document.getElementById('elapsed-value').innerText = fmtTime(data.elapsed || 0);

  var slider = document.getElementById('power-slider');
  if (slider && document.activeElement !== slider) slider.value = data.power;

  var pill = document.getElementById('status-pill');
  var txt = document.getElementById('status-text');
  txt.innerText = data.status + (data.stage && data.stage !== 'IDLE' ? ' / ' + data.stage : '');
  if (data.estop) pill.className = 'px-4 py-1.5 rounded-full text-sm font-semibold flex items-center gap-x-2 bg-red-950 border border-red-600 text-red-400';
  else if (data.status === 'AUTO') pill.className = 'px-4 py-1.5 rounded-full text-sm font-semibold flex items-center gap-x-2 bg-emerald-950 border border-emerald-600 text-emerald-400';
  else if (data.status === 'MANUAL') pill.className = 'px-4 py-1.5 rounded-full text-sm font-semibold flex items-center gap-x-2 bg-amber-950 border border-amber-600 text-amber-400';
  else pill.className = 'px-4 py-1.5 rounded-full text-sm font-semibold flex items-center gap-x-2 bg-zinc-900 border border-zinc-700';

  updateStagePips(data.stageIdx);

  if (data.status !== 'IDLE' && chart && data.tempF) pushChart(data.tempF, data.power, data.elapsed);
  var autoToggle = document.getElementById('auto-toggle');
  if (autoToggle) autoToggle.checked = data.automation;

  currentProfileIndex = data.profileIdx;

  // resume modal
  var modal = document.getElementById('resume-modal');
  if (data.resumePending && data.status === 'IDLE') {
    document.getElementById('resume-info').innerText = 'Previous elapsed: ' + fmtTime(data.resumeElapsed);
    modal.classList.remove('hidden');
  } else {
    modal.classList.add('hidden');
  }
}

function setPower(val) {
  document.getElementById('power-value').innerText = Math.round(parseFloat(val));
  fetch('/api/power', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({power: parseFloat(val)}) });
}

function startDistill() {
  fetch('/api/start', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({resume:false})}).then(function(r){
    if (!r.ok) r.json().then(function(j){ alert('Cannot start: ' + (j.error||'unknown')); });
  });
  if (chart) { chart.data.labels = []; chart.data.datasets[0].data = []; chart.data.datasets[1].data = []; chart.update(); }
  tempData = []; powerData = []; timeLabels = [];
}

function resumeRun() {
  fetch('/api/start', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({resume:true})});
  document.getElementById('resume-modal').classList.add('hidden');
}

function dismissResume() {
  fetch('/api/resume/dismiss', {method:'POST'});
  document.getElementById('resume-modal').classList.add('hidden');
}

function stopRun() { if (confirm('Stop current run?')) fetch('/api/stop', {method:'POST'}); }
function doEStop() { if (!confirm('E-STOP the still?')) return; fetch('/api/estop', {method: 'POST'}); }
function toggleAutomation() { var en = document.getElementById('auto-toggle').checked; fetch('/api/automation', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({enabled:en}) }); }
function advanceStage() { fetch('/api/advance', {method:'POST'}); }

function switchTab(tab) {
  var tabs = document.querySelectorAll('.tab-content');
  for (var i = 0; i < tabs.length; i++) tabs[i].classList.add('hidden');
  document.getElementById('tab-content-' + tab).classList.remove('hidden');
  var navs = document.querySelectorAll('.nav-tab');
  for (var j = 0; j < navs.length; j++) navs[j].classList.toggle('active', j === tab);
}

function loadProfiles() {
  fetch('/api/profiles').then(function(r){return r.json();}).then(function(data){
    var grid = document.getElementById('profiles-grid');
    grid.innerHTML = '';
    data.forEach(function(p, idx){
      var div = document.createElement('div');
      var active = idx === currentProfileIndex;
      div.className = 'p-5 rounded-2xl border transition ' + (active ? 'border-amber-500 bg-zinc-900' : 'border-zinc-700 hover:border-zinc-500');
      div.innerHTML = '<div class="flex justify-between items-start"><div class="font-bold text-lg cursor-pointer">' + p.name + '</div>' +
        '<button class="text-xs text-red-400 hover:text-red-300">DEL</button></div>' +
        '<div class="text-xs text-zinc-400 mt-2">Target ' + p.targetTemp + 'F / Max ' + p.maxPower + '% / Kp ' + p.kp + '</div>' +
        '<div class="text-xs text-zinc-500 mt-1">Heads ' + p.headsPower + '% for ' + Math.round(p.headsDuration_s/60) + 'min</div>' +
        '<div class="text-xs text-zinc-500">Tails ' + p.tailsPower + '% @ ' + p.tailsTemp + 'F, Cut ' + p.cutTemp + 'F</div>';
      var label = div.querySelector('.font-bold');
      label.onclick = function(){ loadProfile(idx); };
      var del = div.querySelector('button');
      del.onclick = function(e){ e.stopPropagation(); if(confirm('Delete "'+p.name+'"?')) deleteProfile(idx); };
      grid.appendChild(div);
    });
  });
}

function loadProfile(idx) {
  currentProfileIndex = idx;
  fetch('/api/profile/load', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({index: idx}) }).then(loadProfiles);
}

function deleteProfile(idx) {
  fetch('/api/profile/delete', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({index: idx}) }).then(loadProfiles);
}

function openProfileModal() { document.getElementById('profile-modal').classList.remove('hidden'); }
function closeProfileModal() { document.getElementById('profile-modal').classList.add('hidden'); }

function saveNewProfile() {
  var payload = {
    name: document.getElementById('pf-name').value,
    targetTemp: parseFloat(document.getElementById('pf-target').value),
    maxPower:   parseFloat(document.getElementById('pf-max').value),
    kp:         parseFloat(document.getElementById('pf-kp').value),
    cutTemp:    parseFloat(document.getElementById('pf-cut').value),
    heatupPower: parseFloat(document.getElementById('pf-heatup').value),
    headsPower:  parseFloat(document.getElementById('pf-heads').value),
    headsDuration_s: Math.round(parseFloat(document.getElementById('pf-heads-min').value) * 60),
    tailsTemp:  parseFloat(document.getElementById('pf-tailst').value),
    tailsPower: parseFloat(document.getElementById('pf-tailsp').value)
  };
  fetch('/api/profile/new', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload) }).then(function(){
    closeProfileModal(); loadProfiles();
  });
}

function saveBatchInfo() {
  var payload = {
    washABV: parseFloat(document.getElementById('batch-abv').value),
    volume: parseFloat(document.getElementById('batch-volume').value),
    unit: document.getElementById('batch-unit').value,
    ingredients: document.getElementById('batch-ingredients').value,
    notes: document.getElementById('batch-notes').value
  };
  fetch('/api/batch', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) }).then(function(){ alert('Batch info saved'); });
}

function saveWifi() {
  var ssid = document.getElementById('wifi-ssid').value;
  var pass = document.getElementById('wifi-pass').value;
  if (!ssid) { alert('SSID required'); return; }
  if (!confirm('Save WiFi and reboot?')) return;
  fetch('/api/wifi', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid:ssid, pass:pass}) });
}

function exportData() { window.open('/api/export', '_blank'); }
function resetSession() { if (confirm('Reset session?')) fetch('/api/reset', {method: 'POST'}); }

window.onload = function() {
  initChart();
  loadProfiles();

  setInterval(function() {
    fetch('/api/status').then(function(r){return r.json();}).then(updateDashboard).catch(function(){});
  }, 650);

  fetch('/api/batch').then(function(r){return r.json();}).then(function(b) {
    document.getElementById('batch-abv').value = b.washABV || 10;
    document.getElementById('batch-volume').value = b.volume || 10;
    document.getElementById('batch-unit').value = b.unit || 'gal';
    document.getElementById('batch-ingredients').value = b.ingredients || '';
    document.getElementById('batch-notes').value = b.notes || '';
  });
};
</script>
</body>
</html>
)HTML";
