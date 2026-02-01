export type Mood = 'focus' | 'rain' | 'arcade' | 'sleep';

interface StemRecipe {
  file: string;
  baseGain: number;
  type: 'drone' | 'rhythm' | 'texture';
}

interface Layer {
  name: string;
  element: HTMLAudioElement;
  source: MediaElementAudioSourceNode;
  gain: GainNode;
  loaded: boolean;
  failed: boolean;
}

export type EngineEvent =
  | { type: 'stem-error'; mood: Mood; file: string }
  | { type: 'stem-loaded'; mood: Mood; file: string }
  | { type: 'all-stems-failed'; mood: Mood };

type EventListener = (event: EngineEvent) => void;

const STORAGE_KEY = 'keegan-radio-prefs';

interface Preferences {
  volume: number;
  energy: number;
  ambience: number;
  mood: Mood;
}

function loadPreferences(): Preferences {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      return {
        volume: typeof parsed.volume === 'number' ? parsed.volume : 0.8,
        energy: typeof parsed.energy === 'number' ? parsed.energy : 0.5,
        ambience: typeof parsed.ambience === 'number' ? parsed.ambience : 0.5,
        mood: ['focus', 'rain', 'arcade', 'sleep'].includes(parsed.mood) ? parsed.mood : 'focus',
      };
    }
  } catch { /* localStorage unavailable */ }
  return { volume: 0.8, energy: 0.5, ambience: 0.5, mood: 'focus' };
}

function savePreferences(prefs: Partial<Preferences>) {
  try {
    const current = loadPreferences();
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ ...current, ...prefs }));
  } catch { /* localStorage unavailable */ }
}

export class WebAudioEngine {
  private ctx: AudioContext;
  private masterGain: GainNode;
  private analyser: AnalyserNode;
  private mood: Mood;
  private layers: Map<string, Layer> = new Map();
  private isPlaying = false;
  private _volume: number;
  private _energy: number;
  private _ambience: number;
  private listeners: EventListener[] = [];
  private timeDomainData: Uint8Array<ArrayBuffer>;

  private static readonly RECIPES: Record<Mood, StemRecipe[]> = {
    focus: [
      { file: 'base_drone.wav', baseGain: 0.6, type: 'drone' },
      { file: 'rhythm_tick.wav', baseGain: 0.4, type: 'rhythm' },
      { file: 'texture_paper.wav', baseGain: 0.3, type: 'texture' },
    ],
    rain: [
      { file: 'drone_water.wav', baseGain: 0.6, type: 'drone' },
      { file: 'drops_layer.wav', baseGain: 0.5, type: 'texture' },
      { file: 'metal_echo.wav', baseGain: 0.3, type: 'rhythm' },
    ],
    arcade: [
      { file: 'neon_bed.wav', baseGain: 0.5, type: 'drone' },
      { file: 'synth_line.wav', baseGain: 0.5, type: 'rhythm' },
      { file: 'coin_echo.wav', baseGain: 0.3, type: 'texture' },
    ],
    sleep: [
      { file: 'engine_thrum.wav', baseGain: 0.6, type: 'drone' },
      { file: 'ventilation.wav', baseGain: 0.4, type: 'texture' },
      { file: 'hull_creak.wav', baseGain: 0.25, type: 'rhythm' },
    ],
  };

  private staticNode: AudioBufferSourceNode | null = null;
  private staticGain: GainNode;
  private currentFrequency: number = 88.0;
  
  // Simulated Ghost Stations
  private static readonly GHOSTS = [
    { freq: 89.5, mood: 'focus' as Mood },
    { freq: 94.2, mood: 'rain' as Mood },
    { freq: 101.8, mood: 'arcade' as Mood },
    { freq: 106.5, mood: 'sleep' as Mood }
  ];

  constructor() {
    const prefs = loadPreferences();
    this._volume = prefs.volume;
    this._energy = prefs.energy;
    this._ambience = prefs.ambience;
    this.mood = prefs.mood;

    this.ctx = new (window.AudioContext || (window as any).webkitAudioContext)();
    this.masterGain = this.ctx.createGain();
    this.staticGain = this.ctx.createGain();
    
    this.analyser = this.ctx.createAnalyser();
    this.analyser.fftSize = 256;
    this.analyser.smoothingTimeConstant = 0.5; // Faster response for visualizer
    
    this.masterGain.connect(this.analyser);
    this.staticGain.connect(this.analyser); // Static shows on visualizer too
    
    this.analyser.connect(this.ctx.destination);
    
    this.masterGain.gain.value = this._volume;
    this.staticGain.gain.value = 0; // Start silent
    
    this.timeDomainData = new Uint8Array(this.analyser.fftSize);
    
    // Create static buffer
    this.createStaticBuffer();
  }

