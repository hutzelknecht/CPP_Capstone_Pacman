/*
 * PROJECT COMMENT (game.cpp)
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

#include "game.h"
#include "paths.h"

#include <SDL_image.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace {

constexpr Uint32 kMonsterExplosionDurationMs = MONSTER_EXPLOSION_DURATION_MS;
constexpr Uint32 kWallImpactDurationMs = 180;
constexpr Uint32 kBiohazardHitSequenceDurationMs = BIOHAZARD_HIT_SEQUENCE_MS;
constexpr Uint32 kBiohazardImpactFlashDurationMs =
    BIOHAZARD_IMPACT_FLASH_DURATION_MS;
constexpr Uint32 kAlienExplosionDurationMs = ALIEN_EXPLOSION_DURATION_MS;
constexpr Uint32 kSlimeSplashDurationMs =
    SLIME_SPLASH_FRAME_MS * SLIME_SPLASH_FRAME_COUNT + SLIME_SPLASH_FADE_MS;
constexpr Uint32 kPlasmaShockwaveDurationMs = PLASMA_SHOCKWAVE_DURATION_MS;
constexpr Uint32 kNuclearExplosionDurationMs = NUCLEAR_EXPLOSION_TOTAL_DURATION_MS;
constexpr Uint32 kNuclearExplosionBDurationMs =
    NUCLEAR_B_EXPLOSION_TOTAL_DURATION_MS;
constexpr Uint32 kDynamiteChainDelayMs = 140;
constexpr double kPlasticExplosiveMonsterHitRadiusCells = 0.72;
constexpr float kAirstrikePlaneMarginCells = 2.0f;
constexpr float kAirstrikePathSamplesPerCell = 14.0f;
constexpr int kNuclearCraterExplosionDelayMs = 80;
constexpr Uint8 kNuclearCraterAlphaThreshold = 0;

struct AirstrikePathCandidate {
  MapCoord coord{0, 0};
  SDL_FPoint point{0.0f, 0.0f};
  double progress = 0.0;
};

struct DifficultyTuning {
  int monster_speed_rating;
  double monster_step_delay_scale;
  Uint32 fireball_cooldown_ms;
  Uint32 fireball_step_duration_ms;
  Uint32 pickup_visible_ms;
  double extra_spawn_interval_scale;
};

struct ExplosionParticleMotionTuning {
  float gravity_cells_per_sec2;
  Uint32 lifetime_ms;
  Uint32 smoke_spawn_interval_ms;
  ExplosionSmokePuffKind smoke_kind;
};

ExplosionParticleMotionTuning
GetExplosionParticleMotionTuning(ExplosionParticleKind kind) {
  switch (kind) {
  case ExplosionParticleKind::WallDebris:
    return {PLASTIC_EXPLOSIVE_WALL_DEBRIS_GRAVITY_CELLS_PER_SEC2,
            PLASTIC_EXPLOSIVE_WALL_DEBRIS_LIFETIME_MS,
            PLASTIC_EXPLOSIVE_WALL_DUST_SPAWN_INTERVAL_MS,
            ExplosionSmokePuffKind::WallDust};
  case ExplosionParticleKind::NuclearDebris:
    return {NUCLEAR_BOMB_DEBRIS_GRAVITY_CELLS_PER_SEC2,
            NUCLEAR_BOMB_DEBRIS_LIFETIME_MS,
            NUCLEAR_BOMB_DUST_SPAWN_INTERVAL_MS,
            ExplosionSmokePuffKind::NuclearDust};
  case ExplosionParticleKind::MonsterExplosion:
  default:
    return {MONSTER_EXPLOSION_PARTICLE_GRAVITY_CELLS_PER_SEC2,
            MONSTER_EXPLOSION_PARTICLE_LIFETIME_MS,
            MONSTER_EXPLOSION_SMOKE_SPAWN_INTERVAL_MS,
            ExplosionSmokePuffKind::MonsterSmoke};
  }
}

Uint32 GetSmokePuffLifetimeMs(ExplosionSmokePuffKind kind) {
  switch (kind) {
  case ExplosionSmokePuffKind::BlastSmoke:
    return EXPLOSION_SMOKE_CLOUD_LIFETIME_MS;
  case ExplosionSmokePuffKind::RocketBlastSmoke:
    return ROCKET_BLAST_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::RocketTrailSmoke:
    return ROCKET_TRAIL_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearRingSmoke:
    return NUCLEAR_RING_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearCoreSmoke:
    return NUCLEAR_CORE_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearMushroomSmoke:
    return NUCLEAR_MUSHROOM_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearBRingSmoke:
    return NUCLEAR_B_RING_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearBTrailSmoke:
    return NUCLEAR_B_TRAIL_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearBStemSmoke:
    return NUCLEAR_B_STEM_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearBCapSmoke:
    return NUCLEAR_B_CAP_SMOKE_LIFETIME_MS;
  case ExplosionSmokePuffKind::WallDust:
    return PLASTIC_EXPLOSIVE_WALL_DUST_LIFETIME_MS;
  case ExplosionSmokePuffKind::NuclearDust:
    return NUCLEAR_BOMB_DUST_LIFETIME_MS;
  case ExplosionSmokePuffKind::GoatRedDust:
    return 2400;
  case ExplosionSmokePuffKind::MonsterDustCloud:
    return GOAT_MONSTER_DUST_LIFETIME_MS;
  case ExplosionSmokePuffKind::LoveSmokeTrail:
    return LOVE_SMOKE_TRAIL_LIFETIME_MS;
  case ExplosionSmokePuffKind::LoveSmokeImpact:
    return LOVE_SMOKE_IMPACT_LIFETIME_MS;
  case ExplosionSmokePuffKind::MonsterSmoke:
  default:
    return MONSTER_EXPLOSION_SMOKE_LIFETIME_MS;
  }
}

std::mt19937 &RandomGenerator() {
  static std::mt19937 generator(std::random_device{}());
  return generator;
}

Uint32 RandomInterval(Uint32 minimum, Uint32 maximum) {
  std::uniform_int_distribution<Uint32> distribution(minimum, maximum);
  return distribution(RandomGenerator());
}

void ShiftActiveTicks(Uint32 &ticks, Uint32 delta_ms) {
  if (ticks != 0) {
    ticks += delta_ms;
  }
}

DifficultyTuning GetDifficultyTuning(Difficulty difficulty) {
  switch (difficulty) {
  case Difficulty::Training:
    return {std::max(1, SPEED_MONSTER - 4), 1.5, 15000, 330, 60000, 0.75};
  case Difficulty::Easy:
    return {std::max(1, SPEED_MONSTER - 3), 1.0, 15000, 220, 60000, 0.75};
  case Difficulty::Hard:
    return {std::max(1, SPEED_MONSTER), 1.0, 6500, 120, 20000, 1.4};
  case Difficulty::Medium:
  default:
    return {std::max(1, SPEED_MONSTER - 2), 1.0, 10000, 160, 40000, 1.0};
  }
}

Uint32 ScaleInterval(Uint32 base_value, double scale) {
  return std::max<Uint32>(
      1000, static_cast<Uint32>(std::lround(base_value * scale)));
}

Uint32 RandomScaledInterval(Uint32 minimum, Uint32 maximum, double scale) {
  const Uint32 scaled_minimum = ScaleInterval(minimum, scale);
  const Uint32 scaled_maximum =
      std::max(scaled_minimum, ScaleInterval(maximum, scale));
  return RandomInterval(scaled_minimum, scaled_maximum);
}

int GetMonsterMovementStepDelayMs(Difficulty difficulty) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  const double base_delay_ms = std::max(1, 10 - tuning.monster_speed_rating);
  return std::max(
      1, static_cast<int>(std::ceil(base_delay_ms * tuning.monster_step_delay_scale)));
}

int GetGoatMovementStepDelayMs() {
  return std::max(1, 10 - SPEED_GOAT);
}

MapCoord StepCoord(MapCoord coord, Directions direction) {
  switch (direction) {
  case Directions::Up:
    coord.u--;
    break;
  case Directions::Down:
    coord.u++;
    break;
  case Directions::Left:
    coord.v--;
    break;
  case Directions::Right:
    coord.v++;
    break;
  default:
    break;
  }
  return coord;
}

bool SameCoord(MapCoord left, MapCoord right) {
  return left.u == right.u && left.v == right.v;
}

bool IsInsideMapBounds(Map *map, MapCoord coord) {
  if (map == nullptr || coord.u < 0 || coord.v < 0) {
    return false;
  }

  return coord.u < static_cast<int>(map->get_map_rows()) &&
         coord.v < static_cast<int>(map->get_map_cols());
}

bool IsOuterWallCoord(Map *map, MapCoord coord) {
  if (!IsInsideMapBounds(map, coord)) {
    return false;
  }

  const int last_row = static_cast<int>(map->get_map_rows()) - 1;
  const int last_col = static_cast<int>(map->get_map_cols()) - 1;
  return coord.u == 0 || coord.v == 0 || coord.u == last_row ||
         coord.v == last_col;
}

bool HasClearAxisLineOfSight(Map *map, MapCoord from, MapCoord to,
                             Directions &direction_out) {
  direction_out = Directions::None;
  if (map == nullptr || SameCoord(from, to)) {
    return false;
  }

  if (from.u == to.u) {
    direction_out = (to.v > from.v) ? Directions::Right : Directions::Left;
    const int step = (to.v > from.v) ? 1 : -1;
    for (int col = from.v + step; col != to.v; col += step) {
      if (IsBlockingMapElement(map->map_entry(static_cast<size_t>(from.u),
                                              static_cast<size_t>(col)))) {
        return false;
      }
    }
    return true;
  }

  if (from.v == to.v) {
    direction_out = (to.u > from.u) ? Directions::Down : Directions::Up;
    const int step = (to.u > from.u) ? 1 : -1;
    for (int row = from.u + step; row != to.u; row += step) {
      if (IsBlockingMapElement(map->map_entry(static_cast<size_t>(row),
                                              static_cast<size_t>(from.v)))) {
        return false;
      }
    }
    return true;
  }

  return false;
}

int DistanceSquared(MapCoord left, MapCoord right) {
  const int delta_u = left.u - right.u;
  const int delta_v = left.v - right.v;
  return delta_u * delta_u + delta_v * delta_v;
}

bool HasLineOfSight(Map *map, MapCoord from, MapCoord to,
                    Directions &direction_out) {
  direction_out = Directions::None;
  if (from.u == to.u) {
    direction_out = (to.v > from.v) ? Directions::Right : Directions::Left;
    const int step = (to.v > from.v) ? 1 : -1;
    for (int col = from.v + step; col != to.v; col += step) {
      if (IsBlockingMapElement(map->map_entry(from.u, col))) {
        return false;
      }
    }
    return true;
  }

  if (from.v == to.v) {
    direction_out = (to.u > from.u) ? Directions::Down : Directions::Up;
    const int step = (to.u > from.u) ? 1 : -1;
    for (int row = from.u + step; row != to.u; row += step) {
      if (IsBlockingMapElement(map->map_entry(row, from.v))) {
        return false;
      }
    }
    return true;
  }

  return false;
}

SDL_FPoint MakeCellCenter(MapCoord coord) {
  return SDL_FPoint{static_cast<float>(coord.v) + 0.5f,
                    static_cast<float>(coord.u) + 0.5f};
}

SDL_FPoint AddPoint(SDL_FPoint left, SDL_FPoint right) {
  return SDL_FPoint{left.x + right.x, left.y + right.y};
}

SDL_FPoint SubtractPoint(SDL_FPoint left, SDL_FPoint right) {
  return SDL_FPoint{left.x - right.x, left.y - right.y};
}

SDL_FPoint ScalePoint(SDL_FPoint point, float factor) {
  return SDL_FPoint{point.x * factor, point.y * factor};
}

double PointDistance(SDL_FPoint left, SDL_FPoint right) {
  return std::hypot(static_cast<double>(left.x - right.x),
                    static_cast<double>(left.y - right.y));
}

Uint32 ReadSurfacePixel(SDL_Surface *surface, int x, int y) {
  const auto *pixels = static_cast<const Uint8 *>(surface->pixels);
  const Uint8 *pixel =
      pixels + y * surface->pitch + x * surface->format->BytesPerPixel;

  switch (surface->format->BytesPerPixel) {
  case 1:
    return *pixel;
  case 2:
    return *reinterpret_cast<const Uint16 *>(pixel);
  case 3:
    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
      return pixel[0] << 16 | pixel[1] << 8 | pixel[2];
    }
    return pixel[0] | pixel[1] << 8 | pixel[2] << 16;
  case 4:
    return *reinterpret_cast<const Uint32 *>(pixel);
  default:
    return 0;
  }
}

struct CraterAlphaMask {
  int width = 0;
  int height = 0;
  std::vector<Uint8> alpha;
  bool loaded = false;
};

const CraterAlphaMask &GetNuclearCraterAlphaMask() {
  static CraterAlphaMask mask = [] {
    CraterAlphaMask loaded_mask;
    SDL_Surface *raw_surface =
        IMG_Load(Paths::GetDataFilePath(NUCLEAR_CRATER_ASSET_PATH).c_str());
    if (raw_surface == nullptr) {
      return loaded_mask;
    }

    SDL_Surface *rgba_surface =
        SDL_ConvertSurfaceFormat(raw_surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw_surface);
    if (rgba_surface == nullptr) {
      return loaded_mask;
    }

    loaded_mask.width = rgba_surface->w;
    loaded_mask.height = rgba_surface->h;
    loaded_mask.alpha.resize(static_cast<size_t>(loaded_mask.width) *
                             static_cast<size_t>(loaded_mask.height));

    const bool lock_surface = SDL_MUSTLOCK(rgba_surface);
    if (lock_surface) {
      SDL_LockSurface(rgba_surface);
    }

    for (int y = 0; y < loaded_mask.height; ++y) {
      for (int x = 0; x < loaded_mask.width; ++x) {
        Uint8 red = 0;
        Uint8 green = 0;
        Uint8 blue = 0;
        Uint8 alpha = 0;
        SDL_GetRGBA(ReadSurfacePixel(rgba_surface, x, y), rgba_surface->format,
                    &red, &green, &blue, &alpha);
        loaded_mask.alpha[static_cast<size_t>(y) *
                              static_cast<size_t>(loaded_mask.width) +
                          static_cast<size_t>(x)] = alpha;
      }
    }

    if (lock_surface) {
      SDL_UnlockSurface(rgba_surface);
    }
    SDL_FreeSurface(rgba_surface);
    loaded_mask.loaded = true;
    return loaded_mask;
  }();
  return mask;
}

float NuclearCraterEdgeRadius(float base_radius, int shape_seed, float angle) {
  const float phase = static_cast<float>(shape_seed % 997) * 0.013f;
  const float wobble =
      0.075f * std::sin(angle * 5.0f + phase) +
      0.045f * std::sin(angle * 9.0f + phase * 1.7f) +
      0.030f * std::sin(angle * 14.0f + phase * 0.61f);
  return base_radius * std::clamp(0.94f + wobble, 0.78f, 1.08f);
}

bool NuclearCraterContainsPoint(const NuclearCrater &crater,
                                SDL_FPoint point) {
  const CraterAlphaMask &mask = GetNuclearCraterAlphaMask();
  if (mask.loaded && mask.width > 0 && mask.height > 0 &&
      crater.radius_cells > 0.0f) {
    const float diameter = crater.radius_cells * 2.0f;
    const float texture_u =
        (point.x - (crater.world_center.x - crater.radius_cells)) / diameter;
    const float texture_v =
        (point.y - (crater.world_center.y - crater.radius_cells)) / diameter;
    if (texture_u < 0.0f || texture_u > 1.0f || texture_v < 0.0f ||
        texture_v > 1.0f) {
      return false;
    }

    const int pixel_x = std::clamp(
        static_cast<int>(std::floor(texture_u * static_cast<float>(mask.width))),
        0, mask.width - 1);
    const int pixel_y = std::clamp(
        static_cast<int>(std::floor(texture_v * static_cast<float>(mask.height))),
        0, mask.height - 1);
    return mask.alpha[static_cast<size_t>(pixel_y) *
                          static_cast<size_t>(mask.width) +
                      static_cast<size_t>(pixel_x)] >
           kNuclearCraterAlphaThreshold;
  }

  const float dx = point.x - crater.world_center.x;
  const float dy = (point.y - crater.world_center.y) /
                   std::max(0.1f, NUCLEAR_CRATER_Y_RADIUS_FACTOR);
  const float distance = std::hypot(dx, dy);
  if (distance <= crater.radius_cells * 0.42f) {
    return true;
  }

  const float angle = std::atan2(dy, dx);
  return distance <=
         NuclearCraterEdgeRadius(crater.radius_cells, crater.shape_seed, angle);
}

SDL_FPoint LerpPoint(SDL_FPoint start, SDL_FPoint end, double progress) {
  const float clamped_progress =
      static_cast<float>(std::clamp(progress, 0.0, 1.0));
  return SDL_FPoint{
      start.x + (end.x - start.x) * clamped_progress,
      start.y + (end.y - start.y) * clamped_progress};
}

Directions DominantDirection(SDL_FPoint vector) {
  if (std::fabs(vector.x) >= std::fabs(vector.y)) {
    return (vector.x >= 0.0f) ? Directions::Right : Directions::Left;
  }

  return (vector.y >= 0.0f) ? Directions::Down : Directions::Up;
}

SDL_FPoint RandomAirstrikeStart(Map *map) {
  if (map == nullptr) {
    return SDL_FPoint{0.0f, 0.0f};
  }

  const float rows = static_cast<float>(map->get_map_rows());
  const float cols = static_cast<float>(map->get_map_cols());
  const float min_x = 0.5f;
  const float max_x = std::max(min_x, cols - 0.5f);
  const float min_y = 0.5f;
  const float max_y = std::max(min_y, rows - 0.5f);
  std::uniform_real_distribution<float> x_distribution(min_x, max_x);
  std::uniform_real_distribution<float> y_distribution(min_y, max_y);
  std::uniform_int_distribution<int> distribution(0, 3);

  switch (distribution(RandomGenerator())) {
  case 0:
    return SDL_FPoint{-kAirstrikePlaneMarginCells, y_distribution(RandomGenerator())};
  case 1:
    return SDL_FPoint{cols + kAirstrikePlaneMarginCells,
                      y_distribution(RandomGenerator())};
  case 2:
    return SDL_FPoint{x_distribution(RandomGenerator()), -kAirstrikePlaneMarginCells};
  case 3:
  default:
    return SDL_FPoint{x_distribution(RandomGenerator()),
                      rows + kAirstrikePlaneMarginCells};
  }
}

std::vector<AirstrikePathCandidate>
CollectAirstrikePathCandidates(Map *map, SDL_FPoint start, SDL_FPoint end,
                               MapCoord excluded_coord) {
  std::vector<AirstrikePathCandidate> candidates;
  if (map == nullptr) {
    return candidates;
  }

  const int rows = static_cast<int>(map->get_map_rows());
  const int cols = static_cast<int>(map->get_map_cols());
  if (rows <= 0 || cols <= 0) {
    return candidates;
  }

  std::vector<bool> visited(static_cast<size_t>(rows * cols), false);
  const double path_length = PointDistance(start, end);
  const int sample_count = std::max(
      1, static_cast<int>(std::ceil(path_length * kAirstrikePathSamplesPerCell)));
  for (int sample = 0; sample <= sample_count; ++sample) {
    const double progress =
        static_cast<double>(sample) / static_cast<double>(sample_count);
    const SDL_FPoint point = LerpPoint(start, end, progress);
    const MapCoord coord{static_cast<int>(std::floor(point.y)),
                         static_cast<int>(std::floor(point.x))};
    if (!IsInsideMapBounds(map, coord) || SameCoord(coord, excluded_coord)) {
      continue;
    }

    const size_t visited_index =
        static_cast<size_t>(coord.u * cols + coord.v);
    if (visited[visited_index]) {
      continue;
    }

    visited[visited_index] = true;
    candidates.push_back({coord, point, progress});
  }

  return candidates;
}

} // namespace

/**
 * @brief Construct a new Game:: Game object
 * 
 * @param _map: Pointer to Map object
 * @param _events: Pointer to Events object
 * @param _audio: Pointer to Audio object
 * @param _starting_lives Configured number of lives for a new run
 */
Game::Game(Map *_map, Events *_events, Audio *_audio, Difficulty _difficulty,
           int _starting_lives)
    : map(_map), events(_events), audio(_audio), difficulty(_difficulty) {
  win = false;
  dead = false;
  starting_lives = std::clamp(_starting_lives, PLAYER_LIVES_MIN, PLAYER_LIVES_MAX);
  remaining_lives = starting_lives;
  score = 0;
  dynamite_inventory = 0;
  plastic_explosive_inventory = 0;
  airstrike_inventory = 0;
  rocket_inventory = 0;
  biohazard_inventory = 0;
  nuclear_bomb_inventory = 0;
  love_potion_inventory = 0;
  nuclear_bomb_collected_once = false;
  plastic_explosive_is_armed = false;
  active_airstrike = {};
  active_biohazard_beam = {};
  active_alien_laser = {};
  nuclear_bomb_target_marker = {};
  active_nuclear_bomb_drop = {};
  active_nuclear_explosion = {};
  active_nuclear_explosion_b = {};
  active_disco_easteregg = {};
  simulation_started = false;
  death_started_ticks = 0;
  life_recovery_until_ticks = 0;
  final_loss_commits_at_ticks = 0;
  last_update_ticks = SDL_GetTicks();
  // create Pacman object
  pacman = new Pacman(map->get_coord_pacman());
  death_coord = pacman->map_coord;
  ScheduleNextInvulnerabilityPotionSpawn(last_update_ticks);
  ScheduleNextDynamiteSpawn(last_update_ticks);
  ScheduleNextPlasticExplosiveSpawn(last_update_ticks);
  ScheduleNextWalkieTalkieSpawn(last_update_ticks);
  ScheduleNextRocketSpawn(last_update_ticks);
  ScheduleNextBiohazardSpawn(last_update_ticks);
  ScheduleNextNuclearBombSpawn(last_update_ticks);
  ScheduleNextLovePotionSpawn(last_update_ticks);
  ScheduleNextLifePickupSpawn(last_update_ticks);
  ScheduleNextDiscoPickupSpawn(last_update_ticks);

  // create Monster objects
  for (int i = 0; i < map->get_number_monsters(); i++) {
    const char monster_char = map->get_char_monster(i);
    const int movement_step_delay_ms =
        (monster_char == GOAT) ? GetGoatMovementStepDelayMs()
                               : GetMonsterMovementStepDelayMs(difficulty);
    Monster *monster = new Monster(map->get_coord_monster(i), i, monster_char,
                                   movement_step_delay_ms);
    if (monster->monster_char == MONSTER_MEDIUM) {
      monster->next_gas_cloud_ticks =
          last_update_ticks +
          RandomInterval(MONSTER_GAS_MIN_INTERVAL_MS,
                         MONSTER_GAS_MAX_INTERVAL_MS);
    }
    monsters.emplace_back(monster);
  }

  // create Goodie objects
  for (int i = 0; i < map->get_number_goodies(); i++) {
    goodies.emplace_back(new Goodie(map->get_coord_goodie(i), i));
  }
};

void Game::StartSimulation() {
  if (simulation_started) {
    return;
  }

  events->SetGameplayFrozen(false);
  events->Keyreset();

  for (size_t i = 0; i < monsters.size(); i++) {
    monster_threads.emplace_back(&Monster::simulate, monsters[i], events, map,
                                 pacman, &monsters);
  }

  pacman_thread = std::thread(&Pacman::simulate, pacman, events, map);
  simulation_started = true;
}

void Game::PauseSimulation() {
  if (!simulation_started || events == nullptr) {
    return;
  }

  events->SetGameplayFrozen(true);
  events->Keyreset();
}

void Game::ResumeSimulation(Uint32 paused_duration_ms) {
  if (!simulation_started || events == nullptr) {
    return;
  }

  ShiftPausedTimers(paused_duration_ms);
  events->SetGameplayFrozen(false);
  events->Keyreset();
}

bool Game::IsDiscoEasterEggActive() const {
  return active_disco_easteregg.is_active;
}

void Game::RequestDiscoEasterEggEnd(Uint32 now) {
  if (!active_disco_easteregg.is_active ||
      active_disco_easteregg.is_ending) {
    return;
  }

  UpdateDiscoRotationPhase(now);
  active_disco_easteregg.is_ending = true;
  active_disco_easteregg.ending_started_ticks = now;
  active_disco_easteregg.music_fade_started = true;
#ifdef AUDIO
  if (audio != nullptr) {
    audio->FadeOutDiscoMusic(DISCO_MUSIC_FADE_OUT_MS);
  }
#endif
}

void Game::AdjustDiscoRotationSpeed(double delta) {
  if (!active_disco_easteregg.is_active ||
      active_disco_easteregg.is_ending) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  UpdateDiscoRotationPhase(now);
  active_disco_easteregg.rotation_speed_scale =
      std::clamp(active_disco_easteregg.rotation_speed_scale + delta,
                 DISCO_ROTATION_SPEED_MIN, DISCO_ROTATION_SPEED_MAX);
}

/**
 * @brief Game logic - is called during each cyclee
 * 
 */
