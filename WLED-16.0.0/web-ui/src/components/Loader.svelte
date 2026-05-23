<div class="loader" role="status" aria-label="Loading">
  <span class="dot"></span>
  <span class="dot"></span>
  <span class="dot"></span>
</div>

<style>
  .loader {
    display: inline-flex;
    gap: 8px;
    align-items: center;
  }
  .dot {
    width: 6px;
    height: 6px;
    border-radius: 999px;
    background: var(--accent);
    opacity: 0.3;
    animation: pulse 1.4s var(--ease-out) infinite;
  }
  .dot:nth-child(2) { animation-delay: 0.2s; }
  .dot:nth-child(3) { animation-delay: 0.4s; }

  @keyframes pulse {
    0%, 100% { opacity: 0.3; }
    30%      { opacity: 1.0; }
    60%      { opacity: 0.6; }
  }

  @media (prefers-reduced-motion: reduce) {
    .dot { animation: none; opacity: 0.8; }
  }
</style>
