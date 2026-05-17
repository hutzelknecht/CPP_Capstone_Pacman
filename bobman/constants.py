"""
Constants and configuration values ported from definitions.h

This module contains all the game constants, paths, colors, and feature flags.
"""

import os
from enum import Enum
from typing import NamedTuple


# =============================================================================
# PATHS
# =============================================================================

# Base data directory
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_DIR = os.path.join(BASE_DIR, "data")
MAPS_DIR = os.path.join(DATA_DIR, "maps")
FLOOR_TEXTURES_DIR = os.path.join(DATA_DIR, "floor_textures")
PACMAN_FRAMES_DIR = os.path.join(DATA_DIR, "pacman_frames")
MONSTER_FRAMES_DIR = os.path.join(DATA_DIR, "monster_frames")

# Font paths
FONT_PATH = os.path.join(DATA_DIR, "font.ttf")

# Texture paths
PACMAN_SPRITE_PATH = os.path.join(DATA_DIR, "pacman.bmp")
MONSTER_SPRITE_PATH = os.path.join(DATA_DIR, "monster.bmp")
GOODIE_SPRITE_PATH = os.path.join(DATA_DIR, "goodie.bmp")
BRICK_TEXTURE_PATH = os.path.join(DATA_DIR, "brick.bmp")
TILE_TEXTURE_PATH = os.path.join(DATA_DIR, "tile.bmp")

# Audio paths
AUDIO_ENABLED = True  # Set to False to disable all audio

MENU_MUSIC_PATH = os.path.join(DATA_DIR, "menu_music.mp3")
WIN_MUSIC_PATH = os.path.join(DATA_DIR, "win_melody.mp3")
LOSE_MUSIC_PATH = os.path.join(DATA_DIR, "lose_melody.mp3")
DISCO_MUSIC_PATH = os.path.join(DATA_DIR, "disco_easteregg.mp3")

# Sound effect paths
COIN_SOUND_PATH = os.path.join(DATA_DIR, "punch.mp3")
LIFE_LOST_SOUND_PATH = os.path.join(DATA_DIR, "monsterexplosion.mp3")
DEATH_SOUND_PATH = os.path.join(DATA_DIR, "monsterexplosion.mp3")

# Map paths
DEFAULT_MAP = os.path.join(MAPS_DIR, "original.txt")


# =============================================================================
# DISPLAY / RENDERING
# =============================================================================

# Window settings
WINDOW_TITLE = "BobMan"
WINDOW_WIDTH = 1280
WINDOW_HEIGHT = 720

# Rendering
FPS = 60
FRAME_DELAY_MS = 1000 // FPS

# Tile size in pixels
TILE_SIZE = 32

# Grid dimensions
MAP_COLS = 28
MAP_ROWS = 31

# Floor texture
FLOOR_TEXTURE_COUNT = 4


# =============================================================================
# COLORS (RGB)
# =============================================================================

class Colors:
    """Color constants in RGB format (0-255)."""
    BLACK = (0, 0, 0)
    WHITE = (255, 255, 255)
    RED = (255, 0, 0)
    GREEN = (0, 255, 0)
    BLUE = (0, 0, 255)
    YELLOW = (255, 255, 0)
    CYAN = (0, 255, 255)
    MAGENTA = (255, 0, 255)
    ORANGE = (255, 165, 0)
    GRAY = (128, 128, 128)
    DARK_GRAY = (64, 64, 64)
    LIGHT_GRAY = (192, 192, 192)


# =============================================================================
# DIRECTIONS
# =============================================================================

class Directions(Enum):
    """Movement directions for Pacman and monsters."""
    NONE = 0
    Up = 1
    Down = 2
    Left = 3
    Right = 4


# =============================================================================
# MAP CHARACTERS
# =============================================================================

# Wall
WALL = "x"
WALL_ALT = "z"  # Alternative wall character

# Path
PATH = "."

# Teleporters (paired: 1<->1, 2<->2, etc.)
TELEPORT_1 = "1"
TELEPORT_2 = "2"
TELEPORT_3 = "3"
TELEPORT_4 = "4"
TELEPORT_5 = "5"

# Pacman start
PACMAN_START = "P"

# Monsters
MONSTER_STANDARD = "M"
MONSTER_GAS = "N"
MONSTER_FIRE = "O"
MONSTER_GOAT = "K"
MONSTER_ALIEN = "A"

# Goodies
GOODIE = "G"

