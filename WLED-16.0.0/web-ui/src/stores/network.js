import { writable } from 'svelte/store';
import { api, generateNetworkConfig } from '../lib/api.js';

export const network = writable(generateNetworkConfig());

export async function loadNetwork() {
  const cfg = await api.getNetwork();
  network.set(cfg);
  return cfg;
}

export async function saveNetwork(cfg) {
  await api.setNetwork(cfg);
  network.set(cfg);
  return cfg;
}

export function newNetworkConfig() {
  const cfg = generateNetworkConfig();
  network.set(cfg);
  return cfg;
}
