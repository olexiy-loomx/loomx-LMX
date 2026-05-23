<script>
  import { Download, FileUp, MoreHorizontal, Plus } from 'lucide-svelte';
  import Button from '../../components/Button.svelte';
  import {
    deleteSequence,
    duplicateSequence,
    durationLabel,
    sequences,
    totalDuration,
  } from '../../stores/appData.js';
  import { addToast } from '../../stores/toasts.js';
  import { route } from '../../lib/router.js';

  let openMenu = null;

  function exportSequence(sequence) {
    console.debug('[loomx-ui] export sequence', sequence);
    addToast('Sequence export ready', 'success');
    openMenu = null;
  }
</script>

<div class="tab-page">
  <header class="tab-header">
    <h2>Sequences</h2>
    <div class="actions">
      <Button size="sm" on:click={() => route.navigate('/edit/sequences/new')}><Plus size={16} />New</Button>
      <Button variant="secondary" size="sm" on:click={() => addToast('Import queued', 'success')}><FileUp size={16} />Import</Button>
    </div>
  </header>

  {#if $sequences.length}
    <div class="rows">
      {#each $sequences as sequence (sequence.id)}
        <article class="row">
          <button class="row-main" type="button" on:click={() => route.navigate(`/edit/sequences/${sequence.id}`)}>
            <strong>{sequence.name}</strong>
            <span>{sequence.steps.length} steps · {durationLabel(totalDuration(sequence))}</span>
          </button>
          <button class="menu-btn" type="button" aria-label={`Actions for ${sequence.name}`} on:click={() => openMenu = openMenu === sequence.id ? null : sequence.id}>
            <MoreHorizontal size={20} />
          </button>
          {#if openMenu === sequence.id}
            <div class="menu">
              <button type="button" on:click={() => route.navigate(`/edit/sequences/${sequence.id}`)}>Edit</button>
              <button type="button" on:click={() => { duplicateSequence(sequence.id); openMenu = null; }}>Duplicate</button>
              <button type="button" on:click={() => exportSequence(sequence)}><Download size={15} />Export</button>
              <button class="danger" type="button" on:click={() => { deleteSequence(sequence.id); openMenu = null; }}>Delete</button>
            </div>
          {/if}
        </article>
      {/each}
    </div>
  {:else}
    <p class="empty">No sequences yet. Tap + New to build your first one.</p>
  {/if}
</div>

<style>
  .tab-page {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .tab-header {
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
  .rows {
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    overflow: visible;
    background: var(--bg-surface);
  }
  .row {
    position: relative;
    z-index: 1;
    min-height: 64px;
    display: grid;
    grid-template-columns: minmax(0, 1fr) 44px;
    border-bottom: 0.5px solid var(--border-default);
  }
  .row:last-child {
    border-bottom: 0;
  }
  .row:has(.menu) {
    z-index: 5;
  }
  .row-main {
    min-height: 64px;
    padding: 12px 14px;
    border: 0;
    border-radius: 0;
    background: transparent;
    text-align: left;
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    justify-content: center;
  }
  .row-main strong {
    color: var(--text-primary);
    font-weight: 500;
  }
  .row-main span {
    color: var(--text-secondary);
    font-size: 13px;
  }
  .menu-btn {
    min-height: 64px;
    padding: 0;
    border: 0;
    border-radius: 0;
    background: transparent;
  }
  .menu {
    position: absolute;
    right: 10px;
    top: 50px;
    z-index: 20;
    width: 160px;
    padding: 6px;
    border: 0.5px solid var(--border-default);
    border-radius: 12px;
    background: var(--bg-surface-2);
  }
  .menu button {
    width: 100%;
    min-height: 38px;
    border: 0;
    background: transparent;
    justify-content: flex-start;
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 0 10px;
    color: var(--text-primary);
  }
  .menu .danger {
    color: var(--danger);
  }
</style>
