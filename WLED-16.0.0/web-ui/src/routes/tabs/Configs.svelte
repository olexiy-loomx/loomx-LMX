<script>
  import { Download, FileUp } from 'lucide-svelte';
  import Button from '../../components/Button.svelte';
  import Input from '../../components/Input.svelte';
  import Toggle from '../../components/Toggle.svelte';
  import { children, cloneChildConfig, updateChildConfig } from '../../stores/appData.js';
  import { addToast } from '../../stores/toasts.js';

  const ledTypes = ['WS2812B', 'WS2811', 'SK6812', 'APA102', 'TM1814'];
  const colorOrders = ['GRB', 'RGB', 'BRG', 'RBG', 'BGR', 'GBR'];
  const controllerTypes = [
    { value: 'VX4', label: 'VX4', outputs: 4 },
    { value: 'VX8', label: 'VX8', outputs: 8 },
  ];

  let selectedId = '';
  let controllerType = 'VX4';
  let outputs = [];
  let cloneOpen = false;
  let cloneTargets = new Set();
  let saving = false;
  let cloning = false;

  $: if (!selectedId && $children.length) selectedId = $children[0].id;
  $: selectedChild = $children.find((child) => child.id === selectedId);
  $: selectedController = controllerTypes.find((type) => type.value === controllerType) || controllerTypes[0];
  $: if (selectedChild && outputs.length === 0) loadSelected();

  function loadSelected() {
    controllerType = selectedChild?.controller_type || 'VX4';
    outputs = selectedChild?.outputs.map((output) => ({ ...output })) || [];
    reconcileOutputs();
  }

  function defaultOutput(slot) {
    return { slot, led_type: 'WS2812B', count: 60, color_order: 'GRB' };
  }

  function reconcileOutputs() {
    const target = (controllerTypes.find((type) => type.value === controllerType) || controllerTypes[0]).outputs;
    outputs = Array.from({ length: target }, (_, index) => ({
      ...defaultOutput(index + 1),
      ...(outputs[index] || {}),
      slot: index + 1,
    }));
  }

  function setControllerType(value) {
    controllerType = value;
    reconcileOutputs();
  }

  async function save() {
    saving = true;
    try {
      await updateChildConfig(selectedId, {
        controller_type: controllerType,
        outputs: outputs.map((output) => ({ ...output, count: Number(output.count) || 0 })),
      });
    } finally {
      saving = false;
    }
  }

  function toggleClone(id) {
    const next = new Set(cloneTargets);
    if (next.has(id)) next.delete(id);
    else next.add(id);
    cloneTargets = next;
  }

  async function confirmClone() {
    cloning = true;
    try {
      await cloneChildConfig(selectedId, [...cloneTargets]);
      cloneTargets = new Set();
      cloneOpen = false;
    } finally {
      cloning = false;
    }
  }
</script>

<div class="tab-page">
  <header class="tab-header">
    <h2>Configs</h2>
    <div class="actions">
      <Button variant="secondary" size="sm" on:click={() => addToast('Config export ready', 'success')}><Download size={16} />Export</Button>
      <Button variant="secondary" size="sm" on:click={() => addToast('Config import queued', 'success')}><FileUp size={16} />Import</Button>
    </div>
  </header>

  {#if $children.length}
    <label class="child-select">
      <span>Child</span>
      <select bind:value={selectedId} on:change={loadSelected}>
        {#each $children as child}
          <option value={child.id}>{child.name}</option>
        {/each}
      </select>
    </label>

    <label class="child-select">
      <span>Controller type</span>
      <select bind:value={controllerType} on:change={(event) => setControllerType(event.currentTarget.value)}>
        {#each controllerTypes as type}
          <option value={type.value}>{type.label}</option>
        {/each}
      </select>
    </label>

    <p class="controller-note">{selectedController.label} provides {selectedController.outputs} configurable outputs.</p>

    <div class="outputs">
      {#each outputs as output, index}
        <article class="output">
          <h3>Output {output.slot || index + 1}</h3>
          <label>
            <span>Type</span>
            <select bind:value={output.led_type}>
              {#each ledTypes as type}
                <option value={type}>{type}</option>
              {/each}
            </select>
          </label>
          <div class="control">
            <span>Count</span>
            <Input type="number" min="0" bind:value={output.count} ariaLabel={`LED count for output ${index + 1}`} />
          </div>
          <label>
            <span>Order</span>
            <select bind:value={output.color_order}>
              {#each colorOrders as order}
                <option value={order}>{order}</option>
              {/each}
            </select>
          </label>
        </article>
      {/each}
    </div>

    <div class="footer-actions">
      <Button disabled={saving} on:click={save}>{saving ? 'Saving' : 'Save to child'}</Button>
      <Button variant="secondary" on:click={() => cloneOpen = true}>Clone to...</Button>
    </div>
  {:else}
    <p class="empty">No children bound.</p>
  {/if}

  {#if cloneOpen}
    <div class="modal-backdrop">
      <section class="modal" aria-modal="true" role="dialog">
        <h3>Clone to...</h3>
        <div class="clone-list">
          {#each $children.filter((child) => child.id !== selectedId) as child}
            <button type="button" on:click={() => toggleClone(child.id)}>
              <span>{child.name}</span>
              <Toggle checked={cloneTargets.has(child.id)} label={`Select ${child.name}`} />
            </button>
          {/each}
        </div>
        <div class="modal-actions">
          <Button variant="ghost" on:click={() => cloneOpen = false}>Cancel</Button>
          <Button disabled={cloneTargets.size === 0 || cloning} on:click={confirmClone}>{cloning ? 'Cloning' : 'Confirm'}</Button>
        </div>
      </section>
    </div>
  {/if}
</div>

<style>
  .tab-page {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .tab-header,
  .footer-actions,
  .modal-actions {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  h2 {
    margin: 0;
    font-size: 22px;
    font-weight: 500;
  }
  .actions {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
    justify-content: flex-end;
  }
  .child-select,
  .output label,
  .output .control {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .child-select span,
  .output label span,
  .output .control span {
    color: var(--text-secondary);
    font-size: 12px;
    font-weight: 500;
    text-transform: uppercase;
  }
  .controller-note {
    margin: -8px 0 0;
    color: var(--text-secondary);
    font-size: 13px;
    line-height: 19px;
  }
  .outputs {
    display: grid;
    gap: 12px;
  }
  @media (min-width: 760px) {
    .outputs {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
  }
  .output {
    display: grid;
    gap: 12px;
    padding: 16px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
  }
  .output h3 {
    margin: 0;
    color: var(--text-primary);
    font-size: 15px;
    font-weight: 500;
  }
  .footer-actions {
    justify-content: flex-start;
    flex-wrap: wrap;
  }
  .modal-backdrop {
    position: fixed;
    inset: 0;
    z-index: 20;
    display: grid;
    place-items: end center;
    padding: 24px;
    background: rgba(0, 0, 0, 0.7);
  }
  .modal {
    width: min(100%, 420px);
    padding: 18px;
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    background: var(--bg-surface);
  }
  .modal h3 {
    margin: 0 0 12px;
    font-size: 18px;
    font-weight: 500;
  }
  .clone-list {
    display: flex;
    flex-direction: column;
    gap: 8px;
    margin-bottom: 18px;
  }
  .clone-list button {
    min-height: 48px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 12px;
    background: var(--bg-surface-2);
  }
</style>
