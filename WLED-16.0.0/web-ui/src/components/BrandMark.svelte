<script>
  export let size = 'md'; // 'sm' | 'md' | 'lg'
</script>

<span class="brand brand-{size}" aria-label="LOOMX">LOOMX</span>

<style>
  .brand {
    color: var(--accent);
    font-family: var(--font-sans);
    font-weight: 900;
    letter-spacing: 0;
    line-height: 1;
    user-select: none;
  }
  .brand-sm { font-size: 16px; }
  .brand-md { font-size: 22px; }
  .brand-lg { font-size: 40px; }
</style>
