<script>
  import { get } from 'svelte/store';
  import { onMount } from 'svelte';
  import { GripVertical, Plus, Save, Trash2, X } from 'lucide-svelte';
  import Button from '../../components/Button.svelte';
  import Input from '../../components/Input.svelte';
  import Toggle from '../../components/Toggle.svelte';
  import {
    children,
    durationLabel,
    loadAppData,
    newSequence,
    saveSequence,
    sequences,
    totalDuration,
  } from '../../stores/appData.js';
  import { route } from '../../lib/router.js';
  import { sendMessage } from '../../stores/ws.js';

  export let sequenceId = 'new';

  const targets = ['all', 'per_child', 'group_a', 'group_b'];
  const targetLabels = {
    all: 'All children',
    per_child: 'Per child',
    group_a: 'Group A',
    group_b: 'Group B',
  };
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
  const swatches = ['#ffffff', '#ff1919', '#ff7a1a', '#ffe600', '#32ff64', '#00d0ff', '#2563ff', '#ff4fd8'];

  let draft = newSequence();

  onMount(() => {
    loadAppData();
    const found = get(sequences).find((item) => item.id === sequenceId);
    draft = sequenceId === 'new' ? newSequence() : JSON.parse(JSON.stringify(found || newSequence()));
    const onKey = (event) => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 's') {
        event.preventDefault();
        save();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  $: duration = totalDuration(draft);

  function addStep() {
    draft.steps = [
      ...draft.steps,
      {
        id: `step_${Date.now().toString(36)}`,
        target: 'all',
        effect: 'solid',
        color: '#ffffff',
        duration_ms: 3000,
        fade_ms: 200,
        speed: 128,
        frequency: 128,
      },
    ];
  }

  function removeStep(id) {
    draft.steps = draft.steps.filter((step) => step.id !== id);
  }

  function preview(step) {
    sendMessage({
      type: 'preview_step',
      target: step.target,
      color: step.color,
      effect: step.effect,
      fade_ms: step.fade_ms,
      speed: step.speed ?? 128,
      frequency: step.frequency ?? 128,
    });
  }

  function effectLabel(name) {
    return effectLabels[name] || name.split('_').map((part) => part[0].toUpperCase() + part.slice(1)).join(' ');
  }

  function controlPercent(value) {
    return Math.round((Math.max(0, Math.min(255, Number(value) || 0)) / 255) * 100);
  }

  function setControl(step, key, percent) {
    step[key] = Math.round((Math.max(0, Math.min(100, Number(percent) || 0)) / 100) * 255);
    preview(step);
  }

  function save() {
    const saved = saveSequence(draft);
    draft = saved;
    route.navigate('/edit/sequences');
  }
</script>

<div class="builder">
  <header class="builder-header">
    <div class="name control">
      <span>Sequence name</span>
      <Input bind:value={draft.name} placeholder="Sequence name" ariaLabel="Sequence name" />
    </div>
    <div class="loop control">
      <span>Loop</span>
      <Toggle bind:checked={draft.loop} label="Loop sequence" />
    </div>
    <div class="actions">
      <Button on:click={save}><Save size={16} />Save</Button>
      <Button variant="ghost" on:click={() => route.navigate('/edit/sequences')}><X size={18} />Cancel</Button>
    </div>
  </header>

  <section class="steps">
    {#each draft.steps as step, index (step.id)}
      <article class="step-card">
        <div class="drag" aria-hidden="true"><GripVertical size={18} /></div>
        <div class="step-content">
          <div class="step-top">
            <span class="badge">Step {index + 1}</span>
            <button class="trash" type="button" aria-label={`Delete step ${index + 1}`} on:click={() => removeStep(step.id)}>
              <Trash2 size={18} />
            </button>
          </div>

          <div class="fields">
            <label>
              <span>Target</span>
              <select bind:value={step.target}>
                {#each targets as target}
                  <option value={target}>{targetLabels[target]}</option>
                {/each}
              </select>
            </label>
            <label>
              <span>Effect</span>
              <select bind:value={step.effect} on:change={() => preview(step)}>
                {#each effects as effect}
                  <option value={effect}>{effectLabel(effect)}</option>
                {/each}
              </select>
            </label>
          </div>

          {#if step.target === 'per_child'}
            <div class="child-swatches">
              {#each $children as child, childIndex}
                <button
                  type="button"
                  style={`background: ${swatches[childIndex % swatches.length]}`}
                  aria-label={`Color for ${child.name}`}
                ></button>
              {/each}
            </div>
          {:else}
            <div class="color-row">
              <div class="swatches">
                {#each swatches as color}
                  <button
                    type="button"
                    class:selected={step.color === color}
                    style={`background: ${color}`}
                    aria-label={`Set color ${color}`}
                    on:click={() => { step.color = color; preview(step); }}
                  ></button>
                {/each}
              </div>
              <Input bind:value={step.color} ariaLabel={`Hex color for step ${index + 1}`} on:change={() => preview(step)} />
            </div>
          {/if}

          <label class="slider">
            <span>Duration <strong>{durationLabel(step.duration_ms)}</strong></span>
            <input type="range" min="100" max="60000" step="100" bind:value={step.duration_ms} />
          </label>
          <label class="slider">
            <span>Fade <strong>{durationLabel(step.fade_ms)}</strong></span>
            <input type="range" min="0" max="5000" step="100" bind:value={step.fade_ms} />
          </label>
          <div class="dual-sliders">
            <label class="slider">
              <span>Speed <strong>{controlPercent(step.speed ?? 128)}%</strong></span>
              <input
                type="range"
                min="0"
                max="100"
                step="1"
                value={controlPercent(step.speed ?? 128)}
                on:input={(event) => setControl(step, 'speed', event.currentTarget.value)}
              />
            </label>
            <label class="slider">
              <span>Frequency <strong>{controlPercent(step.frequency ?? 128)}%</strong></span>
              <input
                type="range"
                min="0"
                max="100"
                step="1"
                value={controlPercent(step.frequency ?? 128)}
                on:input={(event) => setControl(step, 'frequency', event.currentTarget.value)}
              />
            </label>
          </div>
        </div>
      </article>
    {/each}
  </section>

  <Button variant="secondary" full on:click={addStep}><Plus size={18} />Add step</Button>
  <p class="total">Total: {durationLabel(duration)} · {draft.steps.length} {draft.steps.length === 1 ? 'step' : 'steps'}</p>
  <div class="bottom-actions">
    <Button on:click={save}><Save size={16} />Save</Button>
    <Button variant="ghost" on:click={() => route.navigate('/edit/sequences')}><X size={18} />Cancel</Button>
  </div>
</div>

<style>
  .builder {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .builder-header {
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    gap: 12px;
    padding-bottom: 14px;
    border-bottom: 0.5px solid var(--border-default);
  }
  @media (min-width: 820px) {
    .builder-header {
      grid-template-columns: minmax(0, 1fr) auto auto;
      align-items: end;
    }
  }
  label,
  .control {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  label span,
  .name span,
  .loop span {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .loop {
    min-height: 44px;
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .actions {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .bottom-actions {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: 10px;
    padding-top: 4px;
  }
  @media (max-width: 520px) {
    .bottom-actions {
      grid-template-columns: minmax(0, 1fr);
    }
  }
  .steps {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  .step-card {
    position: relative;
    display: grid;
    grid-template-columns: 24px minmax(0, 1fr);
    gap: 8px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
    overflow: hidden;
  }
  .drag {
    display: grid;
    place-items: center;
    color: var(--text-tertiary);
    border-right: 0.5px solid var(--border-default);
  }
  .step-content {
    display: flex;
    flex-direction: column;
    gap: 14px;
    padding: 14px 14px 16px 6px;
  }
  .step-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .badge {
    display: inline-flex;
    align-items: center;
    min-height: 24px;
    padding: 0 10px;
    border-radius: var(--radius-pill);
    background: rgba(198, 245, 66, 0.14);
    color: var(--accent);
    font-size: 12px;
    font-weight: 500;
  }
  .trash {
    width: 44px;
    padding: 0;
    color: var(--danger);
    border-color: transparent;
    background: transparent;
  }
  .fields {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 10px;
  }
  @media (max-width: 520px) {
    .fields {
      grid-template-columns: minmax(0, 1fr);
    }
  }
  .color-row {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 120px;
    gap: 10px;
    align-items: center;
  }
  @media (max-width: 520px) {
    .color-row {
      grid-template-columns: minmax(0, 1fr);
    }
  }
  .swatches,
  .child-swatches {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .swatches button,
  .child-swatches button {
    width: 32px;
    min-width: 32px;
    height: 32px;
    min-height: 32px;
    padding: 0;
    border-radius: 8px;
    border: 1px solid var(--border-default);
  }
  .swatches button.selected {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent);
  }
  .slider span {
    display: flex;
    justify-content: space-between;
    gap: 12px;
  }
  .slider strong {
    color: var(--text-primary);
    font-weight: 500;
  }
  .dual-sliders {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 10px;
  }
  @media (max-width: 520px) {
    .dual-sliders {
      grid-template-columns: minmax(0, 1fr);
    }
  }
  input[type='range'] {
    accent-color: var(--accent);
    width: 100%;
  }
  .total {
    margin: 0;
    color: var(--text-secondary);
  }
</style>
