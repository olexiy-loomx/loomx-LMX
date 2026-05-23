<script>
  import { onMount } from 'svelte';
  import { CheckSquare, MoreHorizontal, Search, Square } from 'lucide-svelte';
  import Button from '../../components/Button.svelte';
  import SignalBars from '../../components/SignalBars.svelte';
  import StatusDot from '../../components/StatusDot.svelte';
  import {
    availableChildren,
    bindChildren,
    children,
    identifyChild,
    pushChildConfig,
    refreshConnectedChildren,
    scanChildren,
    unbindChild,
  } from '../../stores/appData.js';
  import { addToast } from '../../stores/toasts.js';
  import { route } from '../../lib/router.js';

  let scanning = false;
  let selected = new Set();
  let openMenu = null;

  onMount(() => {
    refreshConnectedChildren();
    const timer = setInterval(refreshConnectedChildren, 3000);
    return () => clearInterval(timer);
  });

  async function scan() {
    scanning = true;
    selected = new Set();
    await scanChildren();
    scanning = false;
  }

  function toggle(ssid) {
    const next = new Set(selected);
    if (next.has(ssid)) next.delete(ssid);
    else next.add(ssid);
    selected = next;
  }

  function bindSelected() {
    bindChildren([...selected]);
    selected = new Set();
  }

  function renameChild(child) {
    addToast(`${child.name} renamed`, 'success');
    openMenu = null;
  }
</script>

<div class="tab-page">
  <header class="tab-header">
    <h2>Children</h2>
    <Button variant="secondary" size="sm" on:click={scan} disabled={scanning}>
      <Search size={16} />{scanning ? 'Scanning' : 'Scan for nodes'}
    </Button>
  </header>

  <section>
    <p class="eyebrow">Bound ({$children.length})</p>
    {#if $children.length}
      <div class="rows">
        {#each $children as child (child.id)}
          <article class="row" class:dim={!child.online}>
            <div class="child-main">
              <StatusDot status={child.online ? 'online' : 'offline'} pulse={child.reconnecting} />
              <div class="nameplate">
                <strong>{child.name}</strong>
                <span>{child.ip || 'No IP'} · {child.id}</span>
              </div>
              <SignalBars rssi={child.rssi} />
            </div>
            <button class="menu-btn" type="button" aria-label={`Actions for ${child.name}`} on:click={() => openMenu = openMenu === child.id ? null : child.id}>
              <MoreHorizontal size={20} />
            </button>
            {#if openMenu === child.id}
              <div class="menu">
                <button type="button" on:click={() => route.navigate('/edit/configs')}>Configure</button>
                <button type="button" on:click={() => { pushChildConfig(child.id); openMenu = null; }}>Push config</button>
                <button type="button" on:click={() => renameChild(child)}>Rename</button>
                <button type="button" on:click={() => { identifyChild(child.id); openMenu = null; }}>Identify</button>
                <button class="danger" type="button" on:click={() => { unbindChild(child.id); openMenu = null; }}>Unbind</button>
              </div>
            {/if}
          </article>
        {/each}
      </div>
    {:else}
      <p class="empty">No children bound.</p>
    {/if}
  </section>

  {#if $availableChildren.length}
    <section>
      <p class="eyebrow">Available to bind</p>
      <div class="rows">
        {#each $availableChildren as node (node.ssid)}
          <button class="discovery" type="button" on:click={() => toggle(node.ssid)}>
            {#if selected.has(node.ssid)}
              <CheckSquare size={20} />
            {:else}
              <Square size={20} />
            {/if}
            <span>{node.ssid}</span>
            <SignalBars rssi={node.rssi} />
          </button>
        {/each}
      </div>
      <Button full disabled={selected.size === 0} on:click={bindSelected}>Bind selected ({selected.size})</Button>
    </section>
  {/if}
</div>

<style>
  .tab-page {
    display: flex;
    flex-direction: column;
    gap: 24px;
  }
  .tab-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  h2 {
    margin: 0;
    font-size: 22px;
    font-weight: 500;
  }
  section {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .rows {
    border: 0.5px solid var(--border-default);
    border-radius: var(--radius-card);
    overflow: visible;
    background: var(--bg-surface);
  }
  .row {
    position: relative;
    z-index: 1;
    min-height: 62px;
    display: grid;
    grid-template-columns: minmax(0, 1fr) 44px;
    border-bottom: 0.5px solid var(--border-default);
  }
  .row:last-child {
    border-bottom: 0;
  }
  .row:has(.menu) {
    z-index: 5;
  }
  .row.dim .nameplate strong {
    color: var(--text-secondary);
  }
  .child-main {
    min-width: 0;
    display: grid;
    grid-template-columns: 8px minmax(0, 1fr) auto;
    align-items: center;
    gap: 12px;
    padding: 12px 14px;
  }
  .nameplate {
    min-width: 0;
    display: flex;
    flex-direction: column;
  }
  .nameplate strong {
    color: var(--text-primary);
    font-weight: 500;
  }
  .nameplate span,
  .discovery span {
    color: var(--text-secondary);
    font-family: var(--font-mono);
    font-size: 12px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .menu-btn {
    min-height: 62px;
    padding: 0;
    border: 0;
    border-radius: 0;
    background: transparent;
  }
  .menu {
    position: absolute;
    right: 10px;
    top: 48px;
    z-index: 20;
    width: 160px;
    padding: 6px;
    border: 0.5px solid var(--border-default);
    border-radius: 12px;
    background: var(--bg-surface-2);
  }
  .menu button {
    width: 100%;
    min-height: 38px;
    border: 0;
    background: transparent;
    justify-content: flex-start;
    display: flex;
    align-items: center;
    padding: 0 10px;
    color: var(--text-primary);
  }
  .menu .danger {
    color: var(--danger);
  }
  .discovery {
    min-height: 58px;
    display: grid;
    grid-template-columns: 24px minmax(0, 1fr) auto;
    align-items: center;
    gap: 12px;
    padding: 0 14px;
    border: 0;
    border-bottom: 0.5px solid var(--border-default);
    border-radius: 0;
    background: transparent;
    text-align: left;
  }
  .discovery:last-child {
    border-bottom: 0;
  }
  .discovery :global(svg) {
    color: var(--accent);
  }
</style>
