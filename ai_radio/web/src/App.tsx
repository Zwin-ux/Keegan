import { useState, useEffect, useCallback, useRef } from 'react';
import { HUD, type LiveState } from './components/HUD';
import { StationDirectory, type Station, type Room } from './components/StationDirectory';
import { engine, type Mood } from './lib/AudioEngine';

const REGISTRY_URL = import.meta.env.VITE_REGISTRY_URL || 'http://localhost:8090';
const REGISTRY_KEY = import.meta.env.VITE_REGISTRY_KEY || '';
const BRIDGE_KEY = import.meta.env.VITE_BRIDGE_KEY || '';
const BRIDGE_URL = import.meta.env.VITE_BRIDGE_URL || 'http://localhost:3000';
const REGISTRY_TIMEOUT_MS = 7000;

type BroadcastStatus = {
  broadcasting: boolean;
  sessionId?: string;
  startedAtMs?: number;
  updatedAtMs?: number;
  tokenExpiresAtMs?: number;
  streamUrl?: string;
};

type IngestInfo = {
  broadcasting: boolean;
  sessionId?: string;
  startedAtMs?: number;
  webrtcUrl?: string;
  rtmpUrl?: string;
  hlsUrl?: string;
  protocols?: string[];
};

type HlsHealth = {
  status: 'idle' | 'ok' | 'stalled' | 'error';
  bitrateKbps?: number;
  windowSeconds?: number;
  sequence?: number;
  lastUpdatedMs?: number;
};

const isHlsUrl = (url?: string | null) => !!url && url.toLowerCase().includes('.m3u8');

const formatDurationMs = (ms?: number) => {
  if (!ms || ms <= 0) return '0s';
  const totalSeconds = Math.floor(ms / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  if (hours > 0) return `${hours}h ${minutes}m`;
  if (minutes > 0) return `${minutes}m ${seconds}s`;
  return `${seconds}s`;
};

const parseAttributes = (raw: string) => {
  const attrs: Record<string, string> = {};
  raw.split(',').forEach((chunk) => {
    const [key, ...rest] = chunk.split('=');
    if (!key || rest.length === 0) return;
    const value = rest.join('=').trim().replace(/^"|"$/g, '');
    attrs[key.trim().toUpperCase()] = value;
  });
  return attrs;
};

const parseHlsPlaylist = (text: string) => {
  const lines = text.split(/\r?\n/);
  let maxBandwidth = 0;
  let avgBandwidth = 0;
  let targetDuration = 0;
  let mediaSequence: number | undefined;
  let segmentCount = 0;
  const programDates: number[] = [];

  lines.forEach((line) => {
    if (line.startsWith('#EXT-X-STREAM-INF:')) {
      const attrs = parseAttributes(line.slice('#EXT-X-STREAM-INF:'.length));
      const bw = Number(attrs.BANDWIDTH || 0);
      const avg = Number(attrs['AVERAGE-BANDWIDTH'] || 0);
      if (bw > maxBandwidth) maxBandwidth = bw;
      if (avg > avgBandwidth) avgBandwidth = avg;
    } else if (line.startsWith('#EXT-X-TARGETDURATION:')) {
      targetDuration = Number(line.split(':')[1]) || targetDuration;
    } else if (line.startsWith('#EXT-X-MEDIA-SEQUENCE:')) {
      const parsed = Number(line.split(':')[1]);
      if (Number.isFinite(parsed)) mediaSequence = parsed;
    } else if (line.startsWith('#EXTINF:')) {
      segmentCount += 1;
    } else if (line.startsWith('#EXT-X-PROGRAM-DATE-TIME:')) {
      const raw = line.split(':').slice(1).join(':').trim();
      const parsed = Date.parse(raw);
      if (!Number.isNaN(parsed)) programDates.push(parsed);
    }
  });

  let windowSeconds: number | undefined;
  if (programDates.length >= 2) {
    windowSeconds = Math.max(0, (programDates[programDates.length - 1] - programDates[0]) / 1000);
  } else if (segmentCount > 0 && targetDuration > 0) {
    windowSeconds = segmentCount * targetDuration;
  }

  const bitrate = avgBandwidth || maxBandwidth || 0;
  return {
    sequence: mediaSequence,
    bitrateKbps: bitrate ? Math.round(bitrate / 1000) : undefined,
    windowSeconds,
  };
};

type RegistryFailure = {
  kind: 'timeout' | 'network' | 'cors' | 'http';
  url: string;
  status?: number;
  body?: string;
  detail?: string;
};

class RegistryHttpError extends Error {
  status: number;
  body: string;

  constructor(status: number, body: string) {
    super(`HTTP ${status}`);
    this.status = status;
    this.body = body;
  }
}

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

const fetchWithTimeout = async (url: string, options: RequestInit, timeoutMs: number) => {
  const controller = new AbortController();
  const timeoutId = window.setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    window.clearTimeout(timeoutId);
  }
};

const classifyNetworkFailure = async (url: string, err: unknown): Promise<RegistryFailure> => {
  if (err instanceof DOMException && err.name === 'AbortError') {
    return { kind: 'timeout', url, detail: `Timed out after ${Math.round(REGISTRY_TIMEOUT_MS / 1000)}s` };
  }

  try {
    const controller = new AbortController();
    const timeoutId = window.setTimeout(() => controller.abort(), 2000);
    await fetch(url, { method: 'GET', mode: 'no-cors', signal: controller.signal });
    window.clearTimeout(timeoutId);
    return { kind: 'cors', url, detail: 'Browser blocked the response (CORS)' };
  } catch (probeErr) {
    if (probeErr instanceof DOMException && probeErr.name === 'AbortError') {
      return { kind: 'network', url, detail: 'Network timeout' };
    }
  }
  return { kind: 'network', url, detail: 'Connection refused or host unreachable' };
};

