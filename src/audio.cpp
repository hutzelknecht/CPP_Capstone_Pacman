/*
 * PROJECT COMMENT (audio.cpp)
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

#include "audio.h"
#include "paths.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr double kPi = 3.14159265358979323846;
constexpr double kMenuSpectrumMinFrequency = 40.0;
constexpr double kMenuSpectrumMaxFrequency = 12000.0;
constexpr double kMenuSpectrumMinDb = -52.0;
constexpr double kMenuSpectrumMaxDb = -8.0;
constexpr int kMenuSpectrumMaxFrames = 4096;

std::array<std::atomic<Uint8>, Audio::kMenuSpectrumBandCount>
    g_menu_spectrum_levels{};
std::array<float, Audio::kMenuSpectrumBandCount> g_menu_spectrum_smoothed{};

const std::array<double, Audio::kMenuSpectrumBandCount>
    g_menu_spectrum_frequencies = [] {
      std::array<double, Audio::kMenuSpectrumBandCount> frequencies{};
      const double ratio =
          std::pow(kMenuSpectrumMaxFrequency / kMenuSpectrumMinFrequency,
                   1.0 / std::max(1, Audio::kMenuSpectrumBandCount - 1));
      double current_frequency = kMenuSpectrumMinFrequency;
      for (int index = 0; index < Audio::kMenuSpectrumBandCount; ++index) {
        frequencies[static_cast<size_t>(index)] = current_frequency;
        current_frequency *= ratio;
      }
      return frequencies;
    }();

void ClearMenuSpectrumLevels() {
  std::fill(g_menu_spectrum_smoothed.begin(), g_menu_spectrum_smoothed.end(),
            0.0f);
  for (auto &level : g_menu_spectrum_levels) {
    level.store(0, std::memory_order_relaxed);
  }
}

void UpdateMenuSpectrumLevels(const Uint8 *stream, int len) {
  if (stream == nullptr || len <= 0) {
    ClearMenuSpectrumLevels();
    return;
  }

  const Sint16 *samples = reinterpret_cast<const Sint16 *>(stream);
  const int sample_count = len / static_cast<int>(sizeof(Sint16));
  const int frame_count =
      std::clamp(sample_count / kChannels, 0, kMenuSpectrumMaxFrames);
  if (frame_count <= 0) {
    ClearMenuSpectrumLevels();
    return;
  }

  std::array<double, kMenuSpectrumMaxFrames> mono_samples{};
  double rms = 0.0;
  for (int frame = 0; frame < frame_count; ++frame) {
    const int sample_index = frame * kChannels;
    const double left = static_cast<double>(samples[sample_index]) / 32768.0;
    const double right =
        static_cast<double>(samples[sample_index + 1]) / 32768.0;
    const double mono_sample = (left + right) * 0.5;
    mono_samples[static_cast<size_t>(frame)] = mono_sample;
    rms += mono_sample * mono_sample;
  }

  rms = std::sqrt(rms / std::max(1, frame_count));
  const double gate =
      std::clamp((20.0 * std::log10(rms + 1.0e-6) + 68.0) / 42.0, 0.0, 1.0);

  for (int band = 0; band < Audio::kMenuSpectrumBandCount; ++band) {
    const double frequency = g_menu_spectrum_frequencies[static_cast<size_t>(band)];
    const double omega = 2.0 * kPi * frequency / static_cast<double>(kSampleRate);
    const double coeff = 2.0 * std::cos(omega);
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;

    for (int frame = 0; frame < frame_count; ++frame) {
      q0 = coeff * q1 - q2 + mono_samples[static_cast<size_t>(frame)];
      q2 = q1;
      q1 = q0;
    }

    const double power = std::max(0.0, q1 * q1 + q2 * q2 - coeff * q1 * q2);
    double magnitude = std::sqrt(power) / std::max(1, frame_count);
    magnitude *= 1.0 + 0.18 * std::log2(frequency / kMenuSpectrumMinFrequency + 1.0);

    const double db = 20.0 * std::log10(magnitude + 1.0e-7);
    double normalized =
        (db - kMenuSpectrumMinDb) / (kMenuSpectrumMaxDb - kMenuSpectrumMinDb);
    normalized = std::clamp(normalized * (0.48 + gate * 0.78), 0.0, 1.0);

    const float previous = g_menu_spectrum_smoothed[static_cast<size_t>(band)];
    const float blend = (normalized > previous) ? 0.44f : 0.16f;
    const float smoothed =
        std::clamp(previous + static_cast<float>(normalized - previous) * blend,
                   0.0f, 1.0f);
    g_menu_spectrum_smoothed[static_cast<size_t>(band)] = smoothed;
    g_menu_spectrum_levels[static_cast<size_t>(band)].store(
        static_cast<Uint8>(std::lround(smoothed * 255.0f)),
        std::memory_order_relaxed);
  }
}

void MenuSpectrumPostMix(void *userdata, Uint8 *stream, int len) {
  (void)userdata;
  UpdateMenuSpectrumLevels(stream, len);
}

void append_le16(std::vector<Uint8> &buffer, Uint16 value) {
  buffer.push_back(static_cast<Uint8>(value & 0xFF));
  buffer.push_back(static_cast<Uint8>((value >> 8) & 0xFF));
}

void append_le32(std::vector<Uint8> &buffer, Uint32 value) {
  buffer.push_back(static_cast<Uint8>(value & 0xFF));
  buffer.push_back(static_cast<Uint8>((value >> 8) & 0xFF));
  buffer.push_back(static_cast<Uint8>((value >> 16) & 0xFF));
  buffer.push_back(static_cast<Uint8>((value >> 24) & 0xFF));
}

std::vector<Uint8> build_wav_buffer(const std::vector<Sint16> &pcm_samples) {
  const Uint32 data_size =
      static_cast<Uint32>(pcm_samples.size() * sizeof(Sint16));
  std::vector<Uint8> wav;
  wav.reserve(44 + data_size);

  wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
  append_le32(wav, 36 + data_size);
  wav.insert(wav.end(), {'W', 'A', 'V', 'E'});
  wav.insert(wav.end(), {'f', 'm', 't', ' '});
  append_le32(wav, 16);
  append_le16(wav, 1);
  append_le16(wav, kChannels);
  append_le32(wav, kSampleRate);
  append_le32(wav, kSampleRate * kChannels * sizeof(Sint16));
  append_le16(wav, kChannels * sizeof(Sint16));
  append_le16(wav, 16);
  wav.insert(wav.end(), {'d', 'a', 't', 'a'});
  append_le32(wav, data_size);

  const Uint8 *sample_bytes =
      reinterpret_cast<const Uint8 *>(pcm_samples.data());
  wav.insert(wav.end(), sample_bytes, sample_bytes + data_size);
  return wav;
}

} // namespace

std::array<float, Audio::kMenuSpectrumBandCount> Audio::GetMenuSpectrumLevels() {
  std::array<float, Audio::kMenuSpectrumBandCount> levels{};
  for (int index = 0; index < kMenuSpectrumBandCount; ++index) {
    levels[static_cast<size_t>(index)] =
        static_cast<float>(g_menu_spectrum_levels[static_cast<size_t>(index)].load(
            std::memory_order_relaxed)) /
        255.0f;
  }
  return levels;
}

/**
 * @brief Construct a new Audio:: Audio object
 * Takes care of all sound effects
 */
