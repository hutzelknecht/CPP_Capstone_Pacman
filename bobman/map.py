"""
Map module - Handles map loading, parsing, and tile management.

Ported from src/map.cpp and src/map.h
"""

import os
from typing import List, Dict, Optional, Tuple
from bobman.constants import (
    MAPS_DIR,
    MAP_ROWS,
    MAP_COLS,
    WALL,
    PATH,
    TELEPORT_1,
    TELEPORT_2,
    TELEPORT_3,
    TELEPORT_4,
    TELEPORT_5,
    PACMAN_START,
    MONSTER_STANDARD,
    MONSTER_GAS,
    MONSTER_FIRE,
    MONSTER_GOAT,
    MONSTER_ALIEN,
    GOODIE,
    MapCoord,
)


class Map:
    """
    Represents the game map.
    
    This class handles:
    - Loading map files from disk
    - Parsing map layout
    - Finding teleporter pairs
    - Accessing map data (walls, paths, entities)
    """
    
    def __init__(self, file_path: str):
        """
        Initialize the map.
        
        Args:
            file_path: Path to the map file
        """
        self.file_path = file_path
        self.display_name = ""
        self.grid: List[List[str]] = []
        self.rows = 0
        self.cols = 0
        
        # Entity positions
        self.pacman_start: Optional[MapCoord] = None
        self.monster_starts: List[MapCoord] = []
        self.monster_chars: List[str] = []
        self.goodie_coords: List[MapCoord] = []
        
        # Teleporters
        self.teleporter_pairs: Dict[str, List[MapCoord]] = {}
        
        print(f"Map initialized: {file_path}")
    
    def load(self) -> bool:
        """
        Load the map from file.
        
        Returns:
            True if loaded successfully, False otherwise
        """
        if not os.path.exists(self.file_path):
            print(f"Error: Map file not found: {self.file_path}")
            return False
        
        try:
            with open(self.file_path, 'r') as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Error reading map file: {e}")
            return False
        
        # First line is display name
        if lines:
            self.display_name = lines[0].strip()
            lines = lines[1:]  # Skip display name
        
        # Parse grid
        self.grid = []
        for line in lines:
            line = line.strip()
            if line:
                self.grid.append(list(line[:MAP_COLS]))  # Limit to MAP_COLS
        
        # Normalize grid size
        self.rows = min(len(self.grid), MAP_ROWS)
        self.cols = MAP_COLS
        
        for i in range(self.rows):
            while len(self.grid[i]) < self.cols:
                self.grid[i].append(' ')  # Pad with spaces
        
        # Find entities
        self._find_entities()
        
        # Find teleporter pairs
        self._find_teleporters()
        
        print(f"Map loaded: {self.cols}x{self.rows}, name='{self.display_name}'")
        return True
    
    def _find_entities(self):
        """Find all entity positions on the map."""
        for row in range(self.rows):
            for col in range(self.cols):
                cell = self.grid[row][col]
                coord = MapCoord(row, col)
                
                if cell == PACMAN_START:
                    self.pacman_start = coord
                elif cell in [MONSTER_STANDARD, MONSTER_GAS, MONSTER_FIRE, 
                              MONSTER_GOAT, MONSTER_ALIEN]:
                    self.monster_starts.append(coord)
                    self.monster_chars.append(cell)
                elif cell == GOODIE:
                    self.goodie_coords.append(coord)
    
    def _find_teleporters(self):
        """Find and pair teleporter tiles."""
        teleporter_positions: Dict[str, List[MapCoord]] = {}
        
        for row in range(self.rows):
            for col in range(self.cols):
                cell = self.grid[row][col]
                if cell in [TELEPORT_1, TELEPORT_2, TELEPORT_3, TELEPORT_4, TELEPORT_5]:
                    if cell not in teleporter_positions:
                        teleporter_positions[cell] = []
                    teleporter_positions[cell].append(MapCoord(row, col))
        
        # Validate pairs (each teleporter digit should appear exactly 0 or 2 times)
        for digit, coords in teleporter_positions.items():
            if len(coords) != 2:
                print(f"Warning: Teleporter '{digit}' has {len(coords)} positions (expected 2)")
            else:
                self.teleporter_pairs[digit] = coords
    
    def get_cell(self, coord: MapCoord) -> str:
        """Get the character at a map coordinate."""
        if 0 <= coord.u < self.rows and 0 <= coord.v < self.cols:
            return self.grid[coord.u][coord.v]
        return ' '  # Out of bounds = empty
    
    def is_wall(self, coord: MapCoord) -> bool:
        """Check if a coordinate is a wall."""
        return self.get_cell(coord) == WALL
    
    def is_path(self, coord: MapCoord) -> bool:
        """Check if a coordinate is a path."""
        return self.get_cell(coord) == PATH
    
    def is_teleporter(self, coord: MapCoord) -> bool:
        """Check if a coordinate is a teleporter."""
        cell = self.get_cell(coord)
        return cell in [TELEPORT_1, TELEPORT_2, TELEPORT_3, TELEPORT_4, TELEPORT_5]
    
    def get_teleporter_pair(self, coord: MapCoord) -> Optional[MapCoord]:
        """
        Get the paired teleporter coordinate.
        
        Args:
            coord: One end of a teleporter pair
            
        Returns:
            The other end of the pair, or None if not a teleporter or no pair
        """
        cell = self.get_cell(coord)
        if cell in self.teleporter_pairs:
            for other in self.teleporter_pairs[cell]:
                if other != coord:
                    return other
        return None
    
    def get_coord_pacman(self) -> MapCoord:
        """Get Pacman's starting coordinate."""
        if self.pacman_start is None:
            # Fallback: use center of map
            return MapCoord(self.rows // 2, self.cols // 2)
        return self.pacman_start
    
    def get_number_monsters(self) -> int:
        """Get the number of monsters on the map."""
        return len(self.monster_starts)
    
    def get_coord_monster(self, index: int) -> MapCoord:
        """Get a monster's starting coordinate by index."""
        if 0 <= index < len(self.monster_starts):
            return self.monster_starts[index]
        return MapCoord(0, 0)
    
    def get_char_monster(self, index: int) -> str:
        """Get a monster's character type by index."""
        if 0 <= index < len(self.monster_chars):
            return self.monster_chars[index]
        return MONSTER_STANDARD
    
    def get_goodie_coords(self) -> List[MapCoord]:
        """Get all goodie coordinates."""
        return self.goodie_coords.copy()
    
    def get_map_rows(self) -> int:
        """Get the number of rows in the map."""
        return self.rows
    
    def get_map_cols(self) -> int:
        """Get the number of columns in the map."""
        return self.cols
    
    def find_path_coords(self) -> List[MapCoord]:
        """Find all path coordinates on the map."""
        coords = []
        for row in range(self.rows):
            for col in range(self.cols):
                if self.grid[row][col] == PATH:
                    coords.append(MapCoord(row, col))
        return coords
    
    @staticmethod
    def get_available_maps() -> List[Tuple[str, str]]:
        """
        Get a list of available map files.
        
        Returns:
            List of (display_name, file_path) tuples
        """
        maps = []
        if not os.path.exists(MAPS_DIR):
            return maps
        
        for filename in sorted(os.listdir(MAPS_DIR)):
            if filename.endswith('.txt') or filename.endswith('.map'):
                filepath = os.path.join(MAPS_DIR, filename)
                maps.append((filename, filepath))
        
        return maps