void Game::Update() {
  const Uint32 now = SDL_GetTicks();
  if (active_disco_easteregg.is_active) {
    UpdateDiscoEasterEgg(now);
    return;
  }
  CleanupEffects(now);
  UpdateScheduledMonsterBlasts(now);
  UpdateInvulnerability(now);
  UpdateFinalLossSequence(now);
  if (IsFinalLossSequenceActive(now)) {
    return;
  }
  if (pacman->teleport_animation_active &&
      !pacman->teleport_arrival_sound_played &&
      now - pacman->teleport_animation_started_ticks >=
          TELEPORT_ANIMATION_PHASE_MS) {
#ifdef AUDIO
    audio->PlayTeleporterArc();
#endif
    pacman->teleport_arrival_sound_played = true;
  }

  if (pacman->teleport_animation_active &&
      now - pacman->teleport_animation_started_ticks >=
          TELEPORT_ANIMATION_PHASE_MS * 2) {
    pacman->teleport_animation_active = false;
  }
  if (dead) {
    UpdateNuclearExplosion(now);
    UpdateNuclearExplosionB(now);
    UpdateNuclearBombDrop(now);
    UpdateAirstrike(now);
    UpdatePlacedDynamites(now);
    UpdateRockets(now);
    UpdateAlienLaser(now);
    UpdateAlienExplosions(now);
    UpdateExplosionParticles(now);
    return;
  }

  ApplyTeleporters();
  ApplyCheats();
  UpdateInvulnerabilityPotion(now);
  UpdateDynamitePickup(now);
  UpdatePlasticExplosivePickup(now);
  UpdateWalkieTalkiePickup(now);
  UpdateRocketPickup(now);
  UpdateBiohazardPickup(now);
  UpdateNuclearBombPickup(now);
  UpdateLovePotionPickup(now);
  UpdateLifePickup(now);
  UpdateDiscoPickup(now);
  if (active_disco_easteregg.is_active) {
    return;
  }
  TrySpawnAlien(now);
  TryUseBiohazardBeam(now);
  UpdateBiohazardBeam(now);
  TryFireAlienLaser(now);
  UpdateAlienLaser(now);
  UpdateAlienExplosions(now);
  TryUseNuclearBomb(now);
  UpdateNuclearBombDrop(now);
  TryTriggerNuclearExplosion(now);
  UpdateNuclearExplosion(now);
  TryTriggerNuclearExplosionB(now);
  UpdateNuclearExplosionB(now);
  TryUseAirstrike(now);
  TryPlaceDynamite(now);
  TryUsePlasticExplosive(now);
  TryFireRocket(now);
  TryUseLovePotion(now);
  UpdateLoveSmokeProjectiles(now);
  UpdateAirstrike(now);
  UpdatePlacedDynamites(now);
  UpdateRockets(now);
  if (dead || IsFinalLossSequenceActive(now)) {
    return;
  }

  // Check for collision with Goodies ... game is won if all goodies are
  // collected
  win = true;
  for (auto i : goodies) {
    if (i->map_coord.u == pacman->map_coord.u &&
        i->map_coord.v == pacman->map_coord.v && i->is_active) {
      score += SCORE_PER_GOODIE;
#ifdef AUDIO
      audio->PlayCoin();
#endif
      i->Deactivate();
    }
    win = (i->is_active) ? false : win;
  }
  if (win) {
    StopBiohazardBeam();
#ifdef AUDIO
    audio->StopInvulnerabilityLoop();
    audio->PlayWin();
#endif
  }

  TryShootFireballs(now);
  TryShootSlimeballs(now);
  TrySpawnGasClouds(now);
  UpdateFireballs(now);
  UpdateSlimeballs(now);
  UpdateGasClouds(now);
  UpdateElectrifiedMonsters(now);
  UpdateScheduledMonsterBlasts(now);
  UpdateExplosionParticles(now);
  if (dead) {
    return;
  }

  HandleGoatRequests(now);
  UpdateNuclearCraterClouds(now);

  // Check for collision with Monsters ... game is lost if collision occurs.
  // Use precise sub-cell positions and hitbox circles so the collision matches
  // what the player sees on screen.
  const SDL_FPoint pacman_center = PreciseWorldCenter(pacman);
  for (auto i : monsters) {
    if (!i->is_alive || i->is_electrified) {
      continue;
    }
    const SDL_FPoint monster_center = PreciseWorldCenter(i);
    if (CirclesOverlap(pacman_center, PACMAN_HITBOX_RADIUS_CELLS,
                       monster_center, MONSTER_HITBOX_RADIUS_CELLS)) {
      if (i->monster_char == GOAT) {
        // Goat collisions are non-lethal: charging/sliding goat punches
        // pacman, then ignores him for a grace period. Other goat states
        // produce no contact effect.
        const bool active = (i->goat_state == Monster::GoatState::Charging ||
                             i->goat_state == Monster::GoatState::Sliding);
        if (active && now >= i->goat_ignore_player_until_ticks) {
#ifdef AUDIO
          if (audio != nullptr) {
            audio->PlayPunch();
          }
#endif
          pacman->paralyzed_until_ticks =
              std::max(pacman->paralyzed_until_ticks,
                       now + PACMAN_STUN_DURATION_MS);
          i->goat_ignore_player_until_ticks = now + GOAT_PLAYER_GRACE_MS;
          i->goat_state = Monster::GoatState::PostHitGrace;
          i->goat_is_jumping = false;
          i->px_delta.x = 0;
          i->px_delta.y = 0;
        }
        continue;
      }
      if (!IsPacmanInvulnerable(now)) {
        TriggerLoss(pacman->map_coord, now);
      }
    }
  }
}

void Game::ShiftPausedTimers(Uint32 paused_duration_ms) {
  if (paused_duration_ms == 0) {
    return;
  }

  ShiftActiveTicks(last_update_ticks, paused_duration_ms);
  ShiftActiveTicks(death_started_ticks, paused_duration_ms);
  ShiftActiveTicks(life_recovery_until_ticks, paused_duration_ms);
  ShiftActiveTicks(final_loss_commits_at_ticks, paused_duration_ms);

  if (pacman != nullptr) {
    ShiftActiveTicks(pacman->paralyzed_until_ticks, paused_duration_ms);
    ShiftActiveTicks(pacman->invulnerable_until_ticks, paused_duration_ms);
    ShiftActiveTicks(pacman->slimed_until_ticks, paused_duration_ms);
    ShiftActiveTicks(pacman->teleport_animation_started_ticks,
                     paused_duration_ms);
  }

  for (Monster *monster : monsters) {
    if (monster == nullptr) {
      continue;
    }
    ShiftActiveTicks(monster->last_fireball_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->next_gas_cloud_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->death_started_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->scheduled_dynamite_blast_ticks,
                     paused_duration_ms);
    ShiftActiveTicks(monster->electrified_started_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->biohazard_paralyzed_until_ticks,
                     paused_duration_ms);
    ShiftActiveTicks(monster->next_grazing_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->grazing_until_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->goat_stun_until_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->goat_recovery_until_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->goat_ignore_player_until_ticks,
                     paused_duration_ms);
    ShiftActiveTicks(monster->goat_love_started_ticks, paused_duration_ms);
    ShiftActiveTicks(monster->goat_love_attack_at_ticks, paused_duration_ms);
  }

  for (Fireball &fireball : fireballs) {
    ShiftActiveTicks(fireball.segment_started_ticks, paused_duration_ms);
  }
  for (Slimeball &slimeball : slimeballs) {
    ShiftActiveTicks(slimeball.segment_started_ticks, paused_duration_ms);
  }
  for (GasCloud &cloud : gas_clouds) {
    ShiftActiveTicks(cloud.started_ticks, paused_duration_ms);
    ShiftActiveTicks(cloud.fade_started_ticks, paused_duration_ms);
  }
  for (ExplosionParticle &particle : explosion_particles) {
    ShiftActiveTicks(particle.spawned_ticks, paused_duration_ms);
    ShiftActiveTicks(particle.last_smoke_spawn_ticks, paused_duration_ms);
  }
  for (ExplosionSmokePuff &puff : explosion_smoke_puffs) {
    ShiftActiveTicks(puff.spawned_ticks, paused_duration_ms);
  }
  ShiftActiveTicks(explosion_particles_last_update_ticks, paused_duration_ms);

  ShiftActiveTicks(invulnerability_potion.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(invulnerability_potion.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(invulnerability_potion.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(dynamite_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(dynamite_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(dynamite_pickup.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(plastic_explosive_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(plastic_explosive_pickup.fade_started_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(plastic_explosive_pickup.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(walkie_talkie_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(walkie_talkie_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(walkie_talkie_pickup.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(rocket_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(rocket_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(rocket_pickup.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(biohazard_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(biohazard_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(biohazard_pickup.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(nuclear_bomb_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(nuclear_bomb_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(nuclear_bomb_pickup.next_spawn_ticks, paused_duration_ms);

  ShiftActiveTicks(love_potion_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(love_potion_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(love_potion_pickup.next_spawn_ticks, paused_duration_ms);
  ShiftActiveTicks(life_pickup.appeared_ticks, paused_duration_ms);
  ShiftActiveTicks(life_pickup.fade_started_ticks, paused_duration_ms);
  ShiftActiveTicks(life_pickup.next_spawn_ticks, paused_duration_ms);
  ShiftActiveTicks(nuclear_bomb_target_marker.marked_ticks, paused_duration_ms);
  ShiftActiveTicks(active_nuclear_bomb_drop.alarm_started_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_bomb_drop.drop_started_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_bomb_drop.impact_ticks, paused_duration_ms);

  for (PlacedDynamite &placed_dynamite : placed_dynamites) {
    ShiftActiveTicks(placed_dynamite.placed_ticks, paused_duration_ms);
    ShiftActiveTicks(placed_dynamite.explode_at_ticks, paused_duration_ms);
  }

  ShiftActiveTicks(placed_plastic_explosive.placed_ticks, paused_duration_ms);

  ShiftActiveTicks(active_airstrike.started_ticks, paused_duration_ms);
  ShiftActiveTicks(active_airstrike.plane_launch_ticks, paused_duration_ms);
  ShiftActiveTicks(active_airstrike.overflight_ticks, paused_duration_ms);
  ShiftActiveTicks(active_airstrike.plane_finished_ticks, paused_duration_ms);
  ShiftActiveTicks(active_airstrike.finished_ticks, paused_duration_ms);
  for (AirstrikeBomb &bomb : active_airstrike.bombs) {
    ShiftActiveTicks(bomb.release_ticks, paused_duration_ms);
    ShiftActiveTicks(bomb.explode_ticks, paused_duration_ms);
  }

  for (RocketProjectile &rocket : active_rockets) {
    ShiftActiveTicks(rocket.segment_started_ticks, paused_duration_ms);
    ShiftActiveTicks(rocket.last_smoke_spawn_ticks, paused_duration_ms);
  }

  for (LoveSmokeProjectile &smoke : active_love_smokes) {
    ShiftActiveTicks(smoke.spawned_ticks, paused_duration_ms);
  }

  ShiftActiveTicks(active_alien_laser.started_ticks, paused_duration_ms);
  ShiftActiveTicks(active_alien_laser.visible_until_ticks, paused_duration_ms);
  for (AlienAgent &alien : aliens) {
    ShiftActiveTicks(alien.spawned_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.animation_started_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.animation_until_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.next_animation_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.thought_started_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.thought_visible_until_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.next_thought_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.scream_trigger_ticks, paused_duration_ms);
    ShiftActiveTicks(alien.explosion_trigger_ticks, paused_duration_ms);
  }

  ShiftActiveTicks(active_biohazard_beam.started_ticks, paused_duration_ms);
  ShiftActiveTicks(active_biohazard_beam.minimum_visible_until_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_biohazard_beam.hit_sequence_started_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_biohazard_beam.locked_until_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion.started_ticks, paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion.last_ring_spawn_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion.last_center_smoke_spawn_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion.last_mushroom_smoke_spawn_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion_b.started_ticks, paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion_b.last_ring_spawn_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion_b.last_trail_spawn_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion_b.last_stem_spawn_ticks,
                   paused_duration_ms);
  ShiftActiveTicks(active_nuclear_explosion_b.last_cap_spawn_ticks,
                   paused_duration_ms);

  for (ScheduledMonsterBlast &blast : scheduled_monster_blasts) {
    ShiftActiveTicks(blast.trigger_ticks, paused_duration_ms);
  }

  for (GameEffect &effect : effects) {
    ShiftActiveTicks(effect.started_ticks, paused_duration_ms);
  }
}

/**
 * @brief Destroy the Game:: Game object
 * 
 */
Game::~Game() {
#ifdef AUDIO
  audio->StopBiohazardBeam();
  audio->StopInvulnerabilityLoop();
#endif
  // std::cout << "Waiting for threads to finish ... \n";
  for (size_t i = 0; i < monster_threads.size(); i++) {
    if (monster_threads[i].joinable()) {
      monster_threads[i].join();
    }
  }
  if (pacman_thread.joinable()) {
    pacman_thread.join();
  }

  // std::cout << "Threads finished.\n";

  for (auto p : monsters) {
    delete p;
  }
  for (auto p : goodies) {
    delete p;
  }
  delete pacman;
};

bool Game::is_lost() { return dead; }
bool Game::is_won() { return win; }

bool Game::IsFinalLossSequenceActive(Uint32 now) const {
  return !dead && final_loss_commits_at_ticks != 0 &&
         now < final_loss_commits_at_ticks;
}

Uint32 Game::GetDeathStartedTicks() const { return death_started_ticks; }

void Game::TriggerLoss(MapCoord coord, Uint32 now) {
  if (dead || IsFinalLossSequenceActive(now)) {
    return;
  }

  if (remaining_lives > 1) {
    ActivateLifeRecovery(now);
#ifdef AUDIO
    audio->PlayLifeLost();
#endif
    return;
  }

  death_coord = coord;
  death_started_ticks = now;
  final_loss_commits_at_ticks = now + FINAL_LOSS_SOUND_LEAD_IN_MS;
  pacman->px_delta.x = 0;
  pacman->px_delta.y = 0;
  pacman->teleport_animation_active = false;
  pacman->teleport_animation_started_ticks = 0;
  pacman->teleport_animation_from_coord = pacman->map_coord;
  pacman->teleport_animation_to_coord = pacman->map_coord;
  pacman->teleport_arrival_sound_played = false;
  if (events != nullptr) {
    events->SetGameplayFrozen(true);
    events->Keyreset();
  }
  StopBiohazardBeam();
#ifdef AUDIO
  audio->StopInvulnerabilityLoop();
  audio->PlayGameOver();
#endif
}

void Game::ApplyTeleporters() {
  TryTeleportElement(pacman);
}

void Game::TryTeleportElement(MapElement *element) {
  if (element == nullptr) {
    return;
  }

  MapCoord destination;
  char teleporter_digit = '\0';
  if (!map->TryGetTeleporterDestination(element->map_coord, destination,
                                        teleporter_digit)) {
    element->active_teleporter_digit = '\0';
    return;
  }

  if (element->active_teleporter_digit == teleporter_digit) {
    return;
  }

  const MapCoord source_coord = element->map_coord;
  element->map_coord = destination;
  element->px_delta.x = 0;
  element->px_delta.y = 0;
  element->active_teleporter_digit = teleporter_digit;

  if (element == pacman) {
    pacman->teleport_animation_active = true;
    pacman->teleport_animation_started_ticks = SDL_GetTicks();
    pacman->teleport_animation_from_coord = source_coord;
    pacman->teleport_animation_to_coord = destination;
    pacman->teleport_arrival_sound_played = false;
    pacman->paralyzed_until_ticks =
        std::max(pacman->paralyzed_until_ticks,
                 pacman->teleport_animation_started_ticks +
                     TELEPORT_ANIMATION_PHASE_MS * 2);
#ifdef AUDIO
    audio->PlayTeleporterZap();
#endif
  }
}

void Game::TryShootFireballs(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  for (auto monster : monsters) {
    if (!monster->is_alive || monster->is_electrified ||
        monster->biohazard_paralyzed_until_ticks > now ||
        monster->monster_char != MONSTER_MANY) {
      continue;
    }

    if (monster->last_fireball_ticks != 0 &&
        now - monster->last_fireball_ticks < tuning.fireball_cooldown_ms) {
      continue;
    }

    Directions direction = Directions::None;
    if (!HasLineOfSight(map, monster->map_coord, pacman->map_coord, direction)) {
      continue;
    }

    fireballs.push_back(
        {monster->map_coord, direction, now, monster->id,
         tuning.fireball_step_duration_ms});
    monster->last_fireball_ticks = now;
#ifdef AUDIO
    audio->PlayMonsterShot();
#endif
  }
}

void Game::TryShootSlimeballs(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  for (auto monster : monsters) {
    if (!monster->is_alive || monster->is_electrified ||
        monster->biohazard_paralyzed_until_ticks > now ||
        monster->monster_char != MONSTER_EXTRA) {
      continue;
    }

    if (monster->last_fireball_ticks != 0 &&
        now - monster->last_fireball_ticks < tuning.fireball_cooldown_ms) {
      continue;
    }

    Directions direction = Directions::None;
    if (!HasLineOfSight(map, monster->map_coord, pacman->map_coord, direction)) {
      continue;
    }

    slimeballs.push_back(
        {monster->map_coord, direction, now, monster->id,
         tuning.fireball_step_duration_ms});
    monster->last_fireball_ticks = now;
#ifdef AUDIO
    audio->PlaySlimeShot();
#endif
  }
}

void Game::TrySpawnGasClouds(Uint32 now) {
  for (auto monster : monsters) {
    if (!monster->is_alive || monster->is_electrified ||
        monster->biohazard_paralyzed_until_ticks > now ||
        monster->monster_char != MONSTER_MEDIUM) {
      continue;
    }

    if (monster->next_gas_cloud_ticks == 0 ||
        now < monster->next_gas_cloud_ticks) {
      continue;
    }

    gas_clouds.push_back(
        {monster->map_coord, now, now + GAS_CLOUD_ACTIVE_MS,
         static_cast<int>(monster->id * 43 + now % 997), monster->id, false,
         false});
    monster->next_gas_cloud_ticks =
        now + RandomInterval(MONSTER_GAS_MIN_INTERVAL_MS,
                             MONSTER_GAS_MAX_INTERVAL_MS);
#ifdef AUDIO
    audio->PlayMonsterFart();
#endif
  }
}

void Game::ScheduleNextInvulnerabilityPotionSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  invulnerability_potion.next_spawn_ticks =
      now + RandomScaledInterval(BLUE_POTION_SPAWN_MIN_INTERVAL_MS,
                                 BLUE_POTION_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextDynamiteSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  dynamite_pickup.next_spawn_ticks =
      now + RandomScaledInterval(DYNAMITE_SPAWN_MIN_INTERVAL_MS,
                                 DYNAMITE_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextPlasticExplosiveSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  plastic_explosive_pickup.next_spawn_ticks =
      now + RandomScaledInterval(PLASTIC_EXPLOSIVE_SPAWN_MIN_INTERVAL_MS,
                                 PLASTIC_EXPLOSIVE_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextWalkieTalkieSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  walkie_talkie_pickup.next_spawn_ticks =
      now + RandomScaledInterval(WALKIE_TALKIE_SPAWN_MIN_INTERVAL_MS,
                                 WALKIE_TALKIE_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextRocketSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  rocket_pickup.next_spawn_ticks =
      now + RandomScaledInterval(ROCKET_SPAWN_MIN_INTERVAL_MS,
                                 ROCKET_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextBiohazardSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  biohazard_pickup.next_spawn_ticks =
      now + RandomScaledInterval(BIOHAZARD_SPAWN_MIN_INTERVAL_MS,
                                 BIOHAZARD_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextNuclearBombSpawn(Uint32 now) {
  if (nuclear_bomb_collected_once) {
    nuclear_bomb_pickup.next_spawn_ticks = 0;
    return;
  }

  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  nuclear_bomb_pickup.next_spawn_ticks =
      now + RandomScaledInterval(NUCLEAR_BOMB_SPAWN_MIN_INTERVAL_MS,
                                 NUCLEAR_BOMB_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextLifePickupSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  life_pickup.next_spawn_ticks =
      now + RandomScaledInterval(LIFE_PICKUP_SPAWN_MIN_INTERVAL_MS,
                                 LIFE_PICKUP_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextLovePotionSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  love_potion_pickup.next_spawn_ticks =
      now + RandomScaledInterval(LOVE_POTION_SPAWN_MIN_INTERVAL_MS,
                                 LOVE_POTION_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

void Game::ScheduleNextDiscoPickupSpawn(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  disco_pickup.next_spawn_ticks =
      now + RandomScaledInterval(DISCO_PICKUP_SPAWN_MIN_INTERVAL_MS,
                                 DISCO_PICKUP_SPAWN_MAX_INTERVAL_MS,
                                 tuning.extra_spawn_interval_scale);
}

bool Game::IsWithinRadius(MapCoord center, MapCoord target,
                          int radius_cells) const {
  const int clamped_radius = std::max(0, radius_cells);
  return DistanceSquared(center, target) <= clamped_radius * clamped_radius;
}

bool Game::IsWithinDynamiteRadius(MapCoord center, MapCoord target) const {
  return IsWithinRadius(center, target, DYNAMITE_EXPLOSION_RADIUS_CELLS);
}

bool Game::IsCraterCell(MapCoord coord) const {
  if (!IsInsideMapBounds(map, coord)) {
    return true;
  }

  return map->map_entry(static_cast<size_t>(coord.u),
                        static_cast<size_t>(coord.v)) ==
         ElementType::TYPE_CRATER;
}

bool Game::NuclearCraterTouchesCell(const NuclearCrater &crater,
                                    MapCoord coord) const {
  const int sample_steps = std::max(2, NUCLEAR_CRATER_EDGE_SAMPLE_STEPS);
  for (int sample_y = 0; sample_y <= sample_steps; ++sample_y) {
    for (int sample_x = 0; sample_x <= sample_steps; ++sample_x) {
      const SDL_FPoint point{
          static_cast<float>(coord.v) +
              static_cast<float>(sample_x) / static_cast<float>(sample_steps),
          static_cast<float>(coord.u) +
              static_cast<float>(sample_y) / static_cast<float>(sample_steps)};
      if (NuclearCraterContainsPoint(crater, point)) {
        return true;
      }
    }
  }

  return false;
}

void Game::RemoveUnreachableGoodiesAfterCrater() {
  if (map == nullptr || pacman == nullptr ||
      !IsInsideMapBounds(map, pacman->map_coord)) {
    return;
  }

  const size_t row_count = map->get_map_rows();
  const size_t col_count = map->get_map_cols();
  std::vector<std::vector<bool>> reachable(
      row_count, std::vector<bool>(col_count, false));
  std::vector<MapCoord> frontier;
  frontier.push_back(pacman->map_coord);
  reachable[static_cast<size_t>(pacman->map_coord.u)]
           [static_cast<size_t>(pacman->map_coord.v)] = true;

  for (size_t index = 0; index < frontier.size(); ++index) {
    const MapCoord current = frontier[index];
    for (Directions direction :
         {Directions::Up, Directions::Down, Directions::Left,
          Directions::Right}) {
      const MapCoord next = StepCoord(current, direction);
      if (!IsInsideMapBounds(map, next) ||
          IsBlockingMapElement(map->map_entry(static_cast<size_t>(next.u),
                                             static_cast<size_t>(next.v))) ||
          reachable[static_cast<size_t>(next.u)]
                   [static_cast<size_t>(next.v)]) {
        continue;
      }

      reachable[static_cast<size_t>(next.u)]
               [static_cast<size_t>(next.v)] = true;
      frontier.push_back(next);
    }

    MapCoord teleporter_destination;
    char teleporter_digit = '\0';
    if (map->TryGetTeleporterDestination(current, teleporter_destination,
                                         teleporter_digit) &&
        IsInsideMapBounds(map, teleporter_destination) &&
        !IsBlockingMapElement(map->map_entry(
            static_cast<size_t>(teleporter_destination.u),
            static_cast<size_t>(teleporter_destination.v))) &&
        !reachable[static_cast<size_t>(teleporter_destination.u)]
                  [static_cast<size_t>(teleporter_destination.v)]) {
      reachable[static_cast<size_t>(teleporter_destination.u)]
               [static_cast<size_t>(teleporter_destination.v)] = true;
      frontier.push_back(teleporter_destination);
    }
  }

  for (auto *goodie : goodies) {
    if (goodie == nullptr || !goodie->is_active ||
        !IsInsideMapBounds(map, goodie->map_coord)) {
      continue;
    }

    if (reachable[static_cast<size_t>(goodie->map_coord.u)]
                 [static_cast<size_t>(goodie->map_coord.v)]) {
      continue;
    }

    goodie->Deactivate();
    if (map->map_entry(static_cast<size_t>(goodie->map_coord.u),
                       static_cast<size_t>(goodie->map_coord.v)) ==
        ElementType::TYPE_GOODIE) {
      map->SetEntry(goodie->map_coord, ElementType::TYPE_PATH);
    }
  }
}

bool Game::IsCellFreeForDynamitePickup(MapCoord coord) const {
  if (map == nullptr ||
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v)) !=
          ElementType::TYPE_PATH) {
    return false;
  }

  if (SameCoord(coord, pacman->map_coord)) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  if (invulnerability_potion.is_visible &&
      SameCoord(coord, invulnerability_potion.coord)) {
    return false;
  }

  if (plastic_explosive_pickup.is_visible &&
      SameCoord(coord, plastic_explosive_pickup.coord)) {
    return false;
  }

  if (walkie_talkie_pickup.is_visible &&
      SameCoord(coord, walkie_talkie_pickup.coord)) {
    return false;
  }

  if (rocket_pickup.is_visible && SameCoord(coord, rocket_pickup.coord)) {
    return false;
  }

  if (biohazard_pickup.is_visible && SameCoord(coord, biohazard_pickup.coord)) {
    return false;
  }

  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }

  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }
  if (disco_pickup.is_visible && SameCoord(coord, disco_pickup.coord)) {
    return false;
  }

  for (const auto *monster : monsters) {
    if ((monster->is_alive ||
         monster->scheduled_dynamite_blast_ticks != 0) &&
        SameCoord(coord, monster->map_coord)) {
      return false;
    }
  }

  for (const auto &fireball : fireballs) {
    if (SameCoord(coord, fireball.current_coord)) {
      return false;
    }
  }

  for (const auto &slimeball : slimeballs) {
    if (SameCoord(coord, slimeball.current_coord)) {
      return false;
    }
  }

  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  for (const auto &cloud : gas_clouds) {
    if (SameCoord(coord, cloud.coord)) {
      return false;
    }
  }

  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }

  if (plastic_explosive_is_armed &&
      SameCoord(coord, placed_plastic_explosive.coord)) {
    return false;
  }

  return true;
}

bool Game::IsCellFreeForInvulnerabilityPotion(MapCoord coord) const {
  if (map == nullptr ||
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v)) !=
          ElementType::TYPE_PATH) {
    return false;
  }

  if (SameCoord(coord, pacman->map_coord)) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  for (const auto *monster : monsters) {
    if (monster->is_alive && SameCoord(coord, monster->map_coord)) {
      return false;
    }
  }

  for (const auto &fireball : fireballs) {
    if (SameCoord(coord, fireball.current_coord)) {
      return false;
    }
  }

  for (const auto &slimeball : slimeballs) {
    if (SameCoord(coord, slimeball.current_coord)) {
      return false;
    }
  }

  for (const auto &cloud : gas_clouds) {
    if (SameCoord(coord, cloud.coord)) {
      return false;
    }
  }

  if (dynamite_pickup.is_visible && SameCoord(coord, dynamite_pickup.coord)) {
    return false;
  }

  if (plastic_explosive_pickup.is_visible &&
      SameCoord(coord, plastic_explosive_pickup.coord)) {
    return false;
  }

  if (walkie_talkie_pickup.is_visible &&
      SameCoord(coord, walkie_talkie_pickup.coord)) {
    return false;
  }

  if (rocket_pickup.is_visible && SameCoord(coord, rocket_pickup.coord)) {
    return false;
  }

  if (biohazard_pickup.is_visible && SameCoord(coord, biohazard_pickup.coord)) {
    return false;
  }

  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }

  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }

  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }

  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  if (plastic_explosive_is_armed &&
      SameCoord(coord, placed_plastic_explosive.coord)) {
    return false;
  }

  return true;
}

bool Game::IsCellFreeForPlasticExplosivePickup(MapCoord coord) const {
  if (map == nullptr ||
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v)) !=
          ElementType::TYPE_PATH) {
    return false;
  }

  if (SameCoord(coord, pacman->map_coord)) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  if (invulnerability_potion.is_visible &&
      SameCoord(coord, invulnerability_potion.coord)) {
    return false;
  }

  if (dynamite_pickup.is_visible && SameCoord(coord, dynamite_pickup.coord)) {
    return false;
  }

  if (walkie_talkie_pickup.is_visible &&
      SameCoord(coord, walkie_talkie_pickup.coord)) {
    return false;
  }

  if (rocket_pickup.is_visible && SameCoord(coord, rocket_pickup.coord)) {
    return false;
  }

  if (biohazard_pickup.is_visible && SameCoord(coord, biohazard_pickup.coord)) {
    return false;
  }

  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }

  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }
  if (disco_pickup.is_visible && SameCoord(coord, disco_pickup.coord)) {
    return false;
  }

  for (const auto *monster : monsters) {
    if ((monster->is_alive ||
         monster->scheduled_dynamite_blast_ticks != 0) &&
        SameCoord(coord, monster->map_coord)) {
      return false;
    }
  }

  for (const auto &fireball : fireballs) {
    if (SameCoord(coord, fireball.current_coord)) {
      return false;
    }
  }

  for (const auto &slimeball : slimeballs) {
    if (SameCoord(coord, slimeball.current_coord)) {
      return false;
    }
  }

  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  for (const auto &cloud : gas_clouds) {
    if (SameCoord(coord, cloud.coord)) {
      return false;
    }
  }

  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }

  if (plastic_explosive_is_armed &&
      SameCoord(coord, placed_plastic_explosive.coord)) {
    return false;
  }

  return true;
}

bool Game::IsCellFreeForWalkieTalkiePickup(MapCoord coord) const {
  if (map == nullptr ||
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v)) !=
          ElementType::TYPE_PATH) {
    return false;
  }

  if (SameCoord(coord, pacman->map_coord)) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  if (invulnerability_potion.is_visible &&
      SameCoord(coord, invulnerability_potion.coord)) {
    return false;
  }

  if (dynamite_pickup.is_visible && SameCoord(coord, dynamite_pickup.coord)) {
    return false;
  }

  if (plastic_explosive_pickup.is_visible &&
      SameCoord(coord, plastic_explosive_pickup.coord)) {
    return false;
  }

  if (rocket_pickup.is_visible && SameCoord(coord, rocket_pickup.coord)) {
    return false;
  }

  if (biohazard_pickup.is_visible && SameCoord(coord, biohazard_pickup.coord)) {
    return false;
  }

  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }

  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }

  for (const auto *monster : monsters) {
    if ((monster->is_alive ||
         monster->scheduled_dynamite_blast_ticks != 0) &&
        SameCoord(coord, monster->map_coord)) {
      return false;
    }
  }

  for (const auto &fireball : fireballs) {
    if (SameCoord(coord, fireball.current_coord)) {
      return false;
    }
  }

  for (const auto &slimeball : slimeballs) {
    if (SameCoord(coord, slimeball.current_coord)) {
      return false;
    }
  }

  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  for (const auto &cloud : gas_clouds) {
    if (SameCoord(coord, cloud.coord)) {
      return false;
    }
  }

  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }

  if (plastic_explosive_is_armed &&
      SameCoord(coord, placed_plastic_explosive.coord)) {
    return false;
  }

  return true;
}

bool Game::IsCellFreeForRocketPickup(MapCoord coord) const {
  if (map == nullptr ||
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v)) !=
          ElementType::TYPE_PATH) {
    return false;
  }

  if (SameCoord(coord, pacman->map_coord)) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  if (invulnerability_potion.is_visible &&
      SameCoord(coord, invulnerability_potion.coord)) {
    return false;
  }

  if (dynamite_pickup.is_visible && SameCoord(coord, dynamite_pickup.coord)) {
    return false;
  }

  if (plastic_explosive_pickup.is_visible &&
      SameCoord(coord, plastic_explosive_pickup.coord)) {
    return false;
  }

  if (walkie_talkie_pickup.is_visible &&
      SameCoord(coord, walkie_talkie_pickup.coord)) {
    return false;
  }

  if (biohazard_pickup.is_visible && SameCoord(coord, biohazard_pickup.coord)) {
    return false;
  }

  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }

  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }

  for (const auto *monster : monsters) {
    if ((monster->is_alive ||
         monster->scheduled_dynamite_blast_ticks != 0) &&
        SameCoord(coord, monster->map_coord)) {
      return false;
    }
  }

  for (const auto &fireball : fireballs) {
    if (SameCoord(coord, fireball.current_coord)) {
      return false;
    }
  }

  for (const auto &slimeball : slimeballs) {
    if (SameCoord(coord, slimeball.current_coord)) {
      return false;
    }
  }

  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  for (const auto &cloud : gas_clouds) {
    if (SameCoord(coord, cloud.coord)) {
      return false;
    }
  }

  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }

  if (plastic_explosive_is_armed &&
      SameCoord(coord, placed_plastic_explosive.coord)) {
    return false;
  }

  return true;
}

bool Game::IsCellFreeForBiohazardPickup(MapCoord coord) const {
  if (map == nullptr ||
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v)) !=
          ElementType::TYPE_PATH) {
    return false;
  }

  if (SameCoord(coord, pacman->map_coord)) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  if (invulnerability_potion.is_visible &&
      SameCoord(coord, invulnerability_potion.coord)) {
    return false;
  }

  if (dynamite_pickup.is_visible && SameCoord(coord, dynamite_pickup.coord)) {
    return false;
  }

  if (plastic_explosive_pickup.is_visible &&
      SameCoord(coord, plastic_explosive_pickup.coord)) {
    return false;
  }

  if (walkie_talkie_pickup.is_visible &&
      SameCoord(coord, walkie_talkie_pickup.coord)) {
    return false;
  }

  if (rocket_pickup.is_visible && SameCoord(coord, rocket_pickup.coord)) {
    return false;
  }

  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }

  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }
  if (disco_pickup.is_visible && SameCoord(coord, disco_pickup.coord)) {
    return false;
  }

  for (const auto *monster : monsters) {
    if ((monster->is_alive ||
         monster->scheduled_dynamite_blast_ticks != 0) &&
        SameCoord(coord, monster->map_coord)) {
      return false;
    }
  }

  for (const auto &fireball : fireballs) {
    if (SameCoord(coord, fireball.current_coord)) {
      return false;
    }
  }

  for (const auto &slimeball : slimeballs) {
    if (SameCoord(coord, slimeball.current_coord)) {
      return false;
    }
  }

  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  for (const auto &cloud : gas_clouds) {
    if (SameCoord(coord, cloud.coord)) {
      return false;
    }
  }

  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }

  if (plastic_explosive_is_armed &&
      SameCoord(coord, placed_plastic_explosive.coord)) {
    return false;
  }

  return true;
}

bool Game::IsCellFreeForNuclearBombPickup(MapCoord coord) const {
  return !nuclear_bomb_collected_once && IsCellFreeForBiohazardPickup(coord);
}

bool Game::IsCellFreeForLovePotionPickup(MapCoord coord) const {
  return IsCellFreeForBiohazardPickup(coord);
}

bool Game::IsCellFreeForLifePickup(MapCoord coord) const {
  return IsCellFreeForBiohazardPickup(coord);
}

bool Game::IsCellFreeForDiscoPickup(MapCoord coord) const {
  return IsCellFreeForBiohazardPickup(coord);
}

bool Game::IsCellOccupiedByAlien(MapCoord coord) const {
  for (const AlienAgent &alien : aliens) {
    const bool occupies_cell =
        alien.is_alive ||
        (!alien.explosion_spawned && alien.explosion_trigger_ticks != 0);
    if (occupies_cell && SameCoord(coord, alien.coord)) {
      return true;
    }
  }
  return false;
}

bool Game::IsCellFreeForAlienSpawn(MapCoord coord) const {
  if (!IsCellFreeForBiohazardPickup(coord)) {
    return false;
  }

  return !IsCellOccupiedByAlien(coord);
}

bool Game::CanPlacePlasticExplosiveAt(MapCoord coord) const {
  if (!IsInsideMapBounds(map, coord)) {
    return false;
  }

  const ElementType entry =
      map->map_entry(static_cast<size_t>(coord.u), static_cast<size_t>(coord.v));
  if (entry == ElementType::TYPE_TELEPORTER ||
      entry == ElementType::TYPE_CRATER) {
    return false;
  }

  if (IsCellOccupiedByAlien(coord)) {
    return false;
  }

  if (dynamite_pickup.is_visible && SameCoord(coord, dynamite_pickup.coord)) {
    return false;
  }
  if (invulnerability_potion.is_visible &&
      SameCoord(coord, invulnerability_potion.coord)) {
    return false;
  }
  if (plastic_explosive_pickup.is_visible &&
      SameCoord(coord, plastic_explosive_pickup.coord)) {
    return false;
  }
  if (walkie_talkie_pickup.is_visible &&
      SameCoord(coord, walkie_talkie_pickup.coord)) {
    return false;
  }
  if (rocket_pickup.is_visible && SameCoord(coord, rocket_pickup.coord)) {
    return false;
  }
  if (biohazard_pickup.is_visible && SameCoord(coord, biohazard_pickup.coord)) {
    return false;
  }
  if (nuclear_bomb_pickup.is_visible &&
      SameCoord(coord, nuclear_bomb_pickup.coord)) {
    return false;
  }
  if (love_potion_pickup.is_visible &&
      SameCoord(coord, love_potion_pickup.coord)) {
    return false;
  }
  if (life_pickup.is_visible &&
      SameCoord(coord, life_pickup.coord)) {
    return false;
  }
  for (const auto &dynamite : placed_dynamites) {
    if (SameCoord(coord, dynamite.coord)) {
      return false;
    }
  }
  for (const auto &rocket : active_rockets) {
    if (SameCoord(coord, rocket.current_coord)) {
      return false;
    }
  }

  return true;
}

void Game::TrySpawnInvulnerabilityPotion(Uint32 now) {
  if (invulnerability_potion.is_visible ||
      now < invulnerability_potion.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForInvulnerabilityPotion(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    invulnerability_potion.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  invulnerability_potion.coord = candidates[distribution(RandomGenerator())];
  invulnerability_potion.appeared_ticks = now;
  invulnerability_potion.fade_started_ticks = 0;
  invulnerability_potion.animation_seed =
      static_cast<int>((now % 997) + invulnerability_potion.coord.u * 31 +
                       invulnerability_potion.coord.v * 17);
  invulnerability_potion.is_visible = true;
  invulnerability_potion.is_fading = false;
#ifdef AUDIO
  audio->PlayPotionSpawn();
#endif
}

void Game::ActivateInvulnerability(Uint32 now) {
  pacman->invulnerable_until_ticks = now + PACMAN_INVULNERABLE_MS;
#ifdef AUDIO
  audio->StartInvulnerabilityLoop();
#endif
}

void Game::ActivateLifeRecovery(Uint32 now) {
  if (pacman == nullptr) {
    return;
  }

  remaining_lives = std::max(PLAYER_LIVES_MIN, remaining_lives - 1);
  life_recovery_until_ticks =
      std::max(life_recovery_until_ticks,
               now + PACMAN_EXTRA_LIFE_INVULNERABLE_MS);

  pacman->px_delta.x = 0;
  pacman->px_delta.y = 0;
  pacman->paralyzed_until_ticks = 0;
  pacman->slimed_until_ticks = 0;
  pacman->teleport_animation_active = false;
  pacman->teleport_animation_started_ticks = 0;
  pacman->teleport_animation_from_coord = pacman->map_coord;
  pacman->teleport_animation_to_coord = pacman->map_coord;
  pacman->teleport_arrival_sound_played = false;
  if (events != nullptr) {
    events->Keyreset();
  }
}

void Game::TrySpawnDynamite(Uint32 now) {
  if (dynamite_pickup.is_visible || now < dynamite_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForDynamitePickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    dynamite_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  dynamite_pickup.coord = candidates[distribution(RandomGenerator())];
  dynamite_pickup.appeared_ticks = now;
  dynamite_pickup.fade_started_ticks = 0;
  dynamite_pickup.animation_seed = static_cast<int>(
      (now % 997) + dynamite_pickup.coord.u * 19 + dynamite_pickup.coord.v * 37);
  dynamite_pickup.is_visible = true;
  dynamite_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayDynamiteSpawn();
#endif
}

void Game::UpdateDynamitePickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnDynamite(now);
  if (!dynamite_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, dynamite_pickup.coord)) {
    dynamite_inventory++;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    dynamite_pickup.is_visible = false;
    dynamite_pickup.is_fading = false;
    dynamite_pickup.fade_started_ticks = 0;
    ScheduleNextDynamiteSpawn(now);
    return;
  }

  if (!dynamite_pickup.is_fading &&
      now - dynamite_pickup.appeared_ticks >= tuning.pickup_visible_ms) {
    dynamite_pickup.is_fading = true;
    dynamite_pickup.fade_started_ticks = now;
  }

  if (dynamite_pickup.is_fading &&
      now - dynamite_pickup.fade_started_ticks >= DYNAMITE_FADE_MS) {
    dynamite_pickup.is_visible = false;
    dynamite_pickup.is_fading = false;
    dynamite_pickup.fade_started_ticks = 0;
    ScheduleNextDynamiteSpawn(now);
  }
}

void Game::TrySpawnPlasticExplosive(Uint32 now) {
  if (plastic_explosive_pickup.is_visible ||
      now < plastic_explosive_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForPlasticExplosivePickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    plastic_explosive_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  plastic_explosive_pickup.coord = candidates[distribution(RandomGenerator())];
  plastic_explosive_pickup.appeared_ticks = now;
  plastic_explosive_pickup.fade_started_ticks = 0;
  plastic_explosive_pickup.animation_seed = static_cast<int>(
      (now % 997) + plastic_explosive_pickup.coord.u * 41 +
      plastic_explosive_pickup.coord.v * 23);
  plastic_explosive_pickup.is_visible = true;
  plastic_explosive_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayPlasticExplosiveSpawn();
#endif
}

void Game::UpdatePlasticExplosivePickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnPlasticExplosive(now);
  if (!plastic_explosive_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, plastic_explosive_pickup.coord)) {
    plastic_explosive_inventory++;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    plastic_explosive_pickup.is_visible = false;
    plastic_explosive_pickup.is_fading = false;
    plastic_explosive_pickup.fade_started_ticks = 0;
    ScheduleNextPlasticExplosiveSpawn(now);
    return;
  }

  if (!plastic_explosive_pickup.is_fading &&
      now - plastic_explosive_pickup.appeared_ticks >=
          tuning.pickup_visible_ms) {
    plastic_explosive_pickup.is_fading = true;
    plastic_explosive_pickup.fade_started_ticks = now;
  }

  if (plastic_explosive_pickup.is_fading &&
      now - plastic_explosive_pickup.fade_started_ticks >=
          PLASTIC_EXPLOSIVE_FADE_MS) {
    plastic_explosive_pickup.is_visible = false;
    plastic_explosive_pickup.is_fading = false;
    plastic_explosive_pickup.fade_started_ticks = 0;
    ScheduleNextPlasticExplosiveSpawn(now);
  }
}

void Game::TrySpawnWalkieTalkie(Uint32 now) {
  if (walkie_talkie_pickup.is_visible ||
      now < walkie_talkie_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForWalkieTalkiePickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    walkie_talkie_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  walkie_talkie_pickup.coord = candidates[distribution(RandomGenerator())];
  walkie_talkie_pickup.appeared_ticks = now;
  walkie_talkie_pickup.fade_started_ticks = 0;
  walkie_talkie_pickup.animation_seed = static_cast<int>(
      (now % 997) + walkie_talkie_pickup.coord.u * 29 +
      walkie_talkie_pickup.coord.v * 43);
  walkie_talkie_pickup.is_visible = true;
  walkie_talkie_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayDynamiteSpawn();
#endif
}

void Game::UpdateWalkieTalkiePickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnWalkieTalkie(now);
  if (!walkie_talkie_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, walkie_talkie_pickup.coord)) {
    airstrike_inventory++;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    walkie_talkie_pickup.is_visible = false;
    walkie_talkie_pickup.is_fading = false;
    walkie_talkie_pickup.fade_started_ticks = 0;
    ScheduleNextWalkieTalkieSpawn(now);
    return;
  }

  if (!walkie_talkie_pickup.is_fading &&
      now - walkie_talkie_pickup.appeared_ticks >= tuning.pickup_visible_ms) {
    walkie_talkie_pickup.is_fading = true;
    walkie_talkie_pickup.fade_started_ticks = now;
  }

  if (walkie_talkie_pickup.is_fading &&
      now - walkie_talkie_pickup.fade_started_ticks >=
          WALKIE_TALKIE_FADE_MS) {
    walkie_talkie_pickup.is_visible = false;
    walkie_talkie_pickup.is_fading = false;
    walkie_talkie_pickup.fade_started_ticks = 0;
    ScheduleNextWalkieTalkieSpawn(now);
  }
}

void Game::TrySpawnRocket(Uint32 now) {
  if (rocket_pickup.is_visible || now < rocket_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForRocketPickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    rocket_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  rocket_pickup.coord = candidates[distribution(RandomGenerator())];
  rocket_pickup.appeared_ticks = now;
  rocket_pickup.fade_started_ticks = 0;
  rocket_pickup.animation_seed = static_cast<int>(
      (now % 997) + rocket_pickup.coord.u * 59 + rocket_pickup.coord.v * 67);
  rocket_pickup.is_visible = true;
  rocket_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayDynamiteSpawn();
#endif
}

void Game::UpdateRocketPickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnRocket(now);
  if (!rocket_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, rocket_pickup.coord)) {
    rocket_inventory++;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    rocket_pickup.is_visible = false;
    rocket_pickup.is_fading = false;
    rocket_pickup.fade_started_ticks = 0;
    ScheduleNextRocketSpawn(now);
    return;
  }

  if (!rocket_pickup.is_fading &&
      now - rocket_pickup.appeared_ticks >= tuning.pickup_visible_ms) {
    rocket_pickup.is_fading = true;
    rocket_pickup.fade_started_ticks = now;
  }

  if (rocket_pickup.is_fading &&
      now - rocket_pickup.fade_started_ticks >= ROCKET_FADE_MS) {
    rocket_pickup.is_visible = false;
    rocket_pickup.is_fading = false;
    rocket_pickup.fade_started_ticks = 0;
    ScheduleNextRocketSpawn(now);
  }
}

