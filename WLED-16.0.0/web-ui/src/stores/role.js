import { writable } from 'svelte/store';
import { api } from '../lib/api.js';

// 'parent' | 'child' | null
export const role = writable(null);

export async function loadRole() {
  const { role: r } = await api.getRole();
  role.set(r);
  return r;
}

export async function commitRole(r) {
  await api.setRole(r);
  role.set(r);
}
