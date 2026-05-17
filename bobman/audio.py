"""
Audio module - Handles all sound and music playback.

Ported from src/audio.cpp and src/audio.h
"""

import os
from typing import TYPE_CHECKING, Optional, Dict
import sdl2
import sdl2.sdlmixer

if TYPE_CHECKING:
    pass

from bobman.constants import (
    DATA_DIR,
    AUDIO_ENABLED,
    MENU_MUSIC_PATH,
    WIN_MUSIC_PATH,
    LOSE_MUSIC_PATH,
    DISCO_MUSIC_PATH,
    COIN_SOUND_PATH,
    LIFE_LOST_SOUND_PATH,
    DEATH_SOUND_PATH,
)


class Audio:
    """
    Handles loading and playing of sound effects and music.
    
    This class manages:
    - SDL_mixer initialization
    - Loading sound effects (WAV files)
    - Loading music (MP3 files)
    - Playing sounds with volume and panning
    - Music playback and fading
    """
    
    def __init__(self):
        """Initialize the audio system."""
        self.audio_ready = False
        self.menu_music_active = False
        
        # Sound effects cache
        self.sounds: Dict[str, Optional[sdl2.sdlmixer.Mix_Chunk]] = {}
        
        # Music cache
        self.music: Dict[str, Optional[sdl2.sdlmixer.Mix_Music]] = {}
        
        # Volume settings
        self.sfx_volume = 64  # 0-128
        self.music_volume = 64  # 0-128
        
        # Initialize SDL_mixer if not already initialized
        if sdl2.sdlmixer.Mix_OpenedAudio() == 0:
            if sdl2.sdlmixer.Mix_OpenAudio(
                44100,  # freq
                sdl2.AUDIO_S16,  # format
                2,  # channels
                2048  # chunksize
            ) == 0:
                self.audio_ready = True
                print("Audio system initialized")
            else:
                print(f"Warning: Failed to open audio: {sdl2.SDL_GetError()}")
        else:
            self.audio_ready = True
            print("Audio system already initialized")
        
        # Pre-load sounds
        if self.audio_ready:
            self._preload_sounds()
    
    def _preload_sounds(self):
        """Pre-load all sound effects and music."""
        print("Pre-loading sounds...")
        
        # Load sound effects
        self.sounds['coin'] = self._load_sound(COIN_SOUND_PATH)
        self.sounds['life_lost'] = self._load_sound(LIFE_LOST_SOUND_PATH)
        self.sounds['death'] = self._load_sound(DEATH_SOUND_PATH)
        
        # Load music
        self.music['menu'] = self._load_music(MENU_MUSIC_PATH)
        self.music['win'] = self._load_music(WIN_MUSIC_PATH)
        self.music['lose'] = self._load_music(LOSE_MUSIC_PATH)
        self.music['disco'] = self._load_music(DISCO_MUSIC_PATH)
    
    def _load_sound(self, filepath: str) -> Optional[sdl2.sdlmixer.Mix_Chunk]:
        """Load a sound effect (WAV file)."""
        if not self.audio_ready:
            return None
        
        if not os.path.exists(filepath):
            print(f"Warning: Sound file not found: {filepath}")
            return None
        
        sound = sdl2.sdlmixer.Mix_LoadWAV(filepath.encode('utf-8'))
        if sound is None:
            print(f"Warning: Failed to load sound {filepath}: {sdl2.SDL_GetError()}")
        return sound
    
    def _load_music(self, filepath: str) -> Optional[sdl2.sdlmixer.Mix_Music]:
        """Load a music file (MP3, OGG, etc.)."""
        if not self.audio_ready:
            return None
        
        if not os.path.exists(filepath):
            print(f"Warning: Music file not found: {filepath}")
            return None
        
        music = sdl2.sdlmixer.Mix_LoadMUS(filepath.encode('utf-8'))
        if music is None:
            print(f"Warning: Failed to load music {filepath}: {sdl2.SDL_GetError()}")
        return music
    
    def play_sound(self, name: str, loops: int = 0, channel: int = -1) -> Optional[int]:
        """
        Play a sound effect.
        
        Args:
            name: Name of the sound to play
            loops: Number of times to loop (0 = play once, -1 = loop forever)
            channel: Channel to play on (-1 = any available channel)
            
        Returns:
            The channel the sound is playing on, or None if failed
        """
        if not self.audio_ready or not AUDIO_ENABLED:
            return None
        
        sound = self.sounds.get(name)
        if sound is None:
            return None
        
        channel = sdl2.sdlmixer.Mix_PlayChannel(channel, sound, loops)
        if channel == -1:
            print(f"Warning: Failed to play sound {name}: {sdl2.SDL_GetError()}")
            return None
        
        # Set volume for this channel
        sdl2.sdlmixer.Mix_Volume(channel, self.sfx_volume)
        
        return channel
    
    def play_coin(self):
        """Play the coin/goodie collection sound."""
        self.play_sound('coin')
    
    def play_life_lost(self):
        """Play the life lost sound."""
        self.play_sound('life_lost')
    
    def play_death(self):
        """Play the death sound."""
        self.play_sound('death')
    
    def play_menu_music(self, loops: int = -1):
        """Play the menu music."""
        if not self.audio_ready or not AUDIO_ENABLED:
            return
        
        self._stop_music()
        music = self.music.get('menu')
        if music is not None:
            sdl2.sdlmixer.Mix_PlayMusic(music, loops)
            sdl2.sdlmixer.Mix_VolumeMusic(self.music_volume)
            self.menu_music_active = True
    
    def play_win_music(self):
        """Play the win music."""
        if not self.audio_ready or not AUDIO_ENABLED:
            return
        
        self._stop_music()
        music = self.music.get('win')
        if music is not None:
            sdl2.sdlmixer.Mix_PlayMusic(music, 0)  # Play once
            sdl2.sdlmixer.Mix_VolumeMusic(self.music_volume)
            self.menu_music_active = False
    
    def play_lose_music(self):
        """Play the lose music."""
        if not self.audio_ready or not AUDIO_ENABLED:
            return
        
        self._stop_music()
        music = self.music.get('lose')
        if music is not None:
            sdl2.sdlmixer.Mix_PlayMusic(music, 0)  # Play once
            sdl2.sdlmixer.Mix_VolumeMusic(self.music_volume)
            self.menu_music_active = False
    
    def start_disco_music(self) -> bool:
        """Start playing disco music for the easter egg."""
        if not self.audio_ready or not AUDIO_ENABLED:
            return False
        
        self._stop_music()
        music = self.music.get('disco')
        if music is not None:
            result = sdl2.sdlmixer.Mix_PlayMusic(music, -1)  # Loop forever
            sdl2.sdlmixer.Mix_VolumeMusic(self.music_volume)
            self.menu_music_active = False
            return result == 0
        return False
    
    def fade_out_disco_music(self, fade_ms: int = 1000) -> bool:
        """Fade out the disco music."""
        if not self.audio_ready or not AUDIO_ENABLED:
            return False
        
        if sdl2.sdlmixer.Mix_PlayingMusic() == 0:
            return False
        
        return sdl2.sdlmixer.Mix_FadeOutMusic(fade_ms) == 1
    
    def _stop_music(self):
        """Stop currently playing music."""
        if sdl2.sdlmixer.Mix_PlayingMusic() != 0:
            sdl2.sdlmixer.Mix_HaltMusic()
        self.menu_music_active = False
    
    def stop_all(self):
        """Stop all sounds and music."""
        sdl2.sdlmixer.Mix_HaltChannel(-1)  # Halt all channels
        sdl2.sdlmixer.Mix_HaltMusic()
    
    def set_sfx_volume(self, volume: int):
        """Set sound effects volume (0-128)."""
        self.sfx_volume = max(0, min(128, volume))
    
    def set_music_volume(self, volume: int):
        """Set music volume (0-128)."""
        self.music_volume = max(0, min(128, volume))
        if self.audio_ready:
            sdl2.sdlmixer.Mix_VolumeMusic(self.music_volume)
    
    def cleanup(self):
        """Clean up all loaded audio resources."""
        print("Cleaning up audio...")
        
        # Free sounds
        for sound in self.sounds.values():
            if sound is not None:
                sdl2.sdlmixer.Mix_FreeChunk(sound)
        self.sounds.clear()
        
        # Free music
        for music in self.music.values():
            if music is not None:
                sdl2.sdlmixer.Mix_FreeMusic(music)
        self.music.clear()