void Game::TrySpawnBiohazard(Uint32 now) {
  if (biohazard_pickup.is_visible || now < biohazard_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForBiohazardPickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    biohazard_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  biohazard_pickup.coord = candidates[distribution(RandomGenerator())];
  biohazard_pickup.appeared_ticks = now;
  biohazard_pickup.fade_started_ticks = 0;
  biohazard_pickup.animation_seed =
      static_cast<int>((now % 997) + biohazard_pickup.coord.u * 83 +
                       biohazard_pickup.coord.v * 47);
  biohazard_pickup.is_visible = true;
  biohazard_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayDynamiteSpawn();
#endif
}

void Game::UpdateBiohazardPickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnBiohazard(now);
  if (!biohazard_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, biohazard_pickup.coord)) {
    biohazard_inventory++;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    biohazard_pickup.is_visible = false;
    biohazard_pickup.is_fading = false;
    biohazard_pickup.fade_started_ticks = 0;
    ScheduleNextBiohazardSpawn(now);
    return;
  }

  if (!biohazard_pickup.is_fading &&
      now - biohazard_pickup.appeared_ticks >= tuning.pickup_visible_ms) {
    biohazard_pickup.is_fading = true;
    biohazard_pickup.fade_started_ticks = now;
  }

  if (biohazard_pickup.is_fading &&
      now - biohazard_pickup.fade_started_ticks >= BIOHAZARD_FADE_MS) {
    biohazard_pickup.is_visible = false;
    biohazard_pickup.is_fading = false;
    biohazard_pickup.fade_started_ticks = 0;
    ScheduleNextBiohazardSpawn(now);
  }
}

void Game::TrySpawnNuclearBomb(Uint32 now) {
  if (nuclear_bomb_collected_once || nuclear_bomb_pickup.is_visible ||
      now < nuclear_bomb_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForNuclearBombPickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    nuclear_bomb_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  nuclear_bomb_pickup.coord = candidates[distribution(RandomGenerator())];
  nuclear_bomb_pickup.appeared_ticks = now;
  nuclear_bomb_pickup.fade_started_ticks = 0;
  nuclear_bomb_pickup.animation_seed =
      static_cast<int>((now % 997) + nuclear_bomb_pickup.coord.u * 97 +
                       nuclear_bomb_pickup.coord.v * 71);
  nuclear_bomb_pickup.is_visible = true;
  nuclear_bomb_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayDynamiteSpawn();
#endif
}

void Game::UpdateNuclearBombPickup(Uint32 now) {
  TrySpawnNuclearBomb(now);
  if (!nuclear_bomb_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, nuclear_bomb_pickup.coord)) {
    nuclear_bomb_inventory = 1;
    nuclear_bomb_collected_once = true;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    nuclear_bomb_pickup.is_visible = false;
    nuclear_bomb_pickup.is_fading = false;
    nuclear_bomb_pickup.fade_started_ticks = 0;
    nuclear_bomb_pickup.next_spawn_ticks = 0;
    return;
  }

  if (!nuclear_bomb_pickup.is_fading &&
      now - nuclear_bomb_pickup.appeared_ticks >= NUCLEAR_BOMB_VISIBLE_MS) {
    nuclear_bomb_pickup.is_fading = true;
    nuclear_bomb_pickup.fade_started_ticks = now;
  }

  if (nuclear_bomb_pickup.is_fading &&
      now - nuclear_bomb_pickup.fade_started_ticks >= NUCLEAR_BOMB_FADE_MS) {
    nuclear_bomb_pickup.is_visible = false;
    nuclear_bomb_pickup.is_fading = false;
    nuclear_bomb_pickup.fade_started_ticks = 0;
    ScheduleNextNuclearBombSpawn(now);
  }
}

void Game::TrySpawnLovePotion(Uint32 now) {
  if (love_potion_pickup.is_visible || now < love_potion_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForLovePotionPickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    love_potion_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  love_potion_pickup.coord = candidates[distribution(RandomGenerator())];
  love_potion_pickup.appeared_ticks = now;
  love_potion_pickup.fade_started_ticks = 0;
  love_potion_pickup.animation_seed =
      static_cast<int>((now % 997) + love_potion_pickup.coord.u * 109 +
                       love_potion_pickup.coord.v * 79);
  love_potion_pickup.is_visible = true;
  love_potion_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayPotionSpawn();
#endif
}

void Game::UpdateLovePotionPickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnLovePotion(now);
  if (!love_potion_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, love_potion_pickup.coord)) {
    love_potion_inventory++;
#ifdef AUDIO
    audio->PlayCoin();
#endif
    love_potion_pickup.is_visible = false;
    love_potion_pickup.is_fading = false;
    love_potion_pickup.fade_started_ticks = 0;
    ScheduleNextLovePotionSpawn(now);
    return;
  }

  if (!love_potion_pickup.is_fading &&
      now - love_potion_pickup.appeared_ticks >= tuning.pickup_visible_ms) {
    love_potion_pickup.is_fading = true;
    love_potion_pickup.fade_started_ticks = now;
  }

  if (love_potion_pickup.is_fading &&
      now - love_potion_pickup.fade_started_ticks >= LOVE_POTION_FADE_MS) {
    love_potion_pickup.is_visible = false;
    love_potion_pickup.is_fading = false;
    love_potion_pickup.fade_started_ticks = 0;
    ScheduleNextLovePotionSpawn(now);
  }
}

void Game::TrySpawnLifePickup(Uint32 now) {
  if (life_pickup.is_visible || now < life_pickup.next_spawn_ticks) {
    return;
  }
  if (remaining_lives >= PLAYER_LIVES_MAX) {
    life_pickup.next_spawn_ticks = now + 2000;
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForLifePickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    life_pickup.next_spawn_ticks = now + 1000;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  life_pickup.coord = candidates[distribution(RandomGenerator())];
  life_pickup.appeared_ticks = now;
  life_pickup.fade_started_ticks = 0;
  life_pickup.animation_seed =
      static_cast<int>((now % 991) + life_pickup.coord.u * 113 +
                       life_pickup.coord.v * 73);
  life_pickup.is_visible = true;
  life_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayPotionSpawn();
#endif
}

void Game::UpdateLifePickup(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnLifePickup(now);
  if (!life_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, life_pickup.coord)) {
    if (remaining_lives < PLAYER_LIVES_MAX) {
      remaining_lives++;
    }
#ifdef AUDIO
    audio->PlayCoin();
#endif
    life_pickup.is_visible = false;
    life_pickup.is_fading = false;
    life_pickup.fade_started_ticks = 0;
    ScheduleNextLifePickupSpawn(now);
    return;
  }

  if (!life_pickup.is_fading &&
      now - life_pickup.appeared_ticks >= tuning.pickup_visible_ms) {
    life_pickup.is_fading = true;
    life_pickup.fade_started_ticks = now;
  }

  if (life_pickup.is_fading &&
      now - life_pickup.fade_started_ticks >= LIFE_PICKUP_FADE_MS) {
    life_pickup.is_visible = false;
    life_pickup.is_fading = false;
    life_pickup.fade_started_ticks = 0;
    ScheduleNextLifePickupSpawn(now);
  }
}