const registryErrorMessage = (failure: RegistryFailure) => {
  if (failure.kind === 'http') {
    return `Registry HTTP ${failure.status} at ${failure.url}. ${failure.body ? `Body: ${failure.body}` : ''}`.trim();
  }
  if (failure.kind === 'timeout') {
    return `Registry timeout at ${failure.url}.`;
  }
  if (failure.kind === 'cors') {
    return `Registry blocked by CORS at ${failure.url}.`;
  }
  return `Registry connection refused or unreachable at ${failure.url}.`;
};

const registryHealthLabel = (failure?: RegistryFailure | null) => {
  if (!failure) return '';
  if (failure.kind === 'http') return `HTTP ${failure.status}`;
  if (failure.kind === 'timeout') return 'timeout';
  if (failure.kind === 'cors') return 'CORS';
  return 'network';
};

function App() {
  const prefs = engine.getPreferences();
  const [mood, setMood] = useState<Mood>(prefs.mood);
  const [energy, setEnergy] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [volume, setVolume] = useState(prefs.volume);
  const [tunerEnergy, setTunerEnergy] = useState(prefs.energy);
  const [tunerAmbience, setTunerAmbience] = useState(prefs.ambience);
  const [error, setError] = useState<string | null>(null);
  const [liveState, setLiveState] = useState<LiveState | null>(null);
  const [wsStatus, setWsStatus] = useState<'offline' | 'connecting' | 'online'>('offline');
  const energyRaf = useRef<number>(0);

  const [stations, setStations] = useState<Station[]>([]);
  const [rooms, setRooms] = useState<Room[]>([]);
  const [registryLoading, setRegistryLoading] = useState(true);
  const [registryError, setRegistryError] = useState<string | null>(null);
  const [roomsLoading, setRoomsLoading] = useState(true);
  const [roomsError, setRoomsError] = useState<string | null>(null);
  const [registryHealth, setRegistryHealth] = useState<{
    status: 'checking' | 'ok' | 'error';
    failure?: RegistryFailure | null;
    version?: string;
    time?: string;
  }>({ status: 'checking' });
  const [region, setRegion] = useState('us-midwest');
  const [query, setQuery] = useState('');
  const [selectedStation, setSelectedStation] = useState<Station | null>(null);
  const [selectedRoom, setSelectedRoom] = useState<Room | null>(null);
  const [streamStatus, setStreamStatus] = useState<'idle' | 'playing' | 'paused' | 'error'>('idle');
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const listenerIdRef = useRef<string>('');
  const sessionIdRef = useRef<string>('');
  const [autoJoinGroup, setAutoJoinGroup] = useState<'auto' | 'manual'>('manual');
  const [autoJoinedRoom, setAutoJoinedRoom] = useState(false);

  const [broadcastStatus, setBroadcastStatus] = useState<BroadcastStatus | null>(null);
  const [broadcastToken, setBroadcastToken] = useState('');
  const [broadcastTokenExpiry, setBroadcastTokenExpiry] = useState<number | null>(null);
  const [ingestInfo, setIngestInfo] = useState<IngestInfo | null>(null);
  const [ingestError, setIngestError] = useState<string | null>(null);
  const [ingestBusy, setIngestBusy] = useState(false);

  const [hlsHealth, setHlsHealth] = useState<HlsHealth>({ status: 'idle' });
  const hlsSequenceRef = useRef<number | null>(null);
  const hlsStaleRef = useRef(0);

  // Listener identity for registry counts
  useEffect(() => {
    const stored = localStorage.getItem('keegan-listener-id');
    if (stored) {
      listenerIdRef.current = stored;
      return;
    }
    const generated = (typeof window !== 'undefined' && window.crypto && 'randomUUID' in window.crypto)
      ? window.crypto.randomUUID()
      : `listener_${Date.now()}_${Math.random().toString(16).slice(2)}`;
    listenerIdRef.current = generated;
    localStorage.setItem('keegan-listener-id', generated);
  }, []);

  useEffect(() => {
    const stored = localStorage.getItem('keegan-session-id');
    if (stored) {
      sessionIdRef.current = stored;
      return;
    }
    const generated = (typeof window !== 'undefined' && window.crypto && 'randomUUID' in window.crypto)
      ? window.crypto.randomUUID()
      : `session_${Date.now()}_${Math.random().toString(16).slice(2)}`;
    sessionIdRef.current = generated;
    localStorage.setItem('keegan-session-id', generated);
  }, []);

  useEffect(() => {
    const stored = localStorage.getItem('keegan-ab-autojoin');
    if (stored === 'auto' || stored === 'manual') {
      setAutoJoinGroup(stored);
      return;
    }
    const assigned = Math.random() < 0.5 ? 'auto' : 'manual';
    localStorage.setItem('keegan-ab-autojoin', assigned);
    setAutoJoinGroup(assigned);
  }, []);

  // Unlock audio context on first interaction
  useEffect(() => {
    const unlock = () => {
      engine.init().then(() => {
        window.removeEventListener('click', unlock);
        window.removeEventListener('keydown', unlock);
      });
    };
    window.addEventListener('click', unlock);
    window.addEventListener('keydown', unlock);
    return () => {
      window.removeEventListener('click', unlock);
      window.removeEventListener('keydown', unlock);
    };
  }, []);

  // Subscribe to engine events for error reporting
  useEffect(() => {
    return engine.on((event) => {
      if (event.type === 'all-stems-failed') {
        setError(`Audio failed to load for ${event.mood}. Check your connection.`);
      } else if (event.type === 'stem-loaded') {
        setError(null);
      }
    });
  }, []);

  // Real-time RMS energy from audio
  useEffect(() => {
    const tick = () => {
      if (playing) {
        setEnergy(engine.getRMSEnergy());
      }
      energyRaf.current = requestAnimationFrame(tick);
    };
    energyRaf.current = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(energyRaf.current);
  }, [playing]);

  // Keyboard shortcuts
  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      if ((e.target as HTMLElement).tagName === 'INPUT') return;

      if (e.code === 'Space') {
        e.preventDefault();
        handleToggle();
      } else if (e.key === '1') handleMoodChange('focus');
      else if (e.key === '2') handleMoodChange('rain');
      else if (e.key === '3') handleMoodChange('arcade');
      else if (e.key === '4') handleMoodChange('sleep');
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  });

  const handleToggle = () => {
    const isNowPlaying = engine.togglePlay();
    setPlaying(isNowPlaying);
    if (isNowPlaying) {
      engine.setMood(mood);
    } else {
      setEnergy(0);
    }
  };

  const handleMoodChange = (m: Mood) => {
    setMood(m);
    engine.setMood(m);
  };

  const handleVolumeChange = (v: number) => {
    setVolume(v);
    engine.setVolume(v);
  };

  const handleTunerEnergyChange = (v: number) => {
    setTunerEnergy(v);
    engine.setEnergy(v);
  };

  const handleTunerAmbienceChange = (v: number) => {
    setTunerAmbience(v);
    engine.setAmbience(v);
  };

  const getFrequencyData = useCallback(() => {
    try {
      return engine.getFrequencyData();
    } catch {
      return null;
    }
  }, []);

  // Connect to local EXE WebSocket for live engine state
  useEffect(() => {
    let ws: WebSocket | null = null;
    let stopped = false;
    let retryTimer: number | null = null;

    const connect = () => {
      if (stopped) return;
      setWsStatus('connecting');
      let wsUrl = 'ws://localhost:3001/events';
      try {
        const base = new URL(BRIDGE_URL);
        const wsProtocol = base.protocol === 'https:' ? 'wss:' : 'ws:';
        const basePort = base.port ? Number(base.port) : 3000;
        const wsPort = Number.isFinite(basePort) ? basePort + 1 : 3001;
        wsUrl = `${wsProtocol}//${base.hostname}:${wsPort}/events`;
      } catch {
        // fallback to localhost
      }
      if (BRIDGE_KEY) {
        wsUrl += `?token=${encodeURIComponent(BRIDGE_KEY)}`;
      }
      ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        setWsStatus('online');
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data) as LiveState;
          setLiveState(data);
        } catch {
          // ignore parse errors
        }
      };

      ws.onclose = () => {
        setWsStatus('offline');
        if (!stopped) {
          retryTimer = window.setTimeout(connect, 2000);
        }
      };

      ws.onerror = () => {
        ws?.close();
      };
    };

    connect();
    return () => {
      stopped = true;
      if (retryTimer) window.clearTimeout(retryTimer);
      ws?.close();
    };
  }, []);

  const fetchStations = useCallback(async () => {
    setRegistryLoading(true);
    setRegistryError(null);
    try {
      const url = new URL('/api/stations', REGISTRY_URL);
      if (region) url.searchParams.set('region', region);
      const headers: Record<string, string> = {};
      if (REGISTRY_KEY) headers['X-Api-Key'] = REGISTRY_KEY;
      const attemptFetch = async () => {
        const res = await fetchWithTimeout(url.toString(), { headers }, REGISTRY_TIMEOUT_MS);
        if (!res.ok) {
          const body = await res.text();
          console.warn(`[registry] ${res.status} ${url.toString()} ${body}`);
          throw new RegistryHttpError(res.status, body);
        }
        return res.json();
      };
      let data: any;
      try {
        data = await attemptFetch();
      } catch (err) {
        if (err instanceof RegistryHttpError) {
          throw { kind: 'http', url: url.toString(), status: err.status, body: err.body } as RegistryFailure;
        }
        const failure = await classifyNetworkFailure(url.toString(), err);
        if (failure.kind === 'timeout' || failure.kind === 'network') {
          await sleep(500);
          try {
            data = await attemptFetch();
          } catch (retryErr) {
            if (retryErr instanceof RegistryHttpError) {
              throw { kind: 'http', url: url.toString(), status: retryErr.status, body: retryErr.body } as RegistryFailure;
            }
            throw await classifyNetworkFailure(url.toString(), retryErr);
          }
        } else {
          throw failure;
        }
      }
      const nextStations: Station[] = data.stations || [];
      setStations(nextStations);
      setSelectedStation((prev) => {
        if (prev && nextStations.find((s) => s.id === prev.id)) return prev;
        return nextStations[0] || null;
      });
    } catch (err) {
      const fallback: RegistryFailure = { kind: 'network', url: REGISTRY_URL, detail: 'Unknown error' };
      const failure = (err && typeof err === 'object' && 'kind' in err)
        ? (err as RegistryFailure)
        : fallback;
      setRegistryError(registryErrorMessage(failure));
    } finally {
      setRegistryLoading(false);
    }
  }, [region, REGISTRY_URL, REGISTRY_KEY]);

  const fetchRooms = useCallback(async () => {
    setRoomsLoading(true);
    setRoomsError(null);
    try {
      const url = new URL('/api/rooms', REGISTRY_URL);
      if (region) url.searchParams.set('region', region);
      const headers: Record<string, string> = {};
      if (REGISTRY_KEY) headers['X-Api-Key'] = REGISTRY_KEY;
      const attemptFetch = async () => {
        const res = await fetchWithTimeout(url.toString(), { headers }, REGISTRY_TIMEOUT_MS);
        if (!res.ok) {
          const body = await res.text();
          console.warn(`[registry] ${res.status} ${url.toString()} ${body}`);
          throw new RegistryHttpError(res.status, body);
        }
        return res.json();
      };
      let data: any;
      try {
        data = await attemptFetch();
      } catch (err) {
        if (err instanceof RegistryHttpError) {
          throw { kind: 'http', url: url.toString(), status: err.status, body: err.body } as RegistryFailure;
        }
        const failure = await classifyNetworkFailure(url.toString(), err);
        if (failure.kind === 'timeout' || failure.kind === 'network') {
          await sleep(500);
          try {
            data = await attemptFetch();
          } catch (retryErr) {
            if (retryErr instanceof RegistryHttpError) {
              throw { kind: 'http', url: url.toString(), status: retryErr.status, body: retryErr.body } as RegistryFailure;
            }
            throw await classifyNetworkFailure(url.toString(), retryErr);
          }
        } else {
          throw failure;
        }
      }
      const nextRooms: Room[] = data.rooms || [];
      setRooms(nextRooms);
      setSelectedRoom((prev) => {
        if (prev && nextRooms.find((r) => r.roomId === prev.roomId)) return prev;
        return nextRooms[0] || null;
      });
    } catch (err) {
      const fallback: RegistryFailure = { kind: 'network', url: REGISTRY_URL, detail: 'Unknown error' };
      const failure = (err && typeof err === 'object' && 'kind' in err)
        ? (err as RegistryFailure)
        : fallback;
      setRoomsError(registryErrorMessage(failure));
    } finally {
      setRoomsLoading(false);
    }
  }, [region, REGISTRY_URL, REGISTRY_KEY]);

  const postTelemetry = useCallback(async (event: string, data?: Record<string, unknown>) => {
    try {
      const url = new URL('/api/telemetry', REGISTRY_URL);
      const headers: Record<string, string> = { 'Content-Type': 'application/json' };
      if (REGISTRY_KEY) headers['X-Api-Key'] = REGISTRY_KEY;
      await fetch(url.toString(), {
        method: 'POST',
        headers,
        body: JSON.stringify({
          event,
          ts: Date.now(),
          source: 'web',
          sessionId: sessionIdRef.current,
          region,
          ...(data ?? {}),
        }),
      });
    } catch {
      // best-effort only
    }
  }, [REGISTRY_URL, REGISTRY_KEY, region]);

  const pingRegistryHealth = useCallback(async () => {
    const url = new URL('/health', REGISTRY_URL);
    const headers: Record<string, string> = {};
    if (REGISTRY_KEY) headers['X-Api-Key'] = REGISTRY_KEY;
    try {
      const res = await fetchWithTimeout(url.toString(), { headers }, REGISTRY_TIMEOUT_MS);
      if (!res.ok) {
        const body = await res.text();
        console.warn(`[registry] ${res.status} ${url.toString()} ${body}`);
        throw new RegistryHttpError(res.status, body);
      }
      const data = await res.json();
      setRegistryHealth({
        status: 'ok',
        version: data.version ? String(data.version) : undefined,
        time: data.time ? String(data.time) : undefined,
        failure: null,
      });
      postTelemetry('registry_health_ok', { url: url.toString() });
    } catch (err) {
      if (err instanceof RegistryHttpError) {
        setRegistryHealth({
          status: 'error',
          failure: { kind: 'http', url: url.toString(), status: err.status, body: err.body },
        });
        postTelemetry('registry_health_failed', { url: url.toString(), status: err.status });
        return;
      }
      const failure = await classifyNetworkFailure(url.toString(), err);
      setRegistryHealth({ status: 'error', failure });
      postTelemetry('registry_health_failed', { url: url.toString(), reason: failure.kind });
    }
  }, [REGISTRY_URL, REGISTRY_KEY, postTelemetry]);

  useEffect(() => {
    fetchStations();
    fetchRooms();
    pingRegistryHealth();
    const interval = window.setInterval(() => {
      fetchStations();
      fetchRooms();
    }, 15000);
    return () => window.clearInterval(interval);
  }, [fetchStations, fetchRooms, pingRegistryHealth]);

  useEffect(() => {
    if (autoJoinGroup !== 'auto') return;
    if (autoJoinedRoom) return;
    if (selectedRoom || rooms.length === 0) return;
    const room = rooms[0];
    setSelectedRoom(room);
    setAutoJoinedRoom(true);
    postTelemetry('room_auto_join', {
      roomId: room.roomId,
      appKey: room.appKey,
      toneId: room.toneId,
      frequency: room.frequency,
      group: autoJoinGroup,
    });
  }, [autoJoinGroup, autoJoinedRoom, rooms, selectedRoom, postTelemetry]);

  const postListenerEvent = useCallback(async (stationId: string, action: 'join' | 'leave' | 'heartbeat') => {
    if (!stationId) return;
    try {
      const url = new URL(`/api/stations/${stationId}/listen`, REGISTRY_URL);
      const headers: Record<string, string> = { 'Content-Type': 'application/json' };
      if (REGISTRY_KEY) headers['X-Api-Key'] = REGISTRY_KEY;
      await fetch(url.toString(), {
        method: 'POST',
        headers,
        body: JSON.stringify({
          listenerId: listenerIdRef.current,
          action,
        }),
      });
    } catch {
      // best-effort only
    }
  }, [REGISTRY_URL, REGISTRY_KEY]);

  const postRoomPresence = useCallback(async (roomId: string, action: 'join' | 'leave' | 'heartbeat', room?: Room | null) => {
    if (!roomId) return;
    try {
      const url = new URL(`/api/rooms/${roomId}/presence`, REGISTRY_URL);
      const headers: Record<string, string> = { 'Content-Type': 'application/json' };
      if (REGISTRY_KEY) headers['X-Api-Key'] = REGISTRY_KEY;
      await fetch(url.toString(), {
        method: 'POST',
        headers,
        body: JSON.stringify({
          listenerId: listenerIdRef.current,
          action,
          region: room?.region || region,
          appKey: room?.appKey,
          toneId: room?.toneId,
          frequency: room?.frequency,
        }),
      });
    } catch {
      // best-effort only
    }
  }, [REGISTRY_URL, REGISTRY_KEY, region]);

  const bridgeHeaders = useCallback((extra?: Record<string, string>) => {
    const headers: Record<string, string> = { ...(extra ?? {}) };
    if (BRIDGE_KEY) headers['X-Api-Key'] = BRIDGE_KEY;
    return headers;
  }, [BRIDGE_KEY]);

  const copyText = useCallback(async (value?: string | null) => {
    if (!value) return;
    try {
      await navigator.clipboard.writeText(value);
    } catch {
      // ignore clipboard failures
    }
  }, []);

  const fetchBroadcastStatus = useCallback(async () => {
    try {
      const res = await fetch(`${BRIDGE_URL}/api/broadcast/status`, {
        headers: bridgeHeaders(),
      });
      if (!res.ok) return;
      const data = await res.json();
      setBroadcastStatus(data);
      if (data.tokenExpiresAtMs) {
        setBroadcastTokenExpiry(data.tokenExpiresAtMs);
      }
    } catch {
      // ignore when bridge is offline
    }
  }, [bridgeHeaders, BRIDGE_URL]);

  const fetchIngestInfo = useCallback(async (tokenOverride?: string) => {
    const token = tokenOverride || broadcastToken;
    if (!token) return;
    try {
      const res = await fetch(`${BRIDGE_URL}/api/broadcast/ingest`, {
        headers: bridgeHeaders({ 'X-Broadcast-Token': token }),
      });
      if (!res.ok) {
        setIngestError('Invalid or expired broadcast token.');
        return;
      }
      const data = await res.json();
      setIngestInfo(data);
      setIngestError(null);
    } catch {
      setIngestError('Unable to reach local bridge for ingest.');
    }
  }, [bridgeHeaders, broadcastToken, BRIDGE_URL]);

  const requestBroadcastToken = useCallback(async () => {
    setIngestBusy(true);
    setIngestError(null);
    try {
      const res = await fetch(`${BRIDGE_URL}/api/broadcast/token`, {
        method: 'POST',
        headers: bridgeHeaders(),
      });
      if (!res.ok) {
        setIngestError('Broadcast token request rejected.');
        return;
      }
      const data = await res.json();
      setBroadcastToken(data.token || '');
      setBroadcastTokenExpiry(data.expiresAtMs || null);
      if (data.token) {
        await fetchIngestInfo(data.token);
      }
    } catch {
      setIngestError('Unable to reach local bridge for tokens.');
    } finally {
      setIngestBusy(false);
    }
  }, [bridgeHeaders, fetchIngestInfo, BRIDGE_URL]);

  const startBroadcast = useCallback(async () => {
    if (!broadcastToken) {
      setIngestError('Generate a token before starting broadcast.');
      return;
    }
    setIngestBusy(true);
    setIngestError(null);
    try {
      const res = await fetch(`${BRIDGE_URL}/api/broadcast/start`, {
        method: 'POST',
        headers: bridgeHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify({ token: broadcastToken }),
      });
      if (!res.ok) {
        setIngestError('Broadcast start rejected.');
        return;
      }
      const data = await res.json();
      setBroadcastStatus(data);
      await fetchBroadcastStatus();
      await fetchIngestInfo(broadcastToken);
    } catch {
      setIngestError('Unable to start broadcast.');
    } finally {
      setIngestBusy(false);
    }
  }, [bridgeHeaders, broadcastToken, fetchBroadcastStatus, fetchIngestInfo, BRIDGE_URL]);

  const stopBroadcast = useCallback(async () => {
    if (!broadcastToken) {
      setIngestError('Provide a token to stop broadcast.');
      return;
    }
    setIngestBusy(true);
    setIngestError(null);
    try {
      const res = await fetch(`${BRIDGE_URL}/api/broadcast/stop`, {
        method: 'POST',
        headers: bridgeHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify({ token: broadcastToken }),
      });
      if (!res.ok) {
        setIngestError('Broadcast stop rejected.');
        return;
      }
      const data = await res.json();
      setBroadcastStatus((prev) => ({ ...(prev ?? {}), ...data }));
      await fetchBroadcastStatus();
    } catch {
      setIngestError('Unable to stop broadcast.');
    } finally {
      setIngestBusy(false);
    }
  }, [bridgeHeaders, broadcastToken, fetchBroadcastStatus, BRIDGE_URL]);

  useEffect(() => {
    fetchBroadcastStatus();
    const interval = window.setInterval(fetchBroadcastStatus, 5000);
    return () => window.clearInterval(interval);
  }, [fetchBroadcastStatus]);

  useEffect(() => {
    if (broadcastToken) {
      fetchIngestInfo(broadcastToken);
    }
  }, [broadcastToken, fetchIngestInfo]);

  useEffect(() => {
    if (!selectedStation?.id || !selectedStation.streamUrl || streamStatus !== 'playing') return;
    postListenerEvent(selectedStation.id, 'join');
    const interval = window.setInterval(() => {
      postListenerEvent(selectedStation.id, 'heartbeat');
    }, 15000);
    return () => {
      window.clearInterval(interval);
      postListenerEvent(selectedStation.id, 'leave');
    };
  }, [selectedStation?.id, selectedStation?.streamUrl, streamStatus, postListenerEvent]);

  useEffect(() => {
    if (!selectedStation?.id) return;
    postTelemetry('station_selected', {
      stationId: selectedStation.id,
      streamUrl: selectedStation.streamUrl,
    });
  }, [selectedStation?.id, selectedStation?.streamUrl, postTelemetry]);

  useEffect(() => {
    if (!selectedRoom?.roomId) return;
    postRoomPresence(selectedRoom.roomId, 'join', selectedRoom);
    const interval = window.setInterval(() => {
      postRoomPresence(selectedRoom.roomId, 'heartbeat', selectedRoom);
    }, 15000);
    return () => {
      window.clearInterval(interval);
      postRoomPresence(selectedRoom.roomId, 'leave', selectedRoom);
    };
  }, [selectedRoom?.roomId, postRoomPresence]);

  useEffect(() => {
    if (!selectedRoom?.roomId) return;
    postTelemetry('room_selected', {
      roomId: selectedRoom.roomId,
      appKey: selectedRoom.appKey,
      toneId: selectedRoom.toneId,
      frequency: selectedRoom.frequency,
    });
  }, [selectedRoom?.roomId, selectedRoom?.appKey, selectedRoom?.toneId, selectedRoom?.frequency, postTelemetry]);

  useEffect(() => {
    if (!audioRef.current) return;
    if (!selectedStation?.streamUrl) {
      audioRef.current.pause();
      audioRef.current.src = '';
      setStreamStatus('idle');
      return;
    }
    audioRef.current.src = selectedStation.streamUrl;
    audioRef.current
      .play()
      .then(() => setStreamStatus('playing'))
      .catch(() => setStreamStatus('error'));
  }, [selectedStation?.streamUrl]);

  useEffect(() => {
    const streamUrl = selectedStation?.streamUrl;
    if (!isHlsUrl(streamUrl)) {
      setHlsHealth({ status: 'idle' });
      hlsSequenceRef.current = null;
      hlsStaleRef.current = 0;
      return;
    }
    let cancelled = false;
    const poll = async () => {
      try {
        const cacheUrl = new URL(streamUrl!);
        cacheUrl.searchParams.set('t', Date.now().toString());
        const res = await fetch(cacheUrl.toString(), { cache: 'no-store' });
        if (!res.ok) throw new Error('playlist fetch failed');
        const text = await res.text();
        const parsed = parseHlsPlaylist(text);
        const now = Date.now();
        let status: HlsHealth['status'] = 'ok';
        if (typeof parsed.sequence === 'number') {
          if (hlsSequenceRef.current === parsed.sequence) {
            hlsStaleRef.current += 1;
          } else {
            hlsStaleRef.current = 0;
          }
          hlsSequenceRef.current = parsed.sequence;
          if (hlsStaleRef.current >= 3) status = 'stalled';
        }
        if (status === 'stalled' && audioRef.current && streamStatus === 'playing') {
          const reloadUrl = new URL(streamUrl!);
          reloadUrl.searchParams.set('t', now.toString());
          audioRef.current.src = reloadUrl.toString();
          audioRef.current.play().catch(() => setStreamStatus('error'));
        }
        if (!cancelled) {
          setHlsHealth({
            status,
            bitrateKbps: parsed.bitrateKbps,
            windowSeconds: parsed.windowSeconds,
            sequence: parsed.sequence,
            lastUpdatedMs: now,
          });
        }
      } catch {
        if (!cancelled) {
          setHlsHealth({ status: 'error', lastUpdatedMs: Date.now() });
        }
      }
    };
    poll();
    const interval = window.setInterval(poll, 12000);
    return () => {
      cancelled = true;
      window.clearInterval(interval);
    };
  }, [selectedStation?.streamUrl, streamStatus]);

  const playingStation = selectedStation?.streamUrl && streamStatus === 'playing';
  const broadcastUptime = broadcastStatus?.broadcasting && broadcastStatus.startedAtMs
    ? formatDurationMs(Date.now() - broadcastStatus.startedAtMs)
    : '0s';
  const tokenRemaining = broadcastTokenExpiry ? formatDurationMs(broadcastTokenExpiry - Date.now()) : null;
  const hlsWindow = typeof hlsHealth.windowSeconds === 'number'
    ? formatDurationMs(hlsHealth.windowSeconds * 1000)
    : null;

  return (
    <div className="relative h-screen w-screen overflow-hidden bg-[#05070d] text-white">
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -top-40 -right-20 h-[420px] w-[420px] rounded-full bg-sky-500/20 blur-[140px]" />
        <div className="absolute -bottom-48 left-0 h-[520px] w-[520px] rounded-full bg-amber-400/20 blur-[160px]" />
        <div className="absolute inset-0 opacity-[0.06] bg-[radial-gradient(circle_at_20%_20%,#ffffff,transparent_40%)]" />
      </div>

      {error && (
        <div className="absolute top-4 left-1/2 z-30 -translate-x-1/2 rounded-lg border border-red-500/40 bg-red-900/80 px-4 py-2 text-xs text-red-200 backdrop-blur-sm">
          {error}
        </div>
      )}

      <div className="relative z-10 flex h-full flex-col">
        <header className="flex flex-wrap items-center justify-between gap-4 px-6 py-6">
          <div>
            <div className="text-xs uppercase tracking-[0.3em] text-white/40">Keegan</div>
            <h1 className="mt-2 text-3xl font-semibold">Radioverse Console</h1>
          </div>
          <div className="text-xs uppercase tracking-[0.2em] text-white/50">
            Registry: <a className="text-white/70 hover:text-white" href={REGISTRY_URL} target="_blank" rel="noreferrer">{REGISTRY_URL}</a>
            <span className="ml-2 inline-flex items-center gap-1 text-[10px]">
              <span
                className={`h-2 w-2 rounded-full ${
                  registryHealth.status === 'ok'
                    ? 'bg-emerald-400'
                    : registryHealth.status === 'checking'
                      ? 'bg-amber-300 animate-pulse'
                      : 'bg-red-400'
                }`}
              />
              {registryHealth.status === 'ok' ? 'online' : registryHealth.status === 'checking' ? 'checking' : registryHealthLabel(registryHealth.failure)}
            </span>
            {registryHealth.status === 'ok' && registryHealth.version && (
              <span className="ml-2 text-[10px] text-white/40">v{registryHealth.version}</span>
            )}
            {registryHealth.status === 'error' && registryHealth.failure && (
              <span className="ml-2 text-[10px] text-red-200">
                {registryErrorMessage(registryHealth.failure)}
              </span>
            )}
          </div>
        </header>

        <main className="grid flex-1 gap-6 px-6 pb-6 lg:grid-cols-[1.15fr_0.85fr]">
          <section className="rounded-[28px] border border-white/10 bg-white/5 p-6 backdrop-blur">
            <StationDirectory
              stations={stations}
              rooms={rooms}
              loading={registryLoading}
              error={registryError}
              roomsLoading={roomsLoading}
              roomsError={roomsError}
              region={region}
              onRegionChange={setRegion}
              query={query}
              onQueryChange={setQuery}
              onSelect={setSelectedStation}
              onSelectRoom={setSelectedRoom}
              selectedId={selectedStation?.id ?? null}
              selectedRoomId={selectedRoom?.roomId ?? null}
            />
          </section>

          <section className="flex flex-col gap-6">
            <div className="rounded-[28px] border border-white/10 bg-white/5 p-6 backdrop-blur">
              <div className="flex items-center justify-between">
                <div>
                  <div className="text-xs uppercase tracking-[0.25em] text-white/40">Local Engine</div>
                  <div className="mt-2 text-xl font-semibold">Keegan Vibe Core</div>
                </div>
                <div className="text-xs uppercase tracking-[0.2em] text-white/50">
                  WS {wsStatus}
                </div>
              </div>

              <div className="mt-6 flex flex-col items-center gap-4">
                <HUD
                  mood={mood}
                  energy={energy}
                  playing={playing}
                  onToggle={handleToggle}
                  volume={volume}
                  onVolumeChange={handleVolumeChange}
                  tunerEnergy={tunerEnergy}
                  onTunerEnergyChange={handleTunerEnergyChange}
                  tunerAmbience={tunerAmbience}
                  onTunerAmbienceChange={handleTunerAmbienceChange}
                  getFrequencyData={getFrequencyData}
                  liveState={liveState}
                  wsStatus={wsStatus}
                />
              </div>

              <div className="mt-5 flex flex-wrap gap-2">
                {(['focus', 'rain', 'arcade', 'sleep'] as Mood[]).map((m, i) => (
                  <button
                    key={m}
                    onClick={() => handleMoodChange(m)}
                    aria-label={`Switch to ${m} mood`}
                    aria-pressed={mood === m}
                    className={`text-[10px] uppercase font-mono px-3 py-2 border rounded transition-all
                      ${mood === m ? 'bg-white text-black border-white' : 'text-zinc-500 border-zinc-800 hover:border-zinc-600'}
                    `}
                  >
                    <span className="text-zinc-700 mr-1">{i + 1}</span>{m}
                  </button>
                ))}
              </div>
            </div>

            <div className="rounded-[28px] border border-white/10 bg-white/5 p-6 backdrop-blur">
              <div className="flex items-center justify-between">
                <div>
                  <div className="text-xs uppercase tracking-[0.25em] text-white/40">Broadcast</div>
                  <div className="mt-2 text-xl font-semibold">Ingest Control</div>
                </div>
                <div className={`text-xs uppercase tracking-[0.2em] ${broadcastStatus?.broadcasting ? 'text-emerald-300' : 'text-white/40'}`}>
                  {broadcastStatus?.broadcasting ? 'live' : 'idle'}
                </div>
              </div>

              <div className="mt-4 grid gap-4">
                <div className="rounded-2xl border border-white/10 bg-black/40 p-4">
                  <div className="text-xs uppercase tracking-[0.2em] text-white/40">Session</div>
                  <div className="mt-2 text-sm text-white/70">
                    {broadcastStatus?.broadcasting ? 'Broadcasting' : 'Not broadcasting'}
                  </div>
                  <div className="mt-2 text-xs text-white/50">
                    Session ID: <span className="font-mono text-white/80">{broadcastStatus?.sessionId || '--'}</span>
                  </div>
                  <div className="text-xs text-white/50">Uptime: <span className="text-white/80">{broadcastUptime}</span></div>
                  {broadcastStatus?.streamUrl && (
                    <div className="mt-2 text-xs text-white/50">
                      Stream URL: <span className="font-mono text-white/80 break-all">{broadcastStatus.streamUrl}</span>
                    </div>
                  )}
                </div>

                <div className="rounded-2xl border border-white/10 bg-black/40 p-4">
                  <div className="text-xs uppercase tracking-[0.2em] text-white/40">Token</div>
                  <div className="mt-2 text-xs font-mono break-all text-white/80">
                    {broadcastToken || 'Generate a token to unlock ingest.'}
                  </div>
                  <div className="mt-2 text-xs text-white/50">
                    {broadcastTokenExpiry ? `Expires in ${tokenRemaining}` : 'No token issued yet.'}
                  </div>
                  <div className="mt-3 flex flex-wrap gap-2">
                    <button
                      onClick={requestBroadcastToken}
                      disabled={ingestBusy}
                      className="rounded-full border border-white/20 px-3 py-2 text-[11px] uppercase tracking-[0.2em] text-white/70 transition hover:border-white/60 disabled:opacity-40"
                    >
                      Generate Token
                    </button>
                    <button
                      onClick={startBroadcast}
                      disabled={ingestBusy || !broadcastToken}
                      className="rounded-full border border-emerald-300/40 px-3 py-2 text-[11px] uppercase tracking-[0.2em] text-emerald-200 transition hover:border-emerald-300 disabled:opacity-40"
                    >
                      Start
                    </button>
                    <button
                      onClick={stopBroadcast}
                      disabled={ingestBusy || !broadcastStatus?.broadcasting}
                      className="rounded-full border border-red-300/40 px-3 py-2 text-[11px] uppercase tracking-[0.2em] text-red-200 transition hover:border-red-300 disabled:opacity-40"
                    >
                      Stop
                    </button>
                    <button
                      onClick={() => copyText(broadcastToken)}
                      disabled={!broadcastToken}
                      className="rounded-full border border-white/10 px-3 py-2 text-[11px] uppercase tracking-[0.2em] text-white/50 transition hover:border-white/40 disabled:opacity-40"
                    >
                      Copy
                    </button>
                  </div>
                </div>

                <div className="rounded-2xl border border-white/10 bg-black/40 p-4">
                  <div className="text-xs uppercase tracking-[0.2em] text-white/40">Ingest URLs</div>
                  {(['rtmpUrl', 'hlsUrl', 'webrtcUrl'] as const).map((key) => (
                    <div key={key} className="mt-3">
                      <div className="flex items-center justify-between text-[11px] uppercase tracking-[0.2em] text-white/40">
                        <span>{key.replace('Url', '').toUpperCase()}</span>
                        <button
                          onClick={() => copyText(ingestInfo?.[key])}
                          disabled={!ingestInfo?.[key]}
                          className="rounded-full border border-white/10 px-2 py-1 text-[9px] text-white/60 transition hover:border-white/40 disabled:opacity-40"
                        >
                          Copy
                        </button>
                      </div>
                      <div className="mt-1 text-xs font-mono break-all text-white/80">
                        {ingestInfo?.[key] || '--'}
                      </div>
                    </div>
                  ))}
                </div>

                {ingestError && (
                  <div className="text-xs uppercase tracking-[0.2em] text-red-300">
                    {ingestError}
                  </div>
                )}
              </div>
            </div>

            <div className="rounded-[28px] border border-white/10 bg-white/5 p-6 backdrop-blur">
              <div className="flex items-center justify-between">
                <div>
                  <div className="text-xs uppercase tracking-[0.25em] text-white/40">Station Output</div>
                  <div className="mt-2 text-xl font-semibold">Live Stream Monitor</div>
                </div>
                <div className={`text-xs uppercase tracking-[0.2em] ${playingStation ? 'text-emerald-300' : 'text-white/40'}`}>
                  {playingStation ? 'playing' : 'idle'}
                </div>
              </div>

              <div className="mt-4 rounded-2xl border border-white/10 bg-black/40 p-4">
                <div className="text-xs uppercase tracking-[0.2em] text-white/40">Selected Station</div>
                <div className="mt-2 text-lg font-semibold">
                  {selectedStation?.name ?? 'No station selected'}
                </div>
                <div className="text-sm text-white/50">
                  {selectedStation?.frequency ? `${selectedStation.frequency.toFixed(1)} MHz` : '--'}
                  {selectedStation?.region ? ` · ${selectedStation.region}` : ''}
                  {typeof selectedStation?.listenerCount === 'number' ? ` · ${selectedStation.listenerCount} listeners` : ''}
                </div>
                <div className="mt-3 text-sm text-white/60">
                  {selectedStation?.description ?? 'Choose a station to listen.'}
                </div>
                {selectedRoom && (
                  <div className="mt-3 rounded-xl border border-sky-300/20 bg-sky-500/5 px-3 py-2 text-xs text-sky-100">
                    Room: {selectedRoom.toneId ?? 'room'} {selectedRoom.appKey ? `· ${selectedRoom.appKey}` : ''}{' '}
                    {selectedRoom.frequency ? `· ${selectedRoom.frequency.toFixed(1)} MHz` : ''}
                  </div>
                )}
              </div>

              <div className="mt-4">
                <audio
                  ref={audioRef}
                  controls
                  className="w-full"
                  crossOrigin="anonymous"
                  onPause={() => {
                    setStreamStatus('paused');
                    if (selectedStation?.id) {
                      postTelemetry('playback_stop', { stationId: selectedStation.id });
                    }
                  }}
                  onPlay={() => {
                    setStreamStatus('playing');
                    if (selectedStation?.id) {
                      postTelemetry('playback_start', { stationId: selectedStation.id });
                    }
                  }}
                  onError={() => setStreamStatus('error')}
                />
                {selectedStation && !selectedStation.streamUrl && (
                  <div className="mt-2 text-xs uppercase tracking-[0.2em] text-white/40">
                    Stream URL not set.
                  </div>
                )}
                {streamStatus === 'error' && (
                  <div className="mt-2 text-xs uppercase tracking-[0.2em] text-red-300">
                    Stream playback failed.
                  </div>
                )}
              </div>

              <div className="mt-4 rounded-2xl border border-white/10 bg-black/40 p-4">
                <div className="text-xs uppercase tracking-[0.2em] text-white/40">Stream Health</div>
                <div className="mt-3 grid grid-cols-2 gap-3 text-xs text-white/60">
                  <div>
                    <div className="uppercase tracking-[0.2em] text-white/40">Status</div>
                    <div className="mt-1 text-white/80">{hlsHealth.status}</div>
                  </div>
                  <div>
                    <div className="uppercase tracking-[0.2em] text-white/40">Bitrate</div>
                    <div className="mt-1 text-white/80">
                      {hlsHealth.bitrateKbps ? `${hlsHealth.bitrateKbps} kbps` : '—'}
                    </div>
                  </div>
                  <div>
                    <div className="uppercase tracking-[0.2em] text-white/40">Uptime</div>
                    <div className="mt-1 text-white/80">{hlsWindow || '—'}</div>
                  </div>
                  <div>
                    <div className="uppercase tracking-[0.2em] text-white/40">Updated</div>
                    <div className="mt-1 text-white/80">
                      {hlsHealth.lastUpdatedMs ? formatDurationMs(Date.now() - hlsHealth.lastUpdatedMs) : '—'}
                    </div>
                  </div>
                </div>
                {isHlsUrl(selectedStation?.streamUrl) && (
                  <div className="mt-3 text-[11px] uppercase tracking-[0.2em] text-white/40">
                    Uptime reflects the current HLS window.
                  </div>
                )}
                {!isHlsUrl(selectedStation?.streamUrl) && (
                  <div className="mt-3 text-[11px] uppercase tracking-[0.2em] text-white/40">
                    HLS health available for .m3u8 streams only.
                  </div>
                )}
              </div>
            </div>
          </section>
        </main>
      </div>
    </div>
  );
}

export default App;
