import React, { useState } from 'react';

export interface Station {
  id: string;
  name?: string;
  region?: string;
  frequency?: number;
  description?: string;
  streamUrl?: string;
  status?: string;
  mood?: string;
  energy?: number;
  broadcasting?: boolean;
  listenerCount?: number;
  source?: string;
}

export interface Room {
  roomId: string;
  region?: string;
  appKey?: string;
  toneId?: string;
  frequency?: number;
  listenerCount?: number;
  lastSeen?: number;
  source?: string;
}

interface StationDirectoryProps {
  stations: Station[];
  rooms: Room[];
  loading: boolean;
  error: string | null;
  roomsLoading: boolean;
  roomsError: string | null;
  region: string;
  onRegionChange: (value: string) => void;
  query: string;
  onQueryChange: (value: string) => void;
  onSelect: (station: Station) => void;
  onSelectRoom: (room: Room) => void;
  selectedId: string | null;
  selectedRoomId: string | null;
}

export const StationDirectory: React.FC<StationDirectoryProps> = ({
  stations,
  rooms,
  loading,
  error,
  roomsLoading,
  roomsError,
  region,
  onRegionChange,
  query,
  onQueryChange,
  onSelect,
  onSelectRoom,
  selectedId,
  selectedRoomId,
}) => {
  const [tab, setTab] = useState<'stations' | 'rooms'>('stations');

  const filtered = stations.filter((station) => {
    const hay = `${station.name ?? ''} ${station.description ?? ''}`.toLowerCase();
    return !query || hay.includes(query.toLowerCase());
  });

  const filteredRooms = rooms.filter((room) => {
    const hay = `${room.appKey ?? ''} ${room.toneId ?? ''} ${room.roomId ?? ''}`.toLowerCase();
    return !query || hay.includes(query.toLowerCase());
  });

  return (
    <div className="flex h-full flex-col">
      <div className="flex flex-wrap items-center justify-between gap-4 pb-4">
        <div>
          <div className="text-xs uppercase tracking-[0.2em] text-white/50">Stations</div>
          <h2 className="mt-2 text-2xl font-semibold text-white">Regional Directory</h2>
        </div>
        <div className="flex items-center gap-2 text-xs uppercase tracking-[0.2em] text-white/60">
          <button
            onClick={() => setTab('stations')}
            className={`rounded-full border px-3 py-2 transition ${
              tab === 'stations' ? 'border-amber-300/70 text-amber-200' : 'border-white/10 text-white/50'
            }`}
          >
            Stations
          </button>
          <button
            onClick={() => setTab('rooms')}
            className={`rounded-full border px-3 py-2 transition ${
              tab === 'rooms' ? 'border-sky-300/70 text-sky-200' : 'border-white/10 text-white/50'
            }`}
          >
            Rooms
          </button>
        </div>
        <div className="flex flex-wrap gap-2">
          <input
            value={region}
            onChange={(e) => onRegionChange(e.target.value)}
            placeholder="region (ex: us-midwest)"
            className="h-10 w-48 rounded-md border border-white/10 bg-white/5 px-3 text-sm text-white/80 placeholder:text-white/30 focus:border-amber-400/60 focus:outline-none"
          />
          <input
            value={query}
            onChange={(e) => onQueryChange(e.target.value)}
            placeholder={tab === 'rooms' ? 'search room' : 'search station'}
            className="h-10 w-48 rounded-md border border-white/10 bg-white/5 px-3 text-sm text-white/80 placeholder:text-white/30 focus:border-sky-400/60 focus:outline-none"
          />
        </div>
      </div>

      <div className="flex-1 overflow-auto pr-2">
        {tab === 'stations' && loading && (
          <div className="rounded-xl border border-white/10 bg-white/5 p-6 text-sm text-white/60">
            Scanning frequencies...
          </div>
        )}
        {tab === 'stations' && error && (
          <div className="rounded-xl border border-red-400/30 bg-red-500/10 p-6 text-sm text-red-100">
            {error}
          </div>
        )}
        {tab === 'stations' && !loading && !error && filtered.length === 0 && (
          <div className="rounded-xl border border-white/10 bg-white/5 p-6 text-sm text-white/60">
            No stations found.
          </div>
        )}

        {tab === 'stations' && (
          <div className="grid gap-4">
            {filtered.map((station) => {
              const active = station.id === selectedId;
              const status = station.status ?? (station.broadcasting ? 'live' : 'idle');
              const statusColor = status === 'live' ? 'bg-emerald-400' : 'bg-white/40';
              return (
                <button
                  key={station.id}
                  onClick={() => onSelect(station)}
                  className={`group w-full rounded-2xl border px-5 py-4 text-left transition ${
                    active
                      ? 'border-amber-400/60 bg-gradient-to-br from-amber-400/10 via-white/5 to-transparent'
                      : 'border-white/10 bg-white/5 hover:border-white/20'
                  }`}
                >
                  <div className="flex items-center justify-between">
                    <div>
                      <div className="text-xs uppercase tracking-[0.2em] text-white/40">
                        {station.frequency ? `${station.frequency.toFixed(1)} MHz` : 'Unlisted'}
                      </div>
                      <div className="mt-1 text-lg font-semibold text-white">{station.name ?? station.id}</div>
                    </div>
                    <div className="flex items-center gap-3 text-xs uppercase tracking-[0.2em] text-white/60">
                      <span className={`h-2 w-2 rounded-full ${statusColor}`} />
                      {status}
                      {typeof station.listenerCount === 'number' && (
                        <span>{station.listenerCount} listeners</span>
                      )}
                    </div>
                  </div>
                  <div className="mt-2 text-sm text-white/60">
                    {station.description ?? 'Signal locked. Atmosphere undefined.'}
                  </div>
                <div className="mt-3 flex flex-wrap items-center gap-3 text-xs uppercase tracking-[0.18em] text-white/40">
                  <span>{station.region ?? 'unknown region'}</span>
                  {station.source && <span>{station.source}</span>}
                  {station.mood && <span>{station.mood.replace('_', ' ')}</span>}
                  {typeof station.energy === 'number' && (
                    <span>{Math.round(station.energy * 100)}% energy</span>
                  )}
                </div>
                  <div className="mt-4">
                    {station.streamUrl ? (
                      <div className="flex flex-wrap items-center gap-3 text-xs font-semibold uppercase tracking-[0.2em] text-amber-300">
                        <span>Listen now</span>
                        <a
                          href={station.streamUrl}
                          target="_blank"
                          rel="noreferrer"
                          onClick={(event) => event.stopPropagation()}
                          className="text-white/60 underline-offset-4 hover:text-white hover:underline"
                        >
                          Open stream
                        </a>
                      </div>
                    ) : (
                      <span className="text-xs uppercase tracking-[0.2em] text-white/30">No stream</span>
                    )}
                  </div>
                </button>
              );
            })}
          </div>
        )}

        {tab === 'rooms' && roomsLoading && (
          <div className="rounded-xl border border-white/10 bg-white/5 p-6 text-sm text-white/60">
            Scanning rooms...
          </div>
        )}
        {tab === 'rooms' && roomsError && (
          <div className="rounded-xl border border-red-400/30 bg-red-500/10 p-6 text-sm text-red-100">
            {roomsError}
          </div>
        )}
        {tab === 'rooms' && !roomsLoading && !roomsError && filteredRooms.length === 0 && (
          <div className="rounded-xl border border-white/10 bg-white/5 p-6 text-sm text-white/60">
            No rooms found.
          </div>
        )}

        {tab === 'rooms' && (
          <div className="grid gap-4">
            {filteredRooms.map((room) => {
              const active = room.roomId === selectedRoomId;
              return (
                <button
                  key={room.roomId}
                  onClick={() => onSelectRoom(room)}
                  className={`group w-full rounded-2xl border px-5 py-4 text-left transition ${
                    active
                      ? 'border-sky-400/60 bg-gradient-to-br from-sky-400/10 via-white/5 to-transparent'
                      : 'border-white/10 bg-white/5 hover:border-white/20'
                  }`}
                >
                  <div className="flex items-center justify-between">
                    <div>
                      <div className="text-xs uppercase tracking-[0.2em] text-white/40">
                        {room.frequency ? `${room.frequency.toFixed(1)} MHz` : 'Unlisted'}
                      </div>
                      <div className="mt-1 text-lg font-semibold text-white">
                        {room.toneId ?? 'Room'} {room.appKey ? `· ${room.appKey}` : ''}
                      </div>
                    </div>
                    <div className="flex items-center gap-3 text-xs uppercase tracking-[0.2em] text-white/60">
                      {typeof room.listenerCount === 'number' && (
                        <span>{room.listenerCount} listeners</span>
                      )}
                    </div>
                  </div>
                  <div className="mt-2 text-sm text-white/60">
                    Room ID: {room.roomId}
                  </div>
                <div className="mt-3 flex flex-wrap items-center gap-3 text-xs uppercase tracking-[0.18em] text-white/40">
                  <span>{room.region ?? 'unknown region'}</span>
                  {room.source && <span>{room.source}</span>}
                  {room.toneId && <span>{room.toneId.replace('_', ' ')}</span>}
                </div>
                  {active && room.frequency && (
                    <div className="mt-3 text-xs uppercase tracking-[0.2em] text-sky-200">
                      Tune to {room.frequency.toFixed(1)} MHz
                    </div>
                  )}
                </button>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
};
