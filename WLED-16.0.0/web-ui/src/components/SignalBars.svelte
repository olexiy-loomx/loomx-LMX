<script>
  export let rssi = null;
  export let strength = null;

  $: level = strength ?? (
    rssi == null ? 0 :
    rssi >= -50 ? 4 :
    rssi >= -65 ? 3 :
    rssi >= -75 ? 2 :
    rssi >= -85 ? 1 : 0
  );
</script>

<span class="bars" aria-label={`${level} of 4 signal bars`}>
  {#each [1, 2, 3, 4] as bar}
    <span class:active={bar <= level} style={`height: ${6 + bar * 3}px`}></span>
  {/each}
</span>

<style>
  .bars {
    display: inline-flex;
    align-items: flex-end;
    gap: 2px;
    height: 18px;
  }
  .bars span {
    width: 3px;
    border-radius: 2px;
    background: var(--border-default);
  }
  .bars span.active {
    background: var(--accent);
  }
</style>
