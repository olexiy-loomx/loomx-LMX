<script>
  export let value = '';
  export let type = 'text';
  export let placeholder = '';
  export let ariaLabel = undefined;
  export let min = undefined;
  export let max = undefined;
  export let step = undefined;
  export let disabled = false;
</script>

{#if type === 'number'}
  <input
    type="number"
    bind:value
    {placeholder}
    {min}
    {max}
    {step}
    {disabled}
    aria-label={ariaLabel}
    on:input
    on:change
  />
{:else if type === 'password'}
  <input
    type="password"
    bind:value
    {placeholder}
    {disabled}
    aria-label={ariaLabel}
    on:input
    on:change
  />
{:else}
  <input
    type="text"
    bind:value
    {placeholder}
    {disabled}
    aria-label={ariaLabel}
    on:input
    on:change
  />
{/if}