  private createStaticBuffer() {
    const bufferSize = this.ctx.sampleRate * 2; // 2 seconds loop
    const buffer = this.ctx.createBuffer(1, bufferSize, this.ctx.sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < bufferSize; i++) {
        data[i] = (Math.random() * 2 - 1) * 0.15; // Pink-ish noise level
    }
    
    // We create the node when needed, but keep the buffer ready? 
    // Actually efficient to just keep recreating the source node on start.
    this.staticBuffer = buffer;
  }
  private staticBuffer: AudioBuffer | null = null;

  on(listener: EventListener) {
    this.listeners.push(listener);
    return () => {
      this.listeners = this.listeners.filter(l => l !== listener);
    };
  }

  private emit(event: EngineEvent) {
    this.listeners.forEach(l => l(event));
  }

  async init() {
    if (this.ctx.state === 'suspended') {
      await this.ctx.resume();
    }
  }

  getPreferences(): Preferences {
    return {
      volume: this._volume,
      energy: this._energy,
      ambience: this._ambience,
      mood: this.mood,
    };
  }

  getMood(): Mood {
    return this.mood;
  }

  getFrequencyData(): Uint8Array {
    const data = new Uint8Array(this.analyser.frequencyBinCount);
    this.analyser.getByteFrequencyData(data);
    return data;
  }

  /** Returns RMS energy as 0-1 from the current audio signal. */
  getRMSEnergy(): number {
    this.analyser.getByteTimeDomainData(this.timeDomainData);
    let sum = 0;
    for (let i = 0; i < this.timeDomainData.length; i++) {
      const normalized = (this.timeDomainData[i] - 128) / 128;
      sum += normalized * normalized;
    }
    const rms = Math.sqrt(sum / this.timeDomainData.length);
    return Math.min(1, rms * 3);
  }

  setVolume(v: number) {
    this._volume = Math.max(0, Math.min(1, v));
    this.masterGain.gain.setTargetAtTime(this._volume, this.ctx.currentTime, 0.05);
    savePreferences({ volume: this._volume });
  }

  setEnergy(e: number) {
    this._energy = Math.max(0, Math.min(1, e));
    this.applyMix();
    savePreferences({ energy: this._energy });
  }

  setAmbience(a: number) {
    this._ambience = Math.max(0, Math.min(1, a));
    this.applyMix();
    savePreferences({ ambience: this._ambience });
  }

  setMood(mood: Mood) {
    if (this.mood === mood) return;
    this.mood = mood;
    savePreferences({ mood });
    if (this.isPlaying) {
      this.playCurrentMood();
    }
  }

  tune(frequency: number) {
    this.currentFrequency = frequency;
    
    // Find closest station
    const station = WebAudioEngine.GHOSTS.find(g => Math.abs(g.freq - frequency) < 0.3);
    
    if (station) {
        // Locked on?
        const diff = Math.abs(station.freq - frequency);
        const signalStrength = 1.0 - (diff / 0.3); // 0.0 to 1.0
        
        // Signal logic
        if (this.mood !== station.mood) {
             this.setMood(station.mood);
        }
        
        if (this.isPlaying) {
            this.crossfadeSignal(signalStrength);
        }
        
        // Notify UI of "Lock"
        return { locked: true, strength: signalStrength, mood: station.mood };
    } else {
        // Just static
        if (this.isPlaying) {
            this.crossfadeSignal(0); 
        }
        return { locked: false, strength: 0, mood: null };
    }
  }
  
  private crossfadeSignal(signalStrength: number) {
      // signalStrength 1.0 = Full Audio, 0.0 = Full Static
      // Audio Gain
      this.masterGain.gain.setTargetAtTime(this._volume * signalStrength, this.ctx.currentTime, 0.1);
      
      // Static Gain (Local volume control for static mainly)
      // Static gets louder as signal gets weaker
      const staticVol = (1.0 - signalStrength) * 0.15 * this._volume; 
      this.staticGain.gain.setTargetAtTime(staticVol, this.ctx.currentTime, 0.1);
  }

