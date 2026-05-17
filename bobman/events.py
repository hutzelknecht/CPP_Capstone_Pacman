"""
Events module - Handles all input and event processing.

Ported from src/events.cpp and src/events.h
"""

import sdl2
from typing import Optional
from bobman.constants import Directions, ExtraSlot


class Events:
    """
    Handles all keyboard inputs and SDL events for the game.
    
    This class manages:
    - SDL event polling
    - Keyboard input state
    - Gameplay freeze state
    - Input requests (cheats, tests, etc.)
    """
    
    def __init__(self):
        """Initialize the events system."""
        self.quit = False
        self.current_direction = Directions.NONE
        self.gameplay_frozen = False
        
        # Request flags
        self.requested_extra = ExtraSlot.NONE
        self.nuclear_test_requested = False
        self.nuclear_test_b_requested = False
        self.disco_test_requested = False
        self.alien_spawn_requested = False
        self.alien_laser_requested = False
        self.pause_toggle_requested = False
        self.exit_dialog_requested = False
        self.confirm_requested = False
        self.frame_stats_toggle_requested = False
        
        # Cheat tracking (Shift + number keys)
        self.cheat_pending = [False] * 8
        self.cheat_last_inc_ms = [0] * 8
        
        print("Events initialized")
    
    def update(self):
        """
        Process all pending SDL events.
        
        This should be called every frame to poll for new events.
        """
        latest_direction = self.current_direction
        saw_direction = False
        
        # Create a temporary event structure
        event = sdl2.SDL_Event()
        
        while sdl2.SDL_PollEvent(event) != 0:
            if event.type == sdl2.SDL_QUIT:
                self.quit = True
                continue
            
            if event.type != sdl2.SDL_KEYDOWN:
                continue
            
            keycode = event.key.keysym.sym
            is_repeat = event.key.repeat != 0
            key_mod = event.key.keysym.mod
            shift_held = (key_mod & sdl2.KMOD_SHIFT) != 0
            
            # Handle special keys
            self._handle_special_keys(keycode, is_repeat)
            
            # Handle direction keys
            direction = self._keycode_to_direction(keycode)
            if direction is not None:
                latest_direction = direction
                saw_direction = True
            
            # Handle gameplay keys (only if not frozen)
            if not self.gameplay_frozen:
                self._handle_gameplay_keys(keycode, is_repeat, shift_held)
        
        if saw_direction:
            self.current_direction = latest_direction
    
    def _keycode_to_direction(self, keycode) -> Optional[Directions]:
        """Convert an SDL keycode to a Directions enum."""
        key_map = {
            sdl2.SDLK_UP: Directions.Up,
            sdl2.SDLK_DOWN: Directions.Down,
            sdl2.SDLK_LEFT: Directions.Left,
            sdl2.SDLK_RIGHT: Directions.Right,
        }
        return key_map.get(keycode, None)
    
    def _handle_special_keys(self, keycode, is_repeat: bool):
        """Handle keys that work regardless of gameplay state."""
        if is_repeat:
            return
        
        if keycode == sdl2.SDLK_ESCAPE:
            self.exit_dialog_requested = True
        elif keycode == sdl2.SDLK_RETURN or keycode == sdl2.SDLK_KP_ENTER:
            self.confirm_requested = True
        elif keycode == sdl2.SDLK_SPACE:
            self.pause_toggle_requested = True
        elif keycode == sdl2.SDLK_f:
            self.frame_stats_toggle_requested = True
    
    def _handle_gameplay_keys(self, keycode, is_repeat: bool, shift_held: bool):
        """Handle keys that only work during gameplay."""
        if is_repeat:
            return
        
        # Extra slot keys (1-8)
        extra_map = {
            sdl2.SDLK_1: ExtraSlot.Dynamite,
            sdl2.SDLK_KP_1: ExtraSlot.Dynamite,
            sdl2.SDLK_2: ExtraSlot.PlasticExplosive,
            sdl2.SDLK_KP_2: ExtraSlot.PlasticExplosive,
            sdl2.SDLK_3: ExtraSlot.WalkieTalkie,
            sdl2.SDLK_KP_3: ExtraSlot.WalkieTalkie,
            sdl2.SDLK_4: ExtraSlot.Rocket,
            sdl2.SDLK_KP_4: ExtraSlot.Rocket,
            sdl2.SDLK_5: ExtraSlot.Biohazard,
            sdl2.SDLK_KP_5: ExtraSlot.Biohazard,
            sdl2.SDLK_6: ExtraSlot.NuclearBomb,
            sdl2.SDLK_KP_6: ExtraSlot.NuclearBomb,
            sdl2.SDLK_7: ExtraSlot.LovePotion,
            sdl2.SDLK_KP_7: ExtraSlot.LovePotion,
        }
        
        if keycode in extra_map:
            if shift_held:
                # Cheat: Shift + number increments inventory
                slot = extra_map[keycode]
                slot_index = slot.value - 1  # Convert to 0-based index
                if 0 <= slot_index < 8:
                    import time
                    now_ms = int(time.time() * 1000)
                    last = self.cheat_last_inc_ms[slot_index]
                    if last == 0 or now_ms - last >= 1000:
                        self.cheat_pending[slot_index] = True
                        self.cheat_last_inc_ms[slot_index] = now_ms
            else:
                # Normal use
                self.requested_extra = extra_map[keycode]
        
        # Test/debug keys
        if keycode == sdl2.SDLK_b:
            self.nuclear_test_b_requested = True
        elif keycode == sdl2.SDLK_d:
            self.disco_test_requested = True
        elif keycode == sdl2.SDLK_m:
            self.alien_spawn_requested = True
        elif keycode == sdl2.SDLK_l:
            self.alien_laser_requested = True
    
    def set_gameplay_frozen(self, frozen: bool):
        """Set whether gameplay input is frozen."""
        self.gameplay_frozen = frozen
        if frozen:
            # Reset all pending input when freezing
            self.current_direction = Directions.NONE
            self.requested_extra = ExtraSlot.NONE
            self.nuclear_test_requested = False
            self.nuclear_test_b_requested = False
            self.disco_test_requested = False
            self.alien_spawn_requested = False
            self.alien_laser_requested = False
            self.cheat_pending = [False] * 8
    
    def is_gameplay_frozen(self) -> bool:
        """Check if gameplay input is frozen."""
        return self.gameplay_frozen
    
    def keyreset(self):
        """Reset the current direction (called after handling movement)."""
        self.current_direction = Directions.NONE
    
    def request_quit(self):
        """Request the application to quit."""
        self.quit = True
    
    def is_quit(self) -> bool:
        """Check if quit has been requested."""
        return self.quit
    
    def get_next_move(self) -> Directions:
        """Get the current requested movement direction."""
        return self.current_direction
    
    # Consume methods - these reset the flag after checking
    def consume_extra_use_request(self, slot: ExtraSlot) -> bool:
        """Check and consume an extra use request."""
        if self.requested_extra != slot:
            return False
        self.requested_extra = ExtraSlot.NONE
        return True
    
    def consume_cheat_request(self, slot: ExtraSlot) -> bool:
        """Check and consume a cheat request."""
        index = slot.value - 1
        if index < 0 or index >= 8:
            return False
        requested = self.cheat_pending[index]
        self.cheat_pending[index] = False
        return requested
    
    def consume_nuclear_test_request(self) -> bool:
        """Check and consume nuclear test request."""
        requested = self.nuclear_test_requested
        self.nuclear_test_requested = False
        return requested
    
    def consume_nuclear_test_b_request(self) -> bool:
        """Check and consume nuclear test B request."""
        requested = self.nuclear_test_b_requested
        self.nuclear_test_b_requested = False
        return requested
    
    def consume_disco_test_request(self) -> bool:
        """Check and consume disco test request."""
        requested = self.disco_test_requested
        self.disco_test_requested = False
        return requested
    
    def consume_alien_spawn_request(self) -> bool:
        """Check and consume alien spawn request."""
        requested = self.alien_spawn_requested
        self.alien_spawn_requested = False
        return requested
    
    def consume_alien_laser_request(self) -> bool:
        """Check and consume alien laser request."""
        requested = self.alien_laser_requested
        self.alien_laser_requested = False
        return requested
    
    def consume_pause_toggle_request(self) -> bool:
        """Check and consume pause toggle request."""
        requested = self.pause_toggle_requested
        self.pause_toggle_requested = False
        return requested
    
    def consume_exit_dialog_request(self) -> bool:
        """Check and consume exit dialog request."""
        requested = self.exit_dialog_requested
        self.exit_dialog_requested = False
        return requested
    
    def consume_confirm_request(self) -> bool:
        """Check and consume confirm request."""
        requested = self.confirm_requested
        self.confirm_requested = False
        return requested
    
    def consume_frame_stats_toggle_request(self) -> bool:
        """Check and consume frame stats toggle request."""
        requested = self.frame_stats_toggle_requested
        self.frame_stats_toggle_requested = False
        return requested
