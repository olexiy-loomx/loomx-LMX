import { get, writable } from 'svelte/store';
import { api } from '../lib/api.js';
import { addToast } from './toasts.js';
import { sendMessage } from './ws.js';

const DATA_KEY = 'loomx_app_data';
const DATA_VERSION = 6;
const DEFAULT_PRESET_BRIGHTNESS = 200;
const DEFAULT_EFFECT_SPEED = 128;
const DEFAULT_EFFECT_FREQUENCY = 128;
const DEFAULT_MASTER_BRIGHTNESS = 255;
const MASTER_BRIGHTNESS_FADE_MS = 800;
const CONTROLLER_OUTPUTS = {
  VX4: 4,
  VX8: 8,
};
const defaultLive = {
  running_sequence: null,
  current_step: 0,
  active_preset: null,
  master_power: true,
  master_brightness: DEFAULT_MASTER_BRIGHTNESS,
};

const defaultChildren = [];

const defaultPresets = [
  preset('p_all_red', 'All Red', ['#ff1919', '#ff1919', '#ff1919', '#ff1919']),
  preset('p_all_blue', 'All Blue', ['#2563ff', '#2563ff', '#2563ff', '#2563ff']),
  preset('p_mix_1', 'Mix 1', ['#c6f542', '#00d0ff', '#ff4fd8', '#ffffff']),
  preset('p_sunset', 'Sunset', ['#ff7a1a', '#ff3355', '#ef9f27', '#fff0b0']),
];

const defaultSequences = [
  sequence('s_entrance', 'Entrance', [
    step('solid', '#ffffff', 3000, 200),
    step('chase', '#c6f542', 4000, 400),
    step('fade', '#00d0ff', 5000, 700),
    step('solid', '#c6f542', 2000, 100),
    step('pulse', '#ffffff', 3000, 200),
  ]),
  sequence('s_countdown', 'Countdown', [
    step('solid', '#ffffff', 3000, 200),
    step('flash', '#ff1919', 2000, 0),
    step('solid', '#c6f542', 5000, 1000),
  ]),
  sequence('s_drop', 'Drop', [
    step('pulse', '#c6f542', 4000, 200),
    step('flash', '#ffffff', 2000, 0),
    step('rainbow', '#c6f542', 6000, 500),
    step('chase', '#ff4fd8', 5000, 300),
  ]),
  sequence('s_sunset', 'Sunset', [
    step('fade', '#ff7a1a', 6000, 1200),
    step('fade', '#ff3355', 6000, 1200),
  ]),
  sequence('s_strobe', 'Strobe', [
    step('flash', '#ffffff', 2500, 0),
    step('flash', '#c6f542', 2500, 0),
  ]),
];

export const children = writable([]);
export const availableChildren = writable([]);
export const presets = writable([]);
export const sequences = writable([]);
export const live = writable({ ...defaultLive });

let sequenceTimer = null;
let sequenceToken = 0;
let savedChildrenById = new Map();

function defaultOutput(slot) {
  return { slot, led_type: 'WS2812B', count: 60, color_order: 'GRB' };
}

function defaultOutputs(controllerType = 'VX4') {
  const count = CONTROLLER_OUTPUTS[controllerType] || CONTROLLER_OUTPUTS.VX4;
  return Array.from({ length: count }, (_, index) => defaultOutput(index + 1));
}

function normalizeOutputs(outputs = [], controllerType = 'VX4') {
  const count = CONTROLLER_OUTPUTS[controllerType] || CONTROLLER_OUTPUTS.VX4;
  return Array.from({ length: count }, (_, index) => ({
    ...defaultOutput(index + 1),
    ...(outputs[index] || {}),
    slot: index + 1,
  }));
}

function normalizeChild(child) {
  const controllerType = child.controller_type || child.controllerType || 'VX4';
  return {
    ...child,
    controller_type: controllerType,
    outputs: normalizeOutputs(child.outputs, controllerType),
  };
}

function step(effect, color, duration_ms, fade_ms) {
  return {
    id: `step_${cryptoId()}`,
    target: 'all',
    effect,
    color,
    duration_ms,
    fade_ms,
    speed: DEFAULT_EFFECT_SPEED,
    frequency: DEFAULT_EFFECT_FREQUENCY,
  };
}

function sequence(id, name, steps) {
  return { id, name, loop: false, steps };
}

function clampBrightness(value, fallback = DEFAULT_PRESET_BRIGHTNESS) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(0, Math.min(255, Math.round(parsed)));
}

