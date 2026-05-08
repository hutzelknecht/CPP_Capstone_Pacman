/*
 * PROJECT COMMENT (events.h)
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

#ifndef EVENTS_H
#define EVENTS_H

#include <SDL.h>
#include <array>
#include <atomic>
#include <iostream>

#include "globaltypes.h"

class Events {
public:
  Events();
  ~Events();
  void update();
  void Keyreset();
  void RequestQuit();
  void SetGameplayFrozen(bool frozen);
  bool IsGameplayFrozen() const;
  bool is_quit() { return quit; }
  Directions get_next_move() { return current_direction; }
  bool ConsumeExtraUseRequest(ExtraSlot slot);
  bool ConsumeCheatRequest(ExtraSlot slot);
  bool ConsumeNuclearTestRequest();
  bool ConsumeNuclearTestBRequest();
  bool ConsumeDiscoTestRequest();
  bool ConsumeAlienSpawnRequest();
  bool ConsumeAlienLaserRequest();
  bool ConsumePauseToggleRequest();
  bool ConsumeExitDialogRequest();
  bool ConsumeConfirmRequest();
  bool ConsumeFrameStatsToggleRequest();

protected:
private:
  SDL_Event *sdl_events;
  bool quit;
  Directions current_direction;
  ExtraSlot requested_extra;
  bool nuclear_test_requested;
  bool nuclear_test_b_requested;
  bool disco_test_requested;
  bool alien_spawn_requested;
  bool alien_laser_requested;
  bool pause_toggle_requested;
  bool exit_dialog_requested;
  bool confirm_requested;
  bool frame_stats_toggle_requested;
  // Cheat: Shift + Ziffer erhöht das jeweilige Inventar.
  // Index entspricht dem ExtraSlot-Enum (0..7).
  std::array<bool, 8> cheat_pending{};
  std::array<Uint32, 8> cheat_last_inc_ms{};
  std::atomic<bool> gameplay_frozen;
};

#endif
