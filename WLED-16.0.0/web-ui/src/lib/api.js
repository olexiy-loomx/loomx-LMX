const ROLE_KEY = 'loomx_role';
const NETWORK_KEY = 'loomx_network';
const SYSTEM_KEY = 'loomx_system';
const CHILD_CONFIG_KEY = 'loomx_child_config';
const LOOMX_AP_PASSWORD = 'loomxleaf';
export const DEFAULT_FIRMWARE_VERSION = typeof __LOOMX_FIRMWARE_VERSION__ === 'string'
  ? __LOOMX_FIRMWARE_VERSION__
  : 'v0.0.0';

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function fourDigits() {
  return String(Math.floor(1000 + Math.random() * 9000));
}

export function generateNetworkConfig() {
  const id = fourDigits();
  return {
    ssid: `Loomx_Parent_${id}`,
    password: LOOMX_AP_PASSWORD,
    hidden: false,
    channel: 'auto',
  };
}

function readJson(key, fallback) {
  try {
    const raw = localStorage.getItem(key);
    return raw ? JSON.parse(raw) : fallback;
  } catch {
    return fallback;
  }
}

function writeJson(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
  return value;
}

function normalizeSystemInfo(info = {}) {
  const firmware = info.firmware && info.firmware !== 'v0.1.0'
    ? info.firmware
    : DEFAULT_FIRMWARE_VERSION;
  return {
    firmware,
    uptime: info.uptime || '00:00:00',
    free_heap: info.free_heap || '0 KB',
  };
}

async function request(path, options = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 900);
  try {
    const res = await fetch(path, {
      ...options,
      signal: controller.signal,
      headers: {
        'Content-Type': 'application/json',
        ...(options.headers || {}),
      },
    });
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    const type = res.headers.get('content-type') || '';
    if (!type.includes('application/json')) throw new Error('non-json response');
    return await res.json();
  } finally {
    clearTimeout(timer);
  }
}

async function fromFirmware(path, options, fallback) {
  try {
    return await request(path, options);
  } catch {
    await delay(40);
    return fallback();
  }
}

export const api = {
  async getRole() {
    return fromFirmware('/api/role', {}, () => ({ role: localStorage.getItem(ROLE_KEY) || null }));
  },

  async setRole(role) {
    return fromFirmware('/api/role', {
      method: 'POST',
      body: JSON.stringify({ role }),
    }, () => {
      if (role) localStorage.setItem(ROLE_KEY, role);
      else localStorage.removeItem(ROLE_KEY);
      return { ok: true };
    });
  },

  async clearRole() {
    return this.setRole(null);
  },

  async getNetwork() {
    return fromFirmware('/api/network', {}, () => {
      const saved = readJson(NETWORK_KEY, null);
      if (saved) return writeJson(NETWORK_KEY, { ...saved, password: LOOMX_AP_PASSWORD });
      return writeJson(NETWORK_KEY, generateNetworkConfig());
    });
  },

  async setNetwork(cfg) {
    const normalized = { ...cfg, password: LOOMX_AP_PASSWORD };
    return fromFirmware('/api/network', {
      method: 'POST',
      body: JSON.stringify(normalized),
    }, () => {
      writeJson(NETWORK_KEY, normalized);
      return { ok: true };
    });
  },

  async getTransport() {
    return fromFirmware('/api/loomx/transport', {}, () => ({
      transport: readJson('loomx_transport', 'http'),
      options: ['http', 'udp', 'tcp', 'websocket', 'mqtt'],
      ports: { udp: 42717, tcp: 42718, mqtt: 1883 },
    }));
  },

  async setTransport(transport) {
    return fromFirmware('/api/loomx/transport', {
      method: 'POST',
      body: JSON.stringify({ transport }),
    }, () => {
      writeJson('loomx_transport', transport);
      return { ok: true, transport };
    });
  },

  async getChildConfig() {
    return fromFirmware('/api/child/config', {}, () => readJson(CHILD_CONFIG_KEY, {
      controller_type: 'VX4',
    }));
  },

  async setChildConfig(cfg) {
    return fromFirmware('/api/child/config', {
      method: 'POST',
      body: JSON.stringify(cfg),
    }, () => {
      writeJson(CHILD_CONFIG_KEY, cfg);
      return { ok: true };
    });
  },

  async getSystemInfo() {
    const system = await fromFirmware('/api/system/info', {}, () => readJson(SYSTEM_KEY, {}));
    return writeJson(SYSTEM_KEY, normalizeSystemInfo(system));
  },

  async startChildFirmwareOta() {
    return fromFirmware('/api/children/ota', {
      method: 'POST',
      body: JSON.stringify({}),
    }, () => ({
      ok: false,
      started: false,
      error: 'firmware_unavailable',
      status: {
        running: false,
        total: 0,
        completed: 0,
        succeeded: 0,
        failed: 0,
        firmware: DEFAULT_FIRMWARE_VERSION,
        current: '',
        error: 'firmware_unavailable',
      },
    }));
  },

  async getChildFirmwareOtaStatus() {
    return fromFirmware('/api/children/ota/status', {}, () => ({
      ok: false,
      status: {
        running: false,
        total: 0,
        completed: 0,
        succeeded: 0,
        failed: 0,
        firmware: DEFAULT_FIRMWARE_VERSION,
        current: '',
        error: '',
      },
    }));
  },

  async scanChildren() {
    return fromFirmware('/api/children/scan', {}, () => ({
      available: [],
      ok: false,
    }));
  },

  async getChildrenStatus() {
    return fromFirmware('/api/children/status', {}, () => ({
      children: [],
    }));
  },

  async post(path, body = {}) {
    return fromFirmware(path, {
      method: 'POST',
      body: JSON.stringify(body),
    }, () => ({ ok: true }));
  },
};
