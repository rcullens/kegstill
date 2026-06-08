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

      <!-- VALVE STRIP -->
      <div class="mt-5 metric rounded-2xl p-5 border border-zinc-700">
        <div class="flex justify-between items-baseline mb-2">
          <div class="text-xs uppercase tracking-widest text-zinc-500">BALL VALVE
            <span id="valve-cal-pill" class="ml-2 text-[10px] px-2 py-0.5 rounded-full bg-zinc-800 border border-zinc-700">uncalibrated</span>
            <span id="valve-state-pill" class="ml-1 text-[10px] px-2 py-0.5 rounded-full bg-zinc-800 border border-zinc-700">IDLE</span>
          </div>
          <div>
            <span id="valve-value" class="text-3xl font-bold text-sky-400">0</span>
            <span class="text-zinc-400 ml-1">%</span>
            <span id="valve-target-display" class="text-zinc-600 ml-2 text-sm"></span>
          </div>
        </div>
        <input type="range" id="valve-slider" min="0" max="100" step="1" value="0" class="w-full accent-sky-500" oninput="setValve(this.value)">
        <div class="flex items-center justify-between mt-3 text-xs">
          <label class="flex items-center gap-x-2 cursor-pointer">
            <input type="checkbox" id="valve-auto" onchange="toggleValveAuto()" class="accent-sky-500">
            <span>auto-follow stage</span>
          </label>
          <div class="text-zinc-500"><span id="valve-cal-times">Open --ms / Close --ms</span></div>
        </div>
        <div id="valve-fault-row" class="hidden mt-3 p-2 rounded-xl bg-red-950 border border-red-700 text-xs text-red-300 flex justify-between items-center">
          <span><span class="font-bold">FAULT:</span> <span id="valve-fault-msg">-</span></span>
          <button onclick="valveCmd('clearFault')" class="px-3 py-1 bg-red-800 hover:bg-red-700 rounded-lg text-[11px] font-semibold">CLEAR</button>
        </div>
        <div class="mt-3 grid grid-cols-4 gap-2 text-xs">
          <button onclick="valveCmd('close')" class="py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl">JOG CLOSE</button>
          <button onclick="valveCmd('stop')"  class="py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl">STOP</button>
          <button onclick="valveCmd('open')"  class="py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl">JOG OPEN</button>
          <button onclick="valveCmd('rehome')" class="py-2 bg-amber-700 hover:bg-amber-600 border border-amber-500 rounded-xl">RE-HOME</button>
        </div>
        <div class="mt-3 grid grid-cols-[1fr_1fr_auto] gap-2 text-xs items-center">
          <label class="text-zinc-500">Open time (ms)
            <input id="valve-open-input" type="number" min="500" max="120000" step="100" value="8000" oninput="this.dataset.dirty='1'" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-1.5">
          </label>
          <label class="text-zinc-500">Close time (ms)
            <input id="valve-close-input" type="number" min="500" max="120000" step="100" value="8000" oninput="this.dataset.dirty='1'" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-1.5">
          </label>
          <button onclick="saveValveCal()" class="px-4 py-2 mt-5 bg-emerald-700 hover:bg-emerald-600 border border-emerald-500 rounded-xl font-semibold">SAVE CAL</button>
        </div>
        <div class="mt-2 text-[11px] text-zinc-500">
          To calibrate: press <strong>JOG CLOSE</strong> and wait until the valve has fully stopped. Press <strong>JOG OPEN</strong> with a stopwatch — note the time until it stops humming. That's your open time. Repeat with JOG CLOSE for close time. Type both in and SAVE CAL.
        </div>
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
      <div onclick="switchTab(3)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-3">BLE PROBE</div>
      <div onclick="switchTab(4)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-4">DILUTE</div>
      <div onclick="switchTab(5)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-5">HISTORY</div>
      <div onclick="switchTab(6)" class="nav-tab cursor-pointer px-8 py-3 font-semibold whitespace-nowrap" id="tab-6">SYSTEM</div>
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
      <div class="flex justify-between mb-6">
        <div>
          <div class="font-semibold text-xl">BLE Probe Selection</div>
          <div class="text-xs text-zinc-500 mt-1">Pick which advertising device to use as the vapor probe. Selection persists across reboots.</div>
        </div>
        <div class="flex gap-x-2">
          <button onclick="loadBleScan()" class="px-4 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-sm">REFRESH</button>
          <button onclick="clearBleScan()" class="px-4 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-sm">CLEAR LIST</button>
        </div>
      </div>

      <div id="ble-target-row" class="mb-5 flex justify-between items-center p-4 rounded-2xl border border-amber-700 bg-amber-950/30">
        <div>
          <div class="text-xs uppercase tracking-widest text-amber-400">CURRENT TARGET</div>
          <div id="ble-target-display" class="font-mono text-sm mt-1">(auto - any CQ60 in range)</div>
        </div>
        <button onclick="selectBleDevice('')" class="px-4 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-xs">RESET TO AUTO</button>
      </div>

      <div id="ble-list" class="space-y-2"></div>
      <div id="ble-empty" class="text-zinc-500 text-sm hidden">No devices advertising yet. Wait a few seconds and refresh — or wake your CQ60.</div>
    </div>

    <div id="tab-content-4" class="tab-content section rounded-b-3xl p-8 hidden">
      <div class="max-w-3xl">
        <div class="font-semibold text-xl mb-2">Dilution Calculator</div>
        <div class="text-xs text-zinc-500 mb-6">Mixing math is the simple <code>V&middot;A = V&middot;A</code> approximation. Real ethanol/water has a small volume contraction (~3%); for precision use TTB Table 6.</div>

        <div class="grid grid-cols-1 md:grid-cols-2 gap-x-8 gap-y-5">
          <div>
            <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">Source ABV</label>
            <div class="flex gap-x-2 mb-2 text-xs">
              <button id="dil-src-wash" onclick="dilUseSource('wash')" class="px-3 py-1 rounded-xl bg-zinc-800 border border-zinc-700">From batch (wash %)</button>
              <button id="dil-src-meas" onclick="dilUseSource('meas')" class="px-3 py-1 rounded-xl bg-amber-700 border border-amber-500">Measured (hydrometer)</button>
            </div>
            <div class="flex gap-x-3">
              <input id="dil-src-val" type="number" step="0.1" value="60" class="flex-1 bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3 text-2xl font-semibold">
              <select id="dil-src-unit" onchange="dilUpdate()" class="bg-zinc-900 border border-zinc-700 rounded-2xl px-3 text-sm">
                <option value="abv">% ABV</option>
                <option value="proof">PROOF</option>
              </select>
            </div>
          </div>

          <div>
            <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">Target ABV</label>
            <div class="h-7"></div>
            <div class="flex gap-x-3">
              <input id="dil-tgt-val" type="number" step="0.1" value="40" class="flex-1 bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3 text-2xl font-semibold">
              <select id="dil-tgt-unit" onchange="dilUpdate()" class="bg-zinc-900 border border-zinc-700 rounded-2xl px-3 text-sm">
                <option value="abv">% ABV</option>
                <option value="proof">PROOF</option>
              </select>
            </div>
          </div>

          <div>
            <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">Desired Final Volume (this jar)</label>
            <div class="flex gap-x-3">
              <input id="dil-vol-val" type="number" step="0.1" value="750" class="flex-1 bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3 text-2xl font-semibold">
              <select id="dil-vol-unit" onchange="dilUpdate()" class="bg-zinc-900 border border-zinc-700 rounded-2xl px-3 text-sm">
                <option value="oz">FL OZ</option>
                <option value="qt">QUARTS</option>
                <option value="gal">US GAL</option>
                <option value="ml" selected>mL</option>
                <option value="L">LITERS</option>
              </select>
            </div>
            <div class="mt-2 text-xs text-zinc-500">Default starts metric; switch unit any time.</div>
          </div>

          <div>
            <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1.5">Diluent</label>
            <input id="dil-water-label" type="text" value="distilled water" class="w-full bg-zinc-900 border border-zinc-700 rounded-2xl px-5 py-3 text-sm">
          </div>
        </div>

        <button onclick="dilUpdate()" class="mt-6 px-8 py-3 bg-emerald-600 hover:bg-emerald-500 rounded-2xl font-bold">CALCULATE</button>

        <div id="dil-result" class="mt-8 hidden">
          <div class="grid grid-cols-1 md:grid-cols-3 gap-4 text-center">
            <div class="metric p-5 rounded-2xl border border-amber-700">
              <div class="text-xs uppercase tracking-widest text-amber-400 mb-2">DISTILLATE</div>
              <div class="text-3xl font-bold" id="dil-r-spirit">--</div>
              <div class="text-xs text-zinc-500 mt-1" id="dil-r-spirit-alt">--</div>
            </div>
            <div class="metric p-5 rounded-2xl border border-sky-700">
              <div class="text-xs uppercase tracking-widest text-sky-400 mb-2" id="dil-r-water-lbl">WATER TO ADD</div>
              <div class="text-3xl font-bold" id="dil-r-water">--</div>
              <div class="text-xs text-zinc-500 mt-1" id="dil-r-water-alt">--</div>
            </div>
            <div class="metric p-5 rounded-2xl border border-emerald-700">
              <div class="text-xs uppercase tracking-widest text-emerald-400 mb-2">FINAL JAR</div>
              <div class="text-3xl font-bold" id="dil-r-final">--</div>
              <div class="text-xs text-zinc-500 mt-1" id="dil-r-final-alt">--</div>
            </div>
          </div>
          <div id="dil-warn" class="mt-4 text-sm text-red-400 hidden"></div>
        </div>
      </div>
    </div>

    <div id="tab-content-5" class="tab-content section rounded-b-3xl p-8 hidden">
      <div class="flex justify-between mb-6">
        <div class="font-semibold text-xl">Run History</div>
        <button onclick="loadHistory()" class="px-5 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-sm">REFRESH</button>
      </div>
      <div id="history-list" class="space-y-3"></div>
      <div id="history-viewer" class="mt-8 hidden">
        <div class="flex justify-between items-center mb-3">
          <div class="font-semibold" id="history-viewer-title">Run</div>
          <div class="flex gap-x-2">
            <button onclick="closeHistoryViewer()" class="px-4 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-2xl text-xs">CLOSE</button>
          </div>
        </div>
        <div style="position: relative; height: 320px; width: 100%; overflow: hidden;">
          <canvas id="historyChart"></canvas>
        </div>
        <pre id="history-batch" class="mt-4 text-xs text-zinc-400 whitespace-pre-wrap"></pre>
      </div>
    </div>

    <div id="tab-content-6" class="tab-content section rounded-b-3xl p-8 hidden">
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
      <div class="col-span-2 mt-2 text-xs uppercase tracking-widest text-sky-400 border-t border-zinc-800 pt-3">Ball Valve per stage (0-100%)</div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Valve - Heatup %</label>
        <input id="pf-v-heatup" type="number" min="0" max="100" step="1" value="100" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Valve - Heads %</label>
        <input id="pf-v-heads" type="number" min="0" max="100" step="1" value="30" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Valve - Hearts %</label>
        <input id="pf-v-hearts" type="number" min="0" max="100" step="1" value="80" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
      </div>
      <div>
        <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Valve - Tails %</label>
        <input id="pf-v-tails" type="number" min="0" max="100" step="1" value="50" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-4 py-2">
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
  return String(h).padStart(2,'0') + ':' + String(m).padStart(2,'0');
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
  var abvEl = document.getElementById('abv-value');

  if (data.tempF !== undefined && data.tempF !== null) {
    tempEl.innerText = data.tempF;
    tempCSmall.innerText = data.tempC + ' C';
    tempEl.style.color = '#fff';
  } else {
    tempEl.innerText = '--';
    tempCSmall.innerText = 'no probe';
    tempEl.style.color = '#52525b';
  }

  if (data.abv !== undefined && data.abv !== null) {
    abvEl.innerText = Math.round(data.abv);
    abvEl.style.color = '#34d399';
  } else {
    abvEl.innerText = '--';
    abvEl.style.color = '#52525b';
  }

  var blePill = document.getElementById('ble-pill');
  if (data.bleOk) { blePill.innerText = 'BLE OK'; blePill.className = 'px-3 py-1.5 rounded-full text-xs font-semibold bg-emerald-950 border border-emerald-600 text-emerald-400'; }
  else { blePill.innerText = 'BLE WAIT'; blePill.className = 'px-3 py-1.5 rounded-full text-xs font-semibold bg-red-950 border border-red-600 text-red-400'; }

  document.getElementById('power-value').innerText = Math.round(data.power);
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

  // valve % comes from /api/status quickly; rich state polled separately
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

