// ============================================================================
// Empirical Challenger 2 Stress Test Suite: Milestone 3 Web-UI Controls & Stream
// ============================================================================

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const vm = require('vm');

console.log("====================================================================");
console.log("  CHALLENGER 2: EMPIRICAL STRESS & RESILIENCE HARNESS (MILESTONE 3)");
console.log("====================================================================");

const htmlPath = path.resolve(__dirname, '../web-ui/index.html');
assert(fs.existsSync(htmlPath), "web-ui/index.html must exist at " + htmlPath);
const html = fs.readFileSync(htmlPath, 'utf8');

const scriptMatch = html.match(/<script>([\s\S]*?)<\/script>/);
assert(scriptMatch, "Script tag must exist in index.html");
const scriptCode = scriptMatch[1];

// Build simulated browser environment
function createHarness(options = {}) {
  const initialStorage = options.storage || {};
  const storageThrowOnSet = options.storageThrowOnSet || false;
  const storage = Object.assign({}, initialStorage);

  const elements = {};
  const activeTimers = new Map();
  let timerIdCounter = 1;
  let virtualTime = 1000000;

  function createElement(id, tagName = 'div') {
    return {
      id,
      tagName: tagName.toUpperCase(),
      textContent: '',
      innerHTML: '',
      value: '',
      title: '',
      get className() { return Array.from(this.classList._classes).join(' '); },
      set className(v) {
        this.classList._classes = new Set(String(v || '').split(/\s+/).filter(Boolean));
      },
      style: {},
      disabled: false,
      classList: {
        _classes: new Set(),
        add(...cls) { cls.forEach(c => this._classes.add(c)); },
        remove(...cls) { cls.forEach(c => this._classes.delete(c)); },
        toggle(cls) {
          if (this._classes.has(cls)) { this._classes.delete(cls); return false; }
          else { this._classes.add(cls); return true; }
        },
        contains(cls) { return this._classes.has(cls); }
      },
      setAttribute(k, v) { this[k] = v; },
      getAttribute(k) { return this[k]; },
      removeAttribute(k) { delete this[k]; },
      _listeners: {},
      addEventListener(evt, fn) {
        if (!this._listeners[evt]) this._listeners[evt] = [];
        this._listeners[evt].push(fn);
      },
      dispatchEvent(evt) {
        if (this._listeners[evt.type]) {
          this._listeners[evt.type].forEach(fn => fn(evt));
        }
      },
      getBoundingClientRect() { return { left: 0, top: 0, width: 100, height: 100 }; },
      focus() {},
      blur() {},
      contains() { return false; },
      getContext: () => ({
        clearRect() {}, beginPath() {}, arc() {}, stroke() {},
        moveTo() {}, lineTo() {}, fillText() {}, createRadialGradient() { return { addColorStop() {} }; },
        save() {}, restore() {}, translate() {}, rotate() {}, closePath() {}, fill() {}, fillRect() {}, setLineDash() {}
      })
    };
  }

  const doc = {
    getElementById(id) {
      if (!elements[id]) elements[id] = createElement(id);
      return elements[id];
    },
    querySelector(sel) {
      const id = sel.replace(/[.#]/, '');
      return this.getElementById(id);
    },
    documentElement: { style: { setProperty() {} } },
    body: createElement('body'),
    addEventListener() {}
  };

  const win = {
    location: { href: 'http://localhost:8000/', protocol: 'http:', search: '' },
    addEventListener() {},
    DeviceOrientationEvent: undefined
  };

  const ls = {
    getItem(k) { return storage[k] !== undefined ? storage[k] : null; },
    setItem(k, v) {
      if (storageThrowOnSet) {
        const err = new Error("QuotaExceededError: DOMException 22");
        err.name = "QuotaExceededError";
        throw err;
      }
      storage[k] = String(v);
    },
    removeItem(k) { delete storage[k]; },
    clear() { Object.keys(storage).forEach(k => delete storage[k]); }
  };

  const dispatches = [];
  const defaultFetch = (url, opts) => {
    dispatches.push({ url, time: virtualTime });
    const matchFs = url.match(/framesize=(\d+)/);
    const matchQ = url.match(/quality=(\d+)/);
    const fsVal = matchFs ? Number(matchFs[1]) : 8;
    const qVal = matchQ ? Number(matchQ[1]) : 12;
    return Promise.resolve({
      ok: true,
      json: () => Promise.resolve({ status: "ok", framesize: fsVal, quality: qVal })
    });
  };

  const sandbox = {
    window: win,
    document: doc,
    localStorage: ls,
    location: win.location,
    history: { replaceState() {} },
    navigator: { serviceWorker: { register: () => Promise.resolve() } },
    URL: URL,
    URLSearchParams: URLSearchParams,
    ResizeObserver: class { observe() {} unobserve() {} disconnect() {} },
    requestAnimationFrame: () => 1,
    setTimeout: (fn, ms) => {
      const id = timerIdCounter++;
      activeTimers.set(id, { fn, ms, runAt: virtualTime + Math.max(0, ms) });
      return id;
    },
    clearTimeout: (id) => {
      activeTimers.delete(id);
    },
    setInterval: () => 0,
    clearInterval: () => {},
    Date: {
      now: () => virtualTime
    },
    Math: Math,
    Number: Number,
    String: String,
    Boolean: Boolean,
    parseInt: parseInt,
    parseFloat: parseFloat,
    isNaN: isNaN,
    console: {
      log() {},
      warn() {},
      error() {}
    },
    fetch: options.fetch || defaultFetch
  };

  vm.createContext(sandbox);
  vm.runInContext(scriptCode, sandbox);

  return {
    sandbox,
    doc,
    elements,
    storage,
    dispatches,
    activeTimers,
    getVar(name) {
      return vm.runInContext(name, sandbox);
    },
    setVar(name, val) {
      sandbox.__tmp_val = val;
      vm.runInContext(`${name} = __tmp_val; delete __tmp_val;`, sandbox);
    },
    eval(code) {
      return vm.runInContext(code, sandbox);
    },
    triggerTimer(id) {
      if (activeTimers.has(id)) {
        const item = activeTimers.get(id);
        activeTimers.delete(id);
        virtualTime = item.runAt;
        item.fn();
      }
    },
    triggerNextTimer() {
      if (activeTimers.size === 0) return false;
      const sorted = Array.from(activeTimers.entries()).sort((a, b) => a[1].runAt - b[1].runAt);
      const [id, item] = sorted[0];
      activeTimers.delete(id);
      virtualTime = item.runAt;
      item.fn();
      return true;
    },
    advanceTime(ms) {
      const targetTime = virtualTime + ms;
      while (true) {
        const sorted = Array.from(activeTimers.entries()).sort((a, b) => a[1].runAt - b[1].runAt);
        if (sorted.length === 0 || sorted[0][1].runAt > targetTime) {
          break;
        }
        const [id, item] = sorted[0];
        activeTimers.delete(id);
        virtualTime = item.runAt;
        item.fn();
      }
      virtualTime = targetTime;
    },
    drainMicrotasks() {
      return new Promise(resolve => setImmediate(resolve));
    }
  };
}

async function runAllChallengerTests() {
  let passed = 0;
  let failed = 0;

  function report(name, ok, details = "") {
    if (ok) {
      passed++;
      console.log(`  ✓ ${name}`);
    } else {
      failed++;
      console.error(`  ✗ FAIL: ${name} — ${details}`);
    }
  }

  // ============================================================================
  // TASK 1A: UI Controls & Auto/Manual Mode Toggling Stress
  // ============================================================================
  console.log("\n--- Task 1A: Manual vs Auto Mode Toggling & UI Control Enablement ---");
  {
    const h = createHarness();
    const modeSelect = h.doc.getElementById('cam-scaling-mode');
    const resSelect = h.doc.getElementById('cam-select-res');
    const qSlider = h.doc.getElementById('cam-select-quality');
    const badge = h.doc.getElementById('badge-cam-res');

    // 1. Initial state (default auto)
    report("Initial mode is 'auto'", h.getVar("camMode") === 'auto');
    report("Initial res dropdown is disabled", resSelect.disabled === true);
    report("Initial quality slider is disabled", qSlider.disabled === true);
    report("Badge reflects (AUTO)", badge.textContent.includes('(AUTO)'));
    report("Badge does NOT have .manual class", !badge.classList.contains('manual'));

    // 2. Switch to manual mode
    modeSelect.value = 'manual';
    modeSelect.dispatchEvent({ type: 'change' });
    report("Switch to manual updates camMode", h.getVar("camMode") === 'manual');
    report("Manual mode enables res dropdown", resSelect.disabled === false);
    report("Manual mode enables quality slider", qSlider.disabled === false);
    report("Manual mode adds .manual class to badge", badge.classList.contains('manual'));
    report("Manual mode sets badge text to (MAN)", badge.textContent.includes('(MAN)'));

    // 3. Switch back to auto mode
    modeSelect.value = 'auto';
    modeSelect.dispatchEvent({ type: 'change' });
    report("Switch back to auto updates camMode", h.getVar("camMode") === 'auto');
    report("Auto mode disables res dropdown", resSelect.disabled === true);
    report("Auto mode disables quality slider", qSlider.disabled === true);
    report("Auto mode removes .manual class", !badge.classList.contains('manual'));

    // 4. Rapid toggling stress: 100 alternating switches
    for (let i = 0; i < 100; i++) {
      modeSelect.value = (i % 2 === 0) ? 'manual' : 'auto';
      modeSelect.dispatchEvent({ type: 'change' });
    }
    // After 100 iterations (i=99 is 'auto')
    report("Post-100 toggles mode is 'auto'", h.getVar("camMode") === 'auto');
    report("Post-100 toggles resSelect disabled", resSelect.disabled === true);
    report("Post-100 toggles qSlider disabled", qSlider.disabled === true);

    // Toggle once more to manual
    modeSelect.value = 'manual';
    modeSelect.dispatchEvent({ type: 'change' });
    report("Final toggle to manual enables controls", resSelect.disabled === false && qSlider.disabled === false);

    // 5. Verify telemetry updates are IGNORED in manual mode
    h.eval("activeCamFramesize = 5; activeCamQuality = 18; updateCamUI();");
    h.eval("handleAdaptiveCamera({ hd: 10.0, gpsok: true });"); // distance 10m would force Tier 1 (VGA) in AUTO
    report("Telemetry does not override manual framesize", h.getVar("activeCamFramesize") === 5);
    report("Telemetry does not override manual quality", h.getVar("activeCamQuality") === 18);
    report("Badge retains manual framesize (QVGA)", badge.textContent.includes('QVGA'));
  }

  // ============================================================================
  // TASK 1B: LocalStorage Persistence Under Diverse Conditions
  // ============================================================================
  console.log("\n--- Task 1B: LocalStorage Persistence Under Diverse Browser Conditions ---");
  {
    // Condition 1: Clean boot (first-time visit)
    const hClean = createHarness({ storage: {} });
    report("Clean boot defaults: camMode='auto'", hClean.getVar("camMode") === 'auto');
    report("Clean boot defaults: activeCamFramesize=8 (VGA)", hClean.getVar("activeCamFramesize") === 8);
    report("Clean boot defaults: activeCamQuality=12", hClean.getVar("activeCamQuality") === 12);
    report("Clean boot defaults: camHost='192.168.4.2'", hClean.getVar("camHost") === '192.168.4.2');

    // Condition 2: Pre-saved valid settings hydration
    const hSaved = createHarness({
      storage: {
        'boat-cam-mode': 'manual',
        'boat-cam-res': '6',
        'boat-cam-quality': '14',
        'boat-cam-host': '192.168.4.77'
      }
    });
    report("Hydrated camMode is 'manual'", hSaved.getVar("camMode") === 'manual');
    report("Hydrated activeCamFramesize is 6 (CIF)", hSaved.getVar("activeCamFramesize") === 6);
    report("Hydrated activeCamQuality is 14", hSaved.getVar("activeCamQuality") === 14);
    report("Hydrated camHost is '192.168.4.77'", hSaved.getVar("camHost") === '192.168.4.77');
    report("Hydrated controls enabled in manual mode",
      hSaved.doc.getElementById('cam-select-res').disabled === false &&
      hSaved.doc.getElementById('cam-select-quality').disabled === false);
    report("Hydrated badge shows 'CIF · Q14 (MAN)'",
      hSaved.doc.getElementById('badge-cam-res').textContent === 'CIF · Q14 (MAN)');

    // Condition 3: Interactive mutation persistence
    const hMut = createHarness();
    const modeSel = hMut.doc.getElementById('cam-scaling-mode');
    const resSel = hMut.doc.getElementById('cam-select-res');
    const qSld = hMut.doc.getElementById('cam-select-quality');
    const hostIn = hMut.doc.getElementById('cam-host');

    modeSel.value = 'manual';
    modeSel.dispatchEvent({ type: 'change' });
    report("localStorage persisted 'boat-cam-mode'='manual'", hMut.storage['boat-cam-mode'] === 'manual');

    resSel.value = '1';
    resSel.dispatchEvent({ type: 'change' });
    report("localStorage persisted 'boat-cam-res'='1'", hMut.storage['boat-cam-res'] === '1');

    qSld.value = '24';
    qSld.dispatchEvent({ type: 'input' });
    report("localStorage persisted 'boat-cam-quality'='24'", hMut.storage['boat-cam-quality'] === '24');

    hostIn.value = '10.0.0.50';
    hostIn.dispatchEvent({ type: 'change' });
    report("localStorage persisted 'boat-cam-host'='10.0.0.50'", hMut.storage['boat-cam-host'] === '10.0.0.50');

    // Condition 4: Malformed / Corrupted entries
    const hCorrupt = createHarness({
      storage: {
        'boat-cam-mode': 'nonsense_mode',
        'boat-cam-res': 'not_a_number',
        'boat-cam-quality': 'garbage_q',
        'boat-cam-host': '  http://192.168.4.99/path/ '
      }
    });
    // Should safely fallback without throwing uncaught exceptions
    report("Corrupted mode falls back safely without crash", hCorrupt.getVar("camMode") === 'nonsense_mode');
    report("Corrupted mode leaves manual controls disabled", hCorrupt.doc.getElementById('cam-select-res').disabled === true);
    report("Sanitizer cleans malformed camHost URL", hCorrupt.eval("getCamBaseHost()") === '192.168.4.99');
  }

  // ============================================================================
  // TASK 1C: fpvStream.onerror Auto-Reconnect Timer Resilience & Loop Safety
  // ============================================================================
  console.log("\n--- Task 1C: fpvStream.onerror Auto-Reconnect Timer Resilience ---");
  {
    const h = createHarness();
    const fpv = h.doc.getElementById('fpv-stream');
    const btn = h.doc.getElementById('btn-cam');

    // Subtest 1: Burst error flood (1000 synchronous onerror triggers)
    report("onerror handler is attached", typeof fpv.onerror === 'function');
    for (let i = 0; i < 1000; i++) {
      fpv.onerror();
    }
    // Crucial check: Exactly 1 timer should be scheduled, preventing timer explosion
    report("Burst of 1000 onerror calls produces exactly 1 pending timer", h.activeTimers.size === 1);
    report("Button displays 'CAM: RETRYING'", btn.textContent === 'CAM: RETRYING');
    report("Button has 'off' class during retry", btn.classList.contains('off'));

    // Subtest 2: 50 continuous failed reconnect cycles (infinite loop safety)
    let loopHealthy = true;
    for (let cycle = 1; cycle <= 50; cycle++) {
      if (h.activeTimers.size !== 1) {
        loopHealthy = false;
        break;
      }
      // Reconnect timer fires (800ms)
      h.triggerNextTimer();
      // Verify fpv.src updated with new cache buster
      if (!fpv.src || !fpv.src.includes('/stream?t=')) {
        loopHealthy = false;
        break;
      }
      // Network fails again immediately, triggering onerror
      fpv.onerror();
    }
    report("50 continuous reconnect cycles maintain single-timer invariant", loopHealthy);
    report("Final state after 50 retries still exactly 1 timer", h.activeTimers.size === 1);
    report("Button remains 'CAM: RETRYING'", btn.textContent === 'CAM: RETRYING');

    // Subtest 3: Successful stream recovery (onload)
    fpv.onload();
    report("onload cancels pending reconnect timer", h.activeTimers.size === 0);
    report("onload sets button to 'CAM: LIVE'", btn.textContent === 'CAM: LIVE');
    report("onload removes 'off' class from button", !btn.classList.contains('off'));

    // Subtest 4: Operator manual shutdown while camera is RETRYING
    fpv.onerror();
    report("onerror re-schedules retry timer", h.activeTimers.size === 1);
    report("btnCam in RETRYING state", btn.textContent === 'CAM: RETRYING');

    // Click during RETRYING triggers immediate retry request
    btn.dispatchEvent({ type: 'click' });
    report("Click during RETRYING sets 'CAM: WAIT'", btn.textContent === 'CAM: WAIT');

    // Click again while WAIT/LIVE shuts down stream
    btn.textContent = 'CAM: LIVE'; // simulate stream now active
    btn.dispatchEvent({ type: 'click' });
    report("Click while active sets 'CAM: OFF'", btn.textContent === 'CAM: OFF');
    report("Camera visibility set to false", h.getVar("camVisible") === false);
    report("Image src removed on camera off", fpv.src === undefined || fpv.src === '');
    report("No reconnect timer remains when camera is off", h.activeTimers.size === 0);

    // Any subsequent onerror events while camera is OFF must be NO-OPs
    for (let i = 0; i < 20; i++) {
      fpv.onerror();
    }
    report("Subsequent onerror events while OFF schedule ZERO timers", h.activeTimers.size === 0);

    // Subtest 5: Restart stream from OFF
    btn.dispatchEvent({ type: 'click' });
    report("Tap while OFF restarts camera (CAM: WAIT)", btn.textContent === 'CAM: WAIT');
    report("Camera visibility restored to true", h.getVar("camVisible") === true);
    report("Image src restored with new stream URL", Boolean(fpv.src && fpv.src.includes('/stream?t=')));
  }

  // ============================================================================
  // SUMMARY
  // ============================================================================
  console.log("\n====================================================================");
  console.log(`CHALLENGER 2 SUITE SUMMARY: ${passed} PASSED, ${failed} FAILED`);
  console.log("====================================================================");

  if (failed > 0) {
    process.exit(1);
  }
}

runAllChallengerTests().catch(err => {
  console.error("FATAL SUITE ERROR:", err);
  process.exit(1);
});
