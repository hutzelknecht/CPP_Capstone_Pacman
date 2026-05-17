"""
Game module - Core game logic, state management, and collision handling.

Ported from src/game.cpp and src/game.h
"""

import time
from typing import TYPE_CHECKING, Optional
from bobman.constants import (
    MapCoord,
    Directions,
    Difficulty,
    get_difficulty_tuning,
    PACMAN_MOVEMENT_DELAY_MS,
    MONSTER_MOVEMENT_DELAY_MS,
)

if TYPE_CHECKING:
    from bobman.map import Map
    from bobman.events import Events
    from bobman.audio import Audio


class Game:
    """
    Main game class containing game state, entities, and logic.
    
    This class manages:
    - Pacman and monster entities
    - Game state (running, paused, won, lost)
    - Collision detection
    - Pickup spawning and collection
    - Score tracking
    """
    
    def __init__(self, game_map: "Map", events: "Events", audio: "Audio", 
                 difficulty: Difficulty = Difficulty.Medium,
                 lives: int = 3):
        """
        Initialize the game.
        
        Args:
            game_map: The game map
            events: The events handler
            audio: The audio handler
            difficulty: Game difficulty level
            lives: Starting number of lives
        """
        self.map = game_map
        self.events = events
        self.audio = audio
        self.difficulty = difficulty
        self.tuning = get_difficulty_tuning(difficulty)
        
        # Initialize game state
        self.running = False
        self.paused = False
        self.won = False
        self.lost = False
        self.score = 0
        self.remaining_lives = lives
        self.current_lives = lives
        
        # Timing
        self.last_update_time = time.time() * 1000  # in ms
        self.start_time = self.last_update_time
        
        # Initialize entities
        self.pacman = None  # Will be created in start()
        self.monsters = []
        self.goodies = []
        
        # Pickups
        self.disco_pickup = None
        self.active_disco_easteregg = None
        
        print("Game initialized")
    
    def start(self):
        """Start the game simulation."""
        print("Starting game...")
        self.running = True
        self.paused = False
        self.won = False
        self.lost = False
        self.score = 0
        self.remaining_lives = self.current_lives
        
        # Create Pacman at start position
        start_coord = self.map.get_coord_pacman()
        # self.pacman = Pacman(start_coord)  # TODO: Implement Pacman class
        
        # Create monsters
        for i in range(self.map.get_number_monsters()):
            monster_coord = self.map.get_coord_monster(i)
            monster_char = self.map.get_char_monster(i)
            # self.monsters.append(Monster(monster_coord, i, monster_char))  # TODO
        
        # Initialize goodies
        for coord in self.map.get_goodie_coords():
            # self.goodies.append(Goodie(coord))  # TODO
            pass
        
        print("Game started")
    
    def pause(self):
        """Pause the game."""
        self.paused = True
        print("Game paused")
    
    def resume(self):
        """Resume the game."""
        self.paused = False
        print("Game resumed")
    
    def update(self):
        """
        Update game state.
        
        This is called every frame to:
        - Process input
        - Move entities
        - Check collisions
        - Spawn/respawn pickups
        - Handle special effects
        """
        if not self.running or self.paused:
            return
        
        now = time.time() * 1000
        elapsed = now - self.last_update_time
        self.last_update_time = now
        
        # Update entities
        # self._update_pacman(now)  # TODO
        # self._update_monsters(now)  # TODO
        # self._update_pickups(now)  # TODO
        
        # Check collisions
        # self._check_collisions()  # TODO
        
        # Check win/lose conditions
        self._check_game_conditions()
    
    def _check_game_conditions(self):
        """Check if the game has been won or lost."""
        # TODO: Implement win/lose condition checking
        pass
    
    def render(self, renderer):
        """
        Render the game state.
        
        Args:
            renderer: The Renderer instance to use for drawing
        """
        # TODO: Implement rendering logic
        pass
    
    def handle_event(self, event):
        """Handle an SDL event."""
        # TODO: Delegate to appropriate handlers
        pass
    
    def trigger_loss(self, coord: MapCoord):
        """Trigger a loss sequence when Pacman is caught."""
        # TODO: Implement loss sequence
        pass
    
    def trigger_win(self):
        """Trigger a win sequence when all goodies are collected."""
        # TODO: Implement win sequence
        pass
    
    # Properties
    @property
    def is_running(self) -> bool:
        return self.running
    
    @property
    def is_paused(self) -> bool:
        return self.paused
    
    @property
    def is_won(self) -> bool:
        return self.won
    
    @property
    def is_lost(self) -> bool:
        return self.lost
    
    @property
    def is_disco_easter_egg_active(self) -> bool:
        """Check if disco easter egg is active."""
        return self.active_disco_easteregg is not None and self.active_disco_easteregg.is_active
    
    def request_disco_easter_egg_end(self, now: int):
        """Request to end the disco easter egg."""
        # TODO: Implement
        pass
    
    def adjust_disco_rotation_speed(self, delta: float):
        """Adjust the disco ball rotation speed."""
        # TODO: Implement
        pass
