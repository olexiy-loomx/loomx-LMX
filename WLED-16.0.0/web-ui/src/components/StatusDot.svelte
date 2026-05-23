<script>
  export let status = 'offline'; // online | offline | pending | error
  export let pulse = false;
</script>

<span class="dot {status}" class:pulse aria-hidden="true"></span>

<style>
  .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    display: inline-block;
    flex: 0 0 auto;
    background: #555;
  }
  .online { background: var(--success); }
  .offline { background: #555; }
  .pending { background: var(--warning); }
  .error { background: var(--danger); }
  .pulse { animation: dotPulse 1.4s ease-out infinite; }
  @keyframes dotPulse {
    0%, 100% { opacity: 0.45; }
    50% { opacity: 1; }
  }
  @media (prefers-reduced-motion: reduce) {
    .pulse { animation: none; }
  }
</style>