function setValve(val) {
  var v = parseInt(val, 10);
  document.getElementById('valve-value').innerText = v;
  fetch('/api/valve', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({pos: v}) });
}
function toggleValveAuto() {
  var en = document.getElementById('valve-auto').checked;
  fetch('/api/valve', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({auto: en}) });
}
function valveCmd(cmd) {
  fetch('/api/valve', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({cmd: cmd}) });
}

function updateValveStatus(v) {
  if (!v) return;
  document.getElementById('valve-value').innerText = Math.round(v.position);
  document.getElementById('valve-target-display').innerText =
    (v.state === 'OPENING' || v.state === 'CLOSING' || v.state === 'HOMING') ? ('-> ' + v.target + '%') : '';
  var statePill = document.getElementById('valve-state-pill');
  statePill.innerText = v.state;
  statePill.className = 'ml-1 text-[10px] px-2 py-0.5 rounded-full border ' +
    (v.state === 'FAULT' ? 'bg-red-950 border-red-600 text-red-400' :
     (v.state === 'IDLE' ? 'bg-zinc-800 border-zinc-700' :
      'bg-sky-950 border-sky-600 text-sky-400'));
  var calPill = document.getElementById('valve-cal-pill');
  calPill.innerText = v.calibrated ? 'CALIBRATED' : 'uncalibrated';
  calPill.className = 'ml-2 text-[10px] px-2 py-0.5 rounded-full border ' +
    (v.calibrated ? 'bg-emerald-950 border-emerald-600 text-emerald-400'
                  : 'bg-amber-950 border-amber-700 text-amber-400');

  document.getElementById('valve-cal-times').innerText =
    'Open ' + v.openTimeMs + 'ms / Close ' + v.closeTimeMs + 'ms';

  var fault = document.getElementById('valve-fault-row');
  if (v.state === 'FAULT') {
    fault.classList.remove('hidden');
    document.getElementById('valve-fault-msg').innerText = v.faultReason || 'unknown';
  } else { fault.classList.add('hidden'); }

  // populate the cal inputs if user hasn't edited them
  var oi = document.getElementById('valve-open-input');
  var ci = document.getElementById('valve-close-input');
  if (oi && document.activeElement !== oi && !oi.dataset.dirty) oi.value = v.openTimeMs;
  if (ci && document.activeElement !== ci && !ci.dataset.dirty) ci.value = v.closeTimeMs;

  var vs = document.getElementById('valve-slider');
  if (vs && document.activeElement !== vs) vs.value = Math.round(v.position);
  var va = document.getElementById('valve-auto');
  if (va) va.checked = !!v.autoFollow;
}

