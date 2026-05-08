/*
 * PROJECT COMMENT (game.h)
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

#ifndef GAME_H
#define GAME_H

#include <SDL.h>

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "audio.h"
#include "events.h"
#include "goodie.h"
#include "map.h"
#include "mapelement.h"
#include "monster.h"
#include "pacman.h"
#include "renderer.h"

class Map;
class Events;
class Pacman;
class Monster;
class Goodie;
class Renderer;

struct Fireball {
  // Current segment position of the projectile.
  MapCoord current_coord;
  Directions direction;
  Uint32 segment_started_ticks;
  // Monster ID so self-hits can be ignored.
  int owner_id;
  Uint32 step_duration_ms;
};

struct Slimeball {
  MapCoord current_coord;
  Directions direction;
  Uint32 segment_started_ticks;
  int owner_id;
  Uint32 step_duration_ms;
};

struct GasCloud {
  MapCoord coord;
  Uint32 started_ticks;
  Uint32 fade_started_ticks;
  int animation_seed;
  int owner_id;
  bool is_fading;
  // Set once Pacman has walked into the cloud.
  bool triggered_by_pacman;
};

struct InvulnerabilityPotion {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct DynamitePickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct PlasticExplosivePickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct WalkieTalkiePickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct RocketPickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct BiohazardPickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct NuclearBombPickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct LovePotionPickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct LifePickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct DiscoPickup {
  MapCoord coord{0, 0};
  Uint32 appeared_ticks = 0;
  Uint32 fade_started_ticks = 0;
  Uint32 next_spawn_ticks = 0;
  int animation_seed = 0;
  bool is_visible = false;
  bool is_fading = false;
};

struct ActiveDiscoEasterEgg {
  Uint32 started_ticks = 0;
  Uint32 ending_started_ticks = 0;
  Uint32 frozen_started_ticks = 0;
  Uint32 last_rotation_update_ticks = 0;
  double rotation_phase_turns = 0.0;
  double rotation_speed_scale = DISCO_ROTATION_SPEED_MIN;
  int animation_seed = 0;
  bool is_active = false;
  bool is_ending = false;
  bool music_fade_started = false;
};

struct NuclearBombTargetMarker {
  bool is_marked = false;
  MapCoord coord{0, 0};
  Uint32 marked_ticks = 0;
  int animation_seed = 0;
};

struct ActiveNuclearBombDrop {
  bool is_active = false;
  MapCoord target_coord{0, 0};
  SDL_FPoint target_world_center{0.0f, 0.0f};
  Uint32 alarm_started_ticks = 0;
  Uint32 drop_started_ticks = 0;
  Uint32 impact_ticks = 0;
  int animation_seed = 0;
  bool drop_sound_started = false;
};

struct PlacedDynamite {
  MapCoord coord{0, 0};
  Uint32 placed_ticks = 0;
  Uint32 explode_at_ticks = 0;
  int animation_seed = 0;
};

struct PlacedPlasticExplosive {
  MapCoord coord{0, 0};
  Uint32 placed_ticks = 0;
  int animation_seed = 0;
  bool embedded_in_wall = false;
};

struct ScheduledMonsterBlast {
  Monster *monster = nullptr;
  MapCoord coord{0, 0};
  Uint32 trigger_ticks = 0;
};

struct AirstrikeBomb {
  MapCoord coord{0, 0};
  SDL_FPoint release_position{0.0f, 0.0f};
  double path_progress = 0.0;
  Uint32 release_ticks = 0;
  Uint32 explode_ticks = 0;
  bool exploded = false;
  int animation_seed = 0;
};

struct ActiveAirstrike {
  bool is_active = false;
  Directions flight_direction = Directions::Right;
  MapCoord target_coord{0, 0};
  Uint32 started_ticks = 0;
  Uint32 plane_launch_ticks = 0;
  Uint32 overflight_ticks = 0;
  Uint32 plane_finished_ticks = 0;
  Uint32 finished_ticks = 0;
  int animation_seed = 0;
  double target_progress = 0.5;
  bool plane_sound_started = false;
  bool plane_sound_fadeout_started = false;
  SDL_FPoint flight_start{0.0f, 0.0f};
  SDL_FPoint flight_end{0.0f, 0.0f};
  std::vector<AirstrikeBomb> bombs;
};

struct RocketProjectile {
  MapCoord current_coord{0, 0};
  Directions direction = Directions::Right;
  Uint32 segment_started_ticks = 0;
  Uint32 step_duration_ms = 0;
  Uint32 last_smoke_spawn_ticks = 0;
  int animation_seed = 0;
};

struct LoveSmokeProjectile {
  MapCoord origin_coord{0, 0};
  Directions direction = Directions::None;
  Uint32 spawned_ticks = 0;
  Uint32 step_duration_ms = 0;
  int segments_emitted = 0;
  int max_segments = 0;
  bool finished = false;
  int animation_seed = 0;
};

struct ActiveBiohazardBeam {
  bool is_active = false;
  Directions direction = Directions::Right;
  Uint32 started_ticks = 0;
  Uint32 minimum_visible_until_ticks = 0;
  Uint32 hit_sequence_started_ticks = 0;
  Uint32 locked_until_ticks = 0;
  int animation_seed = 0;
  bool is_locked_after_hit = false;
  MapCoord locked_origin_coord{0, 0};
  PixelCoord locked_origin_delta{0, 0};
  SDL_FPoint locked_end_world_center{0.0f, 0.0f};
};

struct AlienAgent {
  MapCoord coord{0, 0};
  Uint32 spawned_ticks = 0;
  Uint32 animation_started_ticks = 0;
  Uint32 animation_until_ticks = 0;
  Uint32 next_animation_ticks = 0;
  Uint32 thought_started_ticks = 0;
  Uint32 thought_visible_until_ticks = 0;
  Uint32 next_thought_ticks = 0;
  Uint32 scream_trigger_ticks = 0;
  Uint32 explosion_trigger_ticks = 0;
  int animation_seed = 0;
  int thought_index = -1;
  bool is_alive = true;
  bool scream_played = false;
  bool explosion_spawned = false;
};

struct ActiveAlienLaser {
  bool is_active = false;
  Directions direction = Directions::Right;
  Uint32 started_ticks = 0;
  Uint32 visible_until_ticks = 0;
  int animation_seed = 0;
  MapCoord origin_coord{0, 0};
  PixelCoord origin_delta{0, 0};
  SDL_FPoint end_world_center{0.0f, 0.0f};
};

struct ActiveNuclearExplosion {
  bool is_active = false;
  bool crater_created = false;
  Uint32 started_ticks = 0;
  Uint32 last_ring_spawn_ticks = 0;
  Uint32 last_center_smoke_spawn_ticks = 0;
  Uint32 last_mushroom_smoke_spawn_ticks = 0;
  int animation_seed = 0;
  MapCoord center_coord{0, 0};
  SDL_FPoint world_center{0.0f, 0.0f};
};

struct NuclearCrater {
  SDL_FPoint world_center{0.0f, 0.0f};
  float radius_cells = 0.0f;
  int shape_seed = 0;
  Uint32 visible_ticks = 0;
  std::vector<MapCoord> cells;
};

struct ActiveNuclearExplosionB {
  bool is_active = false;
  Uint32 started_ticks = 0;
  Uint32 last_ring_spawn_ticks = 0;
  Uint32 last_trail_spawn_ticks = 0;
  Uint32 last_stem_spawn_ticks = 0;
  Uint32 last_cap_spawn_ticks = 0;
  int animation_seed = 0;
  MapCoord center_coord{0, 0};
  SDL_FPoint world_center{0.0f, 0.0f};
};

enum class ExplosionParticleKind {
  MonsterExplosion,
  WallDebris,
  NuclearDebris
};

struct ExplosionParticle {
  SDL_FPoint world_position{0.0f, 0.0f};
  SDL_FPoint velocity_cells_per_sec{0.0f, 0.0f};
  Uint32 spawned_ticks = 0;
  Uint32 last_smoke_spawn_ticks = 0;
  Uint32 shape_seed = 0;
  float radius_cells = 0.0f;
  ExplosionParticleKind kind = ExplosionParticleKind::MonsterExplosion;
};

enum class ExplosionSmokePuffKind {
  MonsterSmoke,
  WallDust,
  NuclearDust,
  BlastSmoke,
  RocketBlastSmoke,
  RocketTrailSmoke,
  NuclearRingSmoke,
  NuclearCoreSmoke,
  NuclearMushroomSmoke,
  NuclearBRingSmoke,
  NuclearBTrailSmoke,
  NuclearBStemSmoke,
  NuclearBCapSmoke,
  GoatRedDust,
  MonsterDustCloud,
  LoveSmokeTrail,
  LoveSmokeImpact
};

struct ExplosionSmokePuff {
  SDL_FPoint world_position{0.0f, 0.0f};
  SDL_FPoint velocity_cells_per_sec{0.0f, 0.0f};
  float vertical_offset_cells = 0.0f;
  float vertical_velocity_cells_per_sec = 0.0f;
  float rotation_radians = 0.0f;
  float rotation_speed_rad_per_sec = 0.0f;
  Uint32 spawned_ticks = 0;
  Uint32 shape_seed = 0;
  ExplosionSmokePuffKind kind = ExplosionSmokePuffKind::MonsterSmoke;
};

struct WallRubblePiece {
  SDL_FPoint world_position{0.0f, 0.0f};
  float half_width_cells = 0.0f;
  float half_height_cells = 0.0f;
  float rotation_radians = 0.0f;
  Uint32 shape_seed = 0;
};

enum class EffectType {
  MonsterExplosion,
  WallImpact,
  BiohazardImpactFlash,
  DynamiteExplosion,
  AirstrikeExplosion,
  SlimeSplash,
  PlasmaShockwave,
  NuclearExplosion,
  NuclearExplosionB,
  AlienExplosion
};

struct GameEffect {
  MapCoord coord{0, 0};
  Uint32 started_ticks = 0;
  EffectType type = EffectType::WallImpact;
  int radius_cells = 0;
  bool has_precise_world_center = false;
  SDL_FPoint precise_world_center{0.0f, 0.0f};
};

class Game {
public:
  Game(Map *, Events *, Audio *, Difficulty, int starting_lives);
  ~Game();
  bool is_won();
  bool is_lost();
  bool IsFinalLossSequenceActive(Uint32 now) const;
  Uint32 GetDeathStartedTicks() const;
  void StartSimulation();
  void PauseSimulation();
  void ResumeSimulation(Uint32 paused_duration_ms);
  bool IsDiscoEasterEggActive() const;
  void RequestDiscoEasterEggEnd(Uint32 now);
  void AdjustDiscoRotationSpeed(double delta);
  void Update();

protected:
private:
  Map *map;
  Events *events;
  Pacman *pacman;
  Audio *audio;
  Difficulty difficulty;
  std::vector<Monster *> monsters;
  std::vector<Goodie *> goodies;
  std::vector<std::thread> monster_threads;
  std::thread pacman_thread;
  int starting_lives;
  int remaining_lives;
  int score;
  bool simulation_started;
  Uint32 death_started_ticks;
  Uint32 life_recovery_until_ticks;
  Uint32 final_loss_commits_at_ticks;
  MapCoord death_coord;
  std::vector<Fireball> fireballs;
  std::vector<Slimeball> slimeballs;
  std::vector<GasCloud> gas_clouds;
  InvulnerabilityPotion invulnerability_potion;
  DynamitePickup dynamite_pickup;
  PlasticExplosivePickup plastic_explosive_pickup;
  WalkieTalkiePickup walkie_talkie_pickup;
  RocketPickup rocket_pickup;
  BiohazardPickup biohazard_pickup;
  NuclearBombPickup nuclear_bomb_pickup;
  LovePotionPickup love_potion_pickup;
  LifePickup life_pickup;
  DiscoPickup disco_pickup;
  ActiveDiscoEasterEgg active_disco_easteregg;
  NuclearBombTargetMarker nuclear_bomb_target_marker;
  ActiveNuclearBombDrop active_nuclear_bomb_drop;
  std::vector<PlacedDynamite> placed_dynamites;
  PlacedPlasticExplosive placed_plastic_explosive;
  bool plastic_explosive_is_armed;
  ActiveAirstrike active_airstrike;
  std::vector<RocketProjectile> active_rockets;
  std::vector<LoveSmokeProjectile> active_love_smokes;
  ActiveBiohazardBeam active_biohazard_beam;
  ActiveAlienLaser active_alien_laser;
  std::vector<AlienAgent> aliens;
  ActiveNuclearExplosion active_nuclear_explosion;
  ActiveNuclearExplosionB active_nuclear_explosion_b;
  std::vector<NuclearCrater> nuclear_craters;
  std::vector<ScheduledMonsterBlast> scheduled_monster_blasts;
  std::vector<GameEffect> effects;
  std::vector<ExplosionParticle> explosion_particles;
  std::vector<ExplosionSmokePuff> explosion_smoke_puffs;
  std::vector<WallRubblePiece> wall_rubble_pieces;
  Uint32 explosion_particles_last_update_ticks = 0;
  Uint32 last_update_ticks;
  int dynamite_inventory;
  int plastic_explosive_inventory;
  int airstrike_inventory;
  int rocket_inventory;
  int biohazard_inventory;
  int nuclear_bomb_inventory;
  int love_potion_inventory;
  bool nuclear_bomb_collected_once;
  friend class Renderer;
  bool dead;
  bool win;

  void TriggerLoss(MapCoord coord, Uint32 now);
  void ApplyTeleporters();
  void TryTeleportElement(MapElement *element);
  void TryShootFireballs(Uint32 now);
  void TryShootSlimeballs(Uint32 now);
  void TrySpawnGasClouds(Uint32 now);
  void ScheduleNextInvulnerabilityPotionSpawn(Uint32 now);
  void TrySpawnInvulnerabilityPotion(Uint32 now);
  void UpdateInvulnerabilityPotion(Uint32 now);
  void UpdateInvulnerability(Uint32 now);
  void UpdateFinalLossSequence(Uint32 now);
  void ActivateInvulnerability(Uint32 now);
  void ActivateLifeRecovery(Uint32 now);
  void ScheduleNextDynamiteSpawn(Uint32 now);
  void TrySpawnDynamite(Uint32 now);
  void UpdateDynamitePickup(Uint32 now);
  void TryPlaceDynamite(Uint32 now);
  void ScheduleNextPlasticExplosiveSpawn(Uint32 now);
  void TrySpawnPlasticExplosive(Uint32 now);
  void UpdatePlasticExplosivePickup(Uint32 now);
  void TryUsePlasticExplosive(Uint32 now);
  void ScheduleNextWalkieTalkieSpawn(Uint32 now);
  void TrySpawnWalkieTalkie(Uint32 now);
  void UpdateWalkieTalkiePickup(Uint32 now);
  void ScheduleNextRocketSpawn(Uint32 now);
  void TrySpawnRocket(Uint32 now);
  void UpdateRocketPickup(Uint32 now);
  void ScheduleNextBiohazardSpawn(Uint32 now);
  void TrySpawnBiohazard(Uint32 now);
  void UpdateBiohazardPickup(Uint32 now);
  void ScheduleNextNuclearBombSpawn(Uint32 now);
  void TrySpawnNuclearBomb(Uint32 now);
  void UpdateNuclearBombPickup(Uint32 now);
  void ScheduleNextLovePotionSpawn(Uint32 now);
  void TrySpawnLovePotion(Uint32 now);
  void UpdateLovePotionPickup(Uint32 now);
  void ScheduleNextLifePickupSpawn(Uint32 now);
  void TrySpawnLifePickup(Uint32 now);
  void UpdateLifePickup(Uint32 now);
  void ScheduleNextDiscoPickupSpawn(Uint32 now);
  void TrySpawnDiscoPickup(Uint32 now);
  void UpdateDiscoPickup(Uint32 now);
  void StartDiscoEasterEgg(Uint32 now);
  void UpdateDiscoEasterEgg(Uint32 now);
  void UpdateDiscoRotationPhase(Uint32 now);
  void TryUseAirstrike(Uint32 now);
  void TryFireRocket(Uint32 now);
  void TryUseBiohazardBeam(Uint32 now);
  void TrySpawnAlien(Uint32 now);
  void TryFireAlienLaser(Uint32 now);
  void TryUseNuclearBomb(Uint32 now);
  void TryUseLovePotion(Uint32 now);
  void UpdateLoveSmokeProjectiles(Uint32 now);
  void ApplyLovePotionToGoat(Monster *goat, Uint32 now);
  void SpawnLoveSmokeTrailPuff(SDL_FPoint world_center, Directions direction,
                               Uint32 now);
  void SpawnLoveSmokeImpactPuff(SDL_FPoint world_center, Uint32 now);
  void ApplyCheats();
  void UpdateNuclearBombDrop(Uint32 now);
  void TryTriggerNuclearExplosion(Uint32 now);
  void TryTriggerNuclearExplosionB(Uint32 now);
  void UpdateBiohazardBeam(Uint32 now);
  void UpdateAlienLaser(Uint32 now);
  void UpdateAlienExplosions(Uint32 now);
  void UpdateNuclearExplosion(Uint32 now);
  void UpdateNuclearExplosionB(Uint32 now);
  void UpdateNuclearCraterClouds(Uint32 now);
  void StopBiohazardBeam();
  void UpdateAirstrike(Uint32 now);
  void UpdateRockets(Uint32 now);
  void UpdateElectrifiedMonsters(Uint32 now);
  void UpdatePlacedDynamites(Uint32 now);
  void DetonateDynamite(const PlacedDynamite &dynamite, Uint32 now);
  void DetonatePlasticExplosive(const PlacedPlasticExplosive &explosive,
                                Uint32 now);
  void DetonateAirstrikeBomb(const AirstrikeBomb &bomb, Uint32 now);
  void DetonateRocket(const RocketProjectile &rocket, MapCoord impact_coord,
                      Monster *hit_monster, bool hit_wall, Uint32 now);
  void UpdateScheduledMonsterBlasts(Uint32 now);
  void ScheduleMonsterBlast(Monster *monster, Uint32 trigger_ticks);
  bool IsCellFreeForDynamitePickup(MapCoord coord) const;
  bool IsCellFreeForPlasticExplosivePickup(MapCoord coord) const;
  bool IsCellFreeForWalkieTalkiePickup(MapCoord coord) const;
  bool IsCellFreeForRocketPickup(MapCoord coord) const;
  bool IsCellFreeForBiohazardPickup(MapCoord coord) const;
  bool IsCellFreeForNuclearBombPickup(MapCoord coord) const;
  bool IsCellFreeForLovePotionPickup(MapCoord coord) const;
  bool IsCellFreeForLifePickup(MapCoord coord) const;
  bool IsCellFreeForDiscoPickup(MapCoord coord) const;
  bool IsCellFreeForAlienSpawn(MapCoord coord) const;
  bool IsCellOccupiedByAlien(MapCoord coord) const;
  bool CanPlacePlasticExplosiveAt(MapCoord coord) const;
  bool IsCraterCell(MapCoord coord) const;
  bool IsWithinRadius(MapCoord center, MapCoord target, int radius_cells) const;
  bool IsWithinDynamiteRadius(MapCoord center, MapCoord target) const;
  bool IsCellFreeForInvulnerabilityPotion(MapCoord coord) const;
  bool IsPacmanInvulnerable(Uint32 now) const;
  bool IsPacmanRecoveringFromLifeLoss(Uint32 now) const;
  Monster *FindMonsterById(int monster_id) const;
  AlienAgent *FindAlienInLineOfSight(Directions direction);
  void ElectrifyMonster(Monster *monster, Uint32 now);
  void UpdateFireballs(Uint32 now);
  void UpdateSlimeballs(Uint32 now);
  void UpdateGasClouds(Uint32 now);
  void ActivateSlimeCover(Uint32 now);
  void CleanupEffects(Uint32 now);
  void ShiftPausedTimers(Uint32 paused_duration_ms);
  void EliminateAlien(AlienAgent *alien, Uint32 sequence_start_ticks);
  void EliminateMonsterWithDynamiteBlast(Monster *monster, Uint32 now);
  void EliminateMonster(Monster *monster, Uint32 now);
  void EliminateMonsterWithDustCloud(Monster *monster, Uint32 now);
  void SpawnMonsterExplosionParticles(SDL_FPoint world_center, Uint32 now);
  void SpawnMonsterDustCloud(SDL_FPoint world_center, Uint32 now);
  void SpawnWallDestructionParticles(SDL_FPoint world_center, Uint32 now);
  void SpawnNuclearBombFragments(SDL_FPoint world_center, Uint32 now);
  void SpawnWallRubble(MapCoord destroyed_coord);
  void HandleGoatRequests(Uint32 now);
  Monster *FindClosestMonsterInGoatSight(Monster *goat,
                                         Directions &direction_out) const;
  void SpawnGoatRedDustCloud(SDL_FPoint world_center, Uint32 now);
  void SpawnExplosionSmokeCloud(SDL_FPoint world_center, float radius_cells,
                                Uint32 now, float density_multiplier = 1.0f);
  void SpawnRocketBlastSmokeCloud(SDL_FPoint world_center, Uint32 now);
  void SpawnRocketTrailSmoke(const RocketProjectile &rocket, Uint32 now);
  void SpawnNuclearRingSmoke(float ring_progress, Uint32 now);
  void SpawnNuclearCenterSmoke(float center_smoke_progress, Uint32 now);
  void SpawnNuclearMushroomSmoke(float mushroom_progress, Uint32 now);
  void TriggerNuclearExplosionAt(SDL_FPoint world_center, MapCoord center_coord,
                                 Uint32 now);
  void CreateNuclearCrater(const ActiveNuclearExplosion &explosion, Uint32 now,
                           Uint32 visible_ticks);
  bool NuclearCraterTouchesCell(const NuclearCrater &crater,
                                MapCoord coord) const;
  void RemoveUnreachableGoodiesAfterCrater();
  void SpawnNuclearBRingSmoke(float ring_progress, Uint32 now);
  void SpawnNuclearBTrailSmoke(float ring_progress, Uint32 now);
  void SpawnNuclearBStemSmoke(float stem_progress, Uint32 now);
  void SpawnNuclearBCapSmoke(float cap_progress, Uint32 now);
  void UpdateExplosionParticles(Uint32 now);
  SDL_FPoint PreciseWorldCenter(const MapElement *element) const;
  SDL_FPoint AlienWorldCenter(const AlienAgent &alien) const;
  SDL_FPoint FireballWorldCenter(const Fireball &fireball, Uint32 now) const;
  SDL_FPoint SlimeballWorldCenter(const Slimeball &slimeball, Uint32 now) const;
  SDL_FPoint RocketWorldCenter(const RocketProjectile &rocket,
                               Uint32 now) const;
  static bool CirclesOverlap(SDL_FPoint a, float radius_a, SDL_FPoint b,
                             float radius_b);
  static bool SegmentHitsCircle(SDL_FPoint segment_start, SDL_FPoint segment_end,
                                SDL_FPoint circle_center, float circle_radius);
};

#endif