Audio::Audio() {
#ifndef AUDIO
  std::cout << "No Audio\n";
#endif
#ifdef AUDIO
  audio_ready = false;
  SFX_coin = nullptr;
  SFX_win = nullptr;
  SFX_life_lost = nullptr;
  SFX_gameover = nullptr;
  SFX_menu_move = nullptr;
  SFX_menu_select = nullptr;
  SFX_countdown_tick = nullptr;
  SFX_start_trumpet = nullptr;
  SFX_monster_shot = nullptr;
  SFX_fireball_wall_hit = nullptr;
  SFX_slime_shot = nullptr;
  SFX_slime_impact = nullptr;
  SFX_monster_explosion = nullptr;
  SFX_monster_fart = nullptr;
  SFX_pacman_gag = nullptr;
  SFX_teleporter_zap = nullptr;
  SFX_teleporter_arc = nullptr;
  SFX_editor_blocked = nullptr;
  SFX_potion_spawn = nullptr;
  SFX_love_potion = nullptr;
  SFX_dynamite_spawn = nullptr;
  SFX_dynamite_ignite = nullptr;
  SFX_dynamite_explosion = nullptr;
  SFX_plastic_explosive_ready = nullptr;
  SFX_plastic_explosive_wall_break = nullptr;
  SFX_airstrike_radio = nullptr;
  SFX_airstrike_explosion = nullptr;
  SFX_airstrike_propeller = nullptr;
  SFX_rocket_launch = nullptr;
  SFX_biohazard_beam = nullptr;
  SFX_electrified_monster_roar = nullptr;
  SFX_electrified_monster_impact = nullptr;
  SFX_nuclear_bomb_alarm = nullptr;
  SFX_nuclear_bomb_drop = nullptr;
  SFX_nuclear_bomb_explosion = nullptr;
  SFX_alien_laser = nullptr;
  SFX_alien_scream = nullptr;
  SFX_alien_explosion = nullptr;
  SFX_invulnerability_loop = nullptr;
  SFX_punch = nullptr;
  SFX_goat_bleat = nullptr;
  SFX_rubble_crash = nullptr;
  menu_music = nullptr;
  disco_music = nullptr;
  win_music = nullptr;
  lose_music = nullptr;
  rocket_launch_channel = 0;
  airstrike_plane_channel = -1;
  biohazard_beam_channel = -1;
  invulnerability_loop_channel = -1;
  menu_music_active = false;

  if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0 &&
      SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL audio could not initialize: " << SDL_GetError() << "\n";
    return;
  }

  if (Mix_OpenAudio(kSampleRate, AUDIO_S16SYS, kChannels, 2048) < 0) {
    std::cerr << "SDL_mixer could not initialize: " << Mix_GetError() << "\n";
    return;
  }

  ClearMenuSpectrumLevels();
  Mix_SetPostMix(MenuSpectrumPostMix, nullptr);
  Mix_AllocateChannels(32);
  Mix_ReserveChannels(1);

  const std::string coin_sound_path = Paths::GetDataFilePath("coin.wav");
  const std::string win_sound_path = Paths::GetDataFilePath("win.wav");
  const std::string life_lost_sound_path = Paths::GetDataFilePath("gameover.wav");
  const std::string monster_shot_sound_path = Paths::GetDataFilePath("schuss.wav");
  const std::string fireball_impact_sound_path =
      Paths::GetDataFilePath("einschlag.wav");
  const std::string slime_shot_sound_path = Paths::GetDataFilePath("slime_shot.mp3");
  const std::string slime_impact_sound_path =
      Paths::GetDataFilePath("slime_impact.mp3");
  const std::string monster_explosion_sound_path =
      Paths::GetDataFilePath("monsterexplosion.wav");
  const std::string monster_fart_sound_path =
      Paths::GetDataFilePath("monster_fart.mp3");
  const std::string dynamite_explosion_sound_path =
      Paths::GetDataFilePath("dynamitexplosion.wav");
  const std::string dynamite_ignite_sound_path =
      Paths::GetDataFilePath("dynamite_ignite.mp3");
  const std::string plastic_explosive_wall_break_sound_path =
      Paths::GetDataFilePath("plastic_explosive_wall_break.mp3");
  const std::string airstrike_radio_sound_path =
      Paths::GetDataFilePath(AIRSTRIKE_RADIO_SOUND_PATH);
  const std::string airstrike_explosion_sound_path =
      Paths::GetDataFilePath(AIRSTRIKE_EXPLOSION_SOUND_PATH);
  const std::string airstrike_propeller_sound_path =
      Paths::GetDataFilePath(AIRSTRIKE_PROPELLER_SOUND_PATH);
  const std::string rocket_launch_sound_path =
      Paths::GetDataFilePath(ROCKET_LAUNCH_SOUND_PATH);
  const std::string biohazard_beam_sound_path =
      Paths::GetDataFilePath(BIOHAZARD_BEAM_SOUND_PATH);
  const std::string electrified_monster_roar_sound_path =
      Paths::GetDataFilePath(ELECTRIFIED_MONSTER_ROAR_SOUND_PATH);
  const std::string electrified_monster_impact_sound_path =
      Paths::GetDataFilePath(ELECTRIFIED_MONSTER_IMPACT_SOUND_PATH);
  const std::string nuclear_bomb_alarm_sound_path =
      Paths::GetDataFilePath(NUCLEAR_BOMB_ALARM_SOUND_PATH);
  const std::string nuclear_bomb_drop_sound_path =
      Paths::GetDataFilePath(NUCLEAR_BOMB_DROP_SOUND_PATH);
  const std::string nuclear_bomb_explosion_sound_path =
      Paths::GetDataFilePath(NUCLEAR_BOMB_EXPLOSION_SOUND_PATH);
  const std::string alien_laser_sound_path =
      Paths::GetDataFilePath(ALIEN_LASER_SOUND_PATH);
  const std::string alien_scream_sound_path =
      Paths::GetDataFilePath(ALIEN_SCREAM_SOUND_PATH);
  const std::string alien_explosion_sound_path =
      Paths::GetDataFilePath(ALIEN_EXPLOSION_SOUND_PATH);
  const std::string menu_music_path = Paths::GetDataFilePath("menu_music.mp3");
  const std::string disco_music_path =
      Paths::GetDataFilePath(DISCO_MUSIC_PATH);
  const std::string win_music_path = Paths::GetDataFilePath("win_melody.mp3");
  const std::string lose_music_path = Paths::GetDataFilePath("lose_melody.mp3");

  SFX_coin = Mix_LoadWAV(coin_sound_path.c_str());
  SFX_win = Mix_LoadWAV(win_sound_path.c_str());
  SFX_life_lost = Mix_LoadWAV(life_lost_sound_path.c_str());
  if (SFX_life_lost == nullptr) {
    SFX_life_lost = CreateSynthChunk({164, 123, 92}, 110, 0.34, 2.0, 70.0);
  }
  SFX_gameover = CreateViolinLamentChunk();
  SFX_menu_move = CreateSynthChunk({1396, 1865}, 70, 0.30, 4.0, 38.0);
  SFX_menu_select = CreateSynthChunk({784, 988, 1175}, 150, 0.42, 6.0, 90.0);
  SFX_countdown_tick = CreateSynthChunk({880, 1320}, 85, 0.25, 2.0, 48.0);
  SFX_start_trumpet = CreateTrumpetChunk();
  SFX_monster_shot = Mix_LoadWAV(monster_shot_sound_path.c_str());
  if (SFX_monster_shot == nullptr) {
    SFX_monster_shot = CreateSynthChunk({698, 932, 1396}, 120, 0.30, 2.0, 56.0);
  }
  SFX_fireball_wall_hit = Mix_LoadWAV(fireball_impact_sound_path.c_str());
  if (SFX_fireball_wall_hit == nullptr) {
    SFX_fireball_wall_hit = CreateSynthChunk({172, 118}, 90, 0.26, 1.0, 44.0);
  }
  SFX_slime_shot = Mix_LoadWAV(slime_shot_sound_path.c_str());
  if (SFX_slime_shot == nullptr) {
    SFX_slime_shot =
        CreateSynthChunk({152, 188, 224}, 110, 0.26, 2.0, 52.0);
  }
  SFX_slime_impact = Mix_LoadWAV(slime_impact_sound_path.c_str());
  if (SFX_slime_impact == nullptr) {
    SFX_slime_impact =
        CreateSynthChunk({128, 92, 66}, 180, 0.30, 1.0, 92.0);
  }
  SFX_monster_explosion = Mix_LoadWAV(monster_explosion_sound_path.c_str());
  if (SFX_monster_explosion == nullptr) {
    SFX_monster_explosion = CreateMonsterExplosionChunk();
  }
  SFX_monster_fart = Mix_LoadWAV(monster_fart_sound_path.c_str());
  if (SFX_monster_fart == nullptr) {
    SFX_monster_fart = CreateMonsterFartChunk();
  }
  SFX_pacman_gag = CreatePacmanGagChunk();
  SFX_teleporter_zap = CreateTeleporterZapChunk();
  SFX_teleporter_arc = CreateTeleporterArcChunk();
  SFX_editor_blocked = CreateEditorBlockedChunk();
  SFX_potion_spawn = CreatePotionSpawnChunk();
  SFX_love_potion = CreateLovePotionChunk();
  SFX_dynamite_spawn = CreateDynamiteSpawnChunk();
  SFX_dynamite_ignite = Mix_LoadWAV(dynamite_ignite_sound_path.c_str());
  if (SFX_dynamite_ignite == nullptr) {
    SFX_dynamite_ignite = CreateDynamiteIgniteChunk();
  }
  SFX_dynamite_explosion = Mix_LoadWAV(dynamite_explosion_sound_path.c_str());
  if (SFX_dynamite_explosion == nullptr) {
    SFX_dynamite_explosion = CreateDynamiteExplosionChunk();
  }
  SFX_plastic_explosive_ready = CreatePlasticExplosiveReadyChunk();
  SFX_plastic_explosive_wall_break =
      Mix_LoadWAV(plastic_explosive_wall_break_sound_path.c_str());
  if (SFX_plastic_explosive_wall_break == nullptr) {
    SFX_plastic_explosive_wall_break = CreatePlasticExplosiveWallBreakChunk();
  }
  SFX_airstrike_radio = Mix_LoadWAV(airstrike_radio_sound_path.c_str());
  if (SFX_airstrike_radio == nullptr) {
    SFX_airstrike_radio =
        CreateSynthChunk({392, 330, 262}, 420, 0.35, 4.0, 110.0);
  }
  SFX_airstrike_explosion = Mix_LoadWAV(airstrike_explosion_sound_path.c_str());
  if (SFX_airstrike_explosion == nullptr) {
    SFX_airstrike_explosion = CreateDynamiteExplosionChunk();
  }
  SFX_airstrike_propeller = Mix_LoadWAV(airstrike_propeller_sound_path.c_str());
  SFX_rocket_launch = Mix_LoadWAV(rocket_launch_sound_path.c_str());
  if (SFX_rocket_launch == nullptr) {
    SFX_rocket_launch =
        CreateSynthChunk({196, 247, 294}, 420, 0.34, 4.0, 90.0);
  }
  SFX_biohazard_beam = Mix_LoadWAV(biohazard_beam_sound_path.c_str());
  if (SFX_biohazard_beam == nullptr) {
    SFX_biohazard_beam = CreateBiohazardBeamChunk();
  }
  SFX_electrified_monster_roar =
      Mix_LoadWAV(electrified_monster_roar_sound_path.c_str());
  if (SFX_electrified_monster_roar == nullptr) {
    SFX_electrified_monster_roar =
        CreateSynthChunk({98, 82, 74, 55}, 760, 0.54, 6.0, 240.0);
  }
  AmplifyChunk(SFX_electrified_monster_roar, ELECTRIFIED_MONSTER_ROAR_GAIN);
  SFX_electrified_monster_impact =
      Mix_LoadWAV(electrified_monster_impact_sound_path.c_str());
  if (SFX_electrified_monster_impact == nullptr) {
    SFX_electrified_monster_impact =
        CreateSynthChunk({148, 222, 296, 444}, 430, 0.64, 2.0, 140.0);
  }
  SFX_nuclear_bomb_alarm = Mix_LoadWAV(nuclear_bomb_alarm_sound_path.c_str());
  if (SFX_nuclear_bomb_alarm == nullptr) {
    SFX_nuclear_bomb_alarm =
        CreateSynthChunk({880, 660, 880, 660}, 900, 0.52, 4.0, 180.0);
  }
  SFX_nuclear_bomb_drop = Mix_LoadWAV(nuclear_bomb_drop_sound_path.c_str());
  if (SFX_nuclear_bomb_drop == nullptr) {
    SFX_nuclear_bomb_drop =
        CreateSynthChunk({220, 174, 132, 98}, 1200, 0.46, 8.0, 240.0);
  }
  SFX_nuclear_bomb_explosion =
      Mix_LoadWAV(nuclear_bomb_explosion_sound_path.c_str());
  if (SFX_nuclear_bomb_explosion == nullptr) {
    SFX_nuclear_bomb_explosion = CreateDynamiteExplosionChunk();
  }
  AmplifyChunk(SFX_nuclear_bomb_explosion, NUCLEAR_BOMB_EXPLOSION_GAIN);
  SFX_alien_laser = Mix_LoadWAV(alien_laser_sound_path.c_str());
  if (SFX_alien_laser == nullptr) {
    SFX_alien_laser = CreateBiohazardBeamChunk();
  }
  AmplifyChunk(SFX_alien_laser, ALIEN_LASER_SOUND_GAIN);
  SFX_alien_scream = Mix_LoadWAV(alien_scream_sound_path.c_str());
  if (SFX_alien_scream == nullptr) {
    SFX_alien_scream = CreateSynthChunk({784, 622, 466, 392}, 520, 0.48, 2.0,
                                        170.0);
  }
  AmplifyChunk(SFX_alien_scream, ALIEN_SCREAM_SOUND_GAIN);
  SFX_alien_explosion = Mix_LoadWAV(alien_explosion_sound_path.c_str());
  if (SFX_alien_explosion == nullptr) {
    SFX_alien_explosion = CreateDynamiteExplosionChunk();
  }
  AmplifyChunk(SFX_alien_explosion, ALIEN_EXPLOSION_SOUND_GAIN);
  SFX_invulnerability_loop = CreateInvulnerabilityLoopChunk();
  const std::string punch_sound_path = Paths::GetDataFilePath("punch.mp3");
  SFX_punch = Mix_LoadWAV(punch_sound_path.c_str());
  if (SFX_punch == nullptr) {
    SFX_punch = CreateSynthChunk({180, 110, 70}, 130, 0.55, 1.0, 70.0);
  }
  const std::string goat_bleat_sound_path =
      Paths::GetDataFilePath("goat_maaa.mp3");
  SFX_goat_bleat = Mix_LoadWAV(goat_bleat_sound_path.c_str());
  if (SFX_goat_bleat == nullptr) {
    SFX_goat_bleat = CreateSynthChunk({440, 392, 349}, 320, 0.36, 4.0, 120.0);
  }
  const std::string rubble_crash_sound_path =
      Paths::GetDataFilePath("rubble_crash.mp3");
  SFX_rubble_crash = Mix_LoadWAV(rubble_crash_sound_path.c_str());
  if (SFX_rubble_crash == nullptr) {
    SFX_rubble_crash = CreateSynthChunk({140, 92, 64}, 420, 0.46, 2.0, 200.0);
  }
  menu_music = Mix_LoadMUS(menu_music_path.c_str());
  if (menu_music == nullptr) {
    std::cerr << "Failed to load menu music " << menu_music_path << ": "
              << Mix_GetError() << "\n";
  }
  disco_music = Mix_LoadMUS(disco_music_path.c_str());
  if (disco_music == nullptr) {
    std::cerr << "Failed to load disco music " << disco_music_path << ": "
              << Mix_GetError() << "\n";
  }
  win_music = Mix_LoadMUS(win_music_path.c_str());
  if (win_music == nullptr) {
    std::cerr << "Failed to load win music " << win_music_path << ": "
              << Mix_GetError() << "\n";
  }
  lose_music = Mix_LoadMUS(lose_music_path.c_str());
  if (lose_music == nullptr) {
    std::cerr << "Failed to load loss music " << lose_music_path << ": "
              << Mix_GetError() << "\n";
  }

  if (SFX_coin == nullptr || SFX_win == nullptr ||
      SFX_life_lost == nullptr || SFX_gameover == nullptr ||
      SFX_menu_move == nullptr || SFX_menu_select == nullptr ||
      SFX_countdown_tick == nullptr || SFX_start_trumpet == nullptr ||
      SFX_monster_shot == nullptr || SFX_fireball_wall_hit == nullptr ||
      SFX_slime_shot == nullptr || SFX_slime_impact == nullptr ||
      SFX_monster_explosion == nullptr || SFX_monster_fart == nullptr ||
      SFX_pacman_gag == nullptr || SFX_teleporter_zap == nullptr ||
      SFX_teleporter_arc == nullptr || SFX_editor_blocked == nullptr ||
      SFX_potion_spawn == nullptr || SFX_dynamite_spawn == nullptr ||
      SFX_dynamite_ignite == nullptr ||
      SFX_dynamite_explosion == nullptr ||
      SFX_plastic_explosive_ready == nullptr ||
      SFX_plastic_explosive_wall_break == nullptr ||
      SFX_airstrike_radio == nullptr ||
      SFX_airstrike_explosion == nullptr ||
      SFX_airstrike_propeller == nullptr ||
      SFX_rocket_launch == nullptr ||
      SFX_biohazard_beam == nullptr ||
      SFX_electrified_monster_roar == nullptr ||
      SFX_electrified_monster_impact == nullptr ||
      SFX_nuclear_bomb_alarm == nullptr ||
      SFX_nuclear_bomb_drop == nullptr ||
      SFX_nuclear_bomb_explosion == nullptr ||
      SFX_alien_laser == nullptr ||
      SFX_alien_scream == nullptr ||
      SFX_alien_explosion == nullptr ||
      SFX_invulnerability_loop == nullptr ||
      SFX_punch == nullptr || SFX_goat_bleat == nullptr ||
      SFX_rubble_crash == nullptr) {
    std::cerr << "Failed to load SFX: " << Mix_GetError() << "\n";
    return;
  }

  audio_ready = true;
