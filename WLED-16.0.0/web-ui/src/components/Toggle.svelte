<script>
  export let checked = false;
  export let disabled = false;
  export let label = 'Toggle';
</script>

<button
  type="button"
  class="toggle"
  class:on={checked}
  {disabled}
  aria-label={label}
  aria-pressed={checked}
  on:click={() => !disabled && (checked = !checked)}
>
  <span></span>
</button>

<style>
  .toggle {
    width: 32px;
    min-width: 32px;
    height: 18px;
    min-height: 18px;
    padding: 0;
    border: 0;
    border-radius: var(--radius-pill);
    background: var(--border-default);
    position: relative;
  }
  .toggle span {
    position: absolute;
    top: 2px;
    left: 2px;
    width: 14px;
    height: 14px;
    border-radius: 50%;
    background: var(--text-primary);
    transition: transform var(--t-base) var(--ease-out), background var(--t-base) var(--ease-out);
  }
  .toggle.on {
    background: var(--accent);
  }
  .toggle.on span {
    transform: translateX(14px);
    background: var(--accent-ink);
  }
  .toggle:focus-visible {
    outline: none;
    box-shadow: 0 0 0 2px var(--accent);
  }
</style>