  togglePlay() {
    this.isPlaying = !this.isPlaying;
    if (this.isPlaying) {
      this.ctx.resume();
      this.startStatic();
      // Check current tuning to see if we should play music
      const tuning = this.tune(this.currentFrequency);
      if (tuning.locked) {
          this.playCurrentMood();
          this.crossfadeSignal(tuning.strength);
      } else {
          this.crossfadeSignal(0); // All static
      }
    } else {
      this.stopAll();
      this.stopStatic();
    }
    return this.isPlaying;
  }
  
  private startStatic() {
      if (this.staticNode) return;
      this.staticNode = this.ctx.createBufferSource();
      this.staticNode.buffer = this.staticBuffer;
      this.staticNode.loop = true;
      this.staticNode.connect(this.staticGain);
      this.staticNode.start();
  }
  
  private stopStatic() {
      if (this.staticNode) {
          this.staticNode.stop();
          this.staticNode.disconnect();
          this.staticNode = null;
      }
  }

  // Modified Play/Stop to not kill MasterGain but use Layer Gains
  // ...


  private applyMix() {
    this.layers.forEach(layer => {
      const recipe = WebAudioEngine.RECIPES[this.mood].find(r => r.file === layer.name);
      if (!recipe) return;

      let gain = recipe.baseGain;
      if (recipe.type === 'rhythm') {
        gain *= 0.5 + this._energy * 0.8;
      } else if (recipe.type === 'drone') {
        gain *= 0.6 + this._ambience * 0.6;
      } else if (recipe.type === 'texture') {
        gain *= 0.4 + this._ambience * 0.5 + this._energy * 0.2;
      }

      layer.gain.gain.setTargetAtTime(Math.min(1, gain), this.ctx.currentTime, 0.3);
    });
  }

  private playCurrentMood() {
    this.stopAll();

    const stems = WebAudioEngine.RECIPES[this.mood] || [];
    let loadedCount = 0;
    let failedCount = 0;
    const totalCount = stems.length;

    stems.forEach(recipe => {
      const audio = new Audio(`/stems/${this.mood}/${recipe.file}`);
      audio.crossOrigin = 'anonymous';
      audio.loop = true;

      const source = this.ctx.createMediaElementSource(audio);
      const gain = this.ctx.createGain();
      gain.gain.value = 0;
      source.connect(gain);
      gain.connect(this.masterGain);

      const layer: Layer = {
        name: recipe.file,
        element: audio,
        source,
        gain,
        loaded: false,
        failed: false,
      };
      this.layers.set(recipe.file, layer);

      audio.addEventListener('error', () => {
        layer.failed = true;
        failedCount++;
        this.emit({ type: 'stem-error', mood: this.mood, file: recipe.file });
        if (failedCount === totalCount) {
          this.emit({ type: 'all-stems-failed', mood: this.mood });
        }
      }, { once: true });

      audio.play().then(() => {
        layer.loaded = true;
        loadedCount++;
        gain.gain.setTargetAtTime(recipe.baseGain, this.ctx.currentTime, 0.5);
        this.emit({ type: 'stem-loaded', mood: this.mood, file: recipe.file });
        if (loadedCount + failedCount === totalCount) {
          this.applyMix();
        }
      }).catch(() => {
        layer.failed = true;
        failedCount++;
        this.emit({ type: 'stem-error', mood: this.mood, file: recipe.file });
        if (failedCount === totalCount) {
          this.emit({ type: 'all-stems-failed', mood: this.mood });
        }
      });
    });
  }

  private stopAll() {
    this.layers.forEach(l => {
      l.gain.gain.setTargetAtTime(0, this.ctx.currentTime, 0.3);
      setTimeout(() => {
        l.element.pause();
        l.element.currentTime = 0;
        try { l.source.disconnect(); } catch { /* already disconnected */ }
        try { l.gain.disconnect(); } catch { /* already disconnected */ }
      }, 400);
    });
    this.layers.clear();
  }
}

// Singleton instance
export const engine = new WebAudioEngine();
(window as any)._keegan_engine = engine;