#endif
};

/**
 * @brief Destroy the Audio:: Audio object
 *
 */
Audio::~Audio() {
#ifdef AUDIO
  Mix_SetPostMix(nullptr, nullptr);
  ClearMenuSpectrumLevels();
  StopInvulnerabilityLoop();
  StopBiohazardBeam();
  FadeOutAirstrikePlane(0);
  if (audio_ready) {
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
  }

  if (SFX_coin != nullptr) {
    Mix_FreeChunk(SFX_coin);
  }
  if (SFX_win != nullptr) {
    Mix_FreeChunk(SFX_win);
  }
  if (SFX_life_lost != nullptr) {
    Mix_FreeChunk(SFX_life_lost);
  }
  if (SFX_gameover != nullptr) {
    Mix_FreeChunk(SFX_gameover);
  }
  if (SFX_menu_move != nullptr) {
    Mix_FreeChunk(SFX_menu_move);
  }
  if (SFX_menu_select != nullptr) {
    Mix_FreeChunk(SFX_menu_select);
  }
  if (SFX_countdown_tick != nullptr) {
    Mix_FreeChunk(SFX_countdown_tick);
  }
  if (SFX_start_trumpet != nullptr) {
    Mix_FreeChunk(SFX_start_trumpet);
  }
  if (SFX_monster_shot != nullptr) {
    Mix_FreeChunk(SFX_monster_shot);
  }
  if (SFX_fireball_wall_hit != nullptr) {
    Mix_FreeChunk(SFX_fireball_wall_hit);
  }
  if (SFX_slime_shot != nullptr) {
    Mix_FreeChunk(SFX_slime_shot);
  }
  if (SFX_slime_impact != nullptr) {
    Mix_FreeChunk(SFX_slime_impact);
  }
  if (SFX_monster_explosion != nullptr) {
    Mix_FreeChunk(SFX_monster_explosion);
  }
  if (SFX_monster_fart != nullptr) {
    Mix_FreeChunk(SFX_monster_fart);
  }
  if (SFX_pacman_gag != nullptr) {
    Mix_FreeChunk(SFX_pacman_gag);
  }
  if (SFX_teleporter_zap != nullptr) {
    Mix_FreeChunk(SFX_teleporter_zap);
  }
  if (SFX_teleporter_arc != nullptr) {
    Mix_FreeChunk(SFX_teleporter_arc);
  }
  if (SFX_editor_blocked != nullptr) {
    Mix_FreeChunk(SFX_editor_blocked);
  }
  if (SFX_potion_spawn != nullptr) {
    Mix_FreeChunk(SFX_potion_spawn);
  }
  if (SFX_love_potion != nullptr) {
    Mix_FreeChunk(SFX_love_potion);
  }
  if (SFX_dynamite_spawn != nullptr) {
    Mix_FreeChunk(SFX_dynamite_spawn);
  }
  if (SFX_dynamite_ignite != nullptr) {
    Mix_FreeChunk(SFX_dynamite_ignite);
  }
  if (SFX_dynamite_explosion != nullptr) {
    Mix_FreeChunk(SFX_dynamite_explosion);
  }
  if (SFX_plastic_explosive_ready != nullptr) {
    Mix_FreeChunk(SFX_plastic_explosive_ready);
  }
  if (SFX_plastic_explosive_wall_break != nullptr) {
    Mix_FreeChunk(SFX_plastic_explosive_wall_break);
  }
  if (SFX_airstrike_radio != nullptr) {
    Mix_FreeChunk(SFX_airstrike_radio);
  }
  if (SFX_airstrike_explosion != nullptr) {
    Mix_FreeChunk(SFX_airstrike_explosion);
  }
  if (SFX_airstrike_propeller != nullptr) {
    Mix_FreeChunk(SFX_airstrike_propeller);
  }
  if (SFX_rocket_launch != nullptr) {
    Mix_FreeChunk(SFX_rocket_launch);
  }
  if (SFX_biohazard_beam != nullptr) {
    Mix_FreeChunk(SFX_biohazard_beam);
  }
  if (SFX_electrified_monster_roar != nullptr) {
    Mix_FreeChunk(SFX_electrified_monster_roar);
  }
  if (SFX_electrified_monster_impact != nullptr) {
    Mix_FreeChunk(SFX_electrified_monster_impact);
  }
  if (SFX_nuclear_bomb_alarm != nullptr) {
    Mix_FreeChunk(SFX_nuclear_bomb_alarm);
  }
  if (SFX_nuclear_bomb_drop != nullptr) {
    Mix_FreeChunk(SFX_nuclear_bomb_drop);
  }
  if (SFX_nuclear_bomb_explosion != nullptr) {
    Mix_FreeChunk(SFX_nuclear_bomb_explosion);
  }
  if (SFX_alien_laser != nullptr) {
    Mix_FreeChunk(SFX_alien_laser);
  }
  if (SFX_alien_scream != nullptr) {
    Mix_FreeChunk(SFX_alien_scream);
  }
  if (SFX_alien_explosion != nullptr) {
    Mix_FreeChunk(SFX_alien_explosion);
  }
  if (SFX_invulnerability_loop != nullptr) {
    Mix_FreeChunk(SFX_invulnerability_loop);
  }
  if (SFX_punch != nullptr) {
    Mix_FreeChunk(SFX_punch);
  }
  if (SFX_goat_bleat != nullptr) {
    Mix_FreeChunk(SFX_goat_bleat);
  }
  if (SFX_rubble_crash != nullptr) {
    Mix_FreeChunk(SFX_rubble_crash);
  }
  if (menu_music != nullptr) {
    Mix_FreeMusic(menu_music);
  }
  if (disco_music != nullptr) {
    Mix_FreeMusic(disco_music);
  }
  if (win_music != nullptr) {
    Mix_FreeMusic(win_music);
  }
  if (lose_music != nullptr) {
    Mix_FreeMusic(lose_music);
  }

  if (Mix_QuerySpec(nullptr, nullptr, nullptr) != 0) {
    Mix_CloseAudio();
  }

  if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
#endif
};