function saveValveCal() {
  var openMs  = parseInt(document.getElementById('valve-open-input').value, 10);
  var closeMs = parseInt(document.getElementById('valve-close-input').value, 10);
  if (!openMs || !closeMs || openMs < 500 || closeMs < 500) { alert('Times must be >= 500ms'); return; }
  fetch('/api/valve', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({openMs:openMs, closeMs:closeMs}) })
    .then(function(){
      document.getElementById('valve-open-input').dataset.dirty  = '';
      document.getElementById('valve-close-input').dataset.dirty = '';
    });
}

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
    tailsPower: parseFloat(document.getElementById('pf-tailsp').value),
    valveHeatup: parseInt(document.getElementById('pf-v-heatup').value, 10),
    valveHeads:  parseInt(document.getElementById('pf-v-heads').value, 10),
    valveHearts: parseInt(document.getElementById('pf-v-hearts').value, 10),
    valveTails:  parseInt(document.getElementById('pf-v-tails').value, 10)
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

// ============ DILUTION CALCULATOR ============
// All math is done in mL internally, then formatted back to chosen units.
var DIL_FACTORS = { ml: 1, L: 1000, oz: 29.5735, qt: 946.353, gal: 3785.41 };
var dilSource = 'meas';   // 'wash' or 'meas'
var lastWashABV = null;   // pulled from batch info