function clampEffectControl(value, fallback = DEFAULT_EFFECT_SPEED) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(0, Math.min(255, Math.round(parsed)));
}

function normalizeLive(state = {}) {
  return {
    ...defaultLive,
    ...state,
    master_power: state.master_power ?? defaultLive.master_power,
    master_brightness: clampBrightness(state.master_brightness, defaultLive.master_brightness),
  };
}

function setLiveState(patch) {
  live.update((state) => normalizeLive({ ...state, ...patch }));
}

function normalizePresetState(state = {}, fallback = {}) {
  return {
    color: state.color || fallback.color || '#ffffff',
    effect: state.effect || fallback.effect || 'solid',
    brightness: clampBrightness(state.brightness ?? fallback.brightness),
    speed: clampEffectControl(state.speed ?? fallback.speed, DEFAULT_EFFECT_SPEED),
    frequency: clampEffectControl(state.frequency ?? fallback.frequency, DEFAULT_EFFECT_FREQUENCY),
  };
}

function normalizeOutputTargets(targets = {}) {
  if (!targets || typeof targets !== 'object' || Array.isArray(targets)) return {};
  const normalized = {};
  Object.entries(targets).forEach(([id, slots]) => {
    if (!id || !Array.isArray(slots)) return;
    const cleanSlots = [...new Set(slots
      .map((slot) => Number(slot))
      .filter((slot) => Number.isInteger(slot) && slot > 0 && slot <= CONTROLLER_OUTPUTS.VX8))]
      .sort((a, b) => a - b);
    if (cleanSlots.length) normalized[id] = cleanSlots;
  });
  return normalized;
}

function meaningfulPresetStateIds(states = {}) {
  if (!states || typeof states !== 'object' || Array.isArray(states)) return [];
  return Object.keys(states).filter((id) => id && !/^child_\d+$/i.test(id));
}

function preset(id, name, colors) {
  return {
    id,
    name,
    colors,
    target_ids: [],
    state: normalizePresetState({ color: colors[0], effect: 'solid', brightness: DEFAULT_PRESET_BRIGHTNESS }),
    states: Object.fromEntries(colors.map((color, index) => [
      defaultChildren[index]?.id || `child_${index}`,
      { color, effect: 'solid', brightness: DEFAULT_PRESET_BRIGHTNESS, speed: DEFAULT_EFFECT_SPEED, frequency: DEFAULT_EFFECT_FREQUENCY },
    ])),
  };
}

function normalizePreset(item) {
  const colors = Array.isArray(item?.colors) && item.colors.length ? item.colors : [];
  const rawStates = item?.states && typeof item.states === 'object' ? item.states : {};
  const outputTargets = normalizeOutputTargets(item?.output_targets);
  const outputTargetIds = Object.keys(outputTargets);
  const stateTargetIds = meaningfulPresetStateIds(rawStates);
  const targetIds = Array.isArray(item?.target_ids) && item.target_ids.length
    ? item.target_ids.filter(Boolean)
    : (outputTargetIds.length ? outputTargetIds : stateTargetIds);
  const firstState = Object.values(rawStates)[0] || {};
  const baseState = normalizePresetState(item?.state || firstState, {
    color: colors[0] || '#ffffff',
    effect: item?.effect || 'solid',
    brightness: item?.brightness ?? DEFAULT_PRESET_BRIGHTNESS,
    speed: item?.speed ?? DEFAULT_EFFECT_SPEED,
    frequency: item?.frequency ?? DEFAULT_EFFECT_FREQUENCY,
  });
  const states = {};

  Object.entries(rawStates).forEach(([id, state]) => {
    states[id] = normalizePresetState(state, baseState);
  });
  targetIds.forEach((id) => {
    states[id] = normalizePresetState(states[id] || baseState, baseState);
  });

  const normalizedColors = colors.length ? colors : Object.values(states).map((state) => state.color);
  return {
    ...item,
    id: item?.id || `p_${cryptoId()}`,
    name: item?.name || 'Untitled preset',
    colors: normalizedColors.length ? normalizedColors : [baseState.color],
    target_ids: targetIds,
    output_targets: outputTargets,
    state: baseState,
    states,
  };
}

function cryptoId() {
  if (globalThis.crypto?.getRandomValues) {
    const value = new Uint32Array(1);
    globalThis.crypto.getRandomValues(value);
    return value[0].toString(36).slice(0, 6);
  }
  return Math.random().toString(36).slice(2, 8);
}