void Game::TrySpawnDiscoPickup(Uint32 now) {
  if (disco_pickup.is_visible || active_disco_easteregg.is_active ||
      now < disco_pickup.next_spawn_ticks) {
    return;
  }

  std::vector<MapCoord> candidates;
  candidates.reserve(map->get_map_rows() * map->get_map_cols());
  for (size_t row = 0; row < map->get_map_rows(); row++) {
    for (size_t col = 0; col < map->get_map_cols(); col++) {
      MapCoord coord{static_cast<int>(row), static_cast<int>(col)};
      if (IsCellFreeForDiscoPickup(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    disco_pickup.next_spawn_ticks = now + DISCO_PICKUP_RETRY_DELAY_MS;
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  disco_pickup.coord = candidates[distribution(RandomGenerator())];
  disco_pickup.appeared_ticks = now;
  disco_pickup.fade_started_ticks = 0;
  disco_pickup.animation_seed =
      static_cast<int>((now % DISCO_PICKUP_ANIMATION_SEED_MODULUS) +
                       disco_pickup.coord.u *
                           DISCO_PICKUP_ANIMATION_SEED_ROW_MULTIPLIER +
                       disco_pickup.coord.v *
                           DISCO_PICKUP_ANIMATION_SEED_COL_MULTIPLIER);
  disco_pickup.is_visible = true;
  disco_pickup.is_fading = false;
#ifdef AUDIO
  audio->PlayPotionSpawn();
#endif
}

void Game::UpdateDiscoPickup(Uint32 now) {
  TrySpawnDiscoPickup(now);
  if (!disco_pickup.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, disco_pickup.coord)) {
#ifdef AUDIO
    audio->PlayCoin();
#endif
    disco_pickup.is_visible = false;
    disco_pickup.is_fading = false;
    disco_pickup.fade_started_ticks = 0;
    ScheduleNextDiscoPickupSpawn(now);
    StartDiscoEasterEgg(now);
    return;
  }

  if (!disco_pickup.is_fading &&
      now - disco_pickup.appeared_ticks >= DISCO_PICKUP_VISIBLE_MS) {
    disco_pickup.is_fading = true;
    disco_pickup.fade_started_ticks = now;
  }

  if (disco_pickup.is_fading &&
      now - disco_pickup.fade_started_ticks >= DISCO_PICKUP_FADE_MS) {
    disco_pickup.is_visible = false;
    disco_pickup.is_fading = false;
    disco_pickup.fade_started_ticks = 0;
    ScheduleNextDiscoPickupSpawn(now);
  }
}

void Game::StartDiscoEasterEgg(Uint32 now) {
  if (active_disco_easteregg.is_active || events == nullptr) {
    return;
  }

  active_disco_easteregg = {};
  active_disco_easteregg.is_active = true;
  active_disco_easteregg.started_ticks = now;
  active_disco_easteregg.frozen_started_ticks = now;
  active_disco_easteregg.last_rotation_update_ticks = now;
  active_disco_easteregg.rotation_phase_turns = 0.0;
  active_disco_easteregg.rotation_speed_scale = DISCO_ROTATION_SPEED_MIN;
  active_disco_easteregg.animation_seed =
      static_cast<int>((now % DISCO_EASTER_EGG_ANIMATION_SEED_MODULUS) +
                       pacman->map_coord.u *
                           DISCO_EASTER_EGG_ANIMATION_SEED_ROW_MULTIPLIER +
                       pacman->map_coord.v *
                           DISCO_EASTER_EGG_ANIMATION_SEED_COL_MULTIPLIER);
  events->SetGameplayFrozen(true);
  events->Keyreset();
#ifdef AUDIO
  if (audio != nullptr) {
    audio->StartDiscoMusic();
  }
#endif
}

void Game::UpdateDiscoRotationPhase(Uint32 now) {
  if (!active_disco_easteregg.is_active ||
      active_disco_easteregg.last_rotation_update_ticks == 0 ||
      active_disco_easteregg.is_ending) {
    return;
  }

  const Uint32 elapsed =
      now - active_disco_easteregg.last_rotation_update_ticks;
  active_disco_easteregg.last_rotation_update_ticks = now;
  active_disco_easteregg.rotation_phase_turns =
      std::fmod(active_disco_easteregg.rotation_phase_turns +
                    (static_cast<double>(elapsed) /
                     std::max(1.0, DISCO_ROTATION_PERIOD_MS)) *
                        active_disco_easteregg.rotation_speed_scale,
                1.0);
}

void Game::UpdateDiscoEasterEgg(Uint32 now) {
  if (!active_disco_easteregg.is_active) {
    return;
  }

  UpdateDiscoRotationPhase(now);

  if (!active_disco_easteregg.is_ending) {
    return;
  }

  const Uint32 elapsed = now - active_disco_easteregg.ending_started_ticks;
  const Uint32 finish_ms =
      std::max({DISCO_BRIGHTEN_MS, DISCO_BALL_RAISE_MS,
                DISCO_MUSIC_FADE_OUT_MS});
  if (elapsed < finish_ms) {
    return;
  }

#ifdef AUDIO
  if (audio != nullptr) {
    audio->StopDiscoMusic();
  }
#endif
  const Uint32 paused_duration_ms =
      now - active_disco_easteregg.frozen_started_ticks;
  active_disco_easteregg = {};
  ShiftPausedTimers(paused_duration_ms);
  events->SetGameplayFrozen(false);
  events->Keyreset();
}

void Game::TryUseBiohazardBeam(Uint32 now) {
  if (events == nullptr || pacman == nullptr) {
    return;
  }

  const bool use_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::Biohazard);
  if (!use_requested || biohazard_inventory <= 0 ||
      active_biohazard_beam.is_active) {
    return;
  }

  Directions facing_direction = events->get_next_move();
  if (facing_direction != Directions::None) {
    pacman->facing_direction = facing_direction;
  } else {
    facing_direction = pacman->facing_direction;
  }
  if (facing_direction == Directions::None) {
    facing_direction = Directions::Down;
  }

  active_biohazard_beam = {};
  active_biohazard_beam.is_active = true;
  active_biohazard_beam.direction = facing_direction;
  active_biohazard_beam.started_ticks = now;
  active_biohazard_beam.minimum_visible_until_ticks =
      now + BIOHAZARD_BEAM_MIN_VISIBLE_MS;
  active_biohazard_beam.animation_seed =
      static_cast<int>((now % 997) + pacman->map_coord.u * 89 +
                       pacman->map_coord.v * 53 + biohazard_inventory * 29);
  biohazard_inventory--;
#ifdef AUDIO
  audio->StartBiohazardBeam();
#endif
}

void Game::StopBiohazardBeam() {
  active_biohazard_beam = {};
#ifdef AUDIO
  if (audio != nullptr) {
    audio->StopBiohazardBeam();
  }
#endif
}

void Game::UpdateBiohazardBeam(Uint32 now) {
  if (!active_biohazard_beam.is_active || pacman == nullptr) {
    return;
  }

  if (active_biohazard_beam.is_locked_after_hit) {
    if (now >= active_biohazard_beam.locked_until_ticks) {
      StopBiohazardBeam();
    }
    return;
  }

  if (now - active_biohazard_beam.started_ticks >= BIOHAZARD_BEAM_ACTIVE_MS) {
    StopBiohazardBeam();
    return;
  }

  // Cast the beam from pacman's precise center along the active direction.
  // It terminates either at the closest wall edge along the path or at the
  // closest monster the beam segment touches.
  const SDL_FPoint beam_origin = PreciseWorldCenter(pacman);
  SDL_FPoint beam_endpoint = beam_origin;
  const int rows = static_cast<int>(map->get_map_rows());
  const int cols = static_cast<int>(map->get_map_cols());
  const int origin_u = pacman->map_coord.u;
  const int origin_v = pacman->map_coord.v;
  switch (active_biohazard_beam.direction) {
  case Directions::Up: {
    float stop_y = 0.0f;
    for (int u = origin_u - 1; u >= 0; --u) {
      if (IsBlockingMapElement(map->map_entry(u, origin_v))) {
        stop_y = static_cast<float>(u) + 1.0f;
        break;
      }
    }
    beam_endpoint = SDL_FPoint{beam_origin.x, stop_y};
    break;
  }
  case Directions::Down: {
    float stop_y = static_cast<float>(rows);
    for (int u = origin_u + 1; u < rows; ++u) {
      if (IsBlockingMapElement(map->map_entry(u, origin_v))) {
        stop_y = static_cast<float>(u);
        break;
      }
    }
    beam_endpoint = SDL_FPoint{beam_origin.x, stop_y};
    break;
  }
  case Directions::Left: {
    float stop_x = 0.0f;
    for (int v = origin_v - 1; v >= 0; --v) {
      if (IsBlockingMapElement(map->map_entry(origin_u, v))) {
        stop_x = static_cast<float>(v) + 1.0f;
        break;
      }
    }
    beam_endpoint = SDL_FPoint{stop_x, beam_origin.y};
    break;
  }
  case Directions::Right: {
    float stop_x = static_cast<float>(cols);
    for (int v = origin_v + 1; v < cols; ++v) {
      if (IsBlockingMapElement(map->map_entry(origin_u, v))) {
        stop_x = static_cast<float>(v);
        break;
      }
    }
    beam_endpoint = SDL_FPoint{stop_x, beam_origin.y};
    break;
  }
  case Directions::None:
  default:
    break;
  }

  Monster *closest_monster_target = nullptr;
  AlienAgent *closest_alien_target = nullptr;
  float closest_distance_squared = std::numeric_limits<float>::max();
  for (Monster *monster : monsters) {
    if (monster == nullptr || !monster->is_alive || monster->is_electrified ||
        monster->monster_char == GOAT) {
      continue;
    }

    const SDL_FPoint monster_center = PreciseWorldCenter(monster);
    if (!SegmentHitsCircle(beam_origin, beam_endpoint, monster_center,
                           MONSTER_HITBOX_RADIUS_CELLS +
                               BEAM_HITBOX_RADIUS_CELLS)) {
      continue;
    }

    const float dx = monster_center.x - beam_origin.x;
    const float dy = monster_center.y - beam_origin.y;
    const float distance_squared = dx * dx + dy * dy;
    if (distance_squared >= closest_distance_squared) {
      continue;
    }

    closest_distance_squared = distance_squared;
    closest_monster_target = monster;
    closest_alien_target = nullptr;
  }

  for (AlienAgent &alien : aliens) {
    if (!alien.is_alive) {
      continue;
    }

    const SDL_FPoint alien_center = AlienWorldCenter(alien);
    if (!SegmentHitsCircle(beam_origin, beam_endpoint, alien_center,
                           ALIEN_HITBOX_RADIUS_CELLS +
                               BEAM_HITBOX_RADIUS_CELLS)) {
      continue;
    }

    const float dx = alien_center.x - beam_origin.x;
    const float dy = alien_center.y - beam_origin.y;
    const float distance_squared = dx * dx + dy * dy;
    if (distance_squared >= closest_distance_squared) {
      continue;
    }

    closest_distance_squared = distance_squared;
    closest_monster_target = nullptr;
    closest_alien_target = &alien;
  }

  if (closest_monster_target != nullptr || closest_alien_target != nullptr) {
    SDL_FPoint impact_point =
        closest_monster_target != nullptr
            ? PreciseWorldCenter(closest_monster_target)
            : AlienWorldCenter(*closest_alien_target);
    const float impact_offset = 0.24f;
    switch (active_biohazard_beam.direction) {
    case Directions::Up:
      impact_point.y += impact_offset;
      break;
    case Directions::Down:
      impact_point.y -= impact_offset;
      break;
    case Directions::Left:
      impact_point.x += impact_offset;
      break;
    case Directions::Right:
      impact_point.x -= impact_offset;
      break;
    case Directions::None:
    default:
      break;
    }

    const MapCoord impact_coord = closest_monster_target != nullptr
                                      ? closest_monster_target->map_coord
                                      : closest_alien_target->coord;
    if (closest_monster_target != nullptr) {
      ElectrifyMonster(closest_monster_target, now);
      closest_monster_target->biohazard_paralyzed_until_ticks = std::max(
          closest_monster_target->biohazard_paralyzed_until_ticks,
          now + kBiohazardHitSequenceDurationMs);
      closest_monster_target->px_delta.x = 0;
      closest_monster_target->px_delta.y = 0;
    } else {
      EliminateAlien(closest_alien_target, now);
    }

    GameEffect impact_flash{impact_coord, now,
                            EffectType::BiohazardImpactFlash, 1};
    impact_flash.has_precise_world_center = true;
    impact_flash.precise_world_center = impact_point;
    effects.push_back(impact_flash);

    active_biohazard_beam.hit_sequence_started_ticks = now;
    active_biohazard_beam.locked_until_ticks =
        now + kBiohazardHitSequenceDurationMs;
    active_biohazard_beam.is_locked_after_hit = true;
    active_biohazard_beam.locked_origin_coord = pacman->map_coord;
    active_biohazard_beam.locked_origin_delta = pacman->px_delta;
    active_biohazard_beam.locked_end_world_center = impact_point;
  }
}

void Game::TrySpawnAlien(Uint32 now) {
  if (events == nullptr || map == nullptr || pacman == nullptr) {
    return;
  }

  if (!events->ConsumeAlienSpawnRequest()) {
    return;
  }

  std::vector<MapCoord> candidates;
  const int rows = static_cast<int>(map->get_map_rows());
  const int cols = static_cast<int>(map->get_map_cols());
  candidates.reserve(static_cast<size_t>(rows * cols));
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const MapCoord coord{row, col};
      if (IsCellFreeForAlienSpawn(coord)) {
        candidates.push_back(coord);
      }
    }
  }

  if (candidates.empty()) {
    return;
  }

  std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
  AlienAgent alien;
  alien.coord = candidates[distribution(RandomGenerator())];
  alien.spawned_ticks = now;
  alien.animation_seed =
      static_cast<int>((now % 997) + alien.coord.u * 61 + alien.coord.v * 43 +
                       static_cast<int>(aliens.size()) * 29);
  alien.next_animation_ticks =
      now + RandomInterval(ALIEN_IDLE_ANIMATION_MIN_INTERVAL_MS,
                           ALIEN_IDLE_ANIMATION_MAX_INTERVAL_MS);
  alien.next_thought_ticks =
      now + RandomInterval(ALIEN_THOUGHT_MIN_INTERVAL_MS / 2,
                           ALIEN_THOUGHT_MAX_INTERVAL_MS / 2);
  aliens.push_back(alien);
}

AlienAgent *Game::FindAlienInLineOfSight(Directions direction) {
  if (map == nullptr || pacman == nullptr || direction == Directions::None) {
    return nullptr;
  }

  AlienAgent *closest_target = nullptr;
  int closest_distance = std::numeric_limits<int>::max();
  for (AlienAgent &alien : aliens) {
    if (!alien.is_alive) {
      continue;
    }

    Directions sight_direction = Directions::None;
    if (!HasClearAxisLineOfSight(map, pacman->map_coord, alien.coord,
                                 sight_direction) ||
        sight_direction != direction) {
      continue;
    }

    const int distance = std::abs(alien.coord.u - pacman->map_coord.u) +
                         std::abs(alien.coord.v - pacman->map_coord.v);
    if (distance >= closest_distance) {
      continue;
    }

    closest_distance = distance;
    closest_target = &alien;
  }

  return closest_target;
}

void Game::TryFireAlienLaser(Uint32 now) {
  if (events == nullptr || pacman == nullptr || active_alien_laser.is_active) {
    return;
  }

  if (!events->ConsumeAlienLaserRequest()) {
    return;
  }

  Directions facing_direction = events->get_next_move();
  if (facing_direction != Directions::None) {
    pacman->facing_direction = facing_direction;
  } else {
    facing_direction = pacman->facing_direction;
  }
  if (facing_direction == Directions::None) {
    facing_direction = Directions::Down;
  }

  AlienAgent *target = FindAlienInLineOfSight(facing_direction);
  if (target == nullptr) {
    return;
  }

  SDL_FPoint impact_point = MakeCellCenter(target->coord);
  const float impact_offset = ALIEN_LASER_IMPACT_OFFSET_CELLS;
  switch (facing_direction) {
  case Directions::Up:
    impact_point.y += impact_offset;
    break;
  case Directions::Down:
    impact_point.y -= impact_offset;
    break;
  case Directions::Left:
    impact_point.x += impact_offset;
    break;
  case Directions::Right:
    impact_point.x -= impact_offset;
    break;
  case Directions::None:
  default:
    break;
  }

  active_alien_laser = {};
  active_alien_laser.is_active = true;
  active_alien_laser.direction = facing_direction;
  active_alien_laser.started_ticks = now;
  active_alien_laser.visible_until_ticks = now + ALIEN_LASER_VISIBLE_MS;
  active_alien_laser.animation_seed =
      static_cast<int>((now % 997) + pacman->map_coord.u * 97 +
                       pacman->map_coord.v * 67 + target->animation_seed);
  active_alien_laser.origin_coord = pacman->map_coord;
  active_alien_laser.origin_delta = pacman->px_delta;
  active_alien_laser.end_world_center = impact_point;

  EliminateAlien(target, now);

#ifdef AUDIO
  if (audio != nullptr) {
    audio->PlayAlienLaser();
  }
#endif
}

void Game::UpdateAlienLaser(Uint32 now) {
  if (active_alien_laser.is_active &&
      now >= active_alien_laser.visible_until_ticks) {
    active_alien_laser = {};
  }
}

void Game::UpdateAlienExplosions(Uint32 now) {
  for (AlienAgent &alien : aliens) {
    if (alien.is_alive) {
      if (alien.next_animation_ticks != 0 &&
          now >= alien.next_animation_ticks) {
        alien.animation_started_ticks = now;
        alien.animation_until_ticks = now + ALIEN_IDLE_ANIMATION_BURST_MS;
        alien.next_animation_ticks =
            now + ALIEN_IDLE_ANIMATION_BURST_MS +
            RandomInterval(ALIEN_IDLE_ANIMATION_MIN_INTERVAL_MS,
                           ALIEN_IDLE_ANIMATION_MAX_INTERVAL_MS);
      }

      if (alien.thought_visible_until_ticks != 0 &&
          now >= alien.thought_visible_until_ticks + ALIEN_THOUGHT_FADE_MS) {
        alien.thought_started_ticks = 0;
        alien.thought_visible_until_ticks = 0;
        alien.thought_index = -1;
      }

      if (alien.next_thought_ticks != 0 && now >= alien.next_thought_ticks &&
          alien.thought_visible_until_ticks == 0) {
        std::uniform_int_distribution<int> thought_dist(0, 14);
        alien.thought_index = thought_dist(RandomGenerator());
        alien.thought_started_ticks = now;
        alien.thought_visible_until_ticks = now + ALIEN_THOUGHT_VISIBLE_MS;
        alien.next_thought_ticks =
            now + ALIEN_THOUGHT_VISIBLE_MS + ALIEN_THOUGHT_FADE_MS +
            RandomInterval(ALIEN_THOUGHT_MIN_INTERVAL_MS,
                           ALIEN_THOUGHT_MAX_INTERVAL_MS);
      }
    }

    if (!alien.is_alive && !alien.scream_played &&
        alien.scream_trigger_ticks != 0 && now >= alien.scream_trigger_ticks) {
      alien.scream_played = true;
#ifdef AUDIO
      if (audio != nullptr) {
        audio->PlayAlienScream();
      }
#endif
    }

    if (alien.is_alive || alien.explosion_spawned ||
        alien.explosion_trigger_ticks == 0 ||
        now < alien.explosion_trigger_ticks) {
      continue;
    }

    const SDL_FPoint explosion_center = MakeCellCenter(alien.coord);
    GameEffect explosion_effect{alien.coord, now, EffectType::AlienExplosion,
                                1};
    explosion_effect.has_precise_world_center = true;
    explosion_effect.precise_world_center = explosion_center;
    effects.push_back(explosion_effect);
    SpawnExplosionSmokeCloud(explosion_center, 0.85f, now, 0.35f);
    alien.explosion_spawned = true;
#ifdef AUDIO
    if (audio != nullptr) {
      audio->PlayAlienExplosion();
    }
#endif
  }

}

void Game::TryPlaceDynamite(Uint32 now) {
  if (events == nullptr || pacman == nullptr) {
    return;
  }

  const bool place_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::Dynamite);
  if (!place_requested || dynamite_inventory <= 0) {
    return;
  }

  const MapCoord placement_coord = pacman->map_coord;
  if (IsCraterCell(placement_coord)) {
    return;
  }

  for (const auto &placed_dynamite : placed_dynamites) {
    if (SameCoord(placed_dynamite.coord, placement_coord)) {
      return;
    }
  }

  placed_dynamites.push_back(
      {placement_coord, now, now + DYNAMITE_COUNTDOWN_MS,
       static_cast<int>((now % 997) + placement_coord.u * 23 +
                        placement_coord.v * 29 + dynamite_inventory * 17)});
  dynamite_inventory--;
#ifdef AUDIO
  audio->PlayDynamiteIgnite();
#endif
}

void Game::TryUsePlasticExplosive(Uint32 now) {
  if (events == nullptr || pacman == nullptr) {
    return;
  }

  const bool use_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::PlasticExplosive);
  if (!use_requested) {
    return;
  }

  if (plastic_explosive_is_armed) {
    const PlacedPlasticExplosive detonated_charge = placed_plastic_explosive;
    plastic_explosive_is_armed = false;
    DetonatePlasticExplosive(detonated_charge, now);
    return;
  }

  if (plastic_explosive_inventory <= 0) {
    return;
  }

  Directions facing_direction = pacman->facing_direction;
  if (facing_direction == Directions::None) {
    facing_direction = Directions::Down;
  }

  const MapCoord target_coord = StepCoord(pacman->map_coord, facing_direction);
  if (!CanPlacePlasticExplosiveAt(target_coord)) {
    return;
  }

  const ElementType target_entry =
      map->map_entry(static_cast<size_t>(target_coord.u),
                     static_cast<size_t>(target_coord.v));
  placed_plastic_explosive = {
      target_coord,
      now,
      static_cast<int>((now % 997) + target_coord.u * 47 + target_coord.v * 31 +
                       plastic_explosive_inventory * 19),
      target_entry == ElementType::TYPE_WALL};
  plastic_explosive_is_armed = true;
  plastic_explosive_inventory--;
#ifdef AUDIO
  audio->PlayPlasticExplosivePlace();
#endif
}

void Game::TryUseLovePotion(Uint32 now) {
  if (events == nullptr || pacman == nullptr || map == nullptr) {
    return;
  }

  const bool use_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::LovePotion);
  if (!use_requested || love_potion_inventory <= 0) {
    return;
  }

  Directions facing_direction = events->get_next_move();
  if (facing_direction != Directions::None) {
    pacman->facing_direction = facing_direction;
  } else {
    facing_direction = pacman->facing_direction;
  }
  if (facing_direction == Directions::None) {
    facing_direction = Directions::Down;
  }

  love_potion_inventory--;

  LoveSmokeProjectile projectile;
  projectile.origin_coord = pacman->map_coord;
  projectile.direction = facing_direction;
  projectile.spawned_ticks = now;
  projectile.step_duration_ms = LOVE_SMOKE_STEP_DURATION_MS;
  projectile.segments_emitted = 0;
  projectile.max_segments = LOVE_SMOKE_RANGE_CELLS;
  projectile.finished = false;
  projectile.animation_seed =
      static_cast<int>((now % 997) + pacman->map_coord.u * 73 +
                       pacman->map_coord.v * 41 + love_potion_inventory * 19);
  active_love_smokes.push_back(projectile);

#ifdef AUDIO
  if (audio != nullptr) {
    audio->PlayLovePotion();
  }
#endif
}

void Game::ApplyLovePotionToGoat(Monster *goat, Uint32 now) {
  if (goat == nullptr) {
    return;
  }
  goat->goat_love_sequence_active = true;
  goat->goat_love_started_ticks = now;
  goat->goat_love_attack_at_ticks = now + GOAT_LOVE_HEART_VISIBLE_MS;
  goat->goat_love_target_id = -1;
  goat->goat_pacified = false;
  goat->goat_state = Monster::GoatState::Grazing;
  goat->goat_slide_direction = Directions::None;
  goat->goat_slide_remaining_steps = 0;
  goat->goat_ignore_player_until_ticks =
      std::max(goat->goat_ignore_player_until_ticks,
               now + GOAT_PLAYER_GRACE_MS);
  goat->goat_is_jumping = false;
  goat->px_delta.x = 0;
  goat->px_delta.y = 0;
}

