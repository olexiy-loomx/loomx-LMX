<script>
  import { onMount } from 'svelte';
  import { route } from './lib/router.js';
  import Splash from './routes/Splash.svelte';
  import RoleSelect from './routes/RoleSelect.svelte';
  import ParentSetup from './routes/ParentSetup.svelte';
  import ChildSetup from './routes/ChildSetup.svelte';
  import ViewMode from './routes/ViewMode.svelte';
  import EditMode from './routes/EditMode.svelte';
  import ChildStandby from './routes/ChildStandby.svelte';
  import GlobalPresetBuilder from './components/GlobalPresetBuilder.svelte';
  import ToastHost from './components/ToastHost.svelte';

  onMount(() => route.init());
  $: showPresetBuilder = $route === '/app' || $route.startsWith('/edit');
</script>

{#if $route === '/' || $route === ''}
  <Splash />
{:else if $route === '/setup/role'}
  <RoleSelect />
{:else if $route === '/setup/network'}
  <ParentSetup />
{:else if $route === '/setup/child'}
  <ChildSetup />
{:else if $route === '/app'}
  <ViewMode />
{:else if $route.startsWith('/edit')}
  <EditMode path={$route} />
{:else if $route === '/child'}
  <ChildStandby />
{:else}
  <ViewMode />
{/if}

{#if showPresetBuilder}
  <GlobalPresetBuilder />
{/if}

<ToastHost />