function readData() {
  try {
    return JSON.parse(localStorage.getItem(DATA_KEY)) || null;
  } catch {
    return null;
  }
}

function writeData() {
  localStorage.setItem(DATA_KEY, JSON.stringify({
    version: DATA_VERSION,
    children: get(children),
    presets: get(presets),
    sequences: get(sequences),
    live: get(live),
  }));
}

export function loadAppData() {
  const saved = readData();
  const current = saved?.version === DATA_VERSION ? saved : null;
  savedChildrenById = new Map((current?.children || defaultChildren).map((child) => {
    const normalized = normalizeChild(child);
    return [normalized.id, normalized];
  }).filter(([id]) => id && !/^child_\d+$/i.test(id)));
  children.set([...savedChildrenById.values()].map((child) => normalizeChild({
    ...child,
    online: false,
    reconnecting: true,
    ip: '',
  })));
  presets.set((saved?.presets || defaultPresets).map(normalizePreset));
  sequences.set(saved?.sequences || defaultSequences);
  live.set(normalizeLive({
    ...(saved?.live || {}),
    running_sequence: null,
    current_step: 0,
    active_preset: saved?.live?.active_preset || null,
  }));
  if (!current || saved?.version !== DATA_VERSION) writeData();
  refreshConnectedChildren();
}

export function totalDuration(sequence) {
  return (sequence?.steps || []).reduce((sum, item) => sum + Number(item.duration_ms || 0), 0);
}

export function durationLabel(ms) {
  const seconds = ms / 1000;
  return `${seconds % 1 === 0 ? seconds.toFixed(0) : seconds.toFixed(1)}s`;
}

function cancelSequencePlayback() {
  sequenceToken += 1;
  if (sequenceTimer) clearTimeout(sequenceTimer);
  sequenceTimer = null;
}

export function stopSequencePlayback() {
  cancelSequencePlayback();
  setLiveState({ running_sequence: null, current_step: 0, active_preset: null });
  writeData();
}

function stepDuration(stepItem) {
  const duration = Number(stepItem?.duration_ms);
  return Number.isFinite(duration) && duration >= 0 ? duration : 0;
}

function playSequenceStep(sequenceItem, stepIndex, token) {
  if (token !== sequenceToken) return;
  const steps = sequenceItem?.steps || [];
  if (!steps.length) {
    setLiveState({ running_sequence: null, current_step: 0, active_preset: null });
    writeData();
    return;
  }

  const normalizedIndex = stepIndex % steps.length;
  const stepItem = steps[normalizedIndex];
  const liveState = get(live);
  setLiveState({
    running_sequence: sequenceItem.id,
    current_step: normalizedIndex + 1,
    active_preset: null,
    master_power: true,
  });
  sendMessage({
    type: 'sequence_step',
    sequence_id: sequenceItem.id,
    step_id: stepItem.id,
    step_index: normalizedIndex,
    target: stepItem.target,
    color: stepItem.color,
    effect: stepItem.effect,
    brightness: liveState.master_brightness,
    speed: clampEffectControl(stepItem.speed, DEFAULT_EFFECT_SPEED),
    frequency: clampEffectControl(stepItem.frequency, DEFAULT_EFFECT_FREQUENCY),
    duration_ms: Number(stepItem.duration_ms) || 0,
    fade_ms: Number(stepItem.fade_ms) || 0,
  }).then(reportSendResult);
  writeData();

  sequenceTimer = setTimeout(() => {
    if (token !== sequenceToken) return;
    if (normalizedIndex + 1 < steps.length) {
      playSequenceStep(sequenceItem, normalizedIndex + 1, token);
      return;
    }
    if (sequenceItem.loop) {
      playSequenceStep(sequenceItem, 0, token);
      return;
    }
    live.update((state) => state.running_sequence === sequenceItem.id
      ? { ...state, running_sequence: null, current_step: 0, active_preset: null }
      : state);
    sequenceTimer = null;
    writeData();
  }, stepDuration(stepItem));
}

export function triggerSequence(id) {
  const sequenceItem = get(sequences).find((item) => item.id === id);
  cancelSequencePlayback();
  if (!sequenceItem) {
    setLiveState({ running_sequence: null, current_step: 0, active_preset: null });
    writeData();
    return;
  }
  const token = sequenceToken;
  playSequenceStep(sequenceItem, 0, token);
}

