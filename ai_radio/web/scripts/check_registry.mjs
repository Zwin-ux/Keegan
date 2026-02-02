const baseUrl = process.env.REGISTRY_URL || process.env.VITE_REGISTRY_URL || 'http://localhost:8090';
const timeoutMs = Number(process.env.REGISTRY_TIMEOUT_MS || 5000);

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const fetchWithTimeout = async (url, options = {}) => {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    clearTimeout(timeoutId);
  }
};

const check = async (path) => {
  const url = new URL(path, baseUrl);
  const res = await fetchWithTimeout(url.toString(), { headers: { 'Content-Type': 'application/json' } });
  if (!res.ok) {
    const body = await res.text();
    throw new Error(`HTTP ${res.status} ${url} ${body}`);
  }
  return res.json().catch(() => ({}));
};

try {
  const health = await check('/health');
  await sleep(200);
  const stations = await check('/api/stations');
  const rooms = await check('/api/rooms');
  console.log('[registry] ok', { baseUrl, health, stationCount: stations.stations?.length ?? 0 });
  console.log('[registry] rooms', { roomCount: rooms.rooms?.length ?? 0 });
  process.exit(0);
} catch (err) {
  console.error('[registry] failed', err?.message || err);
  process.exit(1);
}