function dilUseSource(which) {
  dilSource = which;
  var w = document.getElementById('dil-src-wash');
  var m = document.getElementById('dil-src-meas');
  w.className = 'px-3 py-1 rounded-xl border ' + (which==='wash' ? 'bg-amber-700 border-amber-500' : 'bg-zinc-800 border-zinc-700');
  m.className = 'px-3 py-1 rounded-xl border ' + (which==='meas' ? 'bg-amber-700 border-amber-500' : 'bg-zinc-800 border-zinc-700');
  if (which === 'wash' && lastWashABV !== null) {
    document.getElementById('dil-src-val').value = lastWashABV;
    document.getElementById('dil-src-unit').value = 'abv';
  }
  dilUpdate();
}

function toAbv(val, unit)  { return unit === 'proof' ? val / 2.0 : val; }
function fromMl(ml, unit)  { return ml / DIL_FACTORS[unit]; }
function fmtVol(ml, unit) {
  var v = fromMl(ml, unit);
  return v.toFixed(unit === 'ml' ? 0 : 2) + ' ' + (unit==='ml'?'mL':unit==='L'?'L':unit==='oz'?'fl oz':unit==='qt'?'qt':'gal');
}

function dilUpdate() {
  var srcVal  = parseFloat(document.getElementById('dil-src-val').value);
  var srcUnit = document.getElementById('dil-src-unit').value;
  var tgtVal  = parseFloat(document.getElementById('dil-tgt-val').value);
  var tgtUnit = document.getElementById('dil-tgt-unit').value;
  var volVal  = parseFloat(document.getElementById('dil-vol-val').value);
  var volUnit = document.getElementById('dil-vol-unit').value;
  var label   = document.getElementById('dil-water-label').value || 'water';

  var srcAbv = toAbv(srcVal, srcUnit);
  var tgtAbv = toAbv(tgtVal, tgtUnit);
  var totalMl = volVal * DIL_FACTORS[volUnit];

  var result = document.getElementById('dil-result');
  var warn   = document.getElementById('dil-warn');
  warn.classList.add('hidden');

  if (!isFinite(srcAbv) || !isFinite(tgtAbv) || !isFinite(totalMl) || totalMl<=0) return;
  if (srcAbv <= 0) { warn.innerText = 'Source ABV must be > 0.'; warn.classList.remove('hidden'); result.classList.remove('hidden'); return; }
  if (tgtAbv > srcAbv) { warn.innerText = 'Target ABV ('+tgtAbv.toFixed(1)+'%) is higher than source ABV ('+srcAbv.toFixed(1)+'%). Cannot dilute UP — distill more, or lower target.'; warn.classList.remove('hidden'); }

  var spiritMl = totalMl * (tgtAbv / srcAbv);
  if (spiritMl > totalMl) spiritMl = totalMl;
  var waterMl  = totalMl - spiritMl;

  document.getElementById('dil-r-spirit').innerText      = fmtVol(spiritMl, volUnit);
  document.getElementById('dil-r-spirit-alt').innerText  = fmtVol(spiritMl, volUnit==='ml'?'oz':'ml') + '  @ ' + srcAbv.toFixed(1) + '% ABV';
  document.getElementById('dil-r-water-lbl').innerText   = (label || 'water').toUpperCase() + ' TO ADD';
  document.getElementById('dil-r-water').innerText       = fmtVol(waterMl, volUnit);
  document.getElementById('dil-r-water-alt').innerText   = fmtVol(waterMl, volUnit==='ml'?'oz':'ml');
  document.getElementById('dil-r-final').innerText       = fmtVol(totalMl, volUnit);
  document.getElementById('dil-r-final-alt').innerText   = '@ ' + tgtAbv.toFixed(1) + '% ABV  (' + (tgtAbv*2).toFixed(0) + ' proof)';

  result.classList.remove('hidden');
}