export function applyPreset(id) {
  cancelSequencePlayback();
  const presetItem = get(presets).find((item) => item.id === id);
  if (!presetItem) return;
  const outputTargets = normalizeOutputTargets(presetItem.output_targets);
  const outputTargetIds = Object.keys(outputTargets);
  const stateTargetIds = meaningfulPresetStateIds(presetItem.states);
  const targetIds = Array.isArray(presetItem.target_ids) && presetItem.target_ids.length
    ? presetItem.target_ids.filter(Boolean)
    : (outputTargetIds.length ? outputTargetIds : stateTargetIds);
  setLiveState({ running_sequence: null, current_step: 0, active_preset: id, master_power: true });
  const message = { type: 'preset_apply', preset_id: id, preset: presetItem, cancel_sequence: true };
  if (targetIds.length) message.target_ids = targetIds;
  if (outputTargetIds.length) message.output_targets = outputTargets;
  sendMessage(message).then(reportSendResult);
  writeData();
}

async function resyncRecoveredChildren(recoveredIds = []) {
  if (!recoveredIds.length) return;
  const liveState = get(live);
  const targetSet = new Set(recoveredIds);

  if (liveState.active_preset) {
    const presetItem = get(presets).find((item) => item.id === liveState.active_preset);
    if (presetItem) {
      const outputTargets = normalizeOutputTargets(presetItem.output_targets);
      const presetTargets = Array.isArray(presetItem.target_ids) && presetItem.target_ids.length
        ? presetItem.target_ids.filter(Boolean)
        : meaningfulPresetStateIds(presetItem.states);
      const targetIds = presetTargets.filter((id) => targetSet.has(id));
      if (targetIds.length) {
        const message = { type: 'preset_apply', preset_id: presetItem.id, preset: presetItem, target_ids: targetIds };
        const scopedOutputs = {};
        targetIds.forEach((id) => {
          if (outputTargets[id]) scopedOutputs[id] = outputTargets[id];
        });
        if (Object.keys(scopedOutputs).length) message.output_targets = scopedOutputs;
        sendMessage(message).then(reportSendResult);
      }
    }
    return;
  }

  if (liveState.running_sequence) {
    const sequenceItem = get(sequences).find((item) => item.id === liveState.running_sequence);
    const stepItem = sequenceItem?.steps?.[Math.max(0, Number(liveState.current_step || 1) - 1)];
    if (stepItem) {
      sendMessage({
        type: 'sequence_step',
        sequence_id: sequenceItem.id,
        step_id: stepItem.id,
        target: stepItem.target,
        target_ids: recoveredIds,
        color: stepItem.color,
        effect: stepItem.effect,
        brightness: liveState.master_brightness,
        speed: clampEffectControl(stepItem.speed, DEFAULT_EFFECT_SPEED),
        frequency: clampEffectControl(stepItem.frequency, DEFAULT_EFFECT_FREQUENCY),
        duration_ms: Number(stepItem.duration_ms) || 0,
        fade_ms: Number(stepItem.fade_ms) || 0,
      }).then(reportSendResult);
    }
  }
}

export function stopAll() {
  cancelSequencePlayback();
  setLiveState({ running_sequence: null, current_step: 0, active_preset: null, master_power: false });
  sendMessage({ type: 'stop_all' }).then(reportSendResult);
  writeData();
}

export function setMasterPower(on) {
  const masterPower = !!on;
  const liveState = get(live);
  if (!masterPower) cancelSequencePlayback();
  setLiveState({
    master_power: masterPower,
    running_sequence: masterPower ? liveState.running_sequence : null,
    current_step: masterPower ? liveState.current_step : 0,
    active_preset: masterPower ? liveState.active_preset : null,
  });
  sendMessage({
    type: 'master_power',
    on: masterPower,
    brightness: liveState.master_brightness,
  }).then(reportSendResult);
  writeData();
}

export function setMasterBrightness(value) {
  const liveState = get(live);
  const brightness = clampBrightness(value, liveState.master_brightness);
  setLiveState({ master_brightness: brightness, master_power: true });
  sendMessage({
    type: 'master_brightness',
    brightness,
    on: true,
    fade_ms: MASTER_BRIGHTNESS_FADE_MS,
  }).then(reportSendResult);
  writeData();
}

