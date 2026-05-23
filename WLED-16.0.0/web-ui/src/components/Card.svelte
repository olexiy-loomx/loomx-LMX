<script>
  export let selected = false;
  export let interactive = false;
  export let padding = '20px';
</script>

{#if interactive}
  <div
    class="card interactive"
    class:selected
    style="padding: {padding};"
    on:click
    on:keydown={(e) => (e.key === 'Enter' || e.key === ' ') && e.currentTarget.click()}
    role="button"
    tabindex="0"
  >
    <slot />
  </div>
{:else}
  <div class="card" class:selected style="padding: {padding};">
    <slot />
  </div>
{/if}

<style>
  .card {
    background: var(--bg-surface);
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    transition: border-color var(--t-fast) var(--ease-out);
  }
  .card.selected { border: 1px solid var(--border-active); }
  .card.interactive { cursor: pointer; }
  .card.interactive:hover { border-color: var(--border-strong); }
  .card.interactive:focus-visible {
    outline: none;
    border-color: var(--border-active);
    box-shadow: 0 0 0 1px var(--border-active);
  }
</style>