// ============ HISTORY ============
var historyChart = null;
function loadHistory() {
  fetch('/api/history').then(function(r){return r.json();}).then(function(rows){
    var box = document.getElementById('history-list');
    if (!rows || rows.length === 0) { box.innerHTML = '<div class="text-zinc-500 text-sm">No saved runs yet. Complete a run to see it here.</div>'; return; }
    box.innerHTML = '';
    rows.sort(function(a,b){ return b.id - a.id; });
    rows.forEach(function(row){
      var card = document.createElement('div');
      card.className = 'p-4 rounded-2xl border border-zinc-700 bg-zinc-900 flex justify-between items-center';
      var h = Math.floor(row.duration/3600), m = Math.floor((row.duration%3600)/60);
      card.innerHTML = '<div><div class="font-bold">Run #' + row.id + ' &mdash; ' + row.profile + '</div>' +
        '<div class="text-xs text-zinc-400 mt-1">' + h + 'h ' + m + 'm  &middot;  ' + row.points + ' samples  &middot;  ' + row.bytes + ' B</div></div>' +
        '<div class="flex gap-x-2">' +
        '<button class="px-3 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl text-xs">VIEW</button>' +
        '<button class="px-3 py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl text-xs">CSV</button>' +
        '<button class="px-3 py-2 bg-red-900 hover:bg-red-800 border border-red-700 rounded-xl text-xs">DEL</button>' +
        '</div>';
      var btns = card.querySelectorAll('button');
      btns[0].onclick = function(){ viewRun(row.id); };
      btns[1].onclick = function(){ window.open('/api/history/csv?id=' + row.id, '_blank'); };
      btns[2].onclick = function(){ if (confirm('Delete run #'+row.id+'?')) fetch('/api/history/delete', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({id: row.id})}).then(loadHistory); };
      box.appendChild(card);
    });
  });
}