# Special pickups
INVULNERABILITY_POTION = "I"
DYNAMITE = "D"
PLASTIC_EXPLOSIVE = "E"
WALKIE_TALKIE = "W"
ROCKET = "R"
BIOHAZARD = "B"
NUKE = "U"
LOVE_POTION = "L"
LIFE_PICKUP = "H"
DISCO_PICKUP = "Q"


# =============================================================================
# MAP COORDINATE
# =============================================================================

class MapCoord(NamedTuple):
    """A coordinate on the game map (row, column)."""
    u: int  # row
    v: int  # column


# =============================================================================
# ENTITY TYPES
# =============================================================================

class ExtraSlot(Enum):
    """Inventory slots for special items."""
    NONE = 0
    Dynamite = 1
    PlasticExplosive = 2
    WalkieTalkie = 3
    Rocket = 4
    Biohazard = 5
    NuclearBomb = 6
    LovePotion = 7


# =============================================================================
# GAME TIMING (milliseconds)
# =============================================================================

# Pacman
PACMAN_MOVEMENT_DELAY_MS = 100
PACMAN_DEATH_ANIMATION_MS = 2000

# Monsters
MONSTER_MOVEMENT_DELAY_MS = 200
GOAT_MOVEMENT_DELAY_MS = 400

# Invulnerability
INVULNERABILITY_DURATION_MS = 10000

# Pickup spawning
DYNAMITE_SPAWN_MIN_MS = 15000
DYNAMITE_SPAWN_MAX_MS = 25000
PLASTIC_EXPLOSIVE_SPAWN_MIN_MS = 20000
PLASTIC_EXPLOSIVE_SPAWN_MAX_MS = 35000
ROCKET_SPAWN_MIN_MS = 25000
ROCKET_SPAWN_MAX_MS = 40000
BIOHAZARD_SPAWN_MIN_MS = 30000
BIOHAZARD_SPAWN_MAX_MS = 45000
NUKE_SPAWN_MIN_MS = 40000
NUKE_SPAWN_MAX_MS = 60000
LOVE_POTION_SPAWN_MIN_MS = 25000
LOVE_POTION_SPAWN_MAX_MS = 40000
LIFE_PICKUP_SPAWN_MIN_MS = 30000
LIFE_PICKUP_SPAWN_MAX_MS = 50000
INVULNERABILITY_SPAWN_MIN_MS = 20000
INVULNERABILITY_SPAWN_MAX_MS = 35000
WALKIE_TALKIE_SPAWN_MIN_MS = 20000
WALKIE_TALKIE_SPAWN_MAX_MS = 35000

# Disco pickup
DISCO_PICKUP_SPAWN_MIN_MS = 60000
DISCO_PICKUP_SPAWN_MAX_MS = 90000
DISCO_PICKUP_VISIBLE_MS = 5000
DISCO_PICKUP_FADE_MS = 2000
DISCO_PICKUP_RETRY_DELAY_MS = 5000

# Disco easter egg
DISCO_EASTER_EGG_DURATION_MS = 30000
DISCO_EASTER_EGG_FADE_OUT_MS = 2000
DISCO_ROTATION_PERIOD_MS = 5000.0
DISCO_ROTATION_SPEED_MIN = 0.5
DISCO_ROTATION_SPEED_MAX = 5.0
DISCO_ROTATION_SPEED_STEP = 0.25
DISCO_EASTER_EGG_ANIMATION_SEED_MODULUS = 1000000
DISCO_EASTER_EGG_ANIMATION_SEED_ROW_MULTIPLIER = 1000
DISCO_EASTER_EGG_ANIMATION_SEED_COL_MULTIPLIER = 100

# Nuclear
NUKE_FREEZE_DURATION_MS = 3000
NUKE_EXPLOSION_DURATION_MS = 5000

# Rocket
ROCKET_LIFETIME_MS = 5000
ROCKET_SPEED = 0.5


# =============================================================================
# DIFFICULTY
# =============================================================================

class Difficulty(Enum):
    """Game difficulty levels."""
    Few = 0
    Medium = 1
    Many = 2


# Difficulty tuning values
DIFFICULTY_TUNING = {
    Difficulty.Few: {
        "monster_count": 2,
        "extra_spawn_interval_scale": 1.5,
    },
    Difficulty.Medium: {
        "monster_count": 4,
        "extra_spawn_interval_scale": 1.0,
    },
    Difficulty.Many: {
        "monster_count": 6,
        "extra_spawn_interval_scale": 0.7,
    },
}


def get_difficulty_tuning(difficulty: Difficulty) -> dict:
    """Get tuning parameters for a difficulty level."""
    return DIFFICULTY_TUNING.get(difficulty, DIFFICULTY_TUNING[Difficulty.Medium])
