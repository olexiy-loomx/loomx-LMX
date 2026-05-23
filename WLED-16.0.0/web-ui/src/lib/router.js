import { writable } from 'svelte/store';

// Hash-based SPA router. Spec budget was 30 lines; this is ~20.
function createRouter() {
  const { subscribe, set } = writable('/');

  const syncFromHash = () => {
    const h = window.location.hash.replace(/^#/, '');
    set(h || '/');
  };

  return {
    subscribe,
    navigate(path) {
      // Setting location.hash triggers the 'hashchange' listener which calls
      // syncFromHash for us — no need to call set() directly.
      window.location.hash = path;
    },
    init() {
      window.addEventListener('hashchange', syncFromHash);
      syncFromHash();
    },
  };
}

export const route = createRouter();
