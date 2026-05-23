<script>
  import { onMount } from 'svelte';
  import BrandMark from '../components/BrandMark.svelte';
  import Button from '../components/Button.svelte';
  import SignalBars from '../components/SignalBars.svelte';
  import StatusDot from '../components/StatusDot.svelte';
  import { api, DEFAULT_FIRMWARE_VERSION } from '../lib/api.js';
  import { route } from '../lib/router.js';
  import { loadNetwork } from '../stores/network.js';
  import { commitRole } from '../stores/role.js';

  let parentSsid = 'Loomx_Parent_4827';
  let firmware = DEFAULT_FIRMWARE_VERSION;
  let status = 'online';

  onMount(async () => {
    const cfg = await loadNetwork();
    parentSsid = cfg.ssid;
    const system = await api.getSystemInfo();
    firmware = system.firmware || firmware;
  });

  async function switchRole() {
    await commitRole('parent');
    route.navigate('/setup/network');
  }
</script>

<main class="page-shell child">
  <header><BrandMark size="md" /></header>

  <section>
    <h1>Loomx Leaf</h1>
    <div class="connection">
      <StatusDot {status} />
      <div>
        <p>Connected to</p>
        <strong>{parentSsid}</strong>
      </div>
    </div>
  </section>

  <dl class="meta">
    <div>
      <dt>ID</dt>
      <dd>AA:BB:CC:DD:EE:01</dd>
    </div>
    <div>
      <dt>Outputs</dt>
      <dd>4 configured</dd>
    </div>
    <div>
      <dt>Signal</dt>
      <dd><SignalBars rssi={-52} /></dd>
    </div>
    <div>
      <dt>Firmware</dt>
      <dd>{firmware}</dd>
    </div>
  </dl>

  <footer>
    <Button variant="secondary" full on:click={switchRole}>Switch to Parent role</Button>
  </footer>
</main>

<style>
  .child {
    max-width: 560px;
    display: flex;
    flex-direction: column;
    gap: 32px;
  }
  header {
    display: flex;
    justify-content: center;
    padding-bottom: 24px;
  }
  h1 {
    margin: 0 0 24px;
    font-size: 22px;
    line-height: 28px;
    font-weight: 500;
  }
  .connection {
    display: flex;
    align-items: flex-start;
    gap: 12px;
  }
  .connection p {
    margin: -6px 0 2px;
    color: var(--text-secondary);
  }
  .connection strong {
    font-size: 16px;
    font-weight: 500;
    color: var(--text-primary);
  }
  .meta {
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 14px;
  }
  .meta div {
    display: grid;
    grid-template-columns: 92px minmax(0, 1fr);
    gap: 12px;
  }
  dt {
    color: var(--text-secondary);
  }
  dd {
    margin: 0;
    color: var(--text-primary);
    font-family: var(--font-mono);
    overflow-wrap: anywhere;
  }
  footer {
    margin-top: auto;
  }
</style>
