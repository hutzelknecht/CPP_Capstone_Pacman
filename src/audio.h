/*
 * PROJECT COMMENT (audio.h)
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

#ifndef AUDIO_H
#define AUDIO_H

#include "definitions.h"
#include <SDL.h>
#include <array>
#ifdef AUDIO
#include <SDL_mixer.h>
#endif
#include <iostream>
#include <vector>

class Audio {

public:
  static constexpr int kMenuSpectrumBandCount = 32;
  Audio();
  ~Audio();
  static std::array<float, kMenuSpectrumBandCount> GetMenuSpectrumLevels();
#ifdef AUDIO
  void PlayCoin();
  void PlayLifeLost();
  void PlayGameOver();
  void StartLoseMusic();
  void PlayWin();
  void PlayMenuMove();
  void PlayMenuSelect();
  void PlayCountdownTick();
  void PlayStartTrumpet();
  bool StartMenuMusic();
  bool FadeOutMenuMusic(int fade_out_ms);
  void StopMenuMusic();
  bool StartDiscoMusic();
  bool FadeOutDiscoMusic(int fade_out_ms);
  void StopDiscoMusic();
  void StopEndScreenMusic();
  bool IsMenuMusicPlaying() const;
  void PlayMonsterShot();
  void PlayFireballWallHit();
  void PlaySlimeShot();
  void PlaySlimeImpact();
  void PlayMonsterExplosion();
  void PlayMonsterFart();
  void PlayPacmanGag();
  void PlayTeleporterZap();
  void PlayTeleporterArc();
  void PlayEditorBlocked();
  void PlayPotionSpawn();
  void PlayLovePotion();
  void PlayDynamiteSpawn();
  void PlayDynamiteIgnite();
  void PlayDynamiteExplosion();
  void PlayPlasticExplosiveSpawn();
  void PlayPlasticExplosivePlace();
  void PlayPlasticExplosiveWallBreak();
  void PlayAirstrikeRadio();
  void PlayAirstrikeExplosion();
  void StartAirstrikePlane();
  void FadeOutAirstrikePlane(int fade_out_ms);
  void PlayRocketLaunch();
  void StopRocketLaunch();
  void StartBiohazardBeam();
  void StopBiohazardBeam();
  void PlayElectrifiedMonsterRoar();
  void PlayElectrifiedMonsterImpact();
  void PlayNuclearBombAlarm();
  void PlayNuclearBombDrop();
  void PlayNuclearBombExplosion();
  void PlayAlienLaser();
  void PlayAlienScream();
  void PlayAlienExplosion();
  void PlayPunch();
  void PlayGoatBleat();
  void PlayRubbleCrash();
  Uint32 GetAirstrikeRadioDurationMs() const;
  void StartInvulnerabilityLoop();
  void StopInvulnerabilityLoop();

protected:
private:
  Mix_Chunk *CreateSynthChunk(const std::vector<int> &frequencies,
                              int duration_ms, double volume, double attack_ms,
                              double release_ms);
  Mix_Chunk *CreateTrumpetChunk();
  Mix_Chunk *CreateViolinLamentChunk();
  Mix_Chunk *CreateMonsterExplosionChunk();
  Mix_Chunk *CreateMonsterFartChunk();
  Mix_Chunk *CreatePacmanGagChunk();
  Mix_Chunk *CreateTeleporterZapChunk();
  Mix_Chunk *CreateTeleporterArcChunk();
  Mix_Chunk *CreateEditorBlockedChunk();
  Mix_Chunk *CreatePotionSpawnChunk();
  Mix_Chunk *CreateLovePotionChunk();
  Mix_Chunk *CreateDynamiteSpawnChunk();
  Mix_Chunk *CreateDynamiteIgniteChunk();
  Mix_Chunk *CreateDynamiteExplosionChunk();
  Mix_Chunk *CreatePlasticExplosiveReadyChunk();
  Mix_Chunk *CreatePlasticExplosiveWallBreakChunk();
  Mix_Chunk *CreateBiohazardBeamChunk();
  Mix_Chunk *CreateInvulnerabilityLoopChunk();
  void AmplifyChunk(Mix_Chunk *chunk, double gain);
  void PlayChunk(Mix_Chunk *);

  Mix_Chunk *SFX_coin;
  Mix_Chunk *SFX_win;
  Mix_Chunk *SFX_life_lost;
  Mix_Chunk *SFX_gameover;
  Mix_Chunk *SFX_menu_move;
  Mix_Chunk *SFX_menu_select;
  Mix_Chunk *SFX_countdown_tick;
  Mix_Chunk *SFX_start_trumpet;
  Mix_Chunk *SFX_monster_shot;
  Mix_Chunk *SFX_fireball_wall_hit;
  Mix_Chunk *SFX_slime_shot;
  Mix_Chunk *SFX_slime_impact;
  Mix_Chunk *SFX_monster_explosion;
  Mix_Chunk *SFX_monster_fart;
  Mix_Chunk *SFX_pacman_gag;
  Mix_Chunk *SFX_teleporter_zap;
  Mix_Chunk *SFX_teleporter_arc;
  Mix_Chunk *SFX_editor_blocked;
  Mix_Chunk *SFX_potion_spawn;
  Mix_Chunk *SFX_love_potion;
  Mix_Chunk *SFX_dynamite_spawn;
  Mix_Chunk *SFX_dynamite_ignite;
  Mix_Chunk *SFX_dynamite_explosion;
  Mix_Chunk *SFX_plastic_explosive_ready;
  Mix_Chunk *SFX_plastic_explosive_wall_break;
  Mix_Chunk *SFX_airstrike_radio;
  Mix_Chunk *SFX_airstrike_explosion;
  Mix_Chunk *SFX_airstrike_propeller;
  Mix_Chunk *SFX_rocket_launch;
  Mix_Chunk *SFX_biohazard_beam;
  Mix_Chunk *SFX_electrified_monster_roar;
  Mix_Chunk *SFX_electrified_monster_impact;
  Mix_Chunk *SFX_nuclear_bomb_alarm;
  Mix_Chunk *SFX_nuclear_bomb_drop;
  Mix_Chunk *SFX_nuclear_bomb_explosion;
  Mix_Chunk *SFX_alien_laser;
  Mix_Chunk *SFX_alien_scream;
  Mix_Chunk *SFX_alien_explosion;
  Mix_Chunk *SFX_invulnerability_loop;
  Mix_Chunk *SFX_punch;
  Mix_Chunk *SFX_goat_bleat;
  Mix_Chunk *SFX_rubble_crash;
  Mix_Music *menu_music;
  Mix_Music *disco_music;
  Mix_Music *win_music;
  Mix_Music *lose_music;
  int rocket_launch_channel;
  int airstrike_plane_channel;
  int biohazard_beam_channel;
  int invulnerability_loop_channel;
  bool menu_music_active;
  bool audio_ready;
#endif
};

#endif
