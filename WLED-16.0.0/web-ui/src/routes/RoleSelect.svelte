<script>
  import { Radio, Lightbulb } from 'lucide-svelte';
  import BrandMark from '../components/BrandMark.svelte';
  import Card from '../components/Card.svelte';
  import Button from '../components/Button.svelte';
  import { route } from '../lib/router.js';
  import { commitRole } from '../stores/role.js';

  let selected = null; // 'parent' | 'child' | null

  async function onContinue() {
    if (!selected) return;
    if (selected === 'parent') {
      await commitRole(selected);
      route.navigate('/setup/network');
    } else {
      route.navigate('/setup/child');
    }
  }
</script>

<main class="page">
  <header><BrandMark size="md" /></header>

  <h1>Set up this controller</h1>
  <p class="sub">Pick a role to get started. You can change this later.</p>

  <Card
    interactive
    selected={selected === 'parent'}
    on:click={() => (selected = 'parent')}
  >
    <div class="row">
      <div class="icon" class:on={selected === 'parent'}><Radio size={24} /></div>
      <div>
        <div class="title" class:on={selected === 'parent'}>Parent</div>
        <div class="body">Hosts the network. Controls children. No LEDs.</div>
      </div>
    </div>
  </Card>

  <Card
    interactive
    selected={selected === 'child'}
    on:click={() => (selected = 'child')}
  >
    <div class="row">
      <div class="icon" class:on={selected === 'child'}><Lightbulb size={24} /></div>
      <div>
        <div class="title" class:on={selected === 'child'}>Child</div>
        <div class="body">Drives LEDs. Receives commands from a parent.</div>
      </div>
    </div>
  </Card>

  <div class="cta">
    <Button on:click={onContinue} disabled={!selected}>Continue</Button>
  </div>

  <div class="footer">
    Role: {selected ? (selected === 'parent' ? 'Parent' : 'Child') : 'not set'}
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
  }
  @media (min-width: 768px) { .page { padding: 32px; max-width: 560px; margin: 0 auto; } }
  header { display: flex; justify-content: center; padding-bottom: 8px; }
  h1 { font-size: 22px; font-weight: 500; color: var(--text-primary); margin: 0; }
  .sub { font-size: 14px; color: var(--text-secondary); margin: 0 0 8px; }

  .row { display: flex; gap: 16px; align-items: flex-start; }
  .icon {
    background: var(--bg-surface-2);
    border-radius: 12px;
    padding: 10px;
    color: var(--text-secondary);
    transition: all var(--t-fast) var(--ease-out);
    display: inline-flex;
  }
  .icon.on {
    background: rgba(198, 245, 66, 0.15);
    color: var(--accent);
  }
  .title { font-size: 16px; font-weight: 500; color: var(--text-primary); margin-bottom: 4px; }
  .title.on { color: var(--accent); }
  .body { font-size: 14px; color: var(--text-secondary); }

  .cta { display: flex; }
  .cta > :global(button) { width: 100%; }

  .footer {
    margin-top: auto;
    padding-top: 24px;
    font-size: 11px;
    letter-spacing: 0;
    color: var(--text-tertiary);
    text-align: center;
  }
</style>
