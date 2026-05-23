import { writable } from 'svelte/store';

export const wsState = writable({
  connected: true,
  reconnecting: false,
  last_error: null,
});

export async function sendMessage(message) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 1500);
  const ackRequired = message?.type === 'child_config';
  try {
    const res = await fetch('/api/loomx/send', {
      method: 'POST',
      signal: controller.signal,
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(message),
    });
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    const result = await res.json();
    const accepted = !!result.ok || (!ackRequired && Number(result.sent || 0) === 0);
    const transportError = result.first_error
      ? `${result.transport || 'transport'} ${result.first_error}${result.first_error_ip ? ` @ ${result.first_error_ip}` : ''}`
      : null;
    wsState.set({
      connected: accepted,
      reconnecting: false,
      last_error: accepted ? null : transportError || 'No children acknowledged',
    });
    return {
      ...result,
      ok: accepted,
      acknowledged: !!result.ok,
      sent: Number(result.sent || 0),
      error: result.error || transportError || null,
    };
  } catch (error) {
    wsState.set({
      connected: false,
      reconnecting: false,
      last_error: error?.message || 'Send failed',
    });
    return { ok: false, error: error?.message || 'send_failed' };
  } finally {
    clearTimeout(timer);
  }
}
