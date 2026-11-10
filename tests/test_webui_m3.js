// ============================================================
// Test Suite: Milestone 3 Web-UI HUD Adaptive & Manual Controls
// ============================================================

const fs = require('fs');
const path = require('path');
const assert = require('assert');

console.log("=== Running Milestone 3 Web-UI Tests ===");

// 1. Read and parse web-ui/index.html
const htmlPath = path.resolve(__dirname, '../web-ui/index.html');
assert(fs.existsSync(htmlPath), "web-ui/index.html must exist");
const html = fs.readFileSync(htmlPath, 'utf8');

// 2. DOM Elements Verification
const requiredIds = [
  'fpv-stream',
  'badge-link',
  'badge-mode',
  'badge-cam-res',
  'btn-cam',
  'btn-settings',
  'settings-panel',
  'controller-host',
  'apply-host',
  'cam-host',
  'cam-scaling-mode',
  'cam-select-res',
  'cam-select-quality',
  'cam-quality-val',
  'compact-summary',
  'val-cam-info',
  'val-hd'
];

for (const id of requiredIds) {
  const hasId = html.includes(`id="${id}"`);
  assert(hasId, `DOM element with id="${id}" must exist in index.html`);
}
console.log("✓ All required DOM element IDs exist in web-ui/index.html");

// 3. Extract and eval script context with mocked browser environment
const scriptMatch = html.match(/<script>([\s\S]*?)<\/script>/);
assert(scriptMatch, "Script tag must exist in index.html");
const scriptCode = scriptMatch[1];

// Mock minimal DOM and Browser environment
const mockStorage = {};
const mockElements = {};

