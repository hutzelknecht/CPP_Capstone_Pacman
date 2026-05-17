"""
Renderer module - Handles all drawing and rendering.

Ported from src/renderer.cpp and src/renderer.h
Phase 2: Resource Loading - Full implementation
"""

import os
from typing import TYPE_CHECKING, Optional, List, Tuple, Dict
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
    FLOOR_TEXTURES_DIR,
    PACMAN_FRAMES_DIR,
    MONSTER_FRAMES_DIR,
    Colors,
    FLOOR_TEXTURE_COUNT,
    PACMAN_SPRITE_PATH,
    MONSTER_SPRITE_PATH,
    GOODIE_SPRITE_PATH,
    BRICK_TEXTURE_PATH,
    TILE_TEXTURE_PATH,
    FONT_PATH,
    WALL,
    WALL_ALT,
    PATH,
    PACMAN_START,
    MONSTER_STANDARD,
    MONSTER_GAS,
    MONSTER_FIRE,
    MONSTER_GOAT,
    MONSTER_ALIEN,
    GOODIE,
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
        
        # Texture cache: name -> SDL_Texture
        self.textures: Dict[str, sdl2.SDL_Texture] = {}
        
        # Font cache: name -> TTF_Font
        self.fonts: Dict[str, sdl2.sdlttf.TTF_Font] = {}
        
        # Camera
        self.camera_x = 0
        self.camera_y = 0
        
        # Viewport
        self.viewport = sdl2.SDL_Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT)
        
        # Initialize resources
        self._load_textures()
        self._load_fonts()
        
        print("Renderer initialized")
    
    def _load_textures(self):
        """Load all game textures."""
        print("Loading textures...")
        
        # Load core sprites
        self._load_texture("pacman", PACMAN_SPRITE_PATH)
        self._load_texture("monsters", MONSTER_SPRITE_PATH)
        self._load_texture("goodie", GOODIE_SPRITE_PATH)
        self._load_texture("brick", BRICK_TEXTURE_PATH)
        self._load_texture("tile", TILE_TEXTURE_PATH)
        
        # Load floor textures
        for i in range(1, FLOOR_TEXTURE_COUNT + 1):
            self._load_texture(f"floor_{i}", os.path.join(FLOOR_TEXTURES_DIR, f"floor_texture_{i}.jpg"))
        
        # Load Pacman individual frames
        self._load_pacman_frames()
        
        print(f"Loaded {len(self.textures)} textures")
    
    def _load_pacman_frames(self):
        """Load individual Pacman animation frames."""
        directions = ['up', 'down', 'left', 'right']
        for direction in directions:
            for frame in range(4):
                filename = f"{direction}_{frame}.png"
                filepath = os.path.join(PACMAN_FRAMES_DIR, filename)
                self._load_texture(f"pacman_{direction}_{frame}", filepath)
    
    def _load_texture(self, name: str, filepath: str) -> bool:
        """
        Load a single texture from file.
        
        Args:
            name: Internal name for the texture
            filepath: Path to the texture file
            
        Returns:
            True if loaded successfully, False otherwise
        """
        if not os.path.exists(filepath):
            print(f"Warning: Texture not found: {filepath}")
            return False
        
        # Load surface
        surface = sdl2.sdlimage.IMG_Load(filepath.encode('utf-8'))
        if surface is None:
            print(f"Warning: Failed to load image {filepath}: {sdl2.SDL_GetError()}")
            return False
        
        # Get surface dimensions before freeing
        surf_w = surface.contents.w if hasattr(surface, 'contents') else (surface.w if hasattr(surface, 'w') else 0)
        surf_h = surface.contents.h if hasattr(surface, 'contents') else (surface.h if hasattr(surface, 'h') else 0)
        
        # Create texture from surface
        texture = sdl2.SDL_CreateTextureFromSurface(self.sdl_renderer, surface)
        sdl2.SDL_FreeSurface(surface)
        
        if texture is None:
            print(f"Warning: Failed to create texture from {filepath}: {sdl2.SDL_GetError()}")
            return False
        
        # Enable alpha blending for transparent textures
        sdl2.SDL_SetTextureBlendMode(texture, sdl2.SDL_BLENDMODE_BLEND)
        
        self.textures[name] = texture
        print(f"  Loaded texture: {name} ({surf_w}x{surf_h})")
        return True
    
    def get_texture(self, name: str) -> Optional[sdl2.SDL_Texture]:
        """Get a texture by name."""
        return self.textures.get(name)
    
    def _load_fonts(self):
        """Load all fonts."""
        print("Loading fonts...")
        
        # Load fonts at different sizes
        self.fonts['small'] = self._load_font(FONT_PATH, 12)
        self.fonts['medium'] = self._load_font(FONT_PATH, 24)
        self.fonts['large'] = self._load_font(FONT_PATH, 48)
        self.fonts['title'] = self._load_font(FONT_PATH, 72)
        
        # Count loaded fonts
        loaded = sum(1 for f in self.fonts.values() if f is not None)
        print(f"Loaded {loaded} fonts")
    
    def _load_font(self, filepath: str, size: int) -> Optional[sdl2.sdlttf.TTF_Font]:
        """Load a font at a specific size."""
        if not os.path.exists(filepath):
            print(f"Warning: Font file not found: {filepath}")
            return None
        
        font = sdl2.sdlttf.TTF_OpenFont(filepath.encode('utf-8'), size)
        if font is None:
            print(f"Warning: Failed to load font {filepath} at size {size}: {sdl2.SDL_GetError()}")
        return font
    
    def get_font(self, name: str) -> Optional[sdl2.sdlttf.TTF_Font]:
        """Get a font by name."""
        return self.fonts.get(name)
    
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
        self.clear_screen()
        
        if self.game.is_disco_easter_egg_active:
            self._render_disco_easter_egg()
        elif self.game.is_running:
            self._render_game()
        else:
            self._render_menu()
    
    def clear_screen(self, color: Tuple[int, int, int, int] = Colors.BLACK):
        """Clear the screen with a color."""
        r, g, b, a = color
        sdl2.SDL_SetRenderDrawColor(self.sdl_renderer, r, g, b, a)
        sdl2.SDL_RenderClear(self.sdl_renderer)
    
    def _render_game(self):
        """Render the game scene."""
        # Render map background
        self._render_map()
        
        # Render entities would go here
        # self._render_entities()
        
        # Render HUD
        self._render_hud()
    
    def _render_map(self):
        """Render the map tiles and walls."""
        if self.map is None:
            return
        
        # Get brick texture for walls
        brick_texture = self.textures.get("brick")
        tile_texture = self.textures.get("tile")
        goodie_texture = self.textures.get("goodie")
        
        # Get texture dimensions
        brick_w, brick_h = self._get_texture_size(brick_texture)
        tile_w, tile_h = self._get_texture_size(tile_texture)
        
        for row in range(self.map.rows):
            for col in range(self.map.cols):
                cell = self.map.get_cell(self.map.grid[row][col])
                
                # Calculate screen position
                x = col * TILE_SIZE
                y = row * TILE_SIZE
                
                # Create destination rectangle
                dst = sdl2.SDL_Rect(x, y, TILE_SIZE, TILE_SIZE)
                
                # Render based on cell type
                if cell in ['x', WALL_ALT]:  # Wall (including alternate wall char)
                    if brick_texture:
                        src = sdl2.SDL_Rect(0, 0, min(brick_w, TILE_SIZE), min(brick_h, TILE_SIZE))
                        self.draw_texture(brick_texture, src, dst)
                    else:
                        # Fallback: draw colored rectangle
                        self.draw_rect(dst, Colors.GRAY)
                elif cell == '.':  # Path
                    if tile_texture:
                        src = sdl2.SDL_Rect(0, 0, min(tile_w, TILE_SIZE), min(tile_h, TILE_SIZE))
                        self.draw_texture(tile_texture, src, dst)
                    else:
                        self.draw_rect(dst, Colors.DARK_GRAY)
                elif cell == 'G':  # Goodie
                    if goodie_texture:
                        goodie_w, goodie_h = self._get_texture_size(goodie_texture)
                        src = sdl2.SDL_Rect(0, 0, goodie_w, goodie_h)
                        # Center goodie in cell
                        dst_g = sdl2.SDL_Rect(
                            x + (TILE_SIZE - goodie_w) // 2,
                            y + (TILE_SIZE - goodie_h) // 2,
                            goodie_w, goodie_h
                        )
                        self.draw_texture(goodie_texture, src, dst_g)
                    else:
                        # Fallback: draw yellow circle
                        inner = sdl2.SDL_Rect(x + 8, y + 8, TILE_SIZE - 16, TILE_SIZE - 16)
                        self.draw_rect(inner, (255, 255, 0, 255))
                elif cell == 'P':  # Pacman start
                    self.draw_rect(dst, Colors.YELLOW)
                elif cell in ['M', 'N', 'O', 'K', 'A']:  # Monsters
                    self.draw_rect(dst, Colors.RED)
                elif cell in ['1', '2', '3', '4', '5']:  # Teleporters
                    self.draw_rect(dst, Colors.CYAN)
                else:
                    # Empty/path
                    self.draw_rect(dst, Colors.DARK_GRAY)
    
    def _get_texture_size(self, texture: Optional[sdl2.SDL_Texture]) -> Tuple[int, int]:
        """Get the width and height of a texture."""
        if texture is None:
            return 0, 0
        
        w = sdl2.c_int(0)
        h = sdl2.c_int(0)
        sdl2.SDL_QueryTexture(texture, None, None, w, h)
        return w.value, h.value
    
    def _render_hud(self):
        """Render the HUD (score, lives, etc.)."""
        # Draw score
        if self.game is not None:
            score_text = f"Score: {self.game.score}"
            self.draw_text(score_text, 10, 10, font_name='medium')
            
            # Draw lives
            lives_text = f"Lives: {self.game.remaining_lives}"
            self.draw_text(lives_text, 10, 40, font_name='medium')
    
    def _render_menu(self):
        """Render the main menu."""
        # Draw title
        self.draw_text("BOBMAN", 
                      WINDOW_WIDTH // 2 - 100, 
                      WINDOW_HEIGHT // 4,
                      font_name='title',
                      color=Colors.YELLOW)
        
        # Draw menu items
        y = WINDOW_HEIGHT // 2
        menu_items = [
            ("Start Game", 'medium'),
            ("Map Selection", 'medium'),
            ("Settings", 'medium'),
            ("Map Editor", 'medium'),
            ("Quit", 'medium'),
        ]
        
        for text, font in menu_items:
            self.draw_text(text, WINDOW_WIDTH // 2 - 50, y, font_name=font)
            y += 40
    
    def _render_disco_easter_egg(self):
        """Render the disco easter egg scene."""
        # TODO: Implement disco ball rendering
        self.draw_text("DISCO MODE!", 
                      WINDOW_WIDTH // 2 - 100, 
                      WINDOW_HEIGHT // 2,
                      font_name='large',
                      color=Colors.MAGENTA)
    
    def draw_texture(self, texture: sdl2.SDL_Texture, 
                    src_rect: Optional[sdl2.SDL_Rect] = None, 
                    dst_rect: Optional[sdl2.SDL_Rect] = None,
                    angle: float = 0.0, 
                    center: Optional[sdl2.SDL_Point] = None,
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
        """
        Render text to the screen.
        
        Args:
            text: Text to render
            x: X position
            y: Y position
            font_name: Name of font to use ('small', 'medium', 'large', 'title')
            color: Text color as (R, G, B) tuple
        """
        font = self.fonts.get(font_name)
        if font is None:
            print(f"Warning: Font '{font_name}' not loaded")
            return
        
        r, g, b = color
        surface = sdl2.sdlttf.TTF_RenderText_Solid(
            font,
            text.encode('utf-8'),
            sdl2.SDL_Color(r, g, b, 255)
        )
        if surface is None:
            print(f"Warning: Failed to render text '{text}': {sdl2.SDL_GetError()}")
            return
        
        texture = sdl2.SDL_CreateTextureFromSurface(self.sdl_renderer, surface)
        sdl2.SDL_FreeSurface(surface)
        
        if texture is None:
            print(f"Warning: Failed to create texture from text: {sdl2.SDL_GetError()}")
            return
        
        surf_w = surface.contents.w if hasattr(surface, 'contents') else (surface.w if hasattr(surface, 'w') else 0)
        surf_h = surface.contents.h if hasattr(surface, 'contents') else (surface.h if hasattr(surface, 'h') else 0)
        rect = sdl2.SDL_Rect(x, y, surf_w, surf_h)
        sdl2.SDL_RenderCopy(self.sdl_renderer, texture, None, rect)
        sdl2.SDL_DestroyTexture(texture)
    
    def cleanup(self):
        """Clean up all loaded resources."""
        print("Cleaning up renderer...")
        
        # Destroy textures
        for name, texture in self.textures.items():
            if texture is not None:
                sdl2.SDL_DestroyTexture(texture)
        self.textures.clear()
        
        # Close fonts
        for name, font in self.fonts.items():
            if font is not None:
                sdl2.sdlttf.TTF_CloseFont(font)
        self.fonts.clear()
