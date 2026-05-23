<script>
  import { onMount } from 'svelte';
  import { get } from 'svelte/store';
  import { CheckSquare, Palette, Save, Square, X, Zap } from 'lucide-svelte';
  import Button from './Button.svelte';
  import Input from './Input.svelte';
  import StatusDot from './StatusDot.svelte';
  import {
    children,
    presets,
    refreshConnectedChildren,
    savePreset,
    stopSequencePlayback,
  } from '../stores/appData.js';
  import { addToast } from '../stores/toasts.js';
  import { sendMessage } from '../stores/ws.js';

  let open = false;
  let selectedIds = new Set();
  let presetName = '';
  let color = '#c6f542';
  let brightnessPercent = 100;
  let speedPercent = 50;
  let frequencyPercent = 50;
  let effect = 'solid';
  let lookMode = 'same';
  let childStates = {};
  let outputMode = 'all';
  let selectedOutputsByChild = {};
  let applying = false;

  const effects = [
    'solid',
    'flash',
    'fade',
    'pulse',
    'wipe',
    'theater',
    'twinkle',
    'sparkle',
    'strobe',
    'chase',
    'chase_rainbow',
    'rainbow',
    'scanner',
    'comet',
    'meteor',
    'fire',
    'colorwaves',
    'juggle',
    'sinelon',
    'noise',
    'glitter',
    'popcorn',
    'plasma',
  ];
  const effectLabels = {
    chase_rainbow: 'Chase Rainbow',
  };
  const swatches = ['#ffffff', '#c6f542', '#00d0ff', '#2563ff', '#ff4fd8', '#ff1919', '#ff7a1a', '#ffe600'];

  onMount(() => {
    const handleOpen = () => openPanel();
    window.addEventListener('loomx:open-preset-builder', handleOpen);
    return () => window.removeEventListener('loomx:open-preset-builder', handleOpen);
  });

  $: currentChildIds = new Set($children.map((child) => child.id));
  $: selectedCount = [...selectedIds].filter((id) => currentChildIds.has(id)).length;
  $: allSelected = $children.length > 0 && selectedCount === $children.length;
  $: selectedChildren = $children.filter((child) => selectedIds.has(child.id));
  $: brightnessLabel = `${Math.max(0, Math.min(100, Math.round(Number(brightnessPercent) || 0)))}%`;
  $: speedLabel = `${Math.max(0, Math.min(100, Math.round(Number(speedPercent) || 0)))}%`;
  $: frequencyLabel = `${Math.max(0, Math.min(100, Math.round(Number(frequencyPercent) || 0)))}%`;
  $: outputSummary = outputMode === 'all'
    ? 'All outputs'
    : `${selectedChildren.reduce((total, child) => total + selectedOutputSlots(child, selectedOutputsByChild, outputMode).length, 0)} selected`;

  function percentToBrightness(percent) {
    const value = Math.max(0, Math.min(100, Number(percent) || 0));
    return Math.round((value / 100) * 255);
  }

  function brightnessToPercent(brightness) {
    const value = Math.max(0, Math.min(255, Number(brightness) || 0));
    return Math.round((value / 255) * 100);
  }

  function percentToControl(percent) {
    const value = Math.max(0, Math.min(100, Number(percent) || 0));
    return Math.round((value / 100) * 255);
  }

  function controlToPercent(control) {
    const value = Math.max(0, Math.min(255, Number(control) || 0));
    return Math.round((value / 255) * 100);
  }

  function effectLabel(name) {
    return effectLabels[name] || name.split('_').map((part) => part[0].toUpperCase() + part.slice(1)).join(' ');
  }

  function normalizeColorValue(value, fallback = '#ffffff') {
    const next = String(value || '').trim();
    return /^#[0-9a-f]{6}$/i.test(next) ? next.toLowerCase() : fallback;
  }

  function setBaseColor(value) {
    color = normalizeColorValue(value, color);
  }

  function updateBaseColor(event) {
    setBaseColor(event.currentTarget.value);
  }

  function baseLookState() {
    return {
      color,
      effect,
      brightnessPercent: Number(brightnessPercent) || 0,
      speedPercent: Number(speedPercent) || 0,
      frequencyPercent: Number(frequencyPercent) || 0,
    };
  }

  function normalizeLookState(state = {}, fallback = baseLookState()) {
    return {
      color: state.color || fallback.color || '#ffffff',
      effect: effects.includes(state.effect) ? state.effect : (fallback.effect || 'solid'),
      brightnessPercent: state.brightnessPercent ?? brightnessToPercent(state.brightness ?? percentToBrightness(fallback.brightnessPercent)),
      speedPercent: state.speedPercent ?? controlToPercent(state.speed ?? percentToControl(fallback.speedPercent ?? 50)),
      frequencyPercent: state.frequencyPercent ?? controlToPercent(state.frequency ?? percentToControl(fallback.frequencyPercent ?? 50)),
    };
  }

  function childLookState(id) {
    return normalizeLookState(childStates[id], baseLookState());
  }

  function setChildLookState(id, patch) {
    childStates = {
      ...childStates,
      [id]: normalizeLookState({ ...childLookState(id), ...patch }),
    };
  }

  function updateChildColor(id, event) {
    setChildLookState(id, { color: normalizeColorValue(event.currentTarget.value, childLookState(id).color) });
  }

  function updateChildBrightness(id, event) {
    setChildLookState(id, { brightnessPercent: Number(event.currentTarget.value) });
  }

  function updateChildEffect(id, event) {
    setChildLookState(id, { effect: event.currentTarget.value });
  }

  function updateChildSpeed(id, event) {
    setChildLookState(id, { speedPercent: Number(event.currentTarget.value) });
  }

  function updateChildFrequency(id, event) {
    setChildLookState(id, { frequencyPercent: Number(event.currentTarget.value) });
  }

  function hydrateChildStates(ids = selectedIds, sourceStates = childStates) {
    const next = {};
    ids.forEach((id) => {
      next[id] = normalizeLookState(sourceStates[id], baseLookState());
    });
    childStates = next;
  }

  function setLookMode(mode) {
    lookMode = mode;
    if (mode === 'per_child') hydrateChildStates(selectedIds);
  }

  function buildStates(targetIds) {
    if (lookMode !== 'per_child') return null;
    return Object.fromEntries(targetIds.map((id) => {
      const state = childLookState(id);
      return [id, {
        color: state.color,
        effect: state.effect,
        brightness: percentToBrightness(state.brightnessPercent),
        speed: percentToControl(state.speedPercent),
        frequency: percentToControl(state.frequencyPercent),
      }];
    }));
  }

  function selectedTargetIds() {
    const currentIds = new Set($children.map((child) => child.id));
    return [...selectedIds].filter((id) => currentIds.has(id));
  }

  function selectAllConnected() {
    selectedIds = new Set(get(children).map((child) => child.id));
    if (lookMode === 'per_child') hydrateChildStates(selectedIds);
    if (outputMode === 'custom') hydrateOutputSelection(selectedIds);
  }

  async function openPanel() {
    open = true;
    await refreshConnectedChildren();
    if (!selectedIds.size) selectAllConnected();
  }

  function closePanel() {
    open = false;
  }

  function toggleChild(id) {
    const next = new Set(selectedIds);
    if (next.has(id)) next.delete(id);
    else next.add(id);
    selectedIds = next;
    if (lookMode === 'per_child') hydrateChildStates(selectedIds);
    if (outputMode === 'custom') hydrateOutputSelection(selectedIds);
  }

  function toggleAll() {
    selectedIds = allSelected ? new Set() : new Set($children.map((child) => child.id));
    if (lookMode === 'per_child') hydrateChildStates(selectedIds);
    if (outputMode === 'custom') hydrateOutputSelection(selectedIds);
  }

  function requireSelection() {
    const targetIds = selectedTargetIds();
    if (!targetIds.length) addToast('Select at least one child', 'error');
    return targetIds;
  }

  function outputSlots(child) {
    return (child?.outputs || [])
      .map((output, index) => Number(output.slot || index + 1))
      .filter((slot) => Number.isInteger(slot) && slot > 0);
  }

  function selectedOutputSlots(child, outputSelection = selectedOutputsByChild, mode = outputMode) {
    const slots = outputSlots(child);
    if (!slots.length) return [];
    if (mode === 'all') return slots;
    const selected = outputSelection[child.id];
    if (!Array.isArray(selected)) return slots;
    return selected.filter((slot) => slots.includes(slot));
  }

  function hydrateOutputSelection(ids = selectedIds) {
    const next = {};
    $children.forEach((child) => {
      if (!ids.has(child.id)) return;
      const slots = outputSlots(child);
      if (!slots.length) return;
      const existing = Array.isArray(selectedOutputsByChild[child.id])
        ? selectedOutputsByChild[child.id].filter((slot) => slots.includes(slot))
        : slots;
      next[child.id] = existing;
    });
    selectedOutputsByChild = next;
  }

  function setOutputMode(mode) {
    outputMode = mode;
    if (mode === 'all') selectedOutputsByChild = {};
    if (mode === 'custom') hydrateOutputSelection();
  }

  function toggleOutput(child, slot) {
    if (outputMode !== 'custom') {
      outputMode = 'custom';
      hydrateOutputSelection();
    }
    const nextSlots = new Set(selectedOutputSlots(child, selectedOutputsByChild, outputMode));
    if (nextSlots.has(slot)) nextSlots.delete(slot);
    else nextSlots.add(slot);
    selectedOutputsByChild = {
      ...selectedOutputsByChild,
      [child.id]: [...nextSlots].sort((a, b) => a - b),
    };
  }

  function buildOutputTargets(targetIds) {
    if (outputMode !== 'custom') return null;
    const targets = {};
    targetIds.forEach((id) => {
      const child = $children.find((item) => item.id === id);
      if (!child) return;
      const selected = selectedOutputSlots(child, selectedOutputsByChild, outputMode);
      if (selected.length) targets[id] = selected;
    });
    if (!Object.keys(targets).length) {
      addToast('Select at least one output', 'error');
      return null;
    }
    return targets;
  }

  function applyOutputTargets(message, targetIds) {
    const outputTargets = buildOutputTargets(targetIds);
    if (outputMode === 'custom') {
      if (!outputTargets) return null;
      message.output_targets = outputTargets;
      message.target_ids = Object.keys(outputTargets);
    }
    return message;
  }

  async function applyDraft() {
    if (applying) return;
    const targetIds = requireSelection();
    if (!targetIds.length) return;
    const states = buildStates(targetIds);
    const firstState = states ? Object.values(states)[0] : null;
    const message = states
      ? applyOutputTargets({
        type: 'preset_apply',
        cancel_sequence: true,
        target_ids: targetIds,
        preset: {
          id: 'draft',
          name: presetName || 'Draft preset',
          target_ids: targetIds,
          colors: Object.values(states).map((state) => state.color),
          state: firstState,
          states,
        },
      }, targetIds)
      : applyOutputTargets({
        type: 'preview_step',
        cancel_sequence: true,
        target_ids: targetIds,
        color,
        effect,
        brightness: percentToBrightness(brightnessPercent),
        speed: percentToControl(speedPercent),
        frequency: percentToControl(frequencyPercent),
        fade_ms: 200,
      }, targetIds);
    if (!message) return;
    applying = true;
    try {
      stopSequencePlayback();
      const result = await sendMessage(message);
      addToast(result.ok ? 'Look applied' : result.error || 'No connected children acknowledged', result.ok ? 'success' : 'error');
    } finally {
      applying = false;
    }
  }

  function saveDraft() {
    const targetIds = requireSelection();
    if (!targetIds.length) return;
    const outputTargets = buildOutputTargets(targetIds);
    if (outputMode === 'custom' && !outputTargets) return;
    const states = buildStates(targetIds);
    const saved = savePreset({
      name: presetName,
      target_ids: outputTargets ? Object.keys(outputTargets) : targetIds,
      output_targets: outputTargets,
      color,
      effect,
      brightness: percentToBrightness(brightnessPercent),
      speed: percentToControl(speedPercent),
      frequency: percentToControl(frequencyPercent),
      states,
    });
    presetName = saved.name;
  }
