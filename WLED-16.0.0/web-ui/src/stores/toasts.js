import { writable } from 'svelte/store';

export const toasts = writable([]);
let nextToastId = 1;

export function addToast(message, kind = 'info') {
  const id = nextToastId++;
  toasts.update((items) => [...items.slice(-2), { id, message, kind }]);
  setTimeout(() => removeToast(id), kind === 'error' ? 4000 : 2000);
}

export function removeToast(id) {
  toasts.update((items) => items.filter((item) => item.id !== id));
}
