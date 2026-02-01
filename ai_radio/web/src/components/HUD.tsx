import React, { useEffect, useState, useRef, useCallback } from 'react';
import { motion } from 'framer-motion';
import { Volume2, Power, Sliders, Share2 } from 'lucide-react';
import ShareModal from './ShareModal';

export interface LiveState {
    mood: string;
    targetMood?: string;
    energy: number;
    intensity?: number;
    activity?: number;
    idleSeconds?: number;
    playing: boolean;
    activeProcess?: string;
    updatedAtMs?: number;
}

interface HUDProps {
    mood: string;
    energy: number;
    playing: boolean;
    onToggle: () => void;
    volume: number;
    onVolumeChange: (v: number) => void;
    tunerEnergy: number;
    onTunerEnergyChange: (v: number) => void;
    tunerAmbience: number;
    onTunerAmbienceChange: (v: number) => void;
    getFrequencyData: () => Uint8Array | null;
    liveState: LiveState | null;
    wsStatus: 'offline' | 'connecting' | 'online';
}

const MOOD_STYLES: Record<string, { text: string; accent: string }> = {
    focus: { text: 'text-amber-500', accent: 'shadow-amber-500/50' },
    rain:  { text: 'text-blue-400',  accent: 'shadow-blue-400/50' },
    arcade:{ text: 'text-pink-500',  accent: 'shadow-pink-500/50' },
    sleep: { text: 'text-indigo-400',accent: 'shadow-indigo-400/50' },
    static:{ text: 'text-zinc-500',  accent: 'shadow-zinc-500/50' }
};

