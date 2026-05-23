<script>
  import { onMount } from 'svelte';
  import BrandMark from '../components/BrandMark.svelte';
  import Loader from '../components/Loader.svelte';
  import { route } from '../lib/router.js';
  import { loadRole, role } from '../stores/role.js';

  const MIN_DURATION_MS = 1500;

  onMount(async () => {
    const startedAt = performance.now();
    const r = await loadRole();
    const elapsed = performance.now() - startedAt;
    const wait = Math.max(0, MIN_DURATION_MS - elapsed);
    setTimeout(() => {
      if (r === 'parent')     route.navigate('/app');
      else if (r === 'child') route.navigate('/child');
      else                    route.navigate('/setup/role');
    }, wait);
  });
</script>

<main class="splash">
  <div class="center">
    <BrandMark size="lg" />
    <div class="loader-wrap"><Loader /></div>
  </div>
  <div class="footer">LIGHTING SYSTEMS</div>
</main>

<style>
  .splash {
    position: fixed;
    inset: 0;
    background: var(--bg-base);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    overflow: hidden;
  }
  .center {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 24px;
  }
  .footer {
    position: absolute;
    bottom: 32px;
    font-size: 11px;
    letter-spacing: 0;
    color: var(--text-tertiary);
    text-transform: uppercase;
  }
</style>