void Game::SpawnLoveSmokeTrailPuff(SDL_FPoint world_center,
                                   Directions direction, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> jitter_dist(-0.18f, 0.18f);
  std::uniform_real_distribution<float> drift_dist(-0.25f, 0.25f);
  std::uniform_real_distribution<float> rise_dist(-0.15f, 0.05f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  float forward_x = 0.0f;
  float forward_y = 0.0f;
  switch (direction) {
  case Directions::Up:
    forward_y = -0.6f;
    break;
  case Directions::Down:
    forward_y = 0.6f;
    break;
  case Directions::Left:
    forward_x = -0.6f;
    break;
  case Directions::Right:
    forward_x = 0.6f;
    break;
  default:
    break;
  }

  for (int i = 0; i < LOVE_SMOKE_TRAIL_PUFF_COUNT; ++i) {
    ExplosionSmokePuff puff;
    puff.world_position = {world_center.x + jitter_dist(rng),
                           world_center.y + jitter_dist(rng)};
    puff.velocity_cells_per_sec = {forward_x + drift_dist(rng),
                                   forward_y + rise_dist(rng)};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::LoveSmokeTrail;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnLoveSmokeImpactPuff(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> radius_dist(0.0f, 0.55f);
  std::uniform_real_distribution<float> drift_dist(-0.5f, 0.5f);
  std::uniform_real_distribution<float> rise_dist(-0.4f, 0.05f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  for (int i = 0; i < LOVE_SMOKE_IMPACT_PUFF_COUNT; ++i) {
    const float angle = angle_dist(rng);
    const float radius = radius_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {world_center.x + std::cos(angle) * radius,
                           world_center.y + std::sin(angle) * radius};
    puff.velocity_cells_per_sec = {drift_dist(rng), rise_dist(rng)};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::LoveSmokeImpact;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::UpdateLoveSmokeProjectiles(Uint32 now) {
  for (size_t index = 0; index < active_love_smokes.size();) {
    LoveSmokeProjectile &smoke = active_love_smokes[index];
    const Uint32 step_ms = std::max<Uint32>(1, smoke.step_duration_ms);

    while (!smoke.finished && smoke.segments_emitted < smoke.max_segments) {
      const Uint32 next_segment_index =
          static_cast<Uint32>(smoke.segments_emitted) + 1;
      const Uint32 due_at_ticks = smoke.spawned_ticks + next_segment_index * step_ms;
      if (now < due_at_ticks) {
        break;
      }

      MapCoord segment_coord = smoke.origin_coord;
      for (Uint32 step = 0; step < next_segment_index; ++step) {
        segment_coord = StepCoord(segment_coord, smoke.direction);
      }

      if (!IsInsideMapBounds(map, segment_coord) ||
          IsBlockingMapElement(
              map->map_entry(static_cast<size_t>(segment_coord.u),
                             static_cast<size_t>(segment_coord.v)))) {
        smoke.finished = true;
        break;
      }

      const SDL_FPoint cell_center = MakeCellCenter(segment_coord);
      SpawnLoveSmokeTrailPuff(cell_center, smoke.direction, due_at_ticks);

      Monster *hit_goat = nullptr;
      for (Monster *monster : monsters) {
        if (monster == nullptr || !monster->is_alive ||
            monster->monster_char != GOAT) {
          continue;
        }
        if (monster->goat_love_sequence_active ||
            monster->goat_love_target_id != -1) {
          continue;
        }
        if (SameCoord(monster->map_coord, segment_coord)) {
          hit_goat = monster;
          break;
        }
      }

      if (hit_goat != nullptr) {
        ApplyLovePotionToGoat(hit_goat, due_at_ticks);
        SpawnLoveSmokeImpactPuff(PreciseWorldCenter(hit_goat), due_at_ticks);
        smoke.finished = true;
        break;
      }

      smoke.segments_emitted = static_cast<int>(next_segment_index);
      if (smoke.segments_emitted >= smoke.max_segments) {
        smoke.finished = true;
      }
    }

    if (smoke.finished) {
      active_love_smokes.erase(active_love_smokes.begin() +
                               static_cast<long>(index));
    } else {
      ++index;
    }
  }
}

void Game::ApplyCheats() {
  if (events == nullptr) {
    return;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::Dynamite)) {
    dynamite_inventory++;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::PlasticExplosive)) {
    plastic_explosive_inventory++;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::Airstrike)) {
    airstrike_inventory++;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::Rocket)) {
    rocket_inventory++;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::Biohazard)) {
    biohazard_inventory++;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::NuclearBomb)) {
    nuclear_bomb_inventory++;
  }
  if (events->ConsumeCheatRequest(ExtraSlot::LovePotion)) {
    love_potion_inventory++;
  }
  if (events->ConsumeDiscoTestRequest()) {
    StartDiscoEasterEgg(SDL_GetTicks());
  }
}

void Game::TryFireRocket(Uint32 now) {
  if (events == nullptr || pacman == nullptr) {
    return;
  }

  const bool fire_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::Rocket);
  // Mehrere Raketen dürfen gleichzeitig in der Luft sein. Auto-Fire durch
  // Gedrückthalten der Taste verhindert events->update() (key.repeat-Filter).
  if (!fire_requested || rocket_inventory <= 0) {
    return;
  }

  Directions facing_direction = pacman->facing_direction;
  if (facing_direction == Directions::None) {
    facing_direction = Directions::Down;
  }

  active_rockets.push_back(
      {pacman->map_coord,
       facing_direction,
       now,
       ROCKET_STEP_DURATION_MS,
       (now > ROCKET_TRAIL_SMOKE_SPAWN_INTERVAL_MS)
           ? (now - ROCKET_TRAIL_SMOKE_SPAWN_INTERVAL_MS)
           : 0,
       static_cast<int>((now % 997) + pacman->map_coord.u * 79 +
                        pacman->map_coord.v * 61 + rocket_inventory * 23)});
  rocket_inventory--;
#ifdef AUDIO
  audio->PlayRocketLaunch();
#endif
}

void Game::TryUseNuclearBomb(Uint32 now) {
  if (events == nullptr || pacman == nullptr || map == nullptr) {
    return;
  }

  const bool use_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::NuclearBomb);
  if (!use_requested || active_nuclear_bomb_drop.is_active ||
      active_nuclear_explosion.is_active) {
    return;
  }

  if (!nuclear_bomb_target_marker.is_marked) {
    if (nuclear_bomb_inventory <= 0 || IsCraterCell(pacman->map_coord)) {
      return;
    }

    nuclear_bomb_target_marker = {};
    nuclear_bomb_target_marker.is_marked = true;
    nuclear_bomb_target_marker.coord = pacman->map_coord;
    nuclear_bomb_target_marker.marked_ticks = now;
    nuclear_bomb_target_marker.animation_seed =
        static_cast<int>((now % 997) + pacman->map_coord.u * 101 +
                         pacman->map_coord.v * 73);
    return;
  }

  if (nuclear_bomb_inventory <= 0) {
    return;
  }

  active_nuclear_bomb_drop = {};
  active_nuclear_bomb_drop.is_active = true;
  active_nuclear_bomb_drop.target_coord = nuclear_bomb_target_marker.coord;
  active_nuclear_bomb_drop.target_world_center =
      MakeCellCenter(nuclear_bomb_target_marker.coord);
  active_nuclear_bomb_drop.alarm_started_ticks = now;
  active_nuclear_bomb_drop.drop_started_ticks = now + NUCLEAR_BOMB_ALARM_MS;
  active_nuclear_bomb_drop.impact_ticks =
      active_nuclear_bomb_drop.drop_started_ticks + NUCLEAR_BOMB_FALL_MS;
  active_nuclear_bomb_drop.animation_seed =
      static_cast<int>((now % 997) + active_nuclear_bomb_drop.target_coord.u * 107 +
                       active_nuclear_bomb_drop.target_coord.v * 79);
  active_nuclear_bomb_drop.drop_sound_started = false;
  nuclear_bomb_inventory = 0;
#ifdef AUDIO
  audio->PlayNuclearBombAlarm();
#endif
}

void Game::UpdateNuclearBombDrop(Uint32 now) {
  if (!active_nuclear_bomb_drop.is_active) {
    return;
  }

  if (!active_nuclear_bomb_drop.drop_sound_started &&
      now + 2000 >= active_nuclear_bomb_drop.drop_started_ticks) {
    active_nuclear_bomb_drop.drop_sound_started = true;
#ifdef AUDIO
    audio->PlayNuclearBombDrop();
#endif
  }

  if (now < active_nuclear_bomb_drop.impact_ticks) {
    return;
  }

#ifdef AUDIO
  audio->PlayNuclearBombExplosion();
#endif
  TriggerNuclearExplosionAt(active_nuclear_bomb_drop.target_world_center,
                            active_nuclear_bomb_drop.target_coord, now);
  active_nuclear_bomb_drop = {};
  nuclear_bomb_target_marker = {};
}

