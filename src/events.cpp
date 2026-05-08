/*
 * PROJECT COMMENT (events.cpp)
 * ---------------------------------------------------------------------------
 * This file is part of "BobMan", an SDL2 game inspired by Pac-Man.
 * The code in this unit has a clear responsibility so newcomers can quickly
 * understand the architecture: data model (map, elements), runtime logic
 * (game, events), rendering (renderer), and optional audio output.
 *
 * Important notes for newcomers:
 * - Header files declare classes, methods, and data types.
 * - CPP files contain the concrete implementation of the logic.
 * - Multiple threads move game entities in parallel, so shared data is read
 *   and written in a controlled way.
 * - Macros in definitions.h control resource paths, colors, and features.
 */

#include "events.h"

namespace {

ExtraSlot KeycodeToExtraSlot(SDL_Keycode keycode) {
  switch (keycode) {
  case SDLK_1:
  case SDLK_KP_1:
    return ExtraSlot::Dynamite;
  case SDLK_2:
  case SDLK_KP_2:
    return ExtraSlot::PlasticExplosive;
  case SDLK_3:
  case SDLK_KP_3:
    return ExtraSlot::Airstrike;
  case SDLK_4:
  case SDLK_KP_4:
    return ExtraSlot::Rocket;
  case SDLK_5:
  case SDLK_KP_5:
    return ExtraSlot::Biohazard;
  case SDLK_6:
  case SDLK_KP_6:
    return ExtraSlot::NuclearBomb;
  case SDLK_7:
  case SDLK_KP_7:
    return ExtraSlot::LovePotion;
  default:
    return ExtraSlot::None;
  }
}

} // namespace

/**
 * @brief Construct a new Events:: Events object
 * Events object takes care of all keyboard inputs which are required for the
 * game
 */
Events::Events() {
  sdl_events = new SDL_Event;
  quit = false;
  current_direction = Directions::None;
  requested_extra = ExtraSlot::None;
  nuclear_test_requested = false;
  nuclear_test_b_requested = false;
  disco_test_requested = false;
  alien_spawn_requested = false;
  alien_laser_requested = false;
  pause_toggle_requested = false;
  exit_dialog_requested = false;
  confirm_requested = false;
  frame_stats_toggle_requested = false;
  gameplay_frozen.store(false, std::memory_order_relaxed);
}

/**
 * @brief Destroy the Events:: Events object
 *
 */
Events::~Events() { delete sdl_events; }

/**
 * @brief Make sure there is only one pacman move per key press
 *
 */
void Events::Keyreset() { current_direction = Directions::None; }

void Events::RequestQuit() { quit = true; }

void Events::SetGameplayFrozen(bool frozen) {
  gameplay_frozen.store(frozen, std::memory_order_relaxed);
  if (frozen) {
    current_direction = Directions::None;
    requested_extra = ExtraSlot::None;
    nuclear_test_requested = false;
    nuclear_test_b_requested = false;
    disco_test_requested = false;
    alien_spawn_requested = false;
    alien_laser_requested = false;
    cheat_pending.fill(false);
  }
}

bool Events::IsGameplayFrozen() const {
  return gameplay_frozen.load(std::memory_order_relaxed);
}

bool Events::ConsumeExtraUseRequest(ExtraSlot slot) {
  if (requested_extra != slot) {
    return false;
  }

  requested_extra = ExtraSlot::None;
  return true;
}

bool Events::ConsumeCheatRequest(ExtraSlot slot) {
  const size_t index = static_cast<size_t>(slot);
  if (index >= cheat_pending.size()) {
    return false;
  }
  const bool requested = cheat_pending[index];
  cheat_pending[index] = false;
  return requested;
}

bool Events::ConsumeNuclearTestRequest() {
  const bool requested = nuclear_test_requested;
  nuclear_test_requested = false;
  return requested;
}

bool Events::ConsumeNuclearTestBRequest() {
  const bool requested = nuclear_test_b_requested;
  nuclear_test_b_requested = false;
  return requested;
}

bool Events::ConsumeDiscoTestRequest() {
  const bool requested = disco_test_requested;
  disco_test_requested = false;
  return requested;
}

