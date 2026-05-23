<script>
  export let variant = 'primary'; // 'primary' | 'secondary' | 'ghost' | 'danger'
  export let size = 'md';         // 'sm' | 'md' | 'lg'
  export let disabled = false;
  export let type = 'button';
  export let full = false;
  export let pressed = false;
  export let ariaLabel = undefined;
</script>

<button
  {type}
  {disabled}
  aria-label={ariaLabel}
  aria-pressed={pressed ? 'true' : undefined}
  class="btn btn-{variant} btn-{size}"
  class:full
  on:click
>
  <slot />
</button>

<style>
  .btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    min-height: 44px;
    padding: 0 20px;
    border-radius: var(--radius-button);
    font-family: var(--font-sans);
    font-size: 14px;
    font-weight: 500;
    border: 0.5px solid var(--border-default);
    background: transparent;
    color: var(--text-primary);
    cursor: pointer;
    transition: all var(--t-fast) var(--ease-out);
    white-space: nowrap;
  }
  .full { width: 100%; }
  .btn:disabled { opacity: 0.4; cursor: not-allowed; }
  .btn:focus-visible {
    outline: none;
    border-color: var(--border-active);
    box-shadow: 0 0 0 1px var(--border-active);
  }
  .btn-sm { min-height: 36px; font-size: 13px; padding: 0 14px; }
  .btn-lg { min-height: 52px; font-size: 15px; padding: 0 24px; }

  .btn-primary {
    background: var(--accent);
    color: var(--accent-ink);
    border-color: var(--accent);
  }
  .btn-primary:hover:not(:disabled) { background: var(--accent-hover); }

  .btn-secondary {
    background: transparent;
    color: var(--text-primary);
  }
  .btn-secondary:hover:not(:disabled) { background: var(--bg-surface-2); }

  .btn-ghost {
    background: transparent;
    border-color: transparent;
    color: var(--text-secondary);
  }
  .btn-ghost:hover:not(:disabled) { color: var(--text-primary); }

  .btn-danger {
    background: transparent;
    border-color: #5A3030;
    color: var(--danger);
  }
  .btn-danger:hover:not(:disabled) { background: rgba(240, 149, 149, 0.08); }
</style>
