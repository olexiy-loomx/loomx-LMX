<script>
  import { onMount } from 'svelte';
  import { Palette, Power, Settings, Play, SunMedium } from 'lucide-svelte';
  import BrandMark from '../components/BrandMark.svelte';
  import Button from '../components/Button.svelte';
  import StatusDot from '../components/StatusDot.svelte';
  import { route } from '../lib/router.js';
  import {
    applyPreset,
    children,
    loadAppData,
    live,
    presets,
    refreshConnectedChildren,
    sequences,
    setMasterBrightness,
    setMasterPower,
    triggerSequence,
  } from '../stores/appData.js';

  let brightnessPercentValue = 100;
  let brightnessTimer = null;
  let syncedBrightness = 255;

  $: online = $children.filter((child) => child.online).length;
  $: anyReconnecting = $children.some((child) => child.reconnecting);
  $: anyOffline = $children.some((child) => !child.online);
  $: status = anyOffline ? 'error' : anyReconnecting ? 'pending' : 'online';
  $: masterPower = $live.master_power ?? true;
  $: storeBrightness = Number($live.master_brightness ?? 255);
  $: if (!brightnessTimer && storeBrightness !== syncedBrightness) {
    brightnessPercentValue = Math.round((storeBrightness / 255) * 100);
    syncedBrightness = storeBrightness;
  }
  $: brightnessPercent = Math.max(0, Math.min(100, Math.round(Number(brightnessPercentValue) || 0)));

  onMount(() => {
    loadAppData();
    const refreshTimer = setInterval(refreshConnectedChildren, 3000);
    const onKey = (event) => {
      if (event.target?.matches?.('input, textarea, select')) return;
      if (event.key === ' ') {
        event.preventDefault();
        togglePower();
      } else if (event.key.toLowerCase() === 'e') {
        route.navigate('/edit/sequences');
      } else if (/^[1-9]$/.test(event.key)) {
        const sequence = $sequences[Number(event.key) - 1];
        if (sequence) triggerSequence(sequence.id);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => {
      window.removeEventListener('keydown', onKey);
      clearInterval(refreshTimer);
      if (brightnessTimer) clearTimeout(brightnessTimer);
    };
  });

  function togglePower() {
    setMasterPower(!masterPower);
  }

  function openPresetBuilder() {
    window.dispatchEvent(new CustomEvent('loomx:open-preset-builder'));
  }

  function scheduleBrightness() {
    if (brightnessTimer) clearTimeout(brightnessTimer);
    brightnessTimer = setTimeout(sendBrightness, 120);
  }

  function commitBrightness() {
    if (brightnessTimer) clearTimeout(brightnessTimer);
    sendBrightness();
  }

  function sendBrightness() {
    const percent = Math.max(0, Math.min(100, Number(brightnessPercentValue) || 0));
    const value = Math.round((percent / 100) * 255);
    brightnessPercentValue = percent;
    syncedBrightness = value;
    brightnessTimer = null;
    setMasterBrightness(value);
  }
</script>

<main class="view">
  <header class="topbar">
    <BrandMark size="sm" />
    <button class="node-pill" type="button" on:click={() => route.navigate('/edit/children')}>
      <StatusDot {status} pulse={status === 'pending'} />
      <span>{online}/{Math.max($children.length, 1)} nodes online</span>
    </button>
    <button class="icon-btn" type="button" aria-label="Edit mode" on:click={() => route.navigate('/edit/sequences')}>
      <Settings size={20} />
    </button>
  </header>

  <section class="master panel">
    <div class="master-head">
      <p class="eyebrow">Master</p>
      <button class="builder-btn" type="button" on:click={openPresetBuilder}>
        <Palette size={16} />Builder
      </button>
    </div>
    <label class="brightness">
      <span><SunMedium size={16} />Brightness</span>
      <strong>{brightnessPercent}%</strong>
      <input
        type="range"
        min="0"
        max="100"
        step="1"
        bind:value={brightnessPercentValue}
        on:input={scheduleBrightness}
        on:change={commitBrightness}
      />
    </label>
  </section>

  <section class="panel">
    <div class="section-title">
      <p class="eyebrow">Sequences</p>
    </div>
    {#if $sequences.length}
      <div class="grid">
        {#each $sequences as sequence, index (sequence.id)}
          <button
            type="button"
            class="tile"
            class:active={$live.running_sequence === sequence.id}
            on:click={() => triggerSequence(sequence.id)}
          >
            {#if $live.running_sequence === sequence.id}
              <Play size={14} />
            {/if}
            <span>{sequence.name || `Sequence ${index + 1}`}</span>
          </button>
        {/each}
      </div>
    {:else}
      <button class="empty-link" type="button" on:click={() => route.navigate('/edit/sequences/new')}>
        No sequences yet. Tap the gear to create one.
      </button>
    {/if}
  </section>

  <section class="panel presets">
    <p class="eyebrow">Presets</p>
    {#if $presets.length}
      <div class="grid">
        {#each $presets as preset (preset.id)}
          <button
            type="button"
            class="tile preset"
            class:active-preset={$live.active_preset === preset.id}
            on:click={() => applyPreset(preset.id)}
          >
            <span>{preset.name}</span>
            <span class="swatches">
              {#each preset.colors || [] as color}
                <i style={`background: ${color}`}></i>
              {/each}
            </span>
          </button>
        {/each}
      </div>
    {:else}
      <button class="empty-link" type="button" on:click={() => route.navigate('/edit/presets')}>
        No presets saved. Tap the gear to capture the current state.
      </button>
    {/if}
  </section>

  <div class="power-bar">
    <Button full on:click={togglePower}>
      <Power size={18} />{masterPower ? 'Turn off' : 'Turn on'}
    </Button>
  </div>
</main>

<style>
  .view {
    width: min(100%, 860px);
    min-height: 100vh;
    margin: 0 auto;
    padding: 20px 16px 24px;
    display: flex;
    flex-direction: column;
    gap: 28px;
    background: var(--bg-base);
  }
  @media (min-width: 768px) {
    .view { padding: 28px 32px 32px; }
  }
  .topbar {
    display: grid;
    grid-template-columns: auto minmax(0, 1fr) 44px;
    align-items: center;
    gap: 12px;
    min-height: 44px;
    border-bottom: 0.5px solid var(--border-default);
    padding-bottom: 12px;
  }
  .node-pill {
    justify-self: center;
    max-width: 100%;
    min-height: 36px;
    padding: 0 12px;
    display: inline-flex;
    align-items: center;
    gap: 8px;
    border-radius: var(--radius-pill);
    border: 0.5px solid var(--border-default);
    background: var(--bg-surface);
    color: var(--text-secondary);
    overflow: hidden;
  }
  .node-pill span {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .icon-btn {
    width: 44px;
    padding: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    background: transparent;
  }
  .panel {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  .master {
    gap: 14px;
    padding: 14px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
  }
  .master-head {
    min-height: 36px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .master-head .eyebrow {
    margin: 0;
  }
  .builder-btn {
    min-height: 34px;
    padding: 0 12px;
    display: inline-flex;
    align-items: center;
    gap: 8px;
    border-color: var(--accent);
    background: var(--accent);
    color: var(--accent-ink);
  }
  .brightness {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: 8px 12px;
    align-items: center;
  }
  .brightness span {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .brightness strong {
    color: var(--text-primary);
    font-size: 13px;
    font-weight: 500;
  }
  .brightness input {
    grid-column: 1 / -1;
    width: 100%;
    accent-color: var(--accent);
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 8px;
  }
  .tile {
    position: relative;
    min-height: 56px;
    padding: 8px;
    border-radius: 12px;
    border: 0.5px solid var(--border-strong);
    background: var(--border-default);
    color: var(--text-primary);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 5px;
    font-size: 13px;
    line-height: 16px;
    text-align: center;
    overflow: hidden;
  }
  .tile span:first-child {
    width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .tile.active {
    border: 1.5px solid var(--accent);
    color: var(--accent);
    animation: activePulse 1s ease-out infinite;
  }
  .tile.active-preset {
    border: 1.5px solid var(--accent);
  }
  .swatches {
    display: inline-flex;
    gap: 3px;
    max-width: 100%;
  }
  .swatches i {
    width: 8px;
    height: 8px;
    border-radius: 2px;
    display: block;
  }
  .empty-link {
    min-height: 56px;
    padding: 0 14px;
    border-style: dashed;
    color: var(--text-secondary);
    background: var(--bg-surface);
  }
  .power-bar {
    margin-top: auto;
    position: sticky;
    bottom: 24px;
  }
  @keyframes activePulse {
    0%, 100% { box-shadow: 0 0 0 0 rgba(198, 245, 66, 0); }
    50% { box-shadow: 0 0 0 2px rgba(198, 245, 66, 0.45); }
  }
  @media (prefers-reduced-motion: reduce) {
    .tile.active { animation: none; }
  }
</style>