bool Events::ConsumeAlienSpawnRequest() {
  const bool requested = alien_spawn_requested;
  alien_spawn_requested = false;
  return requested;
}

bool Events::ConsumeAlienLaserRequest() {
  const bool requested = alien_laser_requested;
  alien_laser_requested = false;
  return requested;
}

bool Events::ConsumePauseToggleRequest() {
  const bool requested = pause_toggle_requested;
  pause_toggle_requested = false;
  return requested;
}

bool Events::ConsumeExitDialogRequest() {
  const bool requested = exit_dialog_requested;
  exit_dialog_requested = false;
  return requested;
}

bool Events::ConsumeConfirmRequest() {
  const bool requested = confirm_requested;
  confirm_requested = false;
  return requested;
}

bool Events::ConsumeFrameStatsToggleRequest() {
  const bool requested = frame_stats_toggle_requested;
  frame_stats_toggle_requested = false;
  return requested;
}

/**
 * @brief Updates the input buffer each cycle
 *
 */
void Events::update() {
  Directions latest_direction = current_direction;
  bool saw_direction = false;

  while (SDL_PollEvent(sdl_events) != 0) {
    if (sdl_events->type == SDL_QUIT) {
      quit = true;
      continue;
    }

    if (sdl_events->type != SDL_KEYDOWN) {
      continue;
    }

    const SDL_Keycode keycode = sdl_events->key.keysym.sym;
    switch (keycode) {
    case SDLK_SPACE:
      pause_toggle_requested = true;
      break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      confirm_requested = true;
      break;
    case SDLK_ESCAPE:
      exit_dialog_requested = true;
      break;
    case SDLK_f:
      if (sdl_events->key.repeat == 0) {
        frame_stats_toggle_requested = true;
      }
      break;
    default:
      break;
    }

    const bool is_repeat = sdl_events->key.repeat != 0;
    const Uint16 key_mod = sdl_events->key.keysym.mod;
    const bool shift_held = (key_mod & KMOD_SHIFT) != 0;

    if (!IsGameplayFrozen()) {
      const ExtraSlot requested_slot = KeycodeToExtraSlot(keycode);
      if (requested_slot != ExtraSlot::None) {
        if (shift_held) {
          // Cheat: Shift + Ziffer inkrementiert das Inventar.
          // Auto-Repeat ignorieren und pro Slot 1 s entprellen.
          if (!is_repeat) {
            const size_t slot_index = static_cast<size_t>(requested_slot);
            const Uint32 now_ms = SDL_GetTicks();
            const Uint32 last = cheat_last_inc_ms[slot_index];
            if (last == 0 || now_ms - last >= 1000) {
              cheat_pending[slot_index] = true;
              cheat_last_inc_ms[slot_index] = now_ms;
            }
          }
          continue;
        }
        // Normaler Einsatz: jeder Tastendruck zählt einmal,
        // Auto-Repeat (gehaltene Taste) wird ignoriert.
        if (!is_repeat) {
          requested_extra = requested_slot;
        }
        continue;
      }
      if (keycode == SDLK_b) {
        if (!is_repeat) {
          nuclear_test_b_requested = true;
        }
        continue;
      }
      if (keycode == SDLK_d) {
        if (!is_repeat) {
          disco_test_requested = true;
        }
        continue;
      }
      if (keycode == SDLK_m) {
        if (!is_repeat) {
          alien_spawn_requested = true;
        }
        continue;
      }
      if (keycode == SDLK_l) {
        if (!is_repeat) {
          alien_laser_requested = true;
        }
        continue;
      }
    }

    switch (keycode) {
    case SDLK_UP:
      latest_direction = Directions::Up;
      saw_direction = true;
      break;
    case SDLK_DOWN:
      latest_direction = Directions::Down;
      saw_direction = true;
      break;
    case SDLK_LEFT:
      latest_direction = Directions::Left;
      saw_direction = true;
      break;
    case SDLK_RIGHT:
      latest_direction = Directions::Right;
      saw_direction = true;
      break;
    default:
      break;
    }
  }

  if (saw_direction) {
    current_direction = latest_direction;
  }
}