function createMockElement(id, tagName = 'div') {
  const dummyCtx = {
    clearRect() {},
    beginPath() {},
    arc() {},
    stroke() {},
    moveTo() {},
    lineTo() {},
    fillText() {},
    createRadialGradient() { return { addColorStop() {} }; },
    save() {},
    restore() {},
    translate() {},
    rotate() {},
    closePath() {},
    fill() {},
    fillRect() {},
    setLineDash() {}
  };
  return {
    id,
    tagName: tagName.toUpperCase(),
    textContent: '',
    innerHTML: '',
    value: '',
    style: {},
    width: 220,
    height: 220,
    getContext: () => dummyCtx,
    classList: {
      _classes: new Set(),
      add(...cls) { cls.forEach(c => this._classes.add(c)); },
      remove(...cls) { cls.forEach(c => this._classes.delete(c)); },
      toggle(cls) { if (this._classes.has(cls)) { this._classes.delete(cls); return false; } else { this._classes.add(cls); return true; } },
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
    focus() {},
    blur() {},
    contains() { return false; },
    disabled: false
  };
}

const mockDoc = {
  getElementById(id) {
    if (!mockElements[id]) {
      mockElements[id] = createMockElement(id);
    }
    return mockElements[id];
  },
  querySelector(sel) {
    const id = sel.replace(/[.#]/, '');
    return this.getElementById(id);
  },
  documentElement: {
    style: {
      setProperty() {}
    }
  },
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

// Create sandbox
const vm = require('vm');
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
  setTimeout: setTimeout,
  clearTimeout: clearTimeout,
  setInterval: setInterval,
  clearInterval: clearInterval,
  Date: Date,
  Math: Math,
  Number: Number,
  String: String,
  Boolean: Boolean,
  parseInt: parseInt,
  parseFloat: parseFloat,
  isNaN: isNaN,
  console: console,
  fetch: (url, opts) => {
    return Promise.resolve({
      ok: true,
      json: () => Promise.resolve({ status: "ok", framesize: 8, quality: 12 })
    });
  }
};

vm.createContext(sandbox);

// 4. Test pure functions in sandbox
vm.runInContext(scriptCode, sandbox);

// 5. Test CAM_TIERS definition
const CAM_TIERS = vm.runInContext("CAM_TIERS", sandbox);
assert(CAM_TIERS, "CAM_TIERS must be defined");
assert.strictEqual(CAM_TIERS[1].framesize, 8, "Tier 1 must be VGA (8)");
assert.strictEqual(CAM_TIERS[1].quality, 12, "Tier 1 quality must be 12");
assert.strictEqual(CAM_TIERS[2].framesize, 6, "Tier 2 must be CIF (6)");
assert.strictEqual(CAM_TIERS[2].quality, 14, "Tier 2 quality must be 14");
assert.strictEqual(CAM_TIERS[3].framesize, 5, "Tier 3 must be QVGA (5)");
assert.strictEqual(CAM_TIERS[3].quality, 18, "Tier 3 quality must be 18");
assert.strictEqual(CAM_TIERS[4].framesize, 1, "Tier 4 must be QQVGA (1)");
assert.strictEqual(CAM_TIERS[4].quality, 24, "Tier 4 quality must be 24");
console.log("✓ CAM_TIERS table matches specification");

// 6. Test computeTargetTier state machine
const computeTargetTier = vm.runInContext("computeTargetTier", sandbox);
assert(typeof computeTargetTier === 'function', "computeTargetTier must be a function");

// Test Guards
assert.strictEqual(computeTargetTier(3, -5, true), 1, "Negative distance must clamp to Tier 1");
assert.strictEqual(computeTargetTier(3, 9999, true), 1, "Pre-fix distance >= 9000 must clamp to Tier 1");
assert.strictEqual(computeTargetTier(3, 40, false), 1, "GPS not healthy (!gpsok) must clamp to Tier 1");
assert.strictEqual(computeTargetTier(3, NaN, true), 1, "NaN distance must clamp to Tier 1");
assert.strictEqual(computeTargetTier(3, undefined, true), 1, "Undefined distance must clamp to Tier 1");
console.log("✓ Invalid and pre-fix distance guards default to Tier 1");

// Test Upward Transitions with Hysteresis (Threshold + 2.5m)
// Thresholds: T1->T2 at 18.0 + 2.5 = 20.5; T2->T3 at 38.0 + 2.5 = 40.5; T3->T4 at 55.0 + 2.5 = 57.5
assert.strictEqual(computeTargetTier(1, 10.0, true), 1, "Tier 1: 10m stays Tier 1");
assert.strictEqual(computeTargetTier(1, 18.0, true), 1, "Tier 1: 18.0m stays Tier 1 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(1, 20.4, true), 1, "Tier 1: 20.4m stays Tier 1 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(1, 20.6, true), 2, "Tier 1: 20.6m transitions to Tier 2 (> 20.5m)");
assert.strictEqual(computeTargetTier(1, 42.0, true), 3, "Tier 1: jump to 42m transitions directly to Tier 3 (> 40.5m)");
assert.strictEqual(computeTargetTier(1, 60.0, true), 4, "Tier 1: jump to 60m transitions directly to Tier 4 (> 57.5m)");

assert.strictEqual(computeTargetTier(2, 25.0, true), 2, "Tier 2: 25m stays Tier 2");
assert.strictEqual(computeTargetTier(2, 38.0, true), 2, "Tier 2: 38.0m stays Tier 2 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(2, 40.4, true), 2, "Tier 2: 40.4m stays Tier 2 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(2, 40.6, true), 3, "Tier 2: 40.6m transitions to Tier 3 (> 40.5m)");
assert.strictEqual(computeTargetTier(2, 58.0, true), 4, "Tier 2: jump to 58m transitions directly to Tier 4 (> 57.5m)");

assert.strictEqual(computeTargetTier(3, 45.0, true), 3, "Tier 3: 45m stays Tier 3");
assert.strictEqual(computeTargetTier(3, 55.0, true), 3, "Tier 3: 55.0m stays Tier 3 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(3, 57.4, true), 3, "Tier 3: 57.4m stays Tier 3 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(3, 57.6, true), 4, "Tier 3: 57.6m transitions to Tier 4 (> 57.5m)");
console.log("✓ Upward transitions with +2.5m hysteresis deadband verified");

// Test Downward Transitions with Hysteresis (Threshold - 2.5m)
// Thresholds: T4->T3 at 55.0 - 2.5 = 52.5; T3->T2 at 38.0 - 2.5 = 35.5; T2->T1 at 18.0 - 2.5 = 15.5
assert.strictEqual(computeTargetTier(4, 60.0, true), 4, "Tier 4: 60m stays Tier 4");
assert.strictEqual(computeTargetTier(4, 55.0, true), 4, "Tier 4: 55.0m stays Tier 4 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(4, 52.6, true), 4, "Tier 4: 52.6m stays Tier 4 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(4, 52.4, true), 3, "Tier 4: 52.4m transitions down to Tier 3 (< 52.5m)");
assert.strictEqual(computeTargetTier(4, 30.0, true), 2, "Tier 4: jump to 30m transitions directly down to Tier 2 (< 35.5m)");
assert.strictEqual(computeTargetTier(4, 10.0, true), 1, "Tier 4: jump to 10m transitions directly down to Tier 1 (< 15.5m)");

assert.strictEqual(computeTargetTier(3, 40.0, true), 3, "Tier 3: 40m stays Tier 3");
assert.strictEqual(computeTargetTier(3, 38.0, true), 3, "Tier 3: 38.0m stays Tier 3 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(3, 35.6, true), 3, "Tier 3: 35.6m stays Tier 3 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(3, 35.4, true), 2, "Tier 3: 35.4m transitions down to Tier 2 (< 35.5m)");
assert.strictEqual(computeTargetTier(3, 12.0, true), 1, "Tier 3: jump to 12m transitions directly down to Tier 1 (< 15.5m)");

assert.strictEqual(computeTargetTier(2, 20.0, true), 2, "Tier 2: 20m stays Tier 2");
assert.strictEqual(computeTargetTier(2, 18.0, true), 2, "Tier 2: 18.0m stays Tier 2 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(2, 15.6, true), 2, "Tier 2: 15.6m stays Tier 2 (within hysteresis deadband)");
assert.strictEqual(computeTargetTier(2, 15.4, true), 1, "Tier 2: 15.4m transitions down to Tier 1 (< 15.5m)");
console.log("✓ Downward transitions with -2.5m hysteresis deadband verified");

// 7. Test Rate Limiting Hold-Off (2500ms)
const CAM_RATE_LIMIT_MS = vm.runInContext("CAM_RATE_LIMIT_MS", sandbox);
assert.strictEqual(CAM_RATE_LIMIT_MS, 2500, "CAM_RATE_LIMIT_MS must be 2500");
vm.runInContext("activeCamTier = 1; lastCamRequestTime = Date.now(); handleAdaptiveCamera({ hd: 30.0, gpsok: true });", sandbox);
assert.strictEqual(vm.runInContext("activeCamTier", sandbox), 1, "Rate limit hold-off must defer transition");
assert.strictEqual(vm.runInContext("pendingCamTier", sandbox), 2, "Pending tier must be queued during hold-off");
console.log("✓ Rate-limiting 2500ms hold-off timer correctly buffers pending transition");

// 8. Test HUD Elements Text Formatting and Update
vm.runInContext("activeCamFramesize = 8; activeCamQuality = 12; camMode = 'auto'; updateCamUI();", sandbox);

const badgeElem = mockDoc.getElementById('badge-cam-res');
const valCamElem = mockDoc.getElementById('val-cam-info');
assert.strictEqual(badgeElem.textContent, 'VGA · Q12 (AUTO)', "Badge text must format as 'VGA · Q12 (AUTO)'");
assert.strictEqual(valCamElem.textContent, 'VGA · Q12 (AUTO)', "Telemetry row must format as 'VGA · Q12 (AUTO)'");

vm.runInContext("camMode = 'manual'; activeCamFramesize = 6; activeCamQuality = 14; updateCamUI();", sandbox);
assert.strictEqual(badgeElem.textContent, 'CIF · Q14 (MAN)', "Badge text must format as 'CIF · Q14 (MAN)' in manual mode");
assert.strictEqual(valCamElem.textContent, 'CIF · Q14 (MAN)', "Telemetry row must format as 'CIF · Q14 (MAN)' in manual mode");
console.log("✓ HUD indicators format correctly for both AUTO and MAN modes");

// 9. Test Settings Persistence
const camScalingModeSelect = vm.runInContext("camScalingModeSelect", sandbox);
const camSelectRes = vm.runInContext("camSelectRes", sandbox);
const camSelectQuality = vm.runInContext("camSelectQuality", sandbox);
const camHostInput = vm.runInContext("camHostInput", sandbox);

camScalingModeSelect.value = 'manual';
camScalingModeSelect.dispatchEvent({ type: 'change' });
assert.strictEqual(mockLocalStorage.getItem('boat-cam-mode'), 'manual', "boat-cam-mode must be stored in localStorage");
assert.strictEqual(camSelectRes.disabled, false, "Resolution select must be enabled in manual mode");
assert.strictEqual(camSelectQuality.disabled, false, "Quality range must be enabled in manual mode");

camSelectRes.value = '5';
camSelectRes.dispatchEvent({ type: 'change' });
assert.strictEqual(mockLocalStorage.getItem('boat-cam-res'), '5', "boat-cam-res must be stored in localStorage");

camSelectQuality.value = '18';
camSelectQuality.dispatchEvent({ type: 'input' });
assert.strictEqual(mockLocalStorage.getItem('boat-cam-quality'), '18', "boat-cam-quality must be stored in localStorage");

camHostInput.value = '192.168.4.55';
camHostInput.dispatchEvent({ type: 'change' });
assert.strictEqual(mockLocalStorage.getItem('boat-cam-host'), '192.168.4.55', "boat-cam-host must be stored in localStorage");
console.log("✓ Settings persistence to localStorage verified");

// 10. Test Stream Reconnect Handler on Error
const fpvStream = vm.runInContext("fpvStream", sandbox);
assert(fpvStream.onerror, "fpvStream.onerror must be bound");
vm.runInContext("camVisible = true;", sandbox);
const btnCamElem = mockDoc.getElementById('btn-cam');
fpvStream.onerror();
assert.strictEqual(btnCamElem.textContent, 'CAM: RETRYING', "btnCam text must indicate RETRYING on error");
assert(vm.runInContext("camReconnectTimer !== null", sandbox), "camReconnectTimer must be active after stream error");
console.log("✓ Stream error handling schedules auto-reconnect timer");

console.log("\n============================================");
console.log("ALL MILESTONE 3 WEB-UI TESTS PASSED CLEANLY!");
console.log("============================================");

process.exit(0);
