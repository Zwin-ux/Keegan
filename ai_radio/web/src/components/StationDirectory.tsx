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
          <div className="kicker">Stations</div>
          <h2 className="title mt-2 text-2xl">Regional Directory</h2>
        </div>
        <div className="tab-group">
          <button
            onClick={() => setTab('stations')}
            className={`tab-button ${tab === 'stations' ? 'active' : ''}`}
          >
            Stations
          </button>
          <button
            onClick={() => setTab('rooms')}
            className={`tab-button ${tab === 'rooms' ? 'active' : ''}`}
          >
            Rooms
          </button>
        </div>
        <div className="flex flex-wrap gap-2">
          <input
            value={region}
            onChange={(e) => onRegionChange(e.target.value)}
            placeholder="region (ex: us-midwest)"
            className="field w-48"
          />
          <input
            value={query}
            onChange={(e) => onQueryChange(e.target.value)}
            placeholder={tab === 'rooms' ? 'search room' : 'search station'}
            className="field w-48"
          />
        </div>
      </div>

      <div className="flex-1 overflow-auto pr-2">
        {tab === 'stations' && loading && (
          <div className="panel-inset p-6 text-sm text-muted">
            Scanning frequencies...
          </div>
        )}
        {tab === 'stations' && error && (
          <div className="panel-inset p-6 text-sm text-ember">
            {error}
          </div>
        )}
        {tab === 'stations' && !loading && !error && filtered.length === 0 && (
          <div className="panel-inset p-6 text-sm text-muted">
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
                  className={`card w-full px-5 py-4 text-left ${active ? 'card-active' : ''}`}
                >
                  <div className="flex items-center justify-between">
                    <div>
                      <div className="kicker">
                        {station.frequency ? `${station.frequency.toFixed(1)} MHz` : 'Unlisted'}
                      </div>
                      <div className="title mt-1 text-lg">{station.name ?? station.id}</div>
                    </div>
                    <div className="flex items-center gap-3 text-[10px] font-mono uppercase tracking-[0.2em] text-muted">
                      <span className={`h-2 w-2 rounded-full ${statusColor}`} />
                      {status}
                      {typeof station.listenerCount === 'number' && (
                        <span>{station.listenerCount} listeners</span>
                      )}
                    </div>
                  </div>
                  <div className="mt-2 text-sm text-muted">
                    {station.description ?? 'Signal locked. Atmosphere undefined.'}
                  </div>
                  <div className="mt-3 flex flex-wrap items-center gap-3 text-[10px] font-mono uppercase tracking-[0.2em] text-muted">
                    <span>{station.region ?? 'unknown region'}</span>
                    {station.source && <span>{station.source}</span>}
                    {station.mood && <span>{station.mood.replace('_', ' ')}</span>}
                    {typeof station.energy === 'number' && (
                      <span>{Math.round(station.energy * 100)}% energy</span>
                    )}
                  </div>
                  <div className="mt-4">
                    {station.streamUrl ? (
                      <div className="flex flex-wrap items-center gap-3 text-xs font-semibold uppercase tracking-[0.2em] text-[color:var(--copper)]">
                        <span>Listen now</span>
                        <a
                          href={station.streamUrl}
                          target="_blank"
                          rel="noreferrer"
                          onClick={(event) => event.stopPropagation()}
                          className="text-muted underline-offset-4 hover:text-[color:var(--cloud)] hover:underline"
                        >
                          Open stream
                        </a>
                      </div>
                    ) : (
                      <span className="text-xs uppercase tracking-[0.2em] text-muted">No stream</span>
                    )}
                  </div>
                </button>
              );
            })}
          </div>
        )}

        {tab === 'rooms' && roomsLoading && (
          <div className="panel-inset p-6 text-sm text-muted">
            Scanning rooms...
          </div>
        )}
        {tab === 'rooms' && roomsError && (
          <div className="panel-inset p-6 text-sm text-ember">
            {roomsError}
          </div>
        )}
        {tab === 'rooms' && !roomsLoading && !roomsError && filteredRooms.length === 0 && (
          <div className="panel-inset p-6 text-sm text-muted">
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
                  className={`card w-full px-5 py-4 text-left ${active ? 'card-active' : ''}`}
                >
                  <div className="flex items-center justify-between">
                    <div>
                      <div className="kicker">
                        {room.frequency ? `${room.frequency.toFixed(1)} MHz` : 'Unlisted'}
                      </div>
                      <div className="title mt-1 text-lg">
                        {room.toneId ?? 'Room'} {room.appKey ? `- ${room.appKey}` : ''}
                      </div>
                    </div>
                    <div className="flex items-center gap-3 text-[10px] font-mono uppercase tracking-[0.2em] text-muted">
                      {typeof room.listenerCount === 'number' && (
                        <span>{room.listenerCount} listeners</span>
                      )}
                    </div>
                  </div>
                  <div className="mt-2 text-sm text-muted">
                    Room ID: {room.roomId}
                  </div>
                  <div className="mt-3 flex flex-wrap items-center gap-3 text-[10px] font-mono uppercase tracking-[0.2em] text-muted">
                    <span>{room.region ?? 'unknown region'}</span>
                    {room.source && <span>{room.source}</span>}
                    {room.toneId && <span>{room.toneId.replace('_', ' ')}</span>}
                  </div>
                  {active && room.frequency && (
                    <div className="mt-3 text-xs font-mono uppercase tracking-[0.2em] text-[color:var(--ion)]">
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

