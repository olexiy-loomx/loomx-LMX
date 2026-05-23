<script>
  import { onMount } from 'svelte';
  import { ArrowLeft, Palette } from 'lucide-svelte';
  import StatusDot from '../components/StatusDot.svelte';
  import { route } from '../lib/router.js';
  import { children, loadAppData } from '../stores/appData.js';
  import { wsState } from '../stores/ws.js';
  import Sequences from './tabs/Sequences.svelte';
  import SequenceBuilder from './tabs/SequenceBuilder.svelte';
  import Presets from './tabs/Presets.svelte';
  import Children from './tabs/Children.svelte';
  import Configs from './tabs/Configs.svelte';
  import Settings from './tabs/Settings.svelte';

  export let path = '/edit/sequences';

  const tabs = [
    ['sequences', 'Sequences'],
    ['presets', 'Presets'],
    ['children', 'Children'],
    ['configs', 'Configs'],
    ['settings', 'Settings'],
  ];

  $: parts = path.split('/').filter(Boolean);
  $: tab = parts[1] || 'sequences';
  $: itemId = parts[2] || null;
  $: online = $children.filter((child) => child.online).length;
  $: connectionStatus = $wsState.reconnecting ? 'pending' : $wsState.connected ? 'online' : 'error';
  $: isBuilder = tab === 'sequences' && itemId;

  onMount(() => {
    loadAppData();
    const onKey = (event) => {
      if (event.key === 'Escape') route.navigate('/app');
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  function openPresetBuilder() {
    window.dispatchEvent(new CustomEvent('loomx:open-preset-builder'));
  }
</script>

<main class="edit">
  <header class="chrome">
    <button class="back" type="button" aria-label="Back to view mode" on:click={() => route.navigate('/app')}>
      <ArrowLeft size={20} />
    </button>
    <h1>Edit mode</h1>
    <button class="builder" type="button" aria-label="Open preset builder" on:click={openPresetBuilder}>
      <Palette size={18} />
    </button>
    <div class="status">
      <StatusDot status={connectionStatus} pulse={connectionStatus === 'pending'} />
      <span>{online}/{Math.max($children.length, 1)}</span>
    </div>
  </header>

  {#if !isBuilder}
    <nav class="tabs" aria-label="Edit mode sections">
      {#each tabs as [id, label]}
        <button
          type="button"
          class:active={tab === id}
          on:click={() => route.navigate(`/edit/${id}`)}
        >
          {label}
        </button>
      {/each}
    </nav>
  {/if}

  <section class="content">
    {#if isBuilder}
      <SequenceBuilder sequenceId={itemId} />
    {:else if tab === 'presets'}
      <Presets />
    {:else if tab === 'children'}
      <Children />
    {:else if tab === 'configs'}
      <Configs />
    {:else if tab === 'settings'}
      <Settings />
    {:else}
      <Sequences />
    {/if}
  </section>
</main>

<style>
  .edit {
    width: min(100%, 920px);
    min-height: 100vh;
    margin: 0 auto;
    padding: 16px;
    background: var(--bg-base);
  }
  @media (min-width: 768px) {
    .edit { padding: 24px 32px 32px; }
  }
  .chrome {
    min-height: 44px;
    display: grid;
    grid-template-columns: 44px minmax(0, 1fr) 44px auto;
    align-items: center;
    gap: 10px;
    padding-bottom: 12px;
    border-bottom: 0.5px solid var(--border-default);
  }
  .back {
    width: 44px;
    padding: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    background: transparent;
  }
  .builder {
    width: 44px;
    padding: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-color: var(--accent);
    background: var(--accent);
    color: var(--accent-ink);
  }
  h1 {
    margin: 0;
    color: var(--text-primary);
    font-size: 16px;
    font-weight: 500;
  }
  .status {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    color: var(--text-secondary);
    font-size: 12px;
  }
  .tabs {
    display: flex;
    gap: 18px;
    overflow-x: auto;
    padding: 12px 0 0;
    border-bottom: 0.5px solid var(--border-default);
  }
  .tabs button {
    min-height: 44px;
    padding: 0 0 10px;
    border: 0;
    border-radius: 0;
    background: transparent;
    color: var(--text-secondary);
    position: relative;
  }
  .tabs button.active {
    color: var(--text-primary);
  }
  .tabs button.active::after {
    content: '';
    position: absolute;
    left: 0;
    right: 0;
    bottom: -1px;
    height: 2px;
    background: var(--accent);
  }
  .content {
    padding-top: 20px;
  }
</style>