void Game::TriggerNuclearExplosionAt(SDL_FPoint world_center,
                                     MapCoord center_coord, Uint32 now) {
  if (map == nullptr || active_nuclear_explosion.is_active) {
    return;
  }

  active_nuclear_explosion = {};
  active_nuclear_explosion.is_active = true;
  active_nuclear_explosion.started_ticks = now;
  active_nuclear_explosion.last_ring_spawn_ticks =
      (now > NUCLEAR_SMOKE_RING_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_SMOKE_RING_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion.last_center_smoke_spawn_ticks =
      (now > NUCLEAR_CENTER_SMOKE_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_CENTER_SMOKE_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion.last_mushroom_smoke_spawn_ticks =
      (now > NUCLEAR_MUSHROOM_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_MUSHROOM_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion.animation_seed =
      static_cast<int>((now % 997) + center_coord.u * 43 + center_coord.v * 61);
  active_nuclear_explosion.center_coord = center_coord;
  active_nuclear_explosion.world_center = world_center;

  GameEffect effect{center_coord, now, EffectType::NuclearExplosion,
                    NUCLEAR_EXPLOSION_RADIUS_CELLS};
  effect.has_precise_world_center = true;
  effect.precise_world_center = world_center;
  effects.push_back(effect);

  if (pacman != nullptr &&
      PointDistance(MakeCellCenter(pacman->map_coord), world_center) <=
          static_cast<double>(NUCLEAR_EXPLOSION_RADIUS_CELLS) +
              static_cast<double>(PACMAN_HITBOX_RADIUS_CELLS)) {
    remaining_lives = 1;
    TriggerLoss(pacman->map_coord, now);
  }

  active_nuclear_explosion.crater_created = true;
  CreateNuclearCrater(active_nuclear_explosion, now,
                      now + NUCLEAR_CRATER_REVEAL_DELAY_MS);

  SpawnNuclearBombFragments(world_center, now);
}

void Game::TryTriggerNuclearExplosion(Uint32 now) {
  if (events == nullptr || map == nullptr) {
    return;
  }

  const bool trigger_requested = events->ConsumeNuclearTestRequest();
  if (!trigger_requested || active_nuclear_explosion.is_active) {
    return;
  }

  const int rows = static_cast<int>(map->get_map_rows());
  const int cols = static_cast<int>(map->get_map_cols());
  if (rows <= 0 || cols <= 0) {
    return;
  }

  const SDL_FPoint world_center{static_cast<float>(cols) * 0.5f,
                                static_cast<float>(rows) * 0.5f};
  const MapCoord center_coord{
      std::clamp(static_cast<int>(std::floor(world_center.y)), 0, rows - 1),
      std::clamp(static_cast<int>(std::floor(world_center.x)), 0, cols - 1)};
  TriggerNuclearExplosionAt(world_center, center_coord, now);
}

void Game::CreateNuclearCrater(const ActiveNuclearExplosion &explosion,
                               Uint32 now, Uint32 visible_ticks) {
  if (map == nullptr) {
    return;
  }

  NuclearCrater crater;
  crater.world_center = explosion.world_center;
  crater.radius_cells = NUCLEAR_CRATER_RADIUS_CELLS;
  crater.shape_seed = explosion.animation_seed;
  crater.visible_ticks = visible_ticks;

  const int min_row = std::max(
      0, static_cast<int>(std::floor(crater.world_center.y -
                                     crater.radius_cells - 1.0f)));
  const int max_row = std::min(
      static_cast<int>(map->get_map_rows()) - 1,
      static_cast<int>(std::ceil(crater.world_center.y +
                                 crater.radius_cells + 1.0f)));
  const int min_col = std::max(
      0, static_cast<int>(std::floor(crater.world_center.x -
                                     crater.radius_cells - 1.0f)));
  const int max_col = std::min(
      static_cast<int>(map->get_map_cols()) - 1,
      static_cast<int>(std::ceil(crater.world_center.x +
                                 crater.radius_cells + 1.0f)));

  for (int row = min_row; row <= max_row; ++row) {
    for (int col = min_col; col <= max_col; ++col) {
      const MapCoord coord{row, col};
      if (!NuclearCraterTouchesCell(crater, coord)) {
        continue;
      }

      crater.cells.push_back(coord);
      const ElementType entry =
          map->map_entry(static_cast<size_t>(row), static_cast<size_t>(col));
      const SDL_FPoint cell_center = MakeCellCenter(coord);

      if (entry == ElementType::TYPE_WALL ||
          entry == ElementType::TYPE_TELEPORTER) {
        SpawnWallDestructionParticles(cell_center, now);
        map->SetEntry(coord, ElementType::TYPE_PATH);
        SpawnWallRubble(coord);
      }

      if (entry == ElementType::TYPE_GOODIE) {
        for (auto *goodie : goodies) {
          if (goodie != nullptr && goodie->is_active &&
              SameCoord(goodie->map_coord, coord)) {
            goodie->Deactivate();
          }
        }
      }

      for (auto *monster : monsters) {
        if (monster == nullptr || !monster->is_alive ||
            !SameCoord(monster->map_coord, coord)) {
          continue;
        }

        monster->scheduled_dynamite_blast_ticks =
            now + kNuclearCraterExplosionDelayMs;
        monster->scheduled_dynamite_blast_coord = coord;
        ScheduleMonsterBlast(monster,
                             now + kNuclearCraterExplosionDelayMs);
      }

      for (AlienAgent &alien : aliens) {
        if (!alien.is_alive || !SameCoord(alien.coord, coord)) {
          continue;
        }

        EliminateAlien(&alien, now + kNuclearCraterExplosionDelayMs);
      }

      if (dynamite_pickup.is_visible && SameCoord(dynamite_pickup.coord, coord)) {
        dynamite_pickup.is_visible = false;
        dynamite_pickup.is_fading = false;
        ScheduleNextDynamiteSpawn(now);
      }
      if (invulnerability_potion.is_visible &&
          SameCoord(invulnerability_potion.coord, coord)) {
        invulnerability_potion.is_visible = false;
        invulnerability_potion.is_fading = false;
        ScheduleNextInvulnerabilityPotionSpawn(now);
      }
      if (plastic_explosive_pickup.is_visible &&
          SameCoord(plastic_explosive_pickup.coord, coord)) {
        plastic_explosive_pickup.is_visible = false;
        plastic_explosive_pickup.is_fading = false;
        ScheduleNextPlasticExplosiveSpawn(now);
      }
      if (walkie_talkie_pickup.is_visible &&
          SameCoord(walkie_talkie_pickup.coord, coord)) {
        walkie_talkie_pickup.is_visible = false;
        walkie_talkie_pickup.is_fading = false;
        ScheduleNextWalkieTalkieSpawn(now);
      }
      if (rocket_pickup.is_visible && SameCoord(rocket_pickup.coord, coord)) {
        rocket_pickup.is_visible = false;
        rocket_pickup.is_fading = false;
        ScheduleNextRocketSpawn(now);
      }
      if (biohazard_pickup.is_visible &&
          SameCoord(biohazard_pickup.coord, coord)) {
        biohazard_pickup.is_visible = false;
        biohazard_pickup.is_fading = false;
        ScheduleNextBiohazardSpawn(now);
      }
      if (nuclear_bomb_pickup.is_visible &&
          SameCoord(nuclear_bomb_pickup.coord, coord)) {
        nuclear_bomb_pickup.is_visible = false;
        nuclear_bomb_pickup.is_fading = false;
        ScheduleNextNuclearBombSpawn(now);
      }
      if (love_potion_pickup.is_visible &&
          SameCoord(love_potion_pickup.coord, coord)) {
        love_potion_pickup.is_visible = false;
        love_potion_pickup.is_fading = false;
        ScheduleNextLovePotionSpawn(now);
      }
      if (life_pickup.is_visible &&
          SameCoord(life_pickup.coord, coord)) {
        life_pickup.is_visible = false;
        life_pickup.is_fading = false;
        ScheduleNextLifePickupSpawn(now);
      }
      if (disco_pickup.is_visible &&
          SameCoord(disco_pickup.coord, coord)) {
        disco_pickup.is_visible = false;
        disco_pickup.is_fading = false;
        ScheduleNextDiscoPickupSpawn(now);
      }
      if (nuclear_bomb_target_marker.is_marked &&
          SameCoord(nuclear_bomb_target_marker.coord, coord)) {
        nuclear_bomb_target_marker = {};
      }

      placed_dynamites.erase(
          std::remove_if(placed_dynamites.begin(), placed_dynamites.end(),
                         [coord](const PlacedDynamite &dynamite) {
                           return SameCoord(dynamite.coord, coord);
                         }),
          placed_dynamites.end());
      if (plastic_explosive_is_armed &&
          SameCoord(placed_plastic_explosive.coord, coord)) {
        plastic_explosive_is_armed = false;
        placed_plastic_explosive = {};
      }

      fireballs.erase(
          std::remove_if(fireballs.begin(), fireballs.end(),
                         [coord](const Fireball &fireball) {
                           return SameCoord(fireball.current_coord, coord);
                         }),
          fireballs.end());
      slimeballs.erase(
          std::remove_if(slimeballs.begin(), slimeballs.end(),
                         [coord](const Slimeball &slimeball) {
                           return SameCoord(slimeball.current_coord, coord);
                         }),
          slimeballs.end());
      active_rockets.erase(
          std::remove_if(active_rockets.begin(), active_rockets.end(),
                         [coord](const RocketProjectile &rocket) {
                           return SameCoord(rocket.current_coord, coord);
                         }),
          active_rockets.end());

      map->SetEntry(coord, ElementType::TYPE_CRATER);
    }
  }

  if (!crater.cells.empty()) {
    nuclear_craters.push_back(crater);
  }

  RemoveUnreachableGoodiesAfterCrater();

  if (pacman != nullptr && IsCraterCell(pacman->map_coord)) {
    remaining_lives = 1;
    TriggerLoss(pacman->map_coord, now);
  }
}

void Game::UpdateNuclearExplosion(Uint32 now) {
  if (!active_nuclear_explosion.is_active || now < active_nuclear_explosion.started_ticks) {
    return;
  }

  const Uint32 age = now - active_nuclear_explosion.started_ticks;
  if (!active_nuclear_explosion.crater_created &&
      age >= NUCLEAR_EXPLOSION_FIREBALL_BURN_MS) {
    active_nuclear_explosion.crater_created = true;
    CreateNuclearCrater(active_nuclear_explosion, now, now);
  }

  if (age >= NUCLEAR_EXPLOSION_TOTAL_DURATION_MS) {
    active_nuclear_explosion = {};
    return;
  }

  while (now >= active_nuclear_explosion.last_ring_spawn_ticks +
                    NUCLEAR_SMOKE_RING_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion.last_ring_spawn_ticks <
             active_nuclear_explosion.started_ticks +
                 NUCLEAR_SMOKE_RING_EXPANSION_MS) {
    active_nuclear_explosion.last_ring_spawn_ticks +=
        NUCLEAR_SMOKE_RING_SPAWN_INTERVAL_MS;
    const float ring_progress = std::clamp(
        static_cast<float>(active_nuclear_explosion.last_ring_spawn_ticks -
                           active_nuclear_explosion.started_ticks) /
            static_cast<float>(std::max<Uint32>(1, NUCLEAR_SMOKE_RING_EXPANSION_MS)),
        0.0f, 1.0f);
    SpawnNuclearRingSmoke(ring_progress,
                          active_nuclear_explosion.last_ring_spawn_ticks);
  }

  while (now >= active_nuclear_explosion.last_center_smoke_spawn_ticks +
                    NUCLEAR_CENTER_SMOKE_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion.last_center_smoke_spawn_ticks <
             active_nuclear_explosion.started_ticks +
                 NUCLEAR_CENTER_SMOKE_EMISSION_MS) {
    active_nuclear_explosion.last_center_smoke_spawn_ticks +=
        NUCLEAR_CENTER_SMOKE_SPAWN_INTERVAL_MS;
    const float center_progress = std::clamp(
        static_cast<float>(active_nuclear_explosion.last_center_smoke_spawn_ticks -
                           active_nuclear_explosion.started_ticks) /
            static_cast<float>(
                std::max<Uint32>(1, NUCLEAR_CENTER_SMOKE_EMISSION_MS)),
        0.0f, 1.0f);
    SpawnNuclearCenterSmoke(
        center_progress, active_nuclear_explosion.last_center_smoke_spawn_ticks);
  }

  while (now >= active_nuclear_explosion.last_mushroom_smoke_spawn_ticks +
                    NUCLEAR_MUSHROOM_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion.last_mushroom_smoke_spawn_ticks <
             active_nuclear_explosion.started_ticks + NUCLEAR_MUSHROOM_BUILD_MS) {
    active_nuclear_explosion.last_mushroom_smoke_spawn_ticks +=
        NUCLEAR_MUSHROOM_SPAWN_INTERVAL_MS;
    const float mushroom_progress = std::clamp(
        static_cast<float>(
            active_nuclear_explosion.last_mushroom_smoke_spawn_ticks -
            active_nuclear_explosion.started_ticks) /
            static_cast<float>(std::max<Uint32>(1, NUCLEAR_MUSHROOM_BUILD_MS)),
        0.0f, 1.0f);
    SpawnNuclearMushroomSmoke(
        mushroom_progress,
        active_nuclear_explosion.last_mushroom_smoke_spawn_ticks);
  }
}

void Game::TryTriggerNuclearExplosionB(Uint32 now) {
  if (events == nullptr || map == nullptr) {
    return;
  }

  const bool trigger_requested = events->ConsumeNuclearTestBRequest();
  if (!trigger_requested || active_nuclear_explosion_b.is_active) {
    return;
  }

  const int rows = static_cast<int>(map->get_map_rows());
  const int cols = static_cast<int>(map->get_map_cols());
  if (rows <= 0 || cols <= 0) {
    return;
  }

  const SDL_FPoint world_center{static_cast<float>(cols) * 0.5f,
                                static_cast<float>(rows) * 0.5f};
  const MapCoord center_coord{
      std::clamp(static_cast<int>(std::floor(world_center.y)), 0, rows - 1),
      std::clamp(static_cast<int>(std::floor(world_center.x)), 0, cols - 1)};

  active_nuclear_explosion_b = {};
  active_nuclear_explosion_b.is_active = true;
  active_nuclear_explosion_b.started_ticks = now;
  active_nuclear_explosion_b.last_ring_spawn_ticks =
      (now > NUCLEAR_B_SMOKE_RING_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_B_SMOKE_RING_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion_b.last_trail_spawn_ticks =
      (now > NUCLEAR_B_TRAIL_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_B_TRAIL_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion_b.last_stem_spawn_ticks =
      (now > NUCLEAR_B_STEM_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_B_STEM_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion_b.last_cap_spawn_ticks =
      (now > NUCLEAR_B_CAP_SPAWN_INTERVAL_MS)
          ? (now - NUCLEAR_B_CAP_SPAWN_INTERVAL_MS)
          : 0;
  active_nuclear_explosion_b.animation_seed =
      static_cast<int>((now % 991) + rows * 47 + cols * 67);
  active_nuclear_explosion_b.center_coord = center_coord;
  active_nuclear_explosion_b.world_center = world_center;

  GameEffect effect{center_coord, now, EffectType::NuclearExplosionB,
                    NUCLEAR_EXPLOSION_RADIUS_CELLS};
  effect.has_precise_world_center = true;
  effect.precise_world_center = world_center;
  effects.push_back(effect);

  SpawnNuclearBombFragments(world_center, now);
}

void Game::UpdateNuclearExplosionB(Uint32 now) {
  if (!active_nuclear_explosion_b.is_active ||
      now < active_nuclear_explosion_b.started_ticks) {
    return;
  }

  const Uint32 age = now - active_nuclear_explosion_b.started_ticks;
  if (age >= NUCLEAR_B_EXPLOSION_TOTAL_DURATION_MS) {
    active_nuclear_explosion_b = {};
    return;
  }

  while (now >= active_nuclear_explosion_b.last_ring_spawn_ticks +
                    NUCLEAR_B_SMOKE_RING_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion_b.last_ring_spawn_ticks <
             active_nuclear_explosion_b.started_ticks +
                 NUCLEAR_B_SMOKE_RING_EXPANSION_MS) {
    active_nuclear_explosion_b.last_ring_spawn_ticks +=
        NUCLEAR_B_SMOKE_RING_SPAWN_INTERVAL_MS;
    const float ring_progress = std::clamp(
        static_cast<float>(active_nuclear_explosion_b.last_ring_spawn_ticks -
                           active_nuclear_explosion_b.started_ticks) /
            static_cast<float>(
                std::max<Uint32>(1, NUCLEAR_B_SMOKE_RING_EXPANSION_MS)),
        0.0f, 1.0f);
    SpawnNuclearBRingSmoke(ring_progress,
                           active_nuclear_explosion_b.last_ring_spawn_ticks);
  }

  while (now >= active_nuclear_explosion_b.last_trail_spawn_ticks +
                    NUCLEAR_B_TRAIL_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion_b.last_trail_spawn_ticks <
             active_nuclear_explosion_b.started_ticks +
                 NUCLEAR_B_SMOKE_RING_EXPANSION_MS) {
    active_nuclear_explosion_b.last_trail_spawn_ticks +=
        NUCLEAR_B_TRAIL_SPAWN_INTERVAL_MS;
    const float trail_progress = std::clamp(
        static_cast<float>(active_nuclear_explosion_b.last_trail_spawn_ticks -
                           active_nuclear_explosion_b.started_ticks) /
            static_cast<float>(
                std::max<Uint32>(1, NUCLEAR_B_SMOKE_RING_EXPANSION_MS)),
        0.0f, 1.0f);
    SpawnNuclearBTrailSmoke(trail_progress,
                            active_nuclear_explosion_b.last_trail_spawn_ticks);
  }

  while (now >= active_nuclear_explosion_b.last_stem_spawn_ticks +
                    NUCLEAR_B_STEM_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion_b.last_stem_spawn_ticks <
             active_nuclear_explosion_b.started_ticks +
                 NUCLEAR_B_STEM_BUILD_MS) {
    active_nuclear_explosion_b.last_stem_spawn_ticks +=
        NUCLEAR_B_STEM_SPAWN_INTERVAL_MS;
    const float stem_progress = std::clamp(
        static_cast<float>(active_nuclear_explosion_b.last_stem_spawn_ticks -
                           active_nuclear_explosion_b.started_ticks) /
            static_cast<float>(std::max<Uint32>(1, NUCLEAR_B_STEM_BUILD_MS)),
        0.0f, 1.0f);
    SpawnNuclearBStemSmoke(stem_progress,
                           active_nuclear_explosion_b.last_stem_spawn_ticks);
  }

  while (now >= active_nuclear_explosion_b.last_cap_spawn_ticks +
                    NUCLEAR_B_CAP_SPAWN_INTERVAL_MS &&
         active_nuclear_explosion_b.last_cap_spawn_ticks <
             active_nuclear_explosion_b.started_ticks +
                 NUCLEAR_B_STEM_BUILD_MS) {
    active_nuclear_explosion_b.last_cap_spawn_ticks +=
        NUCLEAR_B_CAP_SPAWN_INTERVAL_MS;
    const float cap_progress = std::clamp(
        static_cast<float>(active_nuclear_explosion_b.last_cap_spawn_ticks -
                           active_nuclear_explosion_b.started_ticks) /
            static_cast<float>(std::max<Uint32>(1, NUCLEAR_B_STEM_BUILD_MS)),
        0.0f, 1.0f);
    SpawnNuclearBCapSmoke(cap_progress,
                          active_nuclear_explosion_b.last_cap_spawn_ticks);
  }
}

void Game::TryUseAirstrike(Uint32 now) {
  if (events == nullptr || pacman == nullptr) {
    return;
  }

  const bool use_requested =
      events->ConsumeExtraUseRequest(ExtraSlot::Airstrike);
  if (!use_requested || airstrike_inventory <= 0 || active_airstrike.is_active) {
    return;
  }

  active_airstrike = {};
  active_airstrike.is_active = true;
  active_airstrike.target_coord = pacman->map_coord;
  active_airstrike.started_ticks = now;
  active_airstrike.animation_seed =
      static_cast<int>((now % 997) + pacman->map_coord.u * 53 +
                       pacman->map_coord.v * 71 + airstrike_inventory * 17);
  active_airstrike.flight_start = RandomAirstrikeStart(map);

  const SDL_FPoint target_point = MakeCellCenter(active_airstrike.target_coord);
  const SDL_FPoint approach_vector =
      SubtractPoint(target_point, active_airstrike.flight_start);
  const double approach_length = PointDistance(active_airstrike.flight_start,
                                               target_point);
  if (approach_length <= 0.001) {
    active_airstrike = {};
    return;
  }

  const SDL_FPoint direction = ScalePoint(
      approach_vector, static_cast<float>(1.0 / approach_length));
  const double max_span =
      std::hypot(static_cast<double>(map->get_map_rows()),
                 static_cast<double>(map->get_map_cols())) +
      kAirstrikePlaneMarginCells * 3.0;
  active_airstrike.flight_end =
      AddPoint(target_point, ScalePoint(direction, static_cast<float>(max_span)));
  active_airstrike.flight_direction =
      DominantDirection(SubtractPoint(active_airstrike.flight_end,
                                      active_airstrike.flight_start));

  const double total_path_length =
      PointDistance(active_airstrike.flight_start, active_airstrike.flight_end);
  if (total_path_length <= 0.001) {
    active_airstrike = {};
    return;
  }

  active_airstrike.target_progress =
      std::clamp(approach_length / total_path_length, 0.0, 1.0);

#ifdef AUDIO
  const Uint32 radio_delay_ms =
      (audio != nullptr) ? audio->GetAirstrikeRadioDurationMs()
                         : AIRSTRIKE_RADIO_DELAY_MS;
#else
  const Uint32 radio_delay_ms = AIRSTRIKE_RADIO_DELAY_MS;
#endif
  const Uint32 plane_flight_ms = std::max<Uint32>(
      AIRSTRIKE_PLANE_MIN_FLIGHT_MS,
      static_cast<Uint32>(
          std::lround(total_path_length * AIRSTRIKE_PLANE_CELL_TRAVEL_MS)));
  active_airstrike.plane_launch_ticks = now + radio_delay_ms;
  active_airstrike.overflight_ticks =
      active_airstrike.plane_launch_ticks +
      static_cast<Uint32>(std::lround(active_airstrike.target_progress *
                                      static_cast<double>(plane_flight_ms)));
  active_airstrike.plane_finished_ticks =
      active_airstrike.plane_launch_ticks + plane_flight_ms;

  std::vector<AirstrikePathCandidate> bomb_candidates =
      CollectAirstrikePathCandidates(map, active_airstrike.flight_start,
                                     active_airstrike.flight_end,
                                     active_airstrike.target_coord);
  if (bomb_candidates.size() > AIRSTRIKE_BOMB_COUNT) {
    std::shuffle(bomb_candidates.begin(), bomb_candidates.end(),
                 RandomGenerator());
    bomb_candidates.resize(AIRSTRIKE_BOMB_COUNT);
  }
  std::sort(bomb_candidates.begin(), bomb_candidates.end(),
            [](const AirstrikePathCandidate &left,
               const AirstrikePathCandidate &right) {
              return left.progress < right.progress;
            });
  active_airstrike.bombs.reserve(bomb_candidates.size());

  for (size_t index = 0; index < bomb_candidates.size(); ++index) {
    const auto &candidate = bomb_candidates[index];
    const Uint32 release_ticks =
        active_airstrike.plane_launch_ticks +
        static_cast<Uint32>(std::lround(candidate.progress *
                                        static_cast<double>(plane_flight_ms)));
    active_airstrike.bombs.push_back(
        {candidate.coord,
         candidate.point,
         candidate.progress,
         release_ticks,
         release_ticks + AIRSTRIKE_BOMB_FALL_MS,
         false,
         static_cast<int>(active_airstrike.animation_seed + index * 31)});
  }

  if (active_airstrike.bombs.empty()) {
    active_airstrike = {};
    return;
  }

  active_airstrike.finished_ticks = std::max(
      active_airstrike.plane_finished_ticks,
      active_airstrike.bombs.back().explode_ticks +
          static_cast<Uint32>(AIRSTRIKE_EXPLOSION_DURATION_MS));

  airstrike_inventory--;
#ifdef AUDIO
  audio->PlayAirstrikeRadio();
  audio->StartAirstrikePlane();
  active_airstrike.plane_sound_started = true;
#endif
}

void Game::UpdateAirstrike(Uint32 now) {
  if (!active_airstrike.is_active) {
    return;
  }

#ifdef AUDIO
  if (!active_airstrike.plane_sound_fadeout_started &&
      active_airstrike.plane_sound_started &&
      now >= active_airstrike.plane_finished_ticks) {
    audio->FadeOutAirstrikePlane(
        static_cast<int>(AIRSTRIKE_PROPELLER_FADE_OUT_MS));
    active_airstrike.plane_sound_fadeout_started = true;
  }
#endif

  for (auto &bomb : active_airstrike.bombs) {
    if (bomb.exploded || now < bomb.explode_ticks) {
      continue;
    }

    bomb.exploded = true;
    DetonateAirstrikeBomb(bomb, now);
  }

  const bool all_bombs_resolved =
      std::all_of(active_airstrike.bombs.begin(), active_airstrike.bombs.end(),
                  [](const AirstrikeBomb &bomb) { return bomb.exploded; });
  if (all_bombs_resolved && now >= active_airstrike.finished_ticks) {
    active_airstrike = {};
  }
}

void Game::UpdateRockets(Uint32 now) {
  for (size_t index = 0; index < active_rockets.size();) {
    RocketProjectile &rocket = active_rockets[index];
    const Uint32 step_duration_ms = std::max<Uint32>(1, rocket.step_duration_ms);
    bool detonate_rocket = false;
    bool hit_wall = false;
    MapCoord impact_coord = rocket.current_coord;
    Monster *hit_monster = nullptr;

    while (!detonate_rocket &&
           now - rocket.segment_started_ticks >= step_duration_ms) {
      const MapCoord next_coord = StepCoord(rocket.current_coord, rocket.direction);
      if (IsBlockingMapElement(
              map->map_entry(static_cast<size_t>(next_coord.u),
                             static_cast<size_t>(next_coord.v)))) {
        impact_coord = rocket.current_coord;
        hit_wall = true;
        detonate_rocket = true;
        break;
      }

      const SDL_FPoint rocket_segment_start =
          MakeCellCenter(rocket.current_coord);
      const SDL_FPoint rocket_segment_end = MakeCellCenter(next_coord);
      rocket.current_coord = next_coord;
      rocket.segment_started_ticks += step_duration_ms;

      for (auto *monster : monsters) {
        if (!monster->is_alive) {
          continue;
        }

        const SDL_FPoint monster_center = PreciseWorldCenter(monster);
        if (SegmentHitsCircle(rocket_segment_start, rocket_segment_end,
                              monster_center,
                              MONSTER_HITBOX_RADIUS_CELLS +
                                  ROCKET_HITBOX_RADIUS_CELLS)) {
          hit_monster = monster;
          impact_coord = rocket.current_coord;
          detonate_rocket = true;
          break;
        }
      }
      if (detonate_rocket) {
        break;
      }

      for (AlienAgent &alien : aliens) {
        if (!alien.is_alive) {
          continue;
        }

        const SDL_FPoint alien_center = AlienWorldCenter(alien);
        if (SegmentHitsCircle(rocket_segment_start, rocket_segment_end,
                              alien_center,
                              ALIEN_HITBOX_RADIUS_CELLS +
                                  ROCKET_HITBOX_RADIUS_CELLS)) {
          impact_coord = rocket.current_coord;
          detonate_rocket = true;
          break;
        }
      }
    }

    if (detonate_rocket) {
      const RocketProjectile detonated_rocket = rocket;
      active_rockets.erase(active_rockets.begin() + static_cast<long>(index));
      DetonateRocket(detonated_rocket, impact_coord, hit_monster, hit_wall, now);
    } else {
      if (now - rocket.last_smoke_spawn_ticks >=
          ROCKET_TRAIL_SMOKE_SPAWN_INTERVAL_MS) {
        SpawnRocketTrailSmoke(rocket, now);
        rocket.last_smoke_spawn_ticks = now;
      }
      index++;
    }
  }
}

void Game::UpdateElectrifiedMonsters(Uint32 now) {
  const auto get_monster_world_center = [](const Monster *current) {
    SDL_FPoint center = MakeCellCenter(current->map_coord);
    center.x += static_cast<float>(current->px_delta.x) / 100.0f;
    center.y += static_cast<float>(current->px_delta.y) / 100.0f;
    return center;
  };
  const auto find_charge_target = [&](Monster *current) -> Monster * {
    if (map == nullptr || current == nullptr || !current->is_alive ||
        !current->is_electrified ||
        current->biohazard_paralyzed_until_ticks > now) {
      return nullptr;
    }

    Monster *best_target = nullptr;
    int best_distance = std::numeric_limits<int>::max();
    for (Monster *other : monsters) {
      if (other == nullptr || other == current || !other->is_alive) {
        continue;
      }

      Directions direction = Directions::None;
      if (!HasLineOfSight(map, current->map_coord, other->map_coord, direction)) {
        continue;
      }

      const int distance = std::abs(other->map_coord.u - current->map_coord.u) +
                           std::abs(other->map_coord.v - current->map_coord.v);
      if (distance >= best_distance) {
        continue;
      }

      best_distance = distance;
      best_target = other;
    }

    return best_target;
  };

  for (Monster *monster : monsters) {
    if (monster == nullptr || !monster->is_alive || !monster->is_electrified) {
      continue;
    }

    if (monster->biohazard_paralyzed_until_ticks > now) {
      monster->electrified_charge_target_id = -1;
      continue;
    }

    Monster *target = find_charge_target(monster);
    const int next_target_id = (target != nullptr) ? target->id : -1;
    if (next_target_id != -1 &&
        monster->electrified_charge_target_id != next_target_id) {
#ifdef AUDIO
      audio->PlayElectrifiedMonsterRoar();
#endif
    }
    monster->electrified_charge_target_id = next_target_id;

    for (Monster *other : monsters) {
      if (other == nullptr || other == monster || !other->is_alive) {
        continue;
      }

      const SDL_FPoint monster_center = get_monster_world_center(monster);
      const SDL_FPoint other_center = get_monster_world_center(other);
      if (!CirclesOverlap(monster_center, MONSTER_HITBOX_RADIUS_CELLS,
                          other_center, MONSTER_HITBOX_RADIUS_CELLS)) {
        continue;
      }
      const SDL_FPoint shared_center =
          ScalePoint(AddPoint(monster_center, other_center), 0.5f);
      GameEffect shockwave_effect{monster->map_coord, now,
                                  EffectType::PlasmaShockwave,
                                  PLASMA_SHOCKWAVE_RADIUS_CELLS};
      shockwave_effect.has_precise_world_center = true;
      shockwave_effect.precise_world_center = shared_center;
      effects.push_back(shockwave_effect);
#ifdef AUDIO
      audio->PlayElectrifiedMonsterImpact();
#endif

      const size_t effect_count_before = effects.size();
      EliminateMonster(monster, now);
      EliminateMonster(other, now);
      for (size_t effect_index = effect_count_before;
           effect_index < effects.size(); ++effect_index) {
        if (effects[effect_index].type == EffectType::MonsterExplosion) {
          effects[effect_index].has_precise_world_center = true;
          effects[effect_index].precise_world_center = shared_center;
        }
      }
      break;
    }
  }
}

void Game::ScheduleMonsterBlast(Monster *monster, Uint32 trigger_ticks) {
  if (monster == nullptr || !monster->is_alive) {
    return;
  }

  monster->is_alive = false;
  monster->px_delta.x = 0;
  monster->px_delta.y = 0;
  monster->electrified_charge_target_id = -1;
  monster->scheduled_dynamite_blast_ticks = trigger_ticks;
  monster->scheduled_dynamite_blast_coord = monster->map_coord;
  monster->death_coord = monster->map_coord;
  scheduled_monster_blasts.push_back(
      {monster, monster->map_coord, trigger_ticks});
}

void Game::DetonateDynamite(const PlacedDynamite &dynamite, Uint32 now) {
  effects.push_back(
      {dynamite.coord, now, EffectType::DynamiteExplosion,
       DYNAMITE_EXPLOSION_RADIUS_CELLS});
  SpawnExplosionSmokeCloud(MakeCellCenter(dynamite.coord),
                           DYNAMITE_EXPLOSION_SMOKE_RADIUS_CELLS, now,
                           DYNAMITE_EXPLOSION_SMOKE_DENSITY_MULTIPLIER);
#ifdef AUDIO
  audio->PlayDynamiteExplosion();
#endif

  for (auto *monster : monsters) {
    if (!monster->is_alive ||
        !IsWithinDynamiteRadius(dynamite.coord, monster->map_coord)) {
      continue;
    }

    const int distance_steps =
        std::max(std::abs(monster->map_coord.u - dynamite.coord.u),
                 std::abs(monster->map_coord.v - dynamite.coord.v));
    const Uint32 trigger_ticks =
        now + kDynamiteChainDelayMs +
        static_cast<Uint32>(std::max(0, distance_steps) * 80);
    ScheduleMonsterBlast(monster, trigger_ticks);
  }

  for (AlienAgent &alien : aliens) {
    if (!alien.is_alive ||
        !IsWithinDynamiteRadius(dynamite.coord, alien.coord)) {
      continue;
    }

    const int distance_steps =
        std::max(std::abs(alien.coord.u - dynamite.coord.u),
                 std::abs(alien.coord.v - dynamite.coord.v));
    const Uint32 trigger_ticks =
        now + kDynamiteChainDelayMs +
        static_cast<Uint32>(std::max(0, distance_steps) * 80);
    EliminateAlien(&alien, trigger_ticks);
  }

  if (!IsPacmanInvulnerable(now) &&
      IsWithinDynamiteRadius(dynamite.coord, pacman->map_coord)) {
    TriggerLoss(pacman->map_coord, now);
  }
}

void Game::DetonatePlasticExplosive(const PlacedPlasticExplosive &explosive,
                                    Uint32 now) {
  effects.push_back({explosive.coord, now, EffectType::DynamiteExplosion, 1});
  const SDL_FPoint explosive_center = MakeCellCenter(explosive.coord);
  SpawnExplosionSmokeCloud(explosive_center,
                           PLASTIC_EXPLOSIVE_SMOKE_RADIUS_CELLS, now,
                           PLASTIC_EXPLOSIVE_SMOKE_DENSITY_MULTIPLIER);

  const bool targets_breakable_wall =
      IsInsideMapBounds(map, explosive.coord) &&
      map->map_entry(static_cast<size_t>(explosive.coord.u),
                     static_cast<size_t>(explosive.coord.v)) ==
          ElementType::TYPE_WALL &&
      !IsOuterWallCoord(map, explosive.coord);
  if (targets_breakable_wall) {
    map->SetEntry(explosive.coord, ElementType::TYPE_PATH);
    SpawnWallDestructionParticles(explosive_center, now);
    SpawnWallRubble(explosive.coord);
#ifdef AUDIO
    audio->PlayPlasticExplosiveWallBreak();
#endif
  }

  bool eliminated_monster = false;
  bool eliminated_alien = false;
  for (auto *monster : monsters) {
    if (!monster->is_alive) {
      continue;
    }

    SDL_FPoint monster_center = MakeCellCenter(monster->map_coord);
    monster_center.x += static_cast<float>(monster->px_delta.x) / 100.0f;
    monster_center.y += static_cast<float>(monster->px_delta.y) / 100.0f;
    if (PointDistance(explosive_center, monster_center) >
        kPlasticExplosiveMonsterHitRadiusCells) {
      continue;
    }

    EliminateMonster(monster, now);
    eliminated_monster = true;
  }

  for (AlienAgent &alien : aliens) {
    if (!alien.is_alive) {
      continue;
    }

    if (PointDistance(explosive_center, AlienWorldCenter(alien)) >
        kPlasticExplosiveMonsterHitRadiusCells) {
      continue;
    }

    EliminateAlien(&alien, now);
    eliminated_alien = true;
  }

  if (!targets_breakable_wall && !eliminated_monster && !eliminated_alien) {
#ifdef AUDIO
    audio->PlayMonsterExplosion();
#endif
  }

  if (!IsPacmanInvulnerable(now) &&
      SameCoord(pacman->map_coord, explosive.coord)) {
    TriggerLoss(pacman->map_coord, now);
  }
}

void Game::DetonateAirstrikeBomb(const AirstrikeBomb &bomb, Uint32 now) {
  effects.push_back(
      {bomb.coord, now, EffectType::AirstrikeExplosion,
       AIRSTRIKE_EXPLOSION_RADIUS_CELLS});
  SpawnExplosionSmokeCloud(MakeCellCenter(bomb.coord),
                           AIRSTRIKE_EXPLOSION_RADIUS_CELLS, now);
#ifdef AUDIO
  audio->PlayAirstrikeExplosion();
#endif

  for (auto *monster : monsters) {
    if (!monster->is_alive ||
        !IsWithinRadius(bomb.coord, monster->map_coord,
                        AIRSTRIKE_EXPLOSION_RADIUS_CELLS)) {
      continue;
    }

    EliminateMonster(monster, now);
  }

  for (AlienAgent &alien : aliens) {
    if (!alien.is_alive ||
        !IsWithinRadius(bomb.coord, alien.coord,
                        AIRSTRIKE_EXPLOSION_RADIUS_CELLS)) {
      continue;
    }

    EliminateAlien(&alien, now);
  }

  if (!IsPacmanInvulnerable(now) &&
      IsWithinRadius(bomb.coord, pacman->map_coord,
                     AIRSTRIKE_EXPLOSION_RADIUS_CELLS)) {
    TriggerLoss(pacman->map_coord, now);
  }
}

void Game::DetonateRocket(const RocketProjectile &rocket, MapCoord impact_coord,
                          Monster *hit_monster, bool hit_wall, Uint32 now) {
#ifdef AUDIO
  audio->StopRocketLaunch();
  audio->PlayDynamiteExplosion();
#endif
  GameEffect explosion_effect{impact_coord, now, EffectType::DynamiteExplosion,
                              ROCKET_EXPLOSION_VISIBLE_RADIUS_CELLS};
  if (hit_wall) {
    explosion_effect.has_precise_world_center = true;
    explosion_effect.precise_world_center = MakeCellCenter(rocket.current_coord);
    switch (rocket.direction) {
    case Directions::Up:
      explosion_effect.precise_world_center.y -= 0.5f;
      break;
    case Directions::Down:
      explosion_effect.precise_world_center.y += 0.5f;
      break;
    case Directions::Left:
      explosion_effect.precise_world_center.x -= 0.5f;
      break;
    case Directions::Right:
      explosion_effect.precise_world_center.x += 0.5f;
      break;
    case Directions::None:
    default:
      break;
    }
  }
  effects.push_back(explosion_effect);
  const SDL_FPoint explosion_center =
      explosion_effect.has_precise_world_center
          ? explosion_effect.precise_world_center
          : MakeCellCenter(impact_coord);
  SpawnRocketBlastSmokeCloud(explosion_center, now);

  for (auto *monster : monsters) {
    if (!monster->is_alive ||
        !IsWithinDynamiteRadius(impact_coord, monster->map_coord)) {
      continue;
    }

    const int distance_steps =
        std::max(std::abs(monster->map_coord.u - impact_coord.u),
                 std::abs(monster->map_coord.v - impact_coord.v));
    const Uint32 trigger_ticks =
        now + kDynamiteChainDelayMs +
        static_cast<Uint32>(std::max(0, distance_steps) * 80);
    ScheduleMonsterBlast(monster, trigger_ticks);
  }

  for (AlienAgent &alien : aliens) {
    if (!alien.is_alive ||
        !IsWithinDynamiteRadius(impact_coord, alien.coord)) {
      continue;
    }

    const int distance_steps =
        std::max(std::abs(alien.coord.u - impact_coord.u),
                 std::abs(alien.coord.v - impact_coord.v));
    const Uint32 trigger_ticks =
        now + kDynamiteChainDelayMs +
        static_cast<Uint32>(std::max(0, distance_steps) * 80);
    EliminateAlien(&alien, trigger_ticks);
  }

  if (!IsPacmanInvulnerable(now) &&
      IsWithinDynamiteRadius(impact_coord, pacman->map_coord)) {
    TriggerLoss(pacman->map_coord, now);
  }

  (void)hit_monster;
}

void Game::UpdatePlacedDynamites(Uint32 now) {
  for (size_t index = 0; index < placed_dynamites.size();) {
    if (now < placed_dynamites[index].explode_at_ticks) {
      index++;
      continue;
    }

    const PlacedDynamite detonated_dynamite = placed_dynamites[index];
    placed_dynamites.erase(placed_dynamites.begin() + static_cast<long>(index));
    DetonateDynamite(detonated_dynamite, now);
  }
}

void Game::UpdateScheduledMonsterBlasts(Uint32 now) {
  for (size_t index = 0; index < scheduled_monster_blasts.size();) {
    ScheduledMonsterBlast &blast = scheduled_monster_blasts[index];
    if (now < blast.trigger_ticks) {
      index++;
      continue;
    }

    if (blast.monster != nullptr) {
      blast.monster->scheduled_dynamite_blast_ticks = 0;
      blast.monster->death_coord = blast.coord;
      blast.monster->death_started_ticks = now;
      effects.push_back({blast.coord, now, EffectType::MonsterExplosion, 1});
      SpawnMonsterExplosionParticles(MakeCellCenter(blast.coord), now);
#ifdef AUDIO
      audio->PlayDynamiteExplosion();
#endif
    }

    scheduled_monster_blasts.erase(scheduled_monster_blasts.begin() +
                                   static_cast<long>(index));
  }
}

void Game::UpdateInvulnerabilityPotion(Uint32 now) {
  const DifficultyTuning tuning = GetDifficultyTuning(difficulty);
  TrySpawnInvulnerabilityPotion(now);
  if (!invulnerability_potion.is_visible) {
    return;
  }

  if (SameCoord(pacman->map_coord, invulnerability_potion.coord)) {
    ActivateInvulnerability(now);
#ifdef AUDIO
    audio->PlayCoin();
#endif
    invulnerability_potion.is_visible = false;
    invulnerability_potion.is_fading = false;
    invulnerability_potion.fade_started_ticks = 0;
    ScheduleNextInvulnerabilityPotionSpawn(now);
    return;
  }

  if (!invulnerability_potion.is_fading &&
      now - invulnerability_potion.appeared_ticks >= tuning.pickup_visible_ms) {
    invulnerability_potion.is_fading = true;
    invulnerability_potion.fade_started_ticks = now;
  }

  if (invulnerability_potion.is_fading &&
      now - invulnerability_potion.fade_started_ticks >= BLUE_POTION_FADE_MS) {
    invulnerability_potion.is_visible = false;
    invulnerability_potion.is_fading = false;
    invulnerability_potion.fade_started_ticks = 0;
    ScheduleNextInvulnerabilityPotionSpawn(now);
  }
}

void Game::UpdateInvulnerability(Uint32 now) {
  if (life_recovery_until_ticks != 0 && now >= life_recovery_until_ticks) {
    life_recovery_until_ticks = 0;
  }

  if (pacman->invulnerable_until_ticks != 0 &&
      now >= pacman->invulnerable_until_ticks) {
    pacman->invulnerable_until_ticks = 0;
#ifdef AUDIO
    audio->StopInvulnerabilityLoop();
#endif
  }
}

void Game::UpdateFinalLossSequence(Uint32 now) {
  if (dead || final_loss_commits_at_ticks == 0 ||
      now < final_loss_commits_at_ticks) {
    return;
  }

  final_loss_commits_at_ticks = 0;
#ifdef AUDIO
  audio->StartLoseMusic();
#endif
  dead = true;
}

bool Game::IsPacmanInvulnerable(Uint32 now) const {
  return pacman != nullptr &&
         (pacman->invulnerable_until_ticks > now ||
          life_recovery_until_ticks > now);
}

bool Game::IsPacmanRecoveringFromLifeLoss(Uint32 now) const {
  return pacman != nullptr && life_recovery_until_ticks > now;
}

Monster *Game::FindMonsterById(int monster_id) const {
  for (Monster *monster : monsters) {
    if (monster != nullptr && monster->id == monster_id) {
      return monster;
    }
  }

  return nullptr;
}

void Game::ElectrifyMonster(Monster *monster, Uint32 now) {
  if (monster == nullptr || !monster->is_alive || monster->monster_char == GOAT) {
    return;
  }

  monster->is_electrified = true;
  monster->electrified_charge_target_id = -1;
  monster->electrified_started_ticks = now;
  monster->electrified_visual_seed =
      static_cast<int>((now % 997) + monster->id * 41 + monster->map_coord.u * 13 +
                       monster->map_coord.v * 17);
}

SDL_FPoint Game::PreciseWorldCenter(const MapElement *element) const {
  if (element == nullptr) {
    return SDL_FPoint{0.0f, 0.0f};
  }
  SDL_FPoint center = MakeCellCenter(element->map_coord);
  center.x += static_cast<float>(element->px_delta.x) / 100.0f;
  center.y += static_cast<float>(element->px_delta.y) / 100.0f;
  return center;
}

SDL_FPoint Game::AlienWorldCenter(const AlienAgent &alien) const {
  return MakeCellCenter(alien.coord);
}

namespace {
SDL_FPoint DirectionVector(Directions direction) {
  switch (direction) {
  case Directions::Up:
    return SDL_FPoint{0.0f, -1.0f};
  case Directions::Down:
    return SDL_FPoint{0.0f, 1.0f};
  case Directions::Left:
    return SDL_FPoint{-1.0f, 0.0f};
  case Directions::Right:
    return SDL_FPoint{1.0f, 0.0f};
  default:
    return SDL_FPoint{0.0f, 0.0f};
  }
}
}  // namespace

SDL_FPoint Game::FireballWorldCenter(const Fireball &fireball,
                                     Uint32 now) const {
  const Uint32 step_duration_ms =
      std::max<Uint32>(1, fireball.step_duration_ms);
  const Uint32 elapsed = (now > fireball.segment_started_ticks)
                             ? (now - fireball.segment_started_ticks)
                             : 0;
  const float progress = std::min(
      1.0f, static_cast<float>(elapsed) / static_cast<float>(step_duration_ms));
  const SDL_FPoint start_center = MakeCellCenter(fireball.current_coord);
  const SDL_FPoint dir = DirectionVector(fireball.direction);
  return SDL_FPoint{start_center.x + dir.x * progress,
                    start_center.y + dir.y * progress};
}

SDL_FPoint Game::SlimeballWorldCenter(const Slimeball &slimeball,
                                      Uint32 now) const {
  const Uint32 step_duration_ms =
      std::max<Uint32>(1, slimeball.step_duration_ms);
  const Uint32 elapsed = (now > slimeball.segment_started_ticks)
                             ? (now - slimeball.segment_started_ticks)
                             : 0;
  const float progress = std::min(
      1.0f, static_cast<float>(elapsed) / static_cast<float>(step_duration_ms));
  const SDL_FPoint start_center = MakeCellCenter(slimeball.current_coord);
  const SDL_FPoint dir = DirectionVector(slimeball.direction);
  return SDL_FPoint{start_center.x + dir.x * progress,
                    start_center.y + dir.y * progress};
}

SDL_FPoint Game::RocketWorldCenter(const RocketProjectile &rocket,
                                   Uint32 now) const {
  const Uint32 step_duration_ms =
      std::max<Uint32>(1, rocket.step_duration_ms);
  const Uint32 elapsed = (now > rocket.segment_started_ticks)
                             ? (now - rocket.segment_started_ticks)
                             : 0;
  const float progress = std::min(
      1.0f, static_cast<float>(elapsed) / static_cast<float>(step_duration_ms));
  const SDL_FPoint start_center = MakeCellCenter(rocket.current_coord);
  const SDL_FPoint dir = DirectionVector(rocket.direction);
  return SDL_FPoint{start_center.x + dir.x * progress,
                    start_center.y + dir.y * progress};
}

bool Game::CirclesOverlap(SDL_FPoint a, float radius_a, SDL_FPoint b,
                          float radius_b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float reach = radius_a + radius_b;
  return (dx * dx + dy * dy) <= (reach * reach);
}

bool Game::SegmentHitsCircle(SDL_FPoint segment_start, SDL_FPoint segment_end,
                             SDL_FPoint circle_center, float circle_radius) {
  const float seg_dx = segment_end.x - segment_start.x;
  const float seg_dy = segment_end.y - segment_start.y;
  const float length_squared = seg_dx * seg_dx + seg_dy * seg_dy;
  float t = 0.0f;
  if (length_squared > 0.0f) {
    t = ((circle_center.x - segment_start.x) * seg_dx +
         (circle_center.y - segment_start.y) * seg_dy) /
        length_squared;
    t = std::clamp(t, 0.0f, 1.0f);
  }
  const float closest_x = segment_start.x + seg_dx * t;
  const float closest_y = segment_start.y + seg_dy * t;
  const float dx = circle_center.x - closest_x;
  const float dy = circle_center.y - closest_y;
  return (dx * dx + dy * dy) <= (circle_radius * circle_radius);
}

void Game::UpdateFireballs(Uint32 now) {
  for (size_t index = 0; index < fireballs.size();) {
    Fireball &fireball = fireballs[index];
    const Uint32 step_duration_ms = std::max<Uint32>(1, fireball.step_duration_ms);
    bool remove_fireball = false;

    while (!remove_fireball &&
           now - fireball.segment_started_ticks >= step_duration_ms) {
      MapCoord next_coord = StepCoord(fireball.current_coord, fireball.direction);

      if (IsBlockingMapElement(map->map_entry(next_coord.u, next_coord.v))) {
        effects.push_back({next_coord, now, EffectType::WallImpact, 0});
#ifdef AUDIO
        audio->PlayFireballWallHit();
#endif
        remove_fireball = true;
        break;
      }

      const SDL_FPoint segment_start = MakeCellCenter(fireball.current_coord);
      const SDL_FPoint segment_end = MakeCellCenter(next_coord);
      fireball.current_coord = next_coord;
      fireball.segment_started_ticks += step_duration_ms;

      const SDL_FPoint pacman_center = PreciseWorldCenter(pacman);
      if (SegmentHitsCircle(segment_start, segment_end, pacman_center,
                            PACMAN_HITBOX_RADIUS_CELLS +
                                FIREBALL_HITBOX_RADIUS_CELLS)) {
#ifdef AUDIO
        audio->PlayFireballWallHit();
#endif
        const Monster *owner = FindMonsterById(fireball.owner_id);
        if ((owner == nullptr || !owner->is_electrified) &&
            !IsPacmanInvulnerable(now)) {
          TriggerLoss(fireball.current_coord, now);
        }
        remove_fireball = true;
        break;
      }

      for (auto monster : monsters) {
        if (!monster->is_alive || monster->id == fireball.owner_id) {
          continue;
        }

        const SDL_FPoint monster_center = PreciseWorldCenter(monster);
        if (SegmentHitsCircle(segment_start, segment_end, monster_center,
                              MONSTER_HITBOX_RADIUS_CELLS +
                                  FIREBALL_HITBOX_RADIUS_CELLS)) {
          EliminateMonster(monster, now);
          remove_fireball = true;
          break;
        }
      }
      if (remove_fireball) {
        break;
      }

      for (AlienAgent &alien : aliens) {
        if (!alien.is_alive) {
          continue;
        }

        const SDL_FPoint alien_center = AlienWorldCenter(alien);
        if (SegmentHitsCircle(segment_start, segment_end, alien_center,
                              ALIEN_HITBOX_RADIUS_CELLS +
                                  FIREBALL_HITBOX_RADIUS_CELLS)) {
          EliminateAlien(&alien, now);
          remove_fireball = true;
          break;
        }
      }
    }

    if (remove_fireball) {
      fireballs.erase(fireballs.begin() + static_cast<long>(index));
    } else {
      index++;
    }
  }
}

void Game::UpdateSlimeballs(Uint32 now) {
  for (size_t index = 0; index < slimeballs.size();) {
    Slimeball &slimeball = slimeballs[index];
    const Uint32 step_duration_ms =
        std::max<Uint32>(1, slimeball.step_duration_ms);
    bool remove_slimeball = false;

    while (!remove_slimeball &&
           now - slimeball.segment_started_ticks >= step_duration_ms) {
      MapCoord next_coord = StepCoord(slimeball.current_coord, slimeball.direction);

      if (IsBlockingMapElement(map->map_entry(next_coord.u, next_coord.v))) {
        GameEffect slime_impact_effect{
            slimeball.current_coord, now, EffectType::SlimeSplash, 1};
        slime_impact_effect.has_precise_world_center = true;
        slime_impact_effect.precise_world_center =
            MakeCellCenter(slimeball.current_coord);
        switch (slimeball.direction) {
        case Directions::Up:
          slime_impact_effect.precise_world_center.y -= 0.5f;
          break;
        case Directions::Down:
          slime_impact_effect.precise_world_center.y += 0.5f;
          break;
        case Directions::Left:
          slime_impact_effect.precise_world_center.x -= 0.5f;
          break;
        case Directions::Right:
          slime_impact_effect.precise_world_center.x += 0.5f;
          break;
        case Directions::None:
        default:
          break;
        }
        effects.push_back(slime_impact_effect);
#ifdef AUDIO
        audio->PlaySlimeImpact();
#endif
        remove_slimeball = true;
        break;
      }

      const SDL_FPoint slime_segment_start =
          MakeCellCenter(slimeball.current_coord);
      const SDL_FPoint slime_segment_end = MakeCellCenter(next_coord);
      slimeball.current_coord = next_coord;
      slimeball.segment_started_ticks += step_duration_ms;

      const SDL_FPoint pacman_center = PreciseWorldCenter(pacman);
      if (SegmentHitsCircle(slime_segment_start, slime_segment_end,
                            pacman_center,
                            PACMAN_HITBOX_RADIUS_CELLS +
                                SLIMEBALL_HITBOX_RADIUS_CELLS)) {
#ifdef AUDIO
        audio->PlaySlimeImpact();
#endif
        const Monster *owner = FindMonsterById(slimeball.owner_id);
        if ((owner == nullptr || !owner->is_electrified) &&
            !IsPacmanInvulnerable(now)) {
          pacman->paralyzed_until_ticks =
              std::max(pacman->paralyzed_until_ticks,
                       now + static_cast<Uint32>(PACMAN_PARALYSIS_MS));
          pacman->px_delta.x = 0;
          pacman->px_delta.y = 0;
          ActivateSlimeCover(now);
        }
        remove_slimeball = true;
        break;
      }
    }

    if (remove_slimeball) {
      slimeballs.erase(slimeballs.begin() + static_cast<long>(index));
    } else {
      index++;
    }
  }
}

void Game::UpdateNuclearCraterClouds(Uint32 now) {
  if (pacman == nullptr || nuclear_craters.empty() ||
      IsPacmanInvulnerable(now)) {
    return;
  }
  const SDL_FPoint pacman_world{
      static_cast<float>(pacman->map_coord.v) + 0.5f +
          static_cast<float>(pacman->px_delta.x) / 100.0f,
      static_cast<float>(pacman->map_coord.u) + 0.5f +
          static_cast<float>(pacman->px_delta.y) / 100.0f};
  for (const NuclearCrater &crater : nuclear_craters) {
    if (now < crater.visible_ticks) {
      continue;
    }
    const Uint32 age = now - crater.visible_ticks;
    if (age >= NUCLEAR_CRATER_GREEN_CLOUD_LIFETIME_MS) {
      continue;
    }
    const float dx = pacman_world.x - crater.world_center.x;
    const float dy = pacman_world.y - crater.world_center.y;
    const float radius_cells =
        crater.radius_cells * NUCLEAR_CRATER_GREEN_CLOUD_RADIUS_FACTOR;
    if (dx * dx + dy * dy <= radius_cells * radius_cells) {
      TriggerLoss(pacman->map_coord, now);
      return;
    }
  }
}

void Game::UpdateGasClouds(Uint32 now) {
  for (size_t index = 0; index < gas_clouds.size();) {
    GasCloud &cloud = gas_clouds[index];

    if (!cloud.is_fading && now >= cloud.fade_started_ticks) {
      cloud.is_fading = true;
    }

    if (!cloud.is_fading && SameCoord(pacman->map_coord, cloud.coord)) {
      cloud.is_fading = true;
      cloud.fade_started_ticks = now;
      cloud.triggered_by_pacman = true;
      const Monster *owner = FindMonsterById(cloud.owner_id);
      if ((owner == nullptr || !owner->is_electrified) &&
          !IsPacmanInvulnerable(now)) {
        ActivateSlimeCover(now);
      }
    }

    if (cloud.is_fading &&
        now - cloud.fade_started_ticks >= GAS_CLOUD_FADE_MS) {
      gas_clouds.erase(gas_clouds.begin() + static_cast<long>(index));
    } else {
      index++;
    }
  }
}

void Game::ActivateSlimeCover(Uint32 now) {
  pacman->slimed_until_ticks =
      std::max(pacman->slimed_until_ticks,
               now + static_cast<Uint32>(SLIME_OVERLAY_HOLD_MS +
                                         SLIME_OVERLAY_FADE_MS));
}

void Game::CleanupEffects(Uint32 now) {
  effects.erase(
      std::remove_if(effects.begin(), effects.end(),
                     [now](const GameEffect &effect) {
                       if (now < effect.started_ticks) {
                         return false;
                       }

                       Uint32 max_age = kWallImpactDurationMs;
                       if (effect.type == EffectType::MonsterExplosion) {
                         max_age = kMonsterExplosionDurationMs;
                       } else if (effect.type ==
                                  EffectType::BiohazardImpactFlash) {
                         max_age = kBiohazardImpactFlashDurationMs;
                       } else if (effect.type ==
                                  EffectType::DynamiteExplosion) {
                         max_age = DYNAMITE_EXPLOSION_DURATION_MS;
                       } else if (effect.type ==
                                  EffectType::AirstrikeExplosion) {
                         max_age = AIRSTRIKE_EXPLOSION_DURATION_MS;
                       } else if (effect.type == EffectType::SlimeSplash) {
                         max_age = kSlimeSplashDurationMs;
                       } else if (effect.type == EffectType::PlasmaShockwave) {
                         max_age = kPlasmaShockwaveDurationMs;
                       } else if (effect.type == EffectType::NuclearExplosion) {
                         max_age = kNuclearExplosionDurationMs;
                       } else if (effect.type ==
                                  EffectType::NuclearExplosionB) {
                         max_age = kNuclearExplosionBDurationMs;
                       } else if (effect.type == EffectType::AlienExplosion) {
                         max_age = kAlienExplosionDurationMs;
                       }
                       return now - effect.started_ticks > max_age;
                     }),
      effects.end());
}

void Game::EliminateAlien(AlienAgent *alien, Uint32 sequence_start_ticks) {
  if (alien == nullptr || !alien->is_alive) {
    return;
  }

  alien->is_alive = false;
  alien->thought_started_ticks = 0;
  alien->thought_visible_until_ticks = 0;
  alien->next_thought_ticks = 0;
  alien->animation_until_ticks = 0;
  alien->next_animation_ticks = 0;
  alien->scream_trigger_ticks = sequence_start_ticks + ALIEN_SCREAM_DELAY_MS;
  alien->explosion_trigger_ticks =
      alien->scream_trigger_ticks + ALIEN_SCREAM_TO_EXPLOSION_DELAY_MS;
  alien->scream_played = false;
  alien->explosion_spawned = false;
}

void Game::EliminateMonsterWithDynamiteBlast(Monster *monster, Uint32 now) {
  if (monster == nullptr || !monster->is_alive) {
    return;
  }

  SDL_FPoint sprite_center = MakeCellCenter(monster->map_coord);
  sprite_center.x += static_cast<float>(monster->px_delta.x) / 100.0f;
  sprite_center.y += static_cast<float>(monster->px_delta.y) / 100.0f;

  monster->is_alive = false;
  monster->px_delta.x = 0;
  monster->px_delta.y = 0;
  monster->electrified_charge_target_id = -1;
  monster->scheduled_dynamite_blast_ticks = 0;
  monster->death_coord = monster->map_coord;
  monster->death_started_ticks = now;
  GameEffect explosion_effect{monster->death_coord, now,
                              EffectType::MonsterExplosion, 1};
  explosion_effect.has_precise_world_center = true;
  explosion_effect.precise_world_center = sprite_center;
  effects.push_back(explosion_effect);
  SpawnMonsterExplosionParticles(sprite_center, now);
#ifdef AUDIO
  audio->PlayDynamiteExplosion();
#endif
}

void Game::EliminateMonster(Monster *monster, Uint32 now) {
  if (monster == nullptr || !monster->is_alive) {
    return;
  }

  SDL_FPoint sprite_center = MakeCellCenter(monster->map_coord);
  sprite_center.x += static_cast<float>(monster->px_delta.x) / 100.0f;
  sprite_center.y += static_cast<float>(monster->px_delta.y) / 100.0f;

  monster->is_alive = false;
  monster->px_delta.x = 0;
  monster->px_delta.y = 0;
  monster->scheduled_dynamite_blast_ticks = 0;
  monster->electrified_charge_target_id = -1;
  monster->death_coord = monster->map_coord;
  monster->death_started_ticks = now;
  GameEffect explosion_effect{monster->death_coord, now,
                              EffectType::MonsterExplosion, 1};
  explosion_effect.has_precise_world_center = true;
  explosion_effect.precise_world_center = sprite_center;
  effects.push_back(explosion_effect);
  SpawnMonsterExplosionParticles(sprite_center, now);
#ifdef AUDIO
  audio->PlayMonsterExplosion();
#endif
}

void Game::EliminateMonsterWithDustCloud(Monster *monster, Uint32 now) {
  if (monster == nullptr || !monster->is_alive) {
    return;
  }

  SDL_FPoint sprite_center = MakeCellCenter(monster->map_coord);
  sprite_center.x += static_cast<float>(monster->px_delta.x) / 100.0f;
  sprite_center.y += static_cast<float>(monster->px_delta.y) / 100.0f;

  monster->is_alive = false;
  monster->px_delta.x = 0;
  monster->px_delta.y = 0;
  monster->scheduled_dynamite_blast_ticks = 0;
  monster->electrified_charge_target_id = -1;
  monster->death_coord = monster->map_coord;
  monster->death_started_ticks = now;
  SpawnMonsterDustCloud(sprite_center, now);
}

void Game::SpawnMonsterExplosionParticles(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> count_dist(
      MONSTER_EXPLOSION_PARTICLE_MIN_COUNT,
      MONSTER_EXPLOSION_PARTICLE_MAX_COUNT);
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> speed_dist(
      MONSTER_EXPLOSION_PARTICLE_MIN_SPEED_CELLS_PER_SEC,
      MONSTER_EXPLOSION_PARTICLE_MAX_SPEED_CELLS_PER_SEC);
  std::uniform_real_distribution<float> radius_dist(0.7f, 1.3f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const int count = count_dist(rng);
  for (int i = 0; i < count; ++i) {
    const float angle = angle_dist(rng);
    const float speed = speed_dist(rng);
    ExplosionParticle particle;
    particle.world_position = world_center;
    particle.velocity_cells_per_sec.x = std::cos(angle) * speed;
    particle.velocity_cells_per_sec.y =
        std::sin(angle) * speed -
        MONSTER_EXPLOSION_PARTICLE_INITIAL_UPWARD_BIAS * speed;
    particle.spawned_ticks = now;
    particle.last_smoke_spawn_ticks = now;
    particle.shape_seed = seed_dist(rng);
    particle.radius_cells =
        MONSTER_EXPLOSION_PARTICLE_RADIUS_CELLS * radius_dist(rng);
    particle.kind = ExplosionParticleKind::MonsterExplosion;
    explosion_particles.push_back(particle);
  }
}

void Game::SpawnMonsterDustCloud(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> radius_dist(
      0.0f, MONSTER_EXPLOSION_SMOKE_FINAL_RADIUS_CELLS * 1.3f);
  std::uniform_real_distribution<float> drift_dist(-0.16f, 0.16f);
  std::uniform_real_distribution<float> rise_dist(-0.20f, -0.04f);
  std::uniform_int_distribution<Uint32> seed_dist(1u, 0xFFFFFFu);

  for (int i = 0; i < GOAT_MONSTER_DUST_PUFF_COUNT; ++i) {
    const float angle = angle_dist(rng);
    const float radius = radius_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {world_center.x + std::cos(angle) * radius,
                           world_center.y + std::sin(angle) * radius};
    puff.velocity_cells_per_sec = {drift_dist(rng), rise_dist(rng)};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::MonsterDustCloud;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnWallDestructionParticles(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> count_dist(
      PLASTIC_EXPLOSIVE_WALL_DEBRIS_MIN_COUNT,
      PLASTIC_EXPLOSIVE_WALL_DEBRIS_MAX_COUNT);
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> speed_dist(
      PLASTIC_EXPLOSIVE_WALL_DEBRIS_MIN_SPEED_CELLS_PER_SEC,
      PLASTIC_EXPLOSIVE_WALL_DEBRIS_MAX_SPEED_CELLS_PER_SEC);
  std::uniform_real_distribution<float> radius_dist(0.75f, 1.35f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);
  std::uniform_int_distribution<int> dust_count_dist(
      PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_PUFF_MIN_COUNT,
      PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_PUFF_MAX_COUNT);
  std::uniform_real_distribution<float> dust_offset_dist(
      -PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_SPREAD_CELLS,
      PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_SPREAD_CELLS);

  const int count = count_dist(rng);
  for (int i = 0; i < count; ++i) {
    const float angle = angle_dist(rng);
    const float speed = speed_dist(rng);
    ExplosionParticle particle;
    particle.world_position = world_center;
    particle.velocity_cells_per_sec.x = std::cos(angle) * speed;
    particle.velocity_cells_per_sec.y =
        std::sin(angle) * speed -
        PLASTIC_EXPLOSIVE_WALL_DEBRIS_INITIAL_UPWARD_BIAS * speed;
    particle.spawned_ticks = now;
    particle.last_smoke_spawn_ticks =
        (now > PLASTIC_EXPLOSIVE_WALL_DUST_SPAWN_INTERVAL_MS)
            ? (now - PLASTIC_EXPLOSIVE_WALL_DUST_SPAWN_INTERVAL_MS)
            : 0;
    particle.shape_seed = seed_dist(rng);
    particle.radius_cells =
        PLASTIC_EXPLOSIVE_WALL_DEBRIS_RADIUS_CELLS * radius_dist(rng);
    particle.kind = ExplosionParticleKind::WallDebris;
    explosion_particles.push_back(particle);
  }

  const int dust_count = dust_count_dist(rng);
  for (int i = 0; i < dust_count; ++i) {
    ExplosionSmokePuff puff;
    puff.world_position = {
        world_center.x + dust_offset_dist(rng),
        world_center.y + dust_offset_dist(rng)};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::WallDust;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearBombFragments(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> count_dist(
      NUCLEAR_BOMB_DEBRIS_MIN_COUNT, NUCLEAR_BOMB_DEBRIS_MAX_COUNT);
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> speed_dist(
      NUCLEAR_BOMB_DEBRIS_MIN_SPEED_CELLS_PER_SEC,
      NUCLEAR_BOMB_DEBRIS_MAX_SPEED_CELLS_PER_SEC);
  std::uniform_real_distribution<float> radius_dist(0.75f, 1.35f);
  std::uniform_real_distribution<float> spawn_offset_dist(
      -NUCLEAR_BOMB_DEBRIS_SPAWN_SPREAD_CELLS,
      NUCLEAR_BOMB_DEBRIS_SPAWN_SPREAD_CELLS);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const int count = count_dist(rng);
  for (int i = 0; i < count; ++i) {
    const float angle = angle_dist(rng);
    const float speed = speed_dist(rng);
    ExplosionParticle particle;
    particle.world_position = {
        world_center.x + spawn_offset_dist(rng),
        world_center.y + spawn_offset_dist(rng)};
    particle.velocity_cells_per_sec.x = std::cos(angle) * speed;
    particle.velocity_cells_per_sec.y =
        std::sin(angle) * speed -
        NUCLEAR_BOMB_DEBRIS_INITIAL_UPWARD_BIAS * speed;
    particle.spawned_ticks = now;
    particle.last_smoke_spawn_ticks =
        (now > NUCLEAR_BOMB_DUST_SPAWN_INTERVAL_MS)
            ? (now - NUCLEAR_BOMB_DUST_SPAWN_INTERVAL_MS)
            : 0;
    particle.shape_seed = seed_dist(rng);
    particle.radius_cells =
        NUCLEAR_BOMB_DEBRIS_RADIUS_CELLS * radius_dist(rng);
    particle.kind = ExplosionParticleKind::NuclearDebris;
    explosion_particles.push_back(particle);
  }
}

void Game::SpawnWallRubble(MapCoord destroyed_coord) {
  if (map == nullptr || !IsInsideMapBounds(map, destroyed_coord)) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> half_size_dist(
      WALL_RUBBLE_MIN_HALF_SIZE_CELLS, WALL_RUBBLE_MAX_HALF_SIZE_CELLS);
  std::uniform_real_distribution<float> aspect_dist(
      WALL_RUBBLE_MIN_ASPECT_RATIO, WALL_RUBBLE_MAX_ASPECT_RATIO);
  std::uniform_real_distribution<float> rotation_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  auto add_piece = [&](MapCoord tile, float min_local_x, float max_local_x,
                       float min_local_y, float max_local_y, float size_scale) {
    if (!IsInsideMapBounds(map, tile)) {
      return;
    }

    const ElementType entry =
        map->map_entry(static_cast<size_t>(tile.u), static_cast<size_t>(tile.v));
    if (entry == ElementType::TYPE_WALL ||
        entry == ElementType::TYPE_CRATER ||
        entry == ElementType::TYPE_TELEPORTER) {
      return;
    }

    std::uniform_real_distribution<float> local_x_dist(min_local_x, max_local_x);
    std::uniform_real_distribution<float> local_y_dist(min_local_y, max_local_y);

    WallRubblePiece piece;
    piece.world_position = {
        static_cast<float>(tile.v) + local_x_dist(rng),
        static_cast<float>(tile.u) + local_y_dist(rng)};
    const float base_half_size = half_size_dist(rng) * size_scale;
    const float aspect_ratio = aspect_dist(rng);
    piece.half_width_cells = base_half_size;
    piece.half_height_cells =
        std::max(WALL_RUBBLE_MIN_HALF_SIZE_CELLS * 0.75f,
                 base_half_size / aspect_ratio);
    piece.rotation_radians = rotation_dist(rng);
    piece.shape_seed = seed_dist(rng);
    wall_rubble_pieces.push_back(piece);
  };

  std::uniform_int_distribution<int> center_count_dist(
      WALL_RUBBLE_CENTER_MIN_COUNT, WALL_RUBBLE_CENTER_MAX_COUNT);
  const int center_count = center_count_dist(rng);
  for (int i = 0; i < center_count; ++i) {
    add_piece(destroyed_coord, 0.12f, 0.88f, 0.14f, 0.88f, 1.0f);
  }

  for (int delta_u = -1; delta_u <= 1; ++delta_u) {
    for (int delta_v = -1; delta_v <= 1; ++delta_v) {
      if (delta_u == 0 && delta_v == 0) {
        continue;
      }

      const MapCoord tile{destroyed_coord.u + delta_u, destroyed_coord.v + delta_v};
      const bool diagonal = delta_u != 0 && delta_v != 0;
      std::uniform_int_distribution<int> count_dist(
          diagonal ? WALL_RUBBLE_NEIGHBOR_DIAGONAL_MIN_COUNT
                   : WALL_RUBBLE_NEIGHBOR_ORTHOGONAL_MIN_COUNT,
          diagonal ? WALL_RUBBLE_NEIGHBOR_DIAGONAL_MAX_COUNT
                   : WALL_RUBBLE_NEIGHBOR_ORTHOGONAL_MAX_COUNT);
      const int piece_count = count_dist(rng);
      if (piece_count <= 0) {
        continue;
      }

      float min_local_x = 0.20f;
      float max_local_x = 0.80f;
      float min_local_y = 0.20f;
      float max_local_y = 0.80f;
      if (delta_v < 0) {
        min_local_x = 0.62f;
        max_local_x = 0.96f;
      } else if (delta_v > 0) {
        min_local_x = 0.04f;
        max_local_x = 0.38f;
      }
      if (delta_u < 0) {
        min_local_y = 0.62f;
        max_local_y = 0.96f;
      } else if (delta_u > 0) {
        min_local_y = 0.04f;
        max_local_y = 0.38f;
      }

      const float size_scale = diagonal ? 0.72f : 0.82f;
      for (int i = 0; i < piece_count; ++i) {
        add_piece(tile, min_local_x, max_local_x, min_local_y, max_local_y,
                  size_scale);
      }
    }
  }
}

void Game::SpawnExplosionSmokeCloud(SDL_FPoint world_center, float radius_cells,
                                    Uint32 now, float density_multiplier) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> count_dist(
      EXPLOSION_SMOKE_CLOUD_MIN_PUFF_COUNT,
      EXPLOSION_SMOKE_CLOUD_MAX_PUFF_COUNT);
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> ring_dist(0.0f, 1.0f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float inner_radius = std::max(
      EXPLOSION_SMOKE_CLOUD_RING_MIN_RADIUS_CELLS,
      std::max(0.1f, radius_cells) *
          EXPLOSION_SMOKE_CLOUD_RING_INNER_RADIUS_FACTOR);
  const float outer_radius = std::max(
      inner_radius + 0.12f,
      std::max(0.1f, radius_cells) *
          EXPLOSION_SMOKE_CLOUD_RING_OUTER_RADIUS_FACTOR);

  const int base_puff_count = count_dist(rng);
  const int puff_count = std::max(
      1, static_cast<int>(std::lround(
             static_cast<float>(base_puff_count) *
             std::max(0.1f, density_multiplier))));
  for (int i = 0; i < puff_count; ++i) {
    const float angle = angle_dist(rng);
    const float radius =
        inner_radius + (outer_radius - inner_radius) * ring_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {
        world_center.x + std::cos(angle) * radius,
        world_center.y + std::sin(angle) * radius};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::BlastSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnGoatRedDustCloud(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> radius_dist(
      0.0f, GOAT_RED_DUST_CLOUD_RADIUS_CELLS);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);
  std::uniform_real_distribution<float> drift_dist(-0.4f, 0.4f);
  std::uniform_real_distribution<float> rise_dist(-0.2f, 0.05f);

  for (int i = 0; i < GOAT_RED_DUST_PUFF_COUNT; ++i) {
    const float angle = angle_dist(rng);
    const float radius = radius_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {
        world_center.x + std::cos(angle) * radius,
        world_center.y + std::sin(angle) * radius};
    puff.velocity_cells_per_sec = {drift_dist(rng), rise_dist(rng)};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::GoatRedDust;
    explosion_smoke_puffs.push_back(puff);
  }
}

Monster *Game::FindClosestMonsterInGoatSight(Monster *goat,
                                             Directions &direction_out) const {
  direction_out = Directions::None;
  if (goat == nullptr || map == nullptr) {
    return nullptr;
  }

  Monster *closest_target = nullptr;
  int closest_distance = std::numeric_limits<int>::max();
  for (Monster *monster : monsters) {
    if (monster == nullptr || monster == goat || !monster->is_alive ||
        monster->monster_char == GOAT) {
      continue;
    }

    Directions direction = Directions::None;
    if (!HasClearAxisLineOfSight(map, goat->map_coord, monster->map_coord,
                                 direction)) {
      continue;
    }

    const int distance = std::abs(monster->map_coord.u - goat->map_coord.u) +
                         std::abs(monster->map_coord.v - goat->map_coord.v);
    if (distance < closest_distance) {
      closest_distance = distance;
      closest_target = monster;
      direction_out = direction;
    }
  }

  return closest_target;
}

void Game::HandleGoatRequests(Uint32 now) {
  for (Monster *monster : monsters) {
    if (monster == nullptr || monster->monster_char != GOAT) {
      continue;
    }
    if (monster->is_alive && monster->goat_love_sequence_active &&
        monster->goat_love_target_id == -1 &&
        monster->goat_love_attack_at_ticks != 0 &&
        now >= monster->goat_love_attack_at_ticks) {
      Directions attack_direction = Directions::None;
      Monster *target =
          FindClosestMonsterInGoatSight(monster, attack_direction);
      if (target == nullptr || attack_direction == Directions::None) {
        // No monster in line of sight yet. Stay in love mode and retry.
        monster->goat_love_attack_at_ticks = now + GOAT_LOVE_HEART_VISIBLE_MS;
      } else {
        monster->goat_love_target_id = target->id;
        monster->goat_state = Monster::GoatState::Charging;
        monster->goat_slide_direction = attack_direction;
        monster->goat_slide_remaining_steps = GOAT_SLIDE_TILES;
        monster->grazing_until_ticks = 0;
        monster->goat_is_jumping = true;
#ifdef AUDIO
        if (audio != nullptr) {
          audio->PlayGoatBleat();
        }
#endif
      }
    }
    if (monster->is_alive && monster->goat_love_target_id != -1) {
      Monster *target = FindMonsterById(monster->goat_love_target_id);
      if (target == nullptr || !target->is_alive) {
        // Target lost before contact: stay in love mode, look for a new one.
        monster->goat_love_target_id = -1;
        monster->goat_love_attack_at_ticks = now + GOAT_LOVE_HEART_VISIBLE_MS;
        monster->goat_state = Monster::GoatState::Grazing;
        monster->goat_slide_direction = Directions::None;
        monster->goat_slide_remaining_steps = 0;
        monster->goat_is_jumping = false;
        monster->px_delta.x = 0;
        monster->px_delta.y = 0;
      } else if (CirclesOverlap(PreciseWorldCenter(monster),
                                MONSTER_HITBOX_RADIUS_CELLS,
                                PreciseWorldCenter(target),
                                MONSTER_HITBOX_RADIUS_CELLS)) {
#ifdef AUDIO
        if (audio != nullptr) {
          audio->PlayPunch();
        }
#endif
        EliminateMonsterWithDustCloud(target, now);
        monster->goat_love_target_id = -1;
        monster->goat_love_sequence_active = false;
        monster->goat_love_attack_at_ticks = 0;
        monster->goat_pacified = true;
        monster->goat_state = Monster::GoatState::Grazing;
        monster->goat_slide_direction = Directions::None;
        monster->goat_slide_remaining_steps = 0;
        monster->goat_is_jumping = false;
        monster->px_delta.x = 0;
        monster->px_delta.y = 0;
      }
    }
    if (monster->goat_request_bleat_sound) {
      monster->goat_request_bleat_sound = false;
#ifdef AUDIO
      if (audio != nullptr) {
        audio->PlayGoatBleat();
      }
#endif
    }
    if (monster->goat_request_punch_sound) {
      monster->goat_request_punch_sound = false;
#ifdef AUDIO
      if (audio != nullptr) {
        audio->PlayPunch();
      }
#endif
    }
    if (monster->goat_request_crash_sound) {
      monster->goat_request_crash_sound = false;
#ifdef AUDIO
      if (audio != nullptr) {
        audio->PlayRubbleCrash();
      }
#endif
    }
    if (monster->goat_request_wall_break) {
      monster->goat_request_wall_break = false;
      const MapCoord wall_coord = monster->goat_wall_break_coord;
      const bool breakable =
          IsInsideMapBounds(map, wall_coord) &&
          map->map_entry(static_cast<size_t>(wall_coord.u),
                         static_cast<size_t>(wall_coord.v)) ==
              ElementType::TYPE_WALL &&
          !IsOuterWallCoord(map, wall_coord);
      if (breakable) {
        const SDL_FPoint wall_center = MakeCellCenter(wall_coord);
        map->SetEntry(wall_coord, ElementType::TYPE_PATH);
        SpawnWallDestructionParticles(wall_center, now);
        SpawnWallRubble(wall_coord);
        SpawnGoatRedDustCloud(wall_center, now);
      } else {
        // Outer wall (can't break): just spawn the dust cloud at goat's
        // current cell so the visual feedback still occurs.
        SpawnGoatRedDustCloud(MakeCellCenter(monster->map_coord), now);
      }
    }
  }
}

void Game::SpawnRocketBlastSmokeCloud(SDL_FPoint world_center, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> count_dist(
      ROCKET_BLAST_SMOKE_MIN_PUFF_COUNT,
      ROCKET_BLAST_SMOKE_MAX_PUFF_COUNT);
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> ring_dist(0.0f, 1.0f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float inner_radius = ROCKET_BLAST_SMOKE_RING_INNER_RADIUS_CELLS;
  const float outer_radius = std::max(inner_radius + 0.12f,
                                      ROCKET_BLAST_SMOKE_RING_OUTER_RADIUS_CELLS);

  const int puff_count = count_dist(rng);
  for (int i = 0; i < puff_count; ++i) {
    const float angle = angle_dist(rng);
    const float radius =
        inner_radius + (outer_radius - inner_radius) * ring_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {
        world_center.x + std::cos(angle) * radius,
        world_center.y + std::sin(angle) * radius};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::RocketBlastSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnRocketTrailSmoke(const RocketProjectile &rocket, Uint32 now) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> longitudinal_jitter_dist(
      -ROCKET_TRAIL_SMOKE_LONGITUDINAL_JITTER_CELLS,
      ROCKET_TRAIL_SMOKE_LONGITUDINAL_JITTER_CELLS);
  std::uniform_real_distribution<float> lateral_offset_dist(
      -ROCKET_TRAIL_SMOKE_LATERAL_SPREAD_CELLS,
      ROCKET_TRAIL_SMOKE_LATERAL_SPREAD_CELLS);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const SDL_FPoint center = RocketWorldCenter(rocket, now);
  const SDL_FPoint forward = DirectionVector(rocket.direction);
  const SDL_FPoint lateral{-forward.y, forward.x};

  for (int i = 0; i < ROCKET_TRAIL_SMOKE_PUFFS_PER_SPAWN; ++i) {
    const float trailing_distance =
        ROCKET_TRAIL_SMOKE_BACK_OFFSET_CELLS + longitudinal_jitter_dist(rng) +
        static_cast<float>(i) * 0.05f;
    ExplosionSmokePuff puff;
    puff.world_position = {
        center.x - forward.x * trailing_distance +
            lateral.x * lateral_offset_dist(rng),
        center.y - forward.y * trailing_distance +
            lateral.y * lateral_offset_dist(rng)};
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::RocketTrailSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearRingSmoke(float ring_progress, Uint32 now) {
  if (!active_nuclear_explosion.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> origin_jitter_dist(
      -NUCLEAR_SMOKE_RING_THICKNESS_CELLS * 0.35f,
      NUCLEAR_SMOKE_RING_THICKNESS_CELLS * 0.35f);
  std::uniform_real_distribution<float> angle_jitter_dist(-0.10f, 0.10f);
  std::uniform_real_distribution<float> speed_unit_dist(0.0f, 1.0f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  (void)ring_progress;
  const int puffs_this_wave =
      std::max(4, NUCLEAR_SMOKE_RING_PUFFS_PER_WAVE);

  const float lifetime_s =
      static_cast<float>(NUCLEAR_RING_SMOKE_LIFETIME_MS) / 1000.0f;
  const float fast_speed =
      NUCLEAR_SMOKE_RING_FINAL_RADIUS_CELLS / std::max(0.1f, lifetime_s);
  const float phase = static_cast<float>(
      active_nuclear_explosion.animation_seed * 0.13 +
      static_cast<double>(now - active_nuclear_explosion.started_ticks) / 310.0);
  for (int index = 0; index < puffs_this_wave; ++index) {
    const float angle =
        (static_cast<float>(index) /
         static_cast<float>(puffs_this_wave)) *
            static_cast<float>(2.0 * M_PI) +
        phase + angle_jitter_dist(rng);
    const float dir_x = std::cos(angle);
    const float dir_y = std::sin(angle);
    ExplosionSmokePuff puff;
    puff.world_position = {
        active_nuclear_explosion.world_center.x + dir_x * origin_jitter_dist(rng),
        active_nuclear_explosion.world_center.y + dir_y * origin_jitter_dist(rng)};
    const float straggler_roll = speed_unit_dist(rng);
    float speed_factor;
    if (straggler_roll < 0.28f) {
      const float t = straggler_roll / 0.28f;
      speed_factor = 0.04f + t * 0.78f;
    } else {
      const float t = (straggler_roll - 0.28f) / 0.72f;
      speed_factor = 0.92f + t * 0.18f;
    }
    const float speed = fast_speed * speed_factor;
    puff.velocity_cells_per_sec = {dir_x * speed, dir_y * speed};
    puff.vertical_offset_cells = 0.05f;
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::NuclearRingSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearCenterSmoke(float center_smoke_progress, Uint32 now) {
  if (!active_nuclear_explosion.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> radius_unit_dist(0.0f, 1.0f);
  std::uniform_real_distribution<float> vertical_jitter_dist(0.0f, 0.8f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float spread =
      NUCLEAR_CENTER_SMOKE_SPREAD_CELLS *
      (0.24f + 0.76f * std::clamp(center_smoke_progress, 0.0f, 1.0f));
  const float base_height =
      0.14f + std::clamp(center_smoke_progress, 0.0f, 1.0f) * 2.2f;
  for (int index = 0; index < NUCLEAR_CENTER_SMOKE_PUFFS_PER_SPAWN; ++index) {
    const float angle = angle_dist(rng);
    const float radius = spread * std::sqrt(radius_unit_dist(rng));
    ExplosionSmokePuff puff;
    puff.world_position = {
        active_nuclear_explosion.world_center.x + std::cos(angle) * radius,
        active_nuclear_explosion.world_center.y + std::sin(angle) * radius};
    puff.vertical_offset_cells = base_height + vertical_jitter_dist(rng);
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::NuclearCoreSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearMushroomSmoke(float mushroom_progress, Uint32 now) {
  if (!active_nuclear_explosion.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> lateral_unit_dist(-1.0f, 1.0f);
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> radius_unit_dist(0.0f, 1.0f);
  std::uniform_real_distribution<float> cap_height_jitter_dist(
      -NUCLEAR_MUSHROOM_CAP_THICKNESS_CELLS,
      NUCLEAR_MUSHROOM_CAP_THICKNESS_CELLS);
  std::uniform_real_distribution<float> stem_height_jitter_dist(-0.38f, 0.38f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float progress = std::clamp(mushroom_progress, 0.0f, 1.0f);
  const float stem_height =
      0.75f + progress * NUCLEAR_MUSHROOM_STEM_HEIGHT_CELLS;
  const float stem_radius = 0.07f + progress * 0.16f;
  for (int index = 0; index < NUCLEAR_MUSHROOM_STEM_PUFFS_PER_SPAWN; ++index) {
    ExplosionSmokePuff puff;
    puff.world_position = {
        active_nuclear_explosion.world_center.x +
            lateral_unit_dist(rng) * stem_radius,
        active_nuclear_explosion.world_center.y +
            lateral_unit_dist(rng) * stem_radius * 0.58f};
    puff.vertical_offset_cells =
        0.30f + stem_height_jitter_dist(rng) +
        stem_height *
            (static_cast<float>(index + 1) /
             static_cast<float>(NUCLEAR_MUSHROOM_STEM_PUFFS_PER_SPAWN + 1));
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::NuclearMushroomSmoke;
    explosion_smoke_puffs.push_back(puff);
  }

  const float cap_radius =
      0.90f + progress * NUCLEAR_MUSHROOM_CAP_RADIUS_CELLS;
  const float cap_height = stem_height + 0.95f + progress * 0.65f;
  for (int index = 0; index < NUCLEAR_MUSHROOM_CAP_PUFFS_PER_SPAWN; ++index) {
    const float angle = angle_dist(rng);
    const float radius =
        cap_radius * (0.30f + 0.70f * std::sqrt(radius_unit_dist(rng)));
    ExplosionSmokePuff puff;
    puff.world_position = {
        active_nuclear_explosion.world_center.x + std::cos(angle) * radius,
        active_nuclear_explosion.world_center.y + std::sin(angle) * radius * 0.60f};
    puff.vertical_offset_cells =
        cap_height + cap_height_jitter_dist(rng) * (0.72f + 0.68f * progress);
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.kind = ExplosionSmokePuffKind::NuclearMushroomSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearBRingSmoke(float ring_progress, Uint32 now) {
  if (!active_nuclear_explosion_b.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> origin_jitter_dist(
      -NUCLEAR_B_SMOKE_RING_THICKNESS_CELLS * 0.4f,
      NUCLEAR_B_SMOKE_RING_THICKNESS_CELLS * 0.4f);
  std::uniform_real_distribution<float> angle_jitter_dist(-0.05f, 0.05f);
  std::uniform_real_distribution<float> speed_unit_dist(0.86f, 1.10f);
  std::uniform_real_distribution<float> roll_jitter_dist(-1.0f, 1.0f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  (void)ring_progress;
  const int puffs_this_wave =
      std::max(4, NUCLEAR_B_SMOKE_RING_PUFFS_PER_WAVE);

  const float lifetime_s =
      static_cast<float>(NUCLEAR_B_RING_SMOKE_LIFETIME_MS) / 1000.0f;
  const float fast_speed =
      NUCLEAR_B_SMOKE_RING_FINAL_RADIUS_CELLS / std::max(0.1f, lifetime_s);
  const float phase = static_cast<float>(
      active_nuclear_explosion_b.animation_seed * 0.17 +
      static_cast<double>(now - active_nuclear_explosion_b.started_ticks) /
          280.0);
  for (int index = 0; index < puffs_this_wave; ++index) {
    const float angle = (static_cast<float>(index) /
                         static_cast<float>(puffs_this_wave)) *
                            static_cast<float>(2.0 * M_PI) +
                        phase + angle_jitter_dist(rng);
    const float dir_x = std::cos(angle);
    const float dir_y = std::sin(angle);
    ExplosionSmokePuff puff;
    puff.world_position = {active_nuclear_explosion_b.world_center.x +
                               dir_x * origin_jitter_dist(rng),
                           active_nuclear_explosion_b.world_center.y +
                               dir_y * origin_jitter_dist(rng)};
    const float speed = fast_speed * speed_unit_dist(rng);
    puff.velocity_cells_per_sec = {dir_x * speed, dir_y * speed};
    puff.vertical_offset_cells = 0.05f;
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.rotation_radians = angle;
    puff.rotation_speed_rad_per_sec =
        NUCLEAR_B_SMOKE_RING_ROLL_SPEED_RAD_PER_SEC *
        (0.85f + 0.30f * roll_jitter_dist(rng));
    puff.kind = ExplosionSmokePuffKind::NuclearBRingSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearBTrailSmoke(float ring_progress, Uint32 now) {
  if (!active_nuclear_explosion_b.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> angle_jitter_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> radial_jitter_dist(
      -NUCLEAR_B_SMOKE_RING_THICKNESS_CELLS * 0.55f,
      NUCLEAR_B_SMOKE_RING_THICKNESS_CELLS * 0.55f);
  std::uniform_real_distribution<float> spread_unit_dist(0.0f, 1.0f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float trail_radius =
      0.1f +
      std::clamp(ring_progress, 0.0f, 1.0f) *
          (NUCLEAR_B_SMOKE_RING_FINAL_RADIUS_CELLS * 0.92f);
  for (int index = 0; index < NUCLEAR_B_TRAIL_PUFFS_PER_WAVE; ++index) {
    const float angle = angle_jitter_dist(rng);
    const float radius =
        trail_radius * std::sqrt(spread_unit_dist(rng)) +
        radial_jitter_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {active_nuclear_explosion_b.world_center.x +
                               std::cos(angle) * radius,
                           active_nuclear_explosion_b.world_center.y +
                               std::sin(angle) * radius};
    puff.velocity_cells_per_sec = {0.0f, 0.0f};
    puff.vertical_offset_cells = 0.02f;
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.rotation_radians = angle;
    puff.rotation_speed_rad_per_sec = 0.0f;
    puff.kind = ExplosionSmokePuffKind::NuclearBTrailSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearBStemSmoke(float stem_progress, Uint32 now) {
  if (!active_nuclear_explosion_b.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> lateral_dist(-1.0f, 1.0f);
  std::uniform_real_distribution<float> jitter_dist(-0.18f, 0.18f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float progress = std::clamp(stem_progress, 0.0f, 1.0f);
  const float current_top =
      0.5f + progress * NUCLEAR_B_STEM_HEIGHT_CELLS;
  for (int index = 0; index < NUCLEAR_B_STEM_PUFFS_PER_SPAWN; ++index) {
    const float vertical_position =
        progress > 0.0f
            ? jitter_dist(rng) +
                  current_top *
                      (static_cast<float>(index) /
                       static_cast<float>(NUCLEAR_B_STEM_PUFFS_PER_SPAWN))
            : 0.30f + jitter_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {active_nuclear_explosion_b.world_center.x +
                               lateral_dist(rng) *
                                   NUCLEAR_B_STEM_RADIUS_CELLS * 0.55f,
                           active_nuclear_explosion_b.world_center.y +
                               lateral_dist(rng) *
                                   NUCLEAR_B_STEM_RADIUS_CELLS * 0.40f};
    puff.velocity_cells_per_sec = {0.0f, 0.0f};
    puff.vertical_offset_cells = vertical_position;
    puff.vertical_velocity_cells_per_sec =
        NUCLEAR_B_STEM_RISE_SPEED_CELLS_PER_SEC * (0.20f + 0.20f * lateral_dist(rng));
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.rotation_radians = 0.0f;
    puff.rotation_speed_rad_per_sec = 0.0f;
    puff.kind = ExplosionSmokePuffKind::NuclearBStemSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::SpawnNuclearBCapSmoke(float cap_progress, Uint32 now) {
  if (!active_nuclear_explosion_b.is_active) {
    return;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> angle_dist(
      0.0f, static_cast<float>(2.0 * M_PI));
  std::uniform_real_distribution<float> jitter_dist(-0.18f, 0.18f);
  std::uniform_real_distribution<float> height_jitter_dist(-0.30f, 0.30f);
  std::uniform_real_distribution<float> curl_speed_jitter_dist(0.85f, 1.18f);
  std::uniform_int_distribution<Uint32> seed_dist(1, 0xFFFFFFFFu);

  const float progress = std::clamp(cap_progress, 0.0f, 1.0f);
  const float cap_top_height =
      0.8f + progress * NUCLEAR_B_STEM_HEIGHT_CELLS +
      NUCLEAR_B_CAP_HEIGHT_OFFSET_CELLS;
  const float lifetime_s =
      static_cast<float>(NUCLEAR_B_CAP_SMOKE_LIFETIME_MS) / 1000.0f;
  // curl_speed * lifetime = pi -> Puff durchläuft eine Halb-Torusbahn.
  const float curl_speed_base =
      static_cast<float>(M_PI) / std::max(0.1f, lifetime_s);
  for (int index = 0; index < NUCLEAR_B_CAP_PUFFS_PER_SPAWN; ++index) {
    const float angle = angle_dist(rng);
    ExplosionSmokePuff puff;
    puff.world_position = {active_nuclear_explosion_b.world_center.x +
                               jitter_dist(rng),
                           active_nuclear_explosion_b.world_center.y +
                               jitter_dist(rng)};
    puff.velocity_cells_per_sec = {0.0f, 0.0f};
    puff.vertical_offset_cells = cap_top_height + height_jitter_dist(rng);
    puff.vertical_velocity_cells_per_sec = 0.0f;
    puff.spawned_ticks = now;
    puff.shape_seed = seed_dist(rng);
    puff.rotation_radians = angle;
    puff.rotation_speed_rad_per_sec =
        curl_speed_base * curl_speed_jitter_dist(rng);
    puff.kind = ExplosionSmokePuffKind::NuclearBCapSmoke;
    explosion_smoke_puffs.push_back(puff);
  }
}

void Game::UpdateExplosionParticles(Uint32 now) {
  if (explosion_particles_last_update_ticks == 0 ||
      explosion_particles_last_update_ticks > now) {
    explosion_particles_last_update_ticks = now;
  }
  const Uint32 last = explosion_particles_last_update_ticks;
  explosion_particles_last_update_ticks = now;
  const float dt = (now > last)
                       ? std::min(0.1f, static_cast<float>(now - last) / 1000.0f)
                       : 0.0f;

  for (size_t index = 0; index < explosion_particles.size();) {
    ExplosionParticle &particle = explosion_particles[index];
    const ExplosionParticleMotionTuning tuning =
        GetExplosionParticleMotionTuning(particle.kind);
    if (now < particle.spawned_ticks) {
      ++index;
      continue;
    }
    const Uint32 age = now - particle.spawned_ticks;
    if (age >= tuning.lifetime_ms) {
      explosion_particles.erase(explosion_particles.begin() +
                                static_cast<long>(index));
      continue;
    }

    if (dt > 0.0f) {
      particle.velocity_cells_per_sec.y +=
          tuning.gravity_cells_per_sec2 * dt;
      particle.world_position.x += particle.velocity_cells_per_sec.x * dt;
      particle.world_position.y += particle.velocity_cells_per_sec.y * dt;
    }

    if (now - particle.last_smoke_spawn_ticks >= tuning.smoke_spawn_interval_ms) {
      ExplosionSmokePuff puff;
      puff.world_position = particle.world_position;
      puff.spawned_ticks = now;
      puff.shape_seed =
          particle.shape_seed ^ (now * 2654435761u) ^
          static_cast<Uint32>(explosion_smoke_puffs.size() * 374761393u);
      if (puff.shape_seed == 0) {
        puff.shape_seed = 1;
      }
      puff.kind = tuning.smoke_kind;
      explosion_smoke_puffs.push_back(puff);
      particle.last_smoke_spawn_ticks = now;
    }

    ++index;
  }

  explosion_smoke_puffs.erase(
      std::remove_if(explosion_smoke_puffs.begin(),
                     explosion_smoke_puffs.end(),
                     [now](const ExplosionSmokePuff &puff) {
                       if (now < puff.spawned_ticks) {
                         return false;
                       }
                       return now - puff.spawned_ticks >=
                              GetSmokePuffLifetimeMs(puff.kind);
                     }),
      explosion_smoke_puffs.end());
}