function viewRun(id) {
  fetch('/api/history/get?id=' + id).then(function(r){return r.json();}).then(function(d){
    document.getElementById('history-viewer-title').innerText = 'Run #' + d.id + ' - ' + d.profile + ' (' + Math.round(d.duration/60) + ' min)';
    var b = d.batch || {};
    document.getElementById('history-batch').innerText =
      'Wash ABV: ' + (b.washABV||'-') + '%\nVolume: ' + (b.volume||'-') + ' ' + (b.unit||'') +
      '\nIngredients: ' + (b.ingredients||'-') + '\nNotes: ' + (b.notes||'-');

    var labels = [], temps = [], pows = [], valves = [];
    (d.readings || []).forEach(function(r){
      // stored as [ts_s, temp_c, power, abv, stage, valve]
      var sec = r[0];
      labels.push(Math.floor(sec/60) + ':' + String(sec%60).padStart(2,'0'));
      temps.push(r[1] * 9/5 + 32);
      pows.push(r[2]);
      valves.push(r[5] || 0);
    });

    var ctx = document.getElementById('historyChart');
    if (historyChart) historyChart.destroy();
    historyChart = new Chart(ctx, {
      type: 'line',
      data: { labels: labels, datasets: [
        { label: 'Temp F', data: temps, borderColor:'#f59e0b', borderWidth:2, tension:0.3, yAxisID:'y', pointRadius:0 },
        { label: 'Power %', data: pows, borderColor:'#64748b', borderWidth:2, tension:0.3, yAxisID:'y1', pointRadius:0 },
        { label: 'Valve %', data: valves, borderColor:'#38bdf8', borderWidth:2, tension:0.3, yAxisID:'y1', pointRadius:0, borderDash:[4,4] }
      ]},
      options: { responsive:true, maintainAspectRatio:false, animation:false,
        scales: {
          y:  { position:'left',  min:60, max:220, grid:{color:'#27272a'}, ticks:{color:'#a1a1aa'} },
          y1: { position:'right', min:0,  max:100, grid:{drawOnChartArea:false}, ticks:{color:'#a1a1aa'} },
          x:  { grid:{color:'#27272a'}, ticks:{color:'#a1a1aa', maxRotation:0, autoSkip:true} }
        },
        plugins:{ legend:{ labels:{color:'#a1a1aa'} } }
      }
    });
    document.getElementById('history-viewer').classList.remove('hidden');
  });
}

