<script>
  import { Cpu } from 'lucide-svelte';
  import BrandMark from '../components/BrandMark.svelte';
  import Button from '../components/Button.svelte';
  import Card from '../components/Card.svelte';
  import { api } from '../lib/api.js';
  import { route } from '../lib/router.js';
  import { commitRole } from '../stores/role.js';
  import { addToast } from '../stores/toasts.js';

  const controllerTypes = [
    { value: 'VX4', title: 'VX4', body: 'Four outputs. Compact controller profile.' },
    { value: 'VX8', title: 'VX8', body: 'Eight outputs. Expanded controller profile.' },
  ];

  let selected = 'VX4';
  let saving = false;

  async function save() {
    saving = true;
    await api.setChildConfig({ controller_type: selected });
    await commitRole('child');
    saving = false;
    addToast('Child controller saved', 'success');
    route.navigate('/child');
  }
</script>

<main class="page-shell setup">
  <header><BrandMark size="md" /></header>

  <section class="intro">
    <h1>Select controller type</h1>
    <p>This sets the child output profile used by firmware.</p>
  </section>

  <section class="cards">
    {#each controllerTypes as type}
      <Card interactive selected={selected === type.value} on:click={() => selected = type.value}>
        <div class="row">
          <div class="icon" class:on={selected === type.value}><Cpu size={24} /></div>
          <div>
            <div class="title" class:on={selected === type.value}>{type.title}</div>
            <div class="body">{type.body}</div>
          </div>
        </div>
      </Card>
    {/each}
  </section>

  <footer>
    <Button full disabled={saving} on:click={save}>{saving ? 'Saving' : 'Continue'}</Button>
  </footer>
</main>

<style>
  .setup {
    max-width: 560px;
    display: flex;
    flex-direction: column;
    gap: 24px;
  }
  header {
    display: flex;
    justify-content: center;
    padding-bottom: 8px;
  }
  h1 {
    margin: 0;
    color: var(--text-primary);
    font-size: 22px;
    line-height: 28px;
    font-weight: 500;
  }
  .intro p {
    margin: 8px 0 0;
    color: var(--text-secondary);
  }
  .cards {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .row {
    display: flex;
    gap: 16px;
    align-items: flex-start;
  }
  .icon {
    background: var(--bg-surface-2);
    border-radius: 12px;
    padding: 10px;
    color: var(--text-secondary);
    display: inline-flex;
  }
  .icon.on {
    background: rgba(198, 245, 66, 0.15);
    color: var(--accent);
  }
  .title {
    margin-bottom: 4px;
    color: var(--text-primary);
    font-size: 16px;
    font-weight: 500;
  }
  .title.on {
    color: var(--accent);
  }
  .body {
    color: var(--text-secondary);
  }
  footer {
    margin-top: auto;
  }
</style>
