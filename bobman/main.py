#!/usr/bin/env python3
"""
BobMan - Main entry point

This is the Python port of the C++ BobMan game using PySDL2.
This file handles the main game loop, menu system, and application lifecycle.
"""

import sys
import sdl2
import sdl2.sdlttf
import sdl2.sdlmixer
from bobman.constants import (
    WINDOW_TITLE,
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    FPS,
    FRAME_DELAY_MS,
    DATA_DIR,
)
from bobman.game import Game
from bobman.renderer import Renderer
from bobman.events import Events
from bobman.audio import Audio
from bobman.map import Map


def init_sdl():
    """Initialize all SDL2 subsystems."""
    print("Initializing SDL2...")
    
    # Initialize SDL
    if sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO | sdl2.SDL_INIT_EVENTS) != 0:
        raise RuntimeError(f"Failed to initialize SDL: {sdl2.SDL_GetError()}")
    
    # Initialize SDL_ttf
    if sdl2.sdlttf.TTF_Init() != 0:
        raise RuntimeError(f"Failed to initialize SDL_ttf: {sdl2.SDL_GetError()}")
    
    # Initialize SDL_mixer
    if sdl2.sdlmixer.Mix_OpenAudio(
        44100,  # freq
        sdl2.AUDIO_S16,  # format
        2,  # channels (stereo)
        2048  # chunksize
    ) != 0:
        print(f"Warning: Failed to initialize SDL_mixer: {sdl2.SDL_GetError()}")
        # Continue without audio
    
    print("SDL2 initialized successfully")


def create_window():
    """Create the main game window."""
    print("Creating window...")
    
    window = sdl2.SDL_CreateWindow(
        WINDOW_TITLE.encode('utf-8'),
        sdl2.SDL_WINDOWPOS_CENTERED,
        sdl2.SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        sdl2.SDL_WINDOW_SHOWN
    )
    
    if window is None:
        raise RuntimeError(f"Failed to create window: {sdl2.SDL_GetError()}")
    
    # Create renderer
    renderer = sdl2.SDL_CreateRenderer(
        window,
        -1,
        sdl2.SDL_RENDERER_ACCELERATED | sdl2.SDL_RENDERER_PRESENTVSYNC
    )
    
    if renderer is None:
        raise RuntimeError(f"Failed to create renderer: {sdl2.SDL_GetError()}")
    
    print(f"Window created: {WINDOW_WIDTH}x{WINDOW_HEIGHT}")
    return window, renderer


def load_map(map_path):
    """Load a game map from file."""
    print(f"Loading map: {map_path}")
    game_map = Map(map_path)
    game_map.load()
    return game_map


def main():
    """Main entry point for the game."""
    print("BobMan starting up...")
    
    try:
        # Initialize SDL2
        init_sdl()
        
        # Create window and renderer
        window, renderer = create_window()
        
        # Load default map
        game_map = load_map(f"{DATA_DIR}/maps/original.txt")
        
        # Create game components
        events = Events()
        audio = Audio()
        game = Game(game_map, events, audio)
        renderer_obj = Renderer(renderer, game_map, game)
        
        print("Game initialized. Starting main loop...")
        
        # Main game loop
        running = True
        while running:
            # Process events
            events.update()
            
            if events.is_quit():
                running = False
                break
            
            # Update game state
            game.update()
            
            # Render
            renderer_obj.render()
            
            # Present
            sdl2.SDL_RenderPresent(renderer)
            
            # Cap frame rate
            sdl2.SDL_Delay(FRAME_DELAY_MS)
        
        print("Game loop ended. Cleaning up...")
        
    except KeyboardInterrupt:
        print("Interrupted by user")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Cleanup SDL
        print("Cleaning up SDL...")
        sdl2.sdlmixer.Mix_CloseAudio()
        sdl2.sdlttf.TTF_Quit()
        sdl2.SDL_Quit()
        print("SDL cleaned up. Goodbye!")


if __name__ == "__main__":
    main()