export function saveSequence(sequence) {
  const normalized = {
    ...sequence,
    id: sequence.id || `s_${cryptoId()}`,
    name: sequence.name?.trim() || 'Untitled',
    steps: (sequence.steps?.length ? sequence.steps : [step('solid', '#ffffff', 3000, 200)]).map((item) => ({
      ...item,
      speed: clampEffectControl(item.speed, DEFAULT_EFFECT_SPEED),
      frequency: clampEffectControl(item.frequency, DEFAULT_EFFECT_FREQUENCY),
    })),
  };
  sequences.update((items) => {
    const exists = items.some((item) => item.id === normalized.id);
    return exists ? items.map((item) => item.id === normalized.id ? normalized : item) : [normalized, ...items];
  });
  writeData();
  addToast('Sequence saved', 'success');
  return normalized;
}

export function newSequence() {
  return sequence('', '', [step('solid', '#ffffff', 3000, 200)]);
}

export function duplicateSequence(id) {
  const source = get(sequences).find((item) => item.id === id);
  if (!source) return;
  const copy = {
    ...source,
    id: `s_${cryptoId()}`,
    name: `${source.name} Copy`,
    steps: source.steps.map((item) => ({ ...item, id: `step_${cryptoId()}` })),
  };
  sequences.update((items) => [copy, ...items]);
  writeData();
  addToast('Sequence duplicated', 'success');
}

export function deleteSequence(id) {
  if (get(live).running_sequence === id) cancelSequencePlayback();
  sequences.update((items) => items.filter((item) => item.id !== id));
  live.update((state) => state.running_sequence === id ? { ...state, running_sequence: null } : state);
  writeData();
  addToast('Sequence deleted', 'success');
}

export function savePreset(config) {
  const outputTargets = normalizeOutputTargets(config?.output_targets);
  const rawStates = config?.states && typeof config.states === 'object' && !Array.isArray(config.states)
    ? config.states
    : {};
  const stateIds = Object.keys(rawStates).filter(Boolean);
  const targetIds = Array.isArray(config?.target_ids) && config.target_ids.length
    ? config.target_ids.filter(Boolean)
    : (stateIds.length ? stateIds : Object.keys(outputTargets));
  const baseState = normalizePresetState(config?.state || Object.values(rawStates)[0] || config);
  const states = Object.fromEntries(targetIds.map((id) => [
    id,
    normalizePresetState(rawStates[id] || baseState, baseState),
  ]));
  const stateColors = Object.values(states).map((state) => state.color).filter(Boolean);
  const item = normalizePreset({
    id: config?.id || `p_${cryptoId()}`,
    name: config?.name?.trim() || `Preset ${get(presets).length + 1}`,
    colors: stateColors.length ? stateColors : [baseState.color],
    target_ids: targetIds,
    output_targets: outputTargets,
    state: baseState,
    speed: config?.speed,
    frequency: config?.frequency,
    states,
  });
  presets.update((items) => {
    const exists = items.some((presetItem) => presetItem.id === item.id);
    return exists ? items.map((presetItem) => presetItem.id === item.id ? item : presetItem) : [item, ...items];
  });
  writeData();
  addToast('Preset Saved', 'success');
  return item;
}

export function deletePreset(id) {
  presets.update((items) => items.filter((item) => item.id !== id));
  live.update((state) => state.active_preset === id ? { ...state, active_preset: null } : state);
  writeData();
  addToast('Preset deleted', 'success');
}

export function renamePreset(id, name) {
  presets.update((items) => items.map((item) => item.id === id ? { ...item, name } : item));
  writeData();
}

function reportSendResult(result) {
  if (result?.ok === false) addToast(result.error || result.first_error || 'No connected children acknowledged', 'error');
}

export async function refreshConnectedChildren() {
  try {
    const result = await api.getChildrenStatus();
    const previousChildren = get(children);
    const previousOnlineIds = new Set(previousChildren.filter((child) => child.online).map((child) => child.id));
    const seen = new Set();
    const firmwareChildren = (result.children || []).filter((node) => {
      if (!node?.id || seen.has(node.id)) return false;
      seen.add(node.id);
      return true;
    });
    const liveIds = new Set(firmwareChildren.map((node) => node.id));
    const recoveredIds = firmwareChildren
      .map((node) => node.id)
      .filter((id) => !previousOnlineIds.has(id));

    children.update((items) => {
      const byId = new Map(items.map((item) => [item.id, item]));
      const liveChildren = firmwareChildren.map((node, index) => {
        const existing = byId.get(node.id) || savedChildrenById.get(node.id) || {};
        const controllerType = existing.controller_type || node.controller_type || 'VX4';
        return normalizeChild({
          ...existing,
          ...node,
          id: node.id,
          mac: node.mac || node.id,
          name: existing.name || node.name || `Child ${index + 1}`,
          online: true,
          reconnecting: false,
          rssi: node.rssi ?? existing.rssi ?? -60,
          controller_type: controllerType,
          outputs: existing.outputs || defaultOutputs(controllerType),
        });
      });
      const offlineChildren = items
        .filter((item) => item?.id && !liveIds.has(item.id))
        .map((item) => normalizeChild({
          ...item,
          online: false,
          reconnecting: true,
          ip: '',
        }));
      savedChildrenById = new Map([
        ...savedChildrenById,
        ...liveChildren.map((child) => [child.id, child]),
        ...offlineChildren.map((child) => [child.id, child]),
      ]);
      return [...liveChildren, ...offlineChildren];
    });
    writeData();
    if (recoveredIds.length) setTimeout(() => resyncRecoveredChildren(recoveredIds), 250);
    return firmwareChildren;
  } catch {
    return [];
  }
}

