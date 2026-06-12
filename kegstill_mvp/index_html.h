#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KEG STILL - Glitch Edition</title>
<script src="https://cdn.tailwindcss.com"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body { font-family: system-ui, sans-serif; }
.big-num { font-size: 3.2rem; line-height: 1; font-weight: 700; }
.section { background-color: #18181b; border: 1px solid #3f3f46; }
.metric  { background-color: #27272a; }
.tab-btn { padding: 0.5rem 1rem; border-radius: 0.75rem; font-weight: 600; font-size: 0.85rem; }
.tab-btn.active { background:#f59e0b; color:#000; }
.tab-btn:not(.active) { background:#27272a; color:#a1a1aa; }
input[type=number] { appearance:textfield; -moz-appearance:textfield; }
input[type=number]::-webkit-inner-spin-button { -webkit-appearance:none; }
.stage-dot { width:8px; height:8px; border-radius:99px; background:#3f3f46; display:inline-block; margin-right:6px; }
.stage-dot.active { background:#f59e0b; box-shadow:0 0 0 3px rgba(245,158,11,.2); }
.stage-dot.done   { background:#10b981; }
</style>
</head>
<body class="bg-zinc-950 text-zinc-200">
<div class="max-w-7xl mx-auto p-6">

  <!-- header -->
  <div class="flex items-center justify-between mb-6">
    <div>
      <h1 class="text-5xl font-bold tracking-tighter">KEG STILL</h1>
      <p class="text-zinc-500 text-sm">Glitch Edition - K-type + Auto Cuts + Profiles</p>
    </div>
    <div class="flex items-center gap-x-3">
      <div id="probe-pill"  class="px-3 py-1.5 rounded-full text-xs font-semibold bg-zinc-900 border border-zinc-700" data-testid="probe-pill">NO PROBE</div>
      <div id="status-pill" class="px-4 py-1.5 rounded-full text-sm font-semibold bg-zinc-900 border border-zinc-700" data-testid="status-pill">IDLE</div>
      <button onclick="doEstop()" data-testid="estop-btn" class="bg-red-600 hover:bg-red-700 px-8 py-3 rounded-2xl font-bold text-lg">E-STOP</button>
    </div>
  </div>

  <!-- tabs -->
  <div class="flex gap-2 mb-5">
    <button class="tab-btn active" data-tab="run"     onclick="setTab('run')"     data-testid="tab-run">RUN</button>
    <button class="tab-btn"        data-tab="profile" onclick="setTab('profile')" data-testid="tab-profile">PROFILE</button>
    <button class="tab-btn"        data-tab="history" onclick="setTab('history')" data-testid="tab-history">HISTORY</button>
    <button class="tab-btn"        data-tab="dilute"  onclick="setTab('dilute')"  data-testid="tab-dilute">DILUTION</button>
    <button class="tab-btn"        data-tab="valve"   onclick="setTab('valve')"   data-testid="tab-valve">VALVE</button>
  </div>

  <!-- RUN TAB -->
  <div id="tab-run" class="tab-content">
    <div class="grid grid-cols-1 lg:grid-cols-12 gap-6">
      <div class="lg:col-span-5 section rounded-3xl p-8">

        <!-- stage strip -->
        <div class="mb-5 p-3 rounded-2xl bg-zinc-900 border border-zinc-800 flex items-center justify-between">
          <div class="flex items-center gap-x-3">
            <div id="stage-warmup" class="text-xs"><span class="stage-dot"></span>WARMUP</div>
            <div id="stage-heads"  class="text-xs"><span class="stage-dot"></span>HEADS</div>
            <div id="stage-hearts" class="text-xs"><span class="stage-dot"></span>HEARTS</div>
            <div id="stage-tails"  class="text-xs"><span class="stage-dot"></span>TAILS</div>
            <div id="stage-done"   class="text-xs"><span class="stage-dot"></span>DONE</div>
          </div>
          <div id="mode-pill" class="text-[10px] px-2 py-1 rounded-full bg-zinc-800 border border-zinc-700">MANUAL</div>
        </div>

        <div class="grid grid-cols-2 gap-6">
          <div class="metric rounded-2xl p-6 border border-zinc-700">
            <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">VAPOR TEMP</div>
            <div class="flex items-baseline gap-x-2">
              <span id="tempF" data-testid="temp-f" class="big-num text-zinc-600">--</span>
              <span class="text-3xl text-zinc-400">&deg;F</span>
            </div>
            <div id="tempC" class="text-sm text-zinc-500 mt-1">-- &deg;C</div>
          </div>
          <div class="metric rounded-2xl p-6 border border-zinc-700">
            <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">POWER</div>
            <div class="flex items-baseline gap-x-1">
              <span id="power" data-testid="power-pct" class="big-num text-white">0</span>
              <span class="text-2xl text-zinc-400">%</span>
            </div>
            <input type="range" id="powerSlider" data-testid="power-slider" min="0" max="100" step="1" value="0" class="w-full accent-amber-500 mt-4" oninput="setPower(this.value)">
          </div>
          <div class="metric rounded-2xl p-6 border border-zinc-700">
            <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">ELAPSED</div>
            <div id="elapsed" class="big-num text-white tabular-nums">00:00</div>
          </div>
          <div class="metric rounded-2xl p-6 border border-zinc-700">
            <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">PROBE FAULT</div>
            <div id="probeFault" class="text-xl font-bold text-emerald-400 mt-2">OK</div>
          </div>
        </div>

        <div class="mt-6 grid grid-cols-2 gap-4">
          <button onclick="confirmStart(false)" data-testid="start-manual-btn"
                  class="bg-emerald-600 hover:bg-emerald-500 text-white font-bold py-5 rounded-3xl text-xl">DISTILL (MANUAL)</button>
          <button onclick="confirmStart(true)" data-testid="start-auto-btn"
                  class="bg-amber-600 hover:bg-amber-500 text-black font-bold py-5 rounded-3xl text-xl">DISTILL (AUTO)</button>
        </div>
        <div class="mt-3">
          <button onclick="doStop()" data-testid="stop-btn"
                  class="w-full bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 py-3 rounded-2xl font-semibold">STOP RUN</button>
        </div>

        <!-- valve mini -->
        <div class="mt-5 metric rounded-2xl p-5 border border-zinc-700">
          <div class="flex justify-between items-baseline mb-2">
            <div class="text-xs uppercase tracking-widest text-zinc-500">BALL VALVE
              <span id="vStatePill" class="ml-2 text-[10px] px-2 py-0.5 rounded-full bg-zinc-800 border border-zinc-700">IDLE</span>
              <span id="vCalPill"   class="ml-1 text-[10px] px-2 py-0.5 rounded-full bg-amber-950 border border-amber-700 text-amber-400">uncal</span>
            </div>
            <div><span id="valvePos" data-testid="valve-pos" class="text-3xl font-bold text-sky-400">0</span><span class="text-zinc-400 ml-1">%</span></div>
          </div>
          <input type="range" id="valveSlider" data-testid="valve-slider" min="0" max="100" step="1" value="0" class="w-full accent-sky-500" oninput="setValve(this.value)">
          <div class="mt-3 grid grid-cols-4 gap-2 text-xs">
            <button onclick="vCmd('close')"  class="py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl">CLOSE</button>
            <button onclick="vCmd('stop')"   class="py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl">STOP</button>
            <button onclick="vCmd('open')"   class="py-2 bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 rounded-xl">OPEN</button>
            <button onclick="vCmd('rehome')" class="py-2 bg-amber-700 hover:bg-amber-600 border border-amber-500 rounded-xl">RE-HOME</button>
          </div>
        </div>
      </div>

      <div class="lg:col-span-7 section rounded-3xl p-6">
        <div class="flex justify-between mb-2 px-2">
          <div class="font-semibold">LIVE DATA</div>
          <div class="text-xs text-zinc-500">Temp &deg;F / Power % / Valve %</div>
        </div>
        <div style="position:relative; height:480px; width:100%; overflow:hidden;">
          <canvas id="runChart"></canvas>
        </div>
      </div>
    </div>
  </div>

  <!-- PROFILE TAB -->
  <div id="tab-profile" class="tab-content hidden">
    <div class="section rounded-3xl p-8 max-w-3xl">
      <div class="flex items-center justify-between mb-5">
        <div class="text-xl font-bold">DISTILLATION PROFILE</div>
        <button onclick="saveProfile()" data-testid="save-profile-btn"
                class="bg-emerald-600 hover:bg-emerald-500 px-5 py-2 rounded-xl font-bold">SAVE TO NVS</button>
      </div>

      <label class="block text-xs uppercase tracking-widest text-zinc-500 mb-1">Profile name</label>
      <input id="p_name" data-testid="profile-name" class="w-full bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2 mb-5" value="default-spirit">

      <div class="grid grid-cols-2 gap-x-6 gap-y-4">

        <div class="metric rounded-2xl p-4 border border-zinc-700 col-span-2">
          <div class="text-xs uppercase tracking-widest text-amber-400 mb-2">WARMUP</div>
          <div class="grid grid-cols-2 gap-3">
            <label class="text-xs text-zinc-500">Target Vapor (&deg;F)
              <input id="p_warmupTempF" type="number" step="0.5" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <div class="text-xs text-zinc-500 flex items-end pb-2 italic">Full power, valve closed. Advances to HEADS when vapor &ge; heads temp.</div>
          </div>
        </div>

        <div class="metric rounded-2xl p-4 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-amber-400 mb-2">HEADS</div>
          <div class="grid grid-cols-2 gap-3">
            <label class="text-xs text-zinc-500">Trigger (&deg;F)
              <input id="p_headsTempF" type="number" step="0.5" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Power (%)
              <input id="p_headsPower" type="number" min="0" max="100" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Valve (%)
              <input id="p_headsValve" type="number" min="0" max="100" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Max (min)
              <input id="p_headsMin" type="number" min="0" max="240" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
          </div>
        </div>

        <div class="metric rounded-2xl p-4 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-emerald-400 mb-2">HEARTS</div>
          <div class="grid grid-cols-2 gap-3">
            <label class="text-xs text-zinc-500">Trigger (&deg;F)
              <input id="p_heartsTempF" type="number" step="0.5" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Power (%)
              <input id="p_heartsPower" type="number" min="0" max="100" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Valve (%)
              <input id="p_heartsValve" type="number" min="0" max="100" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Max (min)
              <input id="p_heartsMin" type="number" min="0" max="240" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
          </div>
        </div>

        <div class="metric rounded-2xl p-4 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-sky-400 mb-2">TAILS</div>
          <div class="grid grid-cols-2 gap-3">
            <label class="text-xs text-zinc-500">Trigger (&deg;F)
              <input id="p_tailsTempF" type="number" step="0.5" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Power (%)
              <input id="p_tailsPower" type="number" min="0" max="100" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Valve (%)
              <input id="p_tailsValve" type="number" min="0" max="100" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <label class="text-xs text-zinc-500">Max (min)
              <input id="p_tailsMin" type="number" min="0" max="240" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
          </div>
        </div>

        <div class="metric rounded-2xl p-4 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-red-400 mb-2">SHUTOFF</div>
          <div class="grid grid-cols-2 gap-3">
            <label class="text-xs text-zinc-500">Hard cutoff (&deg;F)
              <input id="p_shutoffTempF" type="number" step="0.5" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
            <div class="text-xs text-zinc-500 flex items-end pb-2 italic">If vapor exceeds this at any time, automation goes DONE and SSR off.</div>
          </div>
        </div>

      </div>

      <div class="mt-4 text-xs text-zinc-500">
        Profile is saved to ESP32 NVS (Preferences) and persists across reboots.
      </div>
    </div>
  </div>

  <!-- HISTORY TAB -->
  <div id="tab-history" class="tab-content hidden">
    <div class="section rounded-3xl p-6">
      <div class="flex items-center justify-between mb-4">
        <div class="text-xl font-bold">RUN HISTORY <span id="histCount" class="text-zinc-500 text-sm ml-2">(0 samples)</span></div>
        <div class="flex gap-2">
          <a href="/api/history.csv" data-testid="history-csv-btn"
             class="bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 px-4 py-2 rounded-xl text-sm font-semibold">DOWNLOAD CSV</a>
          <button onclick="reloadHistory()" data-testid="history-reload-btn"
                  class="bg-zinc-800 hover:bg-zinc-700 border border-zinc-600 px-4 py-2 rounded-xl text-sm font-semibold">REFRESH</button>
          <button onclick="clearHistory()" data-testid="history-clear-btn"
                  class="bg-red-700 hover:bg-red-600 border border-red-500 px-4 py-2 rounded-xl text-sm font-semibold">CLEAR</button>
        </div>
      </div>
      <div style="position:relative; height:380px;"><canvas id="histChart"></canvas></div>
      <div class="text-xs text-zinc-500 mt-3">Stored in RAM, sampled every 10s during a run. Up to 2h history.</div>
    </div>
  </div>

  <!-- DILUTION TAB -->
  <div id="tab-dilute" class="tab-content hidden">
    <div class="section rounded-3xl p-8 max-w-2xl">
      <div class="text-xl font-bold mb-5">DILUTION CALCULATOR</div>
      <div class="grid grid-cols-2 gap-5">
        <label class="text-xs text-zinc-500">Starting ABV (%)
          <input id="d_abvIn" data-testid="dilute-abv-in" type="number" step="0.1" value="65" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
        <label class="text-xs text-zinc-500">Target ABV (%)
          <input id="d_abvOut" data-testid="dilute-abv-out" type="number" step="0.1" value="40" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
        <label class="text-xs text-zinc-500">Volume of spirit
          <input id="d_volIn" data-testid="dilute-vol-in" type="number" step="0.01" value="1.0" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
        <label class="text-xs text-zinc-500">Units
          <select id="d_unit" data-testid="dilute-unit" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2">
            <option value="L">Liters</option>
            <option value="gal">US Gallons</option>
            <option value="floz">US Fluid Ounces</option>
            <option value="mL">Milliliters</option>
          </select></label>
      </div>
      <button onclick="calcDilute()" data-testid="dilute-calc-btn"
              class="mt-5 w-full bg-amber-600 hover:bg-amber-500 text-black font-bold py-3 rounded-2xl">CALCULATE</button>

      <div id="d_out" class="mt-6 grid grid-cols-2 gap-4 hidden">
        <div class="metric rounded-2xl p-5 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">WATER TO ADD</div>
          <div class="big-num text-amber-400" id="d_water">--</div>
          <div class="text-sm text-zinc-400" id="d_waterUnit"></div>
        </div>
        <div class="metric rounded-2xl p-5 border border-zinc-700">
          <div class="text-xs uppercase tracking-widest text-zinc-500 mb-1">FINAL VOLUME</div>
          <div class="big-num text-emerald-400" id="d_final">--</div>
          <div class="text-sm text-zinc-400" id="d_finalUnit"></div>
        </div>
      </div>
      <div class="text-xs text-zinc-500 mt-3 italic">
        Note: ignores contraction-of-volume effect when ethanol mixes with water (a ~3% effect near 50% ABV). For exact proofing use a hydrometer.
      </div>
    </div>
  </div>

  <!-- VALVE TAB -->
  <div id="tab-valve" class="tab-content hidden">
    <div class="section rounded-3xl p-8 max-w-xl">
      <div class="text-xl font-bold mb-5">VALVE CALIBRATION</div>
      <div class="grid grid-cols-2 gap-4">
        <label class="text-xs text-zinc-500">Open time (ms)
          <input id="openMs" data-testid="valve-open-ms" type="number" value="3500" min="500" max="120000" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
        <label class="text-xs text-zinc-500">Close time (ms)
          <input id="closeMs" data-testid="valve-close-ms" type="number" value="3500" min="500" max="120000" class="w-full mt-1 bg-zinc-950 border border-zinc-700 rounded-xl px-3 py-2"></label>
      </div>
      <button onclick="saveCal()" data-testid="valve-save-cal-btn"
              class="mt-5 w-full bg-emerald-600 hover:bg-emerald-500 font-bold py-3 rounded-2xl">SAVE CALIBRATION TO NVS</button>
      <div class="text-xs text-zinc-500 mt-3 italic">
        Time the valve mechanically takes to travel fully closed&rarr;open and open&rarr;closed. Used for proportional positioning.
      </div>
    </div>
  </div>

</div>

<script>
var chart=null,histChart=null,tT=[],tP=[],tV=[],tL=[];

function setTab(name){
  document.querySelectorAll('.tab-content').forEach(function(e){e.classList.add('hidden');});
  document.getElementById('tab-'+name).classList.remove('hidden');
  document.querySelectorAll('.tab-btn').forEach(function(e){
    e.classList.toggle('active', e.dataset.tab===name);
  });
  if(name==='history') reloadHistory();
  if(name==='profile') loadProfile();
}

function initChart(){
  chart = new Chart(document.getElementById('runChart'),{
    type:'line',
    data:{labels:tL,datasets:[
      {label:'Temp F',data:tT,borderColor:'#f59e0b',borderWidth:2,tension:0.3,yAxisID:'y',pointRadius:0},
      {label:'Power %',data:tP,borderColor:'#64748b',borderWidth:2,tension:0.3,yAxisID:'y1',pointRadius:0},
      {label:'Valve %',data:tV,borderColor:'#38bdf8',borderWidth:2,tension:0.3,yAxisID:'y1',pointRadius:0,borderDash:[4,4]}
    ]},
    options:{responsive:true,maintainAspectRatio:false,animation:false,
      scales:{
        y:{position:'left',min:60,max:220,grid:{color:'#27272a'},ticks:{color:'#a1a1aa'}},
        y1:{position:'right',min:0,max:100,grid:{drawOnChartArea:false},ticks:{color:'#a1a1aa'}},
        x:{grid:{color:'#27272a'},ticks:{color:'#a1a1aa',maxRotation:0,autoSkip:true}}
      },
      plugins:{legend:{labels:{color:'#a1a1aa'}}}
    }
  });
}

function fmtTime(s){var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sec=s%60;
  return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(sec).padStart(2,'0');}

function updStageStrip(stage,isAuto){
  var order=['WARMUP','HEADS','HEARTS','TAILS','DONE'];
  var ids=['stage-warmup','stage-heads','stage-hearts','stage-tails','stage-done'];
  var idx=order.indexOf(stage);
  ids.forEach(function(id,i){
    var dot=document.getElementById(id).firstElementChild;
    dot.className='stage-dot';
    if(isAuto){
      if(i<idx) dot.className='stage-dot done';
      if(i===idx) dot.className='stage-dot active';
    }
  });
  var mp=document.getElementById('mode-pill');
  if(isAuto){mp.innerText='AUTO - '+stage;mp.className='text-[10px] px-2 py-1 rounded-full bg-amber-950 border border-amber-600 text-amber-400';}
  else{mp.innerText='MANUAL';mp.className='text-[10px] px-2 py-1 rounded-full bg-zinc-800 border border-zinc-700';}
}

function refresh(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    var t=document.getElementById('tempF'),c=document.getElementById('tempC');
    if(d.tempF!=null){t.innerText=d.tempF;c.innerText=d.tempC+' C';t.className='big-num text-white';}
    else{t.innerText='--';c.innerText='-- C';t.className='big-num text-zinc-600';}
    document.getElementById('power').innerText=Math.round(d.power);
    document.getElementById('elapsed').innerText=fmtTime(d.elapsed||0);
    document.getElementById('valvePos').innerText=d.valvePos;
    var sp=document.getElementById('status-pill');
    sp.innerText=d.status;
    sp.className='px-4 py-1.5 rounded-full text-sm font-semibold border '+
      (d.estop?'bg-red-950 border-red-600 text-red-400':
       (d.status==='RUN'?'bg-emerald-950 border-emerald-600 text-emerald-400':'bg-zinc-900 border-zinc-700'));
    var pp=document.getElementById('probe-pill');
    if(d.bleOk){pp.innerText='PROBE OK';pp.className='px-3 py-1.5 rounded-full text-xs font-semibold bg-emerald-950 border border-emerald-600 text-emerald-400';}
    else{pp.innerText='NO PROBE';pp.className='px-3 py-1.5 rounded-full text-xs font-semibold bg-red-950 border border-red-600 text-red-400';}
    var ps=document.getElementById('powerSlider');
    if(document.activeElement!==ps) ps.value=d.power;
    ps.disabled=!!d.auto;
    var vs=document.getElementById('valveSlider');
    if(document.activeElement!==vs) vs.value=d.valvePos;
    updStageStrip(d.stage,d.auto);
    if(d.tempF!=null && chart){
      var lbl=fmtTime(d.elapsed||0);
      tL.push(lbl);tT.push(d.tempF);tP.push(d.power);tV.push(d.valvePos);
      if(tL.length>720){tL.shift();tT.shift();tP.shift();tV.shift();}
      chart.update('none');
    }
  }).catch(function(){});
}

function refreshProbe(){
  fetch('/api/probe').then(function(r){return r.json();}).then(function(p){
    var el=document.getElementById('probeFault');
    if(p.fault===0){el.innerText='OK';el.className='text-xl font-bold text-emerald-400 mt-2';}
    else{el.innerText=p.faultStr||'FAULT';el.className='text-xl font-bold text-red-400 mt-2';}
  }).catch(function(){});
}

function refreshValve(){
  fetch('/api/valve/status').then(function(r){return r.json();}).then(function(v){
    var sp=document.getElementById('vStatePill');
    sp.innerText=v.state;
    sp.className='ml-2 text-[10px] px-2 py-0.5 rounded-full border '+
      (v.state==='IDLE'?'bg-zinc-800 border-zinc-700':'bg-sky-950 border-sky-600 text-sky-400');
    var cp=document.getElementById('vCalPill');
    cp.innerText=v.calibrated?'CAL':'uncal';
    cp.className='ml-1 text-[10px] px-2 py-0.5 rounded-full border '+
      (v.calibrated?'bg-emerald-950 border-emerald-600 text-emerald-400':'bg-amber-950 border-amber-700 text-amber-400');
    var oi=document.getElementById('openMs'),ci=document.getElementById('closeMs');
    if(document.activeElement!==oi) oi.value=v.openTimeMs;
    if(document.activeElement!==ci) ci.value=v.closeTimeMs;
  }).catch(function(){});
}

function setPower(v){
  document.getElementById('power').innerText=Math.round(v);
  fetch('/api/power',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({power:parseFloat(v)})});
}
function setValve(v){fetch('/api/valve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pos:parseInt(v,10)})});}
function vCmd(c){fetch('/api/valve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:c})});}
function saveCal(){
  var o=parseInt(document.getElementById('openMs').value,10);
  var c=parseInt(document.getElementById('closeMs').value,10);
  fetch('/api/valve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({openMs:o,closeMs:c})})
    .then(function(){alert('Calibration saved.');});
}

function confirmStart(auto){
  var msg = auto
    ? 'Start AUTOMATED run?\n\nProfile will drive power and valve.\nStages: WARMUP -> HEADS -> HEARTS -> TAILS -> DONE\n\nConfirm pre-flight: probe in column, water on, SSR powered, valve homed.'
    : 'Start MANUAL run?\n\nYou control power and valve. Hard E-STOP if temp > 105C or probe dies.\n\nConfirm pre-flight: probe in column, water on, SSR powered.';
  if(!confirm(msg)) return;
  tL=[];tT=[];tP=[];tV=[];
  if(chart){chart.data.labels=tL;chart.data.datasets[0].data=tT;chart.data.datasets[1].data=tP;chart.data.datasets[2].data=tV;chart.update();}
  var url = auto ? '/api/auto/start' : '/api/start';
  fetch(url,{method:'POST'}).then(function(r){if(!r.ok)r.text().then(function(t){alert('Cannot start: '+t);});});
}
function doStop(){fetch('/api/auto/stop',{method:'POST'}).then(function(){fetch('/api/stop',{method:'POST'});});}
function doEstop(){if(confirm('E-STOP the still?'))fetch('/api/estop',{method:'POST'});}

// profile
function loadProfile(){
  fetch('/api/profile').then(function(r){return r.json();}).then(function(p){
    document.getElementById('p_name').value=p.name||'';
    ['warmupTempF','headsTempF','headsPower','headsValve','headsMin',
     'heartsTempF','heartsPower','heartsValve','heartsMin',
     'tailsTempF','tailsPower','tailsValve','tailsMin','shutoffTempF'].forEach(function(k){
      var el=document.getElementById('p_'+k); if(el && p[k]!=null) el.value=p[k];
    });
  });
}
function saveProfile(){
  var body={name:document.getElementById('p_name').value};
  ['warmupTempF','headsTempF','headsPower','headsValve','headsMin',
   'heartsTempF','heartsPower','heartsValve','heartsMin',
   'tailsTempF','tailsPower','tailsValve','tailsMin','shutoffTempF'].forEach(function(k){
    var v=parseFloat(document.getElementById('p_'+k).value);
    if(!isNaN(v)) body[k]=v;
  });
  fetch('/api/profile',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){alert(r.ok?'Profile saved to NVS':'Save failed');});
}

// history
function reloadHistory(){
  fetch('/api/history').then(function(r){return r.json();}).then(function(d){
    document.getElementById('histCount').innerText='('+d.count+' samples)';
    var lbls=[],T=[],P=[],V=[];
    d.samples.forEach(function(s){
      var t=s[0]; var h=Math.floor(t/3600),m=Math.floor((t%3600)/60);
      lbls.push(String(h).padStart(2,'0')+':'+String(m).padStart(2,'0'));
      T.push(s[1]); P.push(s[2]); V.push(s[3]);
    });
    if(!histChart){
      histChart=new Chart(document.getElementById('histChart'),{
        type:'line',
        data:{labels:lbls,datasets:[
          {label:'Temp F',data:T,borderColor:'#f59e0b',borderWidth:2,tension:0.3,yAxisID:'y',pointRadius:0},
          {label:'Power %',data:P,borderColor:'#64748b',borderWidth:2,tension:0.3,yAxisID:'y1',pointRadius:0},
          {label:'Valve %',data:V,borderColor:'#38bdf8',borderWidth:2,tension:0.3,yAxisID:'y1',pointRadius:0,borderDash:[4,4]}
        ]},
        options:{responsive:true,maintainAspectRatio:false,animation:false,
          scales:{
            y:{position:'left',min:60,max:220,grid:{color:'#27272a'},ticks:{color:'#a1a1aa'}},
            y1:{position:'right',min:0,max:100,grid:{drawOnChartArea:false},ticks:{color:'#a1a1aa'}},
            x:{grid:{color:'#27272a'},ticks:{color:'#a1a1aa',maxRotation:0,autoSkip:true}}
          },
          plugins:{legend:{labels:{color:'#a1a1aa'}}}
        }
      });
    } else {
      histChart.data.labels=lbls;
      histChart.data.datasets[0].data=T;
      histChart.data.datasets[1].data=P;
      histChart.data.datasets[2].data=V;
      histChart.update();
    }
  });
}
function clearHistory(){
  if(!confirm('Clear all history samples?')) return;
  fetch('/api/history',{method:'DELETE'}).then(reloadHistory);
}

// dilution (Pearson's square: vWater = vIn * (abvIn/abvOut - 1))
function calcDilute(){
  var ai=parseFloat(document.getElementById('d_abvIn').value);
  var ao=parseFloat(document.getElementById('d_abvOut').value);
  var v =parseFloat(document.getElementById('d_volIn').value);
  var u =document.getElementById('d_unit').value;
  if(!isFinite(ai)||!isFinite(ao)||!isFinite(v)||ao<=0||ai<=0||v<=0){alert('Bad input');return;}
  if(ao>=ai){alert('Target ABV must be lower than starting ABV');return;}
  var water = v*(ai/ao - 1);
  var fin   = v+water;
  document.getElementById('d_out').classList.remove('hidden');
  document.getElementById('d_water').innerText = water.toFixed(3);
  document.getElementById('d_final').innerText = fin.toFixed(3);
  document.getElementById('d_waterUnit').innerText = u;
  document.getElementById('d_finalUnit').innerText = u;
}

window.onload=function(){
  initChart();
  loadProfile();
  setInterval(refresh,300);
  setInterval(refreshProbe,1000);
  setInterval(refreshValve,500);
};
</script>
</body>
</html>
)HTML";
