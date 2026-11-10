// ============================================================================
// Milestone 3 Challenger Stress Test Suite
// Adversarial Empirical Verification: Hysteresis Deadband, Hold-Off, Noise
// ============================================================================

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const vm = require('vm');

console.log("============================================================");
console.log("  Milestone 3 Empirical Stress & Adversarial Test Suite");
console.log("============================================================");

// Load index.html
const htmlPath = path.resolve(__dirname, '../web-ui/index.html');
assert(fs.existsSync(htmlPath), "web-ui/index.html must exist");
const html = fs.readFileSync(htmlPath, 'utf8');

const scriptMatch = html.match(/<script>([\s\S]*?)<\/script>/);
assert(scriptMatch, "Script tag must exist in index.html");
const scriptCode = scriptMatch[1];

// Virtual Environment Builder
function createTestEnvironment(customFetch = null) {
  let currentTime = 1000000;
  const timers = [];
  let nextTimerId = 1;

  const mockStorage = {};
  const mockElements = {};

  function createMockElement(id, tagName = 'div') {
    const dummyCtx = {
      clearRect() {}, beginPath() {}, arc() {}, stroke() {},
      moveTo() {}, lineTo() {}, fillText() {},
      createRadialGradient() { return { addColorStop() {} }; },
      save() {}, restore() {}, translate() {}, rotate() {},
      closePath() {}, fill() {}, fillRect() {}, setLineDash() {}
    };
    return {
      id,
      tagName: tagName.toUpperCase(),
      textContent: '',
      innerHTML: '',
      value: '',
      style: {},
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
      addEventListener(evt, fn) {
        if (!this._listeners) this._listeners = {};
        if (!this._listeners[evt]) this._listeners[evt] = [];
        this._listeners[evt].push(fn);
      },
      dispatchEvent(evt) {
        if (this._listeners && this._listeners[evt.type]) {
          this._listeners[evt.type].forEach(fn => fn(evt));
        }
      },
      getBoundingClientRect() { return { left: 0, top: 0, width: 100, height: 100 }; },
      getContext: () => dummyCtx,
      focus() {}, blur() {}, contains() { return false; },
      disabled: false
    };
  }

  const mockDoc = {
    getElementById(id) {
      if (!mockElements[id]) mockElements[id] = createMockElement(id);
      return mockElements[id];
    },
    querySelector(sel) {
      const id = sel.replace(/[.#]/, '');
      return this.getElementById(id);
    },
    documentElement: { style: { setProperty() {} } },
    body: createMockElement('body'),
    addEventListener() {}
  };

  const mockWindow = {
    location: { href: 'http://localhost:8000/', protocol: 'http:', search: '' },
    addEventListener() {},
    DeviceOrientationEvent: undefined
  };

  const mockLocalStorage = {
    getItem(k) { return mockStorage[k] !== undefined ? mockStorage[k] : null; },
    setItem(k, v) { mockStorage[k] = String(v); },
    removeItem(k) { delete mockStorage[k]; },
    clear() { Object.keys(mockStorage).forEach(k => delete mockStorage[k]); }
  };

  function virtualSetTimeout(cb, delayMs) {
    const id = nextTimerId++;
    timers.push({ id, runAt: currentTime + Math.max(0, delayMs), cb });
    return id;
  }

  function virtualClearTimeout(id) {
    const idx = timers.findIndex(t => t.id === id);
    if (idx !== -1) timers.splice(idx, 1);
  }

  async function advanceTime(ms) {
    const target = currentTime + ms;
    while (true) {
      // Find earliest timer scheduled before or at target
      timers.sort((a, b) => a.runAt - b.runAt);
      if (timers.length === 0 || timers[0].runAt > target) {
        break;
      }
      const next = timers.shift();
      currentTime = next.runAt;
      next.cb();
      await Promise.resolve();
      await new Promise(r => setImmediate(r));
    }
    currentTime = target;
    await Promise.resolve();
    await new Promise(r => setImmediate(r));
  }

  const controlCalls = [];

  const defaultFetch = (url, opts) => {
    controlCalls.push({ url, time: currentTime });
    const matchFs = url.match(/framesize=(\d+)/);
    const matchQ = url.match(/quality=(\d+)/);
    const fsVal = matchFs ? Number(matchFs[1]) : 8;
    const qVal = matchQ ? Number(matchQ[1]) : 12;
    return Promise.resolve({
      ok: true,
      json: () => Promise.resolve({ status: "ok", framesize: fsVal, quality: qVal })
    });
  };

  const realDate = Date;
  const MockDate = function(arg) {
    if (arg !== undefined) return new realDate(arg);
    return new realDate(currentTime);
  };
  MockDate.now = () => currentTime;

  const sandbox = {
    window: mockWindow,
    document: mockDoc,
    localStorage: mockLocalStorage,
    location: mockWindow.location,
    history: { replaceState() {} },
    navigator: { serviceWorker: { register: () => Promise.resolve() } },
    URL: URL,
    URLSearchParams: URLSearchParams,
    ResizeObserver: class { observe() {} unobserve() {} disconnect() {} },
    requestAnimationFrame: () => 1,
    setTimeout: virtualSetTimeout,
    clearTimeout: virtualClearTimeout,
    setInterval: () => 0,
    clearInterval: () => {},
    Date: MockDate,
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
    fetch: customFetch || defaultFetch
  };

  vm.createContext(sandbox);
  vm.runInContext(scriptCode, sandbox);

  return {
    sandbox,
    mockDoc,
    mockLocalStorage,
    controlCalls,
    advanceTime,
    getTime: () => currentTime,
    setTime: (t) => { currentTime = t; },
    eval: (expr) => vm.runInContext(expr, sandbox),
    run: (code) => vm.runInContext(code, sandbox)
  };
}

// PRNG for fully reproducible pseudorandom noise
function makePrng(seed = 123456789) {
  let s = seed;
  return function() {
    s = (s * 1664525 + 1013904223) % 4294967296;
    return s / 4294967296;
  };
}

async function runAllTests() {
  // ============================================================================
  // TEST 1: Continuous 0m -> 70m -> 0m Trajectory with High-Frequency Noise (±1.5m)
  // ============================================================================
  console.log("\n[TEST 1] Continuous Trajectory (0m -> 70m -> 0m) with ±1.5m Noise");
  {
    const env = createTestEnvironment();
    const prng = makePrng(42);

    // Initialize at 0m, Tier 1
    env.run("activeCamTier = 1; lastCamRequestTime = Date.now() - 5000;");

    const transitionLog = [];
    let prevTier = 1;

    // Track tier changes
    function logIfChanged(reason, currentDist) {
      const curTier = env.eval("activeCamTier");
      if (curTier !== prevTier) {
        transitionLog.push({
          time: env.getTime(),
          from: prevTier,
          to: curTier,
          reason,
          dist: currentDist
        });
        prevTier = curTier;
      }
    }

    // Speed: 1.5 m/s, 5Hz telemetry = 0.2s interval -> 0.3m per step
    const dtSec = 0.2;
    const speed = 1.5;
    const totalOutboundSec = 70.0 / speed; // ~46.6 seconds
    const totalSteps = Math.ceil(totalOutboundSec / dtSec);

    console.log(`  Simulating OUTBOUND: 0m -> 70m (${totalSteps} steps, dt=0.2s, speed=1.5m/s)...`);
    for (let step = 0; step <= totalSteps; step++) {
      const trueDist = Math.min(70.0, step * speed * dtSec);
      const noise = (prng() * 3.0) - 1.5; // ±1.5m
      const noisyDist = Math.max(0, trueDist + noise);

      env.eval(`handleAdaptiveCamera({ hd: ${noisyDist}, gpsok: true });`);
      await env.advanceTime(200); // 200ms
      logIfChanged(`Outbound step ${step} (true=${trueDist.toFixed(1)}m)`, noisyDist);
    }

    // Ensure any pending timers have fired
    await env.advanceTime(3000);
    logIfChanged("Outbound end settling", 70.0);

    const outboundTransitions = [...transitionLog];
    console.log(`  Outbound transitions recorded: ${outboundTransitions.length}`);
    outboundTransitions.forEach((t, i) => {
      console.log(`    [#${i+1}] t=${t.time}ms: Tier ${t.from} -> ${t.to} at dist=${t.dist.toFixed(2)}m (${t.reason})`);
    });

    // Verify outbound: Must be EXACTLY 3 transitions: 1->2, 2->3, 3->4
    assert.strictEqual(outboundTransitions.length, 3,
      `Outbound trajectory must have EXACTLY 3 transitions without flapping (got ${outboundTransitions.length})`);
    assert.strictEqual(outboundTransitions[0].from, 1);
    assert.strictEqual(outboundTransitions[0].to, 2);
    assert.strictEqual(outboundTransitions[1].from, 2);
    assert.strictEqual(outboundTransitions[1].to, 3);
    assert.strictEqual(outboundTransitions[2].from, 3);
    assert.strictEqual(outboundTransitions[2].to, 4);
    assert.strictEqual(env.eval("activeCamTier"), 4, "Final outbound tier must be 4");

    // Inbound voyage: 70m -> 0m
    console.log(`  Simulating INBOUND: 70m -> 0m (${totalSteps} steps, dt=0.2s, speed=1.5m/s)...`);
    const inboundStartIdx = transitionLog.length;

    for (let step = 0; step <= totalSteps; step++) {
      const trueDist = Math.max(0.0, 70.0 - (step * speed * dtSec));
      const noise = (prng() * 3.0) - 1.5; // ±1.5m
      const noisyDist = Math.max(0, trueDist + noise);

      env.eval(`handleAdaptiveCamera({ hd: ${noisyDist}, gpsok: true });`);
      await env.advanceTime(200);
      logIfChanged(`Inbound step ${step} (true=${trueDist.toFixed(1)}m)`, noisyDist);
    }

    await env.advanceTime(3000);
    logIfChanged("Inbound end settling", 0.0);

    const inboundTransitions = transitionLog.slice(inboundStartIdx);
    console.log(`  Inbound transitions recorded: ${inboundTransitions.length}`);
    inboundTransitions.forEach((t, i) => {
      console.log(`    [#${i+1}] t=${t.time}ms: Tier ${t.from} -> ${t.to} at dist=${t.dist.toFixed(2)}m (${t.reason})`);
    });

    // Verify inbound: Must be EXACTLY 3 transitions: 4->3, 3->2, 2->1
    assert.strictEqual(inboundTransitions.length, 3,
      `Inbound trajectory must have EXACTLY 3 transitions without flapping (got ${inboundTransitions.length})`);
    assert.strictEqual(inboundTransitions[0].from, 4);
    assert.strictEqual(inboundTransitions[0].to, 3);
    assert.strictEqual(inboundTransitions[1].from, 3);
    assert.strictEqual(inboundTransitions[1].to, 2);
    assert.strictEqual(inboundTransitions[2].from, 2);
    assert.strictEqual(inboundTransitions[2].to, 1);
    assert.strictEqual(env.eval("activeCamTier"), 1, "Final inbound tier must be 1");

    // Total round-trip transitions
    assert.strictEqual(transitionLog.length, 6,
      `Total round-trip transitions must be exactly 6 (got ${transitionLog.length})`);

    // Verify Hold-Off spacing (>= 2500ms between consecutive switches)
    for (let i = 1; i < transitionLog.length; i++) {
      const delta = transitionLog[i].time - transitionLog[i-1].time;
      assert(delta >= 2500,
        `Hold-off violation: switch #${i} occurred only ${delta}ms after switch #${i-1} (< 2500ms)`);
    }
    console.log("  ✓ All 6 transitions strictly respected 2500ms hold-off window.");
    console.log("  ✓ Hysteresis deadband (±2.5m) and hold-off successfully prevented ALL flapping!");
  }

  // ============================================================================
  // TEST 2: Sustained Boundary Hovering Under Continuous Noise
  // ============================================================================
  console.log("\n[TEST 2] Sustained Boundary Hovering Under Continuous Noise");
  {
    const env = createTestEnvironment();
    const prng = makePrng(999);

    // Test hovering at nominal boundary distances for 50 seconds (250 packets each)
    const testBoundaries = [
      { nominal: 18.0, desc: "Tier 1/2 boundary (18m nominal)" },
      { nominal: 20.0, desc: "Tier 1 upward threshold neighborhood (20.5m)" },
      { nominal: 38.0, desc: "Tier 2/3 boundary (38m nominal)" },
      { nominal: 40.0, desc: "Tier 2 upward threshold neighborhood (40.5m)" },
      { nominal: 55.0, desc: "Tier 3/4 boundary (55m nominal)" },
      { nominal: 57.0, desc: "Tier 3 upward threshold neighborhood (57.5m)" }
    ];

    for (const b of testBoundaries) {
      let switchCount = 0;
      let lastTier = env.eval("activeCamTier");

      for (let p = 0; p < 250; p++) {
        const noise = (prng() * 3.0) - 1.5; // ±1.5m
        const dist = b.nominal + noise;
        env.eval(`handleAdaptiveCamera({ hd: ${dist}, gpsok: true });`);
        await env.advanceTime(200);

        const curTier = env.eval("activeCamTier");
        if (curTier !== lastTier) {
          switchCount++;
          lastTier = curTier;
        }
      }
      await env.advanceTime(3000);
      const curTier = env.eval("activeCamTier");
      if (curTier !== lastTier) {
        switchCount++;
        lastTier = curTier;
      }

      console.log(`  Hovering at ${b.nominal}m (noise [${(b.nominal-1.5).toFixed(1)}m .. ${(b.nominal+1.5).toFixed(1)}m]) for 50s -> ${switchCount} transitions.`);
      // A hover around a boundary should switch AT MOST once (if crossing into the next tier), NEVER flap back and forth
      assert(switchCount <= 1,
        `Boundary flapping detected at ${b.nominal}m: ${switchCount} switches during 50s hover!`);
    }
    console.log("  ✓ Sustained hovering produced zero flapping across all boundary zones.");
  }

  // ============================================================================
  // TEST 3: Extreme Rapid Distance Flapping Stress (Adversarial Telemetry Glitches)
  // ============================================================================
  console.log("\n[TEST 3] Extreme Rapid Distance Flapping Stress (Glitch Injections)");
  {
    const env = createTestEnvironment();
    env.run("activeCamTier = 1; lastCamRequestTime = Date.now() - 10000;");

    // Alternate violently between 5m and 65m every 200ms for 10 seconds (50 alternations)
    const callsBefore = env.controlCalls.length;
    for (let i = 0; i < 50; i++) {
      const dist = (i % 2 === 0) ? 5.0 : 65.0;
      env.eval(`handleAdaptiveCamera({ hd: ${dist}, gpsok: true });`);
      await env.advanceTime(200);
    }
    await env.advanceTime(5000); // allow settle

    const callsDuring = env.controlCalls.length - callsBefore;
    console.log(`  50 extreme 5m <-> 65m alternations generated ${callsDuring} control dispatch requests.`);
    
    // Over a 15-second span (10s flapping + 5s settle), with 2.5s holdoff,
    // maximum possible requests = 15s / 2.5s + 1 = 7 requests.
    assert(callsDuring <= 7,
      `Rate limiter failed: ${callsDuring} requests in 15 seconds (expected <= 7)`);
    assert(callsDuring >= 1, "At least one transition should have occurred");

    // Verify intervals between all calls
    for (let i = 1; i < env.controlCalls.length; i++) {
      const dt = env.controlCalls[i].time - env.controlCalls[i-1].time;
      assert(dt >= 2500, `Rate-limiting violated: calls spaced by only ${dt}ms (< 2500ms)`);
    }
    console.log("  ✓ Rate limiting hold-off strictly clamped request dispatch under extreme flapping.");
  }

  // ============================================================================
  // TEST 4: Telemetry Edge Cases (Negative, NaN, String, Pre-fix 9999m, Loss of GPS)
  // ============================================================================
  console.log("\n[TEST 4] Adversarial Telemetry Edge Cases & Fail-safes");
  {
    const env = createTestEnvironment();

    // Pre-fix 9999m should clamp to Tier 1
    assert.strictEqual(env.eval("computeTargetTier(4, 9999, true)"), 1, "Pre-fix 9999m must return Tier 1");
    assert.strictEqual(env.eval("computeTargetTier(3, 10000, true)"), 1, "Extreme distance >= 9000 must return Tier 1");

    // Negative distance
    assert.strictEqual(env.eval("computeTargetTier(4, -1, true)"), 1, "Negative distance must return Tier 1");
    assert.strictEqual(env.eval("computeTargetTier(2, -99.9, true)"), 1, "Negative distance must return Tier 1");

    // Loss of GPS health
    assert.strictEqual(env.eval("computeTargetTier(4, 60.0, false)"), 1, "gpsok=false must return Tier 1");
    assert.strictEqual(env.eval("computeTargetTier(2, 25.0, false)"), 1, "gpsok=false must return Tier 1");

    // Corrupted / malformed distance values
    assert.strictEqual(env.eval("computeTargetTier(4, NaN, true)"), 1, "NaN must return Tier 1");
    assert.strictEqual(env.eval("computeTargetTier(4, undefined, true)"), 1, "Undefined must return Tier 1");
    assert.strictEqual(env.eval("computeTargetTier(4, null, true)"), 1, "Null must return Tier 1");

    // Mid-flight GPS drop recovery in state machine
    env.run("activeCamTier = 4; lastCamRequestTime = Date.now() - 5000;");
    env.eval("handleAdaptiveCamera({ hd: 62.0, gpsok: false });");
    await env.advanceTime(100);
    assert.strictEqual(env.eval("activeCamTier"), 1, "GPS signal loss must immediately demote to Tier 1");

    console.log("  ✓ All invalid, pre-fix, and GPS-loss telemetry safely default to Tier 1.");
  }

  // ============================================================================
  // TEST 5: Manual Mode Override and Mode Switching
  // ============================================================================
  console.log("\n[TEST 5] Manual Mode Override and Mode Switching");
  {
    const env = createTestEnvironment();
    const camScalingMode = env.mockDoc.getElementById('cam-scaling-mode');
    const camSelectRes = env.mockDoc.getElementById('cam-select-res');
    const camSelectQuality = env.mockDoc.getElementById('cam-select-quality');

    // Switch to Manual Mode
    camScalingMode.value = 'manual';
    camScalingMode.dispatchEvent({ type: 'change' });
    assert.strictEqual(env.eval("camMode"), 'manual');
    assert.strictEqual(camSelectRes.disabled, false);
    assert.strictEqual(camSelectQuality.disabled, false);

    // Set manual resolution to SVGA (9) or VGA (8) or CIF (6)
    camSelectRes.value = '6';
    camSelectRes.dispatchEvent({ type: 'change' });
    camSelectQuality.value = '20';
    camSelectQuality.dispatchEvent({ type: 'input' });

    const callsInManual = env.controlCalls.length;

    // Stream distance telemetry changes while in manual mode
    for (let d = 0; d <= 70; d += 10) {
      env.eval(`handleAdaptiveCamera({ hd: ${d}, gpsok: true });`);
      await env.advanceTime(500);
    }

    // Verify that NO adaptive switches were made while in manual mode
    assert.strictEqual(env.controlCalls.length, callsInManual,
      "No camera requests should be triggered by distance telemetry while in manual mode");

    // Switch back to AUTO mode
    camScalingMode.value = 'auto';
    env.eval("lastTelemetry = { hd: 65.0, gpsok: true };");
    camScalingMode.dispatchEvent({ type: 'change' });
    assert.strictEqual(env.eval("camMode"), 'auto');
    assert.strictEqual(camSelectRes.disabled, true);
    assert.strictEqual(camSelectQuality.disabled, true);

    await env.advanceTime(100);
    assert.strictEqual(env.eval("activeCamTier"), 4,
      "Switching back to AUTO mode must immediately synchronize with current telemetry distance (Tier 4 at 65m)");

    console.log("  ✓ Manual mode strictly overrides adaptive scaling; Auto mode resyncs upon toggle.");
  }

  // ============================================================================
  // TEST 6: Network Fault Resilience (Camera REST API Offline / 500 / Network Error)
  // ============================================================================
  console.log("\n[TEST 6] Network Fault Resilience (REST Endpoint Offline/500)");
  {
    let rejectFetch = false;
    const errorFetch = (url, opts) => {
      if (rejectFetch) {
        return Promise.reject(new Error("Network timeout / Offline"));
      }
      return Promise.resolve({
        ok: true,
        json: () => Promise.resolve({ status: "ok", framesize: 8, quality: 12 })
      });
    };

    const env = createTestEnvironment(errorFetch);
    rejectFetch = true;

    env.run("activeCamTier = 1; lastCamRequestTime = Date.now() - 5000;");

    // Send request that fails
    env.eval("sendCamControl(6, 14, 'auto');");
    await env.advanceTime(100);

    // camRequestInFlight must be reset to false so the state machine does not lock up permanently
    assert.strictEqual(env.eval("camRequestInFlight"), false,
      "camRequestInFlight must reset to false even after network rejection");

    console.log("  ✓ State machine remains fully operational and unlockable after network errors.");
  }

  console.log("\n============================================================");
  console.log("  ALL EMPIRICAL CHALLENGER STRESS TESTS PASSED CLEANLY!");
  console.log("============================================================\n");
}

runAllTests().catch(err => {
  console.error("Test failed with error:", err);
  process.exit(1);
});
