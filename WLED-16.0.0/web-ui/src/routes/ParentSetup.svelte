<script>
  import { RefreshCw } from 'lucide-svelte';
  import { onMount } from 'svelte';
  import BrandMark from '../components/BrandMark.svelte';
  import Button from '../components/Button.svelte';
  import Input from '../components/Input.svelte';
  import Toggle from '../components/Toggle.svelte';
  import { route } from '../lib/router.js';
  import { loadNetwork, newNetworkConfig, saveNetwork } from '../stores/network.js';
  import { addToast } from '../stores/toasts.js';

  let ssid = '';
  let password = '';
  let hidden = false;
  let channel = 'auto';
  let saving = false;

  onMount(async () => {
    const cfg = await loadNetwork();
    ssid = cfg.ssid;
    password = cfg.password;
    hidden = cfg.hidden;
    channel = cfg.channel || 'auto';
  });

  function regen() {
    const cfg = newNetworkConfig();
    ssid = cfg.ssid;
    password = cfg.password;
    hidden = cfg.hidden;
    channel = cfg.channel;
  }

  async function continueSetup() {
    saving = true;
    await saveNetwork({ ssid, password, hidden, channel });
    saving = false;
    addToast('Network saved', 'success');
    route.navigate('/app');
  }
</script>

<main class="page-shell setup">
  <header><BrandMark size="md" /></header>

  <section class="intro">
    <h1>Your network is ready</h1>
    <p>Auto-generated. Edit now or later.</p>
  </section>

  <section class="form">
    <div class="control">
      <span>Network name</span>
      <div class="field-row">
        <Input bind:value={ssid} ariaLabel="Network name" />
        <Button variant="secondary" ariaLabel="Regenerate network name" on:click={regen}>
          <RefreshCw size={18} />
        </Button>
      </div>
    </div>

    <div class="control">
      <span>Password</span>
      <div class="field-row">
        <Input bind:value={password} ariaLabel="Password" />
        <Button variant="secondary" ariaLabel="Regenerate password" on:click={regen}>
          <RefreshCw size={18} />
        </Button>
      </div>
    </div>

    <div class="toggle-row">
      <div>
        <strong>Hide network</strong>
        <p>Network won't appear in scans. You'll need to enter the name manually to connect.</p>
      </div>
      <Toggle bind:checked={hidden} label="Hide network" />
    </div>
  </section>

  <footer>
    <Button full disabled={saving || !ssid || !password} on:click={continueSetup}>
      {saving ? 'Saving' : 'Continue'}
    </Button>
    <Button variant="ghost" full on:click={continueSetup}>Skip - use defaults</Button>
  </footer>
</main>

<style>
  .setup {
    display: flex;
    flex-direction: column;
    gap: 24px;
    max-width: 560px;
  }
  header {
    display: flex;
    justify-content: center;
    padding-bottom: 8px;
  }
  h1 {
    margin: 0;
    color: var(--text-primary);
    font-size: 22px;
    line-height: 28px;
    font-weight: 500;
  }
  .intro p {
    margin: 8px 0 0;
    color: var(--text-secondary);
  }
  .form {
    display: flex;
    flex-direction: column;
    gap: 20px;
  }
  .control {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .control span {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .field-row {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 44px;
    gap: 8px;
  }
  .field-row :global(button) {
    padding: 0;
  }
  .toggle-row {
    min-height: 64px;
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    gap: 18px;
    padding: 2px 0;
  }
  .toggle-row strong {
    color: var(--text-primary);
    font-weight: 500;
  }
  .toggle-row p {
    margin: 4px 0 0;
    color: var(--text-secondary);
    font-size: 13px;
    line-height: 19px;
  }
  footer {
    margin-top: auto;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
</style>