function closeHistoryViewer() { document.getElementById('history-viewer').classList.add('hidden'); }

// Hook tab switch to lazy-load history / BLE scan
var origSwitchTab = switchTab;
switchTab = function(tab) {
  origSwitchTab(tab);
  if (tab === 3) loadBleScan();
  if (tab === 5) loadHistory();
};

// ============ BLE PROBE SCAN ============
function rssiBars(rssi) {
  // -40 strong .. -90 weak
  if (rssi >= -55) return 'rssi-4';
  if (rssi >= -68) return 'rssi-3';
  if (rssi >= -80) return 'rssi-2';
  return 'rssi-1';
}

function loadBleScan() {
  fetch('/api/ble/scan').then(function(r){return r.json();}).then(function(data){
    var target = data.target || '';
    document.getElementById('ble-target-display').innerText = target || '(auto - any CQ60 in range)';

    var list = document.getElementById('ble-list');
    var empty = document.getElementById('ble-empty');
    list.innerHTML = '';
    var devs = data.devices || [];
    if (devs.length === 0) { empty.classList.remove('hidden'); return; }
    empty.classList.add('hidden');
    devs.sort(function(a,b){ return b.rssi - a.rssi; });

    devs.forEach(function(d){
      var isActive = (d.addr === target);
      var ageS = Math.round(d.ageMs / 1000);
      var card = document.createElement('div');
      card.className = 'p-4 rounded-2xl border flex justify-between items-center ' +
        (isActive ? 'border-emerald-500 bg-emerald-950/30' : 'border-zinc-700 bg-zinc-900');
      var tempStr = d.hasTemp ? ('<span class="text-amber-400">' + (d.tempC * 9/5 + 32).toFixed(1) + ' F</span>') : '<span class="text-zinc-600">no temp data</span>';
      card.innerHTML =
        '<div class="min-w-0">' +
          '<div class="font-bold truncate">' + (d.name || '(unnamed)') + '</div>' +
          '<div class="font-mono text-xs text-zinc-400 mt-1">' + d.addr + '</div>' +
          '<div class="text-xs mt-1">' + tempStr + ' &middot; <span class="text-zinc-500">RSSI ' + d.rssi + ' dBm &middot; ' + ageS + 's ago</span></div>' +
        '</div>' +
        '<button class="ml-4 px-4 py-2 rounded-xl text-xs font-semibold ' +
          (isActive ? 'bg-emerald-700 text-emerald-100 border border-emerald-500' : 'bg-zinc-800 hover:bg-zinc-700 border border-zinc-600') +
          '">' + (isActive ? 'TRACKING' : 'SELECT') + '</button>';
      var btn = card.querySelector('button');
      btn.onclick = function(){ if (!isActive) selectBleDevice(d.addr); };
      list.appendChild(card);
    });
  });
}

function selectBleDevice(addr) {
  fetch('/api/ble/select', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({addr: addr}) }).then(loadBleScan);
}

function clearBleScan() {
  fetch('/api/ble/clear', {method:'POST'}).then(loadBleScan);
}

window.onload = function() {
  initChart();
  loadProfiles();

  setInterval(function() {
    fetch('/api/status').then(function(r){return r.json();}).then(updateDashboard).catch(function(){});
  }, 650);

  setInterval(function() {
    fetch('/api/valve/status').then(function(r){return r.json();}).then(updateValveStatus).catch(function(){});
  }, 500);

  fetch('/api/batch').then(function(r){return r.json();}).then(function(b) {
    document.getElementById('batch-abv').value = b.washABV || 10;
    document.getElementById('batch-volume').value = b.volume || 10;
    document.getElementById('batch-unit').value = b.unit || 'gal';
    document.getElementById('batch-ingredients').value = b.ingredients || '';
    document.getElementById('batch-notes').value = b.notes || '';
    lastWashABV = b.washABV;
  });
};
</script>
</body>
</html>
)HTML";
