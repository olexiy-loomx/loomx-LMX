<script>
  import { onDestroy, onMount } from 'svelte';
  import { Check, Power, RadioTower, RotateCcw, Upload } from 'lucide-svelte';
  import Button from '../../components/Button.svelte';
  import Input from '../../components/Input.svelte';
  import Toggle from '../../components/Toggle.svelte';
  import { api, DEFAULT_FIRMWARE_VERSION } from '../../lib/api.js';
  import { route } from '../../lib/router.js';
  import { loadNetwork, saveNetwork } from '../../stores/network.js';
  import { commitRole } from '../../stores/role.js';
  import { addToast } from '../../stores/toasts.js';

  let ssid = '';
  let password = '';
  let hidden = false;
  let channel = 'auto';
  let transport = 'http';
  let transportOptions = ['http', 'udp', 'tcp', 'websocket', 'mqtt'];
  let system = { firmware: DEFAULT_FIRMWARE_VERSION, uptime: '00:00:00', free_heap: '0 KB' };
  let childOta = { running: false, total: 0, completed: 0, succeeded: 0, failed: 0, firmware: '', current: '', error: '' };
  let childOtaTimer = null;
  let confirmingReset = false;
  let wakeLock = null;
  const transportLabels = {
    http: 'HTTP',
    udp: 'UDP',
    tcp: 'TCP',
    websocket: 'WebSocket',
    ws: 'WebSocket',
    mqtt: 'MQTT',
  };
  const transportDetails = {
    http: 'Port 80',
    udp: 'Port 42717',
    tcp: 'Port 42718',
    websocket: 'Port 80',
    ws: 'Port 80',
    mqtt: 'Port 1883',
  };

  $: childOtaTotal = Number(childOta.total || 0);
  $: childOtaCompleted = Number(childOta.completed || 0);
  $: childOtaProgress = childOtaTotal > 0 ? Math.min(100, Math.round((childOtaCompleted / childOtaTotal) * 100)) : 0;
  $: childOtaCurrent = childOta.current || childOta.firmware || 'Preparing nodes';

  onMount(async () => {
    const cfg = await loadNetwork();
    ssid = cfg.ssid;
    password = cfg.password;
    hidden = cfg.hidden;
    channel = cfg.channel || 'auto';
    const transportCfg = await api.getTransport();
    transport = transportCfg.transport || 'http';
    transportOptions = transportCfg.options || transportOptions;
    system = await api.getSystemInfo();
    const otaStatus = await api.getChildFirmwareOtaStatus();
    childOta = otaStatus.status || childOta;
    if (childOta.running) pollChildOta();
  });

  onDestroy(() => {
    if (childOtaTimer) clearTimeout(childOtaTimer);
    releaseWakeLock();
  });

  async function requestWakeLock() {
    if (!('wakeLock' in navigator) || wakeLock) return;
    try {
      wakeLock = await navigator.wakeLock.request('screen');
      wakeLock.addEventListener('release', () => wakeLock = null);
    } catch {
      wakeLock = null;
    }
  }

  function releaseWakeLock() {
    if (!wakeLock) return;
    wakeLock.release().catch(() => {});
    wakeLock = null;
  }

  async function save() {
    await saveNetwork({ ssid, password, hidden, channel });
    addToast('Network saved', 'success');
  }

  async function saveTransport() {
    const result = await api.setTransport(transport);
    if (result.ok) {
      transport = result.transport || transport;
      addToast('Data transmission saved', 'success');
      return;
    }
    addToast(result.error || 'Data transmission failed to save', 'error');
  }

  function selectTransport(option) {
    transport = option;
  }

  function transportLabel(option) {
    return transportLabels[option] || String(option).toUpperCase();
  }

  function transportDetail(option) {
    return transportDetails[option] || 'Parent to children';
  }

  async function switchToChild() {
    await commitRole('child');
    route.navigate('/child');
  }

  function reset() {
    if (!confirmingReset) {
      confirmingReset = true;
      setTimeout(() => confirmingReset = false, 3000);
      return;
    }
    localStorage.clear();
    route.navigate('/');
  }

  function compactOtaText(value) {
    const text = String(value || '');
    if (!text) return '';
    if (text.includes('<!DOCTYPE') || text.includes('<html')) return 'Child returned a web page instead of OTA JSON';
    return text.length > 96 ? `${text.slice(0, 93)}...` : text;
  }

  async function pollChildOta() {
    if (childOtaTimer) clearTimeout(childOtaTimer);
    const result = await api.getChildFirmwareOtaStatus();
    childOta = result.status || childOta;
    if (childOta.running) {
      requestWakeLock();
      childOtaTimer = setTimeout(pollChildOta, 1500);
    } else if (childOta.completed > 0 || childOta.failed > 0) {
      releaseWakeLock();
      addToast(`Children updated: ${childOta.succeeded}/${childOta.total}`, childOta.failed ? 'error' : 'success');
    } else {
      releaseWakeLock();
    }
  }

  async function updateChildren() {
    const result = await api.startChildFirmwareOta();
    childOta = result.status || childOta;
    if (!result.ok) {
      addToast(result.error || 'Child update failed to start', 'error');
      return;
    }
    addToast(result.started ? 'Child firmware and UI update started' : 'Child firmware and UI update already running', 'success');
    if (childOta.running) requestWakeLock();
    pollChildOta();
  }