export const HUD: React.FC<HUDProps> = ({
    mood,
    energy,
    playing,
    onToggle,
    volume,
    onVolumeChange,
    tunerEnergy,
    onTunerEnergyChange,
    tunerAmbience,
    onTunerAmbienceChange,
    getFrequencyData,
    liveState,
    wsStatus,
}) => {
    const [showTuner, setShowTuner] = useState(false);
    const [isShareModalOpen, setShareModalOpen] = useState(false);
    const [freqBars, setFreqBars] = useState<number[]>(new Array(16).fill(0));
    
    // Tuning State
    const [frequency, setFrequency] = useState(88.0);
    const [locked, setLocked] = useState(false);
    
    const rafRef = useRef<number>(0);

    const url = typeof window !== 'undefined' ? `${window.location.protocol}//${window.location.hostname}:${window.location.port}` : '';

    // Tuning Logic
    const handleTune = (delta: number) => {
        setFrequency(prev => {
            const next = Math.max(87.0, Math.min(108.0, Number((prev + delta).toFixed(1))));
            
            // Call engine
            const result = (window as any)._keegan_engine?.tune(next);
            if (result) {
                setLocked(result.locked);
            }
            return next;
        });
    };

    const updateBars = useCallback(() => {
        const data = getFrequencyData();
        if (data && playing) {
            const bins = data.length;
            const bars: number[] = [];
            
            // If static, render noise
            if (!locked) {
                for (let i = 0; i < 16; i++) {
                   bars.push(Math.random() * 0.5); 
                }
            } else {
                const step = Math.max(1, Math.floor(bins / 16));
                for (let i = 0; i < 16; i++) {
                    const idx = Math.min(i * step, bins - 1);
                    bars.push(data[idx] / 255);
                }
            }
            setFreqBars(bars);
        } else if (!playing) {
            setFreqBars(new Array(16).fill(0));
        }
        rafRef.current = requestAnimationFrame(updateBars);
    }, [getFrequencyData, playing, locked]);

    useEffect(() => {
        rafRef.current = requestAnimationFrame(updateBars);
        return () => cancelAnimationFrame(rafRef.current);
    }, [updateBars]);

    const style = locked ? (MOOD_STYLES[mood] || MOOD_STYLES.focus) : MOOD_STYLES.static;
    const bridgeMood = liveState?.mood ?? '--';
    const bridgeEnergy = liveState ? Math.floor(liveState.energy * 100) : '--';
    const bridgeProcess = liveState?.activeProcess || '';
    const wsIndicator =
        wsStatus === 'online' ? 'bg-green-500' : wsStatus === 'connecting' ? 'bg-yellow-500 animate-pulse' : 'bg-red-500';

    return (
        <>
            {isShareModalOpen && <ShareModal url={url} onClose={() => setShareModalOpen(false)} />}
            <div className="relative w-[340px] glass-panel p-4 flex flex-col gap-4 font-mono text-xs border-t-2 border-t-white/10">
                {/* Top Bar */}
                <div className="flex justify-between items-center opacity-80">
                    <div className="flex items-center gap-2">
                        <div
                            className={`w-2 h-2 rounded-full ${playing ? (locked ? 'bg-green-500 animate-pulse' : 'bg-yellow-500 animate-ping') : 'bg-red-500'}`}
                            role="status"
                        />
                        <span className="uppercase tracking-widest text-[10px] text-white/50">
                            {playing ? (locked ? `RX: ${mood.toUpperCase()}` : 'SCANNING...') : 'OFFLINE'}
                        </span>
                    </div>
                    <div className="text-white/50 tracking-widest font-bold text-amber-500">{frequency.toFixed(1)} MHZ</div>
                </div>

                {/* Engine Bridge */}
                <div className="flex items-center justify-between bg-white/5 border border-white/10 rounded px-3 py-2">
                    <div className="flex items-center gap-2">
                        <div className={`w-2 h-2 rounded-full ${wsIndicator}`} />
                        <div className="text-[10px] uppercase tracking-widest text-white/60">Engine</div>
                    </div>
                    <div className="text-[10px] text-white/70">
                        {bridgeMood} · {bridgeEnergy}% {bridgeProcess ? `· ${bridgeProcess}` : ''}
                    </div>
                </div>

                {/* Visualizer / Tuner Area */}
                <div
                    className="h-36 bg-black/40 rounded-lg border border-white/5 relative overflow-hidden flex flex-col items-center justify-center cursor-ew-resize select-none"
                    role="slider"
                    aria-label="Tuning Dial"
                    onWheel={(e) => handleTune(e.deltaY > 0 ? -0.1 : 0.1)}
                >
                    <div className="absolute inset-0 bg-[linear-gradient(rgba(255,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.03)_1px,transparent_1px)] bg-[size:20px_20px]" />

                    {/* Frequency Scale */}
                    <div className="absolute top-2 w-full flex justify-center overflow-hidden opacity-50">
                        <div className="flex gap-4 text-[9px] text-white/30" style={{ transform: `translateX(${(98.0 - frequency) * 20}px)` }}>
                             {[...Array(220)].map((_, i) => {
                                 const f = (87.0 + i * 0.1).toFixed(1);
                                 return (
                                     <div key={i} className={`flex flex-col items-center w-[20px] ${f.endsWith('.0') ? 'text-white/60' : 'text-transparent'}`}>
                                         <div className={`w-[1px] ${f.endsWith('.0') ? 'h-3 bg-white/50' : 'h-1 bg-white/20'}`}></div>
                                         <span className="mt-1">{f}</span>
                                     </div>
                                 )
                             })}
                        </div> 
                        {/* Center Marker */}
                        <div className="absolute top-0 left-1/2 w-[2px] h-full bg-red-500/50 -translate-x-1/2 z-10"></div>
                    </div>

                    <div className="flex items-end gap-[3px] h-20 pointer-events-none mt-6">
                        {freqBars.map((val, i) => (
                            <motion.div
                                key={i}
                                animate={{
                                    height: playing ? Math.max(2, val * 72) : 2,
                                    opacity: playing ? 0.4 + val * 0.6 : 0.2,
                                }}
                                transition={{ duration: 0.06, ease: 'linear' }}
                                className={`w-[10px] rounded-t-sm bg-current ${style.text}`}
                            />
                        ))}
                    </div>
                    
                    {/* StaticOverlay */}
                    {!locked && playing && (
                        <div className="absolute inset-0 bg-white opacity-[0.05] mix-blend-overlay pointer-events-none animate-pulse"></div>
                    )}
                </div>


                {/* Info Section */}
                <div className="flex justify-between items-end">
                    <div>
                        <div className="text-[10px] text-white/40 uppercase mb-1">Protocol</div>
                        <div className={`text-lg font-bold tracking-tighter uppercase ${style.text} ${style.accent} drop-shadow-md`}>
                            {mood}
                        </div>
                    </div>
                    <div className="text-right">
                        <div className="text-[10px] text-white/40 uppercase mb-1">Signal</div>
                        <div className="text-lg font-bold text-white/80">
                            {playing ? `${Math.floor(energy * 100)}%` : '--'}
                        </div>
                    </div>
                </div>

                {/* Volume */}
                <div className="flex items-center gap-3 bg-white/5 p-2 rounded">
                    <Volume2 className="w-4 h-4 text-white/40 shrink-0" aria-hidden="true" />
                    <input
                        type="range"
                        min="0"
                        max="100"
                        value={Math.round(volume * 100)}
                        onChange={(e) => onVolumeChange(Number(e.target.value) / 100)}
                        aria-label="Master volume"
                        className="flex-1 h-1 bg-white/10 rounded-full appearance-none [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:bg-white [&::-webkit-slider-thumb]:rounded-full cursor-pointer"
                    />
                    <span className="text-[10px] text-white/40 w-7 text-right">{Math.round(volume * 100)}</span>
                </div>

                {/* Tuner Panel */}
                {showTuner && (
                    <motion.div
                        initial={{ height: 0, opacity: 0 }}
                        animate={{ height: 'auto', opacity: 1 }}
                        exit={{ height: 0, opacity: 0 }}
                        className="flex flex-col gap-3 bg-white/5 p-3 rounded"
                    >
                        <div className="text-[10px] text-white/40 uppercase tracking-widest">Tuner</div>
                        <label className="flex items-center gap-3">
                            <span className="text-[10px] text-white/50 w-16">Energy</span>
                            <input
                                type="range"
                                min="0"
                                max="100"
                                value={Math.round(tunerEnergy * 100)}
                                onChange={(e) => onTunerEnergyChange(Number(e.target.value) / 100)}
                                aria-label="Energy level — controls rhythm intensity"
                                className="flex-1 h-1 bg-white/10 rounded-full appearance-none [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:bg-amber-500 [&::-webkit-slider-thumb]:rounded-full cursor-pointer"
                            />
                            <span className="text-[10px] text-white/50 w-8 text-right">{Math.round(tunerEnergy * 100)}%</span>
                        </label>
                        <label className="flex items-center gap-3">
                            <span className="text-[10px] text-white/50 w-16">Ambience</span>
                            <input
                                type="range"
                                min="0"
                                max="100"
                                value={Math.round(tunerAmbience * 100)}
                                onChange={(e) => onTunerAmbienceChange(Number(e.target.value) / 100)}
                                aria-label="Ambience level — controls drone and texture volume"
                                className="flex-1 h-1 bg-white/10 rounded-full appearance-none [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:bg-blue-400 [&::-webkit-slider-thumb]:rounded-full cursor-pointer"
                            />
                            <span className="text-[10px] text-white/50 w-8 text-right">{Math.round(tunerAmbience * 100)}%</span>
                        </label>
                    </motion.div>
                )}

                {/* Controls */}
                <div className="grid grid-cols-4 gap-2 mt-2">
                    <button
                        onClick={onToggle}
                        aria-label={playing ? 'Pause playback' : 'Start playback'}
                        className={`col-span-2 glass-button h-10 rounded flex items-center justify-center gap-2 uppercase tracking-wide font-bold hover:bg-white/10 ${playing ? 'text-white' : 'text-white/50'}`}
                    >
                        <Power className="w-4 h-4" aria-hidden="true" />
                        {playing ? 'Active' : 'Standby'}
                    </button>
                    <button
                        onClick={() => setShowTuner(v => !v)}
                        aria-label={showTuner ? 'Close tuner' : 'Open tuner'}
                        aria-expanded={showTuner}
                        className={`glass-button h-10 rounded flex items-center justify-center hover:text-white ${showTuner ? 'text-white bg-white/10' : 'text-white/50'}`}
                    >
                        <Sliders className="w-4 h-4" aria-hidden="true" />
                    </button>
                    <button
                        onClick={() => setShareModalOpen(true)}
                        aria-label="Share station"
                        className="glass-button h-10 rounded flex items-center justify-center text-white/50 hover:text-white"
                    >
                        <Share2 className="w-4 h-4" aria-hidden="true" />
                    </button>
                </div>

                {/* Keyboard hints */}
                <div className="text-[9px] text-white/20 text-center tracking-wider">
                    SPACE play/pause &middot; 1-4 switch mood
                </div>

                {/* Footer */}
                <div className="absolute bottom-[-10px] right-2 text-[8px] text-white/10 rotate-90 origin-right">
                    KEEGAN RADIO
                </div>
            </div>
        </>
    );
};