#ifdef AUDIO

Mix_Chunk *Audio::CreateSynthChunk(const std::vector<int> &frequencies,
                                   int duration_ms, double volume,
                                   double attack_ms, double release_ms) {
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  const int attack_samples =
      std::max(1, static_cast<int>(attack_ms * kSampleRate / 1000.0));
  const int release_samples =
      std::max(1, static_cast<int>(release_ms * kSampleRate / 1000.0));

  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    double envelope = 1.0;

    if (i < attack_samples) {
      envelope = static_cast<double>(i) / attack_samples;
    } else if (i > sample_count - release_samples) {
      envelope =
          static_cast<double>(sample_count - i) / std::max(1, release_samples);
    }

    envelope = std::clamp(envelope, 0.0, 1.0);

    double sample_value = 0.0;
    for (size_t freq_index = 0; freq_index < frequencies.size(); freq_index++) {
      const double frequency = static_cast<double>(frequencies[freq_index]);
      const double harmonic_weight = 1.0 / (freq_index + 1.0);
      sample_value +=
          harmonic_weight * std::sin(2.0 * kPi * frequency * time);
      sample_value += 0.22 * harmonic_weight *
                      std::sin(4.0 * kPi * frequency * time + 0.3);
    }

    sample_value /= std::max<size_t>(1, frequencies.size());
    sample_value *= envelope * volume;
    sample_value = std::clamp(sample_value, -1.0, 1.0);

    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateTrumpetChunk() {
  const std::vector<int> melody{523, 659, 784};
  std::vector<Sint16> pcm_samples;

  for (size_t note_index = 0; note_index < melody.size(); note_index++) {
    const int duration_ms = (note_index + 1 == melody.size()) ? 240 : 110;
    const int sample_count = duration_ms * kSampleRate / 1000;

    for (int i = 0; i < sample_count; i++) {
      const double time = static_cast<double>(i) / kSampleRate;
      const double absolute_time =
          (note_index * 0.11) + static_cast<double>(i) / kSampleRate;
      double envelope = 1.0;

      const int attack_samples = std::max(1, sample_count / 10);
      const int release_samples = std::max(1, sample_count / 5);
      if (i < attack_samples) {
        envelope = static_cast<double>(i) / attack_samples;
      } else if (i > sample_count - release_samples) {
        envelope =
            static_cast<double>(sample_count - i) / std::max(1, release_samples);
      }

      envelope = std::clamp(envelope, 0.0, 1.0);

      const double frequency = melody[note_index];
      const double vibrato =
          1.0 + 0.012 * std::sin(2.0 * kPi * 6.2 * absolute_time);
      const double fundamental =
          std::sin(2.0 * kPi * frequency * vibrato * time);
      const double third_harmonic =
          0.55 * std::sin(2.0 * kPi * frequency * 2.0 * vibrato * time + 0.2);
      const double fifth_harmonic =
          0.22 * std::sin(2.0 * kPi * frequency * 3.0 * vibrato * time + 0.4);
      double sample_value =
          (fundamental + third_harmonic + fifth_harmonic) * envelope * 0.40;

      sample_value = std::clamp(sample_value, -1.0, 1.0);
      const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
      pcm_samples.push_back(pcm);
      pcm_samples.push_back(pcm);
    }
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateViolinLamentChunk() {
  struct ViolinNote {
    double frequency;
    int duration_ms;
    double volume;
  };

  const std::vector<ViolinNote> melody{
      {659.25, 240, 0.16}, {587.33, 280, 0.18}, {523.25, 360, 0.22},
      {493.88, 320, 0.20}, {440.00, 800, 0.24}};
  std::vector<Sint16> pcm_samples;

  double note_time_offset = 0.0;
  for (const auto &note : melody) {
    const int sample_count = std::max(1, note.duration_ms * kSampleRate / 1000);
    const int attack_samples = std::max(1, sample_count / 8);
    const int release_samples = std::max(1, sample_count / 4);

    for (int i = 0; i < sample_count; i++) {
      double envelope = 1.0;
      if (i < attack_samples) {
        envelope = static_cast<double>(i) / attack_samples;
      } else if (i > sample_count - release_samples) {
        envelope =
            static_cast<double>(sample_count - i) / std::max(1, release_samples);
      }
      envelope = std::clamp(envelope, 0.0, 1.0);

      const double time = static_cast<double>(i) / kSampleRate;
      const double absolute_time = note_time_offset + time;
      const double vibrato =
          1.0 + 0.010 * std::sin(2.0 * kPi * 5.1 * absolute_time);
      const double detuned =
          1.0 + 0.004 * std::sin(2.0 * kPi * 2.7 * absolute_time + 0.8);
      const double fundamental =
          std::sin(2.0 * kPi * note.frequency * vibrato * time);
      const double upper =
          0.48 * std::sin(2.0 * kPi * note.frequency * 2.0 * detuned * time +
                          0.32);
      const double airy =
          0.18 * std::sin(2.0 * kPi * note.frequency * 3.0 * time + 1.1);
      const double bow_noise =
          0.035 * std::sin(2.0 * kPi * 91.0 * absolute_time + 0.4);
      double sample_value =
          (fundamental + upper + airy + bow_noise) * envelope * note.volume;

      sample_value = std::clamp(sample_value, -1.0, 1.0);
      const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
      pcm_samples.push_back(pcm);
      pcm_samples.push_back(pcm);
    }

    note_time_offset += static_cast<double>(note.duration_ms) / 1000.0;
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateMonsterExplosionChunk() {
  const int duration_ms = 170;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    double envelope = 1.0 - progress;
    envelope = std::clamp(envelope, 0.0, 1.0);

    const double low = std::sin(2.0 * kPi * 184.0 * time);
    const double mid = 0.55 * std::sin(2.0 * kPi * 133.0 * time + 0.8);
    const double high = 0.25 * std::sin(2.0 * kPi * 512.0 * time + 1.2);
    const double crackle =
        0.09 * std::sin(2.0 * kPi * (1500.0 - progress * 900.0) * time + 0.2);
    double sample_value = (low + mid + high + crackle) * envelope * 0.34;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateMonsterFartChunk() {
  const int duration_ms = 460;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double fundamental_frequency = 86.0 - progress * 34.0;
    const double wobble =
        1.0 + 0.07 * std::sin(2.0 * kPi * 4.4 * time + 0.2);
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.72);
    const double bloom = std::exp(-18.0 * std::pow(progress - 0.18, 2.0));
    const double noise =
        0.11 * std::sin(2.0 * kPi * (470.0 - progress * 170.0) * time + 0.4) +
        0.06 * std::sin(2.0 * kPi * (910.0 - progress * 260.0) * time + 1.1);
    const double body =
        0.95 * std::sin(2.0 * kPi * fundamental_frequency * wobble * time) +
        0.42 *
            std::sin(2.0 * kPi * fundamental_frequency * 1.7 * wobble * time +
                     0.6) +
        0.18 *
            std::sin(2.0 * kPi * fundamental_frequency * 2.4 * wobble * time +
                     1.5);
    double sample_value = (body + noise) * envelope * (0.24 + 0.11 * bloom);

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreatePacmanGagChunk() {
  const int duration_ms = 720;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double burst_one = std::exp(-120.0 * std::pow(progress - 0.18, 2.0));
    const double burst_two = std::exp(-105.0 * std::pow(progress - 0.45, 2.0));
    const double burst_three =
        std::exp(-90.0 * std::pow(progress - 0.69, 2.0));
    const double burst_mix =
        std::clamp(burst_one + 0.95 * burst_two + 0.8 * burst_three, 0.0, 1.4);
    const double throat_frequency = 158.0 - progress * 36.0;
    const double wet_vibrato =
        1.0 + 0.05 * std::sin(2.0 * kPi * 7.3 * time + 0.7);
    const double throat =
        std::sin(2.0 * kPi * throat_frequency * wet_vibrato * time) +
        0.58 *
            std::sin(2.0 * kPi * throat_frequency * 1.96 * wet_vibrato * time +
                     0.45);
    const double retch_formant =
        0.44 * std::sin(2.0 * kPi * (540.0 + burst_mix * 70.0) * time + 1.0) +
        0.24 * std::sin(2.0 * kPi * (860.0 - progress * 180.0) * time + 0.2);
    const double saliva_noise =
        0.10 * std::sin(2.0 * kPi * (1240.0 - progress * 220.0) * time + 0.9) +
        0.07 * std::sin(2.0 * kPi * (1730.0 - progress * 340.0) * time + 1.7);
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.55) * burst_mix;
    double sample_value =
        (0.78 * throat + retch_formant + saliva_noise) * envelope * 0.24;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateTeleporterZapChunk() {
  const int duration_ms = 640;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.42);
    const double pulse =
        0.55 + 0.45 * std::sin(2.0 * kPi * (8.0 + progress * 12.0) * time);
    const double carrier =
        std::sin(2.0 * kPi * (620.0 + progress * 410.0) * time +
                 0.65 * std::sin(2.0 * kPi * 11.0 * time));
    const double high_arc =
        0.55 * std::sin(2.0 * kPi * (1420.0 - progress * 290.0) * time + 0.3);
    const double shimmer =
        0.22 * std::sin(2.0 * kPi * (2380.0 - progress * 700.0) * time + 0.9);
    const double crackle_gate =
        (std::sin(2.0 * kPi * 26.0 * time) > 0.2) ? 1.0 : 0.18;
    const double crackle =
        0.14 * crackle_gate *
        std::sin(2.0 * kPi * (3100.0 - progress * 1200.0) * time + 1.7);
    double sample_value =
        (carrier * pulse + high_arc + shimmer + crackle) * envelope * 0.23;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateTeleporterArcChunk() {
  const int duration_ms = 540;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.33);
    const double arc_mod =
        1.0 + 0.14 * std::sin(2.0 * kPi * 13.0 * time + 0.4);
    const double fundamental =
        std::sin(2.0 * kPi * (840.0 + progress * 360.0) * arc_mod * time);
    const double upper =
        0.62 * std::sin(2.0 * kPi * (1720.0 - progress * 220.0) * time + 0.9);
    const double hiss =
        0.18 * std::sin(2.0 * kPi * (3280.0 - progress * 910.0) * time + 1.8);
    const double gating =
        (std::sin(2.0 * kPi * 31.0 * time) > -0.1) ? 1.0 : 0.10;
    const double crack =
        0.15 * gating *
        std::sin(2.0 * kPi * (4120.0 - progress * 1400.0) * time + 0.1);
    double sample_value =
        (fundamental + upper + hiss + crack) * envelope * 0.20;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateEditorBlockedChunk() {
  const int duration_ms = 170;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.80);
    const double metallic =
        std::sin(2.0 * kPi * 310.0 * time) +
        0.72 * std::sin(2.0 * kPi * 517.0 * time + 0.18) +
        0.38 * std::sin(2.0 * kPi * 836.0 * time + 0.74);
    const double clink =
        0.22 * std::sin(2.0 * kPi * 1510.0 * time + 1.4) +
        0.12 * std::sin(2.0 * kPi * 2140.0 * time + 0.2);
    double sample_value = (metallic + clink) * envelope * 0.17;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreatePotionSpawnChunk() {
  struct Note {
    double frequency;
    int duration_ms;
    double volume;
  };

  const std::vector<Note> melody{
      {880.0, 90, 0.18}, {1174.66, 95, 0.20}, {1396.91, 120, 0.22},
      {1760.0, 170, 0.19}};
  std::vector<Sint16> pcm_samples;

  double time_offset = 0.0;
  for (const auto &note : melody) {
    const int sample_count = std::max(1, note.duration_ms * kSampleRate / 1000);
    const int attack_samples = std::max(1, sample_count / 10);
    const int release_samples = std::max(1, sample_count / 5);

    for (int i = 0; i < sample_count; i++) {
      double envelope = 1.0;
      if (i < attack_samples) {
        envelope = static_cast<double>(i) / attack_samples;
      } else if (i > sample_count - release_samples) {
        envelope =
            static_cast<double>(sample_count - i) / std::max(1, release_samples);
      }
      envelope = std::clamp(envelope, 0.0, 1.0);

      const double time = static_cast<double>(i) / kSampleRate;
      const double absolute_time = time_offset + time;
      const double shimmer =
          1.0 + 0.015 * std::sin(2.0 * kPi * 8.0 * absolute_time + 0.3);
      const double fundamental =
          std::sin(2.0 * kPi * note.frequency * shimmer * time);
      const double bright =
          0.48 * std::sin(2.0 * kPi * note.frequency * 2.0 * time + 0.6);
      const double sparkle =
          0.18 * std::sin(2.0 * kPi * (note.frequency * 3.4) * time + 1.1);
      double sample_value =
          (fundamental + bright + sparkle) * envelope * note.volume;

      sample_value = std::clamp(sample_value, -1.0, 1.0);
      const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
      pcm_samples.push_back(pcm);
      pcm_samples.push_back(pcm);
    }

    time_offset += static_cast<double>(note.duration_ms) / 1000.0;
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateLovePotionChunk() {
  struct Note {
    double frequency;
    int duration_ms;
    double volume;
  };

  const std::vector<Note> melody{
      {659.25, 170, 0.20}, {783.99, 170, 0.21}, {987.77, 230, 0.22},
      {1318.51, 360, 0.18}};
  std::vector<Sint16> pcm_samples;

  double time_offset = 0.0;
  for (const auto &note : melody) {
    const int sample_count = std::max(1, note.duration_ms * kSampleRate / 1000);
    const int attack_samples = std::max(1, sample_count / 9);
    const int release_samples = std::max(1, sample_count / 4);
    for (int i = 0; i < sample_count; ++i) {
      double envelope = 1.0;
      if (i < attack_samples) {
        envelope = static_cast<double>(i) / static_cast<double>(attack_samples);
      } else if (i > sample_count - release_samples) {
        envelope = static_cast<double>(sample_count - i) /
                   static_cast<double>(std::max(1, release_samples));
      }
      envelope = std::clamp(envelope, 0.0, 1.0);

      const double time = static_cast<double>(i) / kSampleRate;
      const double absolute_time = time_offset + time;
      const double vibrato =
          1.0 + 0.006 * std::sin(2.0 * kPi * 5.2 * absolute_time);
      const double tone =
          0.72 * std::sin(2.0 * kPi * note.frequency * vibrato * time) +
          0.28 * std::sin(2.0 * kPi * note.frequency * 2.0 * time + 0.4);
      const double chime =
          0.20 * std::sin(2.0 * kPi * note.frequency * 3.0 * time + 1.0);
      const double sample_value =
          std::clamp((tone + chime) * envelope * note.volume, -1.0, 1.0);
      const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
      pcm_samples.push_back(pcm);
      pcm_samples.push_back(pcm);
    }
    time_offset += static_cast<double>(note.duration_ms) / 1000.0;
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }
  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateDynamiteSpawnChunk() {
  struct Note {
    double frequency;
    int duration_ms;
    double volume;
  };

  const std::vector<Note> melody{
      {392.0, 120, 0.18}, {523.25, 120, 0.19}, {659.25, 160, 0.21},
      {784.0, 220, 0.22}, {659.25, 260, 0.18}};
  std::vector<Sint16> pcm_samples;

  double time_offset = 0.0;
  for (const auto &note : melody) {
    const int sample_count = std::max(1, note.duration_ms * kSampleRate / 1000);
    const int attack_samples = std::max(1, sample_count / 12);
    const int release_samples = std::max(1, sample_count / 4);

    for (int i = 0; i < sample_count; i++) {
      double envelope = 1.0;
      if (i < attack_samples) {
        envelope = static_cast<double>(i) / attack_samples;
      } else if (i > sample_count - release_samples) {
        envelope =
            static_cast<double>(sample_count - i) / std::max(1, release_samples);
      }
      envelope = std::clamp(envelope, 0.0, 1.0);

      const double time = static_cast<double>(i) / kSampleRate;
      const double absolute_time = time_offset + time;
      const double swell =
          0.84 + 0.16 * std::sin(2.0 * kPi * 5.2 * absolute_time + 0.3);
      const double fundamental =
          std::sin(2.0 * kPi * note.frequency * time);
      const double fifth =
          0.46 * std::sin(2.0 * kPi * note.frequency * 1.5 * time + 0.2);
      const double brass =
          0.24 * std::sin(2.0 * kPi * note.frequency * 2.0 * time + 0.8);
      const double tension =
          0.10 * std::sin(2.0 * kPi * (note.frequency * 2.8 + 40.0) * time +
                          1.1);
      double sample_value =
          (fundamental + fifth + brass + tension) * swell * envelope *
          note.volume;

      sample_value = std::clamp(sample_value, -1.0, 1.0);
      const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
      pcm_samples.push_back(pcm);
      pcm_samples.push_back(pcm);
    }

    time_offset += static_cast<double>(note.duration_ms) / 1000.0;
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateDynamiteIgniteChunk() {
  const int duration_ms = 360;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.58);
    const double hiss =
        0.30 * std::sin(2.0 * kPi * (1780.0 + progress * 880.0) * time);
    const double crackle =
        0.24 * std::sin(2.0 * kPi * (2860.0 - progress * 940.0) * time + 0.7);
    const double flare =
        0.18 * std::sin(2.0 * kPi * 620.0 * time + progress * 4.0);
    const double ember =
        0.12 * std::sin(2.0 * kPi * 210.0 * time + 0.3);
    double sample_value = (hiss + crackle + flare + ember) * envelope * 0.26;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateDynamiteExplosionChunk() {
  const int duration_ms = 440;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.38);
    const double boom =
        std::sin(2.0 * kPi * (72.0 - progress * 18.0) * time + 0.1);
    const double body =
        0.72 * std::sin(2.0 * kPi * (118.0 - progress * 22.0) * time + 0.4);
    const double snap =
        0.34 * std::sin(2.0 * kPi * (620.0 - progress * 240.0) * time + 1.3);
    const double crackle =
        0.18 * std::sin(2.0 * kPi * (1820.0 - progress * 960.0) * time + 0.9) +
        0.08 * std::sin(2.0 * kPi * (2640.0 - progress * 1100.0) * time + 1.7);
    const double rumble_gate =
        0.70 + 0.30 * std::sin(2.0 * kPi * 7.0 * time + 0.2);
    double sample_value =
        (boom + body + snap + crackle) * rumble_gate * envelope * 0.36;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateBiohazardBeamChunk() {
  const int duration_ms = 900;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double surge =
        0.72 + 0.28 * std::sin(2.0 * kPi * (5.0 + progress * 3.0) * time);
    const double carrier =
        std::sin(2.0 * kPi * (182.0 + progress * 26.0) * time +
                 0.58 * std::sin(2.0 * kPi * 11.5 * time));
    const double upper =
        0.48 * std::sin(2.0 * kPi * (486.0 + progress * 120.0) * time + 0.8);
    const double arc =
        0.26 * std::sin(2.0 * kPi * (1280.0 - progress * 170.0) * time + 1.4);
    const double crackle_gate =
        (std::sin(2.0 * kPi * 24.0 * time + progress * 4.0) > 0.0) ? 1.0 : 0.18;
    const double crackle =
        0.18 * crackle_gate *
        std::sin(2.0 * kPi * (3120.0 - progress * 540.0) * time + 0.6);
    const double envelope =
        0.82 + 0.18 * std::sin(2.0 * kPi * 1.4 * time + 0.3);
    double sample_value =
        (carrier * surge + upper + arc + crackle) * envelope * 0.20;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreatePlasticExplosiveReadyChunk() {
  struct Note {
    double frequency;
    int duration_ms;
    double volume;
  };

  const std::vector<Note> melody{{1046.50, 72, 0.17},
                                 {1318.51, 82, 0.18},
                                 {987.77, 110, 0.16}};
  std::vector<Sint16> pcm_samples;

  double time_offset = 0.0;
  for (const auto &note : melody) {
    const int sample_count = std::max(1, note.duration_ms * kSampleRate / 1000);
    const int attack_samples = std::max(1, sample_count / 10);
    const int release_samples = std::max(1, sample_count / 3);

    for (int i = 0; i < sample_count; i++) {
      double envelope = 1.0;
      if (i < attack_samples) {
        envelope = static_cast<double>(i) / attack_samples;
      } else if (i > sample_count - release_samples) {
        envelope =
            static_cast<double>(sample_count - i) / std::max(1, release_samples);
      }
      envelope = std::clamp(envelope, 0.0, 1.0);

      const double time = static_cast<double>(i) / kSampleRate;
      const double absolute_time = time_offset + time;
      const double chirp =
          1.0 + 0.020 * std::sin(2.0 * kPi * 12.0 * absolute_time + 0.5);
      const double fundamental =
          std::sin(2.0 * kPi * note.frequency * chirp * time);
      const double upper =
          0.44 * std::sin(2.0 * kPi * note.frequency * 2.0 * time + 0.2);
      const double click =
          0.16 * std::sin(2.0 * kPi * (note.frequency * 4.4) * time + 1.1);
      double sample_value =
          (fundamental + upper + click) * envelope * note.volume;

      sample_value = std::clamp(sample_value, -1.0, 1.0);
      const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
      pcm_samples.push_back(pcm);
      pcm_samples.push_back(pcm);
    }

    time_offset += static_cast<double>(note.duration_ms) / 1000.0;
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreatePlasticExplosiveWallBreakChunk() {
  const int duration_ms = 520;
  const int sample_count = std::max(1, duration_ms * kSampleRate / 1000);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(sample_count * kChannels);

  for (int i = 0; i < sample_count; i++) {
    const double time = static_cast<double>(i) / kSampleRate;
    const double progress = static_cast<double>(i) / sample_count;
    const double envelope =
        std::pow(std::clamp(1.0 - progress, 0.0, 1.0), 0.28);
    const double low =
        std::sin(2.0 * kPi * (82.0 - progress * 24.0) * time);
    const double rumble =
        0.62 * std::sin(2.0 * kPi * (134.0 - progress * 36.0) * time + 0.25);
    const double crack =
        0.28 * std::sin(2.0 * kPi * (1680.0 - progress * 780.0) * time + 0.9);
    const double debris =
        0.18 * std::sin(2.0 * kPi * (2440.0 - progress * 1200.0) * time + 1.8);
    double sample_value = (low + rumble + crack + debris) * envelope * 0.30;

    sample_value = std::clamp(sample_value, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample_value * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

Mix_Chunk *Audio::CreateInvulnerabilityLoopChunk() {
  const double duration_seconds = 4.0;
  const int total_samples =
      static_cast<int>(duration_seconds * kSampleRate);
  std::vector<Sint16> pcm_samples;
  pcm_samples.reserve(static_cast<size_t>(total_samples) * 2);

  const double f_root = 110.0;
  const double f_fifth = 165.0;
  const double f_octave = 220.0;
  const double f_high = 330.0;
  const double f_air = 440.0;
  const double lfo_slow = 0.25;
  const double lfo_med = 0.50;
  const double lfo_shimmer = 1.0;

  for (int i = 0; i < total_samples; ++i) {
    const double t = static_cast<double>(i) / kSampleRate;
    const double slow = 0.78 + 0.22 * std::sin(2.0 * kPi * lfo_slow * t);
    const double med = std::sin(2.0 * kPi * lfo_med * t);
    const double shimmer = std::sin(2.0 * kPi * lfo_shimmer * t);

    double sample =
        0.46 * std::sin(2.0 * kPi * f_root * t) +
        0.30 * std::sin(2.0 * kPi * f_fifth * t + 0.30 * med) +
        0.22 * std::sin(2.0 * kPi * f_octave * t) +
        0.14 * std::sin(2.0 * kPi * f_high * t + 0.45 * med) +
        0.07 * std::sin(2.0 * kPi * f_air * t + 0.6 * shimmer);

    sample *= 0.16 * slow;
    sample = std::clamp(sample, -1.0, 1.0);
    const Sint16 pcm = static_cast<Sint16>(sample * 32767.0);
    pcm_samples.push_back(pcm);
    pcm_samples.push_back(pcm);
  }

  std::vector<Uint8> wav_buffer = build_wav_buffer(pcm_samples);
  SDL_RWops *wav_stream =
      SDL_RWFromConstMem(wav_buffer.data(), static_cast<int>(wav_buffer.size()));
  if (wav_stream == nullptr) {
    return nullptr;
  }

  return Mix_LoadWAV_RW(wav_stream, 1);
}

void Audio::AmplifyChunk(Mix_Chunk *chunk, double gain) {
  if (chunk == nullptr || chunk->abuf == nullptr || chunk->alen == 0 ||
      gain <= 1.0) {
    return;
  }

  Sint16 *samples = reinterpret_cast<Sint16 *>(chunk->abuf);
  const Uint32 sample_count = chunk->alen / sizeof(Sint16);
  for (Uint32 i = 0; i < sample_count; ++i) {
    const double amplified = static_cast<double>(samples[i]) * gain;
    samples[i] = static_cast<Sint16>(
        std::clamp(amplified, -32768.0, 32767.0));
  }
}

void Audio::PlayChunk(Mix_Chunk *chunk) {
  if (!audio_ready || chunk == nullptr) {
    return;
  }
  Mix_PlayChannel(-1, chunk, 0);
}

/**
 * @brief as the name says
 *
 */
void Audio::PlayCoin() { PlayChunk(SFX_coin); };

/**
 * @brief Plays the short "zonk" when one life is consumed.
 *
 */
void Audio::PlayLifeLost() { PlayChunk(SFX_life_lost); };

/**
 * @brief Plays the sad cue before the lose screen appears.
 *
 */
void Audio::PlayGameOver() {
  if (!audio_ready) {
    return;
  }

  PlayChunk(SFX_gameover);
};

void Audio::StartLoseMusic() {
  if (!audio_ready || lose_music == nullptr) {
    return;
  }

  menu_music_active = false;
  if (Mix_PlayingMusic() != 0) {
    Mix_HaltMusic();
  }
  ClearMenuSpectrumLevels();
  Mix_PlayMusic(lose_music, 0);
}

/**
 * @brief as the name says
 *
 */
void Audio::PlayWin() {
  if (!audio_ready) {
    return;
  }

  if (win_music != nullptr) {
    menu_music_active = false;
    if (Mix_PlayingMusic() != 0) {
      Mix_HaltMusic();
    }
    ClearMenuSpectrumLevels();
    if (Mix_PlayMusic(win_music, 0) == 0) {
      return;
    }
  }

  PlayChunk(SFX_win);
};

void Audio::PlayMenuMove() { PlayChunk(SFX_menu_move); };

void Audio::PlayMenuSelect() { PlayChunk(SFX_menu_select); };

void Audio::PlayCountdownTick() { PlayChunk(SFX_countdown_tick); };

void Audio::PlayStartTrumpet() { PlayChunk(SFX_start_trumpet); };

bool Audio::StartMenuMusic() {
  if (!audio_ready || menu_music == nullptr) {
    return false;
  }

  if (menu_music_active && Mix_PlayingMusic() != 0) {
    return true;
  }

  ClearMenuSpectrumLevels();
  if (Mix_PlayingMusic() != 0) {
    Mix_HaltMusic();
  }

  menu_music_active = (Mix_PlayMusic(menu_music, -1) == 0);
  return menu_music_active;
}

bool Audio::FadeOutMenuMusic(int fade_out_ms) {
  if (!audio_ready || menu_music == nullptr || Mix_PlayingMusic() == 0) {
    return false;
  }

  menu_music_active = false;
  if (fade_out_ms <= 0) {
    Mix_HaltMusic();
    ClearMenuSpectrumLevels();
    return true;
  }

  return Mix_FadeOutMusic(fade_out_ms) == 1;
}

void Audio::StopMenuMusic() {
  if (!audio_ready) {
    return;
  }

  menu_music_active = false;
  if (Mix_PlayingMusic() != 0) {
    Mix_HaltMusic();
  }
  ClearMenuSpectrumLevels();
}

bool Audio::StartDiscoMusic() {
  if (!audio_ready || disco_music == nullptr) {
    return false;
  }

  menu_music_active = false;
  ClearMenuSpectrumLevels();
  if (Mix_PlayingMusic() != 0) {
    Mix_HaltMusic();
  }

  return Mix_PlayMusic(disco_music, -1) == 0;
}

bool Audio::FadeOutDiscoMusic(int fade_out_ms) {
  if (!audio_ready || disco_music == nullptr || Mix_PlayingMusic() == 0) {
    return false;
  }

  menu_music_active = false;
  if (fade_out_ms <= 0) {
    Mix_HaltMusic();
    ClearMenuSpectrumLevels();
    return true;
  }

  return Mix_FadeOutMusic(fade_out_ms) == 1;
}

void Audio::StopDiscoMusic() {
  if (!audio_ready) {
    return;
  }

  menu_music_active = false;
  if (Mix_PlayingMusic() != 0) {
    Mix_HaltMusic();
  }
  ClearMenuSpectrumLevels();
}

void Audio::StopEndScreenMusic() {
  if (!audio_ready) {
    return;
  }

  menu_music_active = false;
  if (Mix_PlayingMusic() != 0) {
    Mix_HaltMusic();
  }
  ClearMenuSpectrumLevels();
}

bool Audio::IsMenuMusicPlaying() const {
  if (!audio_ready || menu_music == nullptr) {
    return false;
  }

  return menu_music_active && Mix_PlayingMusic() != 0;
}

void Audio::PlayMonsterShot() { PlayChunk(SFX_monster_shot); };

void Audio::PlayFireballWallHit() { PlayChunk(SFX_fireball_wall_hit); };

void Audio::PlaySlimeShot() { PlayChunk(SFX_slime_shot); };

void Audio::PlaySlimeImpact() { PlayChunk(SFX_slime_impact); };

void Audio::PlayMonsterExplosion() { PlayChunk(SFX_monster_explosion); };

void Audio::PlayMonsterFart() { PlayChunk(SFX_monster_fart); };

void Audio::PlayPacmanGag() { PlayChunk(SFX_pacman_gag); };

void Audio::PlayTeleporterZap() { PlayChunk(SFX_teleporter_zap); };

void Audio::PlayTeleporterArc() { PlayChunk(SFX_teleporter_arc); };

void Audio::PlayEditorBlocked() { PlayChunk(SFX_editor_blocked); };

void Audio::PlayPotionSpawn() { PlayChunk(SFX_potion_spawn); };

void Audio::PlayLovePotion() { PlayChunk(SFX_love_potion); };

void Audio::PlayDynamiteSpawn() { PlayChunk(SFX_dynamite_spawn); };

void Audio::PlayDynamiteIgnite() { PlayChunk(SFX_dynamite_ignite); };

void Audio::PlayDynamiteExplosion() { PlayChunk(SFX_dynamite_explosion); };

void Audio::PlayPlasticExplosiveSpawn() {
  PlayChunk(SFX_plastic_explosive_ready);
};

void Audio::PlayPlasticExplosivePlace() {
  PlayChunk(SFX_plastic_explosive_ready);
};

void Audio::PlayPlasticExplosiveWallBreak() {
  PlayChunk(SFX_plastic_explosive_wall_break);
};

void Audio::PlayPunch() { PlayChunk(SFX_punch); };

void Audio::PlayGoatBleat() { PlayChunk(SFX_goat_bleat); };

void Audio::PlayRubbleCrash() { PlayChunk(SFX_rubble_crash); };

void Audio::PlayAirstrikeRadio() { PlayChunk(SFX_airstrike_radio); };

void Audio::PlayAirstrikeExplosion() { PlayChunk(SFX_airstrike_explosion); };

void Audio::StartAirstrikePlane() {
  if (!audio_ready || SFX_airstrike_propeller == nullptr) {
    return;
  }

  if (airstrike_plane_channel != -1 &&
      Mix_Playing(airstrike_plane_channel) != 0) {
    return;
  }

  airstrike_plane_channel =
      Mix_PlayChannel(-1, SFX_airstrike_propeller, -1);
}

void Audio::FadeOutAirstrikePlane(int fade_out_ms) {
  if (!audio_ready || airstrike_plane_channel == -1) {
    return;
  }

  if (fade_out_ms <= 0) {
    Mix_HaltChannel(airstrike_plane_channel);
  } else {
    Mix_FadeOutChannel(airstrike_plane_channel, fade_out_ms);
  }
  airstrike_plane_channel = -1;
}

void Audio::PlayRocketLaunch() {
  if (!audio_ready || SFX_rocket_launch == nullptr) {
    return;
  }

  Mix_HaltChannel(rocket_launch_channel);
  Mix_PlayChannel(rocket_launch_channel, SFX_rocket_launch, 0);
};

void Audio::StopRocketLaunch() {
  if (!audio_ready) {
    return;
  }

  Mix_HaltChannel(rocket_launch_channel);
}

void Audio::StartBiohazardBeam() {
  if (!audio_ready || SFX_biohazard_beam == nullptr) {
    return;
  }

  if (biohazard_beam_channel != -1) {
    Mix_HaltChannel(biohazard_beam_channel);
  }

  biohazard_beam_channel = Mix_PlayChannel(-1, SFX_biohazard_beam, -1);
}

void Audio::StopBiohazardBeam() {
  if (!audio_ready || biohazard_beam_channel == -1) {
    return;
  }

  Mix_HaltChannel(biohazard_beam_channel);
  biohazard_beam_channel = -1;
}

void Audio::PlayElectrifiedMonsterRoar() {
  PlayChunk(SFX_electrified_monster_roar);
};

void Audio::PlayElectrifiedMonsterImpact() {
  PlayChunk(SFX_electrified_monster_impact);
};

void Audio::PlayNuclearBombAlarm() { PlayChunk(SFX_nuclear_bomb_alarm); };

void Audio::PlayNuclearBombDrop() { PlayChunk(SFX_nuclear_bomb_drop); };

void Audio::PlayNuclearBombExplosion() {
  PlayChunk(SFX_nuclear_bomb_explosion);
};

void Audio::PlayAlienLaser() { PlayChunk(SFX_alien_laser); };

void Audio::PlayAlienScream() { PlayChunk(SFX_alien_scream); };

void Audio::PlayAlienExplosion() { PlayChunk(SFX_alien_explosion); };

Uint32 Audio::GetAirstrikeRadioDurationMs() const {
  if (!audio_ready || SFX_airstrike_radio == nullptr) {
    return AIRSTRIKE_RADIO_DELAY_MS;
  }

  const int bytes_per_frame = kChannels * static_cast<int>(sizeof(Sint16));
  if (bytes_per_frame <= 0) {
    return AIRSTRIKE_RADIO_DELAY_MS;
  }

  const double duration_ms =
      static_cast<double>(SFX_airstrike_radio->alen) * 1000.0 /
      static_cast<double>(kSampleRate * bytes_per_frame);
  return std::max<Uint32>(
      AIRSTRIKE_RADIO_DELAY_MS,
      static_cast<Uint32>(std::lround(std::max(0.0, duration_ms))));
};

void Audio::StartInvulnerabilityLoop() {
  if (!audio_ready || SFX_invulnerability_loop == nullptr) {
    return;
  }

  if (invulnerability_loop_channel != -1 &&
      Mix_Playing(invulnerability_loop_channel) != 0) {
    return;
  }

  invulnerability_loop_channel = Mix_PlayChannel(-1, SFX_invulnerability_loop, -1);
}

void Audio::StopInvulnerabilityLoop() {
  if (!audio_ready || invulnerability_loop_channel == -1) {
    return;
  }

  Mix_HaltChannel(invulnerability_loop_channel);
  invulnerability_loop_channel = -1;
}

#endif