</script>

<div class="tab-page">
  <h2>Settings</h2>

  <section>
    <p class="eyebrow">Parent data</p>
    <div class="settings-grid protocol-grid">
      <div class="protocol-heading">
        <div class="protocol-icon" aria-hidden="true">
          <RadioTower size={18} />
        </div>
        <div>
          <span>Protocol</span>
          <strong>{transportLabel(transport)}</strong>
        </div>
      </div>
      <div class="protocol-options" role="radiogroup" aria-label="Parent data protocol">
        {#each transportOptions as option}
          <button
            type="button"
            class="protocol-option"
            class:active={transport === option}
            aria-checked={transport === option}
            role="radio"
            on:click={() => selectTransport(option)}
          >
            <span>{transportLabel(option)}</span>
            <small>{transportDetail(option)}</small>
            {#if transport === option}
              <Check size={16} />
            {/if}
          </button>
        {/each}
      </div>
      <Button on:click={saveTransport}>Save protocol</Button>
    </div>
  </section>

  <section>
    <p class="eyebrow">Network</p>
    <div class="settings-grid">
      <div class="control">
        <span>Name</span>
        <Input bind:value={ssid} ariaLabel="Network name" />
      </div>
      <div class="control">
        <span>Password</span>
        <Input bind:value={password} ariaLabel="Password" />
      </div>
      <div class="row">
        <span>Hide SSID</span>
        <Toggle bind:checked={hidden} label="Hide SSID" />
      </div>
      <label>
        <span>Channel</span>
        <select bind:value={channel}>
          <option value="auto">Auto</option>
          {#each [1,2,3,4,5,6,7,8,9,10,11] as number}
            <option value={String(number)}>{number}</option>
          {/each}
        </select>
      </label>
      <Button on:click={save}>Save network</Button>
    </div>
  </section>

  <section>
    <p class="eyebrow">Role</p>
    <div class="settings-grid compact">
      <div class="row">
        <span>Current</span>
        <strong>Parent</strong>
      </div>
      <Button variant="secondary" on:click={switchToChild}>Switch to Child</Button>
    </div>
  </section>

  <section>
    <p class="eyebrow">System</p>
    <div class="settings-grid compact">
      <div class="row">
        <span>Firmware</span>
        <strong>{system.firmware}</strong>
      </div>
      <div class="row">
        <span>Free heap</span>
        <strong>{system.free_heap}</strong>
      </div>
      <div class="system-actions">
        <Button on:click={updateChildren} disabled={childOta.running}><Upload size={16} />Update children</Button>
        <Button variant="secondary" on:click={() => api.post('/api/system/restart').then(() => addToast('Restart requested', 'success'))}><Power size={16} />Restart</Button>
        <Button variant="danger" on:click={reset}><RotateCcw size={16} />{confirmingReset ? 'Confirm factory reset' : 'Factory reset'}</Button>
      </div>
      <div class="ota-status">
        <span>{childOta.running ? 'Updating' : 'Child firmware + UI'}</span>
        <strong>{childOta.succeeded}/{childOta.total} updated</strong>
        {#if childOta.current}
          <small>{childOta.current}</small>
        {:else if childOta.error}
          <small>{compactOtaText(childOta.error)}</small>
        {:else if childOta.firmware}
          <small>{childOta.firmware}</small>
        {/if}
      </div>
    </div>
  </section>
</div>

{#if childOta.running}
  <div class="update-overlay" role="alertdialog" aria-modal="true" aria-labelledby="child-update-title">
    <div class="update-panel">
      <div class="loader" aria-hidden="true"></div>
      <p class="eyebrow">Updating nodes</p>
      <h2 id="child-update-title">Please wait for the nodes to update.</h2>
      <p>Do not close app or screen.</p>
      <div class="progress-track" aria-label="Child update progress">
        <span style={`width: ${childOtaProgress}%`}></span>
      </div>
      <strong>{childOta.succeeded}/{childOta.total} updated</strong>
      <small>{compactOtaText(childOtaCurrent)}</small>
    </div>
  </div>
{/if}

<style>
  .tab-page {
    display: flex;
    flex-direction: column;
    gap: 24px;
  }
  h2 {
    margin: 0;
    font-size: 22px;
    font-weight: 500;
  }
  section {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .settings-grid {
    display: grid;
    gap: 14px;
    padding: 16px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
  }
  .protocol-grid {
    gap: 16px;
  }
  .protocol-heading {
    display: grid;
    grid-template-columns: 40px minmax(0, 1fr);
    gap: 12px;
    align-items: center;
  }
  .protocol-heading span {
    display: block;
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .protocol-heading strong {
    display: block;
    margin-top: 3px;
    color: var(--text-primary);
    font-size: 20px;
    font-weight: 500;
  }
  .protocol-icon {
    width: 40px;
    height: 40px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface-2);
    color: var(--accent);
  }
  .protocol-options {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 8px;
  }
  .protocol-option {
    min-height: 66px;
    position: relative;
    display: grid;
    gap: 4px;
    justify-items: start;
    align-content: center;
    padding: 10px 36px 10px 12px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface-2);
    color: var(--text-primary);
    text-align: left;
  }
  .protocol-option.active {
    border-color: var(--accent);
    background: #202815;
  }
  .protocol-option span {
    font-size: 14px;
    font-weight: 600;
  }
  .protocol-option small {
    color: var(--text-secondary);
    font-size: 11px;
    font-weight: 500;
  }
  .protocol-option :global(svg) {
    position: absolute;
    top: 12px;
    right: 12px;
    color: var(--accent);
  }
  @media (min-width: 760px) {
    .settings-grid:not(.compact) {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
    .protocol-grid {
      grid-template-columns: minmax(180px, 0.45fr) minmax(0, 1fr);
      align-items: center;
    }
    .protocol-grid > :global(button) {
      grid-column: 1 / -1;
    }
  }
  @media (max-width: 380px) {
    .protocol-options {
      grid-template-columns: 1fr;
    }
  }
  label,
  .control {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  label span,
  .control span,
  .row span {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .row {
    min-height: 44px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 14px;
  }
  .row strong {
    color: var(--text-primary);
    font-weight: 500;
  }
  .system-actions {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
  }
  .ota-status {
    min-height: 44px;
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: 4px 12px;
    align-items: center;
    padding-top: 2px;
  }
  .ota-status span,
  .ota-status small {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
  }
  .ota-status span {
    text-transform: uppercase;
  }
  .ota-status strong {
    color: var(--text-primary);
    font-size: 14px;
    font-weight: 500;
  }
  .ota-status small {
    grid-column: 1 / -1;
    overflow-wrap: anywhere;
  }
  .update-overlay {
    position: fixed;
    inset: 0;
    z-index: 120;
    display: grid;
    place-items: center;
    padding: 24px;
    background: rgba(0, 0, 0, 0.86);
  }
  .update-panel {
    width: min(100%, 420px);
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 12px;
    padding: 28px 20px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
    text-align: center;
  }
  .update-panel h2 {
    margin: 0;
    max-width: 320px;
    font-size: 22px;
    line-height: 28px;
  }
  .update-panel p {
    margin: 0;
    color: var(--text-secondary);
    font-size: 15px;
    line-height: 22px;
  }
  .update-panel strong {
    color: var(--text-primary);
    font-size: 15px;
    font-weight: 500;
  }
  .update-panel small {
    max-width: 100%;
    color: var(--text-secondary);
    overflow-wrap: anywhere;
  }
  .loader {
    width: 42px;
    height: 42px;
    border: 4px solid var(--border-default);
    border-top-color: var(--accent);
    border-radius: 50%;
    animation: spin 0.9s linear infinite;
  }
  .progress-track {
    width: 100%;
    height: 8px;
    overflow: hidden;
    border-radius: var(--radius-pill);
    background: var(--bg-surface-2);
  }
  .progress-track span {
    display: block;
    height: 100%;
    border-radius: inherit;
    background: var(--accent);
    transition: width var(--t-base) var(--ease-out);
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
</style>
