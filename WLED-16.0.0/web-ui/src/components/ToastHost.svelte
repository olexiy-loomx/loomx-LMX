<script>
  import { removeToast, toasts } from '../stores/toasts.js';
</script>

<div class="toast-host" aria-live="polite" aria-atomic="false">
  {#each $toasts as toast (toast.id)}
    <button class="toast {toast.kind}" type="button" on:click={() => removeToast(toast.id)}>
      {toast.message}
    </button>
  {/each}
</div>

<style>
  .toast-host {
    position: fixed;
    left: 50%;
    bottom: 80px;
    transform: translateX(-50%);
    z-index: 180;
    width: min(calc(100vw - 48px), 420px);
    display: flex;
    flex-direction: column-reverse;
    gap: 8px;
    pointer-events: none;
  }
  .toast {
    min-height: 44px;
    width: 100%;
    padding: 10px 14px;
    border-radius: var(--radius-input);
    border: 0.5px solid var(--border-default);
    border-left: 3px solid var(--accent);
    background: var(--bg-surface-2);
    color: var(--text-primary);
    text-align: left;
    pointer-events: auto;
  }
  .toast.success { border-left-color: var(--success); }
  .toast.error { border-left-color: var(--danger); }
</style>
