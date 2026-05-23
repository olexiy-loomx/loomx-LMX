<script>
  import BrandMark from '../components/BrandMark.svelte';
  import Button from '../components/Button.svelte';
  import { route } from '../lib/router.js';
  import { commitRole } from '../stores/role.js';
  import { api } from '../lib/api.js';

  export let title = 'Placeholder';
  export let detail = '';

  async function resetRole() {
    await api.clearRole();
    route.navigate('/');
  }
</script>

<main class="page">
  <header><BrandMark size="md" /></header>
  <h1>{title}</h1>
  <p>{detail}</p>
  <div class="actions">
    <Button variant="secondary" on:click={() => route.navigate('/')}>Back to splash</Button>
    <Button variant="ghost" on:click={resetRole}>Reset role (dev)</Button>
  </div>
</main>

<style>
  .page {
    min-height: 100vh;
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 16px;
    background: var(--bg-base);
    max-width: 560px;
    margin: 0 auto;
  }
  header { display: flex; justify-content: center; padding-bottom: 8px; }
  h1 { font-size: 22px; font-weight: 500; color: var(--text-primary); margin: 0; }
  p  { font-size: 14px; color: var(--text-secondary); margin: 0; }
  .actions { display: flex; flex-direction: column; gap: 8px; margin-top: 16px; }
  .actions > :global(button) { width: 100%; }
</style>