</script>

{#if !open}
  <button class="quick-builder" type="button" aria-label="Preset builder" on:click={openPanel}>
    <Palette size={22} />
  </button>
{/if}

{#if open}
  <div class="overlay" role="presentation" on:click={closePanel}></div>
  <section class="sheet" aria-modal="true" role="dialog" aria-label="Preset builder">
    <header>
      <div>
        <p class="eyebrow">Preset builder</p>
        <h2>{selectedCount} selected</h2>
      </div>
      <button class="icon" type="button" aria-label="Close preset builder" on:click={closePanel}>
        <X size={20} />
      </button>
    </header>

    <button class="select-all" type="button" disabled={!$children.length} on:click={toggleAll}>
      {#if allSelected}
        <CheckSquare size={18} />
      {:else}
        <Square size={18} />
      {/if}
      All
    </button>

    {#if $children.length}
      <div class="child-picker">
        {#each $children as child (child.id)}
          <button
            type="button"
            class:active={selectedIds.has(child.id)}
            on:click={() => toggleChild(child.id)}
          >
            {#if selectedIds.has(child.id)}
              <CheckSquare size={18} />
            {:else}
              <Square size={18} />
            {/if}
            <StatusDot status={child.online ? 'online' : 'offline'} pulse={child.reconnecting} />
            <span>{child.name}</span>
          </button>
        {/each}
      </div>
    {:else}
      <p class="empty compact">No children connected.</p>
    {/if}

    <div class="output-control">
      <div class="output-head">
        <span>Outputs <strong>{outputSummary}</strong></span>
        <div class="output-mode" aria-label="Output selection mode">
          <button type="button" class:active={outputMode === 'all'} on:click={() => setOutputMode('all')}>All</button>
          <button type="button" class:active={outputMode === 'custom'} disabled={!selectedCount} on:click={() => setOutputMode('custom')}>Choose</button>
        </div>
      </div>
      {#if outputMode === 'custom'}
        {#if selectedChildren.length}
          <div class="output-grid">
            {#each selectedChildren as child (child.id)}
              <div class="output-row">
                <div class="output-name">
                  <span>{child.name}</span>
                  <small>{child.controller_type || 'VX4'}</small>
                </div>
                <div class="output-buttons" aria-label={`Outputs for ${child.name}`}>
                  {#each outputSlots(child) as slot}
                    <button
                      type="button"
                      class:active={selectedOutputSlots(child, selectedOutputsByChild, outputMode).includes(slot)}
                      aria-label={`${child.name} output ${slot}`}
                      on:click={() => toggleOutput(child, slot)}
                    >{slot}</button>
                  {/each}
                </div>
              </div>
            {/each}
          </div>
        {:else}
          <p class="empty compact">Select a controller to choose outputs.</p>
        {/if}
      {/if}
    </div>

    <div class="fields">
      <div class="field">
        <span>Name</span>
        <Input bind:value={presetName} placeholder={`Preset ${$presets.length + 1}`} ariaLabel="Preset name" />
      </div>
      <div class="look-mode" aria-label="Preset look mode">
        <button type="button" class:active={lookMode === 'same'} on:click={() => setLookMode('same')}>Same</button>
        <button type="button" class:active={lookMode === 'per_child'} disabled={!selectedCount} on:click={() => setLookMode('per_child')}>Per child</button>
      </div>
    </div>

    {#if lookMode === 'same'}
      <label>
        <span>Effect</span>
        <select bind:value={effect}>
          {#each effects as effectName}
            <option value={effectName}>{effectLabel(effectName)}</option>
          {/each}
        </select>
      </label>

      <div class="color-control">
        <div class="color-head">
          <span>Color</span>
          <label class="color-input">
            <span class="color-chip" style:background-color={color} aria-hidden="true"></span>
            <input
              type="color"
              bind:value={color}
              aria-label="Preset color"
              on:change={updateBaseColor}
            />
            <span>{color}</span>
          </label>
        </div>
        <div class="swatch-grid">
          {#each swatches as swatch}
            <button
              type="button"
              class:selected={color === swatch}
              style={`background: ${swatch}`}
              aria-label={`Set color ${swatch}`}
              on:click={() => setBaseColor(swatch)}
            ></button>
          {/each}
        </div>
      </div>

      <label class="slider">
        <span>Brightness <strong>{brightnessLabel}</strong></span>
        <input type="range" min="0" max="100" step="1" bind:value={brightnessPercent} />
      </label>
      <label class="slider">
        <span>Speed <strong>{speedLabel}</strong></span>
        <input type="range" min="0" max="100" step="1" bind:value={speedPercent} />
      </label>
      <label class="slider">
        <span>Frequency <strong>{frequencyLabel}</strong></span>
        <input type="range" min="0" max="100" step="1" bind:value={frequencyPercent} />
      </label>
    {:else}
      <div class="child-looks">
        {#if selectedChildren.length}
          {#each selectedChildren as child (child.id)}
            <div class="child-look-row">
              <div class="child-look-name">
                <span>{child.name}</span>
                <small>B {childLookState(child.id).brightnessPercent}% · S {childLookState(child.id).speedPercent}% · F {childLookState(child.id).frequencyPercent}%</small>
              </div>
              <label class="mini-color">
                <span class="mini-chip" style:background-color={childLookState(child.id).color} aria-hidden="true"></span>
                <input
                  type="color"
                  value={childLookState(child.id).color}
                  aria-label={`${child.name} color`}
                  on:input={(event) => updateChildColor(child.id, event)}
                  on:change={(event) => updateChildColor(child.id, event)}
                />
              </label>
              <select
                value={childLookState(child.id).effect}
                aria-label={`${child.name} effect`}
                on:change={(event) => updateChildEffect(child.id, event)}
              >
                {#each effects as effectName}
                  <option value={effectName}>{effectLabel(effectName)}</option>
                {/each}
              </select>
              <div class="mini-sliders">
                <label>
                  <span>Brightness</span>
                  <input
                    class="mini-slider"
                    type="range"
                    min="0"
                    max="100"
                    step="1"
                    value={childLookState(child.id).brightnessPercent}
                    aria-label={`${child.name} brightness`}
                    on:input={(event) => updateChildBrightness(child.id, event)}
                    on:change={(event) => updateChildBrightness(child.id, event)}
                  />
                </label>
                <label>
                  <span>Speed</span>
                  <input
                    class="mini-slider"
                    type="range"
                    min="0"
                    max="100"
                    step="1"
                    value={childLookState(child.id).speedPercent}
                    aria-label={`${child.name} speed`}
                    on:input={(event) => updateChildSpeed(child.id, event)}
                    on:change={(event) => updateChildSpeed(child.id, event)}
                  />
                </label>
                <label>
                  <span>Frequency</span>
                  <input
                    class="mini-slider"
                    type="range"
                    min="0"
                    max="100"
                    step="1"
                    value={childLookState(child.id).frequencyPercent}
                    aria-label={`${child.name} frequency`}
                    on:input={(event) => updateChildFrequency(child.id, event)}
                    on:change={(event) => updateChildFrequency(child.id, event)}
                  />
                </label>
              </div>
            </div>
          {/each}
        {:else}
          <p class="empty compact">Select a controller to edit child looks.</p>
        {/if}
      </div>
    {/if}

    <div class="compose-actions">
      <Button variant="secondary" disabled={selectedCount === 0 || applying} on:click={applyDraft}><Zap size={16} />{applying ? 'Applying' : 'Apply'}</Button>
      <Button disabled={selectedCount === 0} on:click={saveDraft}><Save size={16} />Save as preset</Button>
    </div>
  </section>
{/if}

<style>
  .quick-builder {
    position: fixed;
    right: max(16px, env(safe-area-inset-right));
    bottom: calc(88px + env(safe-area-inset-bottom));
    z-index: 40;
    width: 56px;
    min-height: 56px;
    padding: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-color: var(--accent);
    border-radius: 18px;
    background: var(--accent);
    color: var(--accent-ink);
    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.45);
  }
  .overlay {
    position: fixed;
    inset: 0;
    z-index: 80;
    background: rgba(0, 0, 0, 0.62);
  }
  .sheet {
    position: fixed;
    left: 50%;
    bottom: max(12px, env(safe-area-inset-bottom));
    z-index: 90;
    width: min(calc(100vw - 24px), 680px);
    max-height: min(86vh, 760px);
    overflow: auto;
    transform: translateX(-50%);
    display: flex;
    flex-direction: column;
    gap: 14px;
    padding: 14px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
  }
  header,
  .color-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  h2 {
    margin: 2px 0 0;
    font-size: 16px;
    line-height: 22px;
    font-weight: 500;
  }
  .icon {
    width: 44px;
    padding: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    background: transparent;
  }
  .select-all {
    width: fit-content;
    min-height: 36px;
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 0 12px;
    border-radius: var(--radius-pill);
  }
  .select-all :global(svg),
  .child-picker button :global(svg) {
    color: var(--accent);
  }
  .child-picker {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 8px;
  }
  .child-picker button {
    min-width: 0;
    min-height: 44px;
    display: grid;
    grid-template-columns: 20px 8px minmax(0, 1fr);
    align-items: center;
    gap: 10px;
    padding: 0 12px;
    border-color: var(--border-default);
    background: var(--bg-surface-2);
    text-align: left;
  }
  .child-picker button.active {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent);
  }
  .child-picker span {
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .output-control {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .output-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .output-head > span {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .output-head strong {
    margin-left: 6px;
    color: var(--text-primary);
    font-weight: 500;
    text-transform: none;
  }
  .output-mode,
  .look-mode {
    min-width: 154px;
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 3px;
    padding: 3px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-pill);
    background: var(--bg-surface-2);
  }
  .output-mode button,
  .look-mode button {
    min-height: 32px;
    padding: 0 10px;
    border: 0;
    border-radius: var(--radius-pill);
    background: transparent;
  }
  .output-mode button.active,
  .look-mode button.active {
    background: var(--accent);
    color: var(--accent-ink);
  }
  .output-grid {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .output-row {
    display: grid;
    grid-template-columns: minmax(92px, 132px) minmax(0, 1fr);
    gap: 10px;
    align-items: center;
  }
  .output-name {
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .output-name span,
  .output-name small {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .output-name span {
    color: var(--text-primary);
    font-size: 13px;
  }
  .output-name small {
    color: var(--text-secondary);
    font-size: 11px;
    text-transform: uppercase;
  }
  .output-buttons {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
  }
  .output-buttons button {
    width: 34px;
    min-height: 32px;
    padding: 0;
    border-color: var(--border-default);
    border-radius: 9px;
    background: var(--bg-surface-2);
    color: var(--text-secondary);
    font-family: var(--font-mono);
  }
  .output-buttons button.active {
    border-color: var(--accent);
    background: rgba(198, 245, 66, 0.14);
    color: var(--accent);
  }
  .child-looks {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .child-look-row {
    display: grid;
    grid-template-columns: minmax(92px, 1fr) 40px minmax(112px, 160px);
    gap: 8px;
    align-items: center;
    padding: 10px;
    border: 0.5px solid var(--border-default);
    border-radius: 12px;
    background: var(--bg-surface-2);
  }
  .child-look-name {
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .child-look-name span,
  .child-look-name small {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .child-look-name span {
    color: var(--text-primary);
    font-size: 13px;
  }
  .child-look-name small {
    color: var(--text-secondary);
    font-size: 11px;
  }
  .mini-color {
    width: 36px;
    height: 36px;
    position: relative;
    display: block;
    overflow: hidden;
    border-radius: 9px;
    background: transparent;
  }
  .mini-chip {
    display: block;
    width: 100%;
    height: 100%;
    border-radius: inherit;
  }
  .mini-color input {
    position: absolute;
    inset: 0;
    width: 36px;
    height: 36px;
    padding: 0;
    border: 0;
    opacity: 0;
  }
  .mini-slider {
    width: 100%;
    accent-color: var(--accent);
  }
  .mini-sliders {
    grid-column: 1 / -1;
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 8px;
  }
  .mini-sliders label {
    gap: 4px;
  }
  .mini-sliders span {
    color: var(--text-secondary);
    font-size: 10px;
    font-weight: 500;
    text-transform: uppercase;
  }
  @media (max-width: 420px) {
    .output-head {
      align-items: stretch;
      flex-direction: column;
    }
    .output-mode {
      width: 100%;
    }
    .output-row {
      grid-template-columns: minmax(0, 1fr);
    }
    .child-look-row {
      grid-template-columns: minmax(0, 1fr) 40px;
    }
    .child-look-row select,
    .mini-sliders {
      grid-column: 1 / -1;
    }
    .mini-sliders {
      grid-template-columns: minmax(0, 1fr);
    }
  }
  .fields {
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    gap: 10px;
  }
  @media (min-width: 680px) {
    .fields {
      grid-template-columns: minmax(0, 1fr) 180px;
    }
  }
  label,
  .field,
  .color-control {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  label span,
  .field span,
  .color-head > span,
  .slider span {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  label :global(input),
  .field :global(input),
  label select {
    width: 100%;
  }
  .color-input {
    position: relative;
    min-height: 34px;
    flex-direction: row;
    align-items: center;
    gap: 8px;
    color: var(--text-primary);
    font-family: var(--font-mono);
    font-size: 12px;
    text-transform: uppercase;
  }
  .color-chip {
    width: 34px;
    height: 34px;
    flex: 0 0 34px;
    border: 0.5px solid var(--border-default);
    border-radius: 9px;
  }
  .color-input input {
    position: absolute;
    left: 0;
    top: 0;
    width: 34px;
    height: 34px;
    padding: 0;
    border: 0;
    border-radius: 9px;
    opacity: 0;
    cursor: pointer;
  }
  .swatch-grid {
    display: grid;
    grid-template-columns: repeat(8, minmax(0, 1fr));
    gap: 8px;
  }
  .swatch-grid button {
    width: 100%;
    min-height: 34px;
    border-radius: 9px;
    border-color: transparent;
  }
  .swatch-grid button.selected {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent);
  }
  .slider input {
    width: 100%;
    accent-color: var(--accent);
  }
  .slider strong {
    color: var(--text-primary);
    font-weight: 500;
  }
  .compose-actions {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .compact {
    padding: 12px;
  }
</style>
