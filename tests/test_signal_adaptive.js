// ============================================================================
// Signal Strength (RSSI) & Multi-Sensor Fusion Adaptive Resolution Tests
// ============================================================================
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

console.log("====================================================================");
console.log("  SIGNAL STRENGTH (RSSI) & DUAL-SENSOR FUSION ADAPTIVE TEST SUITE   ");
console.log("====================================================================\n");

const htmlPath = path.resolve(__dirname, '../web-ui/index.html');
const htmlSource = fs.readFileSync(htmlPath, 'utf8');

// Extract script
const scriptMatch = htmlSource.match(/<script>([\s\S]*?)<\/script>/);
if (!scriptMatch) {
  throw new Error("Could not find <script> tag in web-ui/index.html");
}
const scriptContent = scriptMatch[1];

function createSandbox() {
  const elements = {};
  const dummyCtx = {
    clearRect() {}, beginPath() {}, arc() {}, stroke() {}, moveTo() {}, lineTo() {},
    fillText() {}, createRadialGradient() { return { addColorStop() {} }; },
    save() {}, restore() {}, translate() {}, rotate() {}, closePath() {}, fill() {}, fillRect() {}, setLineDash() {}
  };
  const createElement = (id, tag = 'div') => {
    return {
      id,
      tagName: tag.toUpperCase(),
      textContent: '',
      innerHTML: '',
      value: '',
      style: { setProperty() {} },
      width: 220,
      height: 220,
      disabled: false,
      classList: {
        _classes: new Set(),
        add(...cls) { cls.forEach(c => this._classes.add(c)); },
        remove(...cls) { cls.forEach(c => this._classes.delete(c)); },
        toggle(cls) { if (this._classes.has(cls)) { this._classes.delete(cls); return false; } else { this._classes.add(cls); return true; } },
        contains(cls) { return this._classes.has(cls); }
      },
      addEventListener: () => {},
      removeAttribute: () => {},
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 100, height: 100 }),
      getContext: () => dummyCtx
    };
  };

  const doc = {
    getElementById: (id) => {
      if (!elements[id]) elements[id] = createElement(id);
      return elements[id];
    },
    querySelector: (sel) => {
      const id = sel.replace(/[.#]/, '');
      return doc.getElementById(id);
    },
    querySelectorAll: () => [],
    addEventListener: () => {},
    removeEventListener: () => {},
    documentElement: { style: { setProperty() {} } },
    body: createElement('body')
  };

  const mockWindow = {
    location: { href: 'http://localhost:8000/', protocol: 'http:', host: '192.168.4.1', search: '' },
    addEventListener: () => {},
    DeviceOrientationEvent: undefined
  };

  const sandbox = {
    window: mockWindow,
    document: doc,
    location: mockWindow.location,
    history: { replaceState() {} },
    navigator: { serviceWorker: { register: () => Promise.resolve() } },
    URL: URL,
    URLSearchParams: URLSearchParams,
    ResizeObserver: class { observe() {} unobserve() {} disconnect() {} },
    requestAnimationFrame: () => 1,
    localStorage: {
      _data: {},
      getItem(k) { return this._data[k] !== undefined ? this._data[k] : null; },
      setItem(k, v) { this._data[k] = String(v); },
      removeItem(k) { delete this._data[k]; }
    },
    WebSocket: function() {
      return { addEventListener: () => {}, send: () => {}, close: () => {} };
    },
    Date: Date,
    Math: Math,
    Number: Number,
    String: String,
    Boolean: Boolean,
    parseInt: parseInt,
    parseFloat: parseFloat,
    isNaN: isNaN,
    setTimeout: (fn) => setTimeout(fn, 0),
    clearTimeout: (t) => clearTimeout(t),
    setInterval: () => 1,
    clearInterval: () => {},
    console: console,
    fetch: () => Promise.resolve({ ok: true, json: () => Promise.resolve({ status: 'ok' }) })
  };

  vm.createContext(sandbox);
  const stripped = scriptContent
    .replace(/initWebSocket\(\);/g, '// initWebSocket();')
    .replace(/initPhoneOrientation\(\);/g, '// initPhoneOrientation();');
  vm.runInContext(stripped, sandbox);
  return sandbox;
}

const sb = createSandbox();
const computeTargetTier = sb.computeTargetTier;
const computeTierFromDist = sb.computeTierFromDist;
const computeTierFromRssi = sb.computeTierFromRssi;

let passed = 0;
function test(desc, fn) {
  try {
    fn();
    console.log(`  ✓ ${desc}`);
    passed++;
  } catch (err) {
    console.error(`  ✗ FAIL: ${desc}`);
    console.error(`    ${err.message}`);
    process.exit(1);
  }
}

// ----------------------------------------------------------------------------
// TEST SECTION 1: RSSI-Only Tier Selection and Hysteresis
// ----------------------------------------------------------------------------
console.log("--- Test 1: RSSI-Only Tier State Transitions with Hysteresis ---");

test("Tier 1 nominal (RSSI >= -65 dBm): -50 dBm returns Tier 1", () => {
  assert.strictEqual(computeTierFromRssi(1, -50), 1);
});

test("Tier 1 deadband: -67 dBm remains Tier 1 (within [-62, -68] dBm deadband)", () => {
  assert.strictEqual(computeTierFromRssi(1, -67), 1);
});

test("Tier 1 -> Tier 2 transition: -69 dBm drops to Tier 2 (below -68 dBm)", () => {
  assert.strictEqual(computeTierFromRssi(1, -69), 2);
});

test("Tier 2 deadband downward: -75 dBm remains Tier 2 (within [-70, -76] dBm deadband)", () => {
  assert.strictEqual(computeTierFromRssi(2, -75), 2);
});

test("Tier 2 -> Tier 3 transition: -77 dBm drops to Tier 3 (below -76 dBm)", () => {
  assert.strictEqual(computeTierFromRssi(2, -77), 3);
});

test("Tier 2 -> Tier 1 recovery: -61 dBm upscales to Tier 1 (above -62 dBm)", () => {
  assert.strictEqual(computeTierFromRssi(2, -61), 1);
});

test("Tier 3 deadband downward: -82 dBm remains Tier 3 (within [-77, -83] dBm deadband)", () => {
  assert.strictEqual(computeTierFromRssi(3, -82), 3);
});

test("Tier 3 -> Tier 4 transition: -84 dBm drops to Tier 4 (below -83 dBm)", () => {
  assert.strictEqual(computeTierFromRssi(3, -84), 4);
});

test("Tier 4 recovery upward: -76 dBm upscales to Tier 3 (above -77 dBm)", () => {
  assert.strictEqual(computeTierFromRssi(4, -76), 3);
});

test("Direct cross-tier drop: Tier 1 to -86 dBm jumps straight to Tier 4", () => {
  assert.strictEqual(computeTierFromRssi(1, -86), 4);
});

test("Direct cross-tier recovery: Tier 4 to -55 dBm jumps straight to Tier 1", () => {
  assert.strictEqual(computeTierFromRssi(4, -55), 1);
});

test("Invalid RSSI values (0, positive, NaN, null) return null", () => {
  assert.strictEqual(computeTierFromRssi(1, 0), null);
  assert.strictEqual(computeTierFromRssi(1, 15), null);
  assert.strictEqual(computeTierFromRssi(1, null), null);
  assert.strictEqual(computeTierFromRssi(1, undefined), null);
  assert.strictEqual(computeTierFromRssi(1, NaN), null);
});

// ----------------------------------------------------------------------------
// TEST SECTION 2: Dual-Sensor Fusion (GPS + RSSI)
// ----------------------------------------------------------------------------
console.log("\n--- Test 2: Multi-Sensor Fusion & GPS Error Mitigation ---");

test("GPS Error Defense: GPS says 10m (Tier 1), but RSSI is -85 dBm (Tier 4) -> selects Tier 4", () => {
  // If boat is behind a breakwater or GPS is inaccurate, downscale to protect stream!
  const target = computeTargetTier(1, 10.0, true, -85, 'auto');
  assert.strictEqual(target, 4, "Should select Tier 4 to prevent video freeze over weak Wi-Fi link");
});

test("GPS Degraded Fallback: GPS lost (gpsok=false), but RSSI is -60 dBm -> seamlessly uses RSSI Tier 1", () => {
  const target = computeTargetTier(1, 0, false, -60, 'auto');
  assert.strictEqual(target, 1, "Should seamlessly fall back to RSSI when GPS is unhealthy");
});

test("GPS Lost & Weak Signal: GPS lost (gpsok=false), RSSI is -78 dBm -> falls back to RSSI Tier 3", () => {
  const target = computeTargetTier(1, 0, false, -78, 'auto');
  assert.strictEqual(target, 3, "Should safely adapt to Tier 3 under degraded GPS");
});

test("GPS Pre-fix (9999m): RSSI is -64 dBm -> selects Tier 1 instead of crashing", () => {
  const target = computeTargetTier(1, 9999, true, -64, 'auto');
  assert.strictEqual(target, 1, "Should use RSSI when GPS distance is in pre-fix 9999m state");
});

test("Both Agree: Distance 25m (Tier 2) and RSSI -70 dBm (Tier 2) -> returns Tier 2", () => {
  const target = computeTargetTier(2, 25.0, true, -70, 'auto');
  assert.strictEqual(target, 2);
});

test("Long Range Dominance: Distance 60m (Tier 4) and RSSI -70 dBm (Tier 2) -> returns Tier 4", () => {
  const target = computeTargetTier(1, 60.0, true, -70, 'auto');
  assert.strictEqual(target, 4, "Worst-case defense principle: Max tier chosen");
});

// ----------------------------------------------------------------------------
// TEST SECTION 3: Mode Options (Signal Only, Range Only, Auto Fusion)
// ----------------------------------------------------------------------------
console.log("\n--- Test 3: Camera Scaling Mode Selectability ---");

test("Mode 'signal': Ignores distance and evaluates purely based on RSSI", () => {
  // Distance 5m (Tier 1), but RSSI -78 dBm (Tier 3)
  assert.strictEqual(computeTargetTier(1, 5.0, true, -78, 'signal'), 3);
});

test("Mode 'range': Ignores RSSI and evaluates purely based on distance", () => {
  // Distance 45m (Tier 3), but RSSI -50 dBm (Tier 1)
  assert.strictEqual(computeTargetTier(1, 45.0, true, -50, 'range'), 3);
});

test("Backward Compatibility: computeTargetTier(tier, dist, gpsok) with 3 args works identically", () => {
  assert.strictEqual(computeTargetTier(1, 10.0, true), 1);
  assert.strictEqual(computeTargetTier(1, 25.0, true), 2);
  assert.strictEqual(computeTargetTier(1, 45.0, true), 3);
  assert.strictEqual(computeTargetTier(1, 65.0, true), 4);
});

// ----------------------------------------------------------------------------
// TEST SECTION 4: UI Formatting with Signal Metrics
// ----------------------------------------------------------------------------
console.log("\n--- Test 4: HUD Indicator & RSSI Display ---");

test("HUD badge displays RSSI and AUTO tag when RSSI is available", () => {
  vm.runInContext(`
    camMode = 'auto';
    activeCamFramesize = 6;
    activeCamQuality = 14;
    lastTelemetry.rssi = -68;
    updateCamUI();
  `, sb);

  const badgeText = sb.document.getElementById('badge-cam-res').textContent;
  assert(badgeText.includes('📶 -68dBm'), `Badge should contain signal strength, got: "${badgeText}"`);
  assert(badgeText.includes('CIF'), `Badge should contain resolution name, got: "${badgeText}"`);
  assert(badgeText.includes('(AUTO)'), `Badge should indicate AUTO, got: "${badgeText}"`);

  const rssiVal = sb.document.getElementById('val-rssi').textContent;
  assert.strictEqual(rssiVal, '-68', "val-rssi element should display -68");
});

test("HUD badge displays MAN tag without RSSI in manual mode", () => {
  vm.runInContext(`
    camMode = 'manual';
    activeCamFramesize = 8;
    activeCamQuality = 12;
    lastTelemetry.rssi = -55;
    updateCamUI();
  `, sb);

  const badgeText = sb.document.getElementById('badge-cam-res').textContent;
  assert(badgeText.includes('(MAN)'), `Badge should indicate MAN, got: "${badgeText}"`);
  assert(sb.document.getElementById('badge-cam-res').classList.contains('manual'), "Badge should have manual class");
});

// ----------------------------------------------------------------------------
// TEST SECTION 5: Dynamic Adaptive FPS & Live Transmitter Frame Rate Reporting
// ----------------------------------------------------------------------------
console.log("\n--- Test 5: Dynamic Adaptive FPS & Live Frame Rate Metrics ---");

test("CAM_TIERS defines adaptive target FPS per range/signal tier", () => {
  const tiers = vm.runInContext("CAM_TIERS", sb);
  assert.strictEqual(tiers[1].fps, 25, "Tier 1 (close range) should stream at 25 FPS");
  assert.strictEqual(tiers[2].fps, 20, "Tier 2 should stream at 20 FPS");
  assert.strictEqual(tiers[3].fps, 15, "Tier 3 should stream at 15 FPS");
  assert.strictEqual(tiers[4].fps, 10, "Tier 4 (fringe/long range) should stream at 10 FPS to prevent Wi-Fi congestion");
});

test("applyCamTier updates activeCamFps and constructs request with fps param", () => {
  let requestedUrl = null;
  sb.fetch = (url) => {
    requestedUrl = url;
    return Promise.resolve({
      ok: true,
      json: () => Promise.resolve({ status: 'ok', framesize: 1, quality: 24, fps: 10.2, target_fps: 10 })
    });
  };

  vm.runInContext("applyCamTier(4);", sb);
  assert(requestedUrl.includes('framesize=1'), "URL should request framesize 1");
  assert(requestedUrl.includes('quality=24'), "URL should request quality 24");
  assert(requestedUrl.includes('fps=10'), "URL should request target FPS 10 for Tier 4");
});

test("Live measured transmitter FPS formats into badge and val-cam-fps", () => {
  vm.runInContext(`
    camMode = 'auto';
    activeCamFramesize = 8;
    activeCamQuality = 12;
    activeCamFps = 25;
    liveCamFps = 23.8;
    lastTelemetry.rssi = -64;
    updateCamUI();
  `, sb);

  const badgeText = sb.document.getElementById('badge-cam-res').textContent;
  assert(badgeText.includes('24fps'), `Badge should contain formatted live fps, got: "${badgeText}"`);
  assert(badgeText.includes('📶 -64dBm'), `Badge should contain RSSI, got: "${badgeText}"`);

  const fpsText = sb.document.getElementById('val-cam-fps').textContent;
  assert.strictEqual(fpsText, '23.8', "val-cam-fps should display exact 1-decimal live FPS");
});

test("Manual mode FPS selection persists to localStorage", () => {
  const camSelectFps = sb.document.getElementById('cam-select-fps');
  assert(camSelectFps, "cam-select-fps element must exist");
  assert(sb.document.getElementById('val-cam-fps'), "val-cam-fps element must exist");
});

console.log("\n====================================================================");
console.log(`  ALL ${passed} SIGNAL ADAPTIVE & FUSION TESTS PASSED CLEANLY!       `);
console.log("====================================================================");
