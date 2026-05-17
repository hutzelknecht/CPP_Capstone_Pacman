#!/usr/bin/env python3
"""
BobMan - Main entry point

This is the Python port of the C++ BobMan game using PySDL2.
Phase 2: Resource Loading - Full implementation with menu and game states
"""

import sys
import os
import time
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
    MAPS_DIR,
    FONT_PATH,
)
from bobman.game import Game
from bobman.renderer import Renderer
from bobman.events import Events
from bobman.audio import Audio
from bobman.map import Map


class AppState:
    """Application state machine."""
    MENU = 0
    GAME = 1
    EDITOR = 2
    SETTINGS = 3
    QUIT = 4


class App:
    """Main application class."""
    
    def __init__(self):
        """Initialize the application."""
        print("BobMan starting up...")
        
        # Initialize SDL
        self._init_sdl()
        
        # Create window and renderer
        self.window, self.sdl_renderer = self._create_window()
        
        # Initialize subsystems
        self.events = Events()
        self.audio = Audio()
        
        # State
        self.state = AppState.MENU
        self.running = True
        self.game: Optional[Game] = None
        self.renderer: Optional[Renderer] = None
        self.game_map: Optional[Map] = None
        
        # Start menu music
        self.audio.play_menu_music()
        
        print("App initialized")
    
    def _init_sdl(self):
        """Initialize all SDL2 subsystems."""
        print("Initializing SDL2...")
        
        # Initialize SDL
        if sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO | sdl2.SDL_INIT_EVENTS) != 0:
            raise RuntimeError(f"Failed to initialize SDL: {sdl2.SDL_GetError()}")
        
        # Initialize SDL_ttf
        if sdl2.sdlttf.TTF_Init() != 0:
            raise RuntimeError(f"Failed to initialize SDL_ttf: {sdl2.SDL_GetError()}")
        
        # SDL_mixer is initialized by Audio class
        
        print("SDL2 initialized successfully")
    
    def _create_window(self):
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
    
    def load_map(self, map_path: str) -> Optional[Map]:
        """Load a game map from file."""
        print(f"Loading map: {map_path}")
        game_map = Map(map_path)
        if game_map.load():
            return game_map
        return None
    
    def start_game(self, map_path: str):
        """Start a new game with the specified map."""
        print(f"Starting game with map: {map_path}")
        
        # Load map
        self.game_map = self.load_map(map_path)
        if self.game_map is None:
            print("Failed to load map")
            return
        
        # Create game
        self.game = Game(self.game_map, self.events, self.audio)
        
        # Create renderer
        self.renderer = Renderer(self.sdl_renderer, self.game_map, self.game)
        
        # Start the game
        self.game.start()
        
        # Stop menu music
        self.audio.stop_all()
        
        # Change state
        self.state = AppState.GAME
        
        print("Game started")
    
    def run(self):
        """Main application loop."""
        print("Starting main loop...")
        
        try:
            while self.running:
                # Process events
                self._process_events()
                
                # Update
                self._update()
                
                # Render
                self._render()
                
                # Present
                sdl2.SDL_RenderPresent(self.sdl_renderer)
                
                # Cap frame rate
                sdl2.SDL_Delay(FRAME_DELAY_MS)
            
        except KeyboardInterrupt:
            print("Interrupted by user")
        except Exception as e:
            print(f"Error: {e}")
            import traceback
            traceback.print_exc()
        finally:
            self._cleanup()
    
    def _process_events(self):
        """Process SDL events."""
        self.events.update()
        
        if self.events.is_quit():
            self.running = False
            return
        
        # Handle state-specific events
        if self.state == AppState.MENU:
            self._process_menu_events()
        elif self.state == AppState.GAME:
            self._process_game_events()
    
    def _process_menu_events(self):
        """Process events while in menu state."""
        if self.events.consume_confirm_request():
            # Start game with first available map
            maps = Map.get_available_maps()
            if maps:
                self.start_game(maps[0][1])  # First map's file path
        
        if self.events.consume_exit_dialog_request():
            self.running = False
    
    def _process_game_events(self):
        """Process events while in game state."""
        if self.events.consume_exit_dialog_request():
            # Return to menu
            self._return_to_menu()
        
        if self.events.consume_pause_toggle_request():
            if self.game is not None:
                if self.game.is_paused:
                    self.game.resume()
                else:
                    self.game.pause()
        
        # Handle disco test (debug)
        if self.events.consume_disco_test_request():
            if self.game is not None:
                print("Disco test requested")
    
    def _return_to_menu(self):
        """Return to the main menu."""
        print("Returning to menu...")
        
        # Clean up game
        if self.game is not None:
            self.game = None
        
        if self.renderer is not None:
            self.renderer.cleanup()
            self.renderer = None
        
        if self.game_map is not None:
            self.game_map = None
        
        # Resume menu music
        self.audio.play_menu_music()
        
        # Change state
        self.state = AppState.MENU
        
        print("Back to menu")
    
    def _update(self):
        """Update application state."""
        if self.state == AppState.GAME and self.game is not None:
            self.game.update()
    
    def _render(self):
        """Render the current state."""
        if self.state == AppState.GAME and self.renderer is not None:
            self.renderer.render()
        elif self.state == AppState.MENU:
            # Render menu directly (no renderer yet)
            self._render_menu()
    
    def _render_menu(self):
        """Render the main menu without a Renderer."""
        # Clear screen
        sdl2.SDL_SetRenderDrawColor(self.sdl_renderer, 0, 0, 0, 255)
        sdl2.SDL_RenderClear(self.sdl_renderer)
        
        # For now, just show a simple message
        # (Full menu rendering will be implemented in Phase 7)
        self._render_simple_text("BOBMAN - Press ENTER to start", 
                                 WINDOW_WIDTH // 2 - 150, 
                                 WINDOW_HEIGHT // 2)
    
    def _render_simple_text(self, text: str, x: int, y: int, size: int = 36):
        """Render simple text using a temporary font."""
        # Try to load a font
        font_path = FONT_PATH
        if not os.path.exists(font_path):
            # Fallback to any TTF font
            font_files = []
            for root, dirs, files in os.walk(DATA_DIR):
                for f in files:
                    if f.lower().endswith('.ttf'):
                        font_files.append(os.path.join(root, f))
            if font_files:
                font_path = font_files[0]
        
        font = sdl2.sdlttf.TTF_OpenFont(font_path.encode('utf-8'), size)
        if font is None:
            # Can't render text without font
            return
        
        surface = sdl2.sdlttf.TTF_RenderText_Solid(
            font,
            text.encode('utf-8'),
            sdl2.SDL_Color(255, 255, 255, 255)
        )
        if surface is None:
            sdl2.sdlttf.TTF_CloseFont(font)
            return
        
        texture = sdl2.SDL_CreateTextureFromSurface(self.sdl_renderer, surface)
        sdl2.SDL_FreeSurface(surface)
        sdl2.sdlttf.TTF_CloseFont(font)
        
        if texture is not None:
            rect = sdl2.SDL_Rect(x, y, surface.w, surface.h)
            sdl2.SDL_RenderCopy(self.sdl_renderer, texture, None, rect)
            sdl2.SDL_DestroyTexture(texture)
    
    def _cleanup(self):
        """Clean up all resources."""
        print("Cleaning up...")
        
        # Clean up renderer
        if self.renderer is not None:
            self.renderer.cleanup()
            self.renderer = None
        
        # Clean up audio
        if self.audio is not None:
            self.audio.cleanup()
            self.audio = None
        
        # Destroy renderer
        if self.sdl_renderer is not None:
            sdl2.SDL_DestroyRenderer(self.sdl_renderer)
            self.sdl_renderer = None
        
        # Destroy window
        if self.window is not None:
            sdl2.SDL_DestroyWindow(self.window)
            self.window = None
        
        # Quit SDL
        sdl2.sdlmixer.Mix_CloseAudio()
        sdl2.sdlttf.TTF_Quit()
        sdl2.SDL_Quit()
        
        print("Cleanup complete. Goodbye!")


def main():
    """Main entry point for the game."""
    try:
        app = App()
        app.run()
    except Exception as e:
        print(f"Fatal error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