export async function scanChildren() {
  const result = await api.scanChildren();
  await refreshConnectedChildren();
  const boundSsids = new Set(get(children).map((child) => child.ssid).filter(Boolean));
  const boundIds = new Set(get(children).flatMap((child) => [child.id, child.mac]).filter(Boolean).map((id) => String(id).toLowerCase()));
  const selfSsid = result.self_ssid;
  const available = (result.available || [])
    .filter((node) => node.ssid && node.ssid !== selfSsid && !boundSsids.has(node.ssid) && !boundIds.has(String(node.bssid || '').toLowerCase()));
  availableChildren.set(available);
  if (result.error) addToast('Only a Parent can scan for children', 'error');
  return available;
}

export function bindChildren(ssids) {
  addToast(`${ssids.length} selected. Waiting for ${ssids.length === 1 ? 'it' : 'them'} to connect to this Parent AP.`, 'success');
  refreshConnectedChildren();
}

export function identifyChild(id) {
  sendMessage({ type: 'identify', child_id: id }).then(reportSendResult);
  addToast('Identify signal sent', 'success');
}

export function unbindChild(id) {
  savedChildrenById.delete(id);
  children.update((items) => items.filter((item) => item.id !== id));
  writeData();
  addToast('Child unbound', 'success');
}

export async function updateChildConfig(id, config) {
  let savedConfig = null;
  children.update((items) => items.map((item) => {
    if (item.id !== id) return item;
    const controllerType = config.controller_type || item.controller_type || 'VX4';
    savedConfig = {
      ...item,
      controller_type: controllerType,
      outputs: normalizeOutputs(config.outputs, controllerType),
    };
    return savedConfig;
  }));
  writeData();
  if (!savedConfig) return { ok: false, error: 'child_not_found' };

  const result = await sendMessage({
    type: 'child_config',
    target_id: id,
    config: {
      controller_type: savedConfig.controller_type,
      outputs: savedConfig.outputs,
    },
  });
  if (result?.ok) addToast('Config saved to child', 'success');
  else addToast(result?.error || 'Child did not acknowledge config', 'error');
  await refreshConnectedChildren();
  return result;
}

export async function pushChildConfig(id) {
  const child = get(children).find((item) => item.id === id);
  if (!child) return { ok: false, error: 'child_not_found' };
  const result = await sendMessage({
    type: 'child_config',
    target_id: id,
    config: {
      controller_type: child.controller_type || 'VX4',
      outputs: normalizeOutputs(child.outputs, child.controller_type || 'VX4'),
    },
  });
  if (result?.ok) addToast('Config push complete', 'success');
  else addToast(result?.error || 'Child did not acknowledge config', 'error');
  await refreshConnectedChildren();
  return result;
}

export async function cloneChildConfig(sourceId, targetIds) {
  const source = get(children).find((item) => item.id === sourceId);
  if (!source) return;
  const sourceConfig = {
    controller_type: source.controller_type || 'VX4',
    outputs: normalizeOutputs(source.outputs, source.controller_type || 'VX4'),
  };
  children.update((items) => items.map((item) => targetIds.includes(item.id)
    ? {
      ...item,
      controller_type: sourceConfig.controller_type,
      outputs: sourceConfig.outputs,
    }
    : item));
  writeData();
  const result = await sendMessage({
    type: 'child_config',
    target_ids: targetIds,
    config: sourceConfig,
  });
  if (result?.ok) addToast(`Cloned config to ${result.sent || targetIds.length} ${targetIds.length === 1 ? 'child' : 'children'}`, 'success');
  else addToast(result?.error || 'No target children acknowledged clone', 'error');
  await refreshConnectedChildren();
  return result;
}
