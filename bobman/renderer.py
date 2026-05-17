"""
Renderer module - Handles all drawing and rendering.

Ported from src/renderer.cpp and src/renderer.h
"""

import os
from typing import TYPE_CHECKING, Optional, List, Tuple
import sdl2
import sdl2.sdlimage
import sdl2.sdlttf

if TYPE_CHECKING:
    from bobman.game import Game
    from bobman.map import Map

from bobman.constants import (
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    TILE_SIZE,
    DATA_DIR,
    Colors,
    FLOOR_TEXTURE_COUNT,
)


class Renderer:
    """
    Handles all rendering for the game.
    
    This class manages:
    - SDL2 renderer
    - Texture loading and caching
    - Sprite rendering
    - HUD/overlay rendering
    - Camera/viewport management
    """
    
    def __init__(self, sdl_renderer, game_map: "Map", game: "Game"):
        """
        Initialize the renderer.
        
        Args:
            sdl_renderer: The SDL_Renderer pointer
            game_map: The game map
            game: The game instance
        """
        self.sdl_renderer = sdl_renderer
        self.map = game_map
        self.game = game
        
        # Texture cache
        self.textures = {}
        self.fonts = {}
        
        # Camera
        self.camera_x = 0
        self.camera_y = 0
        
        # Initialize
        self._load_textures()
        self._load_fonts()
        
        print("Renderer initialized")
    
    def _load_textures(self):
        """Load all game textures."""
        print("Loading textures...")
        
        # Load sprite sheets
        # TODO: self.textures['pacman'] = self._load_texture('pacman.png')
        # TODO: self.textures['monsters'] = self._load_texture('monsters.png')
        # TODO: self.textures['walls'] = self._load_texture('walls.png')
        
        # Load floor textures
        for i in range(FLOOR_TEXTURE_COUNT):
            # TODO: self.textures[f'floor_{i}'] = self._load_texture(f'floor_{i}.png')
            pass
    
    def _load_texture(self, filename: str) -> Optional[sdl2.SDL_Texture]:
        """Load a single texture from file."""
        filepath = os.path.join(DATA_DIR, filename)
        if not os.path.exists(filepath):
            print(f"Warning: Texture not found: {filepath}")
            return None
        
        # Load surface
        surface = sdl2.sdlimage.IMG_Load(filepath.encode('utf-8'))
        if surface is None:
            print(f"Warning: Failed to load image {filepath}: {sdl2.SDL_GetError()}")
            return None
        
        # Create texture from surface
        texture = sdl2.SDL_CreateTextureFromSurface(self.sdl_renderer, surface)
        sdl2.SDL_FreeSurface(surface)
        
        if texture is None:
            print(f"Warning: Failed to create texture from {filepath}: {sdl2.SDL_GetError()}")
            return None
        
        # Enable alpha blending for transparent textures
        sdl2.SDL_SetTextureBlendMode(texture, sdl2.SDL_BLENDMODE_BLEND)
        
        return texture
    
    def _load_fonts(self):
        """Load all fonts."""
        print("Loading fonts...")
        
        # TODO: Load fonts at different sizes
        # self.fonts['small'] = self._load_font('DejaVuSans.ttf', 12)
        # self.fonts['medium'] = self._load_font('DejaVuSans.ttf', 24)
        # self.fonts['large'] = self._load_font('DejaVuSans.ttf', 48)
    
    def _load_font(self, filename: str, size: int) -> Optional[sdl2.sdlttf.TTF_Font]:
        """Load a font at a specific size."""
        filepath = os.path.join(DATA_DIR, filename)
        font = sdl2.sdlttf.TTF_OpenFont(filepath.encode('utf-8'), size)
        if font is None:
            print(f"Warning: Failed to load font {filepath} at size {size}: {sdl2.SDL_GetError()}")
        return font
    
    def render(self):
        """
        Render the entire game frame.
        
        This calls the appropriate render methods based on game state:
        - Menu
        - Game
        - Editor
        - etc.
        """
        # Clear screen
        sdl2.SDL_SetRenderDrawColor(self.sdl_renderer, 0, 0, 0, 255)
        sdl2.SDL_RenderClear(self.sdl_renderer)
        
        if self.game.is_disco_easter_egg_active:
            self._render_disco_easter_egg()
        elif self.game.is_running:
            self._render_game()
        else:
            self._render_menu()
    
    def _render_game(self):
        """Render the game scene."""
        # TODO: Implement
        # 1. Render map background
        # 2. Render walls
        # 3. Render goodies
        # 4. Render pickups
        # 5. Render entities (Pacman, monsters)
        # 6. Render effects (explosions, etc.)
        # 7. Render HUD
        pass
    
    def _render_menu(self):
        """Render the main menu."""
        # TODO: Implement
        pass
    
    def _render_disco_easter_egg(self):
        """Render the disco easter egg scene."""
        # TODO: Implement
        pass
    
    def draw_texture(self, texture, src_rect: Optional[sdl2.SDL_Rect] = None, 
                    dst_rect: Optional[sdl2.SDL_Rect] = None,
                    angle: float = 0.0, center: Optional[sdl2.SDL_Point] = None,
                    flip: int = sdl2.SDL_FLIP_NONE):
        """
        Draw a texture to the renderer.
        
        Args:
            texture: The texture to draw
            src_rect: Source rectangle (None for entire texture)
            dst_rect: Destination rectangle
            angle: Rotation angle in degrees
            center: Center point for rotation
            flip: Flip mode (SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL)
        """
        sdl2.SDL_RenderCopyEx(
            self.sdl_renderer,
            texture,
            src_rect,
            dst_rect,
            angle,
            center,
            flip
        )
    
    def draw_rect(self, rect: sdl2.SDL_Rect, color: Tuple[int, int, int, int]):
        """Draw a filled rectangle."""
        r, g, b, a = color
        sdl2.SDL_SetRenderDrawColor(self.sdl_renderer, r, g, b, a)
        sdl2.SDL_RenderFillRect(self.sdl_renderer, rect)
    
    def draw_text(self, text: str, x: int, y: int, font_name: str = 'medium',
                  color: Tuple[int, int, int] = Colors.WHITE):
        """Render text to the screen."""
        font = self.fonts.get(font_name)
        if font is None:
            return
        
        r, g, b = color
        surface = sdl2.sdlttf.TTF_RenderText_Solid(
            font,
            text.encode('utf-8'),
            sdl2.SDL_Color(r, g, b, 255)
        )
        if surface is None:
            return
        
        texture = sdl2.SDL_CreateTextureFromSurface(self.sdl_renderer, surface)
        sdl2.SDL_FreeSurface(surface)
        
        if texture is None:
            return
        
        rect = sdl2.SDL_Rect(x, y, surface.w, surface.h)
        sdl2.SDL_RenderCopy(self.sdl_renderer, texture, None, rect)
        sdl2.SDL_DestroyTexture(texture)
    
    def cleanup(self):
        """Clean up all loaded resources."""
        print("Cleaning up renderer...")
        
        # Destroy textures
        for texture in self.textures.values():
            if texture is not None:
                sdl2.SDL_DestroyTexture(texture)
        self.textures.clear()
        
        # Close fonts
        for font in self.fonts.values():
            if font is not None:
                sdl2.sdlttf.TTF_CloseFont(font)
        self.fonts.clear()
