/*
 * PROJECT COMMENT (renderer.cpp)
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

#include "renderer.h"
#include "audio.h"
#include "paths.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <random>

#include "game.h"
#include "goodie.h"
#include "map.h"
#include "monster.h"
#include "pacman.h"

namespace {

const SDL_Color kHudTextColor{243, 236, 222, 255};
const SDL_Color kMenuTextColor{225, 223, 218, 255};
const SDL_Color kSelectedMenuTextColor{255, 248, 238, 255};
const SDL_Color kStatusTextColor{255, 189, 163, 255};
const SDL_Color kWarningTextColor{255, 110, 110, 255};
const SDL_Color kDisabledHudTextColor{156, 150, 162, 255};
const SDL_Color kPanelFillColor{10, 6, 18, 205};
const SDL_Color kPanelBorderColor{196, 130, 92, 255};
const SDL_Color kStartMenuPanelFillColor{10, 6, 18, 118};
const SDL_Color kStartMenuPanelBorderColor{196, 130, 92, 180};
const SDL_Color kBrickOutlineColor{78, 20, 14, 255};
const SDL_Color kShieldTextColor{116, 220, 255, 255};
const SDL_Color kStartLogoHighlightColor{238, 252, 255, 255};
const SDL_Color kStartLogoLightColor{88, 234, 255, 255};
const SDL_Color kStartLogoMidColor{44, 156, 255, 255};
const SDL_Color kStartLogoDeepColor{18, 70, 222, 255};
const SDL_Color kStartLogoOutlineColor{6, 18, 102, 255};
const SDL_Color kTeleporterBlue{78, 160, 255, 255};
const SDL_Color kTeleporterRed{255, 104, 104, 255};
const SDL_Color kTeleporterGreen{110, 205, 118, 255};
const SDL_Color kTeleporterGrey{172, 176, 186, 255};
const SDL_Color kTeleporterYellow{244, 214, 88, 255};
constexpr double kLogoPi = 3.14159265358979323846;
constexpr int kPacmanFramesPerDirection = 4;
constexpr int kMonsterFramesPerDirection = 4;
constexpr int kAlienAnimationFrames = ALIEN_ANIMATION_FRAME_COUNT;
constexpr int kAlienExplosionFrames = ALIEN_EXPLOSION_FRAME_ASSET_COUNT;
constexpr int kRocketFlightFrames = 2;
constexpr int kAirstrikeExplosionFrames = 5;
constexpr int kMonsterExplosionFrames = MONSTER_EXPLOSION_FRAME_COUNT;
constexpr int kFartCloudFrames = 4;
constexpr int kSlimeSplashGridColumns = 3;
constexpr int kSlimeSplashGridRows = 3;
constexpr Uint32 kPacmanWalkFrameMs = 140;
constexpr Uint32 kFartCloudOpenDurationMs = 320;
constexpr Uint32 kFartCloudFrameMs =
    kFartCloudOpenDurationMs / kFartCloudFrames;
constexpr Uint8 kSlimeBallBaseAlpha = 176;
constexpr Uint8 kSlimeOverlayBaseAlpha = 140;
constexpr Uint8 kSlimeSplashBaseAlpha = 176;
constexpr double kMonsterRenderScale = 1.19;
constexpr double kAlienRenderScale = ALIEN_RENDER_SCALE;
constexpr double kFartCloudRenderScale = 1.20;
constexpr double kSlimeBallRenderScale = 1.00;
constexpr double kSlimeOverlayRenderScale = 1.22;
constexpr double kSlimeSplashRenderScale = 1.26;
constexpr double kFartCloudDriftXFactor = 0.06;
constexpr double kFartCloudDriftYFactor = 0.05;
constexpr double kFartCloudScalePulseAmplitude = 0.04;
constexpr double kRocketSpriteAngleOffsetDegrees = 0.0;
constexpr double kSpriteFootRowFactor = 0.65;
constexpr double kFloorObjectDepthRowFactor = 0.55;
constexpr double kNonCharacterSpriteLiftFactor = 0.25;

struct MonsterSpriteDescriptor {
  char monster_char;
  const char *color_name;
};

constexpr std::array<MonsterSpriteDescriptor, 5> kMonsterSpriteDescriptors{{
    {MONSTER_FEW, "purple"},
    {MONSTER_MEDIUM, "blue"},
    {MONSTER_MANY, "red"},
    {MONSTER_EXTRA, "green"},
    {GOAT, "goat"},
}};

constexpr std::array<const char *, 15> kAlienThoughts{{
    "Ich bin Mika, und mein Kopf ist nur Deko.",
    "Ich bin Mika, und selbst Staub denkt schneller.",
    "Ich bin Mika, und ich verliere gegen Tueren.",
    "Ich bin Mika, und mein Plan ist weggelaufen.",
    "Ich bin Mika, und mein Hirn macht Mittag.",
    "Ich bin Mika, und ich bin der Beweis gegen Evolution.",
    "Ich bin Mika, und sogar Sackgassen meiden mich.",
    "Ich bin Mika, und mein IQ stolpert barfuss.",
    "Ich bin Mika, und Denken ist bei mir Deko.",
    "Ich bin Mika, und ich verwirre sogar mich.",
    "Ich bin Mika, und mein Talent ist Scheitern.",
    "Ich bin Mika, und ich stehe dumm im Weg.",
    "Ich bin Mika, und mein Mut hat gekuendigt.",
    "Ich bin Mika, und jeder Kompass gibt auf.",
    "Ich bin Mika, und mein Gehirn laeuft rueckwaerts.",
}};

SDL_Rect makeProjectedFaceRect(SDL_FPoint top_left, SDL_FPoint top_right,
                               SDL_FPoint bottom_left, SDL_FPoint bottom_right) {
  const int left = static_cast<int>(std::lround(std::min(
      {top_left.x, top_right.x, bottom_left.x, bottom_right.x})));
  const int right = static_cast<int>(std::lround(std::max(
      {top_left.x, top_right.x, bottom_left.x, bottom_right.x})));
  const int top = static_cast<int>(std::lround(std::min(
      {top_left.y, top_right.y, bottom_left.y, bottom_right.y})));
  const int bottom = static_cast<int>(std::lround(std::max(
      {top_left.y, top_right.y, bottom_left.y, bottom_right.y})));
  return SDL_Rect{left, top, std::max(1, right - left),
                  std::max(1, bottom - top)};
}

bool hasVisibleArea(const SDL_Rect &rect) {
  return rect.w > 0 && rect.h > 0;
}

double HashUnit(int seed) {
  const double value =
      std::sin(static_cast<double>(seed) * 12.9898 + 78.233) * 43758.5453;
  return value - std::floor(value);
}

float NuclearCraterRenderEdgeRadius(float base_radius, int shape_seed,
                                    float angle) {
  const float phase = static_cast<float>(shape_seed % 997) * 0.013f;
  const float wobble =
      0.075f * std::sin(angle * 5.0f + phase) +
      0.045f * std::sin(angle * 9.0f + phase * 1.7f) +
      0.030f * std::sin(angle * 14.0f + phase * 0.61f);
  return base_radius * std::clamp(0.94f + wobble, 0.78f, 1.08f);
}

double HashSigned(int seed) {
  return HashUnit(seed) * 2.0 - 1.0;
}

SDL_Rect intersectRects(const SDL_Rect &left, const SDL_Rect &right) {
  const int x1 = std::max(left.x, right.x);
  const int y1 = std::max(left.y, right.y);
  const int x2 = std::min(left.x + left.w, right.x + right.w);
  const int y2 = std::min(left.y + left.h, right.y + right.h);
  return SDL_Rect{x1, y1, std::max(0, x2 - x1), std::max(0, y2 - y1)};
}

SDL_Rect expandRect(const SDL_Rect &rect, int padding) {
  return SDL_Rect{rect.x - padding, rect.y - padding, rect.w + padding * 2,
                  rect.h + padding * 2};
}

SDL_Rect offsetRectY(const SDL_Rect &rect, int delta_y) {
  return SDL_Rect{rect.x, rect.y + delta_y, rect.w, rect.h};
}

void subtractRect(const SDL_Rect &source, const SDL_Rect &cut,
                  std::vector<SDL_Rect> &out) {
  const SDL_Rect overlap = intersectRects(source, cut);
  if (!hasVisibleArea(overlap)) {
    out.push_back(source);
    return;
  }

  const SDL_Rect top_rect{source.x, source.y, source.w, overlap.y - source.y};
  const SDL_Rect bottom_rect{source.x, overlap.y + overlap.h, source.w,
                             source.y + source.h - (overlap.y + overlap.h)};
  const SDL_Rect left_rect{source.x, overlap.y, overlap.x - source.x,
                           overlap.h};
  const SDL_Rect right_rect{overlap.x + overlap.w, overlap.y,
                            source.x + source.w - (overlap.x + overlap.w),
                            overlap.h};

  if (hasVisibleArea(top_rect)) {
    out.push_back(top_rect);
  }
  if (hasVisibleArea(bottom_rect)) {
    out.push_back(bottom_rect);
  }
  if (hasVisibleArea(left_rect)) {
    out.push_back(left_rect);
  }
  if (hasVisibleArea(right_rect)) {
    out.push_back(right_rect);
  }
}

void configureSmoothTextureSampling(SDL_Texture *texture) {
  if (texture == nullptr) {
    return;
  }
#if SDL_VERSION_ATLEAST(2, 0, 12)
  SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#endif
}

const char *DifficultyLabel(Difficulty difficulty) {
  switch (difficulty) {
  case Difficulty::Training:
    return "Training";
  case Difficulty::Easy:
    return "leicht";
  case Difficulty::Medium:
    return "mittel";
  case Difficulty::Hard:
    return "schwer";
  default:
    return "mittel";
  }
}

int ClampFloorTextureIndex(int floor_texture_index) {
  return std::clamp(floor_texture_index, 0, FLOOR_TEXTURE_OPTION_COUNT - 1);
}

std::string FloorTextureLabel(int floor_texture_index) {
  return "Textur #" +
         std::to_string(ClampFloorTextureIndex(floor_texture_index) + 1);
}

SDL_Rect MakeFloorTextureSampleRect(const SDL_Point &texture_size, int row,
                                    int col) {
  if (texture_size.x <= 0 || texture_size.y <= 0) {
    return SDL_Rect{0, 0, 0, 0};
  }

  const int sample_width =
      std::clamp(FLOOR_TEXTURE_SAMPLE_SIZE_PX, 1, texture_size.x);
  const int sample_height =
      std::clamp(FLOOR_TEXTURE_SAMPLE_SIZE_PX, 1, texture_size.y);
  const int max_x = std::max(0, texture_size.x - sample_width);
  const int max_y = std::max(0, texture_size.y - sample_height);
  const int stride_x = std::max(
      1, std::min(FLOOR_TEXTURE_SAMPLE_STRIDE_PX, std::max(1, sample_width)));
  const int stride_y = std::max(
      1, std::min(FLOOR_TEXTURE_SAMPLE_STRIDE_PX, std::max(1, sample_height)));
  const int sample_x =
      max_x > 0 ? (col * stride_x + row * 17) % (max_x + 1) : 0;
  const int sample_y =
      max_y > 0 ? (row * stride_y + col * 11) % (max_y + 1) : 0;
  return SDL_Rect{sample_x, sample_y, sample_width, sample_height};
}

int MonsterSpriteVariantIndex(char monster_char) {
  for (size_t index = 0; index < kMonsterSpriteDescriptors.size(); ++index) {
    if (kMonsterSpriteDescriptors[index].monster_char == monster_char) {
      return static_cast<int>(index);
    }
  }
  return 0;
}

const char *MonsterDirectionName(Directions direction) {
  switch (direction) {
  case Directions::Up:
    return "back";
  case Directions::Left:
    return "left";
  case Directions::Right:
    return "right";
  case Directions::Down:
  case Directions::None:
  default:
    return "front";
  }
}

int MonsterDirectionIndex(Directions direction) {
  switch (direction) {
  case Directions::Up:
    return 1;
  case Directions::Left:
    return 2;
  case Directions::Right:
    return 3;
  case Directions::Down:
  case Directions::None:
  default:
    return 0;
  }
}

size_t MonsterTextureIndex(char monster_char, Directions direction,
                           int frame_index) {
  const int variant_index = MonsterSpriteVariantIndex(monster_char);
  const int direction_index = MonsterDirectionIndex(direction);
  const int normalized_frame =
      ((frame_index % kMonsterFramesPerDirection) + kMonsterFramesPerDirection) %
      kMonsterFramesPerDirection;
  return static_cast<size_t>(variant_index * 4 * kMonsterFramesPerDirection +
                             direction_index * kMonsterFramesPerDirection +
                             normalized_frame);
}

size_t GoatDirectionalTextureIndex(Directions direction, int frame_index) {
  const int direction_index = MonsterDirectionIndex(direction);
  const int normalized_frame =
      ((frame_index % kMonsterFramesPerDirection) + kMonsterFramesPerDirection) %
      kMonsterFramesPerDirection;
  return static_cast<size_t>(direction_index * kMonsterFramesPerDirection +
                             normalized_frame);
}

SDL_Rect MonsterRenderRect(const PixelCoord &monster_px, int element_size) {
  const int render_size = std::max(
      1, static_cast<int>(std::lround(element_size * kMonsterRenderScale)));
  const int offset = (element_size - render_size) / 2;
  return SDL_Rect{monster_px.x + offset, monster_px.y + offset, render_size,
                  render_size};
}

SDL_Point TextureRenderSize(int max_size, const SDL_Point &source_size) {
  int render_w = max_size;
  int render_h = max_size;
  if (source_size.x > 0 && source_size.y > 0) {
    if (source_size.x >= source_size.y) {
      render_h = std::max(
          1, static_cast<int>(std::lround(
                 static_cast<double>(max_size) * source_size.y /
                 static_cast<double>(source_size.x))));
    } else {
      render_w = std::max(
          1, static_cast<int>(std::lround(
                 static_cast<double>(max_size) * source_size.x /
                 static_cast<double>(source_size.y))));
    }
  }

  return SDL_Point{render_w, render_h};
}

SDL_FPoint TextureScaleFactors(double max_scale, const SDL_Point &source_size) {
  SDL_FPoint result{static_cast<float>(max_scale),
                    static_cast<float>(max_scale)};
  if (source_size.x <= 0 || source_size.y <= 0) {
    return result;
  }

  if (source_size.x >= source_size.y) {
    result.y = static_cast<float>(
        max_scale * static_cast<double>(source_size.y) /
        static_cast<double>(source_size.x));
  } else {
    result.x = static_cast<float>(
        max_scale * static_cast<double>(source_size.x) /
        static_cast<double>(source_size.y));
  }
  return result;
}

size_t AlienTextureIndexForFrame(int frame_index) {
  const int normalized_frame =
      ((frame_index % kAlienAnimationFrames) + kAlienAnimationFrames) %
      kAlienAnimationFrames;
  return static_cast<size_t>(normalized_frame);
}

SDL_Rect AlienRenderRect(const PixelCoord &alien_px, int element_size,
                         const SDL_Point &source_size) {
  const int max_size = std::max(
      1, static_cast<int>(std::lround(element_size * kAlienRenderScale)));
  const SDL_Point render_size = TextureRenderSize(max_size, source_size);
  const int center_x = alien_px.x + element_size / 2;
  const int anchor_y = alien_px.y + (element_size + max_size) / 2;
  return SDL_Rect{center_x - render_size.x / 2, anchor_y - render_size.y,
                  render_size.x, render_size.y};
}

SDL_Rect BottomAnchoredTextureRect(int center_x, int anchor_y, int max_size,
                                   const SDL_Point &source_size) {
  const SDL_Point render_size = TextureRenderSize(max_size, source_size);
  return SDL_Rect{center_x - render_size.x / 2, anchor_y - render_size.y,
                  render_size.x, render_size.y};
}

SDL_Rect AlienExplosionRenderRect(const PixelCoord &alien_px, int element_size,
                                  const SDL_Point &source_size,
                                  double scale_multiplier = 1.0) {
  const int render_size = std::max(
      1, static_cast<int>(std::lround(
             element_size * ALIEN_EXPLOSION_RENDER_SCALE * scale_multiplier)));
  const int center_x = alien_px.x + element_size / 2;
  const int anchor_y =
      alien_px.y + element_size / 2 +
      static_cast<int>(std::lround(element_size * 0.18));
  return BottomAnchoredTextureRect(center_x, anchor_y, render_size,
                                   source_size);
}

bool AlienExplosionResidueVisible(const AlienAgent &alien, Uint32 now) {
  if (alien.is_alive || !alien.explosion_spawned ||
      alien.explosion_trigger_ticks == 0 ||
      now < alien.explosion_trigger_ticks) {
    return false;
  }
  return now - alien.explosion_trigger_ticks >= ALIEN_EXPLOSION_DURATION_MS;
}

SDL_Color TeleporterColor(char teleporter_digit) {
  switch (teleporter_digit) {
  case '1':
    return kTeleporterBlue;
  case '2':
    return kTeleporterRed;
  case '3':
    return kTeleporterGreen;
  case '4':
    return kTeleporterGrey;
  case '5':
  default:
    return kTeleporterYellow;
  }
}

SDL_Color LerpColor(const SDL_Color &from, const SDL_Color &to, float factor) {
  factor = std::clamp(factor, 0.0f, 1.0f);
  const auto mix_channel = [factor](Uint8 start, Uint8 end) -> Uint8 {
    const float blended =
        static_cast<float>(start) +
        (static_cast<float>(end) - static_cast<float>(start)) * factor;
    return static_cast<Uint8>(std::clamp(static_cast<int>(std::lround(blended)),
                                         0, 255));
  };

  return SDL_Color{mix_channel(from.r, to.r), mix_channel(from.g, to.g),
                   mix_channel(from.b, to.b), 255};
}

SDL_Color WithAlpha(const SDL_Color &color, Uint8 alpha) {
  return SDL_Color{color.r, color.g, color.b, alpha};
}

SDL_Color ScaleColor(const SDL_Color &color, float factor) {
  factor = std::max(0.0f, factor);
  const auto scale_channel = [factor](Uint8 channel) -> Uint8 {
    return static_cast<Uint8>(std::clamp(
        static_cast<int>(std::lround(static_cast<float>(channel) * factor)), 0,
        255));
  };
  return SDL_Color{scale_channel(color.r), scale_channel(color.g),
                   scale_channel(color.b), color.a};
}

float TileNoise01(int row, int col) {
  const double seed = std::sin(static_cast<double>(row) * 127.1 +
                               static_cast<double>(col) * 311.7) *
                      43758.5453123;
  return static_cast<float>(seed - std::floor(seed));
}

SDL_Color SpectrumColor(float factor) {
  factor = std::clamp(factor, 0.0f, 1.0f);
  struct ColorStop {
    float position;
    SDL_Color color;
  };
  constexpr std::array<ColorStop, 6> kStops{{
      {0.00f, SDL_Color{24, 132, 255, 255}},
      {0.20f, SDL_Color{0, 232, 255, 255}},
      {0.40f, SDL_Color{0, 255, 154, 255}},
      {0.60f, SDL_Color{255, 238, 0, 255}},
      {0.80f, SDL_Color{255, 136, 0, 255}},
      {1.00f, SDL_Color{255, 24, 24, 255}},
  }};

  for (size_t index = 1; index < kStops.size(); ++index) {
    if (factor <= kStops[index].position) {
      const float start = kStops[index - 1].position;
      const float range = std::max(0.0001f, kStops[index].position - start);
      const float local = (factor - start) / range;
      return LerpColor(kStops[index - 1].color, kStops[index].color, local);
    }
  }

  return kStops.back().color;
}

float CatmullRomInterpolate(float p0, float p1, float p2, float p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return 0.5f *
         ((2.0f * p1) + (-p0 + p2) * t +
          (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
          (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

MapCoord StepRenderCoord(MapCoord coord, Directions direction) {
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

double DirectionAngleDegrees(Directions direction) {
  switch (direction) {
  case Directions::Up:
    return -90.0;
  case Directions::Down:
    return 90.0;
  case Directions::Left:
    return 180.0;
  case Directions::Right:
  case Directions::None:
  default:
    return 0.0;
  }
}

} // namespace

Renderer::Renderer(Map *_map, Game *_game)
    : screen_res_x(0), screen_res_y(0), element_size(0), hud_top_y(0),
      offset_x(0), offset_y(0), scene_origin_y(0), wall_height_px(0),
      cos_tilt(1.0), sin_tilt(0.0), rows(0), cols(0), map(_map), game(_game),
      sdl_window(nullptr), sdl_renderer(nullptr), sdl_wall_surface(nullptr),
      sdl_wall_texture(nullptr),
      selected_floor_texture_index(FLOOR_TEXTURE_DEFAULT_INDEX),
      sdl_goodie_surface(nullptr),
      sdl_goodie_texture(nullptr), sdl_logo_brick_surface(nullptr),
      sdl_dynamite_texture(nullptr), sdl_dynamite_size{0, 0},
      sdl_nuclear_crater_texture(nullptr), sdl_nuclear_crater_size{0, 0},
      sdl_nuclear_bomb_texture(nullptr), sdl_nuclear_bomb_size{0, 0},
      sdl_nuclear_target_marker_texture(nullptr),
      sdl_nuclear_target_marker_size{0, 0},
      sdl_start_menu_monster_texture(nullptr), sdl_start_menu_monster_size{0, 0},
      sdl_start_menu_hero_texture(nullptr), sdl_start_menu_hero_size{0, 0},
      sdl_win_screen_texture(nullptr), sdl_win_screen_size{0, 0},
      sdl_lose_screen_texture(nullptr), sdl_lose_screen_size{0, 0},
      sdl_walkie_talkie_texture(nullptr), sdl_walkie_talkie_size{0, 0},
      sdl_rocket_texture(nullptr), sdl_rocket_size{0, 0},
      sdl_airstrike_plane_texture(nullptr), sdl_airstrike_plane_size{0, 0},
      sdl_airstrike_explosion_texture(nullptr), sdl_airstrike_explosion_size{0, 0},
      sdl_monster_explosion_texture(nullptr), sdl_monster_explosion_size{0, 0},
      sdl_fart_cloud_texture(nullptr), sdl_fart_cloud_size{0, 0},
      sdl_slime_ball_texture(nullptr), sdl_slime_ball_size{0, 0},
      sdl_slime_overlay_texture(nullptr), sdl_slime_overlay_size{0, 0},
      sdl_slime_splash_texture(nullptr), sdl_slime_splash_size{0, 0},
      sdl_font_hud(nullptr), sdl_font_menu(nullptr), sdl_font_logo(nullptr),
      sdl_font_display(nullptr), sdl_font_color{255, 255, 255, 255},
      sdl_font_back_color{COLOR_BACK, 255}, texW(0), texH(0) {
  initializeRenderer(map->get_map_rows(), map->get_map_cols());
}

Renderer::Renderer(size_t row_count, size_t col_count)
    : screen_res_x(0), screen_res_y(0), element_size(0), hud_top_y(0),
      offset_x(0), offset_y(0), scene_origin_y(0), wall_height_px(0),
      cos_tilt(1.0), sin_tilt(0.0), rows(0), cols(0), map(nullptr),
      game(nullptr),
      sdl_window(nullptr), sdl_renderer(nullptr), sdl_wall_surface(nullptr),
      sdl_wall_texture(nullptr),
      selected_floor_texture_index(FLOOR_TEXTURE_DEFAULT_INDEX),
      sdl_goodie_surface(nullptr),
      sdl_goodie_texture(nullptr), sdl_logo_brick_surface(nullptr),
      sdl_dynamite_texture(nullptr), sdl_dynamite_size{0, 0},
      sdl_nuclear_crater_texture(nullptr), sdl_nuclear_crater_size{0, 0},
      sdl_nuclear_bomb_texture(nullptr), sdl_nuclear_bomb_size{0, 0},
      sdl_nuclear_target_marker_texture(nullptr),
      sdl_nuclear_target_marker_size{0, 0},
      sdl_start_menu_monster_texture(nullptr), sdl_start_menu_monster_size{0, 0},
      sdl_start_menu_hero_texture(nullptr), sdl_start_menu_hero_size{0, 0},
      sdl_win_screen_texture(nullptr), sdl_win_screen_size{0, 0},
      sdl_lose_screen_texture(nullptr), sdl_lose_screen_size{0, 0},
      sdl_walkie_talkie_texture(nullptr), sdl_walkie_talkie_size{0, 0},
      sdl_rocket_texture(nullptr), sdl_rocket_size{0, 0},
      sdl_airstrike_plane_texture(nullptr), sdl_airstrike_plane_size{0, 0},
      sdl_airstrike_explosion_texture(nullptr), sdl_airstrike_explosion_size{0, 0},
      sdl_monster_explosion_texture(nullptr), sdl_monster_explosion_size{0, 0},
      sdl_fart_cloud_texture(nullptr), sdl_fart_cloud_size{0, 0},
      sdl_slime_ball_texture(nullptr), sdl_slime_ball_size{0, 0},
      sdl_slime_overlay_texture(nullptr), sdl_slime_overlay_size{0, 0},
      sdl_slime_splash_texture(nullptr), sdl_slime_splash_size{0, 0},
      sdl_font_hud(nullptr), sdl_font_menu(nullptr), sdl_font_logo(nullptr),
      sdl_font_display(nullptr), sdl_font_color{255, 255, 255, 255},
      sdl_font_back_color{COLOR_BACK, 255}, texW(0), texH(0) {
  initializeRenderer(row_count, col_count);
}

void Renderer::initializeRenderer(size_t row_count_value,
                                  size_t col_count_value) {
  if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0 &&
      SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL video could not initialize.\n";
    std::cerr << "SDL_Error: " << SDL_GetError() << "\n";
    exit(1);
  }

  if (TTF_Init() < 0) {
    std::cerr << "SDL_ttf could not initialize.\n";
    std::cerr << "SDL_Error: " << TTF_GetError() << "\n";
    exit(1);
  }

  const int image_flags = IMG_INIT_PNG | IMG_INIT_JPG;
  if ((IMG_Init(image_flags) & image_flags) != image_flags) {
    std::cerr << "SDL_image PNG/JPG support not fully available: "
              << IMG_GetError() << "\n";
  }

  SDL_DisplayMode display_mode;
  SDL_GetCurrentDisplayMode(0, &display_mode);
  screen_res_x = display_mode.w;
  screen_res_y = display_mode.h;
  updateSceneLayout(row_count_value, col_count_value);

  const Uint32 window_flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
  sdl_window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, display_mode.w,
                                display_mode.h, window_flags);
  if (sdl_window == nullptr) {
    std::cerr << "Window could not be created.\n";
    std::cerr << "SDL_Error: " << SDL_GetError() << "\n";
    exit(1);
  }

  std::string app_icon_path = Paths::GetDataFilePath("app_icon_rgba.png");
  SDL_Surface *app_icon_surface = IMG_Load(app_icon_path.c_str());
  if (app_icon_surface == nullptr) {
    app_icon_path = Paths::GetDataFilePath("app_icon.png");
    app_icon_surface = IMG_Load(app_icon_path.c_str());
  }
  if (app_icon_surface != nullptr) {
    SDL_SetWindowIcon(sdl_window, app_icon_surface);
    SDL_FreeSurface(app_icon_surface);
  }

  SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "2",
                          SDL_HINT_OVERRIDE);
  sdl_renderer = SDL_CreateRenderer(
      sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (sdl_renderer == nullptr) {
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (sdl_renderer == nullptr) {
    std::cerr << "Renderer could not be created.\n";
    std::cerr << "SDL_Error: " << SDL_GetError() << "\n";
    exit(1);
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(sdl_renderer, COLOR_BACK, 255);
  SDL_RenderClear(sdl_renderer);

  recreateSupersampleTarget();
  createSoftPuffTexture();

  const std::string font_path = Paths::GetDataFilePath("font.ttf");
  const int hud_fontsize = std::max(22, screen_res_y / 35);
  sdl_font_hud = TTF_OpenFont(font_path.c_str(), hud_fontsize);
  sdl_font_menu =
      TTF_OpenFont(font_path.c_str(), std::max(30, screen_res_y / 22));
  sdl_font_logo =
      TTF_OpenFont(font_path.c_str(), std::max(84, screen_res_y / 5));
  sdl_font_display =
      TTF_OpenFont(font_path.c_str(), std::max(72, screen_res_y / 7));

  if (sdl_font_hud == nullptr || sdl_font_menu == nullptr ||
      sdl_font_logo == nullptr || sdl_font_display == nullptr) {
    std::cerr << "Could not open font: " << TTF_GetError() << "\n";
    exit(1);
  }

  sdl_font_color = kHudTextColor;

  const std::string wall_path = Paths::GetDataFilePath("brick.bmp");
  const std::string goodie_path = Paths::GetDataFilePath("goodie.bmp");

  sdl_wall_surface = SDL_LoadBMP(wall_path.c_str());
  sdl_goodie_surface = SDL_LoadBMP(goodie_path.c_str());

  for (const auto &monster_descriptor : kMonsterSpriteDescriptors) {
    for (Directions direction :
         {Directions::Down, Directions::Up, Directions::Left,
          Directions::Right}) {
      for (int frame = 1; frame <= kMonsterFramesPerDirection; ++frame) {
        const std::string relative_path =
            "monster_frames/monster_" +
            std::string(monster_descriptor.color_name) + "_" +
            MonsterDirectionName(direction) + "_f" + std::to_string(frame) +
            "_256.png";
        const std::string monster_frame_path =
            Paths::GetDataFilePath(relative_path);
        SDL_Surface *monster_surface = IMG_Load(monster_frame_path.c_str());
        if (monster_surface == nullptr) {
          std::cerr << "Could not open monster sprite asset "
                    << monster_frame_path << ": " << IMG_GetError() << "\n";
          exit(1);
        }
        sdl_monster_surfaces.push_back(monster_surface);
      }
    }
  }
  for (Directions direction : {Directions::Down, Directions::Up,
                              Directions::Left, Directions::Right}) {
    for (int frame = 1; frame <= kMonsterFramesPerDirection; ++frame) {
      const std::string relative_path =
          "monster_frames/monster_goat_grazing_" +
          std::string(MonsterDirectionName(direction)) + "_f" +
          std::to_string(frame) + "_256.png";
      const std::string goat_frame_path = Paths::GetDataFilePath(relative_path);
      SDL_Surface *goat_surface = IMG_Load(goat_frame_path.c_str());
      if (goat_surface == nullptr) {
        std::cerr << "Could not open goat grazing sprite asset "
                  << goat_frame_path << ": " << IMG_GetError() << "\n";
        exit(1);
      }
      sdl_goat_grazing_surfaces.push_back(goat_surface);
    }
  }
  for (Directions direction : {Directions::Down, Directions::Up,
                              Directions::Left, Directions::Right}) {
    for (int frame = 1; frame <= kMonsterFramesPerDirection; ++frame) {
      const std::string relative_path =
          "monster_frames/monster_goat_jumping_" +
          std::string(MonsterDirectionName(direction)) + "_f" +
          std::to_string(frame) + "_256.png";
      const std::string goat_frame_path = Paths::GetDataFilePath(relative_path);
      SDL_Surface *goat_surface = IMG_Load(goat_frame_path.c_str());
      if (goat_surface == nullptr) {
        std::cerr << "Could not open goat jumping sprite asset "
                  << goat_frame_path << ": " << IMG_GetError() << "\n";
        exit(1);
      }
      sdl_goat_jumping_surfaces.push_back(goat_surface);
    }
  }

  const std::vector<std::string> pacman_frame_paths{
      Paths::GetDataFilePath("pacman_frames/down_0.png"),
      Paths::GetDataFilePath("pacman_frames/down_1.png"),
      Paths::GetDataFilePath("pacman_frames/down_2.png"),
      Paths::GetDataFilePath("pacman_frames/down_3.png"),
      Paths::GetDataFilePath("pacman_frames/up_0.png"),
      Paths::GetDataFilePath("pacman_frames/up_1.png"),
      Paths::GetDataFilePath("pacman_frames/up_2.png"),
      Paths::GetDataFilePath("pacman_frames/up_3.png"),
      Paths::GetDataFilePath("pacman_frames/left_0.png"),
      Paths::GetDataFilePath("pacman_frames/left_1.png"),
      Paths::GetDataFilePath("pacman_frames/left_2.png"),
      Paths::GetDataFilePath("pacman_frames/left_3.png"),
      Paths::GetDataFilePath("pacman_frames/right_0.png"),
      Paths::GetDataFilePath("pacman_frames/right_1.png"),
      Paths::GetDataFilePath("pacman_frames/right_2.png"),
      Paths::GetDataFilePath("pacman_frames/right_3.png")};
  for (const std::string &pacman_frame_path : pacman_frame_paths) {
    SDL_Surface *pacman_surface = IMG_Load(pacman_frame_path.c_str());
    if (pacman_surface == nullptr) {
      std::cerr << "Could not open pacman sprite asset "
                << pacman_frame_path << ": " << IMG_GetError() << "\n";
      exit(1);
    }
    sdl_pacman_surfaces.push_back(pacman_surface);
  }

  if (sdl_goodie_surface == nullptr || sdl_wall_surface == nullptr) {
    std::cerr << "Could not open sprite assets: " << SDL_GetError() << "\n";
    exit(1);
  }

  sdl_wall_texture =
      SDL_CreateTextureFromSurface(sdl_renderer, sdl_wall_surface);
  configureSmoothTextureSampling(sdl_wall_texture);
  sdl_floor_textures.clear();
  sdl_floor_texture_sizes.clear();
  sdl_floor_textures.reserve(FLOOR_TEXTURE_ASSET_PATHS.size());
  sdl_floor_texture_sizes.reserve(FLOOR_TEXTURE_ASSET_PATHS.size());
  for (const char *relative_path : FLOOR_TEXTURE_ASSET_PATHS) {
    SDL_Point floor_texture_size{0, 0};
    SDL_Texture *floor_texture = loadTexturePreserveCanvas(
        Paths::GetDataFilePath(relative_path), floor_texture_size);
    if (floor_texture == nullptr) {
      std::cerr << "Could not create floor texture from asset "
                << relative_path << "\n";
      exit(1);
    }
    sdl_floor_textures.push_back(floor_texture);
    sdl_floor_texture_sizes.push_back(floor_texture_size);
  }
  sdl_goodie_texture =
      SDL_CreateTextureFromSurface(sdl_renderer, sdl_goodie_surface);
  configureSmoothTextureSampling(sdl_goodie_texture);
  for (SDL_Surface *pacman_surface : sdl_pacman_surfaces) {
    SDL_Texture *pacman_texture =
        SDL_CreateTextureFromSurface(sdl_renderer, pacman_surface);
    if (pacman_texture == nullptr) {
      std::cerr << "Could not create pacman texture: " << SDL_GetError()
                << "\n";
      exit(1);
    }
    configureSmoothTextureSampling(pacman_texture);
    sdl_pacman_textures.push_back(pacman_texture);
  }
  for (SDL_Surface *monster_surface : sdl_monster_surfaces) {
    SDL_Texture *monster_texture =
        SDL_CreateTextureFromSurface(sdl_renderer, monster_surface);
    if (monster_texture == nullptr) {
      std::cerr << "Could not create monster texture: " << SDL_GetError()
                << "\n";
      exit(1);
    }
    configureSmoothTextureSampling(monster_texture);
    sdl_monster_textures.push_back(monster_texture);
  }
  for (SDL_Surface *goat_surface : sdl_goat_grazing_surfaces) {
    SDL_Texture *goat_texture =
        SDL_CreateTextureFromSurface(sdl_renderer, goat_surface);
    if (goat_texture == nullptr) {
      std::cerr << "Could not create goat grazing texture: " << SDL_GetError()
                << "\n";
      exit(1);
    }
    configureSmoothTextureSampling(goat_texture);
    sdl_goat_grazing_textures.push_back(goat_texture);
  }
  for (SDL_Surface *goat_surface : sdl_goat_jumping_surfaces) {
    SDL_Texture *goat_texture =
        SDL_CreateTextureFromSurface(sdl_renderer, goat_surface);
    if (goat_texture == nullptr) {
      std::cerr << "Could not create goat jumping texture: " << SDL_GetError()
                << "\n";
      exit(1);
    }
    configureSmoothTextureSampling(goat_texture);
    sdl_goat_jumping_textures.push_back(goat_texture);
  }
  sdl_alien_textures.clear();
  sdl_alien_sizes.clear();
  sdl_alien_textures.reserve(ALIEN_FRAME_ASSET_PATHS.size());
  sdl_alien_sizes.reserve(ALIEN_FRAME_ASSET_PATHS.size());
  for (const char *relative_path : ALIEN_FRAME_ASSET_PATHS) {
    SDL_Point alien_size{0, 0};
    SDL_Texture *alien_texture = loadTrimmedChromaKeyTexture(
        Paths::GetDataFilePath(relative_path), alien_size,
        ALIEN_CHROMA_KEY_TOLERANCE);
    if (alien_texture == nullptr) {
      std::cerr << "Could not load alien sprite asset " << relative_path
                << "\n";
      exit(1);
    }
    sdl_alien_textures.push_back(alien_texture);
    sdl_alien_sizes.push_back(alien_size);
  }
  sdl_alien_explosion_textures.clear();
  sdl_alien_explosion_sizes.clear();
  sdl_alien_explosion_textures.reserve(
      ALIEN_EXPLOSION_FRAME_ASSET_PATHS.size());
  sdl_alien_explosion_sizes.reserve(ALIEN_EXPLOSION_FRAME_ASSET_PATHS.size());
  for (const char *relative_path : ALIEN_EXPLOSION_FRAME_ASSET_PATHS) {
    SDL_Point explosion_size{0, 0};
    SDL_Texture *explosion_texture = loadTrimmedChromaKeyTexture(
        Paths::GetDataFilePath(relative_path), explosion_size,
        ALIEN_CHROMA_KEY_TOLERANCE);
    if (explosion_texture == nullptr) {
      std::cerr << "Could not load alien explosion asset " << relative_path
                << "\n";
      exit(1);
    }
    sdl_alien_explosion_textures.push_back(explosion_texture);
    sdl_alien_explosion_sizes.push_back(explosion_size);
  }

  const std::string logo_brick_path = Paths::GetDataFilePath("brick.png");
  SDL_Surface *brick_surface = IMG_Load(logo_brick_path.c_str());
  if (brick_surface == nullptr) {
    brick_surface = SDL_LoadBMP(wall_path.c_str());
  }
  if (brick_surface == nullptr) {
    std::cerr << "Could not open brick texture for logo: " << IMG_GetError()
              << "\n";
    exit(1);
  }
  sdl_logo_brick_surface =
      SDL_ConvertSurfaceFormat(brick_surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(brick_surface);

  if (sdl_logo_brick_surface == nullptr) {
    std::cerr << "Could not convert brick texture: " << SDL_GetError() << "\n";
    exit(1);
  }

  sdl_start_menu_monster_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("start_menu_monster.png"),
      sdl_start_menu_monster_size);
  sdl_start_menu_hero_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("start_menu_spielfigur.png"),
      sdl_start_menu_hero_size);
  sdl_win_screen_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("win_screen_triumph.png"), sdl_win_screen_size);
  sdl_lose_screen_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("lose_screen_defeat.png"), sdl_lose_screen_size);
  sdl_walkie_talkie_texture = loadTrimmedChromaKeyTexture(
      Paths::GetDataFilePath("walkie_talkie.png"), sdl_walkie_talkie_size, 26);
  sdl_rocket_texture =
      loadTrimmedTexture(Paths::GetDataFilePath("rocket.png"), sdl_rocket_size);
  sdl_rocket_flight_textures.clear();
  sdl_rocket_flight_sizes.clear();
  sdl_rocket_flight_textures.reserve(kRocketFlightFrames);
  sdl_rocket_flight_sizes.reserve(kRocketFlightFrames);
  for (int frame_index = 1; frame_index <= kRocketFlightFrames; ++frame_index) {
    const std::string rocket_frame_path =
        Paths::GetDataFilePath("rocket_flight_" + std::to_string(frame_index) +
                               ".png");
    SDL_Point rocket_frame_size{0, 0};
    SDL_Texture *rocket_frame_texture =
        loadTrimmedTexture(rocket_frame_path, rocket_frame_size);
    if (rocket_frame_texture == nullptr) {
      std::cerr << "Could not create rocket sprite texture: " << SDL_GetError()
                << "\n";
      exit(1);
    }
    sdl_rocket_flight_sizes.push_back(rocket_frame_size);
    sdl_rocket_flight_textures.push_back(rocket_frame_texture);
  }
  sdl_airstrike_plane_texture = loadTrimmedChromaKeyTexture(
      Paths::GetDataFilePath("airstrike_plane.png"), sdl_airstrike_plane_size, 26);
  sdl_airstrike_explosion_texture = loadTrimmedChromaKeyTexture(
      Paths::GetDataFilePath("airstrike_explosion_sheet.png"),
      sdl_airstrike_explosion_size, 12);
  sdl_monster_explosion_texture = loadTexturePreserveCanvas(
      Paths::GetDataFilePath(MONSTER_EXPLOSION_SHEET_ASSET_PATH),
      sdl_monster_explosion_size);
  sdl_fart_cloud_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("fart_cloud_sheet.png"), sdl_fart_cloud_size);
  sdl_slime_ball_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("slime_ball.png"), sdl_slime_ball_size);
  sdl_slime_overlay_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("slime_overlay.png"), sdl_slime_overlay_size);
  sdl_slime_splash_texture = loadTrimmedTexture(
      Paths::GetDataFilePath("slime_splash_sheet.png"), sdl_slime_splash_size);
  sdl_dynamite_texture = loadTrimmedTexture(Paths::GetDataFilePath("dynamite.png"),
                                            sdl_dynamite_size);
  sdl_nuclear_bomb_texture = loadTrimmedChromaKeyTexture(
      Paths::GetDataFilePath(NUCLEAR_BOMB_ASSET_PATH), sdl_nuclear_bomb_size,
      42);
  configureSmoothTextureSampling(sdl_nuclear_bomb_texture);
  sdl_nuclear_target_marker_texture = loadTrimmedTexture(
      Paths::GetDataFilePath(NUCLEAR_TARGET_MARKER_ASSET_PATH),
      sdl_nuclear_target_marker_size);
  configureSmoothTextureSampling(sdl_nuclear_target_marker_texture);
  sdl_nuclear_crater_texture = loadTexturePreserveCanvas(
      Paths::GetDataFilePath(NUCLEAR_CRATER_ASSET_PATH),
      sdl_nuclear_crater_size);
  configureSmoothTextureSampling(sdl_nuclear_crater_texture);
  for (size_t index = 0; index < sdl_hud_keycap_textures.size(); ++index) {
    sdl_hud_keycap_sizes[index] = SDL_Point{0, 0};
    sdl_hud_keycap_textures[index] = loadTexturePreserveCanvas(
        Paths::GetDataFilePath(HUD_KEYCAP_ASSET_PATHS[index]),
        sdl_hud_keycap_sizes[index]);
    if (sdl_hud_keycap_textures[index] == nullptr) {
      std::cerr << "Could not load HUD keycap asset "
                << HUD_KEYCAP_ASSET_PATHS[index] << ": " << IMG_GetError()
                << "\n";
    }
  }
}

Renderer::~Renderer() {
  if (sdl_wall_texture != nullptr) {
    SDL_DestroyTexture(sdl_wall_texture);
  }
  for (SDL_Texture *floor_texture : sdl_floor_textures) {
    if (floor_texture != nullptr) {
      SDL_DestroyTexture(floor_texture);
    }
  }
  if (sdl_goodie_texture != nullptr) {
    SDL_DestroyTexture(sdl_goodie_texture);
  }
  for (SDL_Texture *monster_texture : sdl_monster_textures) {
    if (monster_texture != nullptr) {
      SDL_DestroyTexture(monster_texture);
    }
  }
  for (SDL_Texture *goat_texture : sdl_goat_grazing_textures) {
    if (goat_texture != nullptr) {
      SDL_DestroyTexture(goat_texture);
    }
  }
  for (SDL_Texture *goat_texture : sdl_goat_jumping_textures) {
    if (goat_texture != nullptr) {
      SDL_DestroyTexture(goat_texture);
    }
  }
  for (SDL_Texture *alien_texture : sdl_alien_textures) {
    if (alien_texture != nullptr) {
      SDL_DestroyTexture(alien_texture);
    }
  }
  for (SDL_Texture *alien_explosion_texture : sdl_alien_explosion_textures) {
    if (alien_explosion_texture != nullptr) {
      SDL_DestroyTexture(alien_explosion_texture);
    }
  }
  for (SDL_Texture *pacman_texture : sdl_pacman_textures) {
    if (pacman_texture != nullptr) {
      SDL_DestroyTexture(pacman_texture);
    }
  }
  if (sdl_start_menu_monster_texture != nullptr) {
    SDL_DestroyTexture(sdl_start_menu_monster_texture);
  }
  if (sdl_start_menu_hero_texture != nullptr) {
    SDL_DestroyTexture(sdl_start_menu_hero_texture);
  }
  if (sdl_win_screen_texture != nullptr) {
    SDL_DestroyTexture(sdl_win_screen_texture);
  }
  if (sdl_lose_screen_texture != nullptr) {
    SDL_DestroyTexture(sdl_lose_screen_texture);
  }
  if (sdl_walkie_talkie_texture != nullptr) {
    SDL_DestroyTexture(sdl_walkie_talkie_texture);
  }
  if (sdl_rocket_texture != nullptr) {
    SDL_DestroyTexture(sdl_rocket_texture);
  }
  for (SDL_Texture *rocket_flight_texture : sdl_rocket_flight_textures) {
    if (rocket_flight_texture != nullptr) {
      SDL_DestroyTexture(rocket_flight_texture);
    }
  }
  if (sdl_airstrike_plane_texture != nullptr) {
    SDL_DestroyTexture(sdl_airstrike_plane_texture);
  }
  if (sdl_airstrike_explosion_texture != nullptr) {
    SDL_DestroyTexture(sdl_airstrike_explosion_texture);
  }
  if (sdl_monster_explosion_texture != nullptr) {
    SDL_DestroyTexture(sdl_monster_explosion_texture);
  }
  if (sdl_fart_cloud_texture != nullptr) {
    SDL_DestroyTexture(sdl_fart_cloud_texture);
  }
  if (sdl_slime_ball_texture != nullptr) {
    SDL_DestroyTexture(sdl_slime_ball_texture);
  }
  if (sdl_slime_overlay_texture != nullptr) {
    SDL_DestroyTexture(sdl_slime_overlay_texture);
  }
  if (sdl_slime_splash_texture != nullptr) {
    SDL_DestroyTexture(sdl_slime_splash_texture);
  }
  if (sdl_dynamite_texture != nullptr) {
    SDL_DestroyTexture(sdl_dynamite_texture);
  }
  if (sdl_nuclear_crater_texture != nullptr) {
    SDL_DestroyTexture(sdl_nuclear_crater_texture);
  }
  if (sdl_nuclear_bomb_texture != nullptr) {
    SDL_DestroyTexture(sdl_nuclear_bomb_texture);
  }
  if (sdl_nuclear_target_marker_texture != nullptr) {
    SDL_DestroyTexture(sdl_nuclear_target_marker_texture);
  }
  for (SDL_Texture *keycap_texture : sdl_hud_keycap_textures) {
    if (keycap_texture != nullptr) {
      SDL_DestroyTexture(keycap_texture);
    }
  }
  if (sdl_supersample_target != nullptr) {
    SDL_DestroyTexture(sdl_supersample_target);
    sdl_supersample_target = nullptr;
  }
  if (sdl_soft_puff_texture != nullptr) {
    SDL_DestroyTexture(sdl_soft_puff_texture);
    sdl_soft_puff_texture = nullptr;
  }
  if (sdl_solid_disc_texture != nullptr) {
    SDL_DestroyTexture(sdl_solid_disc_texture);
    sdl_solid_disc_texture = nullptr;
  }
  if (sdl_glass_block_base_texture != nullptr) {
    SDL_DestroyTexture(sdl_glass_block_base_texture);
    sdl_glass_block_base_texture = nullptr;
  }
  if (sdl_glass_block_glow_texture != nullptr) {
    SDL_DestroyTexture(sdl_glass_block_glow_texture);
    sdl_glass_block_glow_texture = nullptr;
  }
  if (sdl_renderer != nullptr) {
    SDL_DestroyRenderer(sdl_renderer);
  }
  if (sdl_window != nullptr) {
    SDL_DestroyWindow(sdl_window);
  }

  if (sdl_wall_surface != nullptr) {
    SDL_FreeSurface(sdl_wall_surface);
  }
  if (sdl_goodie_surface != nullptr) {
    SDL_FreeSurface(sdl_goodie_surface);
  }
  for (SDL_Surface *monster_surface : sdl_monster_surfaces) {
    if (monster_surface != nullptr) {
      SDL_FreeSurface(monster_surface);
    }
  }
  for (SDL_Surface *goat_surface : sdl_goat_grazing_surfaces) {
    if (goat_surface != nullptr) {
      SDL_FreeSurface(goat_surface);
    }
  }
  for (SDL_Surface *goat_surface : sdl_goat_jumping_surfaces) {
    if (goat_surface != nullptr) {
      SDL_FreeSurface(goat_surface);
    }
  }
  for (SDL_Surface *pacman_surface : sdl_pacman_surfaces) {
    if (pacman_surface != nullptr) {
      SDL_FreeSurface(pacman_surface);
    }
  }
  if (sdl_logo_brick_surface != nullptr) {
    SDL_FreeSurface(sdl_logo_brick_surface);
  }

  if (sdl_font_hud != nullptr) {
    TTF_CloseFont(sdl_font_hud);
  }
  if (sdl_font_menu != nullptr) {
    TTF_CloseFont(sdl_font_menu);
  }
  if (sdl_font_logo != nullptr) {
    TTF_CloseFont(sdl_font_logo);
  }
  if (sdl_font_display != nullptr) {
    TTF_CloseFont(sdl_font_display);
  }

  IMG_Quit();
  TTF_Quit();
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Renderer::Render(bool show_pause_overlay, bool show_exit_dialog,
                      int exit_dialog_selected) {
  beginSupersampledFrame();
  renderFrame(true);
  drawDiscoEasterEggOverlay(SDL_GetTicks());
  if (show_pause_overlay || show_exit_dialog) {
    drawGameplayPauseOverlay(show_exit_dialog, exit_dialog_selected);
  }
  if (frame_stats_overlay_enabled_) {
    drawFrameStatsOverlay();
  }
  endSupersampledFrameAndPresent();
}

void Renderer::SetFrameStatsOverlay(bool enabled,
                                    const FrameTimer::Stats *stats) {
  frame_stats_overlay_enabled_ = enabled;
  frame_stats_ = stats;
}

void Renderer::drawFrameStatsOverlay() {
  if (frame_stats_ == nullptr || sdl_font_hud == nullptr) {
    return;
  }
  const FrameTimer::Stats &s = *frame_stats_;
  char line1[96];
  char line2[96];
  std::snprintf(line1, sizeof(line1), "FPS %5.1f  avg %5.2fms",
                s.fps, s.avg_ms);
  std::snprintf(line2, sizeof(line2),
                "p50 %ums p95 %ums p99 %ums max %ums  n=%zu",
                s.p50_ms, s.p95_ms, s.p99_ms, s.max_ms, s.window_frames);
  const int line_height = TTF_FontHeight(sdl_font_hud) + 2;
  int line_w1 = 0, line_w2 = 0;
  TTF_SizeUTF8(sdl_font_hud, line1, &line_w1, nullptr);
  TTF_SizeUTF8(sdl_font_hud, line2, &line_w2, nullptr);
  const int box_w = std::max(line_w1, line_w2) + 16;
  const int box_h = line_height * 2 + 12;
  const int box_x = 10;
  const int box_y = 10;
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 180);
  SDL_Rect bg{box_x, box_y, box_w, box_h};
  SDL_RenderFillRect(sdl_renderer, &bg);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, 180);
  SDL_RenderDrawRect(sdl_renderer, &bg);
  const SDL_Color text_color{255, 255, 0, 255};
  renderTextLeft(sdl_font_hud, line1, text_color, box_x + 8, box_y + 6);
  renderTextLeft(sdl_font_hud, line2, text_color, box_x + 8,
                 box_y + 6 + line_height);
}

void Renderer::RenderStartMenu(int selected_item, const std::string &map_name,
                               const std::string &status_message) {
  beginSupersampledFrame();
  renderFrame(false);
  drawDimmer(120);
  drawStartMenuOverlay(selected_item, map_name, status_message);
  endSupersampledFrameAndPresent();
}

void Renderer::RenderConfigMenu(int selected_item, Difficulty difficulty,
                                int player_lives, int floor_texture_index) {
  beginSupersampledFrame();
  renderFrame(false);
  drawDimmer(120);
  drawConfigMenuOverlay(selected_item, difficulty, player_lives,
                        floor_texture_index);
  endSupersampledFrameAndPresent();
}

void Renderer::RenderMapSelectionMenu(const std::vector<std::string> &map_names,
                                      int selected_index) {
  beginSupersampledFrame();
  renderFrame(false);
  drawDimmer(120);
  drawMapSelectionOverlay(map_names, selected_index);
  endSupersampledFrameAndPresent();
}

void Renderer::RenderEditorSelectionMenu(const std::vector<std::string> &items,
                                         int selected_index) {
  beginSupersampledFrame();
  renderFrame(false);
  drawDimmer(120);
  drawEditorSelectionOverlay(items, selected_index);
  endSupersampledFrameAndPresent();
}

void Renderer::RenderEditorSizeSelectionMenu(int selected_index) {
  beginSupersampledFrame();
  renderFrame(false);
  drawDimmer(120);
  drawEditorSizeSelectionOverlay(selected_index);
  endSupersampledFrameAndPresent();
}

void Renderer::RenderEditor(const std::vector<std::string> &layout,
                            const std::string &map_name, MapCoord cursor,
                            const std::string &warning_message,
                            bool show_exit_dialog, int exit_dialog_selected,
                            bool show_name_dialog,
                            const std::string &name_input,
                            const std::string &name_dialog_message) {
  beginSupersampledFrame();
  renderLayoutFrame(layout);
  drawDimmer(20);
  drawEditorOverlay(map_name, cursor, warning_message, show_exit_dialog,
                    exit_dialog_selected, show_name_dialog, name_input,
                    name_dialog_message);
  endSupersampledFrameAndPresent();
}

void Renderer::RenderCountdown(int seconds_left) {
  beginSupersampledFrame();
  renderFrame(false);
  drawDimmer(145);
  drawCountdownOverlay(seconds_left);
  endSupersampledFrameAndPresent();
}

bool Renderer::TryGetLayoutCoordFromScreen(int screen_x, int screen_y,
                                           MapCoord &coord) const {
  const int grid_origin_x = offset_x + 1;
  const int grid_origin_y = offset_y + 1;
  const int cell_stride = element_size + 1;
  const int row_count = static_cast<int>(rows);
  const int col_count = static_cast<int>(cols);
  const int grid_width = col_count * cell_stride;
  const int grid_height = row_count * cell_stride;
  const int relative_x = screen_x - grid_origin_x;
  const int relative_y = screen_y - grid_origin_y;

  if (relative_x < 0 || relative_y < 0 || relative_x >= grid_width ||
      relative_y >= grid_height) {
    return false;
  }

  const int col =
      std::clamp(relative_x / cell_stride, 0, col_count - 1);
  const int row =
      std::clamp(relative_y / cell_stride, 0, row_count - 1);

  coord = {row, col};
  return true;
}

void Renderer::renderFrame(bool show_hud) {
  SDL_SetRenderDrawColor(sdl_renderer, COLOR_BACK, 255);
  SDL_RenderClear(sdl_renderer);
  if (ENABLE_3D_VIEW) {
    drawmap3DFloors();
    drawWallRubble();
    drawNuclearCraters();
    drawmap3DWallTops();

    if (map != nullptr) {
      auto &depth_commands = depth_commands_;
      depth_commands.clear();
      if (depth_commands.capacity() < rows * cols + 64) {
        depth_commands.reserve(rows * cols + 64);
      }
      const double tile_span = static_cast<double>(element_size) + 1.0;
      const auto pixel_delta_to_cells = [&](int delta_percent) {
        const double delta_pixels =
            static_cast<double>(element_size * delta_percent) / 100.0;
        return delta_pixels / tile_span;
      };
      auto push_depth_command = [&](double depth, InlineCallable<192> draw) {
        DepthDrawCommand cmd;
        cmd.depth = static_cast<float>(depth);
        cmd.order = depth_commands.size();
        cmd.draw = std::move(draw);
        depth_commands.push_back(std::move(cmd));
      };
      rebuildWallOcclusionLookupIfNeeded();
      const auto wall_occlusion_top_y = [&](double col_cells,
                                            double row_cells) -> int {
        const int column =
            std::clamp(static_cast<int>(std::floor(col_cells)), 0,
                       static_cast<int>(cols) - 1);
        const int search_start_row =
            std::clamp(static_cast<int>(std::floor(row_cells)) + 1, 0,
                       static_cast<int>(rows));
        const size_t idx = static_cast<size_t>(search_start_row) * cols +
                           static_cast<size_t>(column);
        return wall_top_y_lookup_[idx];
      };
      const auto draw_with_wall_occlusion =
          [&](double col_cells, double row_cells,
              const std::function<void()> &draw) {
            const int clip_top_y = wall_occlusion_top_y(col_cells, row_cells);
            if (clip_top_y <= 0) {
              return;
            }

            if (clip_top_y < screen_res_y) {
              const SDL_Rect clip_rect{0, 0, screen_res_x,
                                       std::clamp(clip_top_y, 0, screen_res_y)};
              SDL_RenderSetClipRect(sdl_renderer, &clip_rect);
              draw();
              SDL_RenderSetClipRect(sdl_renderer, nullptr);
              return;
            }

            draw();
          };
      const int non_character_sprite_lift_px =
          getNonCharacterSpriteLiftPixels();

      for (size_t teleporter_index = 0;
           teleporter_index < map->get_teleporter_pairs().size();
           teleporter_index++) {
        const auto &teleporter_pair = map->get_teleporter_pairs()[teleporter_index];
        const std::vector<MapCoord> positions{teleporter_pair.first,
                                              teleporter_pair.second};
        for (size_t position_index = 0; position_index < positions.size();
             position_index++) {
          const MapCoord position = positions[position_index];
          if (map->map_entry(static_cast<size_t>(position.u),
                             static_cast<size_t>(position.v)) !=
              ElementType::TYPE_TELEPORTER) {
            continue;
          }
          const SDL_Rect teleporter_rect = makeFloorAlignedRect(
              position.v, position.u, 0.08, 0.08, 0.84, 0.84);
          const double depth =
              projectScene(0.5, position.u + kFloorObjectDepthRowFactor, 0.0).y;
          push_depth_command(
              depth, [this, teleporter_rect, teleporter_pair, teleporter_index,
                      position_index, position, &draw_with_wall_occlusion]() {
                draw_with_wall_occlusion(
                    static_cast<double>(position.v) + 0.5,
                    static_cast<double>(position.u) + kFloorObjectDepthRowFactor,
                    [&]() {
                      drawTeleporterGlyph(
                          teleporter_rect, teleporter_pair.digit,
                          static_cast<int>(teleporter_index * 19 +
                                           position_index * 7));
                    });
              });
        }
      }

      if (game != nullptr) {
        const Uint32 now = SDL_GetTicks();
        const bool final_loss_sequence_active =
            game->IsFinalLossSequenceActive(now);

        for (auto goodie : game->goodies) {
          if (!goodie->is_active) {
            continue;
          }

          const MapCoord coord = goodie->map_coord;
          const SDL_Rect rect = offsetRectY(
              makeFloorAlignedRect(coord.v, coord.u, 0.15, 0.15, 0.7, 0.7),
              -non_character_sprite_lift_px);
          const double depth =
              projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
          push_depth_command(depth, [this, rect, coord, goodie, now,
                                     &draw_with_wall_occlusion]() {
            draw_with_wall_occlusion(
                static_cast<double>(coord.v) + 0.5,
                static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                [&]() {
                  if (sdl_goodie_texture != nullptr) {
                    SDL_RenderCopy(sdl_renderer, sdl_goodie_texture, nullptr,
                                   &rect);
                  }
                  renderGoodieSparkle(goodie, rect, now);
                });
          });
        }

        const auto &potion = game->invulnerability_potion;
        if (potion.is_visible) {
          double alpha_factor = 1.0;
          if (potion.is_fading) {
            alpha_factor =
                1.0 -
                std::clamp(static_cast<double>(now - potion.fade_started_ticks) /
                               static_cast<double>(BLUE_POTION_FADE_MS),
                           0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = potion.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, potion, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double wobble_clock =
                        static_cast<double>(now + potion.animation_seed * 53) /
                        260.0;
                    const double pulse = 0.76 + 0.24 * std::sin(wobble_clock);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(120.0 * alpha_factor, 0.0, 140.0));
                    const Uint8 glass_alpha = static_cast<Uint8>(
                        std::clamp(175.0 * alpha_factor, 0.0, 200.0));
                    const Uint8 fluid_alpha = static_cast<Uint8>(
                        std::clamp(210.0 * alpha_factor, 0.0, 230.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 72, 164, 255,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(6,
                                 static_cast<int>(element_size * 0.30 * pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 124, 214, 255,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(8,
                                 static_cast<int>(element_size * 0.38 * pulse)));

                    const int flask_width =
                        std::max(10, static_cast<int>(element_size * 0.34));
                    const int flask_body_height =
                        std::max(12, static_cast<int>(element_size * 0.36));
                    const int flask_neck_width =
                        std::max(5, static_cast<int>(flask_width * 0.42));
                    const int flask_neck_height =
                        std::max(5, static_cast<int>(element_size * 0.16));
                    const int flask_top = center_y - flask_body_height / 2;

                    SDL_Rect neck_rect{center_x - flask_neck_width / 2,
                                       flask_top - flask_neck_height / 2,
                                       flask_neck_width, flask_neck_height};
                    SDL_Rect body_rect{center_x - flask_width / 2, flask_top,
                                       flask_width, flask_body_height};

                    SDL_SetRenderDrawColor(sdl_renderer, 225, 245, 255,
                                           glass_alpha / 4);
                    SDL_RenderFillRect(sdl_renderer, &neck_rect);
                    SDL_RenderFillRect(sdl_renderer, &body_rect);
                    SDL_RenderFillCircle(sdl_renderer, center_x,
                                         body_rect.y + body_rect.h,
                                         std::max(5, flask_width / 2));

                    const int liquid_height =
                        std::max(5, static_cast<int>(flask_body_height *
                                                     (0.42 + 0.10 *
                                                                 std::sin(
                                                                     wobble_clock))));
                    SDL_Rect liquid_rect{
                        body_rect.x + 2,
                        body_rect.y + body_rect.h - liquid_height,
                        std::max(2, body_rect.w - 4), liquid_height};
                    SDL_SetRenderDrawColor(sdl_renderer, 44, 112, 255,
                                           fluid_alpha);
                    SDL_RenderFillRect(sdl_renderer, &liquid_rect);
                    SDL_SetRenderDrawColor(sdl_renderer, 118, 204, 255,
                                           fluid_alpha);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x,
                        body_rect.y + body_rect.h -
                            std::max(2, liquid_height / 3),
                        std::max(4, flask_width / 2 - 2));

                    for (int bubble = 0; bubble < 3; bubble++) {
                      const double bubble_phase =
                          wobble_clock * 1.15 + bubble * 1.7;
                      const double bubble_progress =
                          std::fmod(now / 900.0 + bubble * 0.31, 1.0);
                      const int bubble_x =
                          center_x + static_cast<int>(std::sin(bubble_phase) *
                                                      flask_width * 0.16);
                      const int bubble_y =
                          liquid_rect.y + liquid_rect.h -
                          static_cast<int>(bubble_progress *
                                           std::max(6, liquid_rect.h - 2));
                      const int bubble_radius =
                          std::max(2, element_size / 14 + bubble % 2);
                      SDL_SetRenderDrawColor(sdl_renderer, 220, 245, 255,
                                             glow_alpha);
                      SDL_RenderFillCircle(sdl_renderer, bubble_x, bubble_y,
                                           bubble_radius);
                    }

                    SDL_SetRenderDrawColor(sdl_renderer, 224, 246, 255,
                                           glass_alpha);
                    SDL_RenderDrawRect(sdl_renderer, &neck_rect);
                    SDL_RenderDrawRect(sdl_renderer, &body_rect);
                    SDL_RenderDrawLine(
                        sdl_renderer, body_rect.x, body_rect.y + body_rect.h,
                        center_x - flask_width / 4,
                        body_rect.y + body_rect.h + flask_width / 3);
                    SDL_RenderDrawLine(
                        sdl_renderer, body_rect.x + body_rect.w,
                        body_rect.y + body_rect.h, center_x + flask_width / 4,
                        body_rect.y + body_rect.h + flask_width / 3);
                    SDL_RenderDrawCircle(sdl_renderer, center_x,
                                         body_rect.y + body_rect.h,
                                         std::max(5, flask_width / 2));
                  });
            });
          }
        }

        const auto &dynamite = game->dynamite_pickup;
        if (dynamite.is_visible) {
          double alpha_factor = 1.0;
          if (dynamite.is_fading) {
            alpha_factor =
                1.0 -
                std::clamp(static_cast<double>(now - dynamite.fade_started_ticks) /
                               static_cast<double>(DYNAMITE_FADE_MS),
                           0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = dynamite.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, dynamite, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(now + dynamite.animation_seed * 41) /
                        180.0;
                    const double pulse = 0.80 + 0.20 * std::sin(animation_clock);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(96.0 * alpha_factor, 0.0, 140.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 255, 118, 24,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(7,
                                 static_cast<int>(element_size * 0.28 * pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 255, 202, 110,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(9,
                                 static_cast<int>(element_size * 0.38 * pulse)));

                    const int icon_size =
                        std::max(12, static_cast<int>(element_size * 0.74));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawDynamiteIcon(icon_rect, false, animation_clock,
                                     body_alpha, 0.0);
                  });
            });
          }
        }

        const auto &plastic_explosive = game->plastic_explosive_pickup;
        if (plastic_explosive.is_visible) {
          double alpha_factor = 1.0;
          if (plastic_explosive.is_fading) {
            alpha_factor = 1.0 - std::clamp(
                                      static_cast<double>(
                                          now - plastic_explosive.fade_started_ticks) /
                                          static_cast<double>(
                                              PLASTIC_EXPLOSIVE_FADE_MS),
                                      0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = plastic_explosive.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(
                depth, [&, coord, plastic_explosive, alpha_factor, now]() {
                  draw_with_wall_occlusion(
                      static_cast<double>(coord.v) + 0.5,
                      static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                      [&]() {
                        const SDL_FPoint center =
                            projectScene(coord.v + 0.5,
                                         coord.u + kFloorObjectDepthRowFactor,
                                         0.0);
                        const int center_x =
                            static_cast<int>(std::lround(center.x));
                        const int center_y =
                            static_cast<int>(std::lround(center.y)) -
                            non_character_sprite_lift_px;
                        const double animation_clock =
                            static_cast<double>(
                                now + plastic_explosive.animation_seed * 37) /
                            170.0;
                        const double pulse =
                            0.78 + 0.22 * std::sin(animation_clock);
                        const Uint8 glow_alpha = static_cast<Uint8>(
                            std::clamp(88.0 * alpha_factor, 0.0, 136.0));
                        const Uint8 body_alpha = static_cast<Uint8>(
                            std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                        SDL_SetRenderDrawColor(sdl_renderer, 84, 214, 255,
                                               glow_alpha / 2);
                        SDL_RenderFillCircle(
                            sdl_renderer, center_x, center_y,
                            std::max(7, static_cast<int>(element_size * 0.26 *
                                                         pulse)));
                        SDL_SetRenderDrawColor(sdl_renderer, 190, 244, 255,
                                               glow_alpha);
                        SDL_RenderDrawCircle(
                            sdl_renderer, center_x, center_y,
                            std::max(9, static_cast<int>(element_size * 0.36 *
                                                         pulse)));

                        const int icon_size =
                            std::max(12, static_cast<int>(element_size * 0.74));
                        const SDL_Rect icon_rect{
                            center_x - icon_size / 2,
                            center_y - icon_size / 2, icon_size, icon_size};
                        drawPlasticExplosiveIcon(icon_rect, animation_clock,
                                                 body_alpha, false);
                      });
                });
          }
        }

        const auto &walkie = game->walkie_talkie_pickup;
        if (walkie.is_visible) {
          double alpha_factor = 1.0;
          if (walkie.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(now - walkie.fade_started_ticks) /
                                     static_cast<double>(WALKIE_TALKIE_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = walkie.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, walkie, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(now + walkie.animation_seed * 29) /
                        210.0;
                    const double pulse = 0.82 + 0.18 * std::sin(animation_clock);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(104.0 * alpha_factor, 0.0, 150.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 80, 255, 116,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(7,
                                 static_cast<int>(element_size * 0.28 * pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 194, 255, 214,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(10,
                                 static_cast<int>(element_size * 0.40 * pulse)));

                    const int icon_size =
                        std::max(12, static_cast<int>(element_size * 0.76));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawWalkieTalkieIcon(icon_rect, body_alpha, animation_clock);
                  });
            });
          }
        }

        const auto &rocket = game->rocket_pickup;
        if (rocket.is_visible) {
          double alpha_factor = 1.0;
          if (rocket.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(now - rocket.fade_started_ticks) /
                                     static_cast<double>(ROCKET_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = rocket.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, rocket, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(now + rocket.animation_seed * 31) /
                        190.0;
                    const double pulse = 0.84 + 0.16 * std::sin(animation_clock);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(118.0 * alpha_factor, 0.0, 156.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 255, 112, 46,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(8,
                                 static_cast<int>(element_size * 0.29 * pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 255, 210, 124,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(10,
                                 static_cast<int>(element_size * 0.40 * pulse)));

                    const int icon_size =
                        std::max(12, static_cast<int>(element_size * 0.82));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawRocketIcon(icon_rect, body_alpha, animation_clock);
                  });
            });
          }
        }

        const auto &biohazard = game->biohazard_pickup;
        if (biohazard.is_visible) {
          double alpha_factor = 1.0;
          if (biohazard.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(now - biohazard.fade_started_ticks) /
                                     static_cast<double>(BIOHAZARD_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = biohazard.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, biohazard, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(now + biohazard.animation_seed * 43) /
                        170.0;
                    const double pulse = 0.88 + 0.12 * std::sin(animation_clock);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(114.0 * alpha_factor, 0.0, 160.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 255, 64, 64,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(8,
                                 static_cast<int>(element_size * 0.31 * pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 255, 180, 74,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(10,
                                 static_cast<int>(element_size * 0.42 * pulse)));

                    const int icon_size =
                        std::max(12, static_cast<int>(element_size * 0.82));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawBiohazardIcon(
                        icon_rect, body_alpha, animation_clock,
                        static_cast<double>(now + biohazard.animation_seed * 19) *
                            0.11,
                        0.96 + 0.08 * std::sin(animation_clock * 1.2));
                  });
            });
          }
        }

        const auto &nuclear_bomb = game->nuclear_bomb_pickup;
        if (nuclear_bomb.is_visible) {
          double alpha_factor = 1.0;
          if (nuclear_bomb.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(
                                      now - nuclear_bomb.fade_started_ticks) /
                                     static_cast<double>(NUCLEAR_BOMB_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = nuclear_bomb.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, nuclear_bomb, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(
                            now + nuclear_bomb.animation_seed * 47) /
                        180.0;
                    const double pulse =
                        0.84 + 0.16 * std::sin(animation_clock * 1.2);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(132.0 * alpha_factor, 0.0, 178.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 244, 210, 36,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(8, static_cast<int>(element_size * 0.33 *
                                                     pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 255, 238, 118,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(10, static_cast<int>(element_size * 0.45 *
                                                      pulse)));

                    const int icon_size =
                        std::max(14, static_cast<int>(element_size * 0.92));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawNuclearBombIcon(
                        icon_rect, body_alpha,
                        4.0 * std::sin(animation_clock * 0.7));
                  });
            });
          }
        }

        const auto &love_potion = game->love_potion_pickup;
        if (love_potion.is_visible) {
          double alpha_factor = 1.0;
          if (love_potion.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(
                                      now - love_potion.fade_started_ticks) /
                                     static_cast<double>(LOVE_POTION_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = love_potion.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, love_potion, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(
                            now + love_potion.animation_seed * 53) /
                        160.0;
                    const double pulse =
                        0.84 + 0.16 * std::sin(animation_clock * 1.35);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(132.0 * alpha_factor, 0.0, 180.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 255, 30, 54,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(8, static_cast<int>(element_size * 0.32 *
                                                     pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 255, 150, 160,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(10, static_cast<int>(element_size * 0.43 *
                                                      pulse)));

                    const int icon_size =
                        std::max(12, static_cast<int>(element_size * 0.82));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawLovePotionIcon(icon_rect, body_alpha, animation_clock);
                  });
            });
          }
        }

        const auto &life = game->life_pickup;
        if (life.is_visible) {
          double alpha_factor = 1.0;
          if (life.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(
                                      now - life.fade_started_ticks) /
                                     static_cast<double>(LIFE_PICKUP_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = life.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, life, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(
                            now + life.animation_seed * 53) /
                        160.0;
                    const double pulse =
                        0.86 + 0.18 * std::sin(animation_clock * 1.5);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(140.0 * alpha_factor, 0.0, 200.0));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(255.0 * alpha_factor, 0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, 255, 30, 54,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(8, static_cast<int>(element_size * 0.34 *
                                                     pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, 255, 150, 160,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(10, static_cast<int>(element_size * 0.45 *
                                                      pulse)));

                    const int heart_size =
                        std::max(12, static_cast<int>(element_size * 0.78));
                    drawPulsingHeart(center_x, center_y, heart_size, body_alpha,
                                     pulse);
                  });
            });
          }
        }

        const auto &disco = game->disco_pickup;
        if (disco.is_visible) {
          double alpha_factor = 1.0;
          if (disco.is_fading) {
            alpha_factor =
                1.0 - std::clamp(static_cast<double>(
                                      now - disco.fade_started_ticks) /
                                     static_cast<double>(DISCO_PICKUP_FADE_MS),
                                 0.0, 1.0);
          }
          if (alpha_factor > 0.0) {
            const MapCoord coord = disco.coord;
            const double depth =
                projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
            push_depth_command(depth, [&, coord, disco, alpha_factor, now]() {
              draw_with_wall_occlusion(
                  static_cast<double>(coord.v) + 0.5,
                  static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                  [&]() {
                    const SDL_FPoint center =
                        projectScene(coord.v + 0.5,
                                     coord.u + kFloorObjectDepthRowFactor, 0.0);
                    const int center_x = static_cast<int>(std::lround(center.x));
                    const int center_y =
                        static_cast<int>(std::lround(center.y)) -
                        non_character_sprite_lift_px;
                    const double animation_clock =
                        static_cast<double>(
                            now + disco.animation_seed *
                                      DISCO_PICKUP_RENDER_ANIMATION_SEED_MULTIPLIER) /
                        DISCO_PICKUP_RENDER_ANIMATION_DIVISOR_MS;
                    const double pulse =
                        DISCO_PICKUP_PULSE_BASE +
                        DISCO_PICKUP_PULSE_SWING *
                            std::sin(animation_clock *
                                     DISCO_PICKUP_PULSE_FREQUENCY);
                    const Uint8 glow_alpha = static_cast<Uint8>(
                        std::clamp(DISCO_PICKUP_GLOW_ALPHA_BASE * alpha_factor,
                                   0.0, DISCO_PICKUP_GLOW_ALPHA_MAX));
                    const Uint8 body_alpha = static_cast<Uint8>(
                        std::clamp(DISCO_PICKUP_BODY_ALPHA_BASE * alpha_factor,
                                   0.0, 255.0));

                    SDL_SetRenderDrawColor(sdl_renderer, DISCO_PICKUP_GLOW_COLOR.r,
                                           DISCO_PICKUP_GLOW_COLOR.g,
                                           DISCO_PICKUP_GLOW_COLOR.b,
                                           glow_alpha / 2);
                    SDL_RenderFillCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(DISCO_PICKUP_GLOW_MIN_RADIUS_PX,
                                 static_cast<int>(element_size *
                                                  DISCO_PICKUP_GLOW_RADIUS_CELLS *
                                                  pulse)));
                    SDL_SetRenderDrawColor(sdl_renderer, DISCO_PICKUP_RING_COLOR.r,
                                           DISCO_PICKUP_RING_COLOR.g,
                                           DISCO_PICKUP_RING_COLOR.b,
                                           glow_alpha);
                    SDL_RenderDrawCircle(
                        sdl_renderer, center_x, center_y,
                        std::max(DISCO_PICKUP_RING_MIN_RADIUS_PX,
                                 static_cast<int>(element_size *
                                                  DISCO_PICKUP_RING_RADIUS_CELLS *
                                                  pulse)));

                    const int icon_size =
                        std::max(DISCO_PICKUP_ICON_MIN_SIZE_PX,
                                 static_cast<int>(
                                     element_size *
                                     DISCO_PICKUP_ICON_SIZE_CELLS));
                    const SDL_Rect icon_rect{center_x - icon_size / 2,
                                             center_y - icon_size / 2,
                                             icon_size, icon_size};
                    drawDiscoPickupIcon(icon_rect, body_alpha,
                                        animation_clock);
                  });
            });
          }
        }

        const auto &nuclear_marker = game->nuclear_bomb_target_marker;
        if (nuclear_marker.is_marked) {
          const MapCoord coord = nuclear_marker.coord;
          const double depth =
              projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
          push_depth_command(depth, [&, coord, nuclear_marker, now]() {
            draw_with_wall_occlusion(
                static_cast<double>(coord.v) + 0.5,
                static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                [&]() {
                  const double animation_clock =
                      static_cast<double>(
                          now + nuclear_marker.animation_seed * 31) /
                      180.0;
                  const double pulse = 0.94 + 0.12 * std::sin(animation_clock);
                  const Uint8 alpha = static_cast<Uint8>(std::clamp(
                      168.0 + 64.0 *
                                  (0.5 + 0.5 *
                                             std::sin(animation_clock * 1.25)),
                      0.0, 255.0));
                  const SDL_Rect marker_rect = makeFloorAlignedRect(
                      coord.v, coord.u, 0.10, 0.10, 0.80, 0.80);
                  drawNuclearTargetMarkerIcon(marker_rect, alpha, pulse);
                });
          });
        }

        if (game->pacman != nullptr) {
          const bool potion_invulnerable =
              game->pacman->invulnerable_until_ticks > now;
          const bool life_recovery_active =
              game->IsPacmanRecoveringFromLifeLoss(now);
          const bool slimed = game->pacman->slimed_until_ticks > now;
          const bool front_facing_due_to_gas =
              slimed && game->pacman->paralyzed_until_ticks > now;
          const bool walking =
              game->pacman->px_delta.x != 0 || game->pacman->px_delta.y != 0;
          const Uint32 flicker_phase_ms =
              std::max<Uint32>(1, PACMAN_RECOVERY_FLICKER_PHASE_MS);
          const Uint8 pacman_alpha =
              (!life_recovery_active || ((now / flicker_phase_ms) % 2 == 0))
                  ? 255
                  : PACMAN_RECOVERY_ALPHA;
          Directions facing_direction = game->pacman->facing_direction;
          if (front_facing_due_to_gas ||
              facing_direction == Directions::None) {
            facing_direction = Directions::Down;
          }
          SDL_Texture *pacman_texture =
              getPacmanTexture(facing_direction, walking);

          if (!game->dead && !final_loss_sequence_active) {
            const auto draw_slime_overlay = [&, slimed,
                                             now](const SDL_Rect &anchor_rect,
                                                  double rotation_degrees) {
              if (!slimed) {
                return;
              }

              const Uint32 fade_start_ticks =
                  (game->pacman->slimed_until_ticks >
                   static_cast<Uint32>(SLIME_OVERLAY_FADE_MS))
                      ? game->pacman->slimed_until_ticks -
                            static_cast<Uint32>(SLIME_OVERLAY_FADE_MS)
                      : 0;
              double alpha_factor = 1.0;
              if (now >= fade_start_ticks) {
                alpha_factor =
                    1.0 -
                    std::clamp(static_cast<double>(now - fade_start_ticks) /
                                   static_cast<double>(SLIME_OVERLAY_FADE_MS),
                               0.0, 1.0);
              }
              if (alpha_factor <= 0.0) {
                return;
              }

              (void)rotation_degrees;
              drawPacmanGasCloudlets(anchor_rect, alpha_factor, now);
            };

            if (game->pacman->teleport_animation_active) {
              const Uint32 elapsed =
                  now - game->pacman->teleport_animation_started_ticks;
              const bool in_departure_phase =
                  elapsed < TELEPORT_ANIMATION_PHASE_MS;
              const double phase_progress =
                  std::clamp(static_cast<double>(in_departure_phase
                                                     ? elapsed
                                                     : (elapsed -
                                                        TELEPORT_ANIMATION_PHASE_MS)) /
                                 static_cast<double>(TELEPORT_ANIMATION_PHASE_MS),
                             0.0, 1.0);
              const MapCoord render_coord =
                  in_departure_phase
                      ? game->pacman->teleport_animation_from_coord
                      : game->pacman->teleport_animation_to_coord;
              const double scale = in_departure_phase ? (1.0 - phase_progress)
                                                      : phase_progress;
              const double rotation =
                  (in_departure_phase ? phase_progress
                                      : (1.0 + phase_progress)) *
                  1080.0;
              const int size = std::max(
                  1, static_cast<int>(element_size * 0.9 * std::max(0.0, scale)));
              const SDL_FPoint foot =
                  projectScene(render_coord.v + 0.5,
                               render_coord.u + kSpriteFootRowFactor, 0.0);
              const SDL_Rect animated_rect{
                  static_cast<int>(std::lround(foot.x - size / 2.0)),
                  static_cast<int>(std::lround(foot.y - size)), size, size};
              const double depth =
                  projectScene(0.5, render_coord.u + kSpriteFootRowFactor, 0.0).y;
              push_depth_command(
                  depth,
                  [&, animated_rect, rotation, pacman_texture, pacman_alpha,
                   potion_invulnerable, in_departure_phase, phase_progress,
                   scale, render_coord]() {
                    drawWithWallOcclusion(
                        expandRect(animated_rect, std::max(10, element_size / 3)),
                        static_cast<double>(render_coord.u) + 0.5,
                        [&]() {
                          const Uint8 shadow_alpha = static_cast<Uint8>(
                              std::clamp(static_cast<double>(
                                             CHARACTER_GROUND_SHADOW_BASE_ALPHA) *
                                             (0.35 + 0.65 * std::max(0.0, scale)),
                                         0.0, 255.0));
                          drawFloorSpriteShadow(
                              animated_rect, shadow_alpha, scale,
                              std::max(0.35, scale * 0.82));
                          if (potion_invulnerable) {
                            drawPacmanShield(
                                animated_rect.x + animated_rect.w / 2,
                                animated_rect.y + animated_rect.h / 2,
                                std::max(8, animated_rect.w / 2 + 5),
                                static_cast<double>(now) / 180.0);
                          }

                          const Uint8 spark_alpha = static_cast<Uint8>(
                              std::clamp((in_departure_phase
                                              ? (1.0 - phase_progress)
                                              : (0.35 + 0.65 * phase_progress)) *
                                             170.0,
                                         0.0, 200.0));
                          SDL_SetRenderDrawColor(sdl_renderer, 130, 214, 255,
                                                 spark_alpha / 2);
                          SDL_RenderFillCircle(
                              sdl_renderer,
                              animated_rect.x + animated_rect.w / 2,
                              animated_rect.y + animated_rect.h / 2,
                              std::max(3, static_cast<int>(element_size * 0.42 *
                                                           scale)));
                          SDL_SetRenderDrawColor(sdl_renderer, 220, 245, 255,
                                                 spark_alpha);
                          for (int spark = 0; spark < 5; spark++) {
                            const double angle =
                                (rotation / 180.0) * 3.1415926535 +
                                spark * (2.0 * 3.1415926535 / 5.0);
                            const int spark_length = std::max(
                                4, static_cast<int>(element_size *
                                                    (0.10 + 0.14 * scale)));
                            SDL_RenderDrawLine(
                                sdl_renderer,
                                animated_rect.x + animated_rect.w / 2,
                                animated_rect.y + animated_rect.h / 2,
                                animated_rect.x + animated_rect.w / 2 +
                                    static_cast<int>(std::cos(angle) *
                                                     spark_length),
                                animated_rect.y + animated_rect.h / 2 +
                                    static_cast<int>(std::sin(angle) *
                                                     spark_length));
                          }

                          if (animated_rect.w > 1 && pacman_texture != nullptr) {
                            SDL_SetTextureAlphaMod(pacman_texture, pacman_alpha);
                            SDL_RenderCopyEx(sdl_renderer, pacman_texture,
                                             nullptr, &animated_rect, rotation,
                                             nullptr, SDL_FLIP_NONE);
                            SDL_SetTextureAlphaMod(pacman_texture, 255);
                          }
                          draw_slime_overlay(animated_rect, rotation);
                        });
                  });
            } else {
              const double delta_col_cells =
                  pixel_delta_to_cells(game->pacman->px_delta.x);
              const double delta_row_cells =
                  pixel_delta_to_cells(game->pacman->px_delta.y);
              SDL_Rect pacman_rect =
                  makeBillboardRect(game->pacman->map_coord.v,
                                    game->pacman->map_coord.u, 0.9, 0.9,
                                    kSpriteFootRowFactor, delta_col_cells,
                                    delta_row_cells);
              if (game->active_disco_easteregg.is_active) {
                const int phase_seed =
                    game->active_disco_easteregg.animation_seed +
                    DISCO_PACMAN_DANCE_PHASE_SEED_OFFSET;
                const double beat = std::sin(
                    static_cast<double>(
                        now + phase_seed *
                                  DISCO_DANCE_PHASE_SEED_MULTIPLIER_A) /
                    DISCO_DANCE_BEAT_DIVISOR_MS);
                const double gate = std::sin(
                    static_cast<double>(
                        now + phase_seed *
                                  DISCO_DANCE_PHASE_SEED_MULTIPLIER_B) /
                    DISCO_DANCE_GATE_DIVISOR_MS);
                if (gate > DISCO_DANCE_GATE_THRESHOLD) {
                  pacman_rect.y -= static_cast<int>(std::lround(
                      std::max(DISCO_DANCE_JUMP_MIN_PX,
                               element_size * DISCO_DANCE_JUMP_HEIGHT_CELLS) *
                      std::max(0.0, beat)));
                }
              }
              const double depth =
                  projectScene(0.5,
                               game->pacman->map_coord.u + kSpriteFootRowFactor +
                                   delta_row_cells,
                               0.0)
                      .y;
              push_depth_command(
                  depth,
                  [&, pacman_rect, pacman_texture, pacman_alpha,
                   potion_invulnerable, delta_row_cells, draw_slime_overlay]() {
                    drawWithWallOcclusion(
                        expandRect(pacman_rect, std::max(10, element_size / 3)),
                        static_cast<double>(game->pacman->map_coord.u) + 0.5 +
                            delta_row_cells,
                        [&]() {
                          drawFloorSpriteShadow(
                              pacman_rect, CHARACTER_GROUND_SHADOW_BASE_ALPHA);
                          if (potion_invulnerable) {
                            drawPacmanShield(
                                pacman_rect.x + pacman_rect.w / 2,
                                pacman_rect.y + pacman_rect.h / 2,
                                std::max(8, pacman_rect.w / 2 + 5),
                                static_cast<double>(now) / 180.0);
                          }
                          if (pacman_texture != nullptr) {
                            SDL_SetTextureAlphaMod(pacman_texture, pacman_alpha);
                            SDL_RenderCopy(sdl_renderer, pacman_texture,
                                           nullptr, &pacman_rect);
                            SDL_SetTextureAlphaMod(pacman_texture, 255);
                          }
                          draw_slime_overlay(pacman_rect, 0.0);
                        });
                  });
            }
          }
        }

        for (const auto &placed_dynamite : game->placed_dynamites) {
          const MapCoord coord = placed_dynamite.coord;
          const double depth =
              projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
          push_depth_command(depth, [this, coord, placed_dynamite, now,
                                     non_character_sprite_lift_px,
                                     &draw_with_wall_occlusion]() {
            draw_with_wall_occlusion(
                static_cast<double>(coord.v) + 0.5,
                static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                [this, coord, placed_dynamite, now,
                 non_character_sprite_lift_px]() {
                  const SDL_FPoint center =
                      projectScene(coord.v + 0.5,
                                   coord.u + kFloorObjectDepthRowFactor, 0.0);
                  const int center_x = static_cast<int>(std::lround(center.x));
                  const int center_y =
                      static_cast<int>(std::lround(center.y)) -
                      non_character_sprite_lift_px;
                  const Uint32 remaining_ms =
                      (placed_dynamite.explode_at_ticks > now)
                          ? (placed_dynamite.explode_at_ticks - now)
                          : 0;
                  const double countdown_progress =
                      std::clamp(static_cast<double>(remaining_ms) /
                                     static_cast<double>(std::max<Uint32>(
                                         1, DYNAMITE_COUNTDOWN_MS)),
                                 0.0, 1.0);
                  const double urgency = 1.0 - countdown_progress;
                  const double pulse_clock =
                      static_cast<double>(
                          now + placed_dynamite.animation_seed * 29) /
                      130.0;
                  const double pulse =
                      0.88 + (0.12 + urgency * 0.22) * std::sin(pulse_clock);

                  SDL_SetRenderDrawColor(
                      sdl_renderer, 255, 88, 32,
                      static_cast<Uint8>(
                          std::clamp(72.0 + urgency * 90.0, 0.0, 190.0)));
                  SDL_RenderFillCircle(
                      sdl_renderer, center_x, center_y,
                      std::max(6,
                               static_cast<int>(element_size * 0.24 * pulse)));

                  const int icon_size =
                      std::max(12, static_cast<int>(element_size * 0.74));
                  const SDL_Rect icon_rect{
                      center_x - icon_size / 2,
                      center_y - icon_size / 2,
                      icon_size, icon_size};
                  drawDynamiteIcon(icon_rect, true, pulse_clock, 255, urgency);

                  const int remaining_seconds =
                      static_cast<int>((std::max<Uint32>(1, remaining_ms) +
                                        999) /
                                       1000);
                  renderSimpleText(
                      sdl_font_hud, std::to_string(remaining_seconds),
                      SDL_Color{255, 238, 206, 255}, center_x,
                      center_y - TTF_FontHeight(sdl_font_hud));
                });
          });
        }

        if (game->plastic_explosive_is_armed) {
          const auto placed_plastic_explosive = game->placed_plastic_explosive;
          const MapCoord coord = placed_plastic_explosive.coord;
          const double depth =
              projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
          push_depth_command(depth, [&, coord, placed_plastic_explosive, now]() {
            draw_with_wall_occlusion(
                static_cast<double>(coord.v) + 0.5,
                static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                [&]() {
                  const SDL_FPoint center =
                      projectScene(coord.v + 0.5,
                                   coord.u + kFloorObjectDepthRowFactor, 0.0);
                  const int center_x = static_cast<int>(std::lround(center.x));
                  const int center_y =
                      static_cast<int>(std::lround(center.y)) -
                      non_character_sprite_lift_px;
                  const double pulse_clock =
                      static_cast<double>(
                          now + placed_plastic_explosive.animation_seed * 31) /
                      130.0;
                  const double pulse = 0.84 + 0.28 * std::sin(pulse_clock);

                  SDL_SetRenderDrawColor(
                      sdl_renderer, 255, 72, 52,
                      static_cast<Uint8>(
                          std::clamp(92.0 + pulse * 72.0, 0.0, 188.0)));
                  SDL_RenderFillCircle(
                      sdl_renderer, center_x,
                      center_y +
                          static_cast<int>(std::lround(element_size * 0.08)),
                      std::max(6,
                               static_cast<int>(element_size * 0.22 * pulse)));

                  const int icon_size =
                      std::max(12, static_cast<int>(element_size * 0.76));
                  const SDL_Rect icon_rect{
                      center_x - icon_size / 2,
                      center_y - icon_size / 2 -
                          static_cast<int>(std::lround(element_size * 0.08)),
                      icon_size, icon_size};
                  drawPlasticExplosiveIcon(icon_rect, pulse_clock, 255, true);
                });
          });
        }

        for (auto monster : game->monsters) {
          const bool waiting_for_blast =
              monster->scheduled_dynamite_blast_ticks != 0 &&
              now < monster->scheduled_dynamite_blast_ticks;
          if (!monster->is_alive && !waiting_for_blast) {
            continue;
          }

          const bool goat_stunned =
              monster->monster_char == GOAT && !waiting_for_blast &&
              monster->goat_state == Monster::GoatState::Stunned &&
              now < monster->goat_stun_until_ticks;
          const bool goat_grazing =
              monster->monster_char == GOAT && !waiting_for_blast &&
              !goat_stunned &&
              (monster->grazing_until_ticks > now || monster->goat_pacified ||
               (monster->goat_love_sequence_active &&
                monster->goat_love_target_id == -1));
          const bool goat_jumping =
              monster->monster_char == GOAT && !waiting_for_blast &&
              !goat_stunned && monster->goat_is_jumping;
          SDL_Texture *monster_texture =
              goat_stunned
                  ? getStunnedGoatTexture()
                  : getMonsterTexture(monster->monster_char,
                                      monster->facing_direction, monster->id,
                                      goat_grazing, goat_jumping);
          if (monster_texture == nullptr) {
            continue;
          }

          const MapCoord anchor_coord = waiting_for_blast
                                            ? monster->scheduled_dynamite_blast_coord
                                            : monster->map_coord;
          const double delta_col_cells =
              waiting_for_blast ? 0.0 : pixel_delta_to_cells(monster->px_delta.x);
          const double delta_row_cells =
              waiting_for_blast ? 0.0 : pixel_delta_to_cells(monster->px_delta.y);
          SDL_Rect monster_rect =
              makeBillboardRect(anchor_coord.v, anchor_coord.u,
                                kMonsterRenderScale, kMonsterRenderScale,
                                kSpriteFootRowFactor, delta_col_cells,
                                delta_row_cells);
          if (game->active_disco_easteregg.is_active && !waiting_for_blast) {
            const int phase_seed = monster->id * DISCO_MONSTER_DANCE_ID_MULTIPLIER +
                                   game->active_disco_easteregg.animation_seed;
            const double beat = std::sin(
                static_cast<double>(
                    now + phase_seed * DISCO_DANCE_PHASE_SEED_MULTIPLIER_A) /
                DISCO_DANCE_BEAT_DIVISOR_MS);
            const double gate = std::sin(
                static_cast<double>(
                    now + phase_seed * DISCO_DANCE_PHASE_SEED_MULTIPLIER_B) /
                DISCO_DANCE_GATE_DIVISOR_MS);
            if (gate > DISCO_DANCE_GATE_THRESHOLD) {
              monster_rect.y -= static_cast<int>(std::lround(
                  std::max(DISCO_DANCE_JUMP_MIN_PX,
                           element_size * DISCO_DANCE_JUMP_HEIGHT_CELLS) *
                  std::max(0.0, beat)));
            }
          }
          if (goat_stunned) {
            const double sway = std::sin(static_cast<double>(now) / 120.0) *
                                element_size * 0.04;
            monster_rect.x += static_cast<int>(std::lround(sway));
          }
          const double depth =
              projectScene(0.5, anchor_coord.u + kSpriteFootRowFactor +
                                    delta_row_cells,
                           0.0)
                  .y;
          push_depth_command(
              depth,
              [&, monster_texture, monster_rect, waiting_for_blast, now,
               monster, anchor_coord, delta_row_cells]() {
                drawWithWallOcclusion(
                    expandRect(monster_rect, std::max(8, element_size / 4)),
                    static_cast<double>(anchor_coord.u) + 0.5 +
                        delta_row_cells,
                    [&]() {
                      if (waiting_for_blast) {
                        const Uint32 remaining_ticks =
                            monster->scheduled_dynamite_blast_ticks - now;
                        const double blink =
                            std::sin(static_cast<double>(now) / 60.0);
                        const Uint8 warning_alpha = static_cast<Uint8>(
                            std::clamp(120.0 + 110.0 * std::max(0.0, blink),
                                       0.0, 255.0));
                        SDL_SetRenderDrawColor(sdl_renderer, 255, 120, 48,
                                               warning_alpha / 2);
                        SDL_RenderFillCircle(
                            sdl_renderer,
                            monster_rect.x + monster_rect.w / 2,
                            monster_rect.y + monster_rect.h / 2,
                            std::max(7,
                                     static_cast<int>(element_size * 0.44)));
                        const int alpha_mod = std::min(
                            255, 170 + static_cast<int>(remaining_ticks % 80));
                        SDL_SetTextureAlphaMod(
                            monster_texture, static_cast<Uint8>(alpha_mod));
                      } else {
                        SDL_SetTextureAlphaMod(monster_texture, 255);
                      }
                      if (monster->monster_char == GOAT &&
                          monster->goat_love_sequence_active) {
                        const bool red_flash =
                            (static_cast<int>(now / 80) % 2) == 0;
                        SDL_SetTextureColorMod(monster_texture, 255,
                                               red_flash ? 72 : 128,
                                               red_flash ? 72 : 128);
                      }
                      SDL_RenderCopy(sdl_renderer, monster_texture, nullptr,
                                     &monster_rect);
                      SDL_SetTextureColorMod(monster_texture, 255, 255, 255);
                      if (monster->is_electrified) {
                        drawElectrifiedMonsterAura(
                            monster_rect, monster->electrified_visual_seed, now,
                            monster->electrified_charge_target_id != -1);
                      }
                      SDL_SetTextureAlphaMod(monster_texture, 255);
                    });
              });
        }
        for (const AlienAgent &alien : game->aliens) {
          const bool residue_visible = AlienExplosionResidueVisible(alien, now);
          if (!alien.is_alive && alien.explosion_spawned && !residue_visible) {
            continue;
          }

          SDL_Texture *alien_texture = nullptr;
          SDL_Point source_size{0, 0};
          if (residue_visible && !sdl_alien_explosion_textures.empty()) {
            alien_texture = sdl_alien_explosion_textures.back();
            if (!sdl_alien_explosion_sizes.empty()) {
              source_size = sdl_alien_explosion_sizes.back();
            }
          } else if (!sdl_alien_textures.empty()) {
            const size_t texture_index =
                AlienTextureIndexForFrame(getAlienAnimationFrame(alien, now));
            if (texture_index < sdl_alien_textures.size()) {
              alien_texture = sdl_alien_textures[texture_index];
            }
            if (texture_index < sdl_alien_sizes.size()) {
              source_size = sdl_alien_sizes[texture_index];
            }
          }
          if (alien_texture == nullptr) {
            continue;
          }

          const double bob_cells = 0.0;
          const double render_scale =
              residue_visible ? ALIEN_EXPLOSION_RENDER_SCALE *
                                    ALIEN_EXPLOSION_FINAL_FRAME_SCALE
                              : kAlienRenderScale;
          const SDL_FPoint render_scales =
              TextureScaleFactors(render_scale, source_size);
          SDL_Rect alien_rect =
              makeBillboardRect(alien.coord.v, alien.coord.u,
                                render_scales.x, render_scales.y,
                                kSpriteFootRowFactor, 0.0, bob_cells);
          const double depth =
              projectScene(0.5, alien.coord.u + kSpriteFootRowFactor, 0.0).y;
          push_depth_command(depth,
                             [this, alien_texture, alien_rect, alien]() {
            drawWithWallOcclusion(
                expandRect(alien_rect, std::max(8, element_size / 4)),
                static_cast<double>(alien.coord.u) + 0.5,
                [&]() {
                  SDL_RenderCopy(sdl_renderer, alien_texture, nullptr,
                                 &alien_rect);
                });
          });
        }
      } else {
        for (int i = 0; i < map->get_number_goodies(); i++) {
          const MapCoord coord = map->get_coord_goodie(i);
          const SDL_Rect rect = offsetRectY(
              makeFloorAlignedRect(coord.v, coord.u, 0.15, 0.15, 0.7, 0.7),
              -non_character_sprite_lift_px);
          const double depth =
              projectScene(0.5, coord.u + kFloorObjectDepthRowFactor, 0.0).y;
          push_depth_command(depth, [this, rect, coord,
                                     &draw_with_wall_occlusion]() {
            draw_with_wall_occlusion(
                static_cast<double>(coord.v) + 0.5,
                static_cast<double>(coord.u) + kFloorObjectDepthRowFactor,
                [&]() {
                  if (sdl_goodie_texture != nullptr) {
                    SDL_RenderCopy(sdl_renderer, sdl_goodie_texture, nullptr,
                                   &rect);
                  }
                });
          });
        }

        const MapCoord pacman_coord = map->get_coord_pacman();
        const SDL_Rect pacman_rect = makeBillboardRect(
            pacman_coord.v, pacman_coord.u, 0.9, 0.9, kSpriteFootRowFactor);
        const double pacman_depth =
            projectScene(0.5, pacman_coord.u + kSpriteFootRowFactor, 0.0).y;
        push_depth_command(pacman_depth, [this, pacman_rect, pacman_coord]() {
          drawWithWallOcclusion(
              expandRect(pacman_rect, std::max(8, element_size / 4)),
              static_cast<double>(pacman_coord.u) + 0.5,
              [&]() {
                drawFloorSpriteShadow(pacman_rect,
                                      CHARACTER_GROUND_SHADOW_BASE_ALPHA);
                SDL_Texture *pacman_texture =
                    getPacmanTexture(Directions::Down, false);
                if (pacman_texture != nullptr) {
                  SDL_RenderCopy(sdl_renderer, pacman_texture, nullptr,
                                 &pacman_rect);
                }
              });
        });

        for (int i = 0; i < map->get_number_monsters(); i++) {
          SDL_Texture *monster_texture =
              getMonsterTexture(map->get_char_monster(i), Directions::Down, i);
          if (monster_texture == nullptr) {
            continue;
          }

          const MapCoord coord = map->get_coord_monster(i);
          const SDL_Rect monster_rect =
              makeBillboardRect(coord.v, coord.u, kMonsterRenderScale,
                                kMonsterRenderScale, kSpriteFootRowFactor);
          const double depth =
              projectScene(0.5, coord.u + kSpriteFootRowFactor, 0.0).y;
          push_depth_command(depth,
                             [this, monster_texture, monster_rect, coord]() {
            drawWithWallOcclusion(
                expandRect(monster_rect, std::max(8, element_size / 4)),
                static_cast<double>(coord.u) + 0.5,
                [&]() {
                  SDL_RenderCopy(sdl_renderer, monster_texture, nullptr,
                                 &monster_rect);
                });
          });
        }
      }

      for (size_t u = 0; u < rows; ++u) {
        for (size_t v = 0; v < cols; ++v) {
          if (map->map_entry(u, v) != ElementType::TYPE_WALL) {
            continue;
          }

          const bool has_wall_down =
              u + 1 < rows && map->map_entry(u + 1, v) == ElementType::TYPE_WALL;
          if (has_wall_down) {
            continue;
          }

          const double depth = projectScene(0.5, static_cast<double>(u) + 1.0, 0.0).y;
          push_depth_command(depth, [this, u, v]() {
            drawWallFrontFace3D(static_cast<int>(u), static_cast<int>(v));
          });
        }
      }

      std::stable_sort(depth_commands.begin(), depth_commands.end(),
                       [](const DepthDrawCommand &left,
                          const DepthDrawCommand &right) {
                         if (left.depth == right.depth) {
                           return left.order < right.order;
                         }
                         return left.depth < right.depth;
                       });

      for (auto &command : depth_commands) {
        command.draw();
      }

      // Stun stars overlay: drawn after depth-sorted scene so walls do not
      // occlude them.
      const Uint32 stars_now = SDL_GetTicks();
      if (game != nullptr && game->pacman != nullptr &&
          game->pacman->paralyzed_until_ticks > stars_now) {
        const double delta_col_cells =
            pixel_delta_to_cells(game->pacman->px_delta.x);
        const double delta_row_cells =
            pixel_delta_to_cells(game->pacman->px_delta.y);
        const SDL_Rect pacman_rect =
            makeBillboardRect(game->pacman->map_coord.v,
                              game->pacman->map_coord.u, 0.9, 0.9,
                              kSpriteFootRowFactor, delta_col_cells,
                              delta_row_cells);
        drawStunStars(
            pacman_rect.x + pacman_rect.w / 2,
            pacman_rect.y - static_cast<int>(element_size * 0.25),
            static_cast<int>(element_size * STUN_STARS_ORBIT_RADIUS_CELLS),
            static_cast<int>(element_size * STUN_STARS_RADIUS_CELLS),
            stars_now);
      }
      if (game != nullptr) {
        for (Monster *monster : game->monsters) {
          if (monster == nullptr || !monster->is_alive ||
              monster->monster_char != GOAT) {
            continue;
          }
          const double m_delta_col_cells =
              pixel_delta_to_cells(monster->px_delta.x);
          const double m_delta_row_cells =
              pixel_delta_to_cells(monster->px_delta.y);
          const SDL_Rect monster_rect = makeBillboardRect(
              monster->map_coord.v, monster->map_coord.u,
              kMonsterRenderScale, kMonsterRenderScale, kSpriteFootRowFactor,
              m_delta_col_cells, m_delta_row_cells);
          if (monster->goat_love_sequence_active) {
            const double pulse =
                0.82 + 0.18 * std::sin(static_cast<double>(stars_now) / 105.0);
            drawPulsingHeart(
                monster_rect.x + monster_rect.w / 2,
                monster_rect.y - static_cast<int>(element_size * 0.20),
                std::max(8, static_cast<int>(element_size * 0.38)), 230,
                pulse);
          }
          if (monster->goat_state != Monster::GoatState::Stunned ||
              stars_now >= monster->goat_stun_until_ticks) {
            continue;
          }
          drawStunStars(
              monster_rect.x + monster_rect.w / 2,
              monster_rect.y - static_cast<int>(element_size * 0.25),
              static_cast<int>(element_size *
                               STUN_STARS_ORBIT_RADIUS_CELLS),
              static_cast<int>(element_size * STUN_STARS_RADIUS_CELLS),
            stars_now);
        }
        for (const AlienAgent &alien : game->aliens) {
          if (!alien.is_alive) {
            continue;
          }
          SDL_Point source_size{0, 0};
          if (!sdl_alien_textures.empty()) {
            const size_t texture_index =
                AlienTextureIndexForFrame(getAlienAnimationFrame(alien, stars_now));
            if (texture_index < sdl_alien_sizes.size()) {
              source_size = sdl_alien_sizes[texture_index];
            }
          }
          const SDL_FPoint render_scales =
              TextureScaleFactors(kAlienRenderScale, source_size);
          const SDL_Rect alien_rect =
              makeBillboardRect(alien.coord.v, alien.coord.u,
                                render_scales.x, render_scales.y,
                                kSpriteFootRowFactor);
          drawAlienThoughtBubble(alien, alien_rect, stars_now);
        }
      }

      if (game != nullptr && (game->dead ||
                              game->IsFinalLossSequenceActive(SDL_GetTicks()))) {
        drawDefeatEffect();
      }
    }

    if (game != nullptr) {
      drawgasclouds();
      drawfireballs();
      drawslimeballs();
      drawrockets();
      drawactiveairstrike();
      drawActiveNuclearBombDrop();
      drawbiohazardbeam();
      drawalienlaser();
      draweffects();
      drawExplosionParticles();
    }
    if (show_hud) {
      drawhud();
    }
    return;
  }

  drawmap();
  drawWallRubble();
  drawteleporters();
  if (game != nullptr) {
    drawgoodies();
    drawbonusflask();
    drawdynamitepickup();
    drawplasticexplosivepickup();
    drawwalkietalkiepickup();
    drawrocketpickup();
    drawbiohazardpickup();
    drawnuclearbombpickup();
    drawlovepotionpickup();
    drawNuclearBombTargetMarker();
    drawpacman();
    drawplaceddynamites();
    drawplacedplasticexplosive();
    drawmonsters();
    drawaliens();
    drawbiohazardbeam();
    drawalienlaser();
    drawgasclouds();
    drawfireballs();
    drawslimeballs();
    drawrockets();
    drawactiveairstrike();
    drawActiveNuclearBombDrop();
    draweffects();
    drawExplosionParticles();
  } else {
    drawStaticGoodies();
    drawStaticPacman();
    drawStaticMonsters();
  }
  if (show_hud) {
    drawhud();
  }
}

void Renderer::updateSceneLayout(size_t row_count_value,
                                 size_t col_count_value) {
  rows = row_count_value;
  cols = col_count_value;
  wall_top_y_dirty_ = true;

  constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
  cos_tilt = ENABLE_3D_VIEW ? std::cos(VIEW_3D_TILT_DEGREES * kDegToRad) : 1.0;
  sin_tilt = ENABLE_3D_VIEW ? std::sin(VIEW_3D_TILT_DEGREES * kDegToRad) : 0.0;

  const int row_count = static_cast<int>(rows);
  const int col_count = static_cast<int>(cols);
  const int hud_fontsize = std::max(22, screen_res_y / 35);
  const int reserved_top_space = hud_fontsize * 3 + 24;
  // Vertical footprint in 3D = floor compressed by cos_tilt, plus the wall
  // extrusion that sticks up above the top row by wall_height_px * sin_tilt.
  const double row_span_factor =
      ENABLE_3D_VIEW
          ? row_count * cos_tilt + VIEW_3D_WALL_HEIGHT_FACTOR * sin_tilt
          : static_cast<double>(row_count);
  const int element_size_x = static_cast<int>(
      (screen_res_y - reserved_top_space - row_count - 1) /
      std::max(1.0, row_span_factor));
  const int element_size_y =
      (screen_res_x - col_count - 1) / std::max(1, col_count);
  element_size = std::max(8, std::min(element_size_x, element_size_y));

  wall_height_px =
      ENABLE_3D_VIEW
          ? static_cast<int>(std::lround((element_size + 1) *
                                         VIEW_3D_WALL_HEIGHT_FACTOR))
          : 0;

  const int grid_width = col_count * (element_size + 1) + 1;
  const int grid_height =
      ENABLE_3D_VIEW
          ? static_cast<int>(std::lround(row_count * (element_size + 1) *
                                             cos_tilt +
                                         wall_height_px * sin_tilt)) +
                1
          : row_count * (element_size + 1) + 1;
  const int content_height = reserved_top_space + grid_height;
  const int content_top = std::max(0, (screen_res_y - content_height) / 2);

  offset_x = (screen_res_x - grid_width) / 2;
  offset_y = std::max(0, content_top + reserved_top_space - element_size / 2);
  hud_top_y = content_top + std::max(4, hud_fontsize / 5);

  const int extrusion_headroom =
      ENABLE_3D_VIEW ? static_cast<int>(std::lround(wall_height_px * sin_tilt))
                     : 0;
  scene_origin_y = offset_y + 1 + extrusion_headroom;
}

void Renderer::SetScene(Map *new_map, Game *new_game) {
  map = new_map;
  game = new_game;
  wall_top_y_dirty_ = true;

  if (map != nullptr) {
    updateSceneLayout(map->get_map_rows(), map->get_map_cols());
  }
}

void Renderer::rebuildWallOcclusionLookupIfNeeded() {
  const size_t expected_size = (rows + 1) * cols;
  if (!wall_top_y_dirty_ && wall_top_y_lookup_.size() == expected_size) {
    return;
  }
  wall_top_y_lookup_.assign(expected_size, screen_res_y);
  if (map == nullptr) {
    wall_top_y_dirty_ = false;
    return;
  }
  for (size_t col = 0; col < cols; ++col) {
    int last_y = screen_res_y;
    for (int start_row = static_cast<int>(rows); start_row >= 0;
         --start_row) {
      if (start_row < static_cast<int>(rows) &&
          map->map_entry(static_cast<size_t>(start_row), col) ==
              ElementType::TYPE_WALL) {
        last_y = static_cast<int>(std::floor(
            projectScene(static_cast<double>(col) + 0.5,
                         static_cast<double>(start_row), 1.0)
                .y));
      }
      wall_top_y_lookup_[static_cast<size_t>(start_row) * cols + col] = last_y;
    }
  }
  wall_top_y_dirty_ = false;
}

void Renderer::SetFloorTextureIndex(int floor_texture_index) {
  selected_floor_texture_index = ClampFloorTextureIndex(floor_texture_index);
}

void Renderer::renderLayoutFrame(const std::vector<std::string> &layout) {
  SDL_SetRenderDrawColor(sdl_renderer, COLOR_BACK, 255);
  SDL_RenderClear(sdl_renderer);
  drawLayout(layout);
  drawLayoutTeleporters(layout);
  drawLayoutEntities(layout);
}

void Renderer::drawhud() {
  if (game == nullptr) {
    return;
  }

  if (game->dead) {
    drawDefeatOverlay();
    return;
  }

  if (game->win) {
    drawVictoryOverlay();
    return;
  }

  const std::string title_text = "BOBMAN";
  const std::string lives_text = "x" + std::to_string(game->remaining_lives);
  const std::string score_text = "Score: " + std::to_string(game->score);
  const std::string dynamite_text = std::to_string(game->dynamite_inventory) + "x";
  const std::string plastic_explosive_text =
      std::to_string(game->plastic_explosive_inventory) + "x";
  const std::string airstrike_text =
      std::to_string(game->airstrike_inventory) + "x";
  const std::string rocket_text =
      std::to_string(game->rocket_inventory) + "x";
  const std::string biohazard_text =
      std::to_string(game->biohazard_inventory) + "x";
  const std::string nuclear_bomb_text =
      std::to_string(game->nuclear_bomb_inventory) + "x";
  const std::string love_potion_text =
      std::to_string(game->love_potion_inventory) + "x";
  const bool plastic_explosive_armed = game->plastic_explosive_is_armed;
  const bool airstrike_available =
      game->airstrike_inventory > 0 && !game->active_airstrike.is_active;
  const bool rocket_available =
      game->rocket_inventory > 0 && game->active_rockets.empty();
  const bool biohazard_available =
      game->biohazard_inventory > 0 && !game->active_biohazard_beam.is_active;
  const bool nuclear_bomb_available =
      (game->nuclear_bomb_inventory > 0 ||
       game->nuclear_bomb_target_marker.is_marked) &&
      !game->active_nuclear_bomb_drop.is_active &&
      !game->active_nuclear_explosion.is_active;
  const bool love_potion_available = game->love_potion_inventory > 0;
  struct ExtraHudSlot {
    ExtraSlot slot;
    char key_label;
    std::string count_text;
    bool key_enabled;
    bool emphasize_slot;
    bool armed;
    bool active;
  };
  const std::array<ExtraHudSlot, 7> extra_slots{{
      {ExtraSlot::Dynamite, '1', dynamite_text, game->dynamite_inventory > 0,
       game->dynamite_inventory > 0, false, false},
      {ExtraSlot::PlasticExplosive, '2', plastic_explosive_text,
       plastic_explosive_armed || game->plastic_explosive_inventory > 0,
       plastic_explosive_armed || game->plastic_explosive_inventory > 0,
       plastic_explosive_armed, false},
      {ExtraSlot::Airstrike, '3', airstrike_text, airstrike_available,
       game->airstrike_inventory > 0, false, false},
      {ExtraSlot::Rocket, '4', rocket_text, rocket_available,
       game->rocket_inventory > 0, false, false},
      {ExtraSlot::Biohazard, '5', biohazard_text, biohazard_available,
       biohazard_available || game->active_biohazard_beam.is_active, false,
       game->active_biohazard_beam.is_active},
      {ExtraSlot::NuclearBomb, '6', nuclear_bomb_text, nuclear_bomb_available,
       game->nuclear_bomb_inventory > 0 ||
           game->nuclear_bomb_target_marker.is_marked ||
           game->active_nuclear_bomb_drop.is_active,
       game->nuclear_bomb_target_marker.is_marked,
       game->active_nuclear_bomb_drop.is_active},
      {ExtraSlot::LovePotion, '7', love_potion_text, love_potion_available,
       game->love_potion_inventory > 0, false, false},
  }};
  int title_width = 0;
  int lives_text_width = 0;
  int score_width = 0;
  std::array<int, 7> extra_text_widths{};
  std::array<int, 7> extra_slot_widths{};
  TTF_SizeUTF8(sdl_font_hud, title_text.c_str(), &title_width, nullptr);
  TTF_SizeUTF8(sdl_font_hud, lives_text.c_str(), &lives_text_width, nullptr);
  TTF_SizeUTF8(sdl_font_hud, score_text.c_str(), &score_width, nullptr);

  const int hud_font_height = TTF_FontHeight(sdl_font_hud);
  const int keycap_size =
      std::max(20, static_cast<int>(std::lround(std::max(26, hud_font_height + 6) *
                                                0.86)));
  const int keycap_height = keycap_size;
  const int keycap_width = keycap_size;
  const int keycap_gap = std::max(4, hud_font_height / 6);
  const int line_top = hud_top_y + keycap_height + keycap_gap;
  const int keycap_top = line_top - keycap_height - keycap_gap;
  const int hud_gap = std::max(14, element_size / 2);
  const int icon_size = std::max(18, hud_font_height - 2);
  const int icon_text_gap = 10;

  for (size_t index = 0; index < extra_slots.size(); ++index) {
    TTF_SizeUTF8(sdl_font_hud, extra_slots[index].count_text.c_str(),
                 &extra_text_widths[index], nullptr);
    extra_slot_widths[index] =
        std::max(keycap_width, icon_size + icon_text_gap + extra_text_widths[index]);
  }

  int line_width =
      title_width + hud_gap + icon_size + 10 + lives_text_width + hud_gap +
      score_width;
  for (size_t index = 0; index < extra_slots.size(); ++index) {
    line_width += hud_gap + extra_slot_widths[index];
  }

  const Uint32 now = SDL_GetTicks();
  const double animation_clock = static_cast<double>(now);
  int cursor_x = screen_res_x / 2 - line_width / 2;
  renderTextLeft(sdl_font_hud, title_text, sdl_font_color, cursor_x, line_top);
  cursor_x += title_width + hud_gap;
  const SDL_Rect lives_icon_rect{
      cursor_x,
      line_top + std::max(0, (hud_font_height - icon_size) / 2),
      icon_size, icon_size};
  if (SDL_Texture *lives_texture = getPacmanTexture(Directions::Down, false);
      lives_texture != nullptr) {
    SDL_SetTextureAlphaMod(lives_texture, 255);
    SDL_RenderCopy(sdl_renderer, lives_texture, nullptr, &lives_icon_rect);
  }
  cursor_x += lives_icon_rect.w + 10;
  renderTextLeft(sdl_font_hud, lives_text, sdl_font_color, cursor_x, line_top);
  cursor_x += lives_text_width + hud_gap;
  renderTextLeft(sdl_font_hud, score_text, sdl_font_color, cursor_x, line_top);
  cursor_x += score_width;

  auto draw_keycap = [&](const SDL_Rect &key_rect, char key_label,
                         bool enabled) {
    const int key_index = static_cast<int>(key_label - '0');
    if (key_index >= 0 &&
        key_index < static_cast<int>(sdl_hud_keycap_textures.size())) {
      SDL_Texture *keycap_texture =
          sdl_hud_keycap_textures[static_cast<size_t>(key_index)];
      if (keycap_texture != nullptr) {
        const Uint8 alpha = enabled ? 255 : 152;
        const Uint8 tone = enabled ? 255 : 172;
        SDL_SetTextureBlendMode(keycap_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(keycap_texture, tone, tone, tone);
        SDL_SetTextureAlphaMod(keycap_texture, alpha);
        SDL_RenderCopy(sdl_renderer, keycap_texture, nullptr, &key_rect);
        SDL_SetTextureColorMod(keycap_texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(keycap_texture, 255);
        return;
      }
    }

    const Uint8 outline_alpha = enabled ? 255 : 136;
    const Uint8 label_alpha = enabled ? 255 : 150;
    const int key_depth = std::max(1, key_rect.h / 5);
    const SDL_Rect depth_rect{key_rect.x, key_rect.y + key_depth, key_rect.w,
                              key_rect.h};
    const SDL_Color outline_color{255, 255, 255, outline_alpha};
    const SDL_Color label_color{255, 255, 255, label_alpha};

    SDL_SetRenderDrawColor(sdl_renderer, outline_color.r, outline_color.g,
                           outline_color.b, outline_color.a);
    SDL_RenderDrawRect(sdl_renderer, &depth_rect);
    SDL_RenderDrawRect(sdl_renderer, &key_rect);
    SDL_RenderDrawLine(sdl_renderer, key_rect.x, key_rect.y + key_rect.h - 1,
                       depth_rect.x, depth_rect.y + depth_rect.h - 1);
    SDL_RenderDrawLine(sdl_renderer, key_rect.x + key_rect.w - 1,
                       key_rect.y + key_rect.h - 1,
                       depth_rect.x + depth_rect.w - 1,
                       depth_rect.y + depth_rect.h - 1);

    const std::string key_text(1, key_label);
    const TextCache::Entry *entry = text_cache_.GetOrCreate(
        sdl_renderer, sdl_font_hud, key_text, label_color);
    if (entry == nullptr) {
      return;
    }
    const double label_scale = 0.52;
    const int label_width =
        std::max(1, static_cast<int>(std::lround(entry->w * label_scale)));
    const int label_height =
        std::max(1, static_cast<int>(std::lround(entry->h * label_scale)));
    const SDL_Rect label_rect{
        key_rect.x + (key_rect.w - label_width) / 2,
        key_rect.y + std::max(0, (key_rect.h - label_height) / 2) - 1,
        label_width,
        label_height};
    SDL_RenderCopy(sdl_renderer, entry->texture, nullptr, &label_rect);
  };

  auto draw_extra_slot = [&](size_t index) {
    const ExtraHudSlot &slot = extra_slots[index];
    cursor_x += hud_gap;
    const int slot_left = cursor_x;
    const int slot_width = extra_slot_widths[index];
    const int slot_content_width = icon_size + icon_text_gap + extra_text_widths[index];
    const int content_left = slot_left + (slot_width - slot_content_width) / 2;
    const SDL_Rect key_rect{slot_left + (slot_width - keycap_width) / 2, keycap_top,
                            keycap_width, keycap_height};
    const SDL_Rect icon_rect{content_left,
                             line_top + std::max(0, (hud_font_height - icon_size) / 2),
                             icon_size, icon_size};
    const Uint8 icon_alpha = slot.emphasize_slot ? 255 : 120;
    const SDL_Color text_color =
        slot.emphasize_slot ? sdl_font_color : kDisabledHudTextColor;
    const bool keycap_enabled = slot.key_enabled || slot.active;

    draw_keycap(key_rect, slot.key_label, keycap_enabled);

    switch (slot.slot) {
    case ExtraSlot::Dynamite:
      drawDynamiteIcon(icon_rect, false, animation_clock / 180.0, icon_alpha, 0.0);
      break;
    case ExtraSlot::PlasticExplosive:
      drawPlasticExplosiveIcon(icon_rect, animation_clock / 170.0, icon_alpha,
                               slot.armed);
      break;
    case ExtraSlot::Airstrike:
      drawWalkieTalkieIcon(icon_rect, icon_alpha, animation_clock / 220.0);
      break;
    case ExtraSlot::Rocket:
      drawRocketIcon(icon_rect, icon_alpha, animation_clock / 190.0);
      break;
    case ExtraSlot::Biohazard:
      drawBiohazardIcon(icon_rect, icon_alpha, animation_clock / 180.0,
                        slot.active ? animation_clock * 0.75
                                    : animation_clock * 0.20,
                        slot.active ? 1.08 : 1.0);
      break;
    case ExtraSlot::NuclearBomb:
      drawNuclearBombIcon(icon_rect, icon_alpha,
                          slot.active ? std::sin(animation_clock / 120.0) * 7.0
                                      : 0.0);
      break;
    case ExtraSlot::LovePotion:
      drawLovePotionIcon(icon_rect, icon_alpha, animation_clock / 160.0);
      break;
    case ExtraSlot::None:
    default:
      break;
    }

    renderTextLeft(sdl_font_hud, slot.count_text, text_color,
                   content_left + icon_rect.w + icon_text_gap, line_top);
    cursor_x += slot_width;
  };

  for (size_t index = 0; index < extra_slots.size(); ++index) {
    draw_extra_slot(index);
  }

  if (game->pacman != nullptr && game->pacman->invulnerable_until_ticks > now) {
    const Uint32 remaining_ms = game->pacman->invulnerable_until_ticks - now;
    const int remaining_seconds =
        static_cast<int>((remaining_ms + 999) / 1000);
    renderSimpleText(sdl_font_hud,
                     "Schild: " + std::to_string(remaining_seconds) + "s",
                     kShieldTextColor, screen_res_x / 2,
                     30 + TTF_FontHeight(sdl_font_hud) + 6);
  }

}

void Renderer::drawDimmer(Uint8 alpha) {
  SDL_Rect screen_rect{0, 0, screen_res_x, screen_res_y};
  SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, alpha);
  SDL_RenderFillRect(sdl_renderer, &screen_rect);
}

void Renderer::drawPanel(const SDL_Rect &panel, const SDL_Color &fill_color,
                         const SDL_Color &border_color) {
  SDL_SetRenderDrawColor(sdl_renderer, fill_color.r, fill_color.g, fill_color.b,
                         fill_color.a);
  SDL_RenderFillRect(sdl_renderer, &panel);

  SDL_SetRenderDrawColor(sdl_renderer, border_color.r, border_color.g,
                         border_color.b, border_color.a);
  SDL_RenderDrawRect(sdl_renderer, &panel);

  SDL_Rect inner_panel{panel.x + 6, panel.y + 6, panel.w - 12, panel.h - 12};
  SDL_SetRenderDrawColor(sdl_renderer, border_color.r, border_color.g,
                         border_color.b, std::min<Uint8>(100, border_color.a));
  SDL_RenderDrawRect(sdl_renderer, &inner_panel);
}

void Renderer::drawStartMenuSpectrum(const SDL_Rect &panel) {
  const int available_band_count = Audio::kMenuSpectrumBandCount;
  if (available_band_count <= 0 || panel.w <= 1 || panel.h <= 1) {
    return;
  }

#ifdef START_MENU_SPECTRUM_GLASS_BLOCKS
  drawStartMenuSpectrumGlassBlocks(panel);
  return;
#endif

  const int outer_margin = std::max(0, START_MENU_SPECTRUM_OUTER_MARGIN);
  const int top_y = panel.y;
  const int bottom_y = panel.y + panel.h - 1;
  const int left_edge_x = panel.x;
  const int right_edge_x = panel.x + panel.w - 1;
  const int left_outer_x = outer_margin;
  const int right_outer_x = screen_res_x - outer_margin - 1;
  const int left_max_extent = left_edge_x - left_outer_x;
  const int right_max_extent = right_edge_x < right_outer_x
                                   ? (right_outer_x - right_edge_x)
                                   : 0;
  if (bottom_y <= top_y || left_max_extent < 8 || right_max_extent < 8) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  std::array<float, Audio::kMenuSpectrumBandCount> levels =
      Audio::GetMenuSpectrumLevels();

  float peak_level = 0.0f;
  for (float level : levels) {
    peak_level = std::max(peak_level, level);
  }

  if (peak_level < 0.02f) {
    const double clock = static_cast<double>(now);
    for (int band = 0; band < available_band_count; ++band) {
      const double progress =
          static_cast<double>(band) / std::max(1, available_band_count - 1);
      const double wave_a =
          std::sin(clock / 410.0 + progress * 7.4) * 0.28 + 0.34;
      const double wave_b =
          std::sin(clock / 930.0 - progress * 11.8) * 0.18 + 0.18;
      levels[static_cast<size_t>(band)] = static_cast<float>(
          std::clamp(wave_a + wave_b, 0.06, 0.72));
    }
  }

  auto build_support_points = [&](bool left_side, int edge_x,
                                  int max_extent) -> std::vector<SDL_FPoint> {
    std::vector<SDL_FPoint> support_points;
    support_points.reserve(static_cast<size_t>(available_band_count) + 2);
    support_points.push_back(
        SDL_FPoint{static_cast<float>(edge_x), static_cast<float>(top_y)});

    const float total_height = static_cast<float>(bottom_y - top_y);
    for (int band = 0; band < available_band_count; ++band) {
      const float level =
          std::clamp(levels[static_cast<size_t>(band)], 0.0f, 1.0f);
      const float eased_level = std::pow(level, 0.82f);
      const float extended_level =
          std::clamp(eased_level * 1.28f, 0.0f, 1.0f);
      const float y = static_cast<float>(top_y) +
                      total_height * static_cast<float>(band + 1) /
                          static_cast<float>(available_band_count + 1);
      const float extent = extended_level * static_cast<float>(max_extent);
      const float x = left_side ? static_cast<float>(edge_x) - extent
                                : static_cast<float>(edge_x) + extent;
      support_points.push_back(SDL_FPoint{x, y});
    }

    support_points.push_back(
        SDL_FPoint{static_cast<float>(edge_x), static_cast<float>(bottom_y)});
    return support_points;
  };

  auto sample_curve_points = [&](const std::vector<SDL_FPoint> &support_points,
                                 int edge_x,
                                 int outer_x) -> std::vector<SDL_FPoint> {
    const int sample_count = std::max(64, panel.h / 3);
    const float min_x = static_cast<float>(std::min(edge_x, outer_x));
    const float max_x = static_cast<float>(std::max(edge_x, outer_x));
    const float total_height = static_cast<float>(bottom_y - top_y);
    std::vector<SDL_FPoint> sampled_points;
    sampled_points.reserve(static_cast<size_t>(sample_count) + 1);

    size_t segment_index = 0;
    for (int sample = 0; sample <= sample_count; ++sample) {
      const float y = static_cast<float>(top_y) +
                      total_height * static_cast<float>(sample) /
                          static_cast<float>(sample_count);

      while (segment_index + 1 < support_points.size() - 1 &&
             y > support_points[segment_index + 1].y) {
        ++segment_index;
      }

      const SDL_FPoint &p0 =
          support_points[(segment_index == 0) ? 0 : segment_index - 1];
      const SDL_FPoint &p1 = support_points[segment_index];
      const SDL_FPoint &p2 =
          support_points[std::min(segment_index + 1, support_points.size() - 1)];
      const SDL_FPoint &p3 =
          support_points[std::min(segment_index + 2, support_points.size() - 1)];
      const float segment_height = std::max(1.0f, p2.y - p1.y);
      const float t = std::clamp((y - p1.y) / segment_height, 0.0f, 1.0f);
      const float x =
          std::clamp(CatmullRomInterpolate(p0.x, p1.x, p2.x, p3.x, t), min_x,
                     max_x);
      sampled_points.push_back(SDL_FPoint{x, y});
    }

    return sampled_points;
  };

  auto draw_curve_surface = [&](bool left_side) {
    const int edge_x = left_side ? left_edge_x : right_edge_x;
    const int outer_x = left_side ? left_outer_x : right_outer_x;
    const int max_extent = left_side ? left_max_extent : right_max_extent;
    const std::vector<SDL_FPoint> support_points =
        build_support_points(left_side, edge_x, max_extent);
    const std::vector<SDL_FPoint> curve_points =
        sample_curve_points(support_points, edge_x, outer_x);
    if (curve_points.size() < 2) {
      return;
    }

    constexpr int kSpectrumColumnCount = 9;
    std::vector<SDL_Vertex> vertices;
    vertices.reserve(curve_points.size() * kSpectrumColumnCount);
    std::vector<int> indices;
    indices.reserve((curve_points.size() - 1) * (kSpectrumColumnCount - 1) * 6);

    for (const SDL_FPoint &curve_point : curve_points) {
      for (int column = 0; column < kSpectrumColumnCount; ++column) {
        const float factor =
            (kSpectrumColumnCount > 1)
                ? static_cast<float>(column) /
                      static_cast<float>(kSpectrumColumnCount - 1)
                : 0.0f;
        SDL_Vertex vertex{};
        vertex.position = SDL_FPoint{
            static_cast<float>(edge_x) +
                (curve_point.x - static_cast<float>(edge_x)) * factor,
            curve_point.y};
        vertex.color =
            WithAlpha(SpectrumColor(factor), START_MENU_SPECTRUM_FILL_ALPHA);
        vertex.tex_coord = SDL_FPoint{0.0f, 0.0f};
        vertices.push_back(vertex);
      }
    }

    for (int sample = 0; sample < static_cast<int>(curve_points.size()) - 1;
         ++sample) {
      const int row_base = sample * kSpectrumColumnCount;
      const int next_row_base = (sample + 1) * kSpectrumColumnCount;
      for (int column = 0; column < kSpectrumColumnCount - 1; ++column) {
        indices.push_back(row_base + column);
        indices.push_back(row_base + column + 1);
        indices.push_back(next_row_base + column);
        indices.push_back(row_base + column + 1);
        indices.push_back(next_row_base + column + 1);
        indices.push_back(next_row_base + column);
      }
    }

    SDL_RenderGeometry(sdl_renderer, nullptr, vertices.data(),
                       static_cast<int>(vertices.size()), indices.data(),
                       static_cast<int>(indices.size()));
  };

  draw_curve_surface(true);
  draw_curve_surface(false);
}

void Renderer::ensureGlassBlockTextures(int block_w, int block_h) {
  if (sdl_renderer == nullptr || block_w <= 0 || block_h <= 0) {
    return;
  }
  if (sdl_glass_block_base_texture != nullptr &&
      sdl_glass_block_glow_texture != nullptr &&
      sdl_glass_block_texture_w == block_w &&
      sdl_glass_block_texture_h == block_h) {
    return;
  }
  if (sdl_glass_block_base_texture != nullptr) {
    SDL_DestroyTexture(sdl_glass_block_base_texture);
    sdl_glass_block_base_texture = nullptr;
  }
  if (sdl_glass_block_glow_texture != nullptr) {
    SDL_DestroyTexture(sdl_glass_block_glow_texture);
    sdl_glass_block_glow_texture = nullptr;
  }
  sdl_glass_block_texture_w = 0;
  sdl_glass_block_texture_h = 0;

  // Supersample so the rounded corner / bevel anti-aliases cleanly when SDL's
  // linear filter scales the texture down to the on-screen block size.
  constexpr int kSupersample = 4;
  const int tex_w = block_w * kSupersample;
  const int tex_h = block_h * kSupersample;
  const float corner_radius =
      static_cast<float>(START_MENU_SPECTRUM_GLASS_CORNER_RADIUS_PX) *
      static_cast<float>(kSupersample);
  const float bevel_width = std::max(
      1.0f, static_cast<float>(START_MENU_SPECTRUM_GLASS_EDGE_BEVEL_PX) *
                static_cast<float>(kSupersample));
  const float half_w = static_cast<float>(tex_w) * 0.5f;
  const float half_h = static_cast<float>(tex_h) * 0.5f;
  const float clamped_corner =
      std::min(corner_radius, std::min(half_w, half_h) - 0.5f);

  auto rounded_rect_sdf = [&](float px, float py) -> float {
    const float qx = std::abs(px - half_w) - (half_w - clamped_corner);
    const float qy = std::abs(py - half_h) - (half_h - clamped_corner);
    const float ax = std::max(qx, 0.0f);
    const float ay = std::max(qy, 0.0f);
    const float inside = std::min(std::max(qx, qy), 0.0f);
    return std::sqrt(ax * ax + ay * ay) + inside - clamped_corner;
  };

  // Light direction for the bevel highlight (top-left, slightly above).
  constexpr float kLightX = -0.5f;
  constexpr float kLightY = -0.5f;
  constexpr float kLightZ = 0.7071f;
  const float light_len =
      std::sqrt(kLightX * kLightX + kLightY * kLightY + kLightZ * kLightZ);
  const float lx = kLightX / light_len;
  const float ly = kLightY / light_len;
  const float lz = kLightZ / light_len;
  // Halfway vector between light and view (view = +z), used for specular.
  const float hx_raw = lx;
  const float hy_raw = ly;
  const float hz_raw = lz + 1.0f;
  const float h_len =
      std::sqrt(hx_raw * hx_raw + hy_raw * hy_raw + hz_raw * hz_raw);
  const float hx = hx_raw / h_len;
  const float hy = hy_raw / h_len;
  const float hz = hz_raw / h_len;

  SDL_Surface *base_surface = SDL_CreateRGBSurfaceWithFormat(
      0, tex_w, tex_h, 32, SDL_PIXELFORMAT_RGBA32);
  SDL_Surface *glow_surface = SDL_CreateRGBSurfaceWithFormat(
      0, tex_w, tex_h, 32, SDL_PIXELFORMAT_RGBA32);
  if (base_surface == nullptr || glow_surface == nullptr) {
    if (base_surface != nullptr) {
      SDL_FreeSurface(base_surface);
    }
    if (glow_surface != nullptr) {
      SDL_FreeSurface(glow_surface);
    }
    return;
  }
  SDL_LockSurface(base_surface);
  SDL_LockSurface(glow_surface);
  Uint8 *base_pixels = static_cast<Uint8 *>(base_surface->pixels);
  Uint8 *glow_pixels = static_cast<Uint8 *>(glow_surface->pixels);
  const int base_pitch = base_surface->pitch;
  const int glow_pitch = glow_surface->pitch;

  // The bevel runs from the rim inward over `bevel_width` texels. Inside the
  // bevel the surface curves up like a quarter-circle (cross-section): the
  // center is the flat top of the cuboid and the rim is where the surface
  // peels off. We feed the bevel slope into a Blinn-Phong style lighting
  // model to bake a 3D highlight + specular sparkle into the base texture.
  for (int y = 0; y < tex_h; ++y) {
    Uint8 *base_row = base_pixels + y * base_pitch;
    Uint8 *glow_row = glow_pixels + y * glow_pitch;
    const float fy = static_cast<float>(y) + 0.5f;
    for (int x = 0; x < tex_w; ++x) {
      const float fx = static_cast<float>(x) + 0.5f;
      const float sd = rounded_rect_sdf(fx, fy);
      // Smooth coverage across the rim for AA.
      const float coverage =
          std::clamp(0.5f - sd, 0.0f, 1.0f);
      if (coverage <= 0.0f) {
        for (int c = 0; c < 4; ++c) {
          base_row[x * 4 + c] = 0;
          glow_row[x * 4 + c] = 0;
        }
        continue;
      }

      // Numerical SDF gradient -> outward direction.
      const float gx =
          rounded_rect_sdf(fx + 1.0f, fy) - rounded_rect_sdf(fx - 1.0f, fy);
      const float gy =
          rounded_rect_sdf(fx, fy + 1.0f) - rounded_rect_sdf(fx, fy - 1.0f);
      float gn = std::sqrt(gx * gx + gy * gy);
      if (gn < 1e-4f) {
        gn = 1.0f;
      }
      const float ox = gx / gn;
      const float oy = gy / gn;

      // Surface height profile: 0 at the rim, 1 at the flat top.
      const float edge_dist = std::max(0.0f, -sd);
      const float bevel_t = std::clamp(edge_dist / bevel_width, 0.0f, 1.0f);
      const float bevel_height =
          std::sqrt(std::max(0.0f, 2.0f * bevel_t - bevel_t * bevel_t));
      const float cos_theta =
          std::sqrt(std::max(0.0f, 1.0f - bevel_height * bevel_height));
      const float nx = ox * cos_theta;
      const float ny = oy * cos_theta;
      const float nz = bevel_height;

      const float diffuse = std::max(0.0f, nx * lx + ny * ly + nz * lz);
      const float spec_dot = std::max(0.0f, nx * hx + ny * hy + nz * hz);
      const float specular = std::pow(spec_dot, 28.0f);

      // Strength of the baked 3D illumination. At 0 the block is a flat
      // frosted slab, at 1 the bevel highlight + specular sparkle + rim
      // refraction are at their nominal balance, and >1 exaggerates the
      // plastic "glass cuboid" look.
      const float strength = std::max(
          0.0f, START_MENU_SPECTRUM_GLASS_3D_STRENGTH);
      const float diffuse_term = 0.55f * diffuse;
      const float specular_term = 0.55f * specular;
      // Lerp the lighting variations against a flat reference (full ambient,
      // baseline alpha) so strength=0 truly flattens the block.
      const float body_brightness = std::clamp(
          1.0f + strength * ((0.45f + diffuse_term + specular_term) - 1.0f),
          0.0f, 1.4f);
      // Edge thickening: rim is more opaque (refraction look), center is
      // milkier. The rim contrast scales with bevel height so widening or
      // narrowing the bevel materially changes the visible 3D rim.
      const float rim_factor = 1.0f - bevel_height;
      const float body_alpha = std::clamp(
          0.16f + strength * (0.62f * rim_factor + 0.65f * specular), 0.0f,
          1.0f);

      const Uint8 base_channel = static_cast<Uint8>(std::clamp(
          body_brightness * 255.0f, 0.0f, 255.0f));
      const Uint8 base_alpha = static_cast<Uint8>(std::clamp(
          body_alpha * coverage * 255.0f, 0.0f, 255.0f));
      base_row[x * 4 + 0] = base_channel;
      base_row[x * 4 + 1] = base_channel;
      base_row[x * 4 + 2] = base_channel;
      base_row[x * 4 + 3] = base_alpha;

      // Glow: point light at center, falling off with squared distance.
      // Multiplied by a smooth rim mask so the diffusion stays inside the
      // glass and never hits the rounded edge sharply.
      const float dx = (fx - half_w) / half_w;
      const float dy = (fy - half_h) / half_h;
      const float r2 = dx * dx + dy * dy;
      const float core = std::exp(-r2 * 3.6f);
      const float halo = std::exp(-r2 * 1.05f) * 0.45f;
      float intensity = core + halo;
      // Smooth attenuation across the bevel so the rim still glows but a bit
      // softer than the lit interior.
      const float rim_smooth =
          0.45f + 0.55f * (bevel_t * bevel_t * (3.0f - 2.0f * bevel_t));
      intensity *= rim_smooth;
      intensity = std::clamp(intensity, 0.0f, 1.0f);
      const Uint8 glow_alpha = static_cast<Uint8>(
          std::clamp(intensity * coverage * 255.0f, 0.0f, 255.0f));
      glow_row[x * 4 + 0] = 255;
      glow_row[x * 4 + 1] = 255;
      glow_row[x * 4 + 2] = 255;
      glow_row[x * 4 + 3] = glow_alpha;
    }
  }
  SDL_UnlockSurface(base_surface);
  SDL_UnlockSurface(glow_surface);

  sdl_glass_block_base_texture =
      SDL_CreateTextureFromSurface(sdl_renderer, base_surface);
  sdl_glass_block_glow_texture =
      SDL_CreateTextureFromSurface(sdl_renderer, glow_surface);
  SDL_FreeSurface(base_surface);
  SDL_FreeSurface(glow_surface);
  if (sdl_glass_block_base_texture == nullptr ||
      sdl_glass_block_glow_texture == nullptr) {
    if (sdl_glass_block_base_texture != nullptr) {
      SDL_DestroyTexture(sdl_glass_block_base_texture);
      sdl_glass_block_base_texture = nullptr;
    }
    if (sdl_glass_block_glow_texture != nullptr) {
      SDL_DestroyTexture(sdl_glass_block_glow_texture);
      sdl_glass_block_glow_texture = nullptr;
    }
    return;
  }
  SDL_SetTextureBlendMode(sdl_glass_block_base_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(sdl_glass_block_base_texture, SDL_ScaleModeLinear);
  SDL_SetTextureBlendMode(sdl_glass_block_glow_texture, SDL_BLENDMODE_ADD);
  SDL_SetTextureScaleMode(sdl_glass_block_glow_texture, SDL_ScaleModeLinear);
  sdl_glass_block_texture_w = block_w;
  sdl_glass_block_texture_h = block_h;
}

void Renderer::drawGlassBlock(const SDL_Rect &block_rect,
                              const SDL_Color &color, float lit_amount) {
  if (sdl_glass_block_base_texture == nullptr ||
      sdl_glass_block_glow_texture == nullptr || block_rect.w <= 0 ||
      block_rect.h <= 0) {
    return;
  }
  lit_amount = std::clamp(lit_amount, 0.0f, 1.0f);

  // Tint the base slightly with the column color even when unlit, so the
  // colour ramp is faintly visible across the row. When lit, lift the tint
  // toward the full colour to stop the glass looking grey through the glow.
  const float tint_mix = 0.22f + 0.55f * lit_amount;
  auto blend_channel = [&](Uint8 ch) -> Uint8 {
    const float base_grey = 90.0f;
    const float blended =
        base_grey + (static_cast<float>(ch) - base_grey) * tint_mix;
    return static_cast<Uint8>(std::clamp(blended, 0.0f, 255.0f));
  };
  const Uint8 base_r = blend_channel(color.r);
  const Uint8 base_g = blend_channel(color.g);
  const Uint8 base_b = blend_channel(color.b);
  const Uint8 base_alpha = static_cast<Uint8>(std::clamp(
      170.0f + 50.0f * lit_amount, 0.0f, 255.0f));

  SDL_SetTextureColorMod(sdl_glass_block_base_texture, base_r, base_g, base_b);
  SDL_SetTextureAlphaMod(sdl_glass_block_base_texture, base_alpha);
  SDL_RenderCopy(sdl_renderer, sdl_glass_block_base_texture, nullptr,
                 &block_rect);

  if (lit_amount > 0.0f) {
    // Boost the glow non-linearly so a partially-lit block still reads as
    // "on" without immediately being as bright as a fully-lit one.
    const float glow_curve =
        lit_amount * (0.55f + 0.45f * lit_amount);
    const Uint8 glow_alpha = static_cast<Uint8>(std::clamp(
        glow_curve * 255.0f, 0.0f, 255.0f));
    SDL_SetTextureColorMod(sdl_glass_block_glow_texture, color.r, color.g,
                           color.b);
    SDL_SetTextureAlphaMod(sdl_glass_block_glow_texture, glow_alpha);
    SDL_RenderCopy(sdl_renderer, sdl_glass_block_glow_texture, nullptr,
                   &block_rect);

    // Halo bleed: a soft puff just outside the block sells the diffusion
    // through the glass when the block is brightly lit.
    if (sdl_soft_puff_texture != nullptr && lit_amount > 0.25f) {
      const float halo_strength =
          (lit_amount - 0.25f) / 0.75f;
      const int halo_extra =
          static_cast<int>(std::round(halo_strength * 6.0f));
      const SDL_Rect halo_rect{block_rect.x - halo_extra,
                               block_rect.y - halo_extra,
                               block_rect.w + halo_extra * 2,
                               block_rect.h + halo_extra * 2};
      const Uint8 prev_blend_r = color.r;
      const Uint8 prev_blend_g = color.g;
      const Uint8 prev_blend_b = color.b;
      const Uint8 halo_alpha = static_cast<Uint8>(std::clamp(
          halo_strength * 90.0f, 0.0f, 255.0f));
      SDL_BlendMode previous_mode = SDL_BLENDMODE_BLEND;
      SDL_GetTextureBlendMode(sdl_soft_puff_texture, &previous_mode);
      SDL_SetTextureBlendMode(sdl_soft_puff_texture, SDL_BLENDMODE_ADD);
      SDL_SetTextureColorMod(sdl_soft_puff_texture, prev_blend_r, prev_blend_g,
                             prev_blend_b);
      SDL_SetTextureAlphaMod(sdl_soft_puff_texture, halo_alpha);
      SDL_RenderCopy(sdl_renderer, sdl_soft_puff_texture, nullptr, &halo_rect);
      SDL_SetTextureBlendMode(sdl_soft_puff_texture, previous_mode);
      SDL_SetTextureColorMod(sdl_soft_puff_texture, 255, 255, 255);
      SDL_SetTextureAlphaMod(sdl_soft_puff_texture, 255);
    }
  }

  SDL_SetTextureColorMod(sdl_glass_block_base_texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(sdl_glass_block_base_texture, 255);
  SDL_SetTextureColorMod(sdl_glass_block_glow_texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(sdl_glass_block_glow_texture, 255);
}

void Renderer::drawStartMenuSpectrumGlassBlocks(const SDL_Rect &panel) {
  const int available_band_count = Audio::kMenuSpectrumBandCount;
  if (available_band_count <= 0 || panel.w <= 1 || panel.h <= 1) {
    return;
  }

  const int bar_count = std::clamp(START_MENU_SPECTRUM_GLASS_BAR_COUNT, 1,
                                   available_band_count);
  const int blocks_per_bar =
      std::max(1, START_MENU_SPECTRUM_GLASS_BLOCKS_PER_BAR);
  const int block_gap = std::max(0, START_MENU_SPECTRUM_GLASS_BLOCK_GAP_PX);
  const int bar_gap = std::max(0, START_MENU_SPECTRUM_GLASS_BAR_GAP_PX);
  const int panel_margin =
      std::max(0, START_MENU_SPECTRUM_GLASS_PANEL_MARGIN_PX);
  const int screen_margin =
      std::max(0, START_MENU_SPECTRUM_GLASS_SCREEN_MARGIN_PX);

  const int left_zone_inner = panel.x - panel_margin;
  const int left_zone_outer = screen_margin;
  const int left_zone_width = left_zone_inner - left_zone_outer;
  const int right_zone_inner = panel.x + panel.w + panel_margin;
  const int right_zone_outer = screen_res_x - screen_margin;
  const int right_zone_width = right_zone_outer - right_zone_inner;
  const int min_zone_width =
      blocks_per_bar * 3 + (blocks_per_bar - 1) * block_gap;
  if (left_zone_width < min_zone_width || right_zone_width < min_zone_width) {
    return;
  }

  const int total_block_gap = (blocks_per_bar - 1) * block_gap;
  const int block_width =
      std::max(2, (std::min(left_zone_width, right_zone_width) -
                   total_block_gap) /
                      blocks_per_bar);
  const int total_bar_gap = (bar_count - 1) * bar_gap;
  const int block_height =
      std::max(2, (panel.h - total_bar_gap) / bar_count);

  ensureGlassBlockTextures(block_width, block_height);
  if (sdl_glass_block_base_texture == nullptr ||
      sdl_glass_block_glow_texture == nullptr) {
    return;
  }

  // Fetch FFT levels and supply a gentle idle animation when nothing is
  // playing, so the equalizer never freezes flat.
  std::array<float, Audio::kMenuSpectrumBandCount> levels =
      Audio::GetMenuSpectrumLevels();
  float peak_level = 0.0f;
  for (float level : levels) {
    peak_level = std::max(peak_level, level);
  }
  const Uint32 now = SDL_GetTicks();
  if (peak_level < 0.02f) {
    const double clock = static_cast<double>(now);
    for (int band = 0; band < available_band_count; ++band) {
      const double progress =
          static_cast<double>(band) / std::max(1, available_band_count - 1);
      const double wave_a =
          std::sin(clock / 410.0 + progress * 7.4) * 0.22 + 0.28;
      const double wave_b =
          std::sin(clock / 930.0 - progress * 11.8) * 0.14 + 0.16;
      levels[static_cast<size_t>(band)] = static_cast<float>(
          std::clamp(wave_a + wave_b, 0.04, 0.62));
    }
  }

  // Aggregate adjacent bands so we can drive an arbitrary bar count.
  std::vector<float> bar_levels(static_cast<size_t>(bar_count), 0.0f);
  for (int bar = 0; bar < bar_count; ++bar) {
    const int start =
        bar * available_band_count / bar_count;
    const int end = std::max(start + 1,
                             (bar + 1) * available_band_count / bar_count);
    float sum = 0.0f;
    for (int b = start; b < end; ++b) {
      sum += std::clamp(levels[static_cast<size_t>(b)], 0.0f, 1.0f);
    }
    float avg = sum / static_cast<float>(end - start);
    avg = std::pow(avg, 0.78f);
    bar_levels[static_cast<size_t>(bar)] = std::clamp(avg, 0.0f, 1.0f);
  }

  const int total_bars_height =
      bar_count * block_height + total_bar_gap;
  const int bars_origin_y =
      panel.y + std::max(0, (panel.h - total_bars_height) / 2);

  for (int bar = 0; bar < bar_count; ++bar) {
    const float level = bar_levels[static_cast<size_t>(bar)];
    const int bar_y = bars_origin_y + bar * (block_height + bar_gap);

    for (int side = 0; side < 2; ++side) {
      const bool is_left = (side == 0);
      for (int col = 0; col < blocks_per_bar; ++col) {
        const float color_factor =
            (blocks_per_bar > 1)
                ? static_cast<float>(col) /
                      static_cast<float>(blocks_per_bar - 1)
                : 0.0f;
        const SDL_Color color = SpectrumColor(color_factor);

        int block_x;
        if (is_left) {
          block_x = left_zone_inner - block_width -
                    col * (block_width + block_gap);
        } else {
          block_x = right_zone_inner + col * (block_width + block_gap);
        }

        // VU-meter style threshold: each column corresponds to a level
        // notch; the column on the threshold edge fades smoothly so the
        // bars feel analogue rather than snapping.
        const float lower =
            static_cast<float>(col) / static_cast<float>(blocks_per_bar);
        const float upper = static_cast<float>(col + 1) /
                            static_cast<float>(blocks_per_bar);
        float lit_amount = 0.0f;
        if (level >= upper) {
          lit_amount = 1.0f;
        } else if (level > lower) {
          lit_amount = (level - lower) / (upper - lower);
        }

        const SDL_Rect block_rect{block_x, bar_y, block_width, block_height};
        drawGlassBlock(block_rect, color, lit_amount);
      }
    }
  }
}

void Renderer::drawStartMenuOverlay(int selected_item,
                                    const std::string &map_name,
                                    const std::string &status_message) {
  const int logo_top = screen_res_y / 18;
  const int logo_center_x = screen_res_x / 2;
  int logo_width = 0;
  int logo_height = TTF_FontHeight(sdl_font_logo);
  if (TTF_SizeUTF8(sdl_font_logo, "BobMan", &logo_width, &logo_height) != 0) {
    logo_width = screen_res_x / 3;
    logo_height = TTF_FontHeight(sdl_font_logo);
  }
  const int logo_left = logo_center_x - logo_width / 2;
  const int logo_right = logo_center_x + logo_width / 2;

  const std::vector<std::string> menu_items{
      "Start Spiel", "Karte: " + map_name, "Map Editor", "Konfiguration",
      "Ende"};
  const int item_height = std::max(52, TTF_FontHeight(sdl_font_menu) + 18);
  const int panel_width = std::min(780, screen_res_x * 46 / 100);
  const int panel_height =
      std::max(360, 88 + static_cast<int>(menu_items.size()) * item_height);
  const int header_side_margin = std::max(18, screen_res_x / 34);
  const int mascot_gap = std::max(10, screen_res_x / 130);
  const int available_mascot_width =
      std::max(0, (screen_res_x - logo_width - header_side_margin * 2 -
                   mascot_gap * 2) /
                      2);
  const int mascot_target_height =
      std::clamp(static_cast<int>(logo_height * 1.42), 120, screen_res_y / 4);

  auto scaled_mascot_size = [&](const SDL_Point &source_size) {
    SDL_Point scaled_size{0, 0};
    if (source_size.x <= 0 || source_size.y <= 0 || available_mascot_width < 96) {
      return scaled_size;
    }

    const double scale_by_width =
        static_cast<double>(available_mascot_width) /
        static_cast<double>(source_size.x);
    const double scale_by_height =
        static_cast<double>(mascot_target_height) /
        static_cast<double>(source_size.y);
    const double scale = std::min(scale_by_width, scale_by_height);
    if (scale <= 0.0) {
      return scaled_size;
    }

    scaled_size.x =
        std::max(1, static_cast<int>(std::lround(source_size.x * scale)));
    scaled_size.y =
        std::max(1, static_cast<int>(std::lround(source_size.y * scale)));
    return scaled_size;
  };

  const SDL_Point monster_size = scaled_mascot_size(sdl_start_menu_monster_size);
  const SDL_Point hero_size = scaled_mascot_size(sdl_start_menu_hero_size);
  SDL_Rect monster_rect{0, 0, 0, 0};
  SDL_Rect hero_rect{0, 0, 0, 0};
  if (monster_size.x > 0 && monster_size.y > 0) {
    monster_rect = SDL_Rect{
        std::max(header_side_margin, logo_left - mascot_gap - monster_size.x),
        std::max(8, logo_top + (logo_height - monster_size.y) / 2 + logo_height / 16),
        monster_size.x, monster_size.y};
  }
  if (hero_size.x > 0 && hero_size.y > 0) {
    hero_rect = SDL_Rect{
        std::min(screen_res_x - header_side_margin - hero_size.x,
                 logo_right + mascot_gap),
        std::max(8, logo_top + (logo_height - hero_size.y) / 2 + logo_height / 10),
        hero_size.x, hero_size.y};
  }

  int header_bottom = logo_top + logo_height;
  if (monster_rect.w > 0 && monster_rect.h > 0) {
    header_bottom = std::max(header_bottom, monster_rect.y + monster_rect.h);
  }
  if (hero_rect.w > 0 && hero_rect.h > 0) {
    header_bottom = std::max(header_bottom, hero_rect.y + hero_rect.h);
  }
  const int panel_top =
      std::min(screen_res_y - panel_height - 70,
               header_bottom + screen_res_y / 28);
  SDL_Rect panel{(screen_res_x - panel_width) / 2, panel_top, panel_width,
                 panel_height};

  renderDecorativeTexture(sdl_start_menu_monster_texture, monster_rect,
                          SDL_Color{180, 96, 255, 255}, 0.25, 0.055, 0.07, 0.62);
  renderDecorativeTexture(sdl_start_menu_hero_texture, hero_rect,
                          SDL_Color{88, 196, 255, 255}, 1.35, 0.0, 0.0, 0.0);
  renderStartLogo(sdl_font_logo, "BobMan", logo_center_x, logo_top);

  drawStartMenuSpectrum(panel);
  drawPanel(panel, kStartMenuPanelFillColor, kStartMenuPanelBorderColor);

  const int highlight_width = panel.w - 56;
  const int item_start_y =
      panel.y + 22 + std::max(0, (panel.h - 44 - static_cast<int>(menu_items.size()) *
                                               item_height) /
                                       2);

  for (int i = 0; i < static_cast<int>(menu_items.size()); i++) {
    const SDL_Rect highlight_rect{panel.x + 28, item_start_y + i * item_height,
                                  highlight_width, item_height - 8};
    if (i == selected_item) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);

      SDL_Rect accent_rect{highlight_rect.x + 10, highlight_rect.y + 8, 10,
                           highlight_rect.h - 16};
      SDL_SetRenderDrawColor(sdl_renderer, 255, 215, 160, 255);
      SDL_RenderFillRect(sdl_renderer, &accent_rect);
    }

    const SDL_Color item_color =
        (i == selected_item) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;
    renderSimpleText(sdl_font_menu, menu_items[i], item_color, screen_res_x / 2,
                     text_top);
  }

  if (!status_message.empty()) {
    renderSimpleText(sdl_font_hud, status_message, kStatusTextColor,
                     screen_res_x / 2,
                     panel.y + panel.h - TTF_FontHeight(sdl_font_hud) - 22);
  }
}

void Renderer::drawConfigMenuOverlay(int selected_item, Difficulty difficulty,
                                     int player_lives, int floor_texture_index) {
  const int logo_top = screen_res_y / 18;
  renderBrickText(sdl_font_logo, "BobMan", screen_res_x / 2, logo_top,
                  kBrickOutlineColor);
  renderSimpleText(sdl_font_menu, "Konfiguration", kHudTextColor,
                   screen_res_x / 2,
                   logo_top + TTF_FontHeight(sdl_font_logo) - 4);

  const int panel_width = std::min(860, screen_res_x * 52 / 100);
  const int panel_height = std::max(392, screen_res_y * 42 / 100);
  const int panel_top =
      std::min(screen_res_y - panel_height - 70,
               logo_top + TTF_FontHeight(sdl_font_logo) + screen_res_y / 14);
  SDL_Rect panel{(screen_res_x - panel_width) / 2, panel_top, panel_width,
                 panel_height};

  drawPanel(panel, kPanelFillColor, kPanelBorderColor);

  const std::vector<std::string> menu_items{
      "Schwierigkeitsgrad", "Leben", "Bodentextur", "Zurueck"};
  const std::vector<std::string> value_items{DifficultyLabel(difficulty),
                                             std::to_string(player_lives),
                                             FloorTextureLabel(floor_texture_index),
                                             ""};
  const int item_height = std::max(62, TTF_FontHeight(sdl_font_menu) + 24);
  const int highlight_width = panel.w - 56;
  const int item_start_y =
      panel.y + 32 +
      std::max(0, (panel.h - 86 - static_cast<int>(menu_items.size()) * item_height) /
                       2);

  for (int i = 0; i < static_cast<int>(menu_items.size()); i++) {
    const SDL_Rect highlight_rect{panel.x + 28, item_start_y + i * item_height,
                                  highlight_width, item_height - 8};
    if (i == selected_item) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    }

    const SDL_Color item_color =
        (i == selected_item) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;

    if (i < 3) {
      renderSimpleText(sdl_font_menu, menu_items[i], item_color,
                       panel.x + panel.w / 3, text_top);
      renderSimpleText(sdl_font_menu, value_items[i], item_color,
                       panel.x + panel.w * 3 / 4, text_top);
    } else {
      renderSimpleText(sdl_font_menu, menu_items[i], item_color, screen_res_x / 2,
                       text_top);
    }
  }
}

void Renderer::drawMapSelectionOverlay(const std::vector<std::string> &map_names,
                                       int selected_index) {
  const int logo_top = screen_res_y / 18;
  renderBrickText(sdl_font_logo, "BobMan", screen_res_x / 2, logo_top,
                  kBrickOutlineColor);
  renderSimpleText(sdl_font_menu, "Kartenauswahl", kHudTextColor,
                   screen_res_x / 2,
                   logo_top + TTF_FontHeight(sdl_font_logo) - 4);

  const int item_height = std::max(52, TTF_FontHeight(sdl_font_menu) + 18);
  const int panel_width = std::min(860, screen_res_x * 50 / 100);
  const int desired_panel_height =
      136 + static_cast<int>(map_names.size()) * item_height;
  const int panel_height = std::min(std::max(320, desired_panel_height),
                                    screen_res_y * 72 / 100);
  const int panel_top =
      std::min(screen_res_y - panel_height - 70,
               logo_top + TTF_FontHeight(sdl_font_logo) + screen_res_y / 14);
  SDL_Rect panel{(screen_res_x - panel_width) / 2, panel_top, panel_width,
                 panel_height};

  drawPanel(panel, kPanelFillColor, kPanelBorderColor);

  const int highlight_width = panel.w - 56;
  const int max_visible_items =
      std::max(1, (panel.h - 72) / std::max(1, item_height));
  const int first_visible =
      std::clamp(selected_index - max_visible_items / 2, 0,
                 std::max(0, static_cast<int>(map_names.size()) - max_visible_items));
  const int visible_items =
      std::min(max_visible_items,
               static_cast<int>(map_names.size()) - first_visible);
  const int item_start_y =
      panel.y + 28 +
      std::max(0, (panel.h - 72 - visible_items * item_height) / 2);

  for (int visible_index = 0; visible_index < visible_items; visible_index++) {
    const int i = first_visible + visible_index;
    const SDL_Rect highlight_rect{panel.x + 28,
                                  item_start_y + visible_index * item_height,
                                  highlight_width, item_height - 8};
    if (i == selected_index) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    }

    const SDL_Color item_color =
        (i == selected_index) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;
    renderSimpleText(sdl_font_menu, map_names[i], item_color, screen_res_x / 2,
                     text_top);
  }
  if (first_visible > 0) {
    renderSimpleText(sdl_font_hud, "...", kHudTextColor, screen_res_x / 2,
                     panel.y + 6);
  }
  if (first_visible + visible_items < static_cast<int>(map_names.size())) {
    renderSimpleText(sdl_font_hud, "...", kHudTextColor, screen_res_x / 2,
                     panel.y + panel.h - TTF_FontHeight(sdl_font_hud) - 8);
  }
}

void Renderer::drawEditorSelectionOverlay(const std::vector<std::string> &items,
                                          int selected_index) {
  const int logo_top = screen_res_y / 18;
  renderBrickText(sdl_font_logo, "BobMan", screen_res_x / 2, logo_top,
                  kBrickOutlineColor);
  renderSimpleText(sdl_font_menu, "Map Editor", kHudTextColor, screen_res_x / 2,
                   logo_top + TTF_FontHeight(sdl_font_logo) - 4);

  const int item_height = std::max(50, TTF_FontHeight(sdl_font_menu) + 16);
  const int panel_width = std::min(900, screen_res_x * 56 / 100);
  const int desired_panel_height =
      138 + static_cast<int>(items.size()) * item_height;
  const int panel_height = std::min(std::max(340, desired_panel_height),
                                    screen_res_y * 74 / 100);
  const int panel_top =
      std::min(screen_res_y - panel_height - 70,
               logo_top + TTF_FontHeight(sdl_font_logo) + screen_res_y / 14);
  SDL_Rect panel{(screen_res_x - panel_width) / 2, panel_top, panel_width,
                 panel_height};

  drawPanel(panel, kPanelFillColor, kPanelBorderColor);

  const int highlight_width = panel.w - 56;
  const int max_visible_items =
      std::max(1, (panel.h - 78) / std::max(1, item_height));
  const int first_visible =
      std::clamp(selected_index - max_visible_items / 2, 0,
                 std::max(0, static_cast<int>(items.size()) - max_visible_items));
  const int visible_items =
      std::min(max_visible_items,
               static_cast<int>(items.size()) - first_visible);
  const int item_start_y =
      panel.y + 30 +
      std::max(0, (panel.h - 78 - visible_items * item_height) / 2);

  for (int visible_index = 0; visible_index < visible_items; visible_index++) {
    const int i = first_visible + visible_index;
    const SDL_Rect highlight_rect{panel.x + 28,
                                  item_start_y + visible_index * item_height,
                                  highlight_width, item_height - 8};
    if (i == selected_index) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    }

    const SDL_Color item_color =
        (i == selected_index) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;
    renderSimpleText(sdl_font_menu, items[i], item_color, screen_res_x / 2,
                     text_top);
  }
  if (first_visible > 0) {
    renderSimpleText(sdl_font_hud, "...", kHudTextColor, screen_res_x / 2,
                     panel.y + 6);
  }
  if (first_visible + visible_items < static_cast<int>(items.size())) {
    renderSimpleText(sdl_font_hud, "...", kHudTextColor, screen_res_x / 2,
                     panel.y + panel.h - TTF_FontHeight(sdl_font_hud) - 8);
  }
}

void Renderer::drawEditorSizeSelectionOverlay(int selected_index) {
  const int logo_top = screen_res_y / 18;
  renderBrickText(sdl_font_logo, "BobMan", screen_res_x / 2, logo_top,
                  kBrickOutlineColor);
  renderSimpleText(sdl_font_menu, "Neue Karte", kHudTextColor,
                   screen_res_x / 2,
                   logo_top + TTF_FontHeight(sdl_font_logo) - 4);

  const std::vector<std::string> items{"Klein", "Mittel", "Gross"};
  const int item_height = std::max(56, TTF_FontHeight(sdl_font_menu) + 18);
  const int panel_width = std::min(740, screen_res_x * 46 / 100);
  const int panel_height = std::max(340, screen_res_y * 40 / 100);
  const int panel_top =
      std::min(screen_res_y - panel_height - 70,
               logo_top + TTF_FontHeight(sdl_font_logo) + screen_res_y / 14);
  SDL_Rect panel{(screen_res_x - panel_width) / 2, panel_top, panel_width,
                 panel_height};

  drawPanel(panel, kPanelFillColor, kPanelBorderColor);

  const int highlight_width = panel.w - 56;
  const int item_start_y =
      panel.y + 38 +
      std::max(0, (panel.h - 94 - static_cast<int>(items.size()) * item_height) /
                       2);

  for (int i = 0; i < static_cast<int>(items.size()); i++) {
    const SDL_Rect highlight_rect{panel.x + 28, item_start_y + i * item_height,
                                  highlight_width, item_height - 8};
    if (i == selected_index) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    }

    const SDL_Color item_color =
        (i == selected_index) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;
    renderSimpleText(sdl_font_menu, items[i], item_color, screen_res_x / 2,
                     text_top);
  }
}

void Renderer::drawEditorOverlay(const std::string &map_name, MapCoord cursor,
                                 const std::string &warning_message,
                                 bool show_exit_dialog,
                                 int exit_dialog_selected,
                                 bool show_name_dialog,
                                 const std::string &name_input,
                                 const std::string &name_dialog_message) {
  const int header_top = 12;
  renderBrickText(sdl_font_menu, "Map Editor", screen_res_x / 2, header_top,
                  kBrickOutlineColor);
  renderSimpleText(sdl_font_hud, map_name, kHudTextColor, screen_res_x / 2,
                   header_top + TTF_FontHeight(sdl_font_menu) + 4);

  const int x = offset_x + 1 + cursor.v * (element_size + 1);
  const int y = offset_y + 1 + cursor.u * (element_size + 1);
  const bool blink_on = ((SDL_GetTicks() / 420) % 2) == 0;
  const Uint8 alpha = blink_on ? 255 : 120;
  SDL_Rect cursor_rect{x - 2, y - 2, element_size + 4, element_size + 4};
  SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, alpha);
  SDL_RenderDrawRect(sdl_renderer, &cursor_rect);
  SDL_Rect inner_cursor{x - 1, y - 1, element_size + 2, element_size + 2};
  SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, alpha / 2);
  SDL_RenderDrawRect(sdl_renderer, &inner_cursor);

  const int info_width = std::min(760, screen_res_x - 80);
  const int info_height = std::max(170, screen_res_y / 4);
  SDL_Rect info_panel{(screen_res_x - info_width) / 2, screen_res_y - info_height - 28,
                      info_width, info_height};
  SDL_SetRenderDrawColor(sdl_renderer, 10, 6, 18, 92);
  SDL_RenderFillRect(sdl_renderer, &info_panel);
  SDL_SetRenderDrawColor(sdl_renderer, 196, 130, 92, 150);
  SDL_RenderDrawRect(sdl_renderer, &info_panel);
  SDL_Rect info_inner_panel{info_panel.x + 6, info_panel.y + 6,
                            info_panel.w - 12, info_panel.h - 12};
  SDL_SetRenderDrawColor(sdl_renderer, 196, 130, 92, 70);
  SDL_RenderDrawRect(sdl_renderer, &info_inner_panel);

  const std::string line_one =
      "Pfeile oder Linksklick | Klick schaltet Mauer an/aus";
  const std::string line_two =
      "X/W=Mauer | Leer/Entf=Weg | G=Goodie | P=Spielfigur";
  const std::string line_three =
      "M/N/O/K=Monster | Z=Ziege | 1-5=Teleportpaare";
  const std::string line_four =
      "Belegte Felder blockieren den Klick | Esc oeffnet Dialog";
  renderSimpleText(sdl_font_hud, line_one, kHudTextColor, screen_res_x / 2,
                   info_panel.y + 16);
  renderSimpleText(sdl_font_hud, line_two, kHudTextColor, screen_res_x / 2,
                   info_panel.y + 16 + TTF_FontHeight(sdl_font_hud) + 6);
  renderSimpleText(sdl_font_hud, line_three, kHudTextColor, screen_res_x / 2,
                   info_panel.y + 16 + 2 * (TTF_FontHeight(sdl_font_hud) + 6));
  renderSimpleText(sdl_font_hud, line_four, kHudTextColor, screen_res_x / 2,
                   info_panel.y + 16 + 3 * (TTF_FontHeight(sdl_font_hud) + 6));

  const SDL_Color status_color =
      warning_message.empty() ? kStatusTextColor : kWarningTextColor;
  const std::string status_text =
      warning_message.empty() ? "Karte ist gueltig" : warning_message;
  renderSimpleText(sdl_font_hud, status_text, status_color, screen_res_x / 2,
                   info_panel.y + info_panel.h - TTF_FontHeight(sdl_font_hud) - 14);

  if (!show_exit_dialog && !show_name_dialog) {
    return;
  }

  drawDimmer(160);
  if (show_name_dialog) {
    const int dialog_width = std::min(820, screen_res_x * 48 / 100);
    const int dialog_height = std::max(280, screen_res_y * 30 / 100);
    SDL_Rect dialog{(screen_res_x - dialog_width) / 2,
                    (screen_res_y - dialog_height) / 2, dialog_width,
                    dialog_height};
    drawPanel(dialog, kPanelFillColor, kPanelBorderColor);
    renderSimpleText(sdl_font_menu, "Kartenname", kHudTextColor,
                     screen_res_x / 2, dialog.y + 22);

    SDL_Rect input_rect{dialog.x + 40, dialog.y + 92, dialog.w - 80, 68};
    SDL_SetRenderDrawColor(sdl_renderer, 22, 16, 32, 220);
    SDL_RenderFillRect(sdl_renderer, &input_rect);
    SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
    SDL_RenderDrawRect(sdl_renderer, &input_rect);

    std::string visible_name = name_input;
    if (((SDL_GetTicks() / 500) % 2) == 0) {
      visible_name.push_back('_');
    }
    if (visible_name.empty()) {
      visible_name = "_";
    }
    renderSimpleText(sdl_font_menu, visible_name, kSelectedMenuTextColor,
                     screen_res_x / 2,
                     input_rect.y +
                         (input_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2);

    const SDL_Color message_color =
        name_dialog_message.empty() ? kHudTextColor : kWarningTextColor;
    const std::string message_text =
        name_dialog_message.empty() ? "Enter speichert, Esc kehrt zum Dialog zurueck"
                                    : name_dialog_message;
    renderSimpleText(sdl_font_hud, message_text, message_color,
                     screen_res_x / 2, dialog.y + dialog.h - 56);
    return;
  }

  const int dialog_width = std::min(720, screen_res_x * 42 / 100);
  const int dialog_height = std::max(260, screen_res_y * 28 / 100);
  SDL_Rect dialog{(screen_res_x - dialog_width) / 2,
                  (screen_res_y - dialog_height) / 2, dialog_width,
                  dialog_height};
  drawPanel(dialog, kPanelFillColor, kPanelBorderColor);
  renderSimpleText(sdl_font_menu, "Editor verlassen?", kHudTextColor,
                   screen_res_x / 2, dialog.y + 24);

  const std::vector<std::string> items{"Speichern", "Abbruch",
                                       "Nicht speichern"};
  const int item_height = std::max(52, TTF_FontHeight(sdl_font_menu) + 16);
  const int start_y = dialog.y + 82;
  for (int i = 0; i < static_cast<int>(items.size()); i++) {
    SDL_Rect highlight_rect{dialog.x + 28, start_y + i * item_height,
                            dialog.w - 56, item_height - 8};
    if (i == exit_dialog_selected) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    }

    const SDL_Color item_color =
        (i == exit_dialog_selected) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;
    renderSimpleText(sdl_font_menu, items[i], item_color, screen_res_x / 2,
                     text_top);
  }
}

void Renderer::drawCountdownOverlay(int seconds_left) {
  const int panel_width = std::max(260, screen_res_x / 4);
  const int panel_height = std::max(240, screen_res_y / 3);
  SDL_Rect panel{(screen_res_x - panel_width) / 2,
                 (screen_res_y - panel_height) / 2, panel_width, panel_height};

  drawPanel(panel, kPanelFillColor, kPanelBorderColor);
  renderSimpleText(sdl_font_hud, "Spielstart in", kHudTextColor,
                   screen_res_x / 2, panel.y + 30);
  renderBrickText(
      sdl_font_display, std::to_string(seconds_left), screen_res_x / 2,
      panel.y + (panel.h - TTF_FontHeight(sdl_font_display)) / 2,
      kBrickOutlineColor);
}

void Renderer::drawGameplayPauseOverlay(bool show_exit_dialog,
                                        int exit_dialog_selected) {
  drawDimmer(show_exit_dialog ? 168 : 148);

  if (!show_exit_dialog) {
    const int panel_width = std::min(700, screen_res_x * 38 / 100);
    const int panel_height = std::max(240, screen_res_y / 4);
    SDL_Rect panel{(screen_res_x - panel_width) / 2,
                   (screen_res_y - panel_height) / 2, panel_width,
                   panel_height};
    drawPanel(panel, kPanelFillColor, kPanelBorderColor);
    renderSimpleText(sdl_font_menu, "Pause", kHudTextColor, screen_res_x / 2,
                     panel.y + 28);
    renderSimpleText(sdl_font_hud, "Space setzt das Spiel fort",
                     kHudTextColor, screen_res_x / 2, panel.y + 102);
    renderSimpleText(sdl_font_hud, "Esc oeffnet den Beenden-Dialog",
                     kHudTextColor, screen_res_x / 2, panel.y + 102 +
                     TTF_FontHeight(sdl_font_hud) + 12);
    return;
  }

  const int dialog_width = std::min(700, screen_res_x * 40 / 100);
  const int dialog_height = std::max(250, screen_res_y / 4);
  SDL_Rect dialog{(screen_res_x - dialog_width) / 2,
                  (screen_res_y - dialog_height) / 2, dialog_width,
                  dialog_height};
  drawPanel(dialog, kPanelFillColor, kPanelBorderColor);
  renderSimpleText(sdl_font_menu, "Beenden?", kHudTextColor, screen_res_x / 2,
                   dialog.y + 24);
  renderSimpleText(sdl_font_hud, "Aktuelle Runde abbrechen und ins Menue zurueck?",
                   kHudTextColor, screen_res_x / 2, dialog.y + 86);

  const std::vector<std::string> items{"Ja", "Nein"};
  const int item_width = std::max(140, dialog.w / 4);
  const int item_height = std::max(54, TTF_FontHeight(sdl_font_menu) + 16);
  const int item_gap = std::max(28, dialog.w / 12);
  const int total_width = item_width * static_cast<int>(items.size()) + item_gap;
  const int start_x = dialog.x + (dialog.w - total_width) / 2;
  const int start_y = dialog.y + 132;
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    SDL_Rect highlight_rect{start_x + i * (item_width + item_gap), start_y,
                            item_width, item_height};
    if (i == exit_dialog_selected) {
      SDL_SetRenderDrawColor(sdl_renderer, 138, 46, 29, 185);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 235, 182, 140, 255);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    } else {
      SDL_SetRenderDrawColor(sdl_renderer, 22, 16, 32, 220);
      SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      SDL_SetRenderDrawColor(sdl_renderer, 196, 130, 92, 180);
      SDL_RenderDrawRect(sdl_renderer, &highlight_rect);
    }

    const SDL_Color item_color =
        (i == exit_dialog_selected) ? kSelectedMenuTextColor : kMenuTextColor;
    const int text_top =
        highlight_rect.y +
        (highlight_rect.h - TTF_FontHeight(sdl_font_menu)) / 2 - 2;
    renderSimpleText(sdl_font_menu, items[i], item_color,
                     highlight_rect.x + highlight_rect.w / 2, text_top);
  }

  renderSimpleText(sdl_font_hud, "Pfeile waehlen | Enter bestaetigt | Esc zurueck",
                   kStatusTextColor, screen_res_x / 2, dialog.y + dialog.h - 48);
}

int Renderer::getMonsterAnimationFrame(char monster_char,
                                       int animation_seed,
                                       bool grazing,
                                       bool jumping) const {
  if (sdl_monster_textures.empty()) {
    return 0;
  }

  const Uint32 frame_duration_ms =
      (monster_char == GOAT && jumping)
          ? GOAT_JUMP_ANIMATION_FRAME_MS
          : ((monster_char == GOAT && grazing)
                 ? GOAT_GRAZING_ANIMATION_FRAME_MS
                 : ((monster_char == GOAT) ? GOAT_ANIMATION_FRAME_MS
                                           : MONSTER_ANIMATION_FRAME_MS));
  const Uint32 total_offset =
      static_cast<Uint32>((animation_seed * 137) %
                          (frame_duration_ms * kMonsterFramesPerDirection));
  const Uint32 frame_clock = SDL_GetTicks() + total_offset;
  return static_cast<int>((frame_clock / frame_duration_ms) %
                          kMonsterFramesPerDirection);
}

SDL_Texture *Renderer::getMonsterTexture(char monster_char,
                                         Directions facing_direction,
                                         int animation_seed,
                                         bool grazing,
                                         bool jumping) {
  if (sdl_monster_textures.empty()) {
    return nullptr;
  }

  const int frame_index =
      getMonsterAnimationFrame(monster_char, animation_seed, grazing, jumping);
  if (monster_char == GOAT && jumping) {
    const size_t jumping_texture_index =
        GoatDirectionalTextureIndex(facing_direction, frame_index);
    if (jumping_texture_index >= sdl_goat_jumping_textures.size()) {
      return nullptr;
    }
    return sdl_goat_jumping_textures[jumping_texture_index];
  }
  if (monster_char == GOAT && grazing) {
    const size_t grazing_texture_index =
        GoatDirectionalTextureIndex(facing_direction, frame_index);
    if (grazing_texture_index >= sdl_goat_grazing_textures.size()) {
      return nullptr;
    }
    return sdl_goat_grazing_textures[grazing_texture_index];
  }

  const size_t texture_index =
      MonsterTextureIndex(monster_char, facing_direction, frame_index);
  if (texture_index >= sdl_monster_textures.size()) {
    return nullptr;
  }
  return sdl_monster_textures[texture_index];
}

SDL_Texture *Renderer::getStunnedGoatTexture() {
  // Stunned goat: front-facing, first frame, no animation.
  const size_t idx = MonsterTextureIndex(GOAT, Directions::Down, 0);
  if (idx >= sdl_monster_textures.size()) {
    return nullptr;
  }
  return sdl_monster_textures[idx];
}

int Renderer::getAlienAnimationFrame(const AlienAgent &alien, Uint32 now) const {
  if (sdl_alien_textures.empty()) {
    return 0;
  }

  if (alien.animation_started_ticks == 0 ||
      now < alien.animation_started_ticks ||
      now >= alien.animation_until_ticks) {
    return 0;
  }

  const Uint32 frame_duration_ms = ALIEN_IDLE_ANIMATION_FRAME_MS;
  const Uint32 elapsed = now - alien.animation_started_ticks;
  const Uint32 total_offset =
      static_cast<Uint32>((alien.animation_seed * 137) % frame_duration_ms);
  const Uint32 frame_clock = elapsed + total_offset;
  return static_cast<int>((frame_clock / frame_duration_ms) %
                          kAlienAnimationFrames);
}

SDL_Texture *Renderer::getAlienTexture(const AlienAgent &alien, Uint32 now) {
  if (sdl_alien_textures.empty()) {
    return nullptr;
  }

  const int frame_index = getAlienAnimationFrame(alien, now);
  const size_t texture_index = AlienTextureIndexForFrame(frame_index);
  if (texture_index >= sdl_alien_textures.size()) {
    return nullptr;
  }
  return sdl_alien_textures[texture_index];
}

void Renderer::renderSimpleText(TTF_Font *font, const std::string &text,
                                const SDL_Color &color, int center_x,
                                int top_y) {
  if (text.empty()) {
    return;
  }
  const TextCache::Entry *entry =
      text_cache_.GetOrCreate(sdl_renderer, font, text, color);
  if (entry == nullptr) {
    return;
  }
  SDL_Rect destination{center_x - entry->w / 2, top_y, entry->w, entry->h};
  SDL_RenderCopy(sdl_renderer, entry->texture, nullptr, &destination);
}

void Renderer::renderTextLeft(TTF_Font *font, const std::string &text,
                              const SDL_Color &color, int left_x, int top_y) {
  if (text.empty()) {
    return;
  }
  const TextCache::Entry *entry =
      text_cache_.GetOrCreate(sdl_renderer, font, text, color);
  if (entry == nullptr) {
    return;
  }
  SDL_Rect destination{left_x, top_y, entry->w, entry->h};
  SDL_RenderCopy(sdl_renderer, entry->texture, nullptr, &destination);
}

void Renderer::renderStartLogo(TTF_Font *font, const std::string &text,
                               int center_x, int top_y) {
  if (text.empty()) {
    return;
  }

  SDL_Surface *logo_surface = createStartLogoSurface(font, text);
  if (logo_surface == nullptr) {
    return;
  }

  SDL_Texture *logo_texture =
      SDL_CreateTextureFromSurface(sdl_renderer, logo_surface);
  if (logo_texture == nullptr) {
    SDL_FreeSurface(logo_surface);
    return;
  }
  configureSmoothTextureSampling(logo_texture);

  const int logo_width = logo_surface->w;
  const int logo_height = logo_surface->h;
  const Uint32 now = SDL_GetTicks();
  const double clock = static_cast<double>(now);
  const double base_sway_x =
      std::sin(clock / 620.0) * std::max(4.0, logo_width * 0.010);
  const double base_sway_y =
      std::sin(clock / 890.0 + 0.7) * std::max(2.0, logo_height * 0.040);

  // Continuous fluid-glass warp: every point inside the logo gets a small,
  // smoothly-varying offset built from a sum of sinusoidal fields. This
  // replaces the old per-slice rect drawing (which produced visible pixel
  // steps at every band) with a single mesh-deformed render.
  // Liquid-glass warp. Amplitudes and base periods are scaled to logo size so
  // the look stays consistent across resolutions; `START_MENU_LOGO_WARP_SPEED`
  // multiplies the elapsed clock so values <1 thicken the flow further and
  // values >1 turn it back into a quicker ripple.
  const double warp_amp_x = std::max(
      2.5, static_cast<double>(logo_width) *
               static_cast<double>(START_MENU_LOGO_WARP_AMP_X_FACTOR));
  const double warp_amp_y = std::max(
      1.6, static_cast<double>(logo_height) *
               static_cast<double>(START_MENU_LOGO_WARP_AMP_Y_FACTOR));
  const double warp_speed = std::max(0.0, START_MENU_LOGO_WARP_SPEED);
  const double clock_a = clock * warp_speed / 1180.0;
  const double clock_b = clock * warp_speed / 760.0;
  const double clock_c = clock * warp_speed / 540.0;
  const double clock_d = clock * warp_speed / 880.0;
  const double clock_e = clock * warp_speed / 620.0;
  const double clock_f = clock * warp_speed / 460.0;
  auto fluid_warp = [&](double fx, double fy) -> SDL_FPoint {
    const double ox =
        std::sin(fy * 5.6 + clock_a) * warp_amp_x +
        std::sin(fx * 3.7 + clock_b + 1.4) * warp_amp_x * 0.55 +
        std::sin((fx + fy) * 8.3 - clock_c) * warp_amp_x * 0.32;
    const double oy =
        std::sin(fx * 4.2 + clock_d + 0.8) * warp_amp_y +
        std::sin(fy * 7.1 - clock_e) * warp_amp_y * 0.45 +
        std::sin((fx - fy) * 9.7 + clock_f) * warp_amp_y * 0.32;
    return SDL_FPoint{static_cast<float>(ox + base_sway_x),
                      static_cast<float>(oy + base_sway_y)};
  };

  // Build a uniform mesh covering the source texture. Higher density gives a
  // smoother warp; we tie it to the on-screen size so very small logos don't
  // pay for an oversized grid.
  const int grid_cols =
      std::clamp(logo_width / 14, 14, 64);
  const int grid_rows =
      std::clamp(logo_height / 14, 5, 24);
  const int vertex_count = (grid_cols + 1) * (grid_rows + 1);
  std::vector<SDL_Vertex> mesh_vertices;
  mesh_vertices.reserve(static_cast<size_t>(vertex_count));
  std::vector<int> mesh_indices;
  mesh_indices.reserve(static_cast<size_t>(grid_cols * grid_rows * 6));
  const float logo_left = static_cast<float>(center_x - logo_width / 2);
  const float logo_top = static_cast<float>(top_y);
  for (int gy = 0; gy <= grid_rows; ++gy) {
    for (int gx = 0; gx <= grid_cols; ++gx) {
      const double fx = static_cast<double>(gx) / grid_cols;
      const double fy = static_cast<double>(gy) / grid_rows;
      const SDL_FPoint warp = fluid_warp(fx, fy);
      SDL_Vertex v{};
      v.position.x = logo_left +
                     static_cast<float>(fx) * static_cast<float>(logo_width) +
                     warp.x;
      v.position.y = logo_top +
                     static_cast<float>(fy) * static_cast<float>(logo_height) +
                     warp.y;
      v.tex_coord.x = static_cast<float>(fx);
      v.tex_coord.y = static_cast<float>(fy);
      v.color = SDL_Color{255, 255, 255, 255};
      mesh_vertices.push_back(v);
    }
  }
  for (int gy = 0; gy < grid_rows; ++gy) {
    for (int gx = 0; gx < grid_cols; ++gx) {
      const int tl = gy * (grid_cols + 1) + gx;
      const int tr = tl + 1;
      const int bl = (gy + 1) * (grid_cols + 1) + gx;
      const int br = bl + 1;
      mesh_indices.push_back(tl);
      mesh_indices.push_back(tr);
      mesh_indices.push_back(bl);
      mesh_indices.push_back(tr);
      mesh_indices.push_back(br);
      mesh_indices.push_back(bl);
    }
  }

  // Drop shadow: same warped mesh, offset and tinted dark blue. Drawing the
  // shadow with the warp keeps it perfectly attached to the rippling glass.
  // Offset / color / alpha are tunable via `definitions.h`.
  const int shadow_offset_px =
      std::max(5, static_cast<int>(std::lround(
                       static_cast<float>(TTF_FontHeight(font)) *
                       START_MENU_LOGO_SHADOW_OFFSET_FACTOR)));
  std::vector<SDL_Vertex> shadow_vertices = mesh_vertices;
  for (auto &vertex : shadow_vertices) {
    vertex.position.x += static_cast<float>(shadow_offset_px);
    vertex.position.y += static_cast<float>(shadow_offset_px);
  }
  SDL_SetTextureBlendMode(logo_texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureColorMod(logo_texture, START_MENU_LOGO_SHADOW_COLOR.r,
                         START_MENU_LOGO_SHADOW_COLOR.g,
                         START_MENU_LOGO_SHADOW_COLOR.b);
  SDL_SetTextureAlphaMod(logo_texture, START_MENU_LOGO_SHADOW_ALPHA);
  SDL_RenderGeometry(sdl_renderer, logo_texture, shadow_vertices.data(),
                     static_cast<int>(shadow_vertices.size()),
                     mesh_indices.data(),
                     static_cast<int>(mesh_indices.size()));

  // 3D extrusion: instead of a single 2D mesh draw we paint the logo as a
  // back-to-front stack of warped slices. Each slice is the same surface
  // shifted along the camera-down-right depth axis and tinted darker, so the
  // visible side walls emerge from successive layers and the front face sits
  // on top with its full glass shading. This turns the logo into a real
  // extruded 3D solid rather than a 2D image with painted-on highlights. All
  // tuning of slice count, axis, depth and parallax happens in
  // `definitions.h`.
  SDL_SetTextureBlendMode(logo_texture, SDL_BLENDMODE_BLEND);
  // The depth axis carries a very gentle clock-driven wobble so the 3D solid
  // breathes rather than sitting rigid. The base direction is set by the
  // `START_MENU_LOGO_DEPTH_DIR_*` constants.
  const double depth_breathe = std::sin(clock / 1200.0) * 0.18;
  const float depth_dir_x =
      START_MENU_LOGO_DEPTH_DIR_X + static_cast<float>(depth_breathe) * 0.10f;
  const float depth_dir_y =
      START_MENU_LOGO_DEPTH_DIR_Y - static_cast<float>(depth_breathe) * 0.10f;
  const float total_depth_px = std::max(
      8.0f, static_cast<float>(TTF_FontHeight(font)) *
                START_MENU_LOGO_DEPTH_FACTOR);
  const int kDepthSlices = std::max(2, START_MENU_LOGO_DEPTH_SLICES);
  const float depth_step_x = depth_dir_x * total_depth_px /
                             static_cast<float>(kDepthSlices - 1);
  const float depth_step_y = depth_dir_y * total_depth_px /
                             static_cast<float>(kDepthSlices - 1);
  // Side-wall tint comes from `START_MENU_LOGO_BACK_TINT`.
  std::vector<SDL_Vertex> slice_vertices(mesh_vertices.size());
  for (int slice = kDepthSlices - 1; slice >= 0; --slice) {
    // z_norm: 1 at the back-most slice, 0 at the front face.
    const float z_norm =
        static_cast<float>(slice) / static_cast<float>(kDepthSlices - 1);
    const float ox = depth_step_x * static_cast<float>(slice);
    const float oy = depth_step_y * static_cast<float>(slice);
    // Per-slice parallax on the warp so the depth column doesn't move as a
    // rigid block during the fluid wobble.
    const float warp_falloff =
        1.0f - START_MENU_LOGO_DEPTH_PARALLAX * z_norm;
    for (size_t i = 0; i < mesh_vertices.size(); ++i) {
      const SDL_Vertex &src = mesh_vertices[i];
      SDL_Vertex &dst = slice_vertices[i];
      const double fx = static_cast<double>(src.tex_coord.x);
      const double fy = static_cast<double>(src.tex_coord.y);
      const float untransformed_x = logo_left +
                                    static_cast<float>(fx) *
                                        static_cast<float>(logo_width);
      const float untransformed_y = logo_top +
                                    static_cast<float>(fy) *
                                        static_cast<float>(logo_height);
      const float warp_x = src.position.x - untransformed_x;
      const float warp_y = src.position.y - untransformed_y;
      dst.position.x = untransformed_x + warp_x * warp_falloff + ox;
      dst.position.y = untransformed_y + warp_y * warp_falloff + oy;
      dst.tex_coord = src.tex_coord;
      dst.color = src.color;
    }
    // Per-slice color and alpha grading: the front slice keeps the full
    // glass surface, the slices behind are progressively pulled toward
    // the deep navy side-wall tint and faded out so the back doesn't
    // overpower the front. The midline gets a soft cyan kick to suggest
    // light bouncing through the glass volume.
    const float depth_t = z_norm;
    const float side_mix = 0.18f + 0.62f * depth_t;
    const float alpha_norm =
        std::clamp(0.30f + 0.70f * (1.0f - depth_t * depth_t), 0.0f, 1.0f);
    const Uint8 cmod_r = static_cast<Uint8>(std::lround(
        255.0f * (1.0f - side_mix) +
        static_cast<float>(START_MENU_LOGO_BACK_TINT.r) * side_mix));
    const Uint8 cmod_g = static_cast<Uint8>(std::lround(
        255.0f * (1.0f - side_mix) +
        static_cast<float>(START_MENU_LOGO_BACK_TINT.g) * side_mix));
    const Uint8 cmod_b = static_cast<Uint8>(std::lround(
        255.0f * (1.0f - side_mix) +
        static_cast<float>(START_MENU_LOGO_BACK_TINT.b) * side_mix));
    const Uint8 alpha_mod =
        static_cast<Uint8>(std::clamp(
            static_cast<int>(std::lround(alpha_norm * 255.0f)), 0, 255));
    SDL_SetTextureColorMod(logo_texture, cmod_r, cmod_g, cmod_b);
    SDL_SetTextureAlphaMod(logo_texture, alpha_mod);
    SDL_RenderGeometry(sdl_renderer, logo_texture, slice_vertices.data(),
                       static_cast<int>(slice_vertices.size()),
                       mesh_indices.data(),
                       static_cast<int>(mesh_indices.size()));
  }
  SDL_SetTextureColorMod(logo_texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(logo_texture, 255);

  const bool lock_logo_surface = SDL_MUSTLOCK(logo_surface);
  if (lock_logo_surface) {
    SDL_LockSurface(logo_surface);
  }

  auto logo_alpha = [&](int px, int py) -> Uint8 {
    if (px < 0 || py < 0 || px >= logo_width || py >= logo_height) {
      return 0;
    }

    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_GetRGBA(readPixel(logo_surface, px, py), logo_surface->format, &red,
                &green, &blue, &alpha);
    return alpha;
  };

  auto find_logo_anchor = [&](float norm_x, float norm_y) -> SDL_Point {
    const int candidate_x = std::clamp(
        static_cast<int>(std::lround(norm_x * static_cast<float>(logo_width - 1))),
        0, std::max(0, logo_width - 1));
    const int candidate_y = std::clamp(
        static_cast<int>(std::lround(norm_y * static_cast<float>(logo_height - 1))),
        0, std::max(0, logo_height - 1));
    if (logo_alpha(candidate_x, candidate_y) > 24) {
      return SDL_Point{candidate_x, candidate_y};
    }

    constexpr int kSearchRadius = 26;
    for (int radius = 1; radius <= kSearchRadius; ++radius) {
      for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          const int probe_x = candidate_x + dx;
          const int probe_y = candidate_y + dy;
          if (logo_alpha(probe_x, probe_y) > 24) {
            return SDL_Point{probe_x, probe_y};
          }
        }
      }
    }

    return SDL_Point{-1, -1};
  };

  struct SparkleLane {
    double phase_seconds;
    double cycle_seconds;
    double visible_fraction;
    float min_scale;
    float max_scale;
  };
  constexpr std::array<SparkleLane, 8> kSparkleLanes{{
      {0.20, 2.6, 0.26, 0.84f, 1.18f},
      {0.58, 3.1, 0.22, 0.78f, 1.06f},
      {1.02, 2.8, 0.28, 0.88f, 1.24f},
      {1.46, 3.4, 0.24, 0.74f, 1.08f},
      {1.88, 2.9, 0.25, 0.82f, 1.14f},
      {2.32, 3.7, 0.21, 0.76f, 1.02f},
      {2.74, 3.0, 0.27, 0.86f, 1.20f},
      {3.18, 4.1, 0.23, 0.80f, 1.12f},
  }};

  for (size_t sparkle_index = 0; sparkle_index < kSparkleLanes.size();
       ++sparkle_index) {
    const SparkleLane &sparkle = kSparkleLanes[sparkle_index];
    const double cycle_clock = clock / 1000.0 + sparkle.phase_seconds;
    const double cycle_index = std::floor(cycle_clock / sparkle.cycle_seconds);
    const double cycle_position =
        std::fmod(cycle_clock, sparkle.cycle_seconds) / sparkle.cycle_seconds;
    if (cycle_position < 0.0 || cycle_position > sparkle.visible_fraction) {
      continue;
    }

    const double local_progress = cycle_position / sparkle.visible_fraction;
    const double pulse = std::sin(local_progress * kLogoPi);
    if (pulse <= 0.01) {
      continue;
    }

    const auto random_fraction = [&](double salt) {
      const double raw_random =
          std::sin((cycle_index + 1.0) * (117.1 + salt * 0.9) +
                   sparkle.phase_seconds * (39.7 + salt * 0.3) +
                   static_cast<double>(sparkle_index + 1) * (61.3 + salt * 0.7)) *
          43758.5453123;
      double fraction = raw_random - std::floor(raw_random);
      if (fraction < 0.0) {
        fraction += 1.0;
      }
      return fraction;
    };

    const float norm_x =
        static_cast<float>(0.08 + random_fraction(11.0) * 0.84);
    const float norm_y =
        static_cast<float>(0.10 + random_fraction(23.0) * 0.72);
    const double intensity = std::pow(pulse, 0.62);

    const SDL_Point anchor = find_logo_anchor(norm_x, norm_y);
    if (anchor.x < 0 || anchor.y < 0) {
      continue;
    }

    // Project the anchor through the same fluid warp the mesh used so the
    // sparkles ride on top of the rippling glass instead of drifting.
    const double anchor_fx =
        (logo_width > 0)
            ? static_cast<double>(anchor.x) / static_cast<double>(logo_width)
            : 0.0;
    const double anchor_fy =
        (logo_height > 0)
            ? static_cast<double>(anchor.y) / static_cast<double>(logo_height)
            : 0.0;
    const SDL_FPoint anchor_warp = fluid_warp(anchor_fx, anchor_fy);
    const int sparkle_center_x =
        static_cast<int>(std::lround(static_cast<double>(logo_left) +
                                     anchor.x + anchor_warp.x));
    const int sparkle_center_y =
        static_cast<int>(std::lround(static_cast<double>(logo_top) +
                                     anchor.y + anchor_warp.y));
    const double randomized_scale =
        sparkle.min_scale +
        random_fraction(31.0) * (sparkle.max_scale - sparkle.min_scale);
    const float arm_factor =
        std::max(0.0f, START_MENU_LOGO_SPARKLE_ARM_LENGTH_FACTOR);
    const float thickness_factor =
        std::max(0.0f, START_MENU_LOGO_SPARKLE_THICKNESS_FACTOR);
    const float alpha_factor =
        std::max(0.0f, START_MENU_LOGO_SPARKLE_ALPHA_FACTOR);
    const int halo_arm = std::max(
        2, static_cast<int>(std::lround(logo_height * 0.126 * randomized_scale *
                                        (0.40 + intensity * 0.92) *
                                        arm_factor)));
    const int major_arm = std::max(
        2, static_cast<int>(std::lround(logo_height * 0.104 * randomized_scale *
                                        (0.50 + intensity * 0.86) *
                                        arm_factor)));
    const int inner_arm = std::max(
        1, static_cast<int>(std::lround(major_arm *
                                        (0.46 + random_fraction(41.0) * 0.16))));
    const int raw_thickness = static_cast<int>(std::lround(
        (1.0 + intensity * (1.8 + random_fraction(53.0) * 1.4)) *
        thickness_factor));
    const int outer_thickness = std::max(0, raw_thickness);
    const int inner_thickness = std::max(0, outer_thickness - 1);
    auto scale_alpha = [&](double base) -> Uint8 {
      return static_cast<Uint8>(std::clamp(
          static_cast<int>(std::lround(base * alpha_factor)), 0, 255));
    };
    const Uint8 halo_alpha = scale_alpha(150.0 * intensity + 24.0);
    const Uint8 outer_alpha = scale_alpha(228.0 * intensity + 16.0);
    const Uint8 inner_alpha =
        scale_alpha(255.0 * std::pow(intensity, 0.86));

    auto draw_four_point_star = [&](const SDL_Color &color, Uint8 alpha,
                                    int arm_length, int thickness) {
      SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, alpha);
      for (int offset = -thickness; offset <= thickness; ++offset) {
        const int taper = std::abs(offset) * 2;
        const int horizontal_length = std::max(1, arm_length - taper);
        const int vertical_length = std::max(1, arm_length - taper);
        SDL_RenderDrawLine(sdl_renderer, sparkle_center_x - horizontal_length,
                           sparkle_center_y + offset,
                           sparkle_center_x + horizontal_length,
                           sparkle_center_y + offset);
        SDL_RenderDrawLine(sdl_renderer, sparkle_center_x + offset,
                           sparkle_center_y - vertical_length,
                           sparkle_center_x + offset,
                           sparkle_center_y + vertical_length);
      }
    };

    // Pull the three sparkle layers toward the configured logo tint color.
    // Strength 0 keeps the original cyan/white glints; strength 1 dyes them
    // fully in the logo's color so the sparkles read as part of the glass.
    const float sparkle_tint_t =
        std::clamp(START_MENU_LOGO_SPARKLE_TINT_STRENGTH, 0.0f, 1.0f);
    const SDL_Color sparkle_halo_color = LerpColor(
        SDL_Color{88, 214, 255, 255}, START_MENU_LOGO_SPARKLE_TINT_COLOR,
        sparkle_tint_t);
    const SDL_Color sparkle_outer_color = LerpColor(
        SDL_Color{184, 240, 255, 255}, START_MENU_LOGO_SPARKLE_TINT_COLOR,
        sparkle_tint_t);
    const SDL_Color sparkle_inner_color = LerpColor(
        SDL_Color{255, 255, 255, 255}, START_MENU_LOGO_SPARKLE_TINT_COLOR,
        sparkle_tint_t);

    draw_four_point_star(sparkle_halo_color, halo_alpha, halo_arm,
                         outer_thickness + 1);
    draw_four_point_star(sparkle_outer_color, outer_alpha, major_arm,
                         outer_thickness);
    draw_four_point_star(sparkle_inner_color, inner_alpha, inner_arm,
                         inner_thickness);
  }

  if (lock_logo_surface) {
    SDL_UnlockSurface(logo_surface);
  }

  SDL_DestroyTexture(logo_texture);
  SDL_FreeSurface(logo_surface);
}

SDL_Texture *Renderer::loadTrimmedChromaKeyTexture(const std::string &path,
                                                   SDL_Point &trimmed_size,
                                                   Uint8 tolerance,
                                                   bool trim_to_content) {
  trimmed_size = SDL_Point{0, 0};
  SDL_Surface *raw_surface = IMG_Load(path.c_str());
  if (raw_surface == nullptr) {
    std::cerr << "Could not open chroma-key art " << path << ": "
              << IMG_GetError() << "\n";
    return nullptr;
  }

  SDL_Surface *rgba_surface =
      SDL_ConvertSurfaceFormat(raw_surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(raw_surface);
  if (rgba_surface == nullptr) {
    std::cerr << "Could not convert chroma-key art " << path << ": "
              << SDL_GetError() << "\n";
    return nullptr;
  }

  const bool lock_surface = SDL_MUSTLOCK(rgba_surface);
  if (lock_surface) {
    SDL_LockSurface(rgba_surface);
  }

  const SDL_Point corner_points[4] = {
      {0, 0},
      {std::max(0, rgba_surface->w - 1), 0},
      {0, std::max(0, rgba_surface->h - 1)},
      {std::max(0, rgba_surface->w - 1), std::max(0, rgba_surface->h - 1)}};
  SDL_Color corner_colors[4];
  for (int index = 0; index < 4; ++index) {
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_GetRGBA(readPixel(rgba_surface, corner_points[index].x,
                          corner_points[index].y),
                rgba_surface->format, &red, &green, &blue, &alpha);
    corner_colors[index] = SDL_Color{red, green, blue, alpha};
  }

  const int tolerance_squared = std::max(1, static_cast<int>(tolerance)) *
                                std::max(1, static_cast<int>(tolerance)) * 3;
  for (int y = 0; y < rgba_surface->h; ++y) {
    for (int x = 0; x < rgba_surface->w; ++x) {
      Uint8 red = 0;
      Uint8 green = 0;
      Uint8 blue = 0;
      Uint8 alpha = 0;
      const Uint32 pixel = readPixel(rgba_surface, x, y);
      SDL_GetRGBA(pixel, rgba_surface->format, &red, &green, &blue, &alpha);
      if (alpha == 0) {
        continue;
      }

      int min_distance_squared = tolerance_squared + 1;
      for (const SDL_Color &corner_color : corner_colors) {
        const int delta_r = static_cast<int>(red) - corner_color.r;
        const int delta_g = static_cast<int>(green) - corner_color.g;
        const int delta_b = static_cast<int>(blue) - corner_color.b;
        const int distance_squared =
            delta_r * delta_r + delta_g * delta_g + delta_b * delta_b;
        min_distance_squared = std::min(min_distance_squared, distance_squared);
      }

      if (min_distance_squared <= tolerance_squared) {
        writePixel(rgba_surface, x, y,
                   SDL_MapRGBA(rgba_surface->format, red, green, blue, 0));
      }
    }
  }

  if (lock_surface) {
    SDL_UnlockSurface(rgba_surface);
  }

  int min_x = rgba_surface->w;
  int min_y = rgba_surface->h;
  int max_x = -1;
  int max_y = -1;
  const bool relock_surface = SDL_MUSTLOCK(rgba_surface);
  if (relock_surface) {
    SDL_LockSurface(rgba_surface);
  }
  for (int y = 0; y < rgba_surface->h; ++y) {
    for (int x = 0; x < rgba_surface->w; ++x) {
      Uint8 red = 0;
      Uint8 green = 0;
      Uint8 blue = 0;
      Uint8 alpha = 0;
      SDL_GetRGBA(readPixel(rgba_surface, x, y), rgba_surface->format, &red,
                  &green, &blue, &alpha);
      if (alpha <= 8) {
        continue;
      }
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      max_x = std::max(max_x, x);
      max_y = std::max(max_y, y);
    }
  }
  if (relock_surface) {
    SDL_UnlockSurface(rgba_surface);
  }

  SDL_Rect trim_rect{0, 0, rgba_surface->w, rgba_surface->h};
  if (trim_to_content && max_x >= min_x && max_y >= min_y) {
    trim_rect.x = min_x;
    trim_rect.y = min_y;
    trim_rect.w = max_x - min_x + 1;
    trim_rect.h = max_y - min_y + 1;
  }

  SDL_Surface *trimmed_surface = SDL_CreateRGBSurfaceWithFormat(
      0, trim_rect.w, trim_rect.h, 32, SDL_PIXELFORMAT_RGBA32);
  if (trimmed_surface == nullptr) {
    SDL_FreeSurface(rgba_surface);
    std::cerr << "Could not allocate chroma-key art buffer for " << path
              << ": " << SDL_GetError() << "\n";
    return nullptr;
  }

  SDL_FillRect(trimmed_surface, nullptr,
               SDL_MapRGBA(trimmed_surface->format, 0, 0, 0, 0));
  if (SDL_BlitSurface(rgba_surface, &trim_rect, trimmed_surface, nullptr) != 0) {
    std::cerr << "Could not trim chroma-key art " << path << ": "
              << SDL_GetError() << "\n";
    SDL_FreeSurface(trimmed_surface);
    SDL_FreeSurface(rgba_surface);
    return nullptr;
  }

  SDL_Texture *texture =
      SDL_CreateTextureFromSurface(sdl_renderer, trimmed_surface);
  if (texture == nullptr) {
    std::cerr << "Could not create chroma-key texture " << path << ": "
              << SDL_GetError() << "\n";
  } else {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    configureSmoothTextureSampling(texture);
    trimmed_size = SDL_Point{trim_rect.w, trim_rect.h};
  }

  SDL_FreeSurface(trimmed_surface);
  SDL_FreeSurface(rgba_surface);
  return texture;
}

SDL_Texture *Renderer::loadTrimmedTexture(const std::string &path,
                                          SDL_Point &trimmed_size) {
  trimmed_size = SDL_Point{0, 0};
  SDL_Surface *raw_surface = IMG_Load(path.c_str());
  if (raw_surface == nullptr) {
    std::cerr << "Could not open decorative start menu art " << path << ": "
              << IMG_GetError() << "\n";
    return nullptr;
  }

  SDL_Surface *rgba_surface =
      SDL_ConvertSurfaceFormat(raw_surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(raw_surface);
  if (rgba_surface == nullptr) {
    std::cerr << "Could not convert decorative art " << path << ": "
              << SDL_GetError() << "\n";
    return nullptr;
  }

  const bool lock_surface = SDL_MUSTLOCK(rgba_surface);
  if (lock_surface) {
    SDL_LockSurface(rgba_surface);
  }

  int min_x = rgba_surface->w;
  int min_y = rgba_surface->h;
  int max_x = -1;
  int max_y = -1;
  for (int y = 0; y < rgba_surface->h; ++y) {
    for (int x = 0; x < rgba_surface->w; ++x) {
      Uint8 red = 0;
      Uint8 green = 0;
      Uint8 blue = 0;
      Uint8 alpha = 0;
      SDL_GetRGBA(readPixel(rgba_surface, x, y), rgba_surface->format, &red,
                  &green, &blue, &alpha);
      if (alpha <= 8) {
        continue;
      }
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      max_x = std::max(max_x, x);
      max_y = std::max(max_y, y);
    }
  }

  if (lock_surface) {
    SDL_UnlockSurface(rgba_surface);
  }

  SDL_Rect trim_rect{0, 0, rgba_surface->w, rgba_surface->h};
  if (max_x >= min_x && max_y >= min_y) {
    trim_rect.x = min_x;
    trim_rect.y = min_y;
    trim_rect.w = max_x - min_x + 1;
    trim_rect.h = max_y - min_y + 1;
  }

  SDL_Surface *trimmed_surface = SDL_CreateRGBSurfaceWithFormat(
      0, trim_rect.w, trim_rect.h, 32, SDL_PIXELFORMAT_RGBA32);
  if (trimmed_surface == nullptr) {
    SDL_FreeSurface(rgba_surface);
    std::cerr << "Could not allocate decorative art buffer for " << path
              << ": " << SDL_GetError() << "\n";
    return nullptr;
  }

  SDL_FillRect(trimmed_surface, nullptr,
               SDL_MapRGBA(trimmed_surface->format, 0, 0, 0, 0));
  if (SDL_BlitSurface(rgba_surface, &trim_rect, trimmed_surface, nullptr) != 0) {
    std::cerr << "Could not trim decorative art " << path << ": "
              << SDL_GetError() << "\n";
    SDL_FreeSurface(trimmed_surface);
    SDL_FreeSurface(rgba_surface);
    return nullptr;
  }

  SDL_Texture *texture =
      SDL_CreateTextureFromSurface(sdl_renderer, trimmed_surface);
  if (texture == nullptr) {
    std::cerr << "Could not create decorative texture " << path << ": "
              << SDL_GetError() << "\n";
  } else {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    configureSmoothTextureSampling(texture);
    trimmed_size = SDL_Point{trim_rect.w, trim_rect.h};
  }

  SDL_FreeSurface(trimmed_surface);
  SDL_FreeSurface(rgba_surface);
  return texture;
}

SDL_Texture *Renderer::loadTexturePreserveCanvas(const std::string &path,
                                                 SDL_Point &texture_size) {
  texture_size = SDL_Point{0, 0};
  SDL_Surface *raw_surface = IMG_Load(path.c_str());
  if (raw_surface == nullptr) {
    std::cerr << "Could not open texture sheet " << path << ": "
              << IMG_GetError() << "\n";
    return nullptr;
  }

  SDL_Surface *rgba_surface =
      SDL_ConvertSurfaceFormat(raw_surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(raw_surface);
  if (rgba_surface == nullptr) {
    std::cerr << "Could not convert texture sheet " << path << ": "
              << SDL_GetError() << "\n";
    return nullptr;
  }

  SDL_Texture *texture =
      SDL_CreateTextureFromSurface(sdl_renderer, rgba_surface);
  if (texture == nullptr) {
    std::cerr << "Could not create texture sheet " << path << ": "
              << SDL_GetError() << "\n";
  } else {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    configureSmoothTextureSampling(texture);
    texture_size = SDL_Point{rgba_surface->w, rgba_surface->h};
  }

  SDL_FreeSurface(rgba_surface);
  return texture;
}

void Renderer::renderDecorativeTexture(SDL_Texture *texture,
                                       const SDL_Rect &destination,
                                       const SDL_Color &glow_color,
                                       double sway_phase,
                                       double sway_strength_x,
                                       double sway_strength_y,
                                       double sway_speed) {
  if (texture == nullptr || destination.w <= 0 || destination.h <= 0) {
    return;
  }

  SDL_FRect animated_rect{static_cast<float>(destination.x),
                          static_cast<float>(destination.y),
                          static_cast<float>(destination.w),
                          static_cast<float>(destination.h)};
  if (sway_strength_x > 0.0 || sway_strength_y > 0.0) {
    const double clock = static_cast<double>(SDL_GetTicks());
    const double speed = std::max(0.05, sway_speed);
    const double horizontal_phase = clock * speed / 2100.0 + sway_phase;
    const double vertical_phase = clock * speed / 2950.0 + sway_phase * 1.15;
    const double drift_phase = clock * speed / 5200.0 + sway_phase * 0.75;
    const double horizontal_amplitude =
        std::max(6.0, destination.w * std::max(0.0, sway_strength_x));
    const double vertical_amplitude =
        std::max(5.0, destination.h * std::max(0.0, sway_strength_y));
    const double sway_x =
        std::sin(horizontal_phase) * horizontal_amplitude +
        std::sin(drift_phase) * horizontal_amplitude * 0.18;
    const double sway_y =
        std::sin(vertical_phase) * vertical_amplitude +
        std::cos(drift_phase * 0.82) * vertical_amplitude * 0.12;
    animated_rect.x += static_cast<float>(sway_x);
    animated_rect.y += static_cast<float>(sway_y);
  }

  SDL_FRect shadow_rect{
      animated_rect.x + static_cast<float>(std::max(5, destination.w / 34)),
      animated_rect.y + static_cast<float>(std::max(8, destination.h / 28)),
      animated_rect.w, animated_rect.h};
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureColorMod(texture, 8, 8, 18);
  SDL_SetTextureAlphaMod(texture, 116);
  SDL_RenderCopyF(sdl_renderer, texture, nullptr, &shadow_rect);

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
  for (int pass = 0; pass < 2; ++pass) {
    const int padding = std::max(6, destination.w / (18 - pass * 4));
    SDL_FRect glow_rect{animated_rect.x - static_cast<float>(padding),
                        animated_rect.y - static_cast<float>(padding) / 2.0f,
                        animated_rect.w + static_cast<float>(padding * 2),
                        animated_rect.h + static_cast<float>(padding)};
    SDL_SetTextureColorMod(texture, glow_color.r, glow_color.g, glow_color.b);
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(44 - pass * 18));
    SDL_RenderCopyF(sdl_renderer, texture, nullptr, &glow_rect);
  }

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureColorMod(texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(texture, 255);
  SDL_RenderCopyF(sdl_renderer, texture, nullptr, &animated_rect);
}

void Renderer::renderBrickText(TTF_Font *font, const std::string &text,
                               int center_x, int top_y,
                               const SDL_Color &outline_color) {
  if (text.empty()) {
    return;
  }

  const int shadow_offset = std::max(4, TTF_FontHeight(font) / 28);
  SDL_Surface *shadow_surface =
      TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{0, 0, 0, 220});
  if (shadow_surface != nullptr) {
    SDL_Texture *shadow_texture =
        SDL_CreateTextureFromSurface(sdl_renderer, shadow_surface);
    if (shadow_texture != nullptr) {
      SDL_SetTextureAlphaMod(shadow_texture, 170);
      SDL_Rect shadow_rect{center_x - shadow_surface->w / 2 + shadow_offset,
                           top_y + shadow_offset, shadow_surface->w,
                           shadow_surface->h};
      SDL_RenderCopy(sdl_renderer, shadow_texture, nullptr, &shadow_rect);
      SDL_DestroyTexture(shadow_texture);
    }
    SDL_FreeSurface(shadow_surface);
  }

  SDL_Surface *brick_surface = createBrickTextSurface(font, text, outline_color);
  if (brick_surface == nullptr) {
    return;
  }

  SDL_Texture *brick_texture =
      SDL_CreateTextureFromSurface(sdl_renderer, brick_surface);
  if (brick_texture != nullptr) {
    SDL_Rect destination{center_x - brick_surface->w / 2, top_y, brick_surface->w,
                         brick_surface->h};
    SDL_RenderCopy(sdl_renderer, brick_texture, nullptr, &destination);
    SDL_DestroyTexture(brick_texture);
  }

  SDL_FreeSurface(brick_surface);
}

int Renderer::getPacmanAnimationFrame(bool walking) const {
  if (!walking) {
    return 0;
  }

  const Uint32 frame_clock = SDL_GetTicks() / kPacmanWalkFrameMs;
  return static_cast<int>(frame_clock % kPacmanFramesPerDirection);
}

SDL_Texture *Renderer::getPacmanTexture(Directions facing_direction,
                                        bool walking) const {
  if (sdl_pacman_textures.size() <
      static_cast<size_t>(kPacmanFramesPerDirection * 4)) {
    return nullptr;
  }

  int direction_index = 0;
  switch (facing_direction) {
  case Directions::Up:
    direction_index = 1;
    break;
  case Directions::Left:
    direction_index = 2;
    break;
  case Directions::Right:
    direction_index = 3;
    break;
  case Directions::Down:
  case Directions::None:
  default:
    direction_index = 0;
    break;
  }

  const int frame_index =
      direction_index * kPacmanFramesPerDirection +
      std::clamp(getPacmanAnimationFrame(walking), 0,
                 kPacmanFramesPerDirection - 1);
  return sdl_pacman_textures[static_cast<size_t>(frame_index)];
}

SDL_Surface *Renderer::createStartLogoSurface(TTF_Font *font,
                                              const std::string &text) {
  SDL_Surface *glyph_surface_raw =
      TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{255, 255, 255, 255});
  if (glyph_surface_raw == nullptr) {
    return nullptr;
  }

  SDL_Surface *glyph_surface =
      SDL_ConvertSurfaceFormat(glyph_surface_raw, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(glyph_surface_raw);
  if (glyph_surface == nullptr) {
    return nullptr;
  }

  SDL_Surface *output_surface = SDL_CreateRGBSurfaceWithFormat(
      0, glyph_surface->w, glyph_surface->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (output_surface == nullptr) {
    SDL_FreeSurface(glyph_surface);
    return nullptr;
  }

  SDL_FillRect(output_surface, nullptr,
               SDL_MapRGBA(output_surface->format, 0, 0, 0, 0));

  const bool lock_glyph = SDL_MUSTLOCK(glyph_surface);
  const bool lock_output = SDL_MUSTLOCK(output_surface);
  if (lock_glyph) {
    SDL_LockSurface(glyph_surface);
  }
  if (lock_output) {
    SDL_LockSurface(output_surface);
  }

  const int W = glyph_surface->w;
  const int H = glyph_surface->h;
  const int glyph_height = std::max(1, H - 1);
  auto raw_glyph_alpha = [&](int px, int py) -> Uint8 {
    if (px < 0 || py < 0 || px >= W || py >= H) {
      return 0;
    }

    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_GetRGBA(readPixel(glyph_surface, px, py), glyph_surface->format, &red,
                &green, &blue, &alpha);
    return alpha;
  };

  // Round the sharp font corners (e.g. the peaks of "M") with a small
  // separable Gaussian blur of the alpha map. The blurred map then drives
  // both the silhouette / distance transform and the final per-pixel
  // transparency, so the whole logo reads as a softer, more glassy shape
  // without ever stepping over crisp font hinting. The blur radius is
  // derived from `START_MENU_LOGO_CORNER_BLUR_SIGMA`.
  const float blur_sigma =
      std::max(0.001f, START_MENU_LOGO_CORNER_BLUR_SIGMA);
  const int blur_radius = std::clamp(
      static_cast<int>(std::ceil(blur_sigma * 3.0f)), 1, 8);
  std::vector<float> blur_kernel(static_cast<size_t>(blur_radius * 2 + 1));
  float blur_kernel_sum = 0.0f;
  for (int k = -blur_radius; k <= blur_radius; ++k) {
    const float w = std::exp(-static_cast<float>(k * k) /
                             (2.0f * blur_sigma * blur_sigma));
    blur_kernel[static_cast<size_t>(k + blur_radius)] = w;
    blur_kernel_sum += w;
  }
  for (auto &w : blur_kernel) {
    w /= blur_kernel_sum;
  }
  std::vector<float> blur_horizontal(static_cast<size_t>(W * H), 0.0f);
  std::vector<float> smoothed_alpha(static_cast<size_t>(W * H), 0.0f);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      float sum = 0.0f;
      for (int k = -blur_radius; k <= blur_radius; ++k) {
        const int sx = std::clamp(x + k, 0, W - 1);
        sum += static_cast<float>(raw_glyph_alpha(sx, y)) *
               blur_kernel[static_cast<size_t>(k + blur_radius)];
      }
      blur_horizontal[static_cast<size_t>(y * W + x)] = sum;
    }
  }
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      float sum = 0.0f;
      for (int k = -blur_radius; k <= blur_radius; ++k) {
        const int sy = std::clamp(y + k, 0, H - 1);
        sum += blur_horizontal[static_cast<size_t>(sy * W + x)] *
               blur_kernel[static_cast<size_t>(k + blur_radius)];
      }
      smoothed_alpha[static_cast<size_t>(y * W + x)] = sum;
    }
  }

  auto glyph_alpha = [&](int px, int py) -> Uint8 {
    if (px < 0 || py < 0 || px >= W || py >= H) {
      return 0;
    }
    return static_cast<Uint8>(std::clamp(
        static_cast<int>(std::lround(
            smoothed_alpha[static_cast<size_t>(py * W + px)])),
        0, 255));
  };

  // Dark-blue glass body. Slight vertical gradient gives the volume a hint of
  // depth while keeping the overall tone deep navy. The actual visual life
  // comes from the rim refraction and the multi-light reflections below.
  // Color stops, rim colors and translucency are tuned via `definitions.h`.
  auto body_color_at = [&](float progress) -> SDL_Color {
    if (progress < 0.5f) {
      return LerpColor(START_MENU_LOGO_BODY_TOP_COLOR,
                       START_MENU_LOGO_BODY_MID_COLOR, progress * 2.0f);
    }
    return LerpColor(START_MENU_LOGO_BODY_MID_COLOR,
                     START_MENU_LOGO_BODY_DEEP_COLOR,
                     (progress - 0.5f) * 2.0f);
  };

  // Reflections from a small environment of light sources. Positions are in
  // normalized surface space (0..1 across width / height). Each light writes
  // an additive specular spot whose footprint is a Gaussian; the spot is
  // limited to the "flat top" of the glass via the rim_curve modulator so it
  // never bleeds across the rounded rim.
  struct GlassLight {
    float nx;
    float ny;
    float sigma;
    float intensity;
    Uint8 r;
    Uint8 g;
    Uint8 b;
    float power;  // exponent applied to the Gaussian for sharper / softer hot-spot
  };
  // Two restrained reflections from above, both cool blue-white. Deliberately
  // dim so the body stays a deep, translucent navy instead of being lit up
  // into white blobs.
  constexpr std::array<GlassLight, 2> kGlassLights{{
      // Primary key light: cool white, soft, upper-left.
      {0.22f, 0.22f, 0.20f, 0.55f, 220, 232, 255, 1.4f},
      // Secondary fill from upper-right, slightly cyan, broader and softer.
      {0.78f, 0.34f, 0.26f, 0.36f, 168, 208, 248, 1.1f},
  }};

  // Chamfer 3-4 distance transform: how far each opaque pixel sits from the
  // nearest "outside" pixel. The result drives the soft glass rim, the inner
  // refraction shading and the specular sweep without ever sampling on a
  // per-pixel boolean grid (which is what produced the old stair-stepped,
  // pixelated aesthetic).
  constexpr int kChamferOrth = 3;
  constexpr int kChamferDiag = 4;
  constexpr Uint8 kAlphaInsideThreshold = 16;
  const int kInfDistance = (W + H) * kChamferDiag + 1;
  std::vector<int> distance_field(static_cast<size_t>(W * H), kInfDistance);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (glyph_alpha(x, y) < kAlphaInsideThreshold) {
        distance_field[static_cast<size_t>(y * W + x)] = 0;
      }
    }
  }
  auto chamfer_relax = [&](int x, int y, int dx, int dy, int weight) {
    const int nx = x + dx;
    const int ny = y + dy;
    if (nx < 0 || ny < 0 || nx >= W || ny >= H) {
      return;
    }
    const int candidate =
        distance_field[static_cast<size_t>(ny * W + nx)] + weight;
    int &cell = distance_field[static_cast<size_t>(y * W + x)];
    if (candidate < cell) {
      cell = candidate;
    }
  };
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (distance_field[static_cast<size_t>(y * W + x)] == 0) {
        continue;
      }
      chamfer_relax(x, y, -1, -1, kChamferDiag);
      chamfer_relax(x, y, 0, -1, kChamferOrth);
      chamfer_relax(x, y, 1, -1, kChamferDiag);
      chamfer_relax(x, y, -1, 0, kChamferOrth);
    }
  }
  for (int y = H - 1; y >= 0; --y) {
    for (int x = W - 1; x >= 0; --x) {
      if (distance_field[static_cast<size_t>(y * W + x)] == 0) {
        continue;
      }
      chamfer_relax(x, y, 1, 1, kChamferDiag);
      chamfer_relax(x, y, 0, 1, kChamferOrth);
      chamfer_relax(x, y, -1, 1, kChamferDiag);
      chamfer_relax(x, y, 1, 0, kChamferOrth);
    }
  }

  // Tunables for the glass look — see definitions.h for what each constant
  // does. Rim softness is computed as a fraction of glyph height so the look
  // stays proportional across resolutions.
  const float rim_softness = std::max(
      2.5f, static_cast<float>(H) * START_MENU_LOGO_RIM_SOFTNESS_FACTOR);
  // Aspect ratio compensation so circular highlights stay circular even
  // though the surface is much wider than it is tall.
  const float light_aspect =
      (W > 0) ? static_cast<float>(H) / static_cast<float>(W) : 1.0f;

  auto mix_toward = [](float channel, Uint8 target, float amount) {
    return channel + (static_cast<float>(target) - channel) * amount;
  };

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const Uint8 alpha = glyph_alpha(x, y);
      if (alpha == 0) {
        continue;
      }

      const float progress = static_cast<float>(y) / glyph_height;
      const float ux = (W > 1) ? static_cast<float>(x) /
                                     static_cast<float>(W - 1)
                               : 0.0f;
      const float uy = progress;
      const SDL_Color base_color = body_color_at(progress);

      // Distance-from-edge in approximate pixel units (chamfer 3-4 uses
      // weight 3 for orthogonal moves).
      const float pixel_distance =
          static_cast<float>(distance_field[static_cast<size_t>(y * W + x)]) /
          static_cast<float>(kChamferOrth);
      const float rim_t =
          std::clamp(pixel_distance / rim_softness, 0.0f, 1.0f);
      // Quarter-circle profile: 0 right at the rim, 1 well inside the body.
      const float rim_curve =
          std::sqrt(std::max(0.0f, 2.0f * rim_t - rim_t * rim_t));
      const float rim_factor = 1.0f - rim_curve;

      // Top-of-letter rim catches a cool-white highlight (Fresnel-like).
      // Bottom rim picks up an almost-black outline so the silhouette stays
      // legible against any backdrop.
      const float light_split =
          std::clamp((0.45f - progress) * 2.4f, -1.0f, 1.0f);
      const float top_rim = rim_factor * std::max(0.0f, light_split) * 0.95f;
      const float bottom_rim =
          rim_factor * std::max(0.0f, -light_split) * 0.70f;
      // A subtle cyan refraction band right at the rim, on every side. Makes
      // the edge look like it bends light, which is a key glass cue.
      const float refraction_band =
          rim_factor * std::pow(rim_factor, 1.6f) *
          START_MENU_LOGO_REFRACTION_STRENGTH;

      // Multi-light specular accumulation. Each light adds an additive,
      // colored hot-spot. The whole stack is gated by rim_curve so the
      // reflections live on the "flat top" of the glass and never spill over
      // the rounded rim.
      float light_r = 0.0f;
      float light_g = 0.0f;
      float light_b = 0.0f;
      float light_total = 0.0f;
      for (const GlassLight &light : kGlassLights) {
        const float dx = ux - light.nx;
        const float dy = (uy - light.ny) * light_aspect;
        const float falloff =
            std::exp(-(dx * dx + dy * dy) /
                     (2.0f * light.sigma * light.sigma));
        const float shaped =
            std::pow(falloff, light.power) * light.intensity *
            START_MENU_LOGO_LIGHT_INTENSITY;
        light_r += static_cast<float>(light.r) * shaped;
        light_g += static_cast<float>(light.g) * shaped;
        light_b += static_cast<float>(light.b) * shaped;
        light_total += shaped;
      }
      const float body_gate =
          std::pow(rim_curve, START_MENU_LOGO_LIGHT_GATE_EXPONENT);
      light_r *= body_gate;
      light_g *= body_gate;
      light_b *= body_gate;
      light_total *= body_gate;

      // Inner refraction: deepens the glass toward the geometric center so
      // the body reads as volume rather than a flat fill.
      const float inner_depth =
          rim_curve * (START_MENU_LOGO_INNER_DEPTH_BASE +
                       START_MENU_LOGO_INNER_DEPTH_RAMP * progress);

      float r = base_color.r;
      float g = base_color.g;
      float b = base_color.b;

      r = mix_toward(r, START_MENU_LOGO_BODY_DEEP_COLOR.r, inner_depth);
      g = mix_toward(g, START_MENU_LOGO_BODY_DEEP_COLOR.g, inner_depth);
      b = mix_toward(b, START_MENU_LOGO_BODY_DEEP_COLOR.b, inner_depth);

      r = mix_toward(r, START_MENU_LOGO_RIM_HIGHLIGHT_COLOR.r, top_rim);
      g = mix_toward(g, START_MENU_LOGO_RIM_HIGHLIGHT_COLOR.g, top_rim);
      b = mix_toward(b, START_MENU_LOGO_RIM_HIGHLIGHT_COLOR.b, top_rim);

      r = mix_toward(r, START_MENU_LOGO_RIM_SHADOW_COLOR.r, bottom_rim);
      g = mix_toward(g, START_MENU_LOGO_RIM_SHADOW_COLOR.g, bottom_rim);
      b = mix_toward(b, START_MENU_LOGO_RIM_SHADOW_COLOR.b, bottom_rim);

      r = mix_toward(r, START_MENU_LOGO_RIM_REFRACTION_COLOR.r,
                     refraction_band);
      g = mix_toward(g, START_MENU_LOGO_RIM_REFRACTION_COLOR.g,
                     refraction_band);
      b = mix_toward(b, START_MENU_LOGO_RIM_REFRACTION_COLOR.b,
                     refraction_band);

      // Add the multi-light reflections (additive on top of the body color).
      r += light_r;
      g += light_g;
      b += light_b;

      const int red = std::clamp(static_cast<int>(std::lround(r)), 0, 255);
      const int green = std::clamp(static_cast<int>(std::lround(g)), 0, 255);
      const int blue = std::clamp(static_cast<int>(std::lround(b)), 0, 255);

      // Translucency: rim ~ START_MENU_LOGO_RIM_ALPHA, deep body ~
      // START_MENU_LOGO_BODY_ALPHA. Specular peaks pull the alpha back up so
      // reflections still punch through the otherwise see-through body.
      const float alpha_mul = std::clamp(
          START_MENU_LOGO_BODY_ALPHA +
              (START_MENU_LOGO_RIM_ALPHA - START_MENU_LOGO_BODY_ALPHA) *
                  rim_factor +
              std::min(0.55f, light_total * 0.40f) * (1.0f - rim_factor),
          0.0f, 1.0f);
      const Uint8 out_alpha = static_cast<Uint8>(
          std::clamp(static_cast<int>(std::lround(static_cast<float>(alpha) *
                                                  alpha_mul)),
                     0, 255));

      writePixel(output_surface, x, y,
                 SDL_MapRGBA(output_surface->format, red, green, blue,
                             out_alpha));
    }
  }

  if (lock_output) {
    SDL_UnlockSurface(output_surface);
  }
  if (lock_glyph) {
    SDL_UnlockSurface(glyph_surface);
  }

  SDL_FreeSurface(glyph_surface);
  return output_surface;
}

SDL_Surface *Renderer::createBrickTextSurface(TTF_Font *font,
                                              const std::string &text,
                                              const SDL_Color &outline_color) {
  SDL_Surface *glyph_surface_raw =
      TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{255, 255, 255, 255});
  if (glyph_surface_raw == nullptr) {
    return nullptr;
  }

  SDL_Surface *glyph_surface =
      SDL_ConvertSurfaceFormat(glyph_surface_raw, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(glyph_surface_raw);
  if (glyph_surface == nullptr) {
    return nullptr;
  }

  SDL_Surface *output_surface = SDL_CreateRGBSurfaceWithFormat(
      0, glyph_surface->w, glyph_surface->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (output_surface == nullptr) {
    SDL_FreeSurface(glyph_surface);
    return nullptr;
  }

  SDL_FillRect(output_surface, nullptr,
               SDL_MapRGBA(output_surface->format, 0, 0, 0, 0));

  const bool lock_glyph = SDL_MUSTLOCK(glyph_surface);
  const bool lock_output = SDL_MUSTLOCK(output_surface);
  const bool lock_brick =
      sdl_logo_brick_surface != nullptr && SDL_MUSTLOCK(sdl_logo_brick_surface);

  if (lock_glyph) {
    SDL_LockSurface(glyph_surface);
  }
  if (lock_output) {
    SDL_LockSurface(output_surface);
  }
  if (lock_brick) {
    SDL_LockSurface(sdl_logo_brick_surface);
  }

  const SDL_Color accent_color{177, 60, 45, 255};
  const int glyph_height = std::max(1, glyph_surface->h - 1);
  auto glyph_alpha = [&](int px, int py) -> Uint8 {
    if (px < 0 || py < 0 || px >= glyph_surface->w || py >= glyph_surface->h) {
      return 0;
    }

    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_GetRGBA(readPixel(glyph_surface, px, py), glyph_surface->format, &red,
                &green, &blue, &alpha);
    return alpha;
  };

  for (int y = 0; y < glyph_surface->h; y++) {
    for (int x = 0; x < glyph_surface->w; x++) {
      const Uint8 alpha = glyph_alpha(x, y);
      if (alpha == 0) {
        continue;
      }

      Uint8 brick_r = accent_color.r;
      Uint8 brick_g = accent_color.g;
      Uint8 brick_b = accent_color.b;
      Uint8 brick_a = 255;

      if (sdl_logo_brick_surface != nullptr) {
        const int sample_x =
            (x * 3 + (y / 6) * 5) % sdl_logo_brick_surface->w;
        const int sample_y =
            (y * 3 + (x / 8) * 3) % sdl_logo_brick_surface->h;
        SDL_GetRGBA(readPixel(sdl_logo_brick_surface, sample_x, sample_y),
                    sdl_logo_brick_surface->format, &brick_r, &brick_g, &brick_b,
                    &brick_a);
      }

      float brightness = 1.16f - 0.36f * static_cast<float>(y) / glyph_height;
      int red = static_cast<int>((brick_r * 0.72f + accent_color.r * 0.28f) *
                                 brightness);
      int green =
          static_cast<int>((brick_g * 0.64f + accent_color.g * 0.36f) *
                           brightness);
      int blue =
          static_cast<int>((brick_b * 0.54f + accent_color.b * 0.46f) *
                           brightness);

      const Uint8 left = glyph_alpha(x - 1, y);
      const Uint8 right = glyph_alpha(x + 1, y);
      const Uint8 up = glyph_alpha(x, y - 1);
      const Uint8 down = glyph_alpha(x, y + 1);
      const bool edge = left == 0 || right == 0 || up == 0 || down == 0;
      const bool highlight_edge = left == 0 || up == 0;
      const bool shadow_edge = right == 0 || down == 0;

      if (highlight_edge) {
        red = std::min(255, static_cast<int>(red * 1.08f + 16));
        green = std::min(255, static_cast<int>(green * 1.05f + 10));
        blue = std::min(255, static_cast<int>(blue * 1.02f + 8));
      }
      if (shadow_edge) {
        red = static_cast<int>(red * 0.82f);
        green = static_cast<int>(green * 0.78f);
        blue = static_cast<int>(blue * 0.74f);
      }
      if (edge) {
        red = (red * 2 + outline_color.r * 3) / 5;
        green = (green * 2 + outline_color.g * 3) / 5;
        blue = (blue * 2 + outline_color.b * 3) / 5;
      }

      red = std::clamp(red, 0, 255);
      green = std::clamp(green, 0, 255);
      blue = std::clamp(blue, 0, 255);

      writePixel(output_surface, x, y,
                 SDL_MapRGBA(output_surface->format, red, green, blue, alpha));
    }
  }

  if (lock_brick) {
    SDL_UnlockSurface(sdl_logo_brick_surface);
  }
  if (lock_output) {
    SDL_UnlockSurface(output_surface);
  }
  if (lock_glyph) {
    SDL_UnlockSurface(glyph_surface);
  }

  SDL_FreeSurface(glyph_surface);
  return output_surface;
}

Uint32 Renderer::readPixel(SDL_Surface *surface, int x, int y) {
  Uint32 *pixels = static_cast<Uint32 *>(surface->pixels);
  return pixels[(y * surface->pitch / 4) + x];
}

void Renderer::writePixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {
  Uint32 *pixels = static_cast<Uint32 *>(surface->pixels);
  pixels[(y * surface->pitch / 4) + x] = pixel;
}

void Renderer::drawpacman() {
  if (game == nullptr || game->pacman == nullptr) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const bool final_loss_sequence_active = game->IsFinalLossSequenceActive(now);
  const bool potion_invulnerable = game->pacman->invulnerable_until_ticks > now;
  const bool life_recovery_active = game->IsPacmanRecoveringFromLifeLoss(now);
  const bool slimed = game->pacman->slimed_until_ticks > now;
  const bool front_facing_due_to_gas =
      slimed && game->pacman->paralyzed_until_ticks > now;
  const bool walking =
      game->pacman->px_delta.x != 0 || game->pacman->px_delta.y != 0;
  const Uint32 flicker_phase_ms =
      std::max<Uint32>(1, PACMAN_RECOVERY_FLICKER_PHASE_MS);
  const Uint8 pacman_alpha =
      (!life_recovery_active || ((now / flicker_phase_ms) % 2 == 0))
          ? 255
          : PACMAN_RECOVERY_ALPHA;
  Directions facing_direction = game->pacman->facing_direction;
  if (front_facing_due_to_gas || facing_direction == Directions::None) {
    facing_direction = Directions::Down;
  }
  SDL_Texture *pacman_texture = getPacmanTexture(facing_direction, walking);

  if (game->dead || final_loss_sequence_active) {
    drawDefeatEffect();
    return;
  }

  const auto draw_slime_overlay = [&](const SDL_Rect &anchor_rect,
                                      double rotation_degrees) {
    if (!slimed) {
      return;
    }

    const Uint32 fade_start_ticks =
        (game->pacman->slimed_until_ticks >
         static_cast<Uint32>(SLIME_OVERLAY_FADE_MS))
            ? game->pacman->slimed_until_ticks -
                  static_cast<Uint32>(SLIME_OVERLAY_FADE_MS)
            : 0;
    double alpha_factor = 1.0;
    if (now >= fade_start_ticks) {
      alpha_factor =
          1.0 - std::clamp(static_cast<double>(now - fade_start_ticks) /
                               static_cast<double>(SLIME_OVERLAY_FADE_MS),
                           0.0, 1.0);
    }
    if (alpha_factor <= 0.0) {
      return;
    }

    (void)rotation_degrees;
    drawPacmanGasCloudlets(anchor_rect, alpha_factor, now);
  };

  if (game->pacman->teleport_animation_active) {
    const Uint32 elapsed =
        SDL_GetTicks() - game->pacman->teleport_animation_started_ticks;
    const bool in_departure_phase = elapsed < TELEPORT_ANIMATION_PHASE_MS;
    const double phase_progress =
        std::clamp(static_cast<double>(
                       in_departure_phase ? elapsed
                                          : (elapsed - TELEPORT_ANIMATION_PHASE_MS)) /
                       static_cast<double>(TELEPORT_ANIMATION_PHASE_MS),
                   0.0, 1.0);
    const MapCoord render_coord =
        in_departure_phase ? game->pacman->teleport_animation_from_coord
                           : game->pacman->teleport_animation_to_coord;
    const double scale = in_departure_phase ? (1.0 - phase_progress)
                                            : phase_progress;
    const double rotation =
        (in_departure_phase ? phase_progress : (1.0 + phase_progress)) * 1080.0;
    const PixelCoord base_px = getPixelCoord(render_coord, 0, 0);
    const int size =
        std::max(1, static_cast<int>(element_size * 0.9 * std::max(0.0, scale)));
    const SDL_Rect animated_rect{
        base_px.x + element_size / 2 - size / 2,
        base_px.y + element_size / 2 - size / 2, size, size};

    if (potion_invulnerable) {
      drawPacmanShield(animated_rect.x + animated_rect.w / 2,
                       animated_rect.y + animated_rect.h / 2,
                       std::max(8, animated_rect.w / 2 + 5),
                       static_cast<double>(now) / 180.0);
    }

    const Uint8 spark_alpha = static_cast<Uint8>(
        std::clamp((in_departure_phase ? (1.0 - phase_progress)
                                       : (0.35 + 0.65 * phase_progress)) *
                       170.0,
                   0.0, 200.0));
    SDL_SetRenderDrawColor(sdl_renderer, 130, 214, 255, spark_alpha / 2);
    SDL_RenderFillCircle(sdl_renderer, base_px.x + element_size / 2,
                         base_px.y + element_size / 2,
                         std::max(3, static_cast<int>(element_size * 0.42 * scale)));
    SDL_SetRenderDrawColor(sdl_renderer, 220, 245, 255, spark_alpha);
    for (int spark = 0; spark < 5; spark++) {
      const double angle =
          (rotation / 180.0) * 3.1415926535 + spark * (2.0 * 3.1415926535 / 5.0);
      const int spark_length =
          std::max(4, static_cast<int>(element_size * (0.10 + 0.14 * scale)));
      SDL_RenderDrawLine(
          sdl_renderer, base_px.x + element_size / 2,
          base_px.y + element_size / 2,
          base_px.x + element_size / 2 +
              static_cast<int>(std::cos(angle) * spark_length),
          base_px.y + element_size / 2 +
              static_cast<int>(std::sin(angle) * spark_length));
    }

    if (size > 1 && pacman_texture != nullptr) {
      SDL_SetTextureAlphaMod(pacman_texture, pacman_alpha);
      SDL_RenderCopyEx(sdl_renderer, pacman_texture, nullptr, &animated_rect,
                       rotation, nullptr, SDL_FLIP_NONE);
      SDL_SetTextureAlphaMod(pacman_texture, 255);
    }
    draw_slime_overlay(animated_rect, rotation);
    return;
  }

  game->pacman->px_coord = getPixelCoord(
      game->pacman->map_coord, element_size * game->pacman->px_delta.x / 100.0,
      element_size * game->pacman->px_delta.y / 100.0);
  sdl_pacman_rect =
      SDL_Rect{game->pacman->px_coord.x + int(element_size * 0.05),
               game->pacman->px_coord.y + int(element_size * 0.05),
               int(element_size * 0.9), int(element_size * 0.9)};
  if (potion_invulnerable) {
    drawPacmanShield(sdl_pacman_rect.x + sdl_pacman_rect.w / 2,
                     sdl_pacman_rect.y + sdl_pacman_rect.h / 2,
                     std::max(8, sdl_pacman_rect.w / 2 + 5),
                     static_cast<double>(now) / 180.0);
  }
  if (pacman_texture != nullptr) {
    SDL_SetTextureAlphaMod(pacman_texture, pacman_alpha);
    SDL_RenderCopy(sdl_renderer, pacman_texture, nullptr, &sdl_pacman_rect);
    SDL_SetTextureAlphaMod(pacman_texture, 255);
  }
  draw_slime_overlay(sdl_pacman_rect, 0.0);
  if (game->pacman->paralyzed_until_ticks > now) {
    drawStunStars(sdl_pacman_rect.x + sdl_pacman_rect.w / 2,
                  sdl_pacman_rect.y - static_cast<int>(element_size * 0.25),
                  static_cast<int>(element_size *
                                   STUN_STARS_ORBIT_RADIUS_CELLS),
                  static_cast<int>(element_size * STUN_STARS_RADIUS_CELLS),
                  now);
  }
}

void Renderer::drawbonusflask() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &potion = game->invulnerability_potion;
  if (!potion.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (potion.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - potion.fade_started_ticks) /
                             static_cast<double>(BLUE_POTION_FADE_MS),
                         0.0, 1.0);
  }

  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord potion_px = getPixelCoord(potion.coord, 0, 0);
  const int center_x = potion_px.x + element_size / 2;
  const int center_y = potion_px.y + element_size / 2;
  const double wobble_clock =
      static_cast<double>(now + potion.animation_seed * 53) / 260.0;
  const double pulse = 0.76 + 0.24 * std::sin(wobble_clock);
  const Uint8 glow_alpha =
      static_cast<Uint8>(std::clamp(120.0 * alpha_factor, 0.0, 140.0));
  const Uint8 glass_alpha =
      static_cast<Uint8>(std::clamp(175.0 * alpha_factor, 0.0, 200.0));
  const Uint8 fluid_alpha =
      static_cast<Uint8>(std::clamp(210.0 * alpha_factor, 0.0, 230.0));

  SDL_SetRenderDrawColor(sdl_renderer, 72, 164, 255, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(6, static_cast<int>(element_size * 0.30 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 124, 214, 255, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(8, static_cast<int>(element_size * 0.38 * pulse)));

  const int flask_width = std::max(10, static_cast<int>(element_size * 0.34));
  const int flask_body_height =
      std::max(12, static_cast<int>(element_size * 0.36));
  const int flask_neck_width =
      std::max(5, static_cast<int>(flask_width * 0.42));
  const int flask_neck_height =
      std::max(5, static_cast<int>(element_size * 0.16));
  const int flask_top = center_y - flask_body_height / 2;

  SDL_Rect neck_rect{center_x - flask_neck_width / 2,
                     flask_top - flask_neck_height / 2, flask_neck_width,
                     flask_neck_height};
  SDL_Rect body_rect{center_x - flask_width / 2, flask_top, flask_width,
                     flask_body_height};

  SDL_SetRenderDrawColor(sdl_renderer, 225, 245, 255, glass_alpha / 4);
  SDL_RenderFillRect(sdl_renderer, &neck_rect);
  SDL_RenderFillRect(sdl_renderer, &body_rect);
  SDL_RenderFillCircle(sdl_renderer, center_x, body_rect.y + body_rect.h,
                       std::max(5, flask_width / 2));

  const int liquid_height = std::max(
      5, static_cast<int>(flask_body_height * (0.42 + 0.10 * std::sin(wobble_clock))));
  SDL_Rect liquid_rect{body_rect.x + 2, body_rect.y + body_rect.h - liquid_height,
                       std::max(2, body_rect.w - 4), liquid_height};
  SDL_SetRenderDrawColor(sdl_renderer, 44, 112, 255, fluid_alpha);
  SDL_RenderFillRect(sdl_renderer, &liquid_rect);
  SDL_SetRenderDrawColor(sdl_renderer, 118, 204, 255, fluid_alpha);
  SDL_RenderFillCircle(
      sdl_renderer, center_x,
      body_rect.y + body_rect.h - std::max(2, liquid_height / 3),
      std::max(4, flask_width / 2 - 2));

  for (int bubble = 0; bubble < 3; bubble++) {
    const double bubble_phase = wobble_clock * 1.15 + bubble * 1.7;
    const double bubble_progress = std::fmod(now / 900.0 + bubble * 0.31, 1.0);
    const int bubble_x = center_x +
                         static_cast<int>(std::sin(bubble_phase) * flask_width * 0.16);
    const int bubble_y =
        liquid_rect.y + liquid_rect.h -
        static_cast<int>(bubble_progress * std::max(6, liquid_rect.h - 2));
    const int bubble_radius = std::max(2, element_size / 14 + bubble % 2);
    SDL_SetRenderDrawColor(sdl_renderer, 220, 245, 255, glow_alpha);
    SDL_RenderFillCircle(sdl_renderer, bubble_x, bubble_y, bubble_radius);
  }

  SDL_SetRenderDrawColor(sdl_renderer, 224, 246, 255, glass_alpha);
  SDL_RenderDrawRect(sdl_renderer, &neck_rect);
  SDL_RenderDrawRect(sdl_renderer, &body_rect);
  SDL_RenderDrawLine(sdl_renderer, body_rect.x, body_rect.y + body_rect.h,
                     center_x - flask_width / 4, body_rect.y + body_rect.h + flask_width / 3);
  SDL_RenderDrawLine(sdl_renderer, body_rect.x + body_rect.w,
                     body_rect.y + body_rect.h,
                     center_x + flask_width / 4,
                     body_rect.y + body_rect.h + flask_width / 3);
  SDL_RenderDrawCircle(sdl_renderer, center_x, body_rect.y + body_rect.h,
                       std::max(5, flask_width / 2));
}

void Renderer::drawLovePotionIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                                  double animation_clock) {
  const int center_x = icon_rect.x + icon_rect.w / 2;
  const int center_y = icon_rect.y + icon_rect.h / 2;
  const int tube_width = std::max(5, icon_rect.w / 3);
  const int tube_height = std::max(10, icon_rect.h * 3 / 4);
  const int tube_top = center_y - tube_height / 2;
  const SDL_Rect tube_rect{center_x - tube_width / 2, tube_top, tube_width,
                           tube_height};
  const Uint8 glass_alpha = static_cast<Uint8>(
      std::clamp(static_cast<double>(alpha) * 0.78, 0.0, 255.0));
  const Uint8 liquid_alpha = alpha;

  SDL_SetRenderDrawColor(sdl_renderer, 240, 250, 255, glass_alpha / 4);
  SDL_RenderFillRect(sdl_renderer, &tube_rect);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 42, 54, liquid_alpha);
  const int liquid_height = std::max(
      4, static_cast<int>(tube_height * (0.58 + 0.08 * std::sin(animation_clock))));
  SDL_Rect liquid_rect{tube_rect.x + 2, tube_rect.y + tube_rect.h - liquid_height,
                       std::max(2, tube_rect.w - 4), liquid_height};
  SDL_RenderFillRect(sdl_renderer, &liquid_rect);
  SDL_RenderFillCircle(sdl_renderer, center_x, liquid_rect.y,
                       std::max(2, tube_width / 2 - 2));

  for (int bubble = 0; bubble < 4; ++bubble) {
    const double phase = animation_clock * 0.9 + bubble * 1.8;
    const double progress = std::fmod(animation_clock * 0.13 + bubble * 0.27, 1.0);
    const int bubble_x = center_x +
                         static_cast<int>(std::sin(phase) * tube_width * 0.22);
    const int bubble_y =
        liquid_rect.y + liquid_rect.h -
        static_cast<int>(progress * std::max(4, liquid_rect.h));
    const int bubble_radius = std::max(1, icon_rect.w / 13 + (bubble % 2));
    SDL_SetRenderDrawColor(sdl_renderer, 255, 164, 170, glass_alpha);
    SDL_RenderFillCircle(sdl_renderer, bubble_x, bubble_y, bubble_radius);
  }

  SDL_SetRenderDrawColor(sdl_renderer, 232, 246, 255, glass_alpha);
  SDL_RenderDrawRect(sdl_renderer, &tube_rect);
  SDL_RenderDrawLine(sdl_renderer, tube_rect.x - tube_width / 4, tube_rect.y,
                     tube_rect.x + tube_rect.w + tube_width / 4, tube_rect.y);
  SDL_RenderDrawCircle(sdl_renderer, center_x, tube_rect.y + tube_rect.h,
                       std::max(3, tube_width / 2));
}

void Renderer::drawWalkieTalkieIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                                    double animation_clock) {
  if (sdl_walkie_talkie_texture != nullptr && sdl_walkie_talkie_size.x > 0 &&
      sdl_walkie_talkie_size.y > 0) {
    const double scale_by_width = static_cast<double>(icon_rect.w) /
                                  static_cast<double>(sdl_walkie_talkie_size.x);
    const double scale_by_height =
        static_cast<double>(icon_rect.h) /
        static_cast<double>(sdl_walkie_talkie_size.y);
    const double scale = std::min(scale_by_width, scale_by_height);
    const int draw_width = std::max(
        1, static_cast<int>(std::lround(sdl_walkie_talkie_size.x * scale)));
    const int draw_height = std::max(
        1, static_cast<int>(std::lround(sdl_walkie_talkie_size.y * scale)));
    const int bob_offset =
        static_cast<int>(std::lround(std::sin(animation_clock) * icon_rect.h * 0.04));
    const SDL_Rect draw_rect{icon_rect.x + (icon_rect.w - draw_width) / 2,
                             icon_rect.y + (icon_rect.h - draw_height) / 2 + bob_offset,
                             draw_width, draw_height};
    SDL_SetTextureBlendMode(sdl_walkie_talkie_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(sdl_walkie_talkie_texture, alpha);
    SDL_RenderCopy(sdl_renderer, sdl_walkie_talkie_texture, nullptr, &draw_rect);
    SDL_SetTextureAlphaMod(sdl_walkie_talkie_texture, 255);
    return;
  }

  SDL_SetRenderDrawColor(sdl_renderer, 42, 46, 48, alpha);
  SDL_Rect fallback_rect{icon_rect.x + icon_rect.w / 4,
                         icon_rect.y + icon_rect.h / 5, icon_rect.w / 2,
                         icon_rect.h * 3 / 5};
  SDL_RenderFillRect(sdl_renderer, &fallback_rect);
}

void Renderer::drawRocketIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                              double animation_clock) {
  if (icon_rect.w <= 0 || icon_rect.h <= 0) {
    return;
  }

  if (sdl_rocket_texture != nullptr && sdl_rocket_size.x > 0 &&
      sdl_rocket_size.y > 0) {
    const double scale_by_width =
        static_cast<double>(icon_rect.w) / static_cast<double>(sdl_rocket_size.x);
    const double scale_by_height =
        static_cast<double>(icon_rect.h) / static_cast<double>(sdl_rocket_size.y);
    const double scale = std::min(scale_by_width, scale_by_height);
    const int draw_width = std::max(
        1, static_cast<int>(std::lround(sdl_rocket_size.x * scale)));
    const int draw_height = std::max(
        1, static_cast<int>(std::lround(sdl_rocket_size.y * scale)));
    const int bob_offset = static_cast<int>(
        std::lround(std::sin(animation_clock) * icon_rect.h * 0.04));
    const SDL_Rect draw_rect{icon_rect.x + (icon_rect.w - draw_width) / 2,
                             icon_rect.y + (icon_rect.h - draw_height) / 2 +
                                 bob_offset,
                             draw_width, draw_height};
    SDL_SetTextureBlendMode(sdl_rocket_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(sdl_rocket_texture, alpha);
    SDL_RenderCopy(sdl_renderer, sdl_rocket_texture, nullptr, &draw_rect);
    SDL_SetTextureAlphaMod(sdl_rocket_texture, 255);
    return;
  }

  const int center_x = icon_rect.x + icon_rect.w / 2;
  const int center_y = icon_rect.y + icon_rect.h / 2;
  const int body_width = std::max(6, icon_rect.w / 3);
  const int body_height = std::max(10, icon_rect.h / 2);
  SDL_Rect body_rect{center_x - body_width / 2, center_y - body_height / 4,
                     body_width, body_height};
  SDL_SetRenderDrawColor(sdl_renderer, 132, 164, 196, alpha);
  SDL_RenderFillRect(sdl_renderer, &body_rect);
  SDL_SetRenderDrawColor(sdl_renderer, 228, 82, 52, alpha);
  SDL_RenderDrawLine(sdl_renderer, center_x, icon_rect.y, body_rect.x,
                     body_rect.y + body_rect.h / 3);
  SDL_RenderDrawLine(sdl_renderer, center_x, icon_rect.y,
                     body_rect.x + body_rect.w, body_rect.y + body_rect.h / 3);
  SDL_RenderDrawLine(sdl_renderer, center_x, icon_rect.y, center_x,
                     body_rect.y + body_rect.h / 3);
  SDL_RenderDrawLine(sdl_renderer, body_rect.x, body_rect.y + body_rect.h,
                     body_rect.x - std::max(2, icon_rect.w / 8),
                     body_rect.y + body_rect.h - std::max(2, icon_rect.h / 5));
  SDL_RenderDrawLine(sdl_renderer, body_rect.x + body_rect.w,
                     body_rect.y + body_rect.h,
                     body_rect.x + body_rect.w + std::max(2, icon_rect.w / 8),
                     body_rect.y + body_rect.h - std::max(2, icon_rect.h / 5));
}

void Renderer::drawBiohazardIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                                 double animation_clock,
                                 double rotation_degrees, double scale) {
  if (icon_rect.w <= 0 || icon_rect.h <= 0 || alpha == 0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const int center_x = icon_rect.x + icon_rect.w / 2;
  const int center_y = icon_rect.y + icon_rect.h / 2;
  const int base_radius = std::max(
      4, static_cast<int>(std::lround(
             std::min(icon_rect.w, icon_rect.h) * 0.47 * std::max(0.4, scale))));
  const int orbit_radius = std::max(4, static_cast<int>(std::lround(base_radius * 0.46)));
  const int arc_thickness =
      std::max(2, static_cast<int>(std::lround(base_radius * 0.20)));
  const int core_radius =
      std::max(2, static_cast<int>(std::lround(base_radius * 0.15)));
  const int connector_radius =
      std::max(1, static_cast<int>(std::lround(base_radius * 0.06)));
  const double rotation_radians =
      rotation_degrees * 3.14159265358979323846 / 180.0;
  const SDL_Color disc_color{244, 216, 28, alpha};
  const SDL_Color disc_highlight{255, 246, 156, alpha};
  const SDL_Color symbol_color{12, 12, 12, alpha};

  auto rotated_offset = [&](double radius, double angle) {
    return SDL_FPoint{
        static_cast<float>(center_x + std::cos(angle + rotation_radians) * radius),
        static_cast<float>(center_y + std::sin(angle + rotation_radians) * radius)};
  };
  auto draw_brush_line = [&](SDL_FPoint from, SDL_FPoint to, int radius,
                             const SDL_Color &color) {
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    const int steps = std::max(
        1, static_cast<int>(std::lround(
               std::hypot(static_cast<double>(to.x - from.x),
                          static_cast<double>(to.y - from.y)))));
    for (int step = 0; step <= steps; ++step) {
      const double t = static_cast<double>(step) / static_cast<double>(steps);
      const int x = static_cast<int>(std::lround(
          from.x + (to.x - from.x) * static_cast<float>(t)));
      const int y = static_cast<int>(std::lround(
          from.y + (to.y - from.y) * static_cast<float>(t)));
      SDL_RenderFillCircle(sdl_renderer, x, y, radius);
    }
  };
  auto draw_arc = [&](double angle_center) {
    const double start_angle = angle_center - 1.02;
    const double end_angle = angle_center + 1.02;
    SDL_FPoint previous = rotated_offset(orbit_radius, start_angle);
    for (int step = 1; step <= 24; ++step) {
      const double t = static_cast<double>(step) / 24.0;
      const double angle = start_angle + (end_angle - start_angle) * t;
      const SDL_FPoint point = rotated_offset(orbit_radius, angle);
      draw_brush_line(previous, point, arc_thickness / 2, symbol_color);
      previous = point;
    }

    const SDL_FPoint connector_from =
        rotated_offset(core_radius * 1.2, angle_center);
    const SDL_FPoint connector_to =
        rotated_offset(std::max(1, orbit_radius - arc_thickness), angle_center);
    draw_brush_line(connector_from, connector_to, connector_radius, symbol_color);
  };

  SDL_SetRenderDrawColor(sdl_renderer, disc_color.r, disc_color.g, disc_color.b,
                         disc_color.a);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y, base_radius);
  SDL_SetRenderDrawColor(sdl_renderer, 188, 138, 16,
                         static_cast<Uint8>(std::clamp(alpha * 0.8, 0.0, 255.0)));
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y, base_radius);
  SDL_SetRenderDrawColor(
      sdl_renderer, disc_highlight.r, disc_highlight.g, disc_highlight.b,
      static_cast<Uint8>(std::clamp(0.5 * static_cast<double>(disc_highlight.a),
                                    0.0, 255.0)));
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(3, base_radius - 1));

  for (int arm = 0; arm < 3; ++arm) {
    draw_arc(-3.14159265358979323846 / 2.0 +
             arm * (2.0 * 3.14159265358979323846 / 3.0));
  }

  SDL_SetRenderDrawColor(sdl_renderer, symbol_color.r, symbol_color.g,
                         symbol_color.b, symbol_color.a);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y, core_radius);

  for (int spark = 0; spark < 3; ++spark) {
    const double angle =
        animation_clock * 0.8 + spark * (2.0 * 3.14159265358979323846 / 3.0);
    const int spark_x =
        center_x + static_cast<int>(std::cos(angle) * std::max(2, base_radius / 6));
    const int spark_y =
        center_y + static_cast<int>(std::sin(angle) * std::max(2, base_radius / 6));
    SDL_SetRenderDrawColor(sdl_renderer, 255, 248, 196,
                           static_cast<Uint8>(std::clamp(alpha * 0.45, 0.0, 255.0)));
    SDL_RenderFillCircle(sdl_renderer, spark_x, spark_y, 1);
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawNuclearBombIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                                   double rotation_degrees) {
  if (icon_rect.w <= 0 || icon_rect.h <= 0 || alpha == 0) {
    return;
  }

  if (sdl_nuclear_bomb_texture != nullptr && sdl_nuclear_bomb_size.x > 0 &&
      sdl_nuclear_bomb_size.y > 0) {
    const int dominant_dimension =
        std::max(sdl_nuclear_bomb_size.x, sdl_nuclear_bomb_size.y);
    const double scale =
        static_cast<double>(std::max(icon_rect.w, icon_rect.h)) /
        static_cast<double>(dominant_dimension);
    const int draw_width = std::max(
        1, static_cast<int>(std::lround(sdl_nuclear_bomb_size.x * scale)));
    const int draw_height = std::max(
        1, static_cast<int>(std::lround(sdl_nuclear_bomb_size.y * scale)));
    const SDL_Rect draw_rect{
        icon_rect.x + (icon_rect.w - draw_width) / 2,
        icon_rect.y + (icon_rect.h - draw_height) / 2,
        draw_width,
        draw_height};
    SDL_SetTextureBlendMode(sdl_nuclear_bomb_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(sdl_nuclear_bomb_texture, alpha);
    SDL_RenderCopyEx(sdl_renderer, sdl_nuclear_bomb_texture, nullptr,
                     &draw_rect, rotation_degrees, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(sdl_nuclear_bomb_texture, 255);
    return;
  }

  const int center_x = icon_rect.x + icon_rect.w / 2;
  const int center_y = icon_rect.y + icon_rect.h / 2;
  const int radius_x = std::max(8, icon_rect.w / 2);
  const int radius_y = std::max(6, icon_rect.h / 3);
  SDL_SetRenderDrawColor(sdl_renderer, 46, 84, 64, alpha);
  SDL_RenderFillEllipse(sdl_renderer, center_x, center_y, radius_x, radius_y);
  SDL_SetRenderDrawColor(sdl_renderer, 218, 170, 40, alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(4, std::min(icon_rect.w, icon_rect.h) / 5));
  drawBiohazardIcon(
      SDL_Rect{center_x - icon_rect.w / 7, center_y - icon_rect.h / 7,
               std::max(4, icon_rect.w / 3), std::max(4, icon_rect.h / 3)},
      alpha, 0.0, 0.0, 0.8);
}

void Renderer::drawNuclearTargetMarkerIcon(const SDL_Rect &icon_rect,
                                           Uint8 alpha, double scale) {
  if (icon_rect.w <= 0 || icon_rect.h <= 0 || alpha == 0 || scale <= 0.0) {
    return;
  }

  SDL_Rect draw_rect = icon_rect;
  draw_rect.w =
      std::max(1, static_cast<int>(std::lround(icon_rect.w * scale)));
  draw_rect.h =
      std::max(1, static_cast<int>(std::lround(icon_rect.h * scale)));
  draw_rect.x = icon_rect.x + (icon_rect.w - draw_rect.w) / 2;
  draw_rect.y = icon_rect.y + (icon_rect.h - draw_rect.h) / 2;

  if (sdl_nuclear_target_marker_texture != nullptr &&
      sdl_nuclear_target_marker_size.x > 0 &&
      sdl_nuclear_target_marker_size.y > 0) {
    SDL_SetTextureBlendMode(sdl_nuclear_target_marker_texture,
                            SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(sdl_nuclear_target_marker_texture, alpha);
    SDL_RenderCopy(sdl_renderer, sdl_nuclear_target_marker_texture, nullptr,
                   &draw_rect);
    SDL_SetTextureAlphaMod(sdl_nuclear_target_marker_texture, 255);
    return;
  }

  const int center_x = draw_rect.x + draw_rect.w / 2;
  const int center_y = draw_rect.y + draw_rect.h / 2;
  drawBiohazardIcon(draw_rect, alpha, 0.0, 0.0, 1.0);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 248, 120, alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(4, draw_rect.w / 2 - 2));
}

void Renderer::drawRocketFlight(const SDL_FPoint &center, Directions direction,
                                int max_dimension, int frame_index,
                                Uint8 alpha) {
  if (max_dimension <= 0) {
    return;
  }

  const double angle_degrees =
      DirectionAngleDegrees(direction) + kRocketSpriteAngleOffsetDegrees;
  const int normalized_frame =
      ((frame_index % kRocketFlightFrames) + kRocketFlightFrames) %
      kRocketFlightFrames;
  if (normalized_frame < static_cast<int>(sdl_rocket_flight_textures.size()) &&
      normalized_frame < static_cast<int>(sdl_rocket_flight_sizes.size()) &&
      sdl_rocket_flight_textures[static_cast<size_t>(normalized_frame)] != nullptr &&
      sdl_rocket_flight_sizes[static_cast<size_t>(normalized_frame)].x > 0 &&
      sdl_rocket_flight_sizes[static_cast<size_t>(normalized_frame)].y > 0) {
    SDL_Texture *frame_texture =
        sdl_rocket_flight_textures[static_cast<size_t>(normalized_frame)];
    const SDL_Point frame_size =
        sdl_rocket_flight_sizes[static_cast<size_t>(normalized_frame)];
    const int dominant_dimension = std::max(frame_size.x, frame_size.y);
    const double scale =
        static_cast<double>(max_dimension) / static_cast<double>(dominant_dimension);
    const float draw_width = static_cast<float>(
        std::max(1, static_cast<int>(std::lround(frame_size.x * scale))));
    const float draw_height = static_cast<float>(
        std::max(1, static_cast<int>(std::lround(frame_size.y * scale))));

    SDL_SetTextureBlendMode(frame_texture, SDL_BLENDMODE_BLEND);
    SDL_FRect draw_rect{center.x - draw_width / 2.0f,
                        center.y - draw_height / 2.0f, draw_width,
                        draw_height};
    SDL_SetTextureColorMod(frame_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame_texture, alpha);
    SDL_RenderCopyExF(sdl_renderer, frame_texture, nullptr, &draw_rect,
                      angle_degrees, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(frame_texture, 255);
    return;
  }

  if (sdl_rocket_texture != nullptr && sdl_rocket_size.x > 0 &&
      sdl_rocket_size.y > 0) {
    const int dominant_dimension =
        std::max(sdl_rocket_size.x, sdl_rocket_size.y);
    const double scale =
        static_cast<double>(max_dimension) / static_cast<double>(dominant_dimension);
    const float draw_width = static_cast<float>(
        std::max(1, static_cast<int>(std::lround(sdl_rocket_size.x * scale))));
    const float draw_height = static_cast<float>(
        std::max(1, static_cast<int>(std::lround(sdl_rocket_size.y * scale))));
    SDL_FRect draw_rect{center.x - draw_width / 2.0f,
                        center.y - draw_height / 2.0f, draw_width,
                        draw_height};
    SDL_SetTextureBlendMode(sdl_rocket_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(sdl_rocket_texture, alpha);
    SDL_RenderCopyExF(sdl_renderer, sdl_rocket_texture, nullptr, &draw_rect,
                      angle_degrees + 90.0, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(sdl_rocket_texture, 255);
    return;
  }
}

void Renderer::drawAirstrikePlane(const SDL_FPoint &center,
                                  double angle_degrees, int max_dimension,
                                  double wobble_phase) {
  if (max_dimension <= 0) {
    return;
  }

  const double travel_angle_radians =
      (angle_degrees - 90.0) * 3.14159265358979323846 / 180.0;
  const float wobble_distance =
      static_cast<float>(std::sin(wobble_phase) * std::max(2.0, max_dimension * 0.035));
  const SDL_FPoint wobble_offset{
      static_cast<float>(-std::sin(travel_angle_radians) * wobble_distance),
      static_cast<float>(std::cos(travel_angle_radians) * wobble_distance)};
  const SDL_FPoint draw_center{center.x + wobble_offset.x,
                               center.y + wobble_offset.y};

  if (sdl_airstrike_plane_texture != nullptr && sdl_airstrike_plane_size.x > 0 &&
      sdl_airstrike_plane_size.y > 0) {
    const int dominant_dimension =
        std::max(sdl_airstrike_plane_size.x, sdl_airstrike_plane_size.y);
    const double scale =
        static_cast<double>(max_dimension) / static_cast<double>(dominant_dimension);
    const float draw_width = static_cast<float>(
        std::max(1, static_cast<int>(std::lround(sdl_airstrike_plane_size.x * scale))));
    const float draw_height = static_cast<float>(
        std::max(1, static_cast<int>(std::lround(sdl_airstrike_plane_size.y * scale))));

    SDL_FRect shadow_rect{draw_center.x - draw_width / 2.0f + 7.0f,
                          draw_center.y - draw_height / 2.0f + 9.0f, draw_width,
                          draw_height};
    SDL_SetTextureBlendMode(sdl_airstrike_plane_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(sdl_airstrike_plane_texture, 0, 0, 0);
    SDL_SetTextureAlphaMod(sdl_airstrike_plane_texture, 96);
    SDL_RenderCopyExF(sdl_renderer, sdl_airstrike_plane_texture, nullptr,
                      &shadow_rect, angle_degrees, nullptr, SDL_FLIP_NONE);

    SDL_FRect plane_rect{draw_center.x - draw_width / 2.0f,
                         draw_center.y - draw_height / 2.0f, draw_width,
                         draw_height};
    SDL_SetTextureColorMod(sdl_airstrike_plane_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(sdl_airstrike_plane_texture, 255);
    SDL_RenderCopyExF(sdl_renderer, sdl_airstrike_plane_texture, nullptr,
                      &plane_rect, angle_degrees, nullptr, SDL_FLIP_NONE);
    return;
  }

  const int body_length = std::max(18, max_dimension);
  const int body_width = std::max(10, max_dimension / 3);
  SDL_FRect fallback_rect{draw_center.x - body_width / 2.0f,
                          draw_center.y - body_length / 2.0f,
                          static_cast<float>(body_width),
                          static_cast<float>(body_length)};
  SDL_SetRenderDrawColor(sdl_renderer, 160, 170, 182, 240);
  SDL_RenderFillRectF(sdl_renderer, &fallback_rect);
}

void Renderer::drawDynamiteIcon(const SDL_Rect &icon_rect, bool lit_fuse,
                                double animation_clock, Uint8 alpha,
                                double fuse_burn_progress) {
  if (icon_rect.w <= 0 || icon_rect.h <= 0) {
    return;
  }

  SDL_Rect draw_rect{icon_rect.x, icon_rect.y, icon_rect.w, icon_rect.h};
  const bool can_use_texture =
      sdl_dynamite_texture != nullptr && sdl_dynamite_size.x > 0 &&
      sdl_dynamite_size.y > 0;
  if (can_use_texture) {
    const double scale_by_width =
        static_cast<double>(icon_rect.w) / static_cast<double>(sdl_dynamite_size.x);
    const double scale_by_height =
        static_cast<double>(icon_rect.h) / static_cast<double>(sdl_dynamite_size.y);
    const double scale = std::min(scale_by_width, scale_by_height);
    draw_rect.w = std::max(
        1, static_cast<int>(std::lround(sdl_dynamite_size.x * scale)));
    draw_rect.h = std::max(
        1, static_cast<int>(std::lround(sdl_dynamite_size.y * scale)));
    draw_rect.x = icon_rect.x + (icon_rect.w - draw_rect.w) / 2;
    draw_rect.y = icon_rect.y + (icon_rect.h - draw_rect.h) / 2;

    SDL_SetTextureBlendMode(sdl_dynamite_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(sdl_dynamite_texture, alpha);
    SDL_RenderCopy(sdl_renderer, sdl_dynamite_texture, nullptr, &draw_rect);
    SDL_SetTextureAlphaMod(sdl_dynamite_texture, 255);
  } else {
    const int body_height = std::max(5, icon_rect.h / 3);
    const int body_y = icon_rect.y + icon_rect.h / 2 - body_height / 2;
    const SDL_Rect body_rect{
        icon_rect.x + std::max(1, icon_rect.w / 10), body_y,
        std::max(6, icon_rect.w - std::max(2, icon_rect.w / 5)), body_height};
    const int cap_radius = std::max(2, body_rect.h / 2);
    const int center_y = body_rect.y + body_rect.h / 2;

    SDL_SetRenderDrawColor(sdl_renderer, 122, 18, 12, alpha);
    SDL_RenderFillCircle(sdl_renderer, body_rect.x, center_y, cap_radius);
    SDL_RenderFillCircle(sdl_renderer, body_rect.x + body_rect.w, center_y,
                         cap_radius);
    SDL_SetRenderDrawColor(sdl_renderer, 186, 40, 28, alpha);
    SDL_RenderFillRect(sdl_renderer, &body_rect);
    SDL_SetRenderDrawColor(sdl_renderer, 242, 130, 76, alpha);
    SDL_RenderDrawLine(sdl_renderer, body_rect.x + 1, body_rect.y + 1,
                       body_rect.x + body_rect.w - 1, body_rect.y + 1);
    SDL_SetRenderDrawColor(sdl_renderer, 88, 10, 8, alpha);
    SDL_RenderDrawRect(sdl_renderer, &body_rect);

    const int band_width = std::max(2, body_rect.w / 8);
    for (int band = 0; band < 2; band++) {
      const int band_x = body_rect.x + body_rect.w / 3 + band * body_rect.w / 4;
      SDL_Rect band_rect{band_x, body_rect.y - 1, band_width, body_rect.h + 2};
      SDL_SetRenderDrawColor(sdl_renderer, 236, 220, 174, alpha);
      SDL_RenderFillRect(sdl_renderer, &band_rect);
    }
  }

  if (!lit_fuse) {
    return;
  }

  (void)fuse_burn_progress;

  constexpr SDL_FPoint kFuseTipNorm{0.94f, 0.15f};
  const int tip_x = draw_rect.x +
                    static_cast<int>(std::lround(kFuseTipNorm.x * draw_rect.w));
  const int tip_y = draw_rect.y +
                    static_cast<int>(std::lround(kFuseTipNorm.y * draw_rect.h));

  const double flicker_a = std::sin(animation_clock * 7.3);
  const double flicker_b = std::sin(animation_clock * 11.7 + 1.3);
  const double flicker_c = std::sin(animation_clock * 4.1 + 2.7);
  const double flicker = 0.55 + 0.30 * flicker_a + 0.10 * flicker_b +
                         0.05 * flicker_c;
  const double clamped_flicker = std::clamp(flicker, 0.20, 1.00);

  const int base_radius =
      std::max(2, static_cast<int>(std::max(draw_rect.w, draw_rect.h) * 0.05));
  const int outer_radius = std::max(
      base_radius + 1,
      static_cast<int>(base_radius * (1.6 + 0.6 * clamped_flicker)));
  const int mid_radius = std::max(
      base_radius,
      static_cast<int>(base_radius * (1.1 + 0.5 * clamped_flicker)));
  const int core_radius = std::max(
      1, static_cast<int>(base_radius * (0.55 + 0.35 * clamped_flicker)));

  const int jitter_x =
      static_cast<int>(std::lround(flicker_b * base_radius * 0.35));
  const int jitter_y =
      static_cast<int>(std::lround(-std::abs(flicker_c) * base_radius * 0.45));
  const int flame_x = tip_x + jitter_x;
  const int flame_y = tip_y + jitter_y;

  const Uint8 glow_alpha = static_cast<Uint8>(std::clamp(
      static_cast<double>(alpha) * (0.35 + 0.30 * clamped_flicker), 0.0, 220.0));
  const Uint8 mid_alpha = static_cast<Uint8>(std::clamp(
      static_cast<double>(alpha) * (0.65 + 0.30 * clamped_flicker), 0.0, 255.0));
  const Uint8 core_alpha = static_cast<Uint8>(std::clamp(
      static_cast<double>(alpha) * (0.85 + 0.15 * clamped_flicker), 0.0, 255.0));

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  SDL_SetRenderDrawColor(sdl_renderer, 255, 132, 40, glow_alpha);
  SDL_RenderFillCircle(sdl_renderer, flame_x, flame_y, outer_radius);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 184, 72, mid_alpha);
  SDL_RenderFillCircle(sdl_renderer, flame_x, flame_y - base_radius / 3,
                       mid_radius);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 240, 168, core_alpha);
  SDL_RenderFillCircle(sdl_renderer, flame_x, flame_y - base_radius / 2,
                       core_radius);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 252, 220, core_alpha);
  SDL_RenderFillCircle(sdl_renderer, flame_x, flame_y - base_radius / 2,
                       std::max(1, core_radius - 1));

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawOrganicGasCloud(int center_x, int center_y, int base_size,
                                   double animation_clock,
                                   double alpha_factor, int animation_seed,
                                   double scale_multiplier) {
  if (base_size <= 0 || alpha_factor <= 0.0) {
    return;
  }
  base_size = std::max(
      1, static_cast<int>(std::lround(base_size * GAS_CLOUD_SPREAD_SCALE)));
  alpha_factor *= GAS_CLOUD_OPACITY_SCALE;

  constexpr double kPi = 3.14159265358979323846;
  const std::array<SDL_Color, 5> palette{{
      SDL_Color{64, 154, 78, 255},
      SDL_Color{82, 178, 86, 255},
      SDL_Color{102, 196, 102, 255},
      SDL_Color{126, 214, 126, 255},
      SDL_Color{160, 226, 150, 255},
  }};

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const int haze_radius = std::max(
      4, static_cast<int>(std::lround(base_size * (0.82 + 0.10 * scale_multiplier))));
  SDL_SetRenderDrawColor(
      sdl_renderer, 72, 156, 76,
      static_cast<Uint8>(std::clamp(46.0 * alpha_factor, 0.0, 120.0)));
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y, haze_radius + 3);
  SDL_SetRenderDrawColor(
      sdl_renderer, 118, 208, 116,
      static_cast<Uint8>(std::clamp(22.0 * alpha_factor, 0.0, 80.0)));
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y - haze_radius / 3,
                       haze_radius + 1);

  const int minimum_particle_count =
      std::max(1, std::min(GAS_CLOUD_MIN_PARTICLE_COUNT,
                           GAS_CLOUD_MAX_PARTICLE_COUNT));
  const int maximum_particle_count =
      std::max(minimum_particle_count,
               std::max(GAS_CLOUD_MIN_PARTICLE_COUNT,
                        GAS_CLOUD_MAX_PARTICLE_COUNT));
  const int particle_range =
      maximum_particle_count - minimum_particle_count + 1;
  const int particle_count =
      minimum_particle_count +
      std::abs(animation_seed * 17 + 11) % particle_range;

  for (int puff = 0; puff < particle_count; ++puff) {
    const int seed = animation_seed * 97 + puff * 31;
    const double phase =
        animation_clock * (0.58 + 0.18 * HashUnit(seed + 1)) +
        HashUnit(seed + 2) * 2.0 * kPi;
    const double wobble =
        0.5 + 0.5 *
                  std::sin(animation_clock * (1.24 + 0.22 * HashUnit(seed + 3)) +
                           HashUnit(seed + 4) * 2.0 * kPi);
    const double orbit =
        base_size * scale_multiplier * (0.18 + 0.18 * HashUnit(seed + 5));
    const int offset_x = static_cast<int>(std::lround(
        std::cos(phase) * orbit * (0.76 + 0.34 * wobble)));
    const int offset_y = static_cast<int>(std::lround(
        std::sin(phase * 1.19 + HashSigned(seed + 6) * 0.45) * orbit *
            (0.52 + 0.20 * wobble) -
        base_size * scale_multiplier * (0.06 + 0.07 * wobble)));
    const int puff_radius =
        std::max(3, static_cast<int>(std::lround(
                        base_size * scale_multiplier *
                        (0.26 + 0.16 * HashUnit(seed + 7) + 0.16 * wobble))));
    const SDL_Color color = palette[static_cast<size_t>(puff) % palette.size()];
    const Uint8 puff_alpha = static_cast<Uint8>(std::clamp(
        (56.0 + 18.0 * puff + 54.0 * wobble) * alpha_factor, 0.0, 186.0));
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, puff_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x + offset_x, center_y + offset_y,
                         puff_radius);

    const int highlight_radius = std::max(2, puff_radius / 2);
    SDL_SetRenderDrawColor(
        sdl_renderer, static_cast<Uint8>(std::min(255, color.r + 26)),
        static_cast<Uint8>(std::min(255, color.g + 24)),
        static_cast<Uint8>(std::min(255, color.b + 18)),
        static_cast<Uint8>(std::clamp(puff_alpha * 0.46, 0.0, 120.0)));
    SDL_RenderFillCircle(sdl_renderer, center_x + offset_x + puff_radius / 4,
                         center_y + offset_y - puff_radius / 3,
                         highlight_radius);
  }

  const int wisp_count = std::clamp(2 + particle_count / 3, 3, 6);
  for (int wisp = 0; wisp < wisp_count; ++wisp) {
    const int seed = animation_seed * 149 + wisp * 43;
    const double angle =
        animation_clock * (0.92 + 0.14 * HashUnit(seed + 1)) +
        HashUnit(seed + 2) * 2.0 * kPi;
    const double distance =
        base_size * scale_multiplier * (0.44 + 0.16 * HashUnit(seed + 3));
    const int x1 =
        center_x + static_cast<int>(std::lround(std::cos(angle) * distance * 0.4));
    const int y1 = center_y - base_size / 6 +
                   static_cast<int>(std::lround(std::sin(angle * 1.13) *
                                                distance * 0.3));
    const int x2 = x1 + static_cast<int>(std::lround(std::cos(angle) * distance));
    const int y2 = y1 - static_cast<int>(std::lround(distance * 0.35));
    SDL_SetRenderDrawColor(
        sdl_renderer, 174, 230, 166,
        static_cast<Uint8>(std::clamp(38.0 * alpha_factor, 0.0, 90.0)));
    SDL_RenderDrawLine(sdl_renderer, x1, y1, x2, y2);
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawPacmanGasCloudlets(const SDL_Rect &anchor_rect,
                                      double alpha_factor, Uint32 now) {
  if (anchor_rect.w <= 0 || anchor_rect.h <= 0 || alpha_factor <= 0.0) {
    return;
  }

  constexpr double kPi = 3.14159265358979323846;
  const int center_x = anchor_rect.x + anchor_rect.w / 2;
  const int center_y = anchor_rect.y + anchor_rect.h / 2;
  const int orbit_radius =
      std::max(anchor_rect.w, anchor_rect.h) / 2 + std::max(6, element_size / 10);
  const double aura_clock = static_cast<double>(now) / 230.0;

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(
      sdl_renderer, 78, 176, 82,
      static_cast<Uint8>(std::clamp(26.0 * alpha_factor, 0.0, 80.0)));
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(10, anchor_rect.w / 2 + 6));
  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);

  for (int cloudlet = 0; cloudlet < 6; ++cloudlet) {
    const double angle = aura_clock * 0.72 +
                         cloudlet * (2.0 * kPi / 6.0) +
                         HashSigned(300 + cloudlet * 19) * 0.34;
    const double radial_pulse =
        0.84 + 0.16 * std::sin(aura_clock * 1.3 + cloudlet * 0.9);
    const int cloudlet_x =
        center_x + static_cast<int>(std::lround(std::cos(angle) * orbit_radius *
                                                radial_pulse));
    const int cloudlet_y =
        center_y - anchor_rect.h / 6 +
        static_cast<int>(std::lround(std::sin(angle * 1.18) *
                                     orbit_radius * 0.46));
    const int base_size =
        std::max(4, static_cast<int>(std::lround(
                        element_size * (0.11 + 0.02 * (cloudlet % 3)))));
    drawOrganicGasCloud(cloudlet_x, cloudlet_y, base_size,
                        aura_clock + cloudlet * 0.78, alpha_factor * 0.92,
                        700 + cloudlet * 41, 0.86);
  }
}

void Renderer::drawPlasticExplosiveIcon(const SDL_Rect &icon_rect,
                                        double animation_clock, Uint8 alpha,
                                        bool armed) {
  if (icon_rect.w <= 0 || icon_rect.h <= 0) {
    return;
  }

  const SDL_Color body_color =
      armed ? SDL_Color{204, 56, 44, alpha} : SDL_Color{178, 64, 44, alpha};
  const SDL_Color band_color =
      armed ? SDL_Color{46, 42, 40, alpha} : SDL_Color{56, 52, 48, alpha};
  const SDL_Color cap_color =
      armed ? SDL_Color{255, 238, 194, alpha} : SDL_Color{230, 222, 184, alpha};
  const SDL_Color led_color =
      armed ? SDL_Color{255, 86, 66, alpha} : SDL_Color{84, 220, 255, alpha};
  const double pulse = 0.64 + 0.36 * std::sin(animation_clock * (armed ? 2.6 : 1.7));

  SDL_Rect body_rect{icon_rect.x + std::max(1, icon_rect.w / 8),
                     icon_rect.y + std::max(2, icon_rect.h / 4),
                     std::max(6, icon_rect.w * 3 / 4),
                     std::max(6, icon_rect.h / 2)};
  SDL_SetRenderDrawColor(sdl_renderer, body_color.r, body_color.g, body_color.b,
                         static_cast<Uint8>(alpha));
  SDL_RenderFillRect(sdl_renderer, &body_rect);

  SDL_SetRenderDrawColor(sdl_renderer, cap_color.r, cap_color.g, cap_color.b,
                         alpha);
  SDL_RenderDrawRect(sdl_renderer, &body_rect);

  const int strap_width = std::max(2, body_rect.w / 6);
  SDL_Rect left_strap{body_rect.x + strap_width, body_rect.y, strap_width,
                      body_rect.h};
  SDL_Rect right_strap{body_rect.x + body_rect.w - strap_width * 2, body_rect.y,
                       strap_width, body_rect.h};
  SDL_SetRenderDrawColor(sdl_renderer, band_color.r, band_color.g, band_color.b,
                         static_cast<Uint8>(alpha));
  SDL_RenderFillRect(sdl_renderer, &left_strap);
  SDL_RenderFillRect(sdl_renderer, &right_strap);

  const int wire_start_x = body_rect.x + body_rect.w - 1;
  const int wire_start_y = body_rect.y + std::max(2, body_rect.h / 4);
  const int wire_mid_x = icon_rect.x + icon_rect.w - std::max(2, icon_rect.w / 7);
  const int wire_mid_y = icon_rect.y + std::max(2, icon_rect.h / 5);
  SDL_SetRenderDrawColor(sdl_renderer, 226, 216, 184, alpha);
  SDL_RenderDrawLine(sdl_renderer, wire_start_x, wire_start_y, wire_mid_x,
                     wire_mid_y);
  SDL_RenderDrawLine(sdl_renderer, wire_mid_x, wire_mid_y,
                     wire_mid_x + std::max(1, icon_rect.w / 10),
                     wire_mid_y - std::max(2, icon_rect.h / 7));

  const int led_radius = std::max(2, icon_rect.w / 8);
  const int led_center_x = body_rect.x + body_rect.w / 2;
  const int led_center_y = body_rect.y + body_rect.h / 2;
  SDL_SetRenderDrawColor(
      sdl_renderer, led_color.r, led_color.g, led_color.b,
      static_cast<Uint8>(std::clamp(static_cast<double>(alpha) *
                                        (armed ? (0.72 + pulse * 0.28) : 0.84),
                                    0.0, 255.0)));
  SDL_RenderFillCircle(sdl_renderer, led_center_x, led_center_y, led_radius);
}

void Renderer::drawdynamitepickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &dynamite = game->dynamite_pickup;
  if (!dynamite.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (dynamite.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - dynamite.fade_started_ticks) /
                             static_cast<double>(DYNAMITE_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(dynamite.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + dynamite.animation_seed * 41) / 180.0;
  const double pulse = 0.80 + 0.20 * std::sin(animation_clock);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(96.0 * alpha_factor, 0.0, 140.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 255, 118, 24, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(7, static_cast<int>(element_size * 0.28 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 255, 202, 110, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(9, static_cast<int>(element_size * 0.38 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.74));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawDynamiteIcon(icon_rect, false, animation_clock, body_alpha, 0.0);
}

void Renderer::drawplasticexplosivepickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &plastic_explosive = game->plastic_explosive_pickup;
  if (!plastic_explosive.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (plastic_explosive.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - plastic_explosive.fade_started_ticks) /
                             static_cast<double>(PLASTIC_EXPLOSIVE_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(plastic_explosive.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + plastic_explosive.animation_seed * 37) / 170.0;
  const double pulse = 0.78 + 0.22 * std::sin(animation_clock);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(88.0 * alpha_factor, 0.0, 136.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 84, 214, 255, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(7, static_cast<int>(element_size * 0.26 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 190, 244, 255, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(9, static_cast<int>(element_size * 0.36 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.74));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawPlasticExplosiveIcon(icon_rect, animation_clock, body_alpha, false);
}

void Renderer::drawwalkietalkiepickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &walkie = game->walkie_talkie_pickup;
  if (!walkie.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (walkie.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - walkie.fade_started_ticks) /
                             static_cast<double>(WALKIE_TALKIE_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(walkie.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + walkie.animation_seed * 29) / 210.0;
  const double pulse = 0.82 + 0.18 * std::sin(animation_clock);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(104.0 * alpha_factor, 0.0, 150.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 80, 255, 116, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(7, static_cast<int>(element_size * 0.28 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 194, 255, 214, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(10, static_cast<int>(element_size * 0.40 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.76));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawWalkieTalkieIcon(icon_rect, body_alpha, animation_clock);
}

void Renderer::drawrocketpickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &rocket = game->rocket_pickup;
  if (!rocket.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (rocket.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - rocket.fade_started_ticks) /
                             static_cast<double>(ROCKET_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(rocket.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + rocket.animation_seed * 31) / 190.0;
  const double pulse = 0.84 + 0.16 * std::sin(animation_clock);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(118.0 * alpha_factor, 0.0, 156.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 255, 112, 46, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(8, static_cast<int>(element_size * 0.29 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 255, 210, 124, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(10, static_cast<int>(element_size * 0.40 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.82));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawRocketIcon(icon_rect, body_alpha, animation_clock);
}

void Renderer::drawbiohazardpickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &biohazard = game->biohazard_pickup;
  if (!biohazard.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (biohazard.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - biohazard.fade_started_ticks) /
                             static_cast<double>(BIOHAZARD_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(biohazard.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + biohazard.animation_seed * 43) / 170.0;
  const double pulse = 0.88 + 0.12 * std::sin(animation_clock * 1.2);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(114.0 * alpha_factor, 0.0, 160.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 255, 64, 64, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(8, static_cast<int>(element_size * 0.31 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 255, 180, 74, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(10, static_cast<int>(element_size * 0.42 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.82));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawBiohazardIcon(icon_rect, body_alpha, animation_clock,
                    static_cast<double>(now + biohazard.animation_seed * 19) * 0.11,
                    0.96 + 0.08 * std::sin(animation_clock * 1.2));
}

void Renderer::drawnuclearbombpickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &bomb = game->nuclear_bomb_pickup;
  if (!bomb.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (bomb.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - bomb.fade_started_ticks) /
                             static_cast<double>(NUCLEAR_BOMB_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(bomb.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + bomb.animation_seed * 47) / 180.0;
  const double pulse = 0.84 + 0.16 * std::sin(animation_clock * 1.2);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(132.0 * alpha_factor, 0.0, 178.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 244, 210, 36, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(8, static_cast<int>(element_size * 0.33 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 255, 238, 118, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(10, static_cast<int>(element_size * 0.45 * pulse)));

  const int icon_size = std::max(14, static_cast<int>(element_size * 0.92));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawNuclearBombIcon(icon_rect, body_alpha,
                      4.0 * std::sin(animation_clock * 0.7));
}

void Renderer::drawlovepotionpickup() {
  if (game == nullptr || game->dead) {
    return;
  }

  const auto &potion = game->love_potion_pickup;
  if (!potion.is_visible) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  double alpha_factor = 1.0;
  if (potion.is_fading) {
    alpha_factor =
        1.0 - std::clamp(static_cast<double>(now - potion.fade_started_ticks) /
                             static_cast<double>(LOVE_POTION_FADE_MS),
                         0.0, 1.0);
  }
  if (alpha_factor <= 0.0) {
    return;
  }

  const PixelCoord pickup_px = getPixelCoord(potion.coord, 0, 0);
  const int center_x = pickup_px.x + element_size / 2;
  const int center_y = pickup_px.y + element_size / 2;
  const double animation_clock =
      static_cast<double>(now + potion.animation_seed * 53) / 160.0;
  const double pulse = 0.84 + 0.16 * std::sin(animation_clock * 1.35);
  const Uint8 glow_alpha = static_cast<Uint8>(
      std::clamp(132.0 * alpha_factor, 0.0, 180.0));
  const Uint8 body_alpha = static_cast<Uint8>(
      std::clamp(255.0 * alpha_factor, 0.0, 255.0));

  SDL_SetRenderDrawColor(sdl_renderer, 255, 30, 54, glow_alpha / 2);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(8, static_cast<int>(element_size * 0.32 * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 255, 150, 160, glow_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(10, static_cast<int>(element_size * 0.43 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.82));
  const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                           icon_size, icon_size};
  drawLovePotionIcon(icon_rect, body_alpha, animation_clock);
}

void Renderer::drawNuclearBombTargetMarker() {
  if (game == nullptr || game->dead ||
      !game->nuclear_bomb_target_marker.is_marked) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const auto &marker = game->nuclear_bomb_target_marker;
  const double animation_clock =
      static_cast<double>(now + marker.animation_seed * 31) / 180.0;
  const double pulse = 0.94 + 0.12 * std::sin(animation_clock);
  const Uint8 alpha = static_cast<Uint8>(
      std::clamp(168.0 + 64.0 * (0.5 + 0.5 * std::sin(animation_clock * 1.25)),
                 0.0, 255.0));

  if (ENABLE_3D_VIEW) {
    const SDL_Rect marker_rect =
        makeFloorAlignedRect(marker.coord.v, marker.coord.u, 0.10, 0.10,
                             0.80, 0.80);
    drawNuclearTargetMarkerIcon(marker_rect, alpha, pulse);
    return;
  }

  const PixelCoord marker_px = getPixelCoord(marker.coord, 0, 0);
  const int marker_size = std::max(12, static_cast<int>(element_size * 0.82));
  const SDL_Rect marker_rect{
      marker_px.x + (element_size - marker_size) / 2,
      marker_px.y + (element_size - marker_size) / 2,
      marker_size,
      marker_size};
  drawNuclearTargetMarkerIcon(marker_rect, alpha, pulse);
}

void Renderer::drawActiveNuclearBombDrop() {
  if (game == nullptr || !game->active_nuclear_bomb_drop.is_active) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const auto &drop = game->active_nuclear_bomb_drop;
  if (now < drop.drop_started_ticks) {
    return;
  }

  const double progress =
      std::clamp(static_cast<double>(now - drop.drop_started_ticks) /
                     static_cast<double>(std::max<Uint32>(1, NUCLEAR_BOMB_FALL_MS)),
                 0.0, 1.0);
  const double eased_progress = 1.0 - std::pow(1.0 - progress, 2.0);
  const double size_cells =
      static_cast<double>(NUCLEAR_BOMB_INITIAL_RENDER_SCALE_CELLS) *
          (1.0 - eased_progress) +
      static_cast<double>(NUCLEAR_BOMB_FINAL_RENDER_SCALE_CELLS) *
          eased_progress;
  const int bomb_size =
      std::max(12, static_cast<int>(std::lround(size_cells * element_size)));

  SDL_FPoint screen_center{0.0f, 0.0f};
  if (ENABLE_3D_VIEW) {
    const double z_cells = 4.2 * (1.0 - progress) + 0.18;
    screen_center = projectScene(drop.target_world_center.x,
                                 drop.target_world_center.y, z_cells);
  } else {
    const double tile_span = static_cast<double>(element_size) + 1.0;
    screen_center = SDL_FPoint{
        static_cast<float>(offset_x + 1 +
                           drop.target_world_center.x * tile_span),
        static_cast<float>(offset_y + 1 +
                           drop.target_world_center.y * tile_span -
                           element_size * (2.7 * (1.0 - progress) + 0.15))};
  }

  const int shadow_radius =
      std::max(6, static_cast<int>(element_size * (0.20 + progress * 0.36)));
  const SDL_FPoint target_screen = ENABLE_3D_VIEW
                                      ? projectScene(drop.target_world_center.x,
                                                     drop.target_world_center.y,
                                                     0.02)
                                      : SDL_FPoint{
                                            static_cast<float>(
                                                offset_x + 1 +
                                                drop.target_world_center.x *
                                                    (element_size + 1.0)),
                                            static_cast<float>(
                                                offset_y + 1 +
                                                drop.target_world_center.y *
                                                    (element_size + 1.0))};
  SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0,
                         static_cast<Uint8>(std::clamp(44.0 + progress * 72.0,
                                                       0.0, 132.0)));
  SDL_RenderFillEllipse(sdl_renderer, static_cast<int>(std::lround(target_screen.x)),
                        static_cast<int>(std::lround(target_screen.y)),
                        shadow_radius * 2, shadow_radius);

  const SDL_Rect bomb_rect{
      static_cast<int>(std::lround(screen_center.x - bomb_size / 2.0)),
      static_cast<int>(std::lround(screen_center.y - bomb_size / 2.0)),
      bomb_size,
      bomb_size};
  const double sway =
      std::sin((static_cast<double>(now) + drop.animation_seed * 23.0) / 140.0);
  drawNuclearBombIcon(bomb_rect, 255, sway * 4.0);
}

void Renderer::drawplaceddynamites() {
  if (game == nullptr) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  for (const auto &placed_dynamite : game->placed_dynamites) {
    const PixelCoord placed_px = getPixelCoord(placed_dynamite.coord, 0, 0);
    const int center_x = placed_px.x + element_size / 2;
    const int center_y = placed_px.y + element_size / 2;
    const Uint32 remaining_ms =
        (placed_dynamite.explode_at_ticks > now)
            ? (placed_dynamite.explode_at_ticks - now)
            : 0;
    const double countdown_progress =
        std::clamp(static_cast<double>(remaining_ms) /
                       static_cast<double>(
                           std::max<Uint32>(1, DYNAMITE_COUNTDOWN_MS)),
                   0.0, 1.0);
    const double urgency = 1.0 - countdown_progress;
    const double pulse_clock =
        static_cast<double>(now + placed_dynamite.animation_seed * 29) / 130.0;
    const double pulse =
        0.88 + (0.12 + urgency * 0.22) * std::sin(pulse_clock);

    SDL_SetRenderDrawColor(
        sdl_renderer, 255, 88, 32,
        static_cast<Uint8>(std::clamp(72.0 + urgency * 90.0, 0.0, 190.0)));
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                         std::max(6, static_cast<int>(element_size * 0.24 * pulse)));

    const int icon_size = std::max(12, static_cast<int>(element_size * 0.74));
    const SDL_Rect icon_rect{center_x - icon_size / 2, center_y - icon_size / 2,
                             icon_size, icon_size};
    drawDynamiteIcon(icon_rect, true, pulse_clock, 255, urgency);

    const int remaining_seconds =
        static_cast<int>((std::max<Uint32>(1, remaining_ms) + 999) / 1000);
    renderSimpleText(sdl_font_hud, std::to_string(remaining_seconds),
                     SDL_Color{255, 238, 206, 255}, center_x,
                     placed_px.y - TTF_FontHeight(sdl_font_hud) / 3);
  }
}

void Renderer::drawplacedplasticexplosive() {
  if (game == nullptr || !game->plastic_explosive_is_armed) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const auto &placed_plastic_explosive = game->placed_plastic_explosive;
  const PixelCoord placed_px = getPixelCoord(placed_plastic_explosive.coord, 0, 0);
  const int center_x = placed_px.x + element_size / 2;
  const int center_y = placed_px.y + element_size / 2;
  const double pulse_clock =
      static_cast<double>(now + placed_plastic_explosive.animation_seed * 31) /
      130.0;
  const double pulse = 0.84 + 0.28 * std::sin(pulse_clock);

  SDL_SetRenderDrawColor(
      sdl_renderer, 255, 72, 52,
      static_cast<Uint8>(std::clamp(92.0 + pulse * 72.0, 0.0, 188.0)));
  SDL_RenderFillCircle(
      sdl_renderer, center_x, center_y + element_size / 8,
      std::max(6, static_cast<int>(element_size * 0.22 * pulse)));

  const int icon_size = std::max(12, static_cast<int>(element_size * 0.76));
  const SDL_Rect icon_rect{center_x - icon_size / 2,
                           center_y - icon_size / 2 - element_size / 12,
                           icon_size, icon_size};
  drawPlasticExplosiveIcon(icon_rect, pulse_clock, 255, true);
}

void Renderer::drawactiveairstrike() {
  if (game == nullptr || !game->active_airstrike.is_active) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const auto &airstrike = game->active_airstrike;
  auto world_to_screen = [&](const SDL_FPoint &point) {
    return SDL_FPoint{static_cast<float>(offset_x) + point.x * element_size,
                      static_cast<float>(offset_y) + point.y * element_size};
  };

  if (airstrike.plane_finished_ticks > airstrike.plane_launch_ticks &&
      now >= airstrike.plane_launch_ticks &&
      now <= airstrike.plane_finished_ticks) {
    const double progress = std::clamp(
        static_cast<double>(now - airstrike.plane_launch_ticks) /
            static_cast<double>(airstrike.plane_finished_ticks -
                                airstrike.plane_launch_ticks),
        0.0, 1.0);
    const SDL_FPoint plane_world{
        airstrike.flight_start.x +
            static_cast<float>((airstrike.flight_end.x - airstrike.flight_start.x) *
                               progress),
        airstrike.flight_start.y +
            static_cast<float>((airstrike.flight_end.y - airstrike.flight_start.y) *
                               progress)};
    const SDL_FPoint plane_center = world_to_screen(plane_world);
    const double angle_degrees =
        std::atan2(static_cast<double>(airstrike.flight_end.y -
                                       airstrike.flight_start.y),
                   static_cast<double>(airstrike.flight_end.x -
                                       airstrike.flight_start.x)) *
            180.0 / 3.14159265358979323846 +
        90.0;
    const int plane_size = std::max(40, element_size * 5);
    drawAirstrikePlane(
        plane_center, angle_degrees, plane_size,
        static_cast<double>(now + airstrike.animation_seed * 19) / 140.0);
  }

  for (const auto &bomb : airstrike.bombs) {
    if (bomb.exploded || now < bomb.release_ticks || now >= bomb.explode_ticks) {
      continue;
    }

    const double progress = std::clamp(
        static_cast<double>(now - bomb.release_ticks) /
            static_cast<double>(std::max<Uint32>(1, bomb.explode_ticks -
                                                        bomb.release_ticks)),
        0.0, 1.0);
    const SDL_FPoint start_center = world_to_screen(bomb.release_position);
    const PixelCoord bomb_px = getPixelCoord(bomb.coord, 0, 0);
    const SDL_FPoint target_center{
        static_cast<float>(bomb_px.x + element_size / 2),
        static_cast<float>(bomb_px.y + element_size / 2)};
    const int bomb_center_x = static_cast<int>(std::lround(
        start_center.x + (target_center.x - start_center.x) * progress));
    const int bomb_center_y = static_cast<int>(std::lround(
        start_center.y + (target_center.y - start_center.y) * progress));
    const int bomb_target_y = static_cast<int>(std::lround(target_center.y));
    const int bomb_radius =
        std::max(3, static_cast<int>(element_size * (0.09 + 0.05 * (1.0 - progress))));
    const Uint8 shadow_alpha = static_cast<Uint8>(
        std::clamp(80.0 * (0.35 + progress * 0.65), 0.0, 120.0));

    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, shadow_alpha);
    SDL_RenderFillCircle(sdl_renderer, bomb_center_x, bomb_target_y + element_size / 5,
                         std::max(4, bomb_radius + 1));
    SDL_SetRenderDrawColor(sdl_renderer, 48, 48, 54, 255);
    SDL_RenderFillCircle(sdl_renderer, bomb_center_x, bomb_center_y, bomb_radius);
    SDL_SetRenderDrawColor(sdl_renderer, 255, 90, 42, 220);
    SDL_RenderFillCircle(sdl_renderer, bomb_center_x,
                         bomb_center_y - std::max(1, bomb_radius / 2),
                         std::max(1, bomb_radius / 2));
  }
}

void Renderer::drawrockets() {
  if (game == nullptr) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const int non_character_sprite_lift_px = getNonCharacterSpriteLiftPixels();
  const int rocket_extra_lift_px =
      ENABLE_3D_VIEW
          ? std::max(
                1, static_cast<int>(std::lround(static_cast<double>(element_size) *
                                                ROCKET_FLIGHT_EXTRA_LIFT_FACTOR)))
          : 0;
  for (const auto &rocket : game->active_rockets) {
    const Uint32 step_duration_ms = std::max<Uint32>(1, rocket.step_duration_ms);
    const double progress = std::clamp(
        static_cast<double>(now - rocket.segment_started_ticks) /
            static_cast<double>(step_duration_ms),
        0.0, 1.0);
    const MapCoord next_coord = StepRenderCoord(rocket.current_coord, rocket.direction);
    const bool next_is_wall =
        ENABLE_3D_VIEW && map != nullptr && next_coord.u >= 0 &&
        next_coord.v >= 0 && next_coord.u < static_cast<int>(rows) &&
        next_coord.v < static_cast<int>(cols) &&
        map->map_entry(static_cast<size_t>(next_coord.u),
                       static_cast<size_t>(next_coord.v)) ==
            ElementType::TYPE_WALL;
    const double rendered_progress = progress * (next_is_wall ? 0.5 : 1.0);
    const PixelCoord current_px = getPixelCoord(rocket.current_coord, 0, 0);
    const PixelCoord next_px = getPixelCoord(next_coord, 0, 0);
    const SDL_FPoint center{
        static_cast<float>(current_px.x + element_size / 2 +
                           (next_px.x - current_px.x) * rendered_progress),
        static_cast<float>(current_px.y + element_size / 2 +
                           (next_px.y - current_px.y) * rendered_progress -
                           non_character_sprite_lift_px -
                           rocket_extra_lift_px)};
    const int frame_index =
        static_cast<int>((now / std::max<Uint32>(1, ROCKET_ANIMATION_FRAME_MS) +
                          static_cast<Uint32>(rocket.animation_seed)) %
                         kRocketFlightFrames);
    const int glow_radius = std::max(7, static_cast<int>(element_size * 0.26));
    const int max_dimension = std::max(28, element_size * 2);
    const int half_extent = std::max(glow_radius, max_dimension / 2) + 6;
    const SDL_Rect occlusion_bounds{
        static_cast<int>(std::lround(center.x)) - half_extent,
        static_cast<int>(std::lround(center.y)) - half_extent, half_extent * 2,
        half_extent * 2};
    const double center_row_cells =
        static_cast<double>(rocket.current_coord.u) + 0.5 +
        static_cast<double>(next_coord.u - rocket.current_coord.u) *
            rendered_progress;
    const auto draw_rocket = [&]() {
      SDL_SetRenderDrawColor(sdl_renderer, 255, 122, 38, 84);
      SDL_RenderFillCircle(sdl_renderer, static_cast<int>(std::lround(center.x)),
                           static_cast<int>(std::lround(center.y)), glow_radius);
      drawRocketFlight(center, rocket.direction, max_dimension, frame_index, 255);
    };
    if (ENABLE_3D_VIEW) {
      drawWithWallOcclusion(occlusion_bounds, center_row_cells, draw_rocket);
      continue;
    }
    draw_rocket();
  }
}

void Renderer::drawbiohazardbeam() {
  if (game == nullptr || game->dead || game->pacman == nullptr ||
      !game->active_biohazard_beam.is_active) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const Directions direction = game->active_biohazard_beam.direction;
  const bool use_locked_segment = game->active_biohazard_beam.is_locked_after_hit;
  const MapCoord origin_coord =
      use_locked_segment ? game->active_biohazard_beam.locked_origin_coord
                         : game->pacman->map_coord;
  const PixelCoord origin_delta =
      use_locked_segment ? game->active_biohazard_beam.locked_origin_delta
                         : game->pacman->px_delta;
  const PixelCoord pacman_px = getPixelCoord(
      origin_coord, static_cast<int>(element_size * origin_delta.x / 100.0),
      static_cast<int>(element_size * origin_delta.y / 100.0));
  const SDL_Rect pacman_rect{
      pacman_px.x + static_cast<int>(element_size * 0.05),
      pacman_px.y + static_cast<int>(element_size * 0.05),
      static_cast<int>(element_size * 0.9), static_cast<int>(element_size * 0.9)};
  const int non_character_sprite_lift_px = getNonCharacterSpriteLiftPixels();
  const float side_beam_raise_px =
      (direction == Directions::Left || direction == Directions::Right)
          ? static_cast<float>(element_size * BIOHAZARD_SIDE_BEAM_RAISE_FACTOR)
          : 0.0f;
  const SDL_FPoint start{
      static_cast<float>(pacman_rect.x + pacman_rect.w / 2),
      static_cast<float>(pacman_rect.y + pacman_rect.h / 2 -
                         std::max(2, non_character_sprite_lift_px / 2)) -
          side_beam_raise_px};
  SDL_FPoint beam_start = start;

  SDL_FPoint end{};
  if (use_locked_segment) {
    const SDL_FPoint locked_end = game->active_biohazard_beam.locked_end_world_center;
    end = SDL_FPoint{
        static_cast<float>(offset_x +
                           locked_end.x * static_cast<float>(element_size + 1)),
        static_cast<float>(offset_y +
                           locked_end.y * static_cast<float>(element_size + 1))};
    switch (direction) {
    case Directions::Left:
    case Directions::Right:
      end.y = start.y;
      break;
    case Directions::Up:
    case Directions::Down:
      end.x = start.x;
      break;
    case Directions::None:
    default:
      break;
    }
  } else {
    MapCoord beam_end_coord = game->pacman->map_coord;
    MapCoord next_coord = game->pacman->map_coord;
    while (true) {
      next_coord = StepRenderCoord(next_coord, direction);
      if (next_coord.u < 0 || next_coord.v < 0 ||
          next_coord.u >= static_cast<int>(rows) ||
          next_coord.v >= static_cast<int>(cols)) {
        break;
      }
      if (map != nullptr &&
          map->map_entry(static_cast<size_t>(next_coord.u),
                         static_cast<size_t>(next_coord.v)) ==
              ElementType::TYPE_WALL) {
        break;
      }
      bool monster_blocks = false;
      for (const Monster *monster : game->monsters) {
        if (monster != nullptr && monster->is_alive &&
            monster->map_coord.u == next_coord.u &&
            monster->map_coord.v == next_coord.v) {
          monster_blocks = true;
          break;
        }
      }
      beam_end_coord = next_coord;
      if (monster_blocks) {
        break;
      }
    }

    const PixelCoord end_px = getPixelCoord(beam_end_coord, 0, 0);
    end = SDL_FPoint{
        static_cast<float>(end_px.x + element_size / 2),
        static_cast<float>(end_px.y + element_size / 2 -
                           std::max(2, non_character_sprite_lift_px / 2)) -
            side_beam_raise_px};

    if (beam_end_coord.u == game->pacman->map_coord.u &&
        beam_end_coord.v == game->pacman->map_coord.v) {
      switch (direction) {
      case Directions::Up:
        end.y -= static_cast<float>(element_size * 0.55);
        break;
      case Directions::Down:
        end.y += static_cast<float>(element_size * 0.55);
        break;
      case Directions::Left:
        end.x -= static_cast<float>(element_size * 0.55);
        break;
      case Directions::Right:
        end.x += static_cast<float>(element_size * 0.55);
        break;
      case Directions::None:
      default:
        break;
      }
    }
  }

  if (use_locked_segment &&
      game->active_biohazard_beam.locked_until_ticks >
          game->active_biohazard_beam.hit_sequence_started_ticks &&
      now >= game->active_biohazard_beam.hit_sequence_started_ticks) {
    const Uint32 sequence_duration =
        game->active_biohazard_beam.locked_until_ticks -
        game->active_biohazard_beam.hit_sequence_started_ticks;
    const double raw_progress = std::clamp(
        static_cast<double>(now -
                            game->active_biohazard_beam.hit_sequence_started_ticks) /
            static_cast<double>(sequence_duration),
        0.0, 1.0);
    const double sink_progress = std::pow(raw_progress, 1.25);
    beam_start.x =
        static_cast<float>(start.x + (end.x - start.x) * sink_progress);
    beam_start.y =
        static_cast<float>(start.y + (end.y - start.y) * sink_progress);
  }

  const double axis_x = static_cast<double>(end.x - beam_start.x);
  const double axis_y = static_cast<double>(end.y - beam_start.y);
  const double axis_length = std::hypot(axis_x, axis_y);
  if (axis_length < 2.0) {
    return;
  }

  const double normal_x = -axis_y / axis_length;
  const double normal_y = axis_x / axis_length;
  const double beam_clock =
      static_cast<double>(now + game->active_biohazard_beam.animation_seed * 13) /
      160.0;
  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  struct BeamWave {
    SDL_Color color;
    double amplitude_factor;
    double frequency;
    double frequency_secondary;
    double secondary_weight;
    double phase_speed;
    double phase_offset;
  };

  const std::array<BeamWave, 11> beam_waves{{
      {SDL_Color{12, 58, 200, 96}, 0.30, 0.7, 1.9, 0.30, 1.4, 0.0},
      {SDL_Color{24, 90, 230, 110}, 0.26, 0.95, 2.6, 0.34, 1.7, 0.6},
      {SDL_Color{38, 122, 255, 124}, 0.22, 1.25, 3.4, 0.36, 2.0, 1.2},
      {SDL_Color{58, 162, 255, 142}, 0.19, 1.55, 4.1, 0.40, 2.2, 1.8},
      {SDL_Color{74, 198, 255, 158}, 0.165, 1.85, 4.8, 0.42, 2.4, 2.4},
      {SDL_Color{90, 224, 255, 174}, 0.142, 2.20, 5.5, 0.44, 2.6, 3.0},
      {SDL_Color{120, 232, 255, 188}, 0.118, 2.55, 6.2, 0.46, 2.85, 3.6},
      {SDL_Color{162, 244, 255, 200}, 0.098, 2.90, 7.0, 0.48, 3.05, 4.2},
      {SDL_Color{198, 248, 255, 212}, 0.078, 3.30, 7.9, 0.50, 3.30, 4.8},
      {SDL_Color{224, 252, 255, 224}, 0.060, 3.80, 8.9, 0.52, 3.55, 5.4},
      {SDL_Color{244, 254, 255, 236}, 0.044, 4.40, 10.1, 0.55, 3.85, 6.0},
  }};

  constexpr double kPi = 3.14159265358979323846;

  for (size_t wave_index = 0; wave_index < beam_waves.size(); ++wave_index) {
    const BeamWave &wave_def = beam_waves[wave_index];
    const SDL_Color color = wave_def.color;
    const double amplitude =
        std::max(3.0, element_size * wave_def.amplitude_factor);
    const double phase =
        beam_clock * wave_def.phase_speed + wave_def.phase_offset;
    SDL_FPoint previous = beam_start;

    for (int sample = 1; sample <= 72; ++sample) {
      const double t = static_cast<double>(sample) / 72.0;
      const double envelope = std::sin(t * kPi);
      const double wave =
          std::sin(t * wave_def.frequency * 2.0 * kPi + phase) +
          wave_def.secondary_weight *
              std::sin(t * wave_def.frequency_secondary * 2.0 * kPi -
                       phase * 0.7) +
          0.18 * std::sin(t * wave_def.frequency * 9.0 * kPi + phase * 1.3);
      const double offset = amplitude * envelope * wave;
      SDL_FPoint current{
          static_cast<float>(beam_start.x + axis_x * t + normal_x * offset),
          static_cast<float>(beam_start.y + axis_y * t + normal_y * offset)};
      SDL_RenderDrawAALine(sdl_renderer, previous.x, previous.y, current.x,
                           current.y, color);
      previous = current;
    }
  }

  SDL_RenderDrawAALine(sdl_renderer, beam_start.x, beam_start.y, end.x, end.y,
                       SDL_Color{228, 252, 255, 220});

  for (int pulse = 0; pulse < 4; ++pulse) {
    const double progress =
        std::fmod(beam_clock * 0.21 + pulse * 0.23, 1.0);
    const int pulse_x =
        static_cast<int>(std::lround(beam_start.x + axis_x * progress));
    const int pulse_y =
        static_cast<int>(std::lround(beam_start.y + axis_y * progress));
    SDL_SetRenderDrawColor(sdl_renderer, 110, 224, 255, 150);
    SDL_RenderFillCircle(sdl_renderer, pulse_x, pulse_y,
                         std::max(3, element_size / 10));
    SDL_SetRenderDrawColor(sdl_renderer, 232, 252, 255, 210);
    SDL_RenderFillCircle(sdl_renderer, pulse_x, pulse_y,
                         std::max(1, element_size / 18));
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawalienlaser() {
  if (game == nullptr || game->dead || game->pacman == nullptr ||
      !game->active_alien_laser.is_active) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const ActiveAlienLaser &laser = game->active_alien_laser;
  const Directions direction = laser.direction;
  const PixelCoord pacman_px = getPixelCoord(
      laser.origin_coord,
      static_cast<int>(element_size * laser.origin_delta.x / 100.0),
      static_cast<int>(element_size * laser.origin_delta.y / 100.0));
  const SDL_Rect pacman_rect{
      pacman_px.x + static_cast<int>(element_size * 0.05),
      pacman_px.y + static_cast<int>(element_size * 0.05),
      static_cast<int>(element_size * 0.9),
      static_cast<int>(element_size * 0.9)};
  const int non_character_sprite_lift_px = getNonCharacterSpriteLiftPixels();
  const float side_beam_raise_px =
      (direction == Directions::Left || direction == Directions::Right)
          ? static_cast<float>(element_size * ALIEN_LASER_SIDE_BEAM_RAISE_FACTOR)
          : 0.0f;
  const SDL_FPoint start{
      static_cast<float>(pacman_rect.x + pacman_rect.w / 2),
      static_cast<float>(pacman_rect.y + pacman_rect.h / 2 -
                         std::max(2, non_character_sprite_lift_px / 2)) -
          side_beam_raise_px};
  SDL_FPoint end =
      ENABLE_3D_VIEW
          ? projectScene(laser.end_world_center.x, laser.end_world_center.y,
                         0.0)
          : SDL_FPoint{static_cast<float>(
                           offset_x + 1 +
                           laser.end_world_center.x *
                               static_cast<float>(element_size + 1)),
                       static_cast<float>(
                           offset_y + 1 +
                           laser.end_world_center.y *
                               static_cast<float>(element_size + 1))};
  switch (direction) {
  case Directions::Left:
  case Directions::Right:
    end.y = start.y;
    break;
  case Directions::Up:
  case Directions::Down:
    end.x = start.x;
    break;
  case Directions::None:
  default:
    break;
  }

  const double visible_duration = static_cast<double>(
      std::max<Uint32>(1, laser.visible_until_ticks - laser.started_ticks));
  const double progress =
      std::clamp(static_cast<double>(now - laser.started_ticks) /
                     visible_duration,
                 0.0, 1.0);
  const double flash = std::sin(progress * M_PI);
  const double alpha_scale =
      std::clamp(0.30 + flash * 0.95, 0.0, 1.0);
  SDL_FPoint beam_start = start;
  const Uint32 sink_started_ticks =
      laser.started_ticks + ALIEN_LASER_SINK_START_DELAY_MS;
  if (now >= sink_started_ticks &&
      laser.visible_until_ticks > sink_started_ticks) {
    const double sink_progress =
        std::pow(std::clamp(static_cast<double>(now - sink_started_ticks) /
                                static_cast<double>(laser.visible_until_ticks -
                                                    sink_started_ticks),
                            0.0, 1.0),
                 1.25);
    beam_start.x =
        static_cast<float>(start.x + (end.x - start.x) * sink_progress);
    beam_start.y =
        static_cast<float>(start.y + (end.y - start.y) * sink_progress);
  }
  const double axis_x = static_cast<double>(end.x - beam_start.x);
  const double axis_y = static_cast<double>(end.y - beam_start.y);
  const double axis_length = std::hypot(axis_x, axis_y);
  if (axis_length < 2.0) {
    return;
  }

  const double normal_x = -axis_y / axis_length;
  const double normal_y = axis_x / axis_length;
  const double beam_clock =
      static_cast<double>(now + laser.animation_seed * 17) / 95.0;

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  auto draw_thick_line = [&](SDL_FPoint a, SDL_FPoint b, SDL_Color color,
                             int half_width_px) {
    for (int offset = -half_width_px; offset <= half_width_px; ++offset) {
      const double edge =
          1.0 - std::abs(offset) / static_cast<double>(half_width_px + 1);
      SDL_Color mod_color = color;
      mod_color.a = static_cast<Uint8>(
          std::clamp(static_cast<double>(color.a) * edge * alpha_scale, 0.0,
                     255.0));
      SDL_RenderDrawAALine(
          sdl_renderer, a.x + normal_x * offset, a.y + normal_y * offset,
          b.x + normal_x * offset, b.y + normal_y * offset, mod_color);
    }
  };

  const int halo_width = std::max(8, static_cast<int>(element_size * 0.34));
  const int flame_width = std::max(5, static_cast<int>(element_size * 0.22));
  const int core_width = std::max(2, static_cast<int>(element_size * 0.09));
  draw_thick_line(beam_start, end, SDL_Color{255, 46, 12, 120}, halo_width);
  draw_thick_line(beam_start, end, SDL_Color{255, 118, 22, 175}, flame_width);
  draw_thick_line(beam_start, end, SDL_Color{255, 238, 94, 235}, core_width);
  draw_thick_line(beam_start, end, SDL_Color{255, 252, 216, 245},
                  std::max(1, core_width / 2));

  struct FlameWave {
    SDL_Color color;
    double amplitude_factor;
    double frequency;
    double phase_offset;
    int half_width;
  };

  const std::array<FlameWave, 8> waves{{
      {SDL_Color{255, 28, 8, 130}, 0.46, 1.0, 0.0, halo_width / 2},
      {SDL_Color{255, 72, 12, 150}, 0.38, 1.7, 0.7, halo_width / 3},
      {SDL_Color{255, 124, 18, 172}, 0.30, 2.4, 1.4, flame_width / 2},
      {SDL_Color{255, 168, 34, 188}, 0.24, 3.1, 2.1, flame_width / 3},
      {SDL_Color{255, 214, 68, 205}, 0.18, 3.9, 2.8, core_width + 1},
      {SDL_Color{255, 246, 138, 220}, 0.13, 4.7, 3.5, core_width},
      {SDL_Color{255, 255, 210, 235}, 0.08, 5.9, 4.2,
       std::max(1, core_width / 2)},
      {SDL_Color{255, 62, 18, 145}, 0.52, 0.65, 5.0, halo_width / 3},
  }};

  for (const FlameWave &wave : waves) {
    const double amplitude =
        std::max(3.0, element_size * wave.amplitude_factor);
    const double phase = beam_clock * (1.2 + wave.frequency * 0.25) +
                         wave.phase_offset;
    SDL_FPoint previous = beam_start;
    for (int sample = 1; sample <= 84; ++sample) {
      const double t = static_cast<double>(sample) / 84.0;
      const double envelope = std::sin(t * M_PI);
      const double wave_value =
          std::sin(t * wave.frequency * 2.0 * M_PI + phase) +
          0.42 * std::sin(t * wave.frequency * 5.0 * M_PI - phase * 0.7);
      const double offset = amplitude * envelope * wave_value;
      SDL_FPoint current{
          static_cast<float>(beam_start.x + axis_x * t + normal_x * offset),
          static_cast<float>(beam_start.y + axis_y * t + normal_y * offset)};
      draw_thick_line(previous, current, wave.color,
                      std::max(1, wave.half_width));
      previous = current;
    }
  }

  for (int burst = 0; burst < 7; ++burst) {
    const double t = std::fmod(beam_clock * 0.18 + burst * 0.142, 1.0);
    const double radius_pulse = 0.78 + 0.28 * std::sin(beam_clock + burst);
    const int pulse_x =
        static_cast<int>(std::lround(beam_start.x + axis_x * t));
    const int pulse_y =
        static_cast<int>(std::lround(beam_start.y + axis_y * t));
    const int outer_radius =
        std::max(4, static_cast<int>(element_size * 0.17 * radius_pulse));
    SDL_SetRenderDrawColor(
        sdl_renderer, 255, 76, 10,
        static_cast<Uint8>(std::clamp(130.0 * alpha_scale, 0.0, 180.0)));
    SDL_RenderFillCircle(sdl_renderer, pulse_x, pulse_y, outer_radius);
    SDL_SetRenderDrawColor(
        sdl_renderer, 255, 236, 96,
        static_cast<Uint8>(std::clamp(220.0 * alpha_scale, 0.0, 240.0)));
    SDL_RenderFillCircle(sdl_renderer, pulse_x, pulse_y,
                         std::max(2, outer_radius / 2));
  }

  const int impact_x = static_cast<int>(std::lround(end.x));
  const int impact_y = static_cast<int>(std::lround(end.y));
  const int impact_radius =
      std::max(10, static_cast<int>(element_size * (0.34 + flash * 0.34)));
  SDL_SetRenderDrawColor(
      sdl_renderer, 255, 54, 8,
      static_cast<Uint8>(std::clamp(140.0 * alpha_scale, 0.0, 180.0)));
  SDL_RenderFillCircle(sdl_renderer, impact_x, impact_y, impact_radius);
  SDL_SetRenderDrawColor(
      sdl_renderer, 255, 228, 92,
      static_cast<Uint8>(std::clamp(230.0 * alpha_scale, 0.0, 245.0)));
  SDL_RenderFillCircle(sdl_renderer, impact_x, impact_y,
                       std::max(4, impact_radius / 2));

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawStunStars(int center_x, int center_y, int orbit_radius_px,
                             int star_radius_px, Uint32 now) {
  if (orbit_radius_px <= 0 || star_radius_px <= 0) {
    return;
  }
  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double phase =
      static_cast<double>(now) / static_cast<double>(STUN_STARS_ORBIT_PERIOD_MS);
  const double base_angle = phase * 2.0 * M_PI;
  for (int i = 0; i < STUN_STARS_COUNT; ++i) {
    const double angle = base_angle +
                         (2.0 * M_PI * static_cast<double>(i)) /
                             static_cast<double>(STUN_STARS_COUNT);
    const int sx = center_x +
                   static_cast<int>(std::cos(angle) * orbit_radius_px);
    const int sy = center_y +
                   static_cast<int>(std::sin(angle) * orbit_radius_px * 0.45);

    // Five-pointed star drawn as filled triangles via line bunch.
    const double rot = phase * 4.0 * M_PI;
    SDL_Point pts[11];
    for (int k = 0; k < 10; ++k) {
      const double r = (k % 2 == 0) ? star_radius_px : star_radius_px * 0.45;
      const double a = rot + (M_PI * 2.0 * k) / 10.0 - M_PI / 2.0;
      pts[k].x = sx + static_cast<int>(std::cos(a) * r);
      pts[k].y = sy + static_cast<int>(std::sin(a) * r);
    }
    pts[10] = pts[0];
    SDL_SetRenderDrawColor(sdl_renderer, 255, 230, 90, 240);
    SDL_RenderDrawLines(sdl_renderer, pts, 11);
    SDL_SetRenderDrawColor(sdl_renderer, 255, 200, 50, 200);
    SDL_RenderFillCircle(sdl_renderer, sx, sy,
                         std::max(1, star_radius_px / 3));
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawPulsingHeart(int center_x, int center_y, int size_px,
                                Uint8 alpha, double pulse) {
  if (size_px <= 0 || alpha == 0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const int radius = std::max(2, static_cast<int>(size_px * 0.25 * pulse));
  const int half_width = std::max(3, static_cast<int>(size_px * 0.46 * pulse));
  const int top_y = center_y - static_cast<int>(size_px * 0.14 * pulse);
  SDL_SetRenderDrawColor(sdl_renderer, 255, 44, 62,
                         static_cast<Uint8>(std::clamp<int>(alpha, 0, 255)));
  SDL_RenderFillCircle(sdl_renderer, center_x - radius, top_y, radius);
  SDL_RenderFillCircle(sdl_renderer, center_x + radius, top_y, radius);
  for (int y = 0; y < size_px; ++y) {
    const double progress = static_cast<double>(y) / std::max(1, size_px - 1);
    const int half = static_cast<int>(half_width * (1.0 - progress));
    const int row_y = top_y + y / 2;
    SDL_RenderDrawLine(sdl_renderer, center_x - half, row_y, center_x + half,
                       row_y);
  }
  SDL_SetRenderDrawColor(sdl_renderer, 255, 152, 164, alpha / 2);
  SDL_RenderDrawCircle(sdl_renderer, center_x - radius, top_y, radius);
  SDL_RenderDrawCircle(sdl_renderer, center_x + radius, top_y, radius);

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawAlienThoughtBubble(const AlienAgent &alien,
                                      const SDL_Rect &alien_rect,
                                      Uint32 now) {
  if (!alien.is_alive || alien.thought_index < 0 ||
      alien.thought_index >= static_cast<int>(kAlienThoughts.size()) ||
      alien.thought_started_ticks == 0 ||
      alien.thought_visible_until_ticks == 0 ||
      now >= alien.thought_visible_until_ticks + ALIEN_THOUGHT_FADE_MS) {
    return;
  }

  const Uint32 fade_in_ms = std::min<Uint32>(350, ALIEN_THOUGHT_FADE_MS);
  const double fade_in =
      std::clamp(static_cast<double>(now - alien.thought_started_ticks) /
                     static_cast<double>(std::max<Uint32>(1, fade_in_ms)),
                 0.0, 1.0);
  double fade_out = 1.0;
  if (now > alien.thought_visible_until_ticks) {
    fade_out =
        1.0 - std::clamp(static_cast<double>(now -
                                             alien.thought_visible_until_ticks) /
                             static_cast<double>(
                                 std::max<Uint32>(1, ALIEN_THOUGHT_FADE_MS)),
                         0.0, 1.0);
  }
  const double alpha_scale = std::clamp(fade_in * fade_out, 0.0, 1.0);
  if (alpha_scale <= 0.01) {
    return;
  }

  const std::string text =
      kAlienThoughts[static_cast<size_t>(alien.thought_index)];
  const double text_scale = 0.52;
  const int padding_x = std::max(5, element_size / 10);
  const int padding_y = std::max(4, element_size / 14);
  const int max_rendered_text_width =
      std::clamp(static_cast<int>(std::lround(element_size * 2.45)), 82, 140);
  const int max_text_width =
      std::max(1, static_cast<int>(
                      std::lround(max_rendered_text_width / text_scale)));
  const int font_height = std::max(1, TTF_FontHeight(sdl_font_hud));
  const int line_height =
      std::max(8, static_cast<int>(std::lround(font_height * text_scale)));

  std::vector<std::string> lines;
  std::string current_line;
  size_t word_start = 0;
  while (word_start < text.size()) {
    const size_t word_end = text.find(' ', word_start);
    const std::string word =
        text.substr(word_start, word_end == std::string::npos
                                    ? std::string::npos
                                    : word_end - word_start);
    const std::string candidate =
        current_line.empty() ? word : current_line + " " + word;
    int candidate_width = 0;
    TTF_SizeText(sdl_font_hud, candidate.c_str(), &candidate_width, nullptr);
    if (!current_line.empty() && candidate_width > max_text_width) {
      lines.push_back(current_line);
      current_line = word;
    } else {
      current_line = candidate;
    }
    if (word_end == std::string::npos) {
      break;
    }
    word_start = word_end + 1;
  }
  if (!current_line.empty()) {
    lines.push_back(current_line);
  }
  if (lines.empty()) {
    return;
  }

  int text_width = 0;
  for (const std::string &line : lines) {
    int line_width = 0;
    TTF_SizeText(sdl_font_hud, line.c_str(), &line_width, nullptr);
    text_width = std::max(text_width, line_width);
  }

  const int rendered_text_width =
      static_cast<int>(std::lround(text_width * text_scale));
  const int bubble_w =
      std::min(screen_res_x - 16, rendered_text_width + padding_x * 2);
  const int bubble_h =
      static_cast<int>(lines.size()) * line_height + padding_y * 2;
  int bubble_x = alien_rect.x + alien_rect.w / 2 - bubble_w / 2;
  int bubble_y = alien_rect.y - bubble_h - std::max(4, element_size / 8);
  bubble_x = std::clamp(bubble_x, 8, std::max(8, screen_res_x - bubble_w - 8));
  bubble_y = std::clamp(bubble_y, 8, std::max(8, screen_res_y - bubble_h - 8));
  const SDL_Rect bubble_rect{bubble_x, bubble_y, bubble_w, bubble_h};

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const Uint8 fill_alpha =
      static_cast<Uint8>(std::clamp(220.0 * alpha_scale, 0.0, 220.0));
  const Uint8 outline_alpha =
      static_cast<Uint8>(std::clamp(230.0 * alpha_scale, 0.0, 230.0));
  const Uint8 text_alpha =
      static_cast<Uint8>(std::clamp(255.0 * alpha_scale, 0.0, 255.0));

  const int min_lobe_radius = std::max(3, bubble_h / 10);
  struct CloudLobe {
    double x_factor;
    double y_factor;
    double radius_factor;
  };
  const std::array<CloudLobe, 9> lobes{{
      {0.14, 0.58, 0.20},
      {0.25, 0.32, 0.23},
      {0.42, 0.23, 0.25},
      {0.61, 0.24, 0.24},
      {0.78, 0.36, 0.22},
      {0.87, 0.62, 0.19},
      {0.68, 0.78, 0.21},
      {0.43, 0.80, 0.23},
      {0.20, 0.73, 0.19},
  }};

  auto draw_cloud_shape = [&](SDL_Color color, int radius_delta,
                              int inset) {
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b, color.a);
    const SDL_Rect core_rect{bubble_rect.x + inset + bubble_h / 5,
                             bubble_rect.y + inset + bubble_h / 5,
                             std::max(1, bubble_rect.w - bubble_h / 3 -
                                             inset * 2),
                             std::max(1, bubble_rect.h * 3 / 5 -
                                             inset * 2)};
    const SDL_Rect lower_rect{bubble_rect.x + inset + bubble_h / 4,
                              bubble_rect.y + inset + bubble_rect.h / 2,
                              std::max(1, bubble_rect.w - bubble_h / 2 -
                                              inset * 2),
                              std::max(1, bubble_rect.h / 3 - inset * 2)};
    SDL_RenderFillRect(sdl_renderer, &core_rect);
    SDL_RenderFillRect(sdl_renderer, &lower_rect);
    for (const CloudLobe &lobe : lobes) {
      const int radius =
          std::max(1, min_lobe_radius +
                          static_cast<int>(std::lround(
                              bubble_h * lobe.radius_factor)) +
                          radius_delta);
      const int center_x = bubble_rect.x + inset +
                           static_cast<int>(std::lround(
                               (bubble_rect.w - inset * 2) * lobe.x_factor));
      const int center_y = bubble_rect.y + inset +
                           static_cast<int>(std::lround(
                               (bubble_rect.h - inset * 2) * lobe.y_factor));
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius);
    }
  };

  draw_cloud_shape(SDL_Color{0, 0, 0, outline_alpha}, 1, 0);
  draw_cloud_shape(SDL_Color{255, 255, 255, fill_alpha}, 0, 1);

  const int cloud_anchor_x =
      std::clamp(alien_rect.x + alien_rect.w / 2, bubble_rect.x + bubble_w / 5,
                 bubble_rect.x + bubble_w * 4 / 5);
  const int cloud_anchor_y = bubble_rect.y + bubble_rect.h;
  const int figure_anchor_x = alien_rect.x + alien_rect.w / 2;
  const int figure_anchor_y = alien_rect.y + alien_rect.h / 4;
  for (int bubble_index = 0; bubble_index < 4; ++bubble_index) {
    const double t = 0.22 + bubble_index * 0.18;
    const int center_x = static_cast<int>(std::lround(
        figure_anchor_x + (cloud_anchor_x - figure_anchor_x) * t));
    const int center_y = static_cast<int>(std::lround(
        figure_anchor_y + (cloud_anchor_y - figure_anchor_y) * t));
    const int radius =
        std::max(2, static_cast<int>(std::lround(
                        element_size * (0.020 + bubble_index * 0.010))));
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, outline_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius + 1);
    SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, fill_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius);
  }

  const SDL_Color text_color{0, 0, 0, text_alpha};
  int text_y = bubble_rect.y + padding_y;
  for (const std::string &line : lines) {
    const TextCache::Entry *entry =
        text_cache_.GetOrCreate(sdl_renderer, sdl_font_hud, line, text_color);
    if (entry != nullptr) {
      const SDL_Rect destination{
          bubble_rect.x + padding_x, text_y,
          std::max(1, static_cast<int>(std::lround(entry->w * text_scale))),
          std::max(1, static_cast<int>(std::lround(entry->h * text_scale)))};
      SDL_RenderCopy(sdl_renderer, entry->texture, nullptr, &destination);
    }
    text_y += line_height;
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawDiscoPickupIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                                   double animation_clock) {
  if (alpha == 0 || icon_rect.w <= 0 || icon_rect.h <= 0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const int cx = icon_rect.x + icon_rect.w / 2;
  const int top =
      icon_rect.y +
      std::max(1, static_cast<int>(icon_rect.h /
                                   DISCO_PICKUP_ICON_CHAIN_TOP_DIVISOR));
  const int chain_bottom =
      icon_rect.y +
      static_cast<int>(icon_rect.h /
                       DISCO_PICKUP_ICON_CHAIN_BOTTOM_DIVISOR);
  SDL_SetRenderDrawColor(sdl_renderer, DISCO_PICKUP_ICON_CHAIN_COLOR.r,
                         DISCO_PICKUP_ICON_CHAIN_COLOR.g,
                         DISCO_PICKUP_ICON_CHAIN_COLOR.b, alpha);
  for (int y = top; y < chain_bottom;
       y += std::max(3, static_cast<int>(
                            icon_rect.h /
                            DISCO_PICKUP_ICON_CHAIN_STEP_DIVISOR))) {
    SDL_Rect link{
        cx - std::max(1, static_cast<int>(
                             icon_rect.w /
                             DISCO_PICKUP_ICON_CHAIN_LINK_X_DIVISOR)),
        y,
        std::max(2, static_cast<int>(
                        icon_rect.w /
                        DISCO_PICKUP_ICON_CHAIN_LINK_W_DIVISOR)),
        std::max(3, static_cast<int>(
                        icon_rect.h /
                        DISCO_PICKUP_ICON_CHAIN_LINK_H_DIVISOR))};
    SDL_RenderDrawRect(sdl_renderer, &link);
  }

  const int radius = std::max(
      3, static_cast<int>(icon_rect.w /
                          DISCO_PICKUP_ICON_BALL_RADIUS_DIVISOR));
  drawDiscoBall(cx,
                icon_rect.y + icon_rect.h / 2 +
                    static_cast<int>(radius /
                                     DISCO_PICKUP_ICON_BALL_CENTER_Y_DIVISOR),
                radius,
                std::fmod(animation_clock /
                              DISCO_PICKUP_ICON_ROTATION_DIVISOR,
                          1.0),
                true, alpha);

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawDiscoLightSpots(int axis_x, int axis_y,
                                   double rotation_phase, double intensity) {
  if (sdl_soft_puff_texture == nullptr || intensity <= 0.0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double base_angle = rotation_phase * 2.0 * kLogoPi;
  const int field_w =
      std::max(1, static_cast<int>(cols) * (element_size + 1));
  const int field_h =
      std::max(1, static_cast<int>(rows) * (element_size + 1));
  const double kDiscoAxisTiltRadians =
      DISCO_AXIS_TILT_DEGREES * kLogoPi / 180.0;
  const double tilt_cos = std::cos(kDiscoAxisTiltRadians);
  const double tilt_sin = std::sin(kDiscoAxisTiltRadians);

  for (int i = 0; i < DISCO_LIGHT_SPOT_COUNT; ++i) {
    const double seed_a = i * DISCO_LIGHT_SPOT_GOLDEN_ANGLE_RADIANS;
    const double seed_b =
        std::sin(i * DISCO_LIGHT_SPOT_HASH_A + DISCO_LIGHT_SPOT_HASH_B) *
        DISCO_LIGHT_SPOT_HASH_SCALE;
    const double jitter = seed_b - std::floor(seed_b);
    const double radial =
        std::sqrt((static_cast<double>(
                       (i * DISCO_LIGHT_SPOT_RADIAL_STEP) %
                       DISCO_LIGHT_SPOT_COUNT) +
                   DISCO_LIGHT_SPOT_RADIAL_BASE +
                   jitter * DISCO_LIGHT_SPOT_RADIAL_JITTER_SCALE) /
                  static_cast<double>(DISCO_LIGHT_SPOT_COUNT));
    const double orbit =
        base_angle + seed_a +
        DISCO_LIGHT_SPOT_ORBIT_WOBBLE *
            std::sin(base_angle * DISCO_LIGHT_SPOT_ORBIT_WOBBLE_FREQUENCY + i);
    const double local_x =
        std::cos(orbit) * radial * field_w * DISCO_LIGHT_SPOT_FIELD_RADIUS_X;
    const double local_y =
        std::sin(orbit) * radial * field_h * DISCO_LIGHT_SPOT_FIELD_RADIUS_Y;
    const double local_z = local_y * tilt_sin;
    const double perspective_y =
        (local_y * tilt_cos) *
        (DISCO_LIGHT_SPOT_PERSPECTIVE_BASE +
         DISCO_LIGHT_SPOT_PERSPECTIVE_SWING * std::cos(seed_a + base_angle)) *
        (1.0 + std::clamp(local_z / std::max(1.0, field_h * DISCO_LIGHT_SPOT_DEPTH_DENOMINATOR),
                          DISCO_LIGHT_SPOT_DEPTH_SCALE_MIN,
                          DISCO_LIGHT_SPOT_DEPTH_SCALE_MAX));
    const int x = axis_x + static_cast<int>(std::lround(local_x));
    const int y = axis_y + static_cast<int>(std::lround(perspective_y));
    const int w = std::max(
        DISCO_LIGHT_SPOT_MIN_WIDTH_PX,
        static_cast<int>(element_size *
                         (DISCO_LIGHT_SPOT_WIDTH_CELLS +
                          DISCO_LIGHT_SPOT_WIDTH_JITTER_CELLS * jitter)));
    const int h =
        std::max(DISCO_LIGHT_SPOT_MIN_HEIGHT_PX,
                 static_cast<int>(w * (DISCO_LIGHT_SPOT_HEIGHT_SCALE +
                                       DISCO_LIGHT_SPOT_HEIGHT_RADIAL_SCALE *
                                           radial)));
    SDL_Rect dst{x - w / 2, y - h / 2, w, h};
    const SDL_Color color = DISCO_LIGHT_SPOT_COLORS[static_cast<size_t>(
        i % DISCO_LIGHT_SPOT_COLORS.size())];
    const Uint8 alpha = static_cast<Uint8>(
        std::clamp(DISCO_LIGHT_SPOT_ALPHA_BASE * intensity *
                       (DISCO_LIGHT_SPOT_ALPHA_MIN_FACTOR +
                        DISCO_LIGHT_SPOT_ALPHA_SWING *
                            std::sin(orbit *
                                     DISCO_LIGHT_SPOT_ALPHA_PHASE_SCALE)) *
                       (1.0 - DISCO_LIGHT_SPOT_ALPHA_RADIAL_FADE * radial),
                   0.0, DISCO_LIGHT_SPOT_ALPHA_MAX));
    SDL_SetTextureColorMod(sdl_soft_puff_texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(sdl_soft_puff_texture,
                           static_cast<Uint8>(std::clamp<int>(
                               alpha / DISCO_LIGHT_SPOT_HALO_ALPHA_DIVISOR,
                               0, 255)));
    SDL_Rect halo_dst{
        dst.x - dst.w / DISCO_LIGHT_SPOT_HALO_EXPAND_DIVISOR,
        dst.y - dst.h / DISCO_LIGHT_SPOT_HALO_EXPAND_DIVISOR,
        dst.w + (dst.w * 2) / DISCO_LIGHT_SPOT_HALO_EXPAND_DIVISOR,
        dst.h + (dst.h * 2) / DISCO_LIGHT_SPOT_HALO_EXPAND_DIVISOR};
    SDL_RenderCopyEx(sdl_renderer, sdl_soft_puff_texture, nullptr, &halo_dst,
                     orbit * 180.0 / kLogoPi, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(sdl_soft_puff_texture, alpha);
    SDL_RenderCopyEx(sdl_renderer, sdl_soft_puff_texture, nullptr, &dst,
                     orbit * 180.0 / kLogoPi, nullptr, SDL_FLIP_NONE);
  }

  SDL_SetTextureColorMod(sdl_soft_puff_texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(sdl_soft_puff_texture, 255);
  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawDiscoBall(int center_x, int center_y, int radius_px,
                             double rotation_phase, bool spinning,
                             Uint8 alpha) {
  if (radius_px <= 0 || alpha == 0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
  const double kDiscoAxisTiltRadians =
      DISCO_AXIS_TILT_DEGREES * kLogoPi / 180.0;
  const double tilt_cos = std::cos(kDiscoAxisTiltRadians);
  const double tilt_sin = std::sin(kDiscoAxisTiltRadians);

  const auto project_fragment_corner = [&](double theta,
                                           double phi) -> SDL_FPoint {
    const double local_x = std::sin(phi) * std::cos(theta);
    const double local_y = std::sin(theta);
    const double local_z = std::cos(phi) * std::cos(theta);
    const double x3 = local_x;
    const double y3 = local_y * tilt_cos - local_z * tilt_sin;
    return SDL_FPoint{
        static_cast<float>(center_x + x3 * radius_px),
        static_cast<float>(center_y + y3 * radius_px * DISCO_BALL_SCREEN_Y_SCALE)};
  };
  const auto corner_inside_ball = [&](SDL_FPoint point) {
    const double dx = (point.x - center_x) / static_cast<double>(radius_px);
    const double dy =
        (point.y - center_y) /
        static_cast<double>(radius_px * DISCO_BALL_SCREEN_Y_SCALE);
    return dx * dx + dy * dy <=
           DISCO_BALL_CORNER_CLIP_RADIUS * DISCO_BALL_CORNER_CLIP_RADIUS;
  };
  struct NormalizedDiscoBallLight {
    double x;
    double y;
    double z;
    SDL_Color color;
    double diffuse;
    double specular;
  };
  const auto normalize_light =
      [](const DiscoDirectionalLight &light) -> NormalizedDiscoBallLight {
    const double x = light.x;
    const double y = light.y;
    const double z = light.z;
    const double len = std::max(0.0001, std::sqrt(x * x + y * y + z * z));
    return NormalizedDiscoBallLight{x / len, y / len, z / len, light.color,
                                    light.diffuse, light.specular};
  };
  std::array<NormalizedDiscoBallLight, DISCO_BALL_DIRECTIONAL_LIGHTS.size()>
      lights{};
  for (size_t light_index = 0; light_index < lights.size(); ++light_index) {
    lights[light_index] =
        normalize_light(DISCO_BALL_DIRECTIONAL_LIGHTS[light_index]);
  }

  const double spin = -rotation_phase * 2.0 * kLogoPi;
  const double theta_step = kLogoPi / DISCO_BALL_LATITUDE_COUNT;
  const double phi_step = 2.0 * kLogoPi / DISCO_BALL_LONGITUDE_COUNT;
  for (int lat_index = 0; lat_index < DISCO_BALL_LATITUDE_COUNT; ++lat_index) {
    const double theta =
        -0.5 * kLogoPi + (static_cast<double>(lat_index) + 0.5) * theta_step;
    const double cos_theta = std::cos(theta);
    const double sin_theta = std::sin(theta);

    for (int lon_index = 0; lon_index < DISCO_BALL_LONGITUDE_COUNT;
         ++lon_index) {
      const double phi = spin + static_cast<double>(lon_index) * phi_step;
      const double local_x = std::sin(phi) * cos_theta;
      const double local_y = sin_theta;
      const double local_z = std::cos(phi) * cos_theta;
      const double x3 = local_x;
      const double y3 = local_y * tilt_cos - local_z * tilt_sin;
      const double z3 = local_y * tilt_sin + local_z * tilt_cos;
      if (z3 < DISCO_BALL_BACKFACE_CULL_Z) {
        continue;
      }

      const double theta_half =
          theta_step * DISCO_BALL_FRAGMENT_THETA_HALF_SCALE;
      const double phi_half = phi_step * DISCO_BALL_FRAGMENT_PHI_HALF_SCALE;
      SDL_FPoint corners[4]{
          project_fragment_corner(theta - theta_half, phi - phi_half),
          project_fragment_corner(theta - theta_half, phi + phi_half),
          project_fragment_corner(theta + theta_half, phi + phi_half),
          project_fragment_corner(theta + theta_half, phi - phi_half)};
      if (!corner_inside_ball(corners[0]) || !corner_inside_ball(corners[1]) ||
          !corner_inside_ball(corners[2]) || !corner_inside_ball(corners[3])) {
        continue;
      }

      double light = std::clamp(
          DISCO_BALL_AMBIENT_BASE +
              DISCO_BALL_VIEW_LIGHT_SCALE * std::max(0.0, z3),
          0.0, 1.0);
      double red_accum =
          DISCO_BALL_RED_BASE + light * DISCO_BALL_RED_VIEW_SCALE;
      double green_accum =
          DISCO_BALL_GREEN_BASE + light * DISCO_BALL_GREEN_VIEW_SCALE;
      double blue_accum =
          DISCO_BALL_BLUE_BASE + light * DISCO_BALL_BLUE_VIEW_SCALE;
      double strongest_specular = 0.0;
      SDL_Color strongest_specular_color{235, 242, 255, 255};
      for (const NormalizedDiscoBallLight &light_source : lights) {
        const double ndotl = std::max(
            0.0, x3 * light_source.x + y3 * light_source.y +
                     z3 * light_source.z);
        const double diffuse = ndotl * light_source.diffuse;

        const double reflect_x = 2.0 * ndotl * x3 - light_source.x;
        const double reflect_y = 2.0 * ndotl * y3 - light_source.y;
        const double reflect_z = 2.0 * ndotl * z3 - light_source.z;
        const double specular =
            std::pow(std::max(0.0, reflect_z), DISCO_BALL_SPECULAR_POWER) *
            light_source.specular;

        red_accum +=
            light_source.color.r *
            (diffuse * DISCO_BALL_DIFFUSE_COLOR_SCALE +
             specular * DISCO_BALL_SPECULAR_COLOR_SCALE);
        green_accum +=
            light_source.color.g *
            (diffuse * DISCO_BALL_DIFFUSE_COLOR_SCALE +
             specular * DISCO_BALL_SPECULAR_COLOR_SCALE);
        blue_accum +=
            light_source.color.b *
            (diffuse * DISCO_BALL_DIFFUSE_COLOR_SCALE +
             specular * DISCO_BALL_SPECULAR_COLOR_SCALE);
        light = std::max(light, diffuse + specular);
        if (specular > strongest_specular) {
          strongest_specular = specular;
          strongest_specular_color = light_source.color;
        }
      }

      const double facet_noise =
          DISCO_BALL_FACET_NOISE_BASE +
          DISCO_BALL_FACET_NOISE_SWING *
              std::sin(lon_index * DISCO_BALL_FACET_NOISE_LON_SCALE +
                       lat_index * DISCO_BALL_FACET_NOISE_LAT_SCALE);
      const Uint8 red = static_cast<Uint8>(
          std::clamp(red_accum * facet_noise, 0.0, 255.0));
      const Uint8 green = static_cast<Uint8>(
          std::clamp(green_accum * facet_noise, 0.0, 255.0));
      const Uint8 blue = static_cast<Uint8>(
          std::clamp(blue_accum * facet_noise, 0.0, 255.0));
      const Uint8 tile_alpha = static_cast<Uint8>(
          std::clamp(static_cast<double>(alpha) *
                         (DISCO_BALL_TILE_ALPHA_BASE +
                          DISCO_BALL_TILE_ALPHA_LIGHT_SCALE *
                              std::clamp(light, 0.0, 1.0)),
                     0.0, 255.0));
      SDL_Vertex vertices[4]{
          {corners[0], SDL_Color{red, green, blue, tile_alpha}, SDL_FPoint{0, 0}},
          {corners[1], SDL_Color{red, green, blue, tile_alpha}, SDL_FPoint{0, 0}},
          {corners[2], SDL_Color{red, green, blue, tile_alpha}, SDL_FPoint{0, 0}},
          {corners[3], SDL_Color{red, green, blue, tile_alpha}, SDL_FPoint{0, 0}}};
      const int indices[6]{0, 1, 2, 0, 2, 3};
      SDL_RenderGeometry(sdl_renderer, nullptr, vertices, 4, indices, 6);

      if (strongest_specular > DISCO_BALL_SPECULAR_GLINT_THRESHOLD) {
        const SDL_FPoint center{
            static_cast<float>(center_x + x3 * radius_px),
            static_cast<float>(center_y + y3 * radius_px *
                                              DISCO_BALL_SCREEN_Y_SCALE)};
        const SDL_Color glint_color{strongest_specular_color.r,
                                    strongest_specular_color.g,
                                    strongest_specular_color.b,
                                    static_cast<Uint8>(std::clamp(
                                        strongest_specular * alpha, 0.0,
                                        DISCO_BALL_SPECULAR_GLINT_ALPHA_MAX))};
        SDL_RenderFillCircleAA(sdl_renderer, center.x, center.y,
                               std::max(DISCO_BALL_SPECULAR_GLINT_MIN_RADIUS_PX,
                                        radius_px *
                                            DISCO_BALL_SPECULAR_GLINT_RADIUS_CELLS),
                               glint_color);
      }

      SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255,
                             static_cast<Uint8>(
                                 tile_alpha / DISCO_BALL_EDGE_ALPHA_DIVISOR));
      SDL_RenderDrawAALine(sdl_renderer, corners[0].x, corners[0].y,
                           corners[1].x, corners[1].y,
                           {255, 255, 255,
                            static_cast<Uint8>(
                                tile_alpha / DISCO_BALL_EDGE_ALPHA_DIVISOR)});
      SDL_RenderDrawAALine(sdl_renderer, corners[1].x, corners[1].y,
                           corners[2].x, corners[2].y,
                           {255, 255, 255,
                            static_cast<Uint8>(
                                tile_alpha / DISCO_BALL_EDGE_ALPHA_DIVISOR)});
      SDL_RenderDrawAALine(sdl_renderer, corners[2].x, corners[2].y,
                           corners[3].x, corners[3].y,
                           {255, 255, 255,
                            static_cast<Uint8>(
                                tile_alpha / DISCO_BALL_EDGE_ALPHA_DIVISOR)});
      SDL_RenderDrawAALine(sdl_renderer, corners[3].x, corners[3].y,
                           corners[0].x, corners[0].y,
                           {255, 255, 255,
                            static_cast<Uint8>(
                                tile_alpha / DISCO_BALL_EDGE_ALPHA_DIVISOR)});
    }
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawDiscoEasterEggOverlay(Uint32 now) {
  if (game == nullptr || !game->active_disco_easteregg.is_active) {
    return;
  }

  const ActiveDiscoEasterEgg &disco = game->active_disco_easteregg;
  const Uint32 intro_elapsed = now - disco.started_ticks;
  const double dim_in =
      std::clamp(static_cast<double>(intro_elapsed) /
                     static_cast<double>(std::max<Uint32>(1, DISCO_DIM_DOWN_MS)),
                 0.0, 1.0);
  double dim_factor = dim_in;
  if (disco.is_ending) {
    dim_factor =
        1.0 - std::clamp(static_cast<double>(now - disco.ending_started_ticks) /
                             static_cast<double>(
                                 std::max<Uint32>(1, DISCO_BRIGHTEN_MS)),
                         0.0, 1.0);
  }

  drawDimmer(static_cast<Uint8>(
      std::clamp(dim_factor * DISCO_MAX_DIM_ALPHA, 0.0, 255.0)));

  const int axis_x = screen_res_x / 2;
  const int axis_y = scene_origin_y + (rows * element_size) / 2;
  const double rotation_phase = std::fmod(disco.rotation_phase_turns, 1.0);
  if (!disco.is_ending) {
    drawDiscoLightSpots(axis_x, axis_y, rotation_phase, dim_in);
  }

  double drop = std::clamp(static_cast<double>(intro_elapsed) /
                               static_cast<double>(
                                   std::max<Uint32>(1, DISCO_BALL_DROP_MS)),
                           0.0, 1.0);
  if (disco.is_ending) {
    drop = 1.0 -
           std::clamp(static_cast<double>(now - disco.ending_started_ticks) /
                          static_cast<double>(
                              std::max<Uint32>(1, DISCO_BALL_RAISE_MS)),
                      0.0, 1.0);
  }

  const int radius =
      std::max(DISCO_BALL_MIN_RADIUS_PX,
               static_cast<int>(element_size * DISCO_BALL_RADIUS_CELLS));
  const int ball_y_top = -radius * 2;
  const int ball_y_down = scene_origin_y + std::max(radius + 18, element_size * 2);
  const double eased_drop = 1.0 - std::pow(1.0 - drop, 3.0);
  const int ball_y = static_cast<int>(
      std::lround(ball_y_top + (ball_y_down - ball_y_top) * eased_drop));

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
  const int chain_top = DISCO_CHAIN_TOP_Y;
  const double kDiscoAxisTiltRadians =
      DISCO_AXIS_TILT_DEGREES * kLogoPi / 180.0;
  const double axis_projection_y = std::cos(kDiscoAxisTiltRadians);
  const int chain_bottom =
      ball_y -
      static_cast<int>(std::lround(
          radius * DISCO_CHAIN_TOP_ATTACHMENT_RADIUS_Y_SCALE *
          axis_projection_y));
  const double chain_spin = -rotation_phase * 2.0 * kLogoPi;
  const int link_step =
      std::max(DISCO_CHAIN_LINK_MIN_STEP_PX,
               element_size / DISCO_CHAIN_LINK_STEP_CELL_DIVISOR);
  const int chain_depth =
      std::max(DISCO_CHAIN_DEPTH_MIN_PX,
               radius / DISCO_CHAIN_DEPTH_RADIUS_DIVISOR);
  int link_index = 0;
  for (int y = chain_top; y < chain_bottom; y += link_step / 2, ++link_index) {
    const double phase =
        chain_spin + link_index *
                         (kLogoPi * DISCO_CHAIN_LINK_PHASE_STEP_RADIANS_SCALE);
    const bool vertical_link = (link_index % 2) == 0;
    const double twist = std::sin(phase);
    const double facing = vertical_link ? (0.65 + 0.35 * std::cos(phase))
                                        : (0.65 + 0.35 * std::sin(phase));
    const int link_x =
        axis_x + static_cast<int>(std::lround(twist * chain_depth * 0.55));
    const int link_y = y + link_step / 2;
    const int base_w = std::max(
        DISCO_CHAIN_LINK_MIN_WIDTH_PX,
        static_cast<int>(std::lround(
            radius * (DISCO_CHAIN_LINK_WIDTH_BASE +
                      DISCO_CHAIN_LINK_WIDTH_FACING_SCALE * facing))));
    const int base_h = std::max(
        DISCO_CHAIN_LINK_MIN_HEIGHT_PX,
        static_cast<int>(std::lround(
            radius * (DISCO_CHAIN_LINK_HEIGHT_BASE -
                      DISCO_CHAIN_LINK_HEIGHT_FACING_SCALE * facing))));
    const int link_w = vertical_link ? base_w : std::max(base_w, base_h);
    const int link_h = vertical_link ? std::max(base_h, base_w) : base_w;
    const Uint8 back_alpha = static_cast<Uint8>(
        std::clamp(DISCO_CHAIN_BACK_ALPHA_BASE +
                       DISCO_CHAIN_BACK_ALPHA_SCALE * (1.0 - facing),
                   0.0, 190.0));
    const Uint8 metal_alpha = static_cast<Uint8>(
        std::clamp(DISCO_CHAIN_METAL_ALPHA_BASE +
                       DISCO_CHAIN_METAL_ALPHA_SCALE * facing,
                   0.0, 235.0));
    const Uint8 highlight_alpha = static_cast<Uint8>(
        std::clamp(DISCO_CHAIN_HIGHLIGHT_ALPHA_BASE +
                       DISCO_CHAIN_HIGHLIGHT_ALPHA_SCALE * facing,
                   0.0, 180.0));

    const int outer_rx =
        std::max(3, static_cast<int>(
                        std::lround(link_w *
                                    DISCO_CHAIN_OUTER_RADIUS_X_SCALE)));
    const int outer_ry =
        std::max(5, static_cast<int>(
                        std::lround(link_h *
                                    DISCO_CHAIN_OUTER_RADIUS_Y_SCALE)));
    const int inner_rx =
        std::max(1, static_cast<int>(
                        std::lround(link_w *
                                    DISCO_CHAIN_INNER_RADIUS_X_SCALE)));
    const int inner_ry =
        std::max(2, static_cast<int>(
                        std::lround(link_h *
                                    DISCO_CHAIN_INNER_RADIUS_Y_SCALE)));

    const auto draw_link_arc = [&](int cx, int cy, int rx, int ry,
                                   double start_angle, double end_angle,
                                   SDL_Color color, int thickness) {
      const int segments = 18;
      for (int segment = 0; segment < segments; ++segment) {
        const double t0 = static_cast<double>(segment) / segments;
        const double t1 = static_cast<double>(segment + 1) / segments;
        const double a0 = start_angle + (end_angle - start_angle) * t0;
        const double a1 = start_angle + (end_angle - start_angle) * t1;
        const double x0 = cx + std::cos(a0) * rx;
        const double y0 = cy + std::sin(a0) * ry;
        const double x1 = cx + std::cos(a1) * rx;
        const double y1 = cy + std::sin(a1) * ry;
        for (int band = -thickness; band <= thickness; ++band) {
          SDL_RenderDrawAALine(sdl_renderer, x0 + band * 0.45, y0,
                               x1 + band * 0.45, y1, color);
          SDL_RenderDrawAALine(sdl_renderer, x0, y0 + band * 0.45,
                               x1, y1 + band * 0.45, color);
        }
      }
    };

    const int thickness = std::max(2, std::min(outer_rx, outer_ry) / 4);
    const SDL_Color shadow_color{DISCO_CHAIN_SHADOW_COLOR.r,
                                 DISCO_CHAIN_SHADOW_COLOR.g,
                                 DISCO_CHAIN_SHADOW_COLOR.b, back_alpha};
    const SDL_Color metal_color{DISCO_CHAIN_METAL_COLOR.r,
                                DISCO_CHAIN_METAL_COLOR.g,
                                DISCO_CHAIN_METAL_COLOR.b, metal_alpha};
    const bool front_first = std::cos(phase) >= 0.0;
    const double front_start = vertical_link ? -0.08 * kLogoPi : 0.42 * kLogoPi;
    const double front_end = vertical_link ? 1.08 * kLogoPi : 1.58 * kLogoPi;
    const double back_start = front_end;
    const double back_end = front_start + 2.0 * kLogoPi;

    draw_link_arc(link_x + DISCO_CHAIN_SHADOW_OFFSET_X_PX,
                  link_y + DISCO_CHAIN_SHADOW_OFFSET_Y_PX, outer_rx, outer_ry,
                  0.0, 2.0 * kLogoPi, shadow_color, thickness);
    if (front_first) {
      draw_link_arc(link_x, link_y, outer_rx, outer_ry, back_start, back_end,
                    metal_color, thickness);
      draw_link_arc(link_x, link_y, outer_rx, outer_ry, front_start, front_end,
                    metal_color, thickness + 1);
    } else {
      draw_link_arc(link_x, link_y, outer_rx, outer_ry, front_start, front_end,
                    metal_color, thickness);
      draw_link_arc(link_x, link_y, outer_rx, outer_ry, back_start, back_end,
                    metal_color, thickness + 1);
    }
    SDL_RenderDrawAALine(
        sdl_renderer, link_x + link_w * DISCO_CHAIN_HIGHLIGHT_X0_SCALE,
        link_y + link_h * DISCO_CHAIN_HIGHLIGHT_Y0_SCALE,
        link_x + link_w * DISCO_CHAIN_HIGHLIGHT_X1_SCALE,
        link_y + link_h * DISCO_CHAIN_HIGHLIGHT_Y1_SCALE,
        {DISCO_CHAIN_HIGHLIGHT_COLOR.r, DISCO_CHAIN_HIGHLIGHT_COLOR.g,
         DISCO_CHAIN_HIGHLIGHT_COLOR.b, highlight_alpha});
    SDL_RenderDrawAALine(
        sdl_renderer, link_x + link_w * DISCO_CHAIN_SHADOW_LINE_X0_SCALE,
        link_y + link_h * DISCO_CHAIN_SHADOW_LINE_Y0_SCALE,
        link_x + link_w * DISCO_CHAIN_SHADOW_LINE_X1_SCALE,
        link_y + link_h * DISCO_CHAIN_SHADOW_LINE_Y1_SCALE,
        DISCO_CHAIN_SHADOW_LINE_COLOR);
  }
  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);

  drawDiscoBall(axis_x, ball_y, radius, rotation_phase, !disco.is_ending, 255);
}

void Renderer::drawPacmanShield(int center_x, int center_y, int base_radius,
                                double pulse_clock) {
  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double slow_pulse = 0.5 + 0.5 * std::sin(pulse_clock * 0.6);
  const int sphere_radius =
      base_radius + std::max(2, base_radius / 6) +
      static_cast<int>(std::round(slow_pulse * std::max(1, base_radius / 14)));

  for (int ring = 0; ring < 5; ++ring) {
    const int ring_radius = sphere_radius + ring + 1;
    const Uint8 alpha = static_cast<Uint8>(
        std::clamp(58.0 - ring * 11.0, 0.0, 80.0));
    SDL_SetRenderDrawColor(sdl_renderer, 96, 178, 255, alpha);
    SDL_RenderDrawCircle(sdl_renderer, center_x, center_y, ring_radius);
  }

  const int gradient_steps = 9;
  for (int step = 0; step < gradient_steps; ++step) {
    const double t = static_cast<double>(step) /
                     static_cast<double>(gradient_steps - 1);
    const int r = static_cast<int>(sphere_radius * (1.0 - t * 0.86));
    if (r < 1) {
      break;
    }
    const int offset_x =
        -static_cast<int>(std::round(t * sphere_radius * 0.22));
    const int offset_y =
        -static_cast<int>(std::round(t * sphere_radius * 0.22));
    const Uint8 red = static_cast<Uint8>(std::clamp(18.0 + t * 158.0, 0.0, 255.0));
    const Uint8 green = static_cast<Uint8>(std::clamp(82.0 + t * 152.0, 0.0, 255.0));
    const Uint8 blue = static_cast<Uint8>(std::clamp(210.0 + t * 45.0, 0.0, 255.0));
    const Uint8 alpha = static_cast<Uint8>(std::clamp(34.0 + t * 26.0, 0.0, 100.0));
    SDL_SetRenderDrawColor(sdl_renderer, red, green, blue, alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x + offset_x, center_y + offset_y, r);
  }

  const Uint8 rim_alpha = static_cast<Uint8>(
      std::clamp(150.0 + 70.0 * slow_pulse, 0.0, 230.0));
  SDL_SetRenderDrawColor(sdl_renderer, 178, 226, 255, rim_alpha);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y, sphere_radius);
  SDL_SetRenderDrawColor(sdl_renderer, 132, 198, 255, rim_alpha / 2);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(1, sphere_radius - 1));
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y, sphere_radius + 1);

  const int reflection_offset_y = static_cast<int>(sphere_radius * 0.62);
  const int reflection_radius = std::max(1, sphere_radius - 2);
  SDL_SetRenderDrawColor(sdl_renderer, 168, 220, 255, 100);
  for (int arc = 0; arc < 6; ++arc) {
    const double arc_angle = (3.14159265358979323846 * 0.5) +
                             (arc - 3) * (3.14159265358979323846 / 24.0);
    const int rx = center_x +
                   static_cast<int>(std::round(std::cos(arc_angle) * reflection_radius));
    const int ry = center_y +
                   static_cast<int>(std::round(std::sin(arc_angle) * reflection_radius * 0.92)) +
                   static_cast<int>(reflection_offset_y * 0.0);
    SDL_RenderDrawPoint(sdl_renderer, rx, ry);
    SDL_RenderDrawPoint(sdl_renderer, rx, ry + 1);
  }

  const int spec_cx = center_x -
                      static_cast<int>(std::round(sphere_radius * 0.40));
  const int spec_cy = center_y -
                      static_cast<int>(std::round(sphere_radius * 0.45));
  const int spec_outer = std::max(2, sphere_radius / 4);
  const int spec_mid = std::max(1, sphere_radius / 6);
  const int spec_core = std::max(1, sphere_radius / 9);
  SDL_SetRenderDrawColor(sdl_renderer, 200, 238, 255, 110);
  SDL_RenderFillCircle(sdl_renderer, spec_cx, spec_cy, spec_outer);
  SDL_SetRenderDrawColor(sdl_renderer, 224, 246, 255, 175);
  SDL_RenderFillCircle(sdl_renderer, spec_cx, spec_cy, spec_mid);
  SDL_SetRenderDrawColor(sdl_renderer, 248, 254, 255, 230);
  SDL_RenderFillCircle(sdl_renderer, spec_cx + 1, spec_cy + 1, spec_core);

  const int spec2_cx = center_x +
                       static_cast<int>(std::round(sphere_radius * 0.22));
  const int spec2_cy = center_y +
                       static_cast<int>(std::round(sphere_radius * 0.30));
  SDL_SetRenderDrawColor(sdl_renderer, 196, 232, 255, 70);
  SDL_RenderFillCircle(sdl_renderer, spec2_cx, spec2_cy,
                       std::max(1, sphere_radius / 10));

  const double shimmer_phase = pulse_clock * 0.45;
  const int shimmer_band_count = 18;
  for (int i = 0; i < shimmer_band_count; ++i) {
    const double a = shimmer_phase +
                     i * (2.0 * 3.14159265358979323846 / shimmer_band_count);
    const double radial_factor =
        0.92 + 0.04 * std::sin(a * 3.0 + pulse_clock * 0.8);
    const int rx = center_x +
                   static_cast<int>(std::round(std::cos(a) * sphere_radius * radial_factor));
    const int ry = center_y +
                   static_cast<int>(std::round(std::sin(a) * sphere_radius * radial_factor));
    const Uint8 alpha = static_cast<Uint8>(
        std::clamp(70.0 + 80.0 * (0.5 + 0.5 * std::sin(a * 2.0 + pulse_clock)),
                   0.0, 200.0));
    SDL_SetRenderDrawColor(sdl_renderer, 196, 234, 255, alpha);
    SDL_RenderDrawPoint(sdl_renderer, rx, ry);
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawElectrifiedMonsterAura(const SDL_Rect &monster_rect,
                                          int animation_seed, Uint32 now,
                                          bool aggressive_mode) {
  if (monster_rect.w <= 0 || monster_rect.h <= 0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const int center_x = monster_rect.x + monster_rect.w / 2;
  const int center_y = monster_rect.y + monster_rect.h / 2;
  const int core_radius = std::max(
      8, std::min(monster_rect.w, monster_rect.h) / 3 + std::max(2, element_size / 18));
  const int aura_radius =
      std::max(monster_rect.w, monster_rect.h) / 2 + std::max(9, element_size / 6);
  const int flicker_phase =
      static_cast<int>(now / (aggressive_mode ? ELECTRIFIED_ATTACK_FLICKER_FRAME_MS
                                              : 45)) +
      animation_seed * 19;
  const double pulse =
      aggressive_mode
          ? (0.58 + 0.42 *
                        (0.5 + 0.5 * std::sin(static_cast<double>(now + animation_seed * 31) /
                                              48.0)))
          : (0.72 + 0.28 *
                        std::sin(static_cast<double>(now + animation_seed * 31) /
                                 120.0));

  auto draw_lightning_segment = [&](int x1, int y1, int x2, int y2,
                                    Uint8 glow_alpha, Uint8 core_alpha) {
    SDL_SetRenderDrawColor(sdl_renderer, 34, 98, 255, glow_alpha / 2);
    SDL_RenderDrawLine(sdl_renderer, x1 - 1, y1, x2 - 1, y2);
    SDL_RenderDrawLine(sdl_renderer, x1 + 1, y1, x2 + 1, y2);
    SDL_RenderDrawLine(sdl_renderer, x1, y1 - 1, x2, y2 - 1);
    SDL_RenderDrawLine(sdl_renderer, x1, y1 + 1, x2, y2 + 1);

    SDL_SetRenderDrawColor(sdl_renderer, 84, 196, 255, glow_alpha);
    SDL_RenderDrawLine(sdl_renderer, x1, y1, x2, y2);
    SDL_SetRenderDrawColor(sdl_renderer, 236, 252, 255, core_alpha);
    SDL_RenderDrawLine(sdl_renderer, x1, y1, x2, y2);
  };

  SDL_SetRenderDrawColor(sdl_renderer, 18, 68, 255,
                         aggressive_mode ? 42 : 28);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(aura_radius, static_cast<int>(aura_radius * pulse)));
  SDL_SetRenderDrawColor(sdl_renderer, 90, 212, 255,
                         aggressive_mode ? 82 : 56);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(core_radius + 2,
                                static_cast<int>(core_radius * (1.10 + 0.10 * pulse))));
  SDL_SetRenderDrawColor(sdl_renderer, 140, 232, 255,
                         aggressive_mode ? 188 : 144);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                       std::max(core_radius + 4, static_cast<int>(
                                                     aura_radius *
                                                     (aggressive_mode ? 0.54 : 0.72))));

  const int bolt_count = aggressive_mode
                             ? (ELECTRIFIED_ATTACK_BOLT_MIN_COUNT +
                                std::abs(animation_seed %
                                         std::max(1, ELECTRIFIED_ATTACK_BOLT_MAX_COUNT -
                                                         ELECTRIFIED_ATTACK_BOLT_MIN_COUNT + 1)))
                             : (10 + std::abs(animation_seed % 4));
  for (int bolt = 0; bolt < bolt_count; ++bolt) {
    const int bolt_seed = flicker_phase * 131 + animation_seed * 29 + bolt * 17;
    const double angle =
        (2.0 * kLogoPi * static_cast<double>(bolt) / static_cast<double>(bolt_count)) +
        HashSigned(bolt_seed) * (aggressive_mode ? 0.28 : 0.45);
    const double unit_x = std::cos(angle);
    const double unit_y = std::sin(angle);
    const double normal_x = -unit_y;
    const double normal_y = unit_x;
    const int segment_count =
        aggressive_mode ? (3 + (bolt_seed % 3)) : (4 + (bolt_seed % 4));
    const double bolt_length = aura_radius *
                               (aggressive_mode
                                    ? (ELECTRIFIED_ATTACK_BOLT_LENGTH_MIN_FACTOR +
                                       (ELECTRIFIED_ATTACK_BOLT_LENGTH_MAX_FACTOR -
                                        ELECTRIFIED_ATTACK_BOLT_LENGTH_MIN_FACTOR) *
                                           HashUnit(bolt_seed + 5))
                                    : (0.72 + 0.68 * HashUnit(bolt_seed + 5)));
    const double start_radius = core_radius *
                                (aggressive_mode
                                     ? (0.28 + 0.20 * HashUnit(bolt_seed + 7))
                                     : (0.42 + 0.32 * HashUnit(bolt_seed + 7)));
    int previous_x = static_cast<int>(std::lround(center_x + unit_x * start_radius));
    int previous_y = static_cast<int>(std::lround(center_y + unit_y * start_radius));

    for (int segment = 1; segment <= segment_count; ++segment) {
      const double t =
          static_cast<double>(segment) / static_cast<double>(segment_count);
      const double base_distance = bolt_length * t;
      const double jagged_offset =
          HashSigned(bolt_seed + segment * 23) *
          (aggressive_mode
               ? (element_size * 0.05 + (1.0 - t) * aura_radius * 0.09)
               : (element_size * 0.08 + (1.0 - t) * aura_radius * 0.16));
      const double forward_offset =
          HashSigned(bolt_seed + segment * 41) *
          (aggressive_mode ? element_size * 0.02 : element_size * 0.04);
      const int point_x = static_cast<int>(std::lround(
          center_x + unit_x * (start_radius + base_distance + forward_offset) +
          normal_x * jagged_offset));
      const int point_y = static_cast<int>(std::lround(
          center_y + unit_y * (start_radius + base_distance + forward_offset) +
          normal_y * jagged_offset));
      const double flicker =
          aggressive_mode
              ? (0.22 + 0.78 *
                            (0.5 + 0.5 * std::sin(static_cast<double>(
                                                      now + bolt_seed * 9 +
                                                      segment * 31) /
                                                  18.0)))
              : 1.0;
      const Uint8 glow_alpha = static_cast<Uint8>(std::clamp(
          (196.0 - t * 88.0) * flicker,
          aggressive_mode ? 28.0 : 72.0,
          aggressive_mode ? 228.0 : 196.0));
      const Uint8 core_alpha = static_cast<Uint8>(std::clamp(
          (255.0 - t * 64.0) * (aggressive_mode ? (0.34 + 0.66 * flicker) : 1.0),
          aggressive_mode ? 54.0 : 118.0,
          255.0));
      draw_lightning_segment(previous_x, previous_y, point_x, point_y, glow_alpha,
                             core_alpha);

      if (segment < segment_count &&
          HashUnit(bolt_seed + segment * 59) >
              (aggressive_mode ? (1.0 - ELECTRIFIED_ATTACK_BRANCH_CHANCE) : 0.58)) {
        const double branch_angle =
            angle + HashSigned(bolt_seed + segment * 61) * 0.95;
        const double branch_length =
            bolt_length *
            (aggressive_mode
                 ? (0.08 + 0.10 * HashUnit(bolt_seed + segment * 67))
                 : (0.12 + 0.16 * HashUnit(bolt_seed + segment * 67))) *
            (1.0 - t);
        const int branch_x = static_cast<int>(std::lround(
            point_x + std::cos(branch_angle) * branch_length));
        const int branch_y = static_cast<int>(std::lround(
            point_y + std::sin(branch_angle) * branch_length));
        draw_lightning_segment(previous_x, previous_y, branch_x, branch_y,
                               std::max<Uint8>(aggressive_mode ? 34 : 48,
                                               glow_alpha / 2),
                               std::max<Uint8>(aggressive_mode ? 56 : 96,
                                               core_alpha / 2));
      }

      previous_x = point_x;
      previous_y = point_y;
    }

    SDL_SetRenderDrawColor(sdl_renderer, 206, 246, 255,
                           aggressive_mode ? 228 : 188);
    SDL_RenderFillCircle(sdl_renderer, previous_x, previous_y,
                         std::max(1, element_size / 18));
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawDwarf(const SDL_Rect &dwarf_rect,
                         Directions facing_direction, double gait_phase,
                         bool walking) {
  const double pi = 3.1415926535;
  const Directions resolved_facing =
      (facing_direction == Directions::None) ? Directions::Down
                                             : facing_direction;
  const bool facing_front = resolved_facing == Directions::Down;
  const bool facing_back = resolved_facing == Directions::Up;
  const int mirror = resolved_facing == Directions::Left ? -1 : 1;
  const int center_x = dwarf_rect.x + dwarf_rect.w / 2;
  const int center_y = dwarf_rect.y + dwarf_rect.h / 2;
  const double stride =
      walking ? std::sin(gait_phase) * dwarf_rect.w * 0.08 : 0.0;
  const double counter_stride =
      walking ? std::sin(gait_phase + pi) * dwarf_rect.w * 0.08 : 0.0;
  const double bob =
      walking ? std::sin(gait_phase * 2.0) * dwarf_rect.h * 0.018 : 0.0;
  const double arm_swing =
      walking ? std::sin(gait_phase + pi * 0.5) * dwarf_rect.h * 0.05 : 0.0;
  const double braid_sway =
      walking ? std::sin(gait_phase * 0.75) * dwarf_rect.w * 0.035 : 0.0;

  const SDL_Color outline{34, 24, 18, 255};
  const SDL_Color outline_soft{58, 45, 34, 255};
  const SDL_Color rim_light{246, 232, 196, 110};
  const SDL_Color gold_dark{104, 72, 24, 255};
  const SDL_Color gold{170, 126, 50, 255};
  const SDL_Color gold_light{224, 190, 104, 255};
  const SDL_Color steel_dark{76, 68, 64, 255};
  const SDL_Color steel{126, 114, 102, 255};
  const SDL_Color steel_light{210, 198, 180, 255};
  const SDL_Color leather_dark{68, 42, 20, 255};
  const SDL_Color leather{106, 66, 30, 255};
  const SDL_Color beard_dark{108, 52, 16, 255};
  const SDL_Color beard{164, 82, 28, 255};
  const SDL_Color beard_light{220, 140, 68, 255};
  const SDL_Color skin{214, 170, 120, 255};
  const SDL_Color eye{236, 238, 244, 255};
  const SDL_Color eye_glow{255, 216, 158, 255};
  const SDL_Color shadow{0, 0, 0, 255};

  auto setColor = [&](const SDL_Color &color, Uint8 alpha_override = 255) {
    SDL_SetRenderDrawColor(sdl_renderer, color.r, color.g, color.b,
                           alpha_override);
  };
  auto point = [&](double x, double y) {
    return SDL_Point{center_x + static_cast<int>(std::lround(x)),
                     center_y + static_cast<int>(std::lround(y + bob))};
  };
  auto sidePoint = [&](double x, double y) {
    return SDL_Point{center_x + static_cast<int>(std::lround(x * mirror)),
                     center_y + static_cast<int>(std::lround(y + bob))};
  };
  auto fillPoint = [&](const SDL_Point &point_value, int radius,
                       const SDL_Color &color,
                       Uint8 alpha_override = 255) {
    setColor(color, alpha_override);
    SDL_RenderFillCircle(sdl_renderer, point_value.x, point_value.y, radius);
  };
  auto outlinePoint = [&](const SDL_Point &point_value, int radius,
                          const SDL_Color &color,
                          Uint8 alpha_override = 255) {
    setColor(color, alpha_override);
    SDL_RenderDrawCircle(sdl_renderer, point_value.x, point_value.y, radius);
  };
  auto drawThickLine = [&](const SDL_Point &from, const SDL_Point &to,
                           int thickness, const SDL_Color &color,
                           Uint8 alpha_override = 255) {
    setColor(color, alpha_override);
    const int radius = std::max(0, thickness / 2);
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dy = -radius; dy <= radius; dy++) {
        if (dx * dx + dy * dy > radius * radius + 1) {
          continue;
        }
        SDL_RenderDrawLine(sdl_renderer, from.x + dx, from.y + dy, to.x + dx,
                           to.y + dy);
      }
    }
  };
  auto fillTriangle = [&](const SDL_Point &base_one, const SDL_Point &base_two,
                          const SDL_Point &tip, const SDL_Color &color,
                          Uint8 alpha_override = 255) {
    setColor(color, alpha_override);
    const int steps =
        std::max(10, std::max(std::abs(tip.y - base_one.y),
                              std::abs(tip.y - base_two.y)));
    for (int step = 0; step <= steps; step++) {
      const double t = static_cast<double>(step) / steps;
      const int x1 = base_one.x +
                     static_cast<int>(std::lround((tip.x - base_one.x) * t));
      const int y1 = base_one.y +
                     static_cast<int>(std::lround((tip.y - base_one.y) * t));
      const int x2 = base_two.x +
                     static_cast<int>(std::lround((tip.x - base_two.x) * t));
      const int y2 = base_two.y +
                     static_cast<int>(std::lround((tip.y - base_two.y) * t));
      SDL_RenderDrawLine(sdl_renderer, x1, y1, x2, y2);
    }
  };
  auto drawInsetRect = [&](const SDL_Rect &rect, const SDL_Color &fill_color,
                           const SDL_Color &border_color,
                           Uint8 fill_alpha = 255,
                           Uint8 border_alpha = 255) {
    if (rect.w <= 0 || rect.h <= 0) {
      return;
    }
    setColor(border_color, border_alpha);
    SDL_RenderFillRect(sdl_renderer, &rect);
    if (rect.w > 2 && rect.h > 2) {
      SDL_Rect inner{rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2};
      setColor(fill_color, fill_alpha);
      SDL_RenderFillRect(sdl_renderer, &inner);
    }
  };
  auto drawBoot = [&](const SDL_Point &ankle, int toe_direction, bool large,
                      bool shaded) {
    const int boot_width = large ? std::max(8, dwarf_rect.w / 9)
                                 : std::max(7, dwarf_rect.w / 10);
    const int boot_height = large ? std::max(6, dwarf_rect.h / 12)
                                  : std::max(5, dwarf_rect.h / 13);
    SDL_Rect boot_rect{ankle.x - boot_width / 2, ankle.y - boot_height / 2,
                       boot_width, boot_height};
    drawInsetRect(boot_rect, shaded ? leather_dark : leather, outline);
    fillPoint({ankle.x + toe_direction * std::max(2, boot_width / 3),
               ankle.y + boot_height / 6},
              std::max(2, boot_height / 2), shaded ? leather_dark : leather);
    outlinePoint({ankle.x + toe_direction * std::max(2, boot_width / 3),
                  ankle.y + boot_height / 6},
                 std::max(2, boot_height / 2), outline_soft);
    setColor(gold_light, 145);
    SDL_RenderDrawLine(sdl_renderer, boot_rect.x + 1, boot_rect.y + 1,
                       boot_rect.x + boot_rect.w - 2, boot_rect.y + 1);
  };
  auto drawGauntlet = [&](const SDL_Point &hand, bool highlighted) {
    fillPoint(hand, std::max(3, dwarf_rect.w / 16), outline);
    fillPoint(hand, std::max(2, dwarf_rect.w / 18),
              highlighted ? gold_light : gold);
    outlinePoint(hand, std::max(3, dwarf_rect.w / 16), outline_soft);
  };

  fillPoint({center_x, dwarf_rect.y + dwarf_rect.h - std::max(4, dwarf_rect.h / 12)},
            std::max(6, dwarf_rect.w / 4), shadow, 74);

  const int head_radius = std::max(6, dwarf_rect.w / 7);
  const int shoulder_radius = std::max(6, dwarf_rect.w / 7);
  const int limb_width = std::max(4, dwarf_rect.w / 12);
  const int body_width = std::max(18, static_cast<int>(dwarf_rect.w * 0.35));
  const int body_height = std::max(18, static_cast<int>(dwarf_rect.h * 0.30));

  if (facing_front) {
    const SDL_Point head = point(0, -dwarf_rect.h * 0.22);
    const SDL_Point left_shoulder = point(-dwarf_rect.w * 0.18, -dwarf_rect.h * 0.05);
    const SDL_Point right_shoulder = point(dwarf_rect.w * 0.18, -dwarf_rect.h * 0.05);
    const SDL_Point left_hip = point(-dwarf_rect.w * 0.08, dwarf_rect.h * 0.12);
    const SDL_Point right_hip = point(dwarf_rect.w * 0.08, dwarf_rect.h * 0.12);
    const SDL_Point left_knee = point(-dwarf_rect.w * 0.10 + stride * 0.35,
                                      dwarf_rect.h * 0.23);
    const SDL_Point right_knee = point(dwarf_rect.w * 0.10 + counter_stride * 0.35,
                                       dwarf_rect.h * 0.23);
    const SDL_Point left_foot = point(-dwarf_rect.w * 0.11 + stride * 0.55,
                                      dwarf_rect.h * 0.35);
    const SDL_Point right_foot = point(dwarf_rect.w * 0.11 + counter_stride * 0.55,
                                       dwarf_rect.h * 0.35);
    const SDL_Point left_elbow = point(-dwarf_rect.w * 0.23,
                                       dwarf_rect.h * (0.07 + arm_swing / dwarf_rect.h));
    const SDL_Point right_elbow = point(dwarf_rect.w * 0.23,
                                        dwarf_rect.h * (0.07 - arm_swing / dwarf_rect.h));
    const SDL_Point left_hand = point(-dwarf_rect.w * 0.17,
                                      dwarf_rect.h * (0.21 + arm_swing / dwarf_rect.h * 0.6));
    const SDL_Point right_hand =
        point(dwarf_rect.w * 0.17,
              dwarf_rect.h * (0.21 - arm_swing / dwarf_rect.h * 0.6));

    fillPoint(head, head_radius + 4, rim_light, 72);
    fillPoint(point(0, 0), std::max(10, body_width / 2 + 5), rim_light, 58);

    drawThickLine(left_hip, left_knee, limb_width + 2, outline);
    drawThickLine(left_knee, left_foot, limb_width + 2, outline);
    drawThickLine(right_hip, right_knee, limb_width + 2, outline);
    drawThickLine(right_knee, right_foot, limb_width + 2, outline);
    drawThickLine(left_hip, left_knee, limb_width, steel_dark);
    drawThickLine(left_knee, left_foot, limb_width, leather);
    drawThickLine(right_hip, right_knee, limb_width, steel_dark);
    drawThickLine(right_knee, right_foot, limb_width, leather);
    drawBoot(left_foot, -1, true, false);
    drawBoot(right_foot, 1, true, false);

    drawThickLine(left_shoulder, left_elbow, limb_width + 2, outline);
    drawThickLine(left_elbow, left_hand, limb_width + 2, outline);
    drawThickLine(right_shoulder, right_elbow, limb_width + 2, outline);
    drawThickLine(right_elbow, right_hand, limb_width + 2, outline);
    drawThickLine(left_shoulder, left_elbow, limb_width, leather_dark);
    drawThickLine(left_elbow, left_hand, limb_width, gold_dark);
    drawThickLine(right_shoulder, right_elbow, limb_width, leather_dark);
    drawThickLine(right_elbow, right_hand, limb_width, gold_dark);
    drawGauntlet(left_hand, false);
    drawGauntlet(right_hand, true);

    fillPoint(left_shoulder, shoulder_radius + 2, outline);
    fillPoint(right_shoulder, shoulder_radius + 2, outline);
    fillPoint(left_shoulder, shoulder_radius, gold_dark);
    fillPoint(right_shoulder, shoulder_radius, gold_dark);
    fillPoint(left_shoulder, std::max(4, shoulder_radius - 3), gold);
    fillPoint(right_shoulder, std::max(4, shoulder_radius - 3), gold);
    outlinePoint(left_shoulder, shoulder_radius, gold_light, 210);
    outlinePoint(right_shoulder, shoulder_radius, gold_light, 210);

    SDL_Rect torso{center_x - body_width / 2,
                   center_y - body_height / 3 + static_cast<int>(std::lround(bob)),
                   body_width, body_height};
    drawInsetRect(torso, steel, outline);
    SDL_Rect torso_highlight{torso.x + 3, torso.y + 2, torso.w - 6,
                             std::max(3, torso.h / 3)};
    drawInsetRect(torso_highlight, steel_light, gold_dark, 230, 170);
    SDL_Rect belt{center_x - body_width / 2 + 2,
                  center_y + static_cast<int>(dwarf_rect.h * 0.12 + bob),
                  body_width - 4, std::max(6, dwarf_rect.h / 13)};
    drawInsetRect(belt, leather_dark, outline);
    SDL_Rect buckle{center_x - std::max(4, dwarf_rect.w / 12),
                    belt.y - 1, std::max(8, dwarf_rect.w / 6),
                    belt.h + 2};
    drawInsetRect(buckle, gold_light, outline);
    SDL_Rect buckle_inner{buckle.x + 2, buckle.y + 2, std::max(2, buckle.w - 4),
                          std::max(2, buckle.h - 4)};
    drawInsetRect(buckle_inner, gold_dark, leather_dark);

    const SDL_Point beard_left =
        point(-dwarf_rect.w * 0.12, -dwarf_rect.h * 0.06);
    const SDL_Point beard_right =
        point(dwarf_rect.w * 0.12, -dwarf_rect.h * 0.06);
    const SDL_Point beard_tip =
        point(braid_sway, dwarf_rect.h * 0.25);
    fillTriangle(beard_left, beard_right, beard_tip, outline);
    fillTriangle(point(-dwarf_rect.w * 0.11, -dwarf_rect.h * 0.05),
                 point(dwarf_rect.w * 0.11, -dwarf_rect.h * 0.05),
                 point(braid_sway, dwarf_rect.h * 0.24), beard_dark);
    fillTriangle(point(-dwarf_rect.w * 0.08, -dwarf_rect.h * 0.04),
                 point(dwarf_rect.w * 0.08, -dwarf_rect.h * 0.04),
                 point(braid_sway * 0.8, dwarf_rect.h * 0.22), beard);
    drawThickLine(point(-dwarf_rect.w * 0.02, -dwarf_rect.h * 0.03),
                  point(braid_sway * 0.6, dwarf_rect.h * 0.21),
                  std::max(2, dwarf_rect.w / 18), beard_light, 210);
    drawThickLine(point(dwarf_rect.w * 0.03, -dwarf_rect.h * 0.02),
                  point(dwarf_rect.w * 0.07 + braid_sway * 0.2,
                        dwarf_rect.h * 0.17),
                  std::max(2, dwarf_rect.w / 20), beard_light, 180);
    drawThickLine(point(-dwarf_rect.w * 0.07, -dwarf_rect.h * 0.02),
                  point(-dwarf_rect.w * 0.08 + braid_sway * 0.2,
                        dwarf_rect.h * 0.17),
                  std::max(2, dwarf_rect.w / 20), beard_light, 180);

    fillPoint(head, head_radius + 2, outline);
    fillPoint(point(0, -dwarf_rect.h * 0.14), std::max(4, head_radius / 3), skin);
    fillPoint(head, head_radius, steel_dark);
    fillPoint(point(0, -dwarf_rect.h * 0.25), std::max(5, head_radius - 2),
              gold_dark);
    fillPoint(point(0, -dwarf_rect.h * 0.26), std::max(4, head_radius - 4),
              gold);
    SDL_Rect brow_band{center_x - head_radius - 2,
                       head.y - std::max(2, head_radius / 6),
                       head_radius * 2 + 4, std::max(7, head_radius / 2)};
    drawInsetRect(brow_band, gold, outline);
    SDL_Rect nose_guard{center_x - std::max(2, dwarf_rect.w / 30), head.y - 1,
                        std::max(4, dwarf_rect.w / 15),
                        std::max(12, dwarf_rect.h / 8)};
    drawInsetRect(nose_guard, gold, outline);
    SDL_Rect left_cheek{head.x - head_radius - 2, head.y - 1,
                        std::max(5, dwarf_rect.w / 11),
                        std::max(14, dwarf_rect.h / 7)};
    SDL_Rect right_cheek{head.x + head_radius - std::max(3, dwarf_rect.w / 18),
                         head.y - 1, std::max(5, dwarf_rect.w / 11),
                         std::max(14, dwarf_rect.h / 7)};
    drawInsetRect(left_cheek, steel, outline);
    drawInsetRect(right_cheek, steel, outline);
    fillPoint(point(-dwarf_rect.w * 0.05, -dwarf_rect.h * 0.17),
              std::max(2, dwarf_rect.w / 25), eye);
    fillPoint(point(dwarf_rect.w * 0.05, -dwarf_rect.h * 0.17),
              std::max(2, dwarf_rect.w / 25), eye);
    fillPoint(point(-dwarf_rect.w * 0.05, -dwarf_rect.h * 0.17), 1, eye_glow);
    fillPoint(point(dwarf_rect.w * 0.05, -dwarf_rect.h * 0.17), 1, eye_glow);

    outlinePoint(head, head_radius + 1, rim_light, 130);
    outlinePoint(point(0, 0), std::max(8, body_width / 2 + 1), rim_light, 86);
  } else if (facing_back) {
    const SDL_Point head = point(0, -dwarf_rect.h * 0.21);
    const SDL_Point left_shoulder = point(-dwarf_rect.w * 0.17, -dwarf_rect.h * 0.05);
    const SDL_Point right_shoulder = point(dwarf_rect.w * 0.17, -dwarf_rect.h * 0.05);
    const SDL_Point left_hip = point(-dwarf_rect.w * 0.08, dwarf_rect.h * 0.11);
    const SDL_Point right_hip = point(dwarf_rect.w * 0.08, dwarf_rect.h * 0.11);
    const SDL_Point left_knee = point(-dwarf_rect.w * 0.10 + stride * 0.32,
                                      dwarf_rect.h * 0.23);
    const SDL_Point right_knee = point(dwarf_rect.w * 0.10 + counter_stride * 0.32,
                                       dwarf_rect.h * 0.23);
    const SDL_Point left_foot = point(-dwarf_rect.w * 0.11 + stride * 0.48,
                                      dwarf_rect.h * 0.35);
    const SDL_Point right_foot = point(dwarf_rect.w * 0.11 + counter_stride * 0.48,
                                       dwarf_rect.h * 0.35);
    const SDL_Point left_hand =
        point(-dwarf_rect.w * 0.17, dwarf_rect.h * (0.18 + arm_swing / dwarf_rect.h));
    const SDL_Point right_hand =
        point(dwarf_rect.w * 0.17,
              dwarf_rect.h * (0.18 - arm_swing / dwarf_rect.h));

    fillPoint(head, head_radius + 4, rim_light, 68);
    fillPoint(point(0, 0), std::max(10, body_width / 2 + 4), rim_light, 52);

    drawThickLine(left_hip, left_knee, limb_width + 2, outline);
    drawThickLine(left_knee, left_foot, limb_width + 2, outline);
    drawThickLine(right_hip, right_knee, limb_width + 2, outline);
    drawThickLine(right_knee, right_foot, limb_width + 2, outline);
    drawThickLine(left_hip, left_knee, limb_width, steel_dark);
    drawThickLine(left_knee, left_foot, limb_width, leather);
    drawThickLine(right_hip, right_knee, limb_width, steel_dark);
    drawThickLine(right_knee, right_foot, limb_width, leather);
    drawBoot(left_foot, -1, false, true);
    drawBoot(right_foot, 1, false, true);

    drawThickLine(left_shoulder, left_hand, limb_width + 2, outline);
    drawThickLine(right_shoulder, right_hand, limb_width + 2, outline);
    drawThickLine(left_shoulder, left_hand, limb_width, leather_dark);
    drawThickLine(right_shoulder, right_hand, limb_width, leather_dark);
    drawGauntlet(left_hand, false);
    drawGauntlet(right_hand, false);

    fillPoint(left_shoulder, shoulder_radius + 2, outline);
    fillPoint(right_shoulder, shoulder_radius + 2, outline);
    fillPoint(left_shoulder, shoulder_radius, gold_dark);
    fillPoint(right_shoulder, shoulder_radius, gold_dark);
    fillPoint(left_shoulder, std::max(4, shoulder_radius - 4), gold);
    fillPoint(right_shoulder, std::max(4, shoulder_radius - 4), gold);

    SDL_Rect backplate{center_x - body_width / 2,
                       center_y - body_height / 3 + static_cast<int>(std::lround(bob)),
                       body_width, body_height};
    drawInsetRect(backplate, steel_dark, outline);
    SDL_Rect cloak_band{backplate.x + 2, backplate.y + 2, backplate.w - 4,
                        std::max(6, backplate.h / 4)};
    drawInsetRect(cloak_band, gold, outline);
    SDL_Rect belt{center_x - body_width / 2 + 2,
                  center_y + static_cast<int>(dwarf_rect.h * 0.12 + bob),
                  body_width - 4, std::max(6, dwarf_rect.h / 13)};
    drawInsetRect(belt, leather_dark, outline);
    SDL_Rect pouch{center_x - std::max(5, dwarf_rect.w / 12),
                   belt.y + std::max(1, belt.h / 4), std::max(10, dwarf_rect.w / 7),
                   std::max(8, dwarf_rect.h / 10)};
    drawInsetRect(pouch, leather, outline);

    fillPoint(head, head_radius + 2, outline);
    fillPoint(head, head_radius, steel_dark);
    fillPoint(point(0, -dwarf_rect.h * 0.25), std::max(5, head_radius - 2),
              gold_dark);
    fillPoint(point(0, -dwarf_rect.h * 0.26), std::max(4, head_radius - 4),
              gold);
    SDL_Rect back_rim{center_x - head_radius - 1, head.y + head_radius / 4,
                      head_radius * 2 + 2, std::max(6, dwarf_rect.h / 14)};
    drawInsetRect(back_rim, steel, outline);
    drawThickLine(point(0, -dwarf_rect.h * 0.12),
                  point(braid_sway * 0.35, dwarf_rect.h * 0.20),
                  std::max(6, dwarf_rect.w / 10), outline);
    drawThickLine(point(0, -dwarf_rect.h * 0.12),
                  point(braid_sway * 0.30, dwarf_rect.h * 0.20),
                  std::max(5, dwarf_rect.w / 11), beard_dark);
    drawThickLine(point(0, -dwarf_rect.h * 0.11),
                  point(braid_sway * 0.28, dwarf_rect.h * 0.18),
                  std::max(3, dwarf_rect.w / 14), beard);
    drawThickLine(point(0, -dwarf_rect.h * 0.10),
                  point(braid_sway * 0.24, dwarf_rect.h * 0.14),
                  std::max(2, dwarf_rect.w / 18), beard_light, 200);
    outlinePoint(head, head_radius + 1, rim_light, 118);
  } else {
    const SDL_Point head = sidePoint(dwarf_rect.w * 0.01, -dwarf_rect.h * 0.21);
    const SDL_Point front_shoulder =
        sidePoint(dwarf_rect.w * 0.04, -dwarf_rect.h * 0.06);
    const SDL_Point back_shoulder =
        sidePoint(-dwarf_rect.w * 0.08, -dwarf_rect.h * 0.04);
    const SDL_Point front_hip = sidePoint(dwarf_rect.w * 0.03, dwarf_rect.h * 0.11);
    const SDL_Point back_hip = sidePoint(-dwarf_rect.w * 0.06, dwarf_rect.h * 0.11);
    const SDL_Point front_knee =
        sidePoint(dwarf_rect.w * 0.05 + stride * 0.35, dwarf_rect.h * 0.23);
    const SDL_Point back_knee =
        sidePoint(-dwarf_rect.w * 0.03 + counter_stride * 0.28,
                  dwarf_rect.h * 0.23);
    const SDL_Point front_foot =
        sidePoint(dwarf_rect.w * 0.10 + stride * 0.5, dwarf_rect.h * 0.35);
    const SDL_Point back_foot =
        sidePoint(-dwarf_rect.w * 0.02 + counter_stride * 0.38,
                  dwarf_rect.h * 0.34);
    const SDL_Point front_elbow =
        sidePoint(dwarf_rect.w * 0.15, dwarf_rect.h * (0.05 - arm_swing / dwarf_rect.h));
    const SDL_Point front_hand =
        sidePoint(dwarf_rect.w * 0.12, dwarf_rect.h * (0.18 - arm_swing / dwarf_rect.h * 0.7));
    const SDL_Point back_elbow =
        sidePoint(-dwarf_rect.w * 0.12, dwarf_rect.h * (0.07 + arm_swing / dwarf_rect.h * 0.6));
    const SDL_Point back_hand =
        sidePoint(-dwarf_rect.w * 0.08, dwarf_rect.h * (0.18 + arm_swing / dwarf_rect.h * 0.45));
    const SDL_Point torso = sidePoint(-dwarf_rect.w * 0.02, 0.01 * dwarf_rect.h);

    fillPoint(head, head_radius + 4, rim_light, 74);
    fillPoint(torso, std::max(10, body_width / 2 + 4), rim_light, 56);

    drawThickLine(back_hip, back_knee, limb_width + 1, outline);
    drawThickLine(back_knee, back_foot, limb_width + 1, outline);
    drawThickLine(back_hip, back_knee, std::max(3, limb_width - 1), steel_dark);
    drawThickLine(back_knee, back_foot, std::max(3, limb_width - 1), leather_dark);
    drawBoot(back_foot, mirror, false, true);

    drawThickLine(back_shoulder, back_elbow, limb_width + 1, outline);
    drawThickLine(back_elbow, back_hand, limb_width + 1, outline);
    drawThickLine(back_shoulder, back_elbow, std::max(3, limb_width - 1),
                  leather_dark);
    drawThickLine(back_elbow, back_hand, std::max(3, limb_width - 1), gold_dark);
    drawGauntlet(back_hand, false);

    fillPoint(back_shoulder, shoulder_radius, outline);
    fillPoint(back_shoulder, std::max(5, shoulder_radius - 1), gold_dark);
    fillPoint(back_shoulder, std::max(3, shoulder_radius - 4), gold);

    fillPoint(sidePoint(-dwarf_rect.w * 0.03, dwarf_rect.h * 0.02),
              std::max(10, body_width / 2), outline);
    fillPoint(sidePoint(-dwarf_rect.w * 0.03, dwarf_rect.h * 0.02),
              std::max(9, body_width / 2 - 1), steel_dark);
    fillPoint(torso, std::max(8, body_width / 2 - 2), steel);
    fillPoint(front_shoulder, shoulder_radius + 2, outline);
    fillPoint(front_shoulder, shoulder_radius, gold_dark);
    fillPoint(front_shoulder, std::max(4, shoulder_radius - 3), gold);
    outlinePoint(front_shoulder, shoulder_radius, gold_light, 210);

    SDL_Rect belt{center_x - std::max(10, dwarf_rect.w / 9),
                  center_y + static_cast<int>(dwarf_rect.h * 0.12 + bob),
                  std::max(16, dwarf_rect.w / 4), std::max(6, dwarf_rect.h / 13)};
    drawInsetRect(belt, leather_dark, outline);
    SDL_Rect buckle{belt.x + belt.w - std::max(8, dwarf_rect.w / 9),
                    belt.y - 1, std::max(8, dwarf_rect.w / 8), belt.h + 2};
    drawInsetRect(buckle, gold_light, outline);

    drawThickLine(front_hip, front_knee, limb_width + 2, outline);
    drawThickLine(front_knee, front_foot, limb_width + 2, outline);
    drawThickLine(front_hip, front_knee, limb_width, steel_dark);
    drawThickLine(front_knee, front_foot, limb_width, leather);
    drawBoot(front_foot, mirror, true, false);

    drawThickLine(front_shoulder, front_elbow, limb_width + 2, outline);
    drawThickLine(front_elbow, front_hand, limb_width + 2, outline);
    drawThickLine(front_shoulder, front_elbow, limb_width, leather_dark);
    drawThickLine(front_elbow, front_hand, limb_width, gold_dark);
    drawGauntlet(front_hand, true);

    fillPoint(head, head_radius + 2, outline);
    fillPoint(head, head_radius, steel_dark);
    fillPoint(sidePoint(dwarf_rect.w * 0.03, -dwarf_rect.h * 0.25),
              std::max(5, head_radius - 2), gold_dark);
    fillPoint(sidePoint(dwarf_rect.w * 0.03, -dwarf_rect.h * 0.26),
              std::max(4, head_radius - 4), gold);
    SDL_Rect brow_band{head.x - head_radius / 2,
                       head.y - std::max(2, head_radius / 6),
                       head_radius + std::max(6, dwarf_rect.w / 11),
                       std::max(7, head_radius / 2)};
    if (mirror < 0) {
      brow_band.x = head.x - brow_band.w + head_radius / 2;
    }
    drawInsetRect(brow_band, gold, outline);
    SDL_Rect cheek_guard{head.x + mirror * (head_radius / 3),
                         head.y + std::max(1, head_radius / 8),
                         std::max(5, dwarf_rect.w / 10),
                         std::max(13, dwarf_rect.h / 7)};
    if (mirror < 0) {
      cheek_guard.x -= cheek_guard.w;
    }
    drawInsetRect(cheek_guard, steel, outline);
    fillPoint(sidePoint(dwarf_rect.w * 0.07, -dwarf_rect.h * 0.17),
              std::max(2, dwarf_rect.w / 25), eye);
    fillPoint(sidePoint(dwarf_rect.w * 0.07, -dwarf_rect.h * 0.17), 1, eye_glow);
    fillPoint(sidePoint(dwarf_rect.w * 0.10, -dwarf_rect.h * 0.14),
              std::max(3, dwarf_rect.w / 22), skin);
    const SDL_Point beard_root =
        sidePoint(dwarf_rect.w * 0.05, -dwarf_rect.h * 0.08);
    const SDL_Point beard_tip = sidePoint(dwarf_rect.w * 0.14 + braid_sway * 0.15,
                                          dwarf_rect.h * 0.24);
    const SDL_Point beard_back =
        sidePoint(-dwarf_rect.w * 0.02, dwarf_rect.h * 0.09);
    fillTriangle(beard_root, beard_back, beard_tip, outline);
    fillTriangle(sidePoint(dwarf_rect.w * 0.05, -dwarf_rect.h * 0.07),
                 sidePoint(-dwarf_rect.w * 0.01, dwarf_rect.h * 0.08), beard_tip,
                 beard_dark);
    fillTriangle(sidePoint(dwarf_rect.w * 0.06, -dwarf_rect.h * 0.06),
                 sidePoint(0, dwarf_rect.h * 0.06),
                 sidePoint(dwarf_rect.w * 0.13 + braid_sway * 0.12,
                           dwarf_rect.h * 0.22),
                 beard);
    drawThickLine(sidePoint(dwarf_rect.w * 0.06, -dwarf_rect.h * 0.05),
                  sidePoint(dwarf_rect.w * 0.11 + braid_sway * 0.08,
                            dwarf_rect.h * 0.18),
                  std::max(2, dwarf_rect.w / 18), beard_light, 210);

    outlinePoint(head, head_radius + 1, rim_light, 124);
    outlinePoint(torso, std::max(8, body_width / 2 - 2), rim_light, 86);
  }
}

void Renderer::drawExplosionParticles() {
  if (game == nullptr) {
    return;
  }
  if (game->explosion_particles.empty() &&
      game->explosion_smoke_puffs.empty()) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  SDL_Texture *const puff_texture = sdl_soft_puff_texture;
  std::vector<SDL_Vertex> puff_vertices;
  std::vector<int> puff_indices;
  if (puff_texture != nullptr) {
    const size_t expected_blobs =
        game->explosion_smoke_puffs.size() * 10 +
        game->explosion_particles.size() * 6;
    puff_vertices.reserve(expected_blobs * 4);
    puff_indices.reserve(expected_blobs * 6);
  }
  auto draw_soft_puff = [&](float cx, float cy, float radius, Uint8 r, Uint8 g,
                            Uint8 b, Uint8 a) {
    if (puff_texture == nullptr || a == 0) {
      return;
    }
    const float half = std::max(1.0f, radius);
    const SDL_Color color{r, g, b, a};
    const int base = static_cast<int>(puff_vertices.size());
    puff_vertices.push_back(SDL_Vertex{SDL_FPoint{cx - half, cy - half}, color,
                                       SDL_FPoint{0.0f, 0.0f}});
    puff_vertices.push_back(SDL_Vertex{SDL_FPoint{cx + half, cy - half}, color,
                                       SDL_FPoint{1.0f, 0.0f}});
    puff_vertices.push_back(SDL_Vertex{SDL_FPoint{cx + half, cy + half}, color,
                                       SDL_FPoint{1.0f, 1.0f}});
    puff_vertices.push_back(SDL_Vertex{SDL_FPoint{cx - half, cy + half}, color,
                                       SDL_FPoint{0.0f, 1.0f}});
    puff_indices.push_back(base + 0);
    puff_indices.push_back(base + 1);
    puff_indices.push_back(base + 2);
    puff_indices.push_back(base + 0);
    puff_indices.push_back(base + 2);
    puff_indices.push_back(base + 3);
  };

  const float cell_to_px = static_cast<float>(element_size + 1);
  const float rocket_trail_screen_lift_px =
      ENABLE_3D_VIEW
          ? static_cast<float>(
                getNonCharacterSpriteLiftPixels() +
                std::max(1, static_cast<int>(std::lround(
                                static_cast<double>(element_size) *
                                ROCKET_FLIGHT_EXTRA_LIFT_FACTOR))))
          : 0.0f;

  struct SmokeRenderTuning {
    Uint32 lifetime_ms;
    float initial_radius_cells;
    float final_radius_cells;
    Uint8 initial_alpha;
    SDL_Color base_color;
    SDL_Color highlight_color;
    int blob_min_count;
    int blob_max_count;
    float blob_offset_factor;
    float blob_radius_min_factor;
    float blob_radius_max_factor;
    float wobble_amplitude_cells;
    float wobble_frequency_hz;
    float vertical_rise_cells;
  };
  auto get_smoke_tuning = [](ExplosionSmokePuffKind kind) {
    switch (kind) {
    case ExplosionSmokePuffKind::BlastSmoke:
      return SmokeRenderTuning{
          EXPLOSION_SMOKE_CLOUD_LIFETIME_MS,
          EXPLOSION_SMOKE_CLOUD_INITIAL_RADIUS_CELLS,
          EXPLOSION_SMOKE_CLOUD_FINAL_RADIUS_CELLS,
          EXPLOSION_SMOKE_CLOUD_INITIAL_ALPHA,
          EXPLOSION_SMOKE_CLOUD_COLOR,
          EXPLOSION_SMOKE_CLOUD_HIGHLIGHT_COLOR,
          EXPLOSION_SMOKE_CLOUD_BLOB_MIN_COUNT,
          EXPLOSION_SMOKE_CLOUD_BLOB_MAX_COUNT,
          EXPLOSION_SMOKE_CLOUD_BLOB_OFFSET_FACTOR,
          EXPLOSION_SMOKE_CLOUD_BLOB_RADIUS_MIN_FACTOR,
          EXPLOSION_SMOKE_CLOUD_BLOB_RADIUS_MAX_FACTOR,
          EXPLOSION_SMOKE_CLOUD_WOBBLE_AMPLITUDE_CELLS,
          EXPLOSION_SMOKE_CLOUD_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::RocketBlastSmoke:
      return SmokeRenderTuning{
          ROCKET_BLAST_SMOKE_LIFETIME_MS,
          ROCKET_BLAST_SMOKE_INITIAL_RADIUS_CELLS,
          ROCKET_BLAST_SMOKE_FINAL_RADIUS_CELLS,
          ROCKET_BLAST_SMOKE_INITIAL_ALPHA,
          ROCKET_BLAST_SMOKE_COLOR,
          ROCKET_BLAST_SMOKE_HIGHLIGHT_COLOR,
          ROCKET_BLAST_SMOKE_BLOB_MIN_COUNT,
          ROCKET_BLAST_SMOKE_BLOB_MAX_COUNT,
          ROCKET_BLAST_SMOKE_BLOB_OFFSET_FACTOR,
          ROCKET_BLAST_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          ROCKET_BLAST_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          ROCKET_BLAST_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          ROCKET_BLAST_SMOKE_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::RocketTrailSmoke:
      return SmokeRenderTuning{
          ROCKET_TRAIL_SMOKE_LIFETIME_MS,
          ROCKET_TRAIL_SMOKE_INITIAL_RADIUS_CELLS,
          ROCKET_TRAIL_SMOKE_FINAL_RADIUS_CELLS,
          ROCKET_TRAIL_SMOKE_INITIAL_ALPHA,
          ROCKET_TRAIL_SMOKE_COLOR,
          ROCKET_TRAIL_SMOKE_HIGHLIGHT_COLOR,
          ROCKET_TRAIL_SMOKE_BLOB_MIN_COUNT,
          ROCKET_TRAIL_SMOKE_BLOB_MAX_COUNT,
          ROCKET_TRAIL_SMOKE_BLOB_OFFSET_FACTOR,
          ROCKET_TRAIL_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          ROCKET_TRAIL_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          ROCKET_TRAIL_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          ROCKET_TRAIL_SMOKE_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::NuclearRingSmoke:
      return SmokeRenderTuning{
          NUCLEAR_RING_SMOKE_LIFETIME_MS,
          NUCLEAR_RING_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_RING_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_RING_SMOKE_INITIAL_ALPHA,
          NUCLEAR_RING_SMOKE_COLOR,
          NUCLEAR_RING_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_RING_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_RING_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_RING_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_RING_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_RING_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_RING_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_RING_SMOKE_WOBBLE_FREQUENCY_HZ,
          NUCLEAR_RING_SMOKE_VERTICAL_RISE_CELLS};
    case ExplosionSmokePuffKind::NuclearCoreSmoke:
      return SmokeRenderTuning{
          NUCLEAR_CORE_SMOKE_LIFETIME_MS,
          NUCLEAR_CORE_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_CORE_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_CORE_SMOKE_INITIAL_ALPHA,
          NUCLEAR_CORE_SMOKE_COLOR,
          NUCLEAR_CORE_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_CORE_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_CORE_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_CORE_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_CORE_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_CORE_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_CORE_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_CORE_SMOKE_WOBBLE_FREQUENCY_HZ,
          NUCLEAR_CORE_SMOKE_VERTICAL_RISE_CELLS};
    case ExplosionSmokePuffKind::NuclearMushroomSmoke:
      return SmokeRenderTuning{
          NUCLEAR_MUSHROOM_SMOKE_LIFETIME_MS,
          NUCLEAR_MUSHROOM_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_MUSHROOM_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_MUSHROOM_SMOKE_INITIAL_ALPHA,
          NUCLEAR_MUSHROOM_SMOKE_COLOR,
          NUCLEAR_MUSHROOM_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_MUSHROOM_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_MUSHROOM_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_MUSHROOM_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_MUSHROOM_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_MUSHROOM_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_MUSHROOM_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_MUSHROOM_SMOKE_WOBBLE_FREQUENCY_HZ,
          NUCLEAR_MUSHROOM_SMOKE_VERTICAL_RISE_CELLS};
    case ExplosionSmokePuffKind::NuclearBRingSmoke:
      return SmokeRenderTuning{
          NUCLEAR_B_RING_SMOKE_LIFETIME_MS,
          NUCLEAR_B_RING_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_B_RING_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_B_RING_SMOKE_INITIAL_ALPHA,
          NUCLEAR_B_RING_SMOKE_COLOR,
          NUCLEAR_B_RING_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_B_RING_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_B_RING_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_B_RING_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_B_RING_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_B_RING_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_B_RING_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_B_RING_SMOKE_WOBBLE_FREQUENCY_HZ,
          NUCLEAR_B_RING_SMOKE_VERTICAL_RISE_CELLS};
    case ExplosionSmokePuffKind::NuclearBTrailSmoke:
      return SmokeRenderTuning{
          NUCLEAR_B_TRAIL_SMOKE_LIFETIME_MS,
          NUCLEAR_B_TRAIL_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_B_TRAIL_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_B_TRAIL_SMOKE_INITIAL_ALPHA,
          NUCLEAR_B_TRAIL_SMOKE_COLOR,
          NUCLEAR_B_TRAIL_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_B_TRAIL_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_B_TRAIL_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_B_TRAIL_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_B_TRAIL_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_B_TRAIL_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_B_TRAIL_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_B_TRAIL_SMOKE_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::NuclearBStemSmoke:
      return SmokeRenderTuning{
          NUCLEAR_B_STEM_SMOKE_LIFETIME_MS,
          NUCLEAR_B_STEM_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_B_STEM_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_B_STEM_SMOKE_INITIAL_ALPHA,
          NUCLEAR_B_STEM_SMOKE_COLOR,
          NUCLEAR_B_STEM_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_B_STEM_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_B_STEM_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_B_STEM_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_B_STEM_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_B_STEM_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_B_STEM_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_B_STEM_SMOKE_WOBBLE_FREQUENCY_HZ,
          NUCLEAR_B_STEM_SMOKE_VERTICAL_RISE_CELLS};
    case ExplosionSmokePuffKind::NuclearBCapSmoke:
      return SmokeRenderTuning{
          NUCLEAR_B_CAP_SMOKE_LIFETIME_MS,
          NUCLEAR_B_CAP_SMOKE_INITIAL_RADIUS_CELLS,
          NUCLEAR_B_CAP_SMOKE_FINAL_RADIUS_CELLS,
          NUCLEAR_B_CAP_SMOKE_INITIAL_ALPHA,
          NUCLEAR_B_CAP_SMOKE_COLOR,
          NUCLEAR_B_CAP_SMOKE_HIGHLIGHT_COLOR,
          NUCLEAR_B_CAP_SMOKE_BLOB_MIN_COUNT,
          NUCLEAR_B_CAP_SMOKE_BLOB_MAX_COUNT,
          NUCLEAR_B_CAP_SMOKE_BLOB_OFFSET_FACTOR,
          NUCLEAR_B_CAP_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_B_CAP_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_B_CAP_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_B_CAP_SMOKE_WOBBLE_FREQUENCY_HZ,
          NUCLEAR_B_CAP_SMOKE_VERTICAL_RISE_CELLS};
    case ExplosionSmokePuffKind::WallDust:
      return SmokeRenderTuning{
          PLASTIC_EXPLOSIVE_WALL_DUST_LIFETIME_MS,
          PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_RADIUS_CELLS,
          PLASTIC_EXPLOSIVE_WALL_DUST_FINAL_RADIUS_CELLS,
          PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_ALPHA,
          PLASTIC_EXPLOSIVE_WALL_DUST_COLOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_HIGHLIGHT_COLOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_MIN_COUNT,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_MAX_COUNT,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_OFFSET_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_RADIUS_MIN_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_RADIUS_MAX_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_WOBBLE_AMPLITUDE_CELLS,
          PLASTIC_EXPLOSIVE_WALL_DUST_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::NuclearDust:
      return SmokeRenderTuning{
          NUCLEAR_BOMB_DUST_LIFETIME_MS,
          NUCLEAR_BOMB_DUST_INITIAL_RADIUS_CELLS,
          NUCLEAR_BOMB_DUST_FINAL_RADIUS_CELLS,
          NUCLEAR_BOMB_DUST_INITIAL_ALPHA,
          NUCLEAR_BOMB_DUST_COLOR,
          NUCLEAR_BOMB_DUST_HIGHLIGHT_COLOR,
          NUCLEAR_BOMB_DUST_BLOB_MIN_COUNT,
          NUCLEAR_BOMB_DUST_BLOB_MAX_COUNT,
          NUCLEAR_BOMB_DUST_BLOB_OFFSET_FACTOR,
          NUCLEAR_BOMB_DUST_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_BOMB_DUST_BLOB_RADIUS_MAX_FACTOR,
          NUCLEAR_BOMB_DUST_WOBBLE_AMPLITUDE_CELLS,
          NUCLEAR_BOMB_DUST_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::GoatRedDust:
      return SmokeRenderTuning{
          2400,
          PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_RADIUS_CELLS,
          PLASTIC_EXPLOSIVE_WALL_DUST_FINAL_RADIUS_CELLS * 1.4f,
          240,
          SDL_Color{170, 40, 32, 255},
          SDL_Color{220, 90, 70, 255},
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_MIN_COUNT,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_MAX_COUNT,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_OFFSET_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_RADIUS_MIN_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_RADIUS_MAX_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DUST_WOBBLE_AMPLITUDE_CELLS * 1.4f,
          PLASTIC_EXPLOSIVE_WALL_DUST_WOBBLE_FREQUENCY_HZ,
          0.15f};
    case ExplosionSmokePuffKind::MonsterDustCloud:
      return SmokeRenderTuning{
          GOAT_MONSTER_DUST_LIFETIME_MS,
          MONSTER_EXPLOSION_SMOKE_INITIAL_RADIUS_CELLS * 1.4f,
          MONSTER_EXPLOSION_SMOKE_FINAL_RADIUS_CELLS * 1.8f,
          190,
          SDL_Color{82, 80, 78, 255},
          SDL_Color{150, 146, 140, 255},
          MONSTER_EXPLOSION_SMOKE_BLOB_MIN_COUNT,
          MONSTER_EXPLOSION_SMOKE_BLOB_MAX_COUNT + 2,
          MONSTER_EXPLOSION_SMOKE_BLOB_OFFSET_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MAX_FACTOR * 1.15f,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_AMPLITUDE_CELLS * 1.6f,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_FREQUENCY_HZ * 0.65f,
          0.05f};
    case ExplosionSmokePuffKind::LoveSmokeTrail:
      return SmokeRenderTuning{
          LOVE_SMOKE_TRAIL_LIFETIME_MS,
          LOVE_SMOKE_TRAIL_INITIAL_RADIUS_CELLS,
          LOVE_SMOKE_TRAIL_FINAL_RADIUS_CELLS,
          LOVE_SMOKE_TRAIL_INITIAL_ALPHA,
          LOVE_SMOKE_COLOR,
          LOVE_SMOKE_HIGHLIGHT_COLOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_MIN_COUNT,
          MONSTER_EXPLOSION_SMOKE_BLOB_MAX_COUNT,
          MONSTER_EXPLOSION_SMOKE_BLOB_OFFSET_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_AMPLITUDE_CELLS * 1.2f,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_FREQUENCY_HZ,
          0.0f};
    case ExplosionSmokePuffKind::LoveSmokeImpact:
      return SmokeRenderTuning{
          LOVE_SMOKE_IMPACT_LIFETIME_MS,
          LOVE_SMOKE_IMPACT_INITIAL_RADIUS_CELLS,
          LOVE_SMOKE_IMPACT_FINAL_RADIUS_CELLS,
          LOVE_SMOKE_IMPACT_INITIAL_ALPHA,
          LOVE_SMOKE_COLOR,
          LOVE_SMOKE_HIGHLIGHT_COLOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_MIN_COUNT + 1,
          MONSTER_EXPLOSION_SMOKE_BLOB_MAX_COUNT + 2,
          MONSTER_EXPLOSION_SMOKE_BLOB_OFFSET_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MAX_FACTOR * 1.1f,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_AMPLITUDE_CELLS * 1.4f,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_FREQUENCY_HZ * 0.8f,
          0.05f};
    case ExplosionSmokePuffKind::MonsterSmoke:
    default:
      return SmokeRenderTuning{
          MONSTER_EXPLOSION_SMOKE_LIFETIME_MS,
          MONSTER_EXPLOSION_SMOKE_INITIAL_RADIUS_CELLS,
          MONSTER_EXPLOSION_SMOKE_FINAL_RADIUS_CELLS,
          MONSTER_EXPLOSION_SMOKE_INITIAL_ALPHA,
          MONSTER_EXPLOSION_SMOKE_COLOR,
          MONSTER_EXPLOSION_SMOKE_HIGHLIGHT_COLOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_MIN_COUNT,
          MONSTER_EXPLOSION_SMOKE_BLOB_MAX_COUNT,
          MONSTER_EXPLOSION_SMOKE_BLOB_OFFSET_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MIN_FACTOR,
          MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MAX_FACTOR,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_AMPLITUDE_CELLS,
          MONSTER_EXPLOSION_SMOKE_WOBBLE_FREQUENCY_HZ,
          0.0f};
    }
  };

  struct ParticleRenderTuning {
    Uint32 lifetime_ms;
    Uint8 initial_alpha;
    SDL_Color base_color;
    SDL_Color highlight_color;
    int blob_min_count;
    int blob_max_count;
    float blob_offset_factor;
    float blob_radius_min_factor;
    float blob_radius_max_factor;
    float growth_factor;
    bool render_as_chunks;
  };
  auto get_particle_tuning = [](ExplosionParticleKind kind) {
    switch (kind) {
    case ExplosionParticleKind::WallDebris:
      return ParticleRenderTuning{
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_LIFETIME_MS,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_INITIAL_ALPHA,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_COLOR,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_HIGHLIGHT_COLOR,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_MIN_COUNT,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_MAX_COUNT,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_OFFSET_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_RADIUS_MIN_FACTOR,
          PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_RADIUS_MAX_FACTOR,
          0.24f,
          true};
    case ExplosionParticleKind::NuclearDebris:
      return ParticleRenderTuning{
          NUCLEAR_BOMB_DEBRIS_LIFETIME_MS,
          NUCLEAR_BOMB_DEBRIS_INITIAL_ALPHA,
          NUCLEAR_BOMB_DEBRIS_COLOR,
          NUCLEAR_BOMB_DEBRIS_HIGHLIGHT_COLOR,
          NUCLEAR_BOMB_DEBRIS_BLOB_MIN_COUNT,
          NUCLEAR_BOMB_DEBRIS_BLOB_MAX_COUNT,
          NUCLEAR_BOMB_DEBRIS_BLOB_OFFSET_FACTOR,
          NUCLEAR_BOMB_DEBRIS_BLOB_RADIUS_MIN_FACTOR,
          NUCLEAR_BOMB_DEBRIS_BLOB_RADIUS_MAX_FACTOR,
          0.24f,
          true};
    case ExplosionParticleKind::MonsterExplosion:
    default:
      return ParticleRenderTuning{
          MONSTER_EXPLOSION_PARTICLE_LIFETIME_MS,
          MONSTER_EXPLOSION_PARTICLE_INITIAL_ALPHA,
          MONSTER_EXPLOSION_PARTICLE_COLOR,
          MONSTER_EXPLOSION_PARTICLE_HIGHLIGHT_COLOR,
          MONSTER_EXPLOSION_PARTICLE_BLOB_MIN_COUNT,
          MONSTER_EXPLOSION_PARTICLE_BLOB_MAX_COUNT,
          MONSTER_EXPLOSION_PARTICLE_BLOB_OFFSET_FACTOR,
          MONSTER_EXPLOSION_PARTICLE_BLOB_RADIUS_MIN_FACTOR,
          MONSTER_EXPLOSION_PARTICLE_BLOB_RADIUS_MAX_FACTOR,
          0.55f,
          false};
    }
  };

  auto world_to_screen = [&](SDL_FPoint world, float vertical_cells) {
    if (ENABLE_3D_VIEW) {
      return projectScene(world.x, world.y, vertical_cells);
    }
    return SDL_FPoint{
        static_cast<float>(offset_x) + world.x * cell_to_px,
        static_cast<float>(offset_y) + world.y * cell_to_px -
            vertical_cells * cell_to_px * 0.86f};
  };

  for (const ExplosionSmokePuff &puff : game->explosion_smoke_puffs) {
    const SmokeRenderTuning tuning = get_smoke_tuning(puff.kind);
    if (now < puff.spawned_ticks) {
      continue;
    }
    const Uint32 age = now - puff.spawned_ticks;
    if (age >= tuning.lifetime_ms) {
      continue;
    }
    const double progress =
        static_cast<double>(age) / static_cast<double>(tuning.lifetime_ms);
    const float radius_cells =
        tuning.initial_radius_cells +
        (tuning.final_radius_cells - tuning.initial_radius_cells) *
            static_cast<float>(progress);
    const Uint8 base_alpha = static_cast<Uint8>(std::clamp(
        tuning.initial_alpha * (1.0 - progress), 0.0, 255.0));
    const float age_seconds = static_cast<float>(age) / 1000.0f;
    float vertical_cells =
        puff.vertical_offset_cells +
        tuning.vertical_rise_cells * static_cast<float>(progress) +
        puff.vertical_velocity_cells_per_sec * age_seconds;
    SDL_FPoint displaced_position{
        puff.world_position.x + puff.velocity_cells_per_sec.x * age_seconds,
        puff.world_position.y + puff.velocity_cells_per_sec.y * age_seconds};
    // Pilzkappe: Puff-Mittelpunkt folgt einer Halb-Torus-Bahn
    // (oben am Säulenkopf raus, am Außenrand nach unten umschlagen).
    if (puff.kind == ExplosionSmokePuffKind::NuclearBCapSmoke) {
      const float curl_progress = std::clamp(
          age_seconds * puff.rotation_speed_rad_per_sec,
          0.0f, static_cast<float>(M_PI));
      const float dir_x_world = std::cos(puff.rotation_radians);
      const float dir_y_world = std::sin(puff.rotation_radians);
      const float radial_curl =
          NUCLEAR_B_CAP_RADIUS_CELLS * std::sin(curl_progress);
      const float vertical_drop =
          NUCLEAR_B_CAP_CURL_DROP_CELLS * 0.5f *
          (1.0f - std::cos(curl_progress));
      displaced_position = {
          puff.world_position.x + dir_x_world * radial_curl,
          puff.world_position.y + dir_y_world * radial_curl * 0.55f};
      vertical_cells = puff.vertical_offset_cells - vertical_drop;
    }
    SDL_FPoint screen = world_to_screen(displaced_position, vertical_cells);
    if (puff.kind == ExplosionSmokePuffKind::RocketTrailSmoke) {
      screen.y -= rocket_trail_screen_lift_px;
    }
    const float base_radius_px = radius_cells * cell_to_px;
    const double wobble_clock =
        static_cast<double>(now) / 1000.0 *
        (2.0 * 3.14159265358979323846 * tuning.wobble_frequency_hz);

    std::mt19937 shape_rng(puff.shape_seed);
    std::uniform_int_distribution<int> blob_count_dist(
        tuning.blob_min_count, tuning.blob_max_count);
    std::uniform_real_distribution<float> offset_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> radius_factor_dist(
        tuning.blob_radius_min_factor, tuning.blob_radius_max_factor);
    std::uniform_real_distribution<float> shade_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> phase_dist(
        0.0f, static_cast<float>(2.0 * 3.14159265358979323846));

    const int blob_count = blob_count_dist(shape_rng);
    const float wobble_px =
        tuning.wobble_amplitude_cells * cell_to_px;

    // Rauchring rollt um die tangentiale Achse der Ringrichtung
    // (radiale & vertikale Komponente jedes Blobs werden im Querschnitt rotiert).
    const bool is_rolling_ring =
        puff.kind == ExplosionSmokePuffKind::NuclearBRingSmoke;
    const float ring_dir_x = is_rolling_ring ? std::cos(puff.rotation_radians) : 0.0f;
    const float ring_dir_y = is_rolling_ring ? std::sin(puff.rotation_radians) : 0.0f;
    const float ring_roll_phase =
        is_rolling_ring ? puff.rotation_speed_rad_per_sec * age_seconds : 0.0f;
    const float ring_cos = std::cos(ring_roll_phase);
    const float ring_sin = std::sin(ring_roll_phase);

    for (int blob = 0; blob < blob_count; ++blob) {
      float base_offset_x =
          offset_dist(shape_rng) * tuning.blob_offset_factor * base_radius_px;
      float base_offset_y =
          offset_dist(shape_rng) * tuning.blob_offset_factor * base_radius_px;
      if (is_rolling_ring) {
        // Lokale Querschnittskoordinaten (radial, vertikal). Rotation um
        // die tangentiale Achse: nach -roll_phase, damit der Vorderrand
        // beim Auswärtsrollen nach unten kippt.
        const float cs_radial = base_offset_x * ring_cos + base_offset_y * ring_sin;
        const float cs_vertical = -base_offset_x * ring_sin + base_offset_y * ring_cos;
        // Auf Bildschirm projizieren: radial entlang der Ringrichtung,
        // vertikal als Bildschirm-Hoch (negatives y).
        base_offset_x = cs_radial * ring_dir_x;
        base_offset_y = cs_radial * ring_dir_y - cs_vertical;
      }
      const float blob_radius =
          base_radius_px * radius_factor_dist(shape_rng);
      const float shade_mix = shade_dist(shape_rng);
      const float phase_x = phase_dist(shape_rng);
      const float phase_y = phase_dist(shape_rng);
      const float wobble_x =
          wobble_px * static_cast<float>(std::sin(wobble_clock + phase_x));
      const float wobble_y =
          wobble_px * static_cast<float>(std::cos(wobble_clock * 0.83 + phase_y));
      const Uint8 r = static_cast<Uint8>(
          tuning.base_color.r * (1.0f - shade_mix) +
          tuning.highlight_color.r * shade_mix);
      const Uint8 g = static_cast<Uint8>(
          tuning.base_color.g * (1.0f - shade_mix) +
          tuning.highlight_color.g * shade_mix);
      const Uint8 b = static_cast<Uint8>(
          tuning.base_color.b * (1.0f - shade_mix) +
          tuning.highlight_color.b * shade_mix);
      draw_soft_puff(screen.x + base_offset_x + wobble_x,
                     screen.y + base_offset_y + wobble_y,
                     std::max(1.0f, blob_radius), r, g, b, base_alpha);
    }
  }

  for (const ExplosionParticle &particle : game->explosion_particles) {
    const ParticleRenderTuning tuning = get_particle_tuning(particle.kind);
    if (now < particle.spawned_ticks) {
      continue;
    }
    const Uint32 age = now - particle.spawned_ticks;
    if (age >= tuning.lifetime_ms) {
      continue;
    }
    const double life_progress =
        static_cast<double>(age) / static_cast<double>(tuning.lifetime_ms);
    const double fade =
        std::clamp(1.0 - life_progress * life_progress, 0.0, 1.0);
    const Uint8 base_alpha = static_cast<Uint8>(std::clamp(
        tuning.initial_alpha * fade, 0.0, 255.0));
    const float growth =
        1.0f + tuning.growth_factor * static_cast<float>(life_progress);
    const SDL_FPoint screen = world_to_screen(particle.world_position, 0.0f);

    std::mt19937 shape_rng(particle.shape_seed);
    std::uniform_int_distribution<int> blob_count_dist(
        tuning.blob_min_count, tuning.blob_max_count);
    std::uniform_real_distribution<float> offset_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> radius_factor_dist(
        tuning.blob_radius_min_factor, tuning.blob_radius_max_factor);
    std::uniform_real_distribution<float> shade_dist(0.0f, 1.0f);

    const int blob_count = blob_count_dist(shape_rng);
    const float base_radius_px = particle.radius_cells * cell_to_px * growth;

    for (int blob = 0; blob < blob_count; ++blob) {
      const float offset_x =
          offset_dist(shape_rng) * tuning.blob_offset_factor * base_radius_px;
      const float offset_y =
          offset_dist(shape_rng) * tuning.blob_offset_factor * base_radius_px;
      const float blob_radius =
          base_radius_px * radius_factor_dist(shape_rng);
      const float shade_mix = shade_dist(shape_rng);
      const Uint8 r = static_cast<Uint8>(
          tuning.base_color.r * (1.0f - shade_mix) +
          tuning.highlight_color.r * shade_mix);
      const Uint8 g = static_cast<Uint8>(
          tuning.base_color.g * (1.0f - shade_mix) +
          tuning.highlight_color.g * shade_mix);
      const Uint8 b = static_cast<Uint8>(
          tuning.base_color.b * (1.0f - shade_mix) +
          tuning.highlight_color.b * shade_mix);
      if (tuning.render_as_chunks) {
        const int chunk_width = std::max(
            1, static_cast<int>(std::lround(blob_radius * (1.15f + 0.30f * shade_mix))));
        const int chunk_height = std::max(
            1, static_cast<int>(std::lround(blob_radius * (0.82f + 0.22f * (1.0f - shade_mix)))));
        const SDL_Rect chunk_rect{
            static_cast<int>(std::lround(screen.x + offset_x - chunk_width / 2.0f)),
            static_cast<int>(std::lround(screen.y + offset_y - chunk_height / 2.0f)),
            chunk_width,
            chunk_height};
        SDL_SetRenderDrawColor(sdl_renderer, r, g, b, base_alpha);
        SDL_RenderFillRect(sdl_renderer, &chunk_rect);

        const SDL_Rect highlight_rect{
            chunk_rect.x,
            chunk_rect.y,
            std::max(1, chunk_rect.w * 2 / 3),
            std::max(1, chunk_rect.h / 2)};
        SDL_SetRenderDrawColor(
            sdl_renderer,
            static_cast<Uint8>(std::min(255, static_cast<int>(r) + 18)),
            static_cast<Uint8>(std::min(255, static_cast<int>(g) + 16)),
            static_cast<Uint8>(std::min(255, static_cast<int>(b) + 14)),
            static_cast<Uint8>(std::clamp(base_alpha * 0.72, 0.0, 255.0)));
        SDL_RenderFillRect(sdl_renderer, &highlight_rect);
      } else {
        draw_soft_puff(screen.x + offset_x, screen.y + offset_y,
                       std::max(1.0f, blob_radius), r, g, b, base_alpha);
      }
    }
  }

  if (puff_texture != nullptr && !puff_indices.empty()) {
    SDL_SetTextureColorMod(puff_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(puff_texture, 255);
    SDL_RenderGeometry(sdl_renderer, puff_texture, puff_vertices.data(),
                       static_cast<int>(puff_vertices.size()),
                       puff_indices.data(),
                       static_cast<int>(puff_indices.size()));
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::buildWallRubblePieceVertices(const WallRubblePiece &piece,
                                            SDL_FPoint (&out_vertices)[4]) const {
  const double cos_angle = std::cos(piece.rotation_radians);
  const double sin_angle = std::sin(piece.rotation_radians);
  const std::array<SDL_FPoint, 4> local_vertices{{
      SDL_FPoint{
          -piece.half_width_cells * static_cast<float>(0.92 + 0.20 * HashUnit(
                                                                   static_cast<int>(
                                                                       piece.shape_seed ^ 11u))),
          -piece.half_height_cells * static_cast<float>(0.88 + 0.22 * HashUnit(
                                                                    static_cast<int>(
                                                                        piece.shape_seed ^ 23u)))},
      SDL_FPoint{
          piece.half_width_cells * static_cast<float>(0.86 + 0.26 * HashUnit(
                                                                  static_cast<int>(
                                                                      piece.shape_seed ^ 37u))),
          -piece.half_height_cells * static_cast<float>(0.74 + 0.22 * HashUnit(
                                                                    static_cast<int>(
                                                                        piece.shape_seed ^ 41u)))},
      SDL_FPoint{
          piece.half_width_cells * static_cast<float>(0.94 + 0.18 * HashUnit(
                                                                  static_cast<int>(
                                                                      piece.shape_seed ^ 53u))),
          piece.half_height_cells * static_cast<float>(0.84 + 0.24 * HashUnit(
                                                                   static_cast<int>(
                                                                       piece.shape_seed ^ 67u)))},
      SDL_FPoint{
          -piece.half_width_cells * static_cast<float>(0.82 + 0.24 * HashUnit(
                                                                   static_cast<int>(
                                                                       piece.shape_seed ^ 79u))),
          piece.half_height_cells * static_cast<float>(0.90 + 0.18 * HashUnit(
                                                                   static_cast<int>(
                                                                       piece.shape_seed ^ 97u)))},
  }};

  const float tile_span = static_cast<float>(element_size + 1);
  for (size_t index = 0; index < local_vertices.size(); ++index) {
    const double rotated_x =
        static_cast<double>(local_vertices[index].x) * cos_angle -
        static_cast<double>(local_vertices[index].y) * sin_angle;
    const double rotated_y =
        static_cast<double>(local_vertices[index].x) * sin_angle +
        static_cast<double>(local_vertices[index].y) * cos_angle;
    const double world_col = static_cast<double>(piece.world_position.x) + rotated_x;
    const double world_row = static_cast<double>(piece.world_position.y) + rotated_y;
    if (ENABLE_3D_VIEW) {
      out_vertices[index] = projectScene(world_col, world_row, 0.0);
    } else {
      out_vertices[index] = SDL_FPoint{
          static_cast<float>(offset_x + 1) +
              static_cast<float>(world_col) * tile_span,
          static_cast<float>(offset_y + 1) +
              static_cast<float>(world_row) * tile_span};
    }
  }
}

SDL_Rect Renderer::getWallRubblePieceBounds(const WallRubblePiece &piece) const {
  SDL_FPoint vertices[4];
  buildWallRubblePieceVertices(piece, vertices);
  return makeProjectedFaceRect(vertices[0], vertices[1], vertices[3], vertices[2]);
}

void Renderer::drawNuclearCrater(const NuclearCrater &crater) {
  if (crater.radius_cells <= 0.0f) {
    return;
  }

  constexpr int kSegments = 96;
  const double tile_span = static_cast<double>(element_size) + 1.0;
  const auto world_to_screen = [&](double col, double row) {
    if (ENABLE_3D_VIEW) {
      return projectScene(col, row, 0.012);
    }
    return SDL_FPoint{
        static_cast<float>(offset_x + 1) + static_cast<float>(col * tile_span),
        static_cast<float>(offset_y + 1) + static_cast<float>(row * tile_span)};
  };

  if (sdl_nuclear_crater_texture != nullptr &&
      sdl_nuclear_crater_size.x > 0 && sdl_nuclear_crater_size.y > 0) {
    const double radius = static_cast<double>(crater.radius_cells);
    const double left = static_cast<double>(crater.world_center.x) - radius;
    const double right = static_cast<double>(crater.world_center.x) + radius;
    const double top = static_cast<double>(crater.world_center.y) - radius;
    const double bottom = static_cast<double>(crater.world_center.y) + radius;
    const SDL_Color tint{255, 255, 255, NUCLEAR_CRATER_BASE_ALPHA};
    SDL_Vertex vertices[4]{
        {world_to_screen(left, top), tint, SDL_FPoint{0.0f, 0.0f}},
        {world_to_screen(right, top), tint, SDL_FPoint{1.0f, 0.0f}},
        {world_to_screen(right, bottom), tint, SDL_FPoint{1.0f, 1.0f}},
        {world_to_screen(left, bottom), tint, SDL_FPoint{0.0f, 1.0f}},
    };
    const int indices[6]{0, 1, 2, 0, 2, 3};
    SDL_SetTextureBlendMode(sdl_nuclear_crater_texture, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(sdl_renderer, sdl_nuclear_crater_texture, vertices, 4,
                       indices, 6);
    return;
  }

  std::vector<SDL_Vertex> vertices;
  vertices.reserve(kSegments + 2);
  SDL_Vertex center_vertex;
  center_vertex.position =
      world_to_screen(crater.world_center.x, crater.world_center.y);
  center_vertex.color = NUCLEAR_CRATER_INNER_COLOR;
  center_vertex.color.a = NUCLEAR_CRATER_BASE_ALPHA;
  center_vertex.tex_coord = SDL_FPoint{0.0f, 0.0f};
  vertices.push_back(center_vertex);

  for (int segment = 0; segment <= kSegments; ++segment) {
    const float angle =
        static_cast<float>(segment) *
        static_cast<float>((2.0 * kLogoPi) / static_cast<double>(kSegments));
    const float radius = NuclearCraterRenderEdgeRadius(
        crater.radius_cells, crater.shape_seed, angle);
    const double col = crater.world_center.x + std::cos(angle) * radius;
    const double row = crater.world_center.y + std::sin(angle) * radius;
    const double noise =
        HashUnit(crater.shape_seed + segment * 92821 + 37) * 0.34;
    SDL_Vertex vertex;
    vertex.position = world_to_screen(col, row);
    vertex.color = SDL_Color{
        static_cast<Uint8>(std::clamp(
            NUCLEAR_CRATER_OUTER_COLOR.r + 28.0 * noise, 0.0, 255.0)),
        static_cast<Uint8>(std::clamp(
            NUCLEAR_CRATER_OUTER_COLOR.g + 20.0 * noise, 0.0, 255.0)),
        static_cast<Uint8>(std::clamp(
            NUCLEAR_CRATER_OUTER_COLOR.b + 14.0 * noise, 0.0, 255.0)),
        static_cast<Uint8>(std::clamp(
            static_cast<double>(NUCLEAR_CRATER_BASE_ALPHA) *
                (0.74 + 0.18 * noise),
            0.0, 255.0))};
    vertex.tex_coord = SDL_FPoint{0.0f, 0.0f};
    vertices.push_back(vertex);
  }

  std::vector<int> indices;
  indices.reserve(kSegments * 3);
  for (int segment = 1; segment <= kSegments; ++segment) {
    indices.push_back(0);
    indices.push_back(segment);
    indices.push_back(segment + 1);
  }

  SDL_RenderGeometry(sdl_renderer, nullptr, vertices.data(),
                     static_cast<int>(vertices.size()), indices.data(),
                     static_cast<int>(indices.size()));

  const float glow_radius = crater.radius_cells * 0.58f;
  std::vector<SDL_Vertex> glow_vertices;
  glow_vertices.reserve(kSegments + 2);
  SDL_Vertex glow_center = center_vertex;
  glow_center.color = NUCLEAR_CRATER_GLOW_COLOR;
  glow_center.color.a = 86;
  glow_vertices.push_back(glow_center);
  for (int segment = 0; segment <= kSegments; ++segment) {
    const float angle =
        static_cast<float>(segment) *
        static_cast<float>((2.0 * kLogoPi) / static_cast<double>(kSegments));
    SDL_Vertex vertex;
    vertex.position = world_to_screen(
        crater.world_center.x + std::cos(angle) * glow_radius,
        crater.world_center.y + std::sin(angle) * glow_radius);
    vertex.color = NUCLEAR_CRATER_GLOW_COLOR;
    vertex.color.a = 0;
    vertex.tex_coord = SDL_FPoint{0.0f, 0.0f};
    glow_vertices.push_back(vertex);
  }
  SDL_RenderGeometry(sdl_renderer, nullptr, glow_vertices.data(),
                     static_cast<int>(glow_vertices.size()), indices.data(),
                     static_cast<int>(indices.size()));
}

void Renderer::drawNuclearCraterGreenCloud(const NuclearCrater &crater,
                                           Uint32 now) {
  if (crater.radius_cells <= 0.0f) {
    return;
  }

  const double tile_span = static_cast<double>(element_size) + 1.0;
  const auto world_to_screen = [&](double col, double row) {
    if (ENABLE_3D_VIEW) {
      return projectScene(col, row, 0.018);
    }
    return SDL_FPoint{
        static_cast<float>(offset_x + 1) + static_cast<float>(col * tile_span),
        static_cast<float>(offset_y + 1) + static_cast<float>(row * tile_span)};
  };

  const Uint32 age = (now > crater.visible_ticks) ? (now - crater.visible_ticks)
                                                  : 0;
  if (age >= NUCLEAR_CRATER_GREEN_CLOUD_LIFETIME_MS) {
    return;
  }
  const double fade_in =
      std::clamp(static_cast<double>(age) /
                     static_cast<double>(NUCLEAR_CRATER_GREEN_CLOUD_FADE_IN_MS),
                 0.0, 1.0);
  const Uint32 fade_out_start =
      (NUCLEAR_CRATER_GREEN_CLOUD_LIFETIME_MS >
       NUCLEAR_CRATER_GREEN_CLOUD_FADE_OUT_MS)
          ? NUCLEAR_CRATER_GREEN_CLOUD_LIFETIME_MS -
                NUCLEAR_CRATER_GREEN_CLOUD_FADE_OUT_MS
          : 0;
  double fade_out = 1.0;
  if (age >= fade_out_start) {
    fade_out = 1.0 - std::clamp(static_cast<double>(age - fade_out_start) /
                                    static_cast<double>(std::max<Uint32>(
                                        1, NUCLEAR_CRATER_GREEN_CLOUD_FADE_OUT_MS)),
                                0.0, 1.0);
  }
  const double peak_alpha =
      static_cast<double>(NUCLEAR_CRATER_GREEN_CLOUD_PEAK_ALPHA) * fade_in *
      fade_out;
  if (peak_alpha < 1.0) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double clock = static_cast<double>(now) / 1000.0;
  const double cloud_radius_cells =
      static_cast<double>(crater.radius_cells) *
      static_cast<double>(NUCLEAR_CRATER_GREEN_CLOUD_RADIUS_FACTOR);
  const double cloud_radius_px = cloud_radius_cells * tile_span;

  const int kBlobCount = std::max(4, NUCLEAR_CRATER_GREEN_CLOUD_BLOB_COUNT);
  for (int blob = 0; blob < kBlobCount; ++blob) {
    const double base_angle =
        blob * (2.0 * kLogoPi / static_cast<double>(kBlobCount));
    const double phase =
        HashUnit(crater.shape_seed + blob * 9173) * 2.0 * kLogoPi;
    // Tighter clustering: radial unit pulled inward and the random spread
    // narrowed so blobs hug the crater center.
    const double radial_unit =
        0.20 + 0.30 * HashUnit(crater.shape_seed + blob * 6151 + 17);
    const double waft =
        0.10 * std::sin(clock * 0.45 + phase) +
        0.07 * std::cos(clock * 0.27 + phase * 0.7);
    const double angle =
        base_angle + 0.20 * std::sin(clock * 0.33 + phase);
    const double offset_cells =
        cloud_radius_cells * (radial_unit + waft);
    const double col =
        static_cast<double>(crater.world_center.x) + std::cos(angle) * offset_cells;
    const double row =
        static_cast<double>(crater.world_center.y) +
        std::sin(angle) * offset_cells * 0.78 -
        0.04 * std::sin(clock * 0.6 + phase);
    const SDL_FPoint screen = world_to_screen(col, row);
    const double blob_radius_cells =
        cloud_radius_cells *
        (0.32 + 0.18 * HashUnit(crater.shape_seed + blob * 4421 + 91)) *
        (0.92 + 0.08 * std::sin(clock * 0.9 + phase));
    const double blob_radius_px = blob_radius_cells * tile_span;
    const double alpha_jitter =
        0.78 + 0.22 * HashUnit(crater.shape_seed + blob * 3331 + 47);
    const Uint8 alpha = static_cast<Uint8>(
        std::clamp(peak_alpha * alpha_jitter, 0.0, 255.0));
    SDL_RenderFillCircleAA(
        sdl_renderer, screen.x, screen.y, blob_radius_px,
        SDL_Color{NUCLEAR_CRATER_GREEN_CLOUD_COLOR.r,
                  NUCLEAR_CRATER_GREEN_CLOUD_COLOR.g,
                  NUCLEAR_CRATER_GREEN_CLOUD_COLOR.b, alpha});
  }

  const SDL_FPoint center_screen =
      world_to_screen(static_cast<double>(crater.world_center.x),
                      static_cast<double>(crater.world_center.y));
  const Uint8 halo_alpha = static_cast<Uint8>(
      std::clamp(peak_alpha * 0.55, 0.0, 255.0));
  SDL_RenderFillCircleAA(sdl_renderer, center_screen.x, center_screen.y,
                         cloud_radius_px * 0.95,
                         SDL_Color{NUCLEAR_CRATER_GREEN_CLOUD_COLOR.r,
                                   NUCLEAR_CRATER_GREEN_CLOUD_COLOR.g,
                                   NUCLEAR_CRATER_GREEN_CLOUD_COLOR.b,
                                   halo_alpha});

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawNuclearCraters() {
  if (game == nullptr || game->nuclear_craters.empty()) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
  const Uint32 now = SDL_GetTicks();
  for (const NuclearCrater &crater : game->nuclear_craters) {
    if (now < crater.visible_ticks) {
      continue;
    }
    drawNuclearCrater(crater);
    drawNuclearCraterGreenCloud(crater, now);
  }
  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawWallRubblePiece(const WallRubblePiece &piece) {
  SDL_FPoint vertices[4];
  buildWallRubblePieceVertices(piece, vertices);

  const double shade_mix = HashUnit(static_cast<int>(piece.shape_seed ^ 131u));
  const double highlight_mix = HashUnit(static_cast<int>(piece.shape_seed ^ 173u));
  const SDL_Color base_color{
      static_cast<Uint8>(WALL_RUBBLE_DARK_COLOR.r * (1.0 - shade_mix) +
                         WALL_RUBBLE_LIGHT_COLOR.r * shade_mix),
      static_cast<Uint8>(WALL_RUBBLE_DARK_COLOR.g * (1.0 - shade_mix) +
                         WALL_RUBBLE_LIGHT_COLOR.g * shade_mix),
      static_cast<Uint8>(WALL_RUBBLE_DARK_COLOR.b * (1.0 - shade_mix) +
                         WALL_RUBBLE_LIGHT_COLOR.b * shade_mix),
      WALL_RUBBLE_BASE_ALPHA};
  const SDL_Color light_color{
      static_cast<Uint8>(std::min(
          255, static_cast<int>(base_color.r + 18 + 18 * highlight_mix))),
      static_cast<Uint8>(std::min(
          255, static_cast<int>(base_color.g + 16 + 14 * highlight_mix))),
      static_cast<Uint8>(std::min(
          255, static_cast<int>(base_color.b + 14 + 10 * highlight_mix))),
      static_cast<Uint8>(std::clamp(WALL_RUBBLE_BASE_ALPHA * 0.92, 0.0, 255.0))};
  const SDL_Color shadow_color{
      static_cast<Uint8>(base_color.r * 0.72),
      static_cast<Uint8>(base_color.g * 0.70),
      static_cast<Uint8>(base_color.b * 0.68),
      static_cast<Uint8>(std::clamp(WALL_RUBBLE_BASE_ALPHA * 0.96, 0.0, 255.0))};

  const SDL_FPoint center{
      (vertices[0].x + vertices[1].x + vertices[2].x + vertices[3].x) * 0.25f,
      (vertices[0].y + vertices[1].y + vertices[2].y + vertices[3].y) * 0.25f};

  SDL_Vertex quad_vertices[4];
  quad_vertices[0].position = vertices[0];
  quad_vertices[0].color = light_color;
  quad_vertices[1].position = vertices[1];
  quad_vertices[1].color = base_color;
  quad_vertices[2].position = vertices[2];
  quad_vertices[2].color = shadow_color;
  quad_vertices[3].position = vertices[3];
  quad_vertices[3].color = base_color;
  for (SDL_Vertex &vertex : quad_vertices) {
    vertex.tex_coord = SDL_FPoint{0.0f, 0.0f};
  }
  const int quad_indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(sdl_renderer, nullptr, quad_vertices, 4, quad_indices, 6);

  SDL_Vertex highlight_vertices[3];
  highlight_vertices[0].position = SDL_FPoint{
      center.x + (vertices[0].x - center.x) * 0.52f,
      center.y + (vertices[0].y - center.y) * 0.52f};
  highlight_vertices[1].position = SDL_FPoint{
      center.x + (vertices[1].x - center.x) * 0.46f,
      center.y + (vertices[1].y - center.y) * 0.46f};
  highlight_vertices[2].position = SDL_FPoint{
      center.x + (vertices[3].x - center.x) * 0.38f,
      center.y + (vertices[3].y - center.y) * 0.38f};
  const SDL_Color glint_color{
      static_cast<Uint8>(std::min(255, static_cast<int>(light_color.r + 10))),
      static_cast<Uint8>(std::min(255, static_cast<int>(light_color.g + 10))),
      static_cast<Uint8>(std::min(255, static_cast<int>(light_color.b + 8))),
      static_cast<Uint8>(std::clamp(WALL_RUBBLE_BASE_ALPHA * 0.52, 0.0, 255.0))};
  for (SDL_Vertex &vertex : highlight_vertices) {
    vertex.color = glint_color;
    vertex.tex_coord = SDL_FPoint{0.0f, 0.0f};
  }
  const int highlight_indices[3] = {0, 1, 2};
  SDL_RenderGeometry(sdl_renderer, nullptr, highlight_vertices, 3,
                     highlight_indices, 3);
}

void Renderer::drawWallRubble() {
  if (game == nullptr || game->wall_rubble_pieces.empty()) {
    return;
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  std::vector<const WallRubblePiece *> sorted_pieces;
  sorted_pieces.reserve(game->wall_rubble_pieces.size());
  for (const WallRubblePiece &piece : game->wall_rubble_pieces) {
    sorted_pieces.push_back(&piece);
  }
  std::stable_sort(sorted_pieces.begin(), sorted_pieces.end(),
                   [](const WallRubblePiece *left, const WallRubblePiece *right) {
                     if (left->world_position.y == right->world_position.y) {
                       return left->world_position.x < right->world_position.x;
                     }
                     return left->world_position.y < right->world_position.y;
                   });

  for (const WallRubblePiece *piece : sorted_pieces) {
    if (piece == nullptr) {
      continue;
    }

    if (ENABLE_3D_VIEW) {
      const SDL_Rect bounds = getWallRubblePieceBounds(*piece);
      drawWithWallOcclusion(bounds, piece->world_position.y,
                            [this, piece]() { drawWallRubblePiece(*piece); });
    } else {
      drawWallRubblePiece(*piece);
    }
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawMonsterFireball(int center_x, int center_y, Uint32 elapsed,
                                   Uint32 seed) {
  if (elapsed >= MONSTER_EXPLOSION_FIREBALL_DURATION_MS) {
    return;
  }
  const double duration =
      static_cast<double>(MONSTER_EXPLOSION_FIREBALL_DURATION_MS);
  const double progress =
      std::clamp(static_cast<double>(elapsed) / duration, 0.0, 1.0);
  const double expansion_progress = std::clamp(
      static_cast<double>(elapsed) /
          static_cast<double>(
              std::max<Uint32>(1, MONSTER_EXPLOSION_FIREBALL_EXPANSION_MS)),
      0.0, 1.0);
  const double eased_expansion =
      1.0 - std::pow(1.0 - expansion_progress, 2.4);
  const double late_growth =
      MONSTER_EXPLOSION_FIREBALL_LATE_GROWTH * (progress - 0.20);
  const double scale =
      std::max(0.0, eased_expansion + std::max(0.0, late_growth));
  if (scale <= 0.0) {
    return;
  }
  const double fade_start = MONSTER_EXPLOSION_FIREBALL_FADE_START_PROGRESS;
  double global_alpha = 1.0;
  if (progress > fade_start) {
    global_alpha = std::max(
        0.0, 1.0 - (progress - fade_start) / std::max(0.001, 1.0 - fade_start));
    global_alpha = std::pow(global_alpha, 1.4);
  }

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const float cell_to_px = static_cast<float>(element_size + 1);
  const float peak_radius_px =
      MONSTER_EXPLOSION_FIREBALL_PEAK_RADIUS_CELLS * cell_to_px;

  const float wobble_px =
      MONSTER_EXPLOSION_FIREBALL_WOBBLE_AMPLITUDE_CELLS * cell_to_px;
  const double wobble_clock =
      static_cast<double>(SDL_GetTicks()) / 1000.0 *
      (2.0 * 3.14159265358979323846 *
       MONSTER_EXPLOSION_FIREBALL_WOBBLE_FREQUENCY_HZ);

  const Uint8 halo_alpha = static_cast<Uint8>(std::clamp(
      MONSTER_EXPLOSION_FIREBALL_HALO_ALPHA * global_alpha, 0.0, 255.0));
  if (halo_alpha > 0) {
    const int halo_radius = std::max(
        2, static_cast<int>(std::lround(peak_radius_px * scale *
                                        MONSTER_EXPLOSION_FIREBALL_HALO_RADIUS_FACTOR)));
    SDL_SetRenderDrawColor(sdl_renderer,
                           MONSTER_EXPLOSION_FIREBALL_HALO_COLOR.r,
                           MONSTER_EXPLOSION_FIREBALL_HALO_COLOR.g,
                           MONSTER_EXPLOSION_FIREBALL_HALO_COLOR.b, halo_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y, halo_radius);
  }

  struct FireballLayer {
    SDL_Color color;
    SDL_Color highlight;
    float radius_factor;
    int blob_count;
    Uint8 alpha;
  };
  const FireballLayer layers[] = {
      {MONSTER_EXPLOSION_FIREBALL_OUTER_COLOR,
       MONSTER_EXPLOSION_FIREBALL_RED_COLOR,
       MONSTER_EXPLOSION_FIREBALL_OUTER_RADIUS_FACTOR,
       MONSTER_EXPLOSION_FIREBALL_OUTER_BLOB_COUNT,
       MONSTER_EXPLOSION_FIREBALL_OUTER_ALPHA},
      {MONSTER_EXPLOSION_FIREBALL_RED_COLOR,
       MONSTER_EXPLOSION_FIREBALL_ORANGE_COLOR,
       MONSTER_EXPLOSION_FIREBALL_RED_RADIUS_FACTOR,
       MONSTER_EXPLOSION_FIREBALL_RED_BLOB_COUNT,
       MONSTER_EXPLOSION_FIREBALL_RED_ALPHA},
      {MONSTER_EXPLOSION_FIREBALL_ORANGE_COLOR,
       MONSTER_EXPLOSION_FIREBALL_YELLOW_COLOR,
       MONSTER_EXPLOSION_FIREBALL_ORANGE_RADIUS_FACTOR,
       MONSTER_EXPLOSION_FIREBALL_ORANGE_BLOB_COUNT,
       MONSTER_EXPLOSION_FIREBALL_ORANGE_ALPHA},
      {MONSTER_EXPLOSION_FIREBALL_YELLOW_COLOR,
       MONSTER_EXPLOSION_FIREBALL_CORE_COLOR,
       MONSTER_EXPLOSION_FIREBALL_YELLOW_RADIUS_FACTOR,
       MONSTER_EXPLOSION_FIREBALL_YELLOW_BLOB_COUNT,
       MONSTER_EXPLOSION_FIREBALL_YELLOW_ALPHA},
      {MONSTER_EXPLOSION_FIREBALL_CORE_COLOR,
       MONSTER_EXPLOSION_FIREBALL_CORE_COLOR,
       MONSTER_EXPLOSION_FIREBALL_CORE_RADIUS_FACTOR,
       MONSTER_EXPLOSION_FIREBALL_CORE_BLOB_COUNT,
       MONSTER_EXPLOSION_FIREBALL_CORE_ALPHA},
  };

  for (size_t layer_index = 0;
       layer_index < sizeof(layers) / sizeof(layers[0]); ++layer_index) {
    const FireballLayer &layer = layers[layer_index];
    const float layer_radius_px = static_cast<float>(
        peak_radius_px * scale * layer.radius_factor);
    if (layer_radius_px <= 0.5f) {
      continue;
    }
    const Uint8 layer_alpha = static_cast<Uint8>(std::clamp(
        layer.alpha * global_alpha, 0.0, 255.0));
    if (layer_alpha == 0) {
      continue;
    }

    std::mt19937 rng(seed ^ (0x9E3779B1u * (static_cast<Uint32>(layer_index) + 1u)));
    std::uniform_real_distribution<float> offset_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> radius_factor_dist(
        MONSTER_EXPLOSION_FIREBALL_BLOB_RADIUS_MIN_FACTOR,
        MONSTER_EXPLOSION_FIREBALL_BLOB_RADIUS_MAX_FACTOR);
    std::uniform_real_distribution<float> shade_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> phase_dist(
        0.0f, static_cast<float>(2.0 * 3.14159265358979323846));

    for (int blob = 0; blob < layer.blob_count; ++blob) {
      const float base_offset_x =
          offset_dist(rng) *
          MONSTER_EXPLOSION_FIREBALL_BLOB_OFFSET_FACTOR * layer_radius_px;
      const float base_offset_y =
          offset_dist(rng) *
          MONSTER_EXPLOSION_FIREBALL_BLOB_OFFSET_FACTOR * layer_radius_px;
      const float blob_radius =
          layer_radius_px * radius_factor_dist(rng);
      const float shade_mix = shade_dist(rng);
      const float phase_x = phase_dist(rng);
      const float phase_y = phase_dist(rng);
      const float wobble_x =
          wobble_px * static_cast<float>(std::sin(wobble_clock + phase_x));
      const float wobble_y =
          wobble_px *
          static_cast<float>(std::cos(wobble_clock * 0.83 + phase_y));
      const Uint8 r = static_cast<Uint8>(
          layer.color.r * (1.0f - shade_mix) + layer.highlight.r * shade_mix);
      const Uint8 g = static_cast<Uint8>(
          layer.color.g * (1.0f - shade_mix) + layer.highlight.g * shade_mix);
      const Uint8 b = static_cast<Uint8>(
          layer.color.b * (1.0f - shade_mix) + layer.highlight.b * shade_mix);
      SDL_SetRenderDrawColor(sdl_renderer, r, g, b, layer_alpha);
      SDL_RenderFillCircle(
          sdl_renderer,
          static_cast<int>(
              std::lround(center_x + base_offset_x + wobble_x)),
          static_cast<int>(
              std::lround(center_y + base_offset_y + wobble_y)),
          std::max(1, static_cast<int>(std::lround(blob_radius))));
    }
  }

  const double flare_progress =
      std::clamp(progress / std::max(0.01, fade_start), 0.0, 1.0);
  const double flare_strength = std::sin(flare_progress * 3.14159265358979323846);
  if (flare_strength > 0.01) {
    const Uint8 flare_alpha = static_cast<Uint8>(std::clamp(
        MONSTER_EXPLOSION_FIREBALL_FLARE_ALPHA * flare_strength * global_alpha,
        0.0, 255.0));
    if (flare_alpha > 0) {
      std::mt19937 flare_rng(seed ^ 0x517CC1B7u);
      std::uniform_real_distribution<float> jitter_dist(-0.18f, 0.18f);
      const double angle_step = 2.0 * 3.14159265358979323846 /
                                std::max(1, MONSTER_EXPLOSION_FIREBALL_FLARE_COUNT);
      for (int flare = 0; flare < MONSTER_EXPLOSION_FIREBALL_FLARE_COUNT; ++flare) {
        const double base_angle = flare * angle_step;
        const double angle =
            base_angle + jitter_dist(flare_rng) +
            wobble_clock * 0.06;
        const float inner_r = static_cast<float>(
            peak_radius_px * scale *
            MONSTER_EXPLOSION_FIREBALL_FLARE_INNER_FACTOR);
        const float outer_r = static_cast<float>(
            peak_radius_px * scale *
            MONSTER_EXPLOSION_FIREBALL_FLARE_OUTER_FACTOR *
            (0.85f + 0.30f * jitter_dist(flare_rng)));
        const int x1 = center_x +
                       static_cast<int>(std::lround(std::cos(angle) * inner_r));
        const int y1 = center_y +
                       static_cast<int>(std::lround(std::sin(angle) * inner_r));
        const int x2 = center_x +
                       static_cast<int>(std::lround(std::cos(angle) * outer_r));
        const int y2 = center_y +
                       static_cast<int>(std::lround(std::sin(angle) * outer_r));
        const SDL_Color flare_color{
            MONSTER_EXPLOSION_FIREBALL_FLARE_COLOR.r,
            MONSTER_EXPLOSION_FIREBALL_FLARE_COLOR.g,
            MONSTER_EXPLOSION_FIREBALL_FLARE_COLOR.b, flare_alpha};
        SDL_RenderDrawAALine(sdl_renderer, static_cast<double>(x1),
                             static_cast<double>(y1), static_cast<double>(x2),
                             static_cast<double>(y2), flare_color);
      }
    }
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawNuclearFireball(int center_x, int center_y, Uint32 elapsed,
                                   Uint32 seed) {
  if (elapsed >= NUCLEAR_EXPLOSION_FIREBALL_DURATION_MS) {
    return;
  }

  const double burn_duration =
      static_cast<double>(std::max<Uint32>(1, NUCLEAR_EXPLOSION_FIREBALL_BURN_MS));
  const double collapse_duration = static_cast<double>(
      std::max<Uint32>(1, NUCLEAR_EXPLOSION_FIREBALL_COLLAPSE_MS));
  const double cell_to_px = static_cast<double>(element_size + 1);
  const double start_radius =
      NUCLEAR_FIREBALL_START_RADIUS_CELLS * cell_to_px;
  const double peak_radius = std::max(
      start_radius + cell_to_px * 0.5,
      static_cast<double>(NUCLEAR_FIREBALL_PEAK_RADIUS_CELLS) * cell_to_px);

  double radius_px = start_radius;
  double alpha_scale = 1.0;
  double collapse_progress = 0.0;
  if (elapsed < NUCLEAR_EXPLOSION_FIREBALL_BURN_MS) {
    const double burn_t =
        std::clamp(static_cast<double>(elapsed) / burn_duration, 0.0, 1.0);
    const double expansion = 1.0 - std::pow(1.0 - burn_t, 2.15);
    radius_px = start_radius + (peak_radius - start_radius) * expansion;
  } else {
    collapse_progress = std::clamp(
        static_cast<double>(elapsed - NUCLEAR_EXPLOSION_FIREBALL_BURN_MS) /
            collapse_duration,
        0.0, 1.0);
    const double collapse_ease =
        1.0 - std::pow(1.0 - collapse_progress, 1.75);
    radius_px = peak_radius * (1.0 - 0.78 * collapse_ease);
    alpha_scale = std::pow(1.0 - collapse_progress, 1.65);
  }

  if (radius_px <= 1.0 || alpha_scale <= 0.0) {
    return;
  }

  const double clock =
      static_cast<double>(elapsed) / 1000.0 *
      (2.0 * kLogoPi * NUCLEAR_FIREBALL_WOBBLE_FREQUENCY_HZ);
  const double surface_pulse =
      0.88 + 0.12 * std::sin(clock * 0.73 + static_cast<double>(seed % 97));
  const double wobble_px =
      NUCLEAR_FIREBALL_WOBBLE_AMPLITUDE_CELLS * cell_to_px *
      (0.45 + 0.55 * alpha_scale);

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const Uint8 halo_alpha = static_cast<Uint8>(
      std::clamp(205.0 * alpha_scale * surface_pulse, 0.0, 220.0));
  SDL_RenderFillCircleAA(
      sdl_renderer, center_x, center_y, radius_px * 1.62,
      SDL_Color{255, 232, 178, static_cast<Uint8>(halo_alpha / 2)});
  SDL_RenderFillCircleAA(sdl_renderer, center_x, center_y, radius_px * 1.30,
                         SDL_Color{255, 168, 64, halo_alpha});
  const Uint8 white_core_alpha = static_cast<Uint8>(
      std::clamp(220.0 * alpha_scale, 0.0, 240.0));
  SDL_RenderFillCircleAA(sdl_renderer, center_x, center_y, radius_px * 0.42,
                         SDL_Color{255, 250, 226, white_core_alpha});

  std::mt19937 flame_rng(seed ^ 0xC2B2AE35u);
  std::uniform_real_distribution<double> flame_angle_jitter(-0.035, 0.035);
  std::uniform_real_distribution<double> flame_length_dist(0.64, 1.08);
  std::uniform_real_distribution<double> flame_inner_dist(0.12, 0.42);
  std::uniform_real_distribution<double> flame_phase_dist(0.0, 2.0 * kLogoPi);
  const Uint8 tongue_alpha = static_cast<Uint8>(
      std::clamp(225.0 * alpha_scale * (1.0 - collapse_progress * 0.42), 0.0,
                 230.0));
  for (int tongue = 0; tongue < NUCLEAR_FIREBALL_FLAME_TONGUE_COUNT; ++tongue) {
    const double base_angle =
        tongue * (2.0 * kLogoPi / NUCLEAR_FIREBALL_FLAME_TONGUE_COUNT);
    const double phase = flame_phase_dist(flame_rng);
    const double angle =
        base_angle + flame_angle_jitter(flame_rng) + std::sin(clock * 0.22 + phase) * 0.025;
    const double lick =
        0.88 + 0.12 * std::sin(clock * (0.52 + 0.01 * tongue) + phase);
    const double inner = radius_px * flame_inner_dist(flame_rng);
    const double outer = radius_px * flame_length_dist(flame_rng) * lick;
    const double middle = inner + (outer - inner) * 0.58;
    SDL_RenderDrawAALine(
        sdl_renderer, center_x + std::cos(angle) * inner,
        center_y + std::sin(angle) * inner,
        center_x + std::cos(angle) * outer,
        center_y + std::sin(angle) * outer,
        SDL_Color{255, 96, 24,
                  static_cast<Uint8>(std::clamp(tongue_alpha * 0.58, 0.0, 255.0))});
    SDL_RenderDrawAALine(
        sdl_renderer, center_x + std::cos(angle) * (inner * 0.92),
        center_y + std::sin(angle) * (inner * 0.92),
        center_x + std::cos(angle) * middle,
        center_y + std::sin(angle) * middle,
        SDL_Color{255, 204, 74, tongue_alpha});
  }

  struct NuclearFireballLayer {
    SDL_Color base;
    SDL_Color highlight;
    double radius_factor;
    int blob_count;
    Uint8 alpha;
  };
  const NuclearFireballLayer layers[] = {
      {SDL_Color{128, 24, 10, 255}, SDL_Color{226, 58, 18, 255}, 1.00,
       NUCLEAR_FIREBALL_OUTER_BLOB_COUNT, 218},
      {SDL_Color{224, 62, 18, 255}, SDL_Color{255, 146, 34, 255}, 0.76,
       NUCLEAR_FIREBALL_MID_BLOB_COUNT, 236},
      {SDL_Color{255, 132, 28, 255}, SDL_Color{255, 226, 92, 255}, 0.54,
       NUCLEAR_FIREBALL_MID_BLOB_COUNT, 248},
      {SDL_Color{255, 218, 96, 255}, SDL_Color{255, 255, 236, 255}, 0.32,
       NUCLEAR_FIREBALL_CORE_BLOB_COUNT, 255},
  };

  for (size_t layer_index = 0;
       layer_index < sizeof(layers) / sizeof(layers[0]); ++layer_index) {
    const NuclearFireballLayer &layer = layers[layer_index];
    const double layer_radius = radius_px * layer.radius_factor;
    if (layer_radius < 1.0) {
      continue;
    }
    const Uint8 layer_alpha = static_cast<Uint8>(
        std::clamp(static_cast<double>(layer.alpha) * alpha_scale, 0.0, 255.0));
    if (layer_alpha == 0) {
      continue;
    }

    std::mt19937 rng(seed ^ (0x85EBCA6Bu * (static_cast<Uint32>(layer_index) + 1u)));
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * kLogoPi);
    std::uniform_real_distribution<double> offset_dist(0.0, 1.0);
    std::uniform_real_distribution<double> radius_dist(
        NUCLEAR_FIREBALL_TEXTURE_BLOB_MIN_FACTOR,
        NUCLEAR_FIREBALL_TEXTURE_BLOB_MAX_FACTOR);
    std::uniform_real_distribution<double> shade_dist(0.0, 1.0);
    std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * kLogoPi);

    for (int blob = 0; blob < layer.blob_count; ++blob) {
      const double angle = angle_dist(rng);
      const double radial_unit = std::sqrt(offset_dist(rng));
      const double offset =
          radial_unit * NUCLEAR_FIREBALL_TEXTURE_OFFSET_FACTOR * layer_radius;
      const double phase_x = phase_dist(rng);
      const double phase_y = phase_dist(rng);
      const double boil =
          0.90 + 0.10 * std::sin(clock * (0.48 + 0.03 * blob) + phase_x);
      const double blob_radius =
          layer_radius * radius_dist(rng) * boil *
          (1.0 - 0.34 * collapse_progress);
      const double wobble_x =
          wobble_px * 0.35 * std::sin(clock * 0.72 + phase_x + layer_index * 0.7);
      const double wobble_y =
          wobble_px * 0.35 * std::cos(clock * 0.57 + phase_y + layer_index * 0.4);
      const double shade_mix = std::clamp(
          shade_dist(rng) * 0.72 + (1.0 - radial_unit) * 0.42, 0.0, 1.0);
      const Uint8 r = static_cast<Uint8>(
          layer.base.r * (1.0 - shade_mix) + layer.highlight.r * shade_mix);
      const Uint8 g = static_cast<Uint8>(
          layer.base.g * (1.0 - shade_mix) + layer.highlight.g * shade_mix);
      const Uint8 b = static_cast<Uint8>(
          layer.base.b * (1.0 - shade_mix) + layer.highlight.b * shade_mix);
      SDL_RenderFillCircleAA(
          sdl_renderer,
          center_x + std::cos(angle) * offset + wobble_x,
          center_y + std::sin(angle) * offset + wobble_y,
          std::max(1.0, blob_radius), SDL_Color{r, g, b, layer_alpha});
    }
  }

  const int flare_count = 32;
  const Uint8 flare_alpha = static_cast<Uint8>(
      std::clamp(235.0 * alpha_scale * (1.0 - collapse_progress * 0.55), 0.0,
                 240.0));
  for (int flare = 0; flare < flare_count; ++flare) {
    const double angle =
        clock * 0.09 + flare * (2.0 * kLogoPi / flare_count) +
        HashSigned(static_cast<int>(seed + flare * 41u)) * 0.14;
    const double inner = radius_px * (0.48 + 0.08 * (flare % 3));
    const double outer = radius_px *
                         (0.92 + 0.18 *
                                     HashUnit(static_cast<int>(seed + flare * 67u)));
    SDL_RenderDrawAALine(
        sdl_renderer, center_x + std::cos(angle) * inner,
        center_y + std::sin(angle) * inner,
        center_x + std::cos(angle) * outer,
        center_y + std::sin(angle) * outer,
        SDL_Color{255, 220, 118, flare_alpha});
  }

  const Uint8 ember_alpha = static_cast<Uint8>(
      std::clamp(92.0 * alpha_scale * (1.0 - collapse_progress), 0.0, 92.0));
  for (int pocket = 0; pocket < 16; ++pocket) {
    const double angle =
        pocket * (2.0 * kLogoPi / 16.0) +
        HashSigned(static_cast<int>(seed + pocket * 113u)) * 0.28;
    const double dist =
        radius_px * (0.52 + 0.36 * HashUnit(static_cast<int>(seed + pocket * 149u)));
    const double pocket_radius =
        radius_px * (0.055 + 0.045 * HashUnit(static_cast<int>(seed + pocket * 181u)));
    SDL_RenderFillCircleAA(
        sdl_renderer, center_x + std::cos(angle) * dist,
        center_y + std::sin(angle) * dist, pocket_radius,
        SDL_Color{74, 18, 8, ember_alpha});
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawNuclearBFireball(int center_x, int center_y, Uint32 elapsed,
                                    Uint32 seed) {
  if (elapsed >= NUCLEAR_B_EXPLOSION_FIREBALL_DURATION_MS) {
    return;
  }

  const double burn_duration = static_cast<double>(
      std::max<Uint32>(1, NUCLEAR_B_EXPLOSION_FIREBALL_BURN_MS));
  const double collapse_duration = static_cast<double>(
      std::max<Uint32>(1, NUCLEAR_B_EXPLOSION_FIREBALL_COLLAPSE_MS));
  const double cell_to_px = static_cast<double>(element_size + 1);
  const double start_radius =
      NUCLEAR_B_FIREBALL_START_RADIUS_CELLS * cell_to_px;
  const double peak_radius = std::max(
      start_radius + cell_to_px * 0.5,
      static_cast<double>(NUCLEAR_B_FIREBALL_PEAK_RADIUS_CELLS) * cell_to_px);

  double radius_px = start_radius;
  double alpha_scale = 1.0;
  double collapse_progress = 0.0;
  if (elapsed < NUCLEAR_B_EXPLOSION_FIREBALL_BURN_MS) {
    const double burn_t =
        std::clamp(static_cast<double>(elapsed) / burn_duration, 0.0, 1.0);
    const double expansion = 1.0 - std::pow(1.0 - burn_t, 1.65);
    radius_px = start_radius + (peak_radius - start_radius) * expansion;
  } else {
    collapse_progress = std::clamp(
        static_cast<double>(elapsed - NUCLEAR_B_EXPLOSION_FIREBALL_BURN_MS) /
            collapse_duration,
        0.0, 1.0);
    const double collapse_ease =
        1.0 - std::pow(1.0 - collapse_progress, 1.85);
    radius_px = peak_radius * (1.0 - 0.82 * collapse_ease);
    alpha_scale = std::pow(1.0 - collapse_progress, 1.55);
  }

  if (radius_px <= 1.0 || alpha_scale <= 0.0) {
    return;
  }

  const double clock =
      static_cast<double>(elapsed) / 1000.0 *
      (2.0 * kLogoPi * NUCLEAR_B_FIREBALL_WOBBLE_FREQUENCY_HZ);
  const double surface_pulse =
      0.86 + 0.14 * std::sin(clock * 0.81 + static_cast<double>(seed % 113));
  const double wobble_px =
      NUCLEAR_B_FIREBALL_WOBBLE_AMPLITUDE_CELLS * cell_to_px *
      (0.40 + 0.60 * alpha_scale);

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const Uint8 halo_alpha = static_cast<Uint8>(
      std::clamp(170.0 * alpha_scale * surface_pulse, 0.0, 170.0));
  SDL_RenderFillCircleAA(
      sdl_renderer, center_x, center_y, radius_px * 1.55,
      SDL_Color{120, 36, 14, static_cast<Uint8>(halo_alpha / 2)});
  SDL_RenderFillCircleAA(sdl_renderer, center_x, center_y, radius_px * 1.22,
                         SDL_Color{210, 70, 22, halo_alpha});

  std::mt19937 flame_rng(seed ^ 0x9E3779B9u);
  std::uniform_real_distribution<double> flame_angle_jitter(-0.04, 0.04);
  std::uniform_real_distribution<double> flame_length_dist(0.72, 1.24);
  std::uniform_real_distribution<double> flame_inner_dist(0.20, 0.56);
  std::uniform_real_distribution<double> flame_phase_dist(0.0, 2.0 * kLogoPi);
  const Uint8 tongue_alpha = static_cast<Uint8>(
      std::clamp(200.0 * alpha_scale * (1.0 - collapse_progress * 0.38), 0.0,
                 200.0));
  for (int tongue = 0; tongue < NUCLEAR_B_FIREBALL_FLAME_TONGUE_COUNT;
       ++tongue) {
    const double base_angle =
        tongue * (2.0 * kLogoPi /
                  static_cast<double>(NUCLEAR_B_FIREBALL_FLAME_TONGUE_COUNT));
    const double phase = flame_phase_dist(flame_rng);
    const double angle =
        base_angle + flame_angle_jitter(flame_rng) +
        std::sin(clock * 0.34 + phase) * 0.045;
    const double lick =
        0.78 + 0.28 * std::sin(clock * (0.62 + 0.013 * tongue) + phase);
    const double inner = radius_px * flame_inner_dist(flame_rng);
    const double outer = radius_px * flame_length_dist(flame_rng) * lick;
    SDL_RenderDrawAALine(
        sdl_renderer, center_x + std::cos(angle) * inner,
        center_y + std::sin(angle) * inner,
        center_x + std::cos(angle) * outer,
        center_y + std::sin(angle) * outer,
        SDL_Color{255, 110, 28,
                  static_cast<Uint8>(std::clamp(tongue_alpha * 0.62, 0.0, 255.0))});
    SDL_RenderDrawAALine(
        sdl_renderer, center_x + std::cos(angle) * (inner * 0.92),
        center_y + std::sin(angle) * (inner * 0.92),
        center_x + std::cos(angle) * (inner + (outer - inner) * 0.62),
        center_y + std::sin(angle) * (inner + (outer - inner) * 0.62),
        SDL_Color{255, 220, 110, tongue_alpha});
  }

  struct NuclearBFireballLayer {
    SDL_Color base;
    SDL_Color highlight;
    double radius_factor;
    int blob_count;
    Uint8 alpha;
  };
  const NuclearBFireballLayer layers[] = {
      {SDL_Color{146, 32, 12, 255}, SDL_Color{236, 70, 22, 255}, 1.05,
       NUCLEAR_B_FIREBALL_OUTER_BLOB_COUNT, 220},
      {SDL_Color{232, 78, 24, 255}, SDL_Color{255, 162, 46, 255}, 0.80,
       NUCLEAR_B_FIREBALL_MID_BLOB_COUNT, 240},
      {SDL_Color{255, 156, 38, 255}, SDL_Color{255, 232, 116, 255}, 0.56,
       NUCLEAR_B_FIREBALL_MID_BLOB_COUNT, 250},
      {SDL_Color{255, 232, 132, 255}, SDL_Color{255, 255, 244, 255}, 0.30,
       NUCLEAR_B_FIREBALL_CORE_BLOB_COUNT, 255},
  };

  for (size_t layer_index = 0;
       layer_index < sizeof(layers) / sizeof(layers[0]); ++layer_index) {
    const NuclearBFireballLayer &layer = layers[layer_index];
    const double layer_radius = radius_px * layer.radius_factor;
    if (layer_radius < 1.0) {
      continue;
    }
    const Uint8 layer_alpha = static_cast<Uint8>(
        std::clamp(static_cast<double>(layer.alpha) * alpha_scale, 0.0, 255.0));
    if (layer_alpha == 0) {
      continue;
    }

    std::mt19937 rng(seed ^ (0xCC9E2D51u * (static_cast<Uint32>(layer_index) + 3u)));
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * kLogoPi);
    std::uniform_real_distribution<double> offset_dist(0.0, 1.0);
    std::uniform_real_distribution<double> radius_dist(
        NUCLEAR_B_FIREBALL_TEXTURE_BLOB_MIN_FACTOR,
        NUCLEAR_B_FIREBALL_TEXTURE_BLOB_MAX_FACTOR);
    std::uniform_real_distribution<double> shade_dist(0.0, 1.0);
    std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * kLogoPi);

    for (int blob = 0; blob < layer.blob_count; ++blob) {
      const double angle = angle_dist(rng);
      const double radial_unit = std::sqrt(offset_dist(rng));
      const double offset =
          radial_unit * NUCLEAR_B_FIREBALL_TEXTURE_OFFSET_FACTOR * layer_radius;
      const double phase_x = phase_dist(rng);
      const double phase_y = phase_dist(rng);
      const double boil =
          0.86 + 0.16 * std::sin(clock * (0.62 + 0.04 * blob) + phase_x);
      const double blob_radius =
          layer_radius * radius_dist(rng) * boil *
          (1.0 - 0.32 * collapse_progress);
      const double wobble_x =
          wobble_px * 0.42 * std::sin(clock * 0.86 + phase_x + layer_index * 0.6);
      const double wobble_y =
          wobble_px * 0.42 * std::cos(clock * 0.69 + phase_y + layer_index * 0.4);
      const double shade_mix = std::clamp(
          shade_dist(rng) * 0.78 + (1.0 - radial_unit) * 0.46, 0.0, 1.0);
      const Uint8 r = static_cast<Uint8>(
          layer.base.r * (1.0 - shade_mix) + layer.highlight.r * shade_mix);
      const Uint8 g = static_cast<Uint8>(
          layer.base.g * (1.0 - shade_mix) + layer.highlight.g * shade_mix);
      const Uint8 b = static_cast<Uint8>(
          layer.base.b * (1.0 - shade_mix) + layer.highlight.b * shade_mix);
      SDL_RenderFillCircleAA(
          sdl_renderer,
          center_x + std::cos(angle) * offset + wobble_x,
          center_y + std::sin(angle) * offset + wobble_y,
          std::max(1.0, blob_radius), SDL_Color{r, g, b, layer_alpha});
    }
  }

  // Sun-like prominences: long flickering tongues breaking out radially.
  const int prominence_count = 18;
  const Uint8 prominence_alpha = static_cast<Uint8>(
      std::clamp(190.0 * alpha_scale * (1.0 - collapse_progress * 0.55), 0.0,
                 190.0));
  for (int prominence = 0; prominence < prominence_count; ++prominence) {
    const double angle =
        clock * 0.13 + prominence * (2.0 * kLogoPi / prominence_count) +
        HashSigned(static_cast<int>(seed + prominence * 53u)) * 0.18;
    const double inner = radius_px * 0.55;
    const double outer =
        radius_px *
        (1.05 + 0.30 * HashUnit(static_cast<int>(seed + prominence * 89u)));
    SDL_RenderDrawAALine(
        sdl_renderer, center_x + std::cos(angle) * inner,
        center_y + std::sin(angle) * inner,
        center_x + std::cos(angle) * outer,
        center_y + std::sin(angle) * outer,
        SDL_Color{255, 232, 138, prominence_alpha});
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawNuclearBBaseFire(int center_x, int center_y, Uint32 elapsed,
                                    Uint32 seed) {
  const double total_progress =
      static_cast<double>(elapsed) /
      static_cast<double>(NUCLEAR_B_EXPLOSION_TOTAL_DURATION_MS);
  if (total_progress >= 1.0) {
    return;
  }

  // Schnelle Aufhellung am Anfang, langsamer Abfall am Ende.
  double intensity = 1.0;
  if (elapsed < 180) {
    intensity = static_cast<double>(elapsed) / 180.0;
  }
  if (total_progress > 0.65) {
    intensity *= std::pow(std::max(0.0, 1.0 - (total_progress - 0.65) / 0.35),
                          1.6);
  }
  if (intensity <= 0.0) {
    return;
  }

  SDL_BlendMode prev_blend;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &prev_blend);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double cell_to_px = static_cast<double>(element_size + 1);
  const double base_radius_px =
      cell_to_px * NUCLEAR_B_BASE_FIRE_RADIUS_CELLS;
  const double clock = static_cast<double>(elapsed) / 1000.0;

  // Großes weiches Halo unter dem Sockel.
  SDL_RenderFillEllipseAA(
      sdl_renderer, center_x, center_y - base_radius_px * 0.18,
      base_radius_px * 1.95, base_radius_px * 0.95,
      SDL_Color{NUCLEAR_B_BASE_FIRE_HALO_COLOR.r,
                NUCLEAR_B_BASE_FIRE_HALO_COLOR.g,
                NUCLEAR_B_BASE_FIRE_HALO_COLOR.b,
                static_cast<Uint8>(std::clamp(140.0 * intensity, 0.0, 200.0))});

  std::mt19937 rng(seed ^ 0xBA5EF14Eu);
  std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * kLogoPi);
  std::uniform_real_distribution<double> radius_unit_dist(0.0, 1.0);
  std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * kLogoPi);
  std::uniform_real_distribution<double> blob_radius_dist(0.45, 1.05);

  for (int blob = 0; blob < NUCLEAR_B_BASE_FIRE_BLOB_COUNT; ++blob) {
    const double angle = angle_dist(rng);
    const double rad_unit = std::sqrt(radius_unit_dist(rng));
    const double rad_px = base_radius_px * rad_unit;
    const double phase = phase_dist(rng);
    const double flicker =
        0.62 + 0.38 * std::sin(clock * 5.1 + phase + blob * 0.17);
    const double x = center_x + std::cos(angle) * rad_px;
    // Kuppel-Form: blobs nahe dem Zentrum sind höher.
    const double dome_lift = (1.0 - rad_unit) * cell_to_px * 0.85;
    const double y =
        center_y + std::sin(angle) * rad_px * 0.42 - dome_lift;
    const double r = cell_to_px * blob_radius_dist(rng) *
                     (1.05 - rad_unit * 0.45) * intensity;

    SDL_RenderFillCircleAA(
        sdl_renderer, x, y, r * 1.7,
        SDL_Color{NUCLEAR_B_BASE_FIRE_OUTER_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_OUTER_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_OUTER_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(150.0 * intensity * flicker, 0.0, 210.0))});
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y, r * 1.0,
        SDL_Color{NUCLEAR_B_BASE_FIRE_MID_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_MID_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_MID_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(210.0 * intensity * flicker, 0.0, 240.0))});
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y, r * 0.45,
        SDL_Color{NUCLEAR_B_BASE_FIRE_INNER_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_INNER_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_INNER_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(230.0 * intensity * flicker, 0.0, 250.0))});
  }

  // Flammen, die nach oben züngeln.
  for (int tongue = 0; tongue < NUCLEAR_B_BASE_FIRE_TONGUE_COUNT; ++tongue) {
    const double angle =
        -kLogoPi * 0.5 + (radius_unit_dist(rng) - 0.5) * kLogoPi * 0.9;
    const double base_rad =
        base_radius_px * (0.05 + 0.65 * radius_unit_dist(rng));
    const double phase = phase_dist(rng);
    const double lick = 0.55 + 0.45 * std::sin(clock * 6.0 + phase);
    const double base_x = center_x + std::cos(angle) * base_rad * 0.5;
    const double base_y = center_y + std::sin(angle) * base_rad * 0.18;
    const double tip_x = base_x + std::cos(angle) * base_radius_px * 0.30;
    const double tip_y = base_y - cell_to_px * (1.4 + 1.6 * lick) * intensity;
    SDL_RenderDrawAALine(
        sdl_renderer, base_x, base_y, tip_x, tip_y,
        SDL_Color{NUCLEAR_B_BASE_FIRE_INNER_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_INNER_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_INNER_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(190.0 * intensity * lick, 0.0, 230.0))});
    const double tip2_x = base_x + std::cos(angle) * base_radius_px * 0.18;
    const double tip2_y = base_y - cell_to_px * (0.8 + 1.0 * lick) * intensity;
    SDL_RenderDrawAALine(
        sdl_renderer, base_x, base_y, tip2_x, tip2_y,
        SDL_Color{NUCLEAR_B_BASE_FIRE_MID_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_MID_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_MID_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(170.0 * intensity * lick, 0.0, 220.0))});
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, prev_blend);
}

void Renderer::drawNuclearBStemFire(int center_x, int center_y, Uint32 elapsed,
                                    Uint32 seed, double stem_progress,
                                    double total_progress) {
  if (stem_progress <= 0.0) {
    return;
  }

  SDL_BlendMode prev_blend;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &prev_blend);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double cell_to_px = static_cast<double>(element_size + 1);
  const int stem_top_y = static_cast<int>(std::lround(
      center_y - cell_to_px * NUCLEAR_B_STEM_HEIGHT_CELLS * 0.78 *
                     stem_progress));
  const double base_glow_alpha =
      225.0 * std::clamp(1.0 - total_progress * 1.05, 0.0, 1.0);
  const double clock = static_cast<double>(elapsed) / 1000.0;
  std::mt19937 rng(seed ^ 0xA1B2C3D4u);
  std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * kLogoPi);
  std::uniform_real_distribution<double> radius_jitter_dist(0.55, 1.20);
  std::uniform_real_distribution<double> lateral_jitter_dist(-1.0, 1.0);

  const int span = std::max(1, center_y - stem_top_y);
  for (int blob = 0; blob < NUCLEAR_B_FIRE_GLOW_BLOB_COUNT; ++blob) {
    const double height_unit =
        static_cast<double>(blob) /
        static_cast<double>(NUCLEAR_B_FIRE_GLOW_BLOB_COUNT - 1);
    const double phase = phase_dist(rng);
    const double flicker = 0.55 + 0.45 * std::sin(clock * 4.4 + phase);
    const Uint8 inner_alpha = static_cast<Uint8>(
        std::clamp(base_glow_alpha * flicker, 0.0, 245.0));
    if (inner_alpha == 0) {
      continue;
    }
    const double y_offset =
        stem_top_y + height_unit * static_cast<double>(span);
    // Säule weitet sich zum Sockel hin auf (height_unit=1 ≙ unten).
    const double width_factor = 0.55 + 0.95 * height_unit;
    const double sway = lateral_jitter_dist(rng) * cell_to_px *
                        NUCLEAR_B_STEM_RADIUS_CELLS * width_factor;
    const double horizontal_oscillation =
        std::sin(clock * 1.8 + phase + height_unit * 6.2) * cell_to_px *
        0.20;
    const double glow_radius =
        cell_to_px * NUCLEAR_B_FIRE_GLOW_RADIUS_CELLS *
        radius_jitter_dist(rng) * (0.80 + 0.55 * height_unit) *
        (0.55 + 0.55 * stem_progress);
    const double x = center_x + sway + horizontal_oscillation;
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y_offset, glow_radius * 1.65,
        SDL_Color{NUCLEAR_B_FIRE_GLOW_OUTER_COLOR.r,
                  NUCLEAR_B_FIRE_GLOW_OUTER_COLOR.g,
                  NUCLEAR_B_FIRE_GLOW_OUTER_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(inner_alpha * 0.62, 0.0, 220.0))});
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y_offset, glow_radius,
        SDL_Color{NUCLEAR_B_FIRE_GLOW_INNER_COLOR.r,
                  NUCLEAR_B_FIRE_GLOW_INNER_COLOR.g,
                  NUCLEAR_B_FIRE_GLOW_INNER_COLOR.b, inner_alpha});
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y_offset, glow_radius * 0.45,
        SDL_Color{255, 250, 220,
                  static_cast<Uint8>(
                      std::clamp(inner_alpha * 0.85, 0.0, 235.0))});
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, prev_blend);
}

void Renderer::drawNuclearBCapFire(int center_x, int center_y, Uint32 elapsed,
                                   Uint32 seed, double stem_progress,
                                   double total_progress) {
  if (stem_progress < 0.05) {
    return;
  }

  SDL_BlendMode prev_blend;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &prev_blend);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double cell_to_px = static_cast<double>(element_size + 1);
  const double cap_top_cells =
      0.8 + stem_progress * NUCLEAR_B_STEM_HEIGHT_CELLS +
      NUCLEAR_B_CAP_HEIGHT_OFFSET_CELLS;
  const double cap_top_y = center_y - cap_top_cells * cell_to_px * 0.78;
  const double clock = static_cast<double>(elapsed) / 1000.0;
  const double base_alpha =
      210.0 * std::clamp(1.0 - total_progress * 1.05, 0.0, 1.0) *
      std::min(1.0, stem_progress * 1.6);

  std::mt19937 rng(seed ^ 0xCAFE2024u);
  std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * kLogoPi);
  std::uniform_real_distribution<double> radius_unit_dist(0.0, 1.0);
  std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * kLogoPi);
  std::uniform_real_distribution<double> blob_size_dist(0.65, 1.30);

  for (int blob = 0; blob < NUCLEAR_B_CAP_FIRE_BLOB_COUNT; ++blob) {
    const double angle = angle_dist(rng);
    const double rad_unit = std::sqrt(radius_unit_dist(rng));
    const double rad_cells = rad_unit * NUCLEAR_B_CAP_RADIUS_CELLS * 0.95;
    // Vertical position: 0 = direkt am Säulenkopf,
    // 1 = unterer Rand der Pilzkappe.
    const double v_unit = 0.30 + 0.70 * radius_unit_dist(rng);
    const double v_drop_cells = NUCLEAR_B_CAP_CURL_DROP_CELLS * v_unit;
    const double phase = phase_dist(rng);
    const double flicker = 0.55 + 0.45 * std::sin(clock * 5.2 + phase);
    const Uint8 inner_alpha = static_cast<Uint8>(
        std::clamp(base_alpha * flicker, 0.0, 240.0));
    if (inner_alpha == 0) {
      continue;
    }

    const double dx = std::cos(angle) * rad_cells * cell_to_px;
    const double dy = std::sin(angle) * rad_cells * cell_to_px * 0.42;
    const double x = center_x + dx;
    const double y = cap_top_y + v_drop_cells * cell_to_px * 0.78 + dy;

    const double glow_radius =
        cell_to_px * NUCLEAR_B_CAP_FIRE_RADIUS_CELLS *
        blob_size_dist(rng) * (0.95 - 0.30 * v_unit);

    SDL_RenderFillCircleAA(
        sdl_renderer, x, y, glow_radius * 1.7,
        SDL_Color{NUCLEAR_B_BASE_FIRE_OUTER_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_OUTER_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_OUTER_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(inner_alpha * 0.55, 0.0, 210.0))});
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y, glow_radius,
        SDL_Color{NUCLEAR_B_BASE_FIRE_MID_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_MID_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_MID_COLOR.b,
                  static_cast<Uint8>(
                      std::clamp(inner_alpha * 0.92, 0.0, 240.0))});
    SDL_RenderFillCircleAA(
        sdl_renderer, x, y, glow_radius * 0.45,
        SDL_Color{NUCLEAR_B_BASE_FIRE_INNER_COLOR.r,
                  NUCLEAR_B_BASE_FIRE_INNER_COLOR.g,
                  NUCLEAR_B_BASE_FIRE_INNER_COLOR.b, inner_alpha});
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, prev_blend);
}

void Renderer::drawNuclearBGroundDebris(int center_x, int center_y,
                                        Uint32 elapsed, Uint32 seed) {
  // Schutt erscheint kurz nach Einschlag, bleibt dann liegen und fadet
  // gegen Ende leicht aus.
  if (elapsed < 80) {
    return;
  }
  const double total_progress =
      static_cast<double>(elapsed) /
      static_cast<double>(NUCLEAR_B_EXPLOSION_TOTAL_DURATION_MS);
  double alpha_scale = 1.0;
  if (elapsed < 320) {
    alpha_scale = (elapsed - 80) / 240.0;
  }
  if (total_progress > 0.85) {
    alpha_scale *= std::pow(std::max(0.0, 1.0 - (total_progress - 0.85) / 0.15),
                            1.4);
  }
  if (alpha_scale <= 0.0) {
    return;
  }

  SDL_BlendMode prev_blend;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &prev_blend);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const double cell_to_px = static_cast<double>(element_size + 1);
  std::mt19937 rng(seed ^ 0x7E575E1Du);
  std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * kLogoPi);
  std::uniform_real_distribution<double> radius_dist(
      NUCLEAR_B_GROUND_DEBRIS_INNER_RADIUS_CELLS,
      NUCLEAR_B_GROUND_DEBRIS_OUTER_RADIUS_CELLS);
  std::uniform_real_distribution<double> half_size_dist(
      NUCLEAR_B_GROUND_DEBRIS_MIN_HALF_SIZE_CELLS,
      NUCLEAR_B_GROUND_DEBRIS_MAX_HALF_SIZE_CELLS);
  std::uniform_real_distribution<double> shade_dist(0.0, 1.0);
  std::uniform_real_distribution<double> aspect_dist(0.6, 1.5);

  // Brocken dürfen den Krater nicht überlagern: Position gegen die
  // Krater-Ellipse prüfen und Treffer verwerfen.
  const double crater_x_rad_px =
      NUCLEAR_CRATER_RADIUS_CELLS * cell_to_px;
  const double crater_y_rad_px =
      NUCLEAR_CRATER_RADIUS_CELLS * NUCLEAR_CRATER_Y_RADIUS_FACTOR * cell_to_px;

  for (int chunk = 0; chunk < NUCLEAR_B_GROUND_DEBRIS_COUNT; ++chunk) {
    const double angle = angle_dist(rng);
    const double radius_cells = radius_dist(rng);
    const double half_size_cells = half_size_dist(rng);
    const double aspect = aspect_dist(rng);
    const double shade = shade_dist(rng);

    const double cx = center_x + std::cos(angle) * radius_cells * cell_to_px;
    const double cy =
        center_y + std::sin(angle) * radius_cells * cell_to_px * 0.42;
    const double w = half_size_cells * cell_to_px * aspect;
    const double h = half_size_cells * cell_to_px / std::max(0.4, aspect);

    const double dx = cx - center_x;
    const double dy = cy - center_y;
    const double ex = crater_x_rad_px + w;
    const double ey = crater_y_rad_px + h;
    if (ex > 0.0 && ey > 0.0 &&
        (dx * dx) / (ex * ex) + (dy * dy) / (ey * ey) < 1.0) {
      continue;
    }

    const Uint8 r = static_cast<Uint8>(
        NUCLEAR_B_GROUND_DEBRIS_DARK_COLOR.r * (1.0 - shade) +
        NUCLEAR_B_GROUND_DEBRIS_LIGHT_COLOR.r * shade);
    const Uint8 g = static_cast<Uint8>(
        NUCLEAR_B_GROUND_DEBRIS_DARK_COLOR.g * (1.0 - shade) +
        NUCLEAR_B_GROUND_DEBRIS_LIGHT_COLOR.g * shade);
    const Uint8 b = static_cast<Uint8>(
        NUCLEAR_B_GROUND_DEBRIS_DARK_COLOR.b * (1.0 - shade) +
        NUCLEAR_B_GROUND_DEBRIS_LIGHT_COLOR.b * shade);

    SDL_RenderFillEllipseAA(
        sdl_renderer, cx + 1.0, cy + 1.5, w + 1.0, h + 1.0,
        SDL_Color{30, 24, 20,
                  static_cast<Uint8>(std::clamp(120.0 * alpha_scale, 0.0, 180.0))});
    SDL_RenderFillEllipseAA(
        sdl_renderer, cx, cy, w, h,
        SDL_Color{r, g, b,
                  static_cast<Uint8>(std::clamp(220.0 * alpha_scale, 0.0, 240.0))});
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, prev_blend);
}

void Renderer::drawNuclearBExplosion(const GameEffect &effect, int center_x,
                                     int center_y, Uint32 elapsed) {
  if (elapsed < NUCLEAR_B_FLASH_DURATION_MS) {
    const double flash_t =
        static_cast<double>(elapsed) /
        static_cast<double>(std::max<Uint32>(1, NUCLEAR_B_FLASH_DURATION_MS));
    constexpr double kFlashRiseFraction = 0.08;
    double flash_intensity;
    if (flash_t < kFlashRiseFraction) {
      flash_intensity = flash_t / kFlashRiseFraction;
    } else {
      flash_intensity = std::pow(
          std::max(0.0, 1.0 - (flash_t - kFlashRiseFraction) /
                                  (1.0 - kFlashRiseFraction)),
          1.4);
    }
    const Uint8 flash_alpha = static_cast<Uint8>(std::clamp(
        flash_intensity * static_cast<double>(NUCLEAR_B_FLASH_PEAK_ALPHA), 0.0,
        255.0));
    if (flash_alpha > 0) {
      SDL_BlendMode prev_blend;
      SDL_GetRenderDrawBlendMode(sdl_renderer, &prev_blend);
      SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(sdl_renderer, NUCLEAR_B_FLASH_COLOR.r,
                             NUCLEAR_B_FLASH_COLOR.g, NUCLEAR_B_FLASH_COLOR.b,
                             flash_alpha);
      SDL_Rect flash_rect{0, 0, screen_res_x, screen_res_y};
      SDL_RenderFillRect(sdl_renderer, &flash_rect);
      SDL_SetRenderDrawBlendMode(sdl_renderer, prev_blend);
    }
  }

  const Uint32 fireball_seed =
      static_cast<Uint32>(effect.started_ticks) ^
      (static_cast<Uint32>(effect.coord.u) * 73856093u) ^
      (static_cast<Uint32>(effect.coord.v) * 19349663u) ^ 0x5BD1E995u;

  // 1. Bodenschutt (am weitesten hinten, damit alle Flammen darüber liegen).
  drawNuclearBGroundDebris(center_x, center_y, elapsed, fireball_seed);

  // 2. Initialer aufgehender und kollabierender Feuerball.
  drawNuclearBFireball(center_x, center_y, elapsed, fireball_seed);

  // 3. Persistenter Sockel-Feuerball, der die ganze Animation über brennt.
  drawNuclearBBaseFire(center_x, center_y, elapsed, fireball_seed);

  // Stem- und Cap-Fortschritt für Feuer-Helfer.
  const double stem_progress = std::clamp(
      static_cast<double>(elapsed) /
          static_cast<double>(std::max<Uint32>(1, NUCLEAR_B_STEM_BUILD_MS)),
      0.0, 1.0);
  const double total_progress = std::clamp(
      static_cast<double>(elapsed) /
          static_cast<double>(NUCLEAR_B_EXPLOSION_TOTAL_DURATION_MS),
      0.0, 1.0);

  // 4. Feuer in der Säule (durchscheint hinter Säulenrauch).
  drawNuclearBStemFire(center_x, center_y, elapsed, fireball_seed,
                       stem_progress, total_progress);

  // 5. Feuer in der unteren Pilzkappenhälfte.
  drawNuclearBCapFire(center_x, center_y, elapsed, fireball_seed,
                      stem_progress, total_progress);
}

void Renderer::drawDefeatEffect() {
  if (game == nullptr) {
    return;
  }

  const PixelCoord base_coord = getPixelCoord(game->death_coord, 0, 0);
  const int center_x = base_coord.x + element_size / 2;
  const int center_y = base_coord.y + element_size / 2;
  const Uint32 elapsed =
      (game->death_started_ticks == 0)
          ? 1000
          : (SDL_GetTicks() - game->death_started_ticks);

  if (elapsed < 900) {
    const double progress =
        std::clamp(static_cast<double>(elapsed) / 900.0, 0.0, 1.0);
    const double pulse = 0.70 + 0.30 * std::sin(progress * 5.0 * 3.1415926535);
    const int outer_radius =
        std::max(8, static_cast<int>(element_size * (0.32 + progress * 0.72)));
    const int mid_radius =
        std::max(5, static_cast<int>(element_size * (0.22 + progress * 0.52)));
    const int core_radius =
        std::max(3, static_cast<int>(element_size * (0.12 + progress * 0.28)));
    const Uint8 outer_alpha =
        static_cast<Uint8>(std::clamp(120.0 * (1.0 - progress), 0.0, 120.0));
    const Uint8 mid_alpha = static_cast<Uint8>(
        std::clamp(180.0 * (1.0 - progress * 0.55), 0.0, 180.0));
    const Uint8 core_alpha = static_cast<Uint8>(
        std::clamp(220.0 * (1.0 - progress * 0.35), 0.0, 220.0));

    SDL_SetRenderDrawColor(sdl_renderer, 255, 92, 28, outer_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y, outer_radius);
    SDL_SetRenderDrawColor(sdl_renderer, 255, 170, 48, mid_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y, mid_radius);
    SDL_SetRenderDrawColor(sdl_renderer, 255, 228, 108, core_alpha);
    SDL_RenderFillCircle(sdl_renderer, center_x, center_y, core_radius);

    const int spark_count = 7;
    for (int spark = 0; spark < spark_count; spark++) {
      const double angle =
          progress * 4.6 + spark * (2.0 * 3.1415926535 / spark_count);
      const int spark_start =
          std::max(2, static_cast<int>(element_size * (0.10 + 0.20 * pulse)));
      const int spark_end = std::max(
          spark_start + 2,
          static_cast<int>(element_size * (0.42 + 0.70 * progress)));
      const int x1 = center_x + static_cast<int>(std::cos(angle) * spark_start);
      const int y1 = center_y + static_cast<int>(std::sin(angle) * spark_start);
      const int x2 = center_x + static_cast<int>(std::cos(angle) * spark_end);
      const int y2 = center_y + static_cast<int>(std::sin(angle) * spark_end);
      SDL_SetRenderDrawColor(
          sdl_renderer, 255, 210 - spark * 10, 90,
          static_cast<Uint8>(std::clamp(210.0 * (1.0 - progress), 0.0, 210.0)));
      SDL_RenderDrawLine(sdl_renderer, x1, y1, x2, y2);
    }
    return;
  }

  const int skull_radius = std::max(6, static_cast<int>(element_size * 0.24f));
  SDL_SetRenderDrawColor(sdl_renderer, 240, 236, 230, 220);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y - skull_radius / 3,
                       skull_radius);

  SDL_Rect jaw_rect{center_x - skull_radius / 2, center_y, skull_radius,
                    std::max(4, skull_radius / 2)};
  SDL_SetRenderDrawColor(sdl_renderer, 240, 236, 230, 220);
  SDL_RenderFillRect(sdl_renderer, &jaw_rect);

  SDL_SetRenderDrawColor(sdl_renderer, 28, 18, 18, 230);
  SDL_RenderFillCircle(sdl_renderer, center_x - skull_radius / 3,
                       center_y - skull_radius / 2, std::max(2, skull_radius / 4));
  SDL_RenderFillCircle(sdl_renderer, center_x + skull_radius / 3,
                       center_y - skull_radius / 2, std::max(2, skull_radius / 4));
  const SDL_Point nose_points[4] = {
      {center_x, center_y - skull_radius / 5},
      {center_x - std::max(2, skull_radius / 6), center_y + skull_radius / 6},
      {center_x + std::max(2, skull_radius / 6), center_y + skull_radius / 6},
      {center_x, center_y - skull_radius / 5}};
  SDL_RenderDrawLines(sdl_renderer, nose_points, 4);

  SDL_SetRenderDrawColor(sdl_renderer, 210, 206, 198, 220);
  for (int tooth = -1; tooth <= 1; tooth++) {
    const int tooth_x = center_x + tooth * std::max(2, skull_radius / 4);
    SDL_RenderDrawLine(sdl_renderer, tooth_x, center_y + 1, tooth_x,
                       center_y + jaw_rect.h - 2);
  }
}

void Renderer::drawEndScreenOverlay(const std::string &title_text,
                                    SDL_Texture *texture,
                                    const SDL_Point &texture_size,
                                    const SDL_Color &glow_color) {
  drawDimmer(118);

  const int title_top = std::max(26, screen_res_y / 10);
  renderBrickText(sdl_font_display, title_text, screen_res_x / 2, title_top,
                  kBrickOutlineColor);

  if (texture == nullptr || texture_size.x <= 0 || texture_size.y <= 0) {
    return;
  }

  const int max_width = std::min(screen_res_x * 38 / 100, screen_res_x - 120);
  const int max_height = std::min(screen_res_y * 60 / 100, screen_res_y - 220);
  const double width_scale =
      static_cast<double>(max_width) /
      static_cast<double>(texture_size.x);
  const double height_scale =
      static_cast<double>(max_height) /
      static_cast<double>(texture_size.y);
  const double scale = std::max(0.05, std::min(width_scale, height_scale));
  const int image_width = std::max(
      1, static_cast<int>(std::lround(texture_size.x * scale)));
  const int image_height = std::max(
      1, static_cast<int>(std::lround(texture_size.y * scale)));
  const int center_x = screen_res_x / 2;
  const int center_y = screen_res_y / 2 + screen_res_y / 12;
  const SDL_Rect image_rect{center_x - image_width / 2,
                            center_y - image_height / 2, image_width,
                            image_height};

  renderDecorativeTexture(texture, image_rect, glow_color, 0.0, 0.0, 0.0, 1.0);
}

void Renderer::drawDefeatOverlay() {
  drawEndScreenOverlay("Verloren!", sdl_lose_screen_texture,
                       sdl_lose_screen_size, SDL_Color{255, 162, 120, 255});
}

void Renderer::drawVictoryOverlay() {
  drawEndScreenOverlay("Gewonnen!", sdl_win_screen_texture,
                       sdl_win_screen_size, SDL_Color{138, 224, 255, 255});
}

void Renderer::drawgoodies() {
  if (game == nullptr) {
    return;
  }

  if (game->dead) {
    return;
  }

  const Uint32 now = SDL_GetTicks();

  for (auto goodie : game->goodies) {
    if (!goodie->is_active) {
      continue;
    }

    goodie->px_coord = getPixelCoord(goodie->map_coord, 0, 0);
    sdl_goodie_rect =
        SDL_Rect{goodie->px_coord.x + int(element_size * 0.15),
                 goodie->px_coord.y + int(element_size * 0.15),
                 int(element_size * 0.7), int(element_size * 0.7)};
    SDL_RenderCopy(sdl_renderer, sdl_goodie_texture, nullptr,
                   &sdl_goodie_rect);
    renderGoodieSparkle(goodie, sdl_goodie_rect, now);
  }
}

void Renderer::renderGoodieSparkle(Goodie *goodie, const SDL_Rect &sprite_rect,
                                   Uint32 now) {
  if (goodie == nullptr) {
    return;
  }

  const Uint32 sparkle_interval_range =
      GOODIE_SPARKLE_MAX_INTERVAL_MS - GOODIE_SPARKLE_MIN_INTERVAL_MS;

  if (goodie->next_sparkle_ticks == 0) {
    goodie->next_sparkle_ticks =
        now + GOODIE_SPARKLE_MIN_INTERVAL_MS +
        static_cast<Uint32>(rand() % (sparkle_interval_range + 1));
  }

  if (goodie->sparkle_started_ticks == 0 &&
      now >= goodie->next_sparkle_ticks) {
    goodie->sparkle_started_ticks = now;
    goodie->sparkle_offset_x =
        0.10f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) *
                    0.80f;
    goodie->sparkle_offset_y =
        0.10f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) *
                    0.80f;
  }

  if (goodie->sparkle_started_ticks == 0) {
    return;
  }

  const Uint32 sparkle_age = now - goodie->sparkle_started_ticks;
  if (sparkle_age >= GOODIE_SPARKLE_DURATION_MS) {
    goodie->sparkle_started_ticks = 0;
    goodie->next_sparkle_ticks =
        now + GOODIE_SPARKLE_MIN_INTERVAL_MS +
        static_cast<Uint32>(rand() % (sparkle_interval_range + 1));
    return;
  }

  const double progress = static_cast<double>(sparkle_age) /
                          static_cast<double>(GOODIE_SPARKLE_DURATION_MS);
  const double envelope = std::sin(progress * 3.14159265358979323846);
  const int sparkle_center_x =
      sprite_rect.x +
      static_cast<int>(goodie->sparkle_offset_x * sprite_rect.w);
  const int sparkle_center_y =
      sprite_rect.y +
      static_cast<int>(goodie->sparkle_offset_y * sprite_rect.h);
  const int reference_size = std::max(sprite_rect.w, sprite_rect.h);
  const int core_radius =
      std::max(1, static_cast<int>(reference_size * 0.09 * envelope));
  const int arm_length =
      std::max(2, static_cast<int>(reference_size * 0.38 * envelope));
  const Uint8 alpha =
      static_cast<Uint8>(std::clamp(255.0 * envelope, 0.0, 255.0));

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, alpha);
  SDL_RenderDrawLine(sdl_renderer, sparkle_center_x - arm_length,
                     sparkle_center_y, sparkle_center_x + arm_length,
                     sparkle_center_y);
  SDL_RenderDrawLine(sdl_renderer, sparkle_center_x,
                     sparkle_center_y - arm_length, sparkle_center_x,
                     sparkle_center_y + arm_length);
  SDL_RenderDrawLine(sdl_renderer, sparkle_center_x - arm_length,
                     sparkle_center_y - arm_length,
                     sparkle_center_x + arm_length,
                     sparkle_center_y + arm_length);
  SDL_RenderDrawLine(sdl_renderer, sparkle_center_x - arm_length,
                     sparkle_center_y + arm_length,
                     sparkle_center_x + arm_length,
                     sparkle_center_y - arm_length);

  if (core_radius > 0) {
    SDL_RenderFillCircle(sdl_renderer, sparkle_center_x, sparkle_center_y,
                         core_radius);
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawmonsters() {
  if (game == nullptr) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  for (auto monster : game->monsters) {
    const bool waiting_for_blast =
        monster->scheduled_dynamite_blast_ticks != 0 &&
        now < monster->scheduled_dynamite_blast_ticks;
    if (!monster->is_alive && !waiting_for_blast) {
      continue;
    }

    const bool goat_stunned = monster->monster_char == GOAT &&
                              !waiting_for_blast &&
                              monster->goat_state ==
                                  Monster::GoatState::Stunned &&
                              now < monster->goat_stun_until_ticks;
    const bool goat_grazing = monster->monster_char == GOAT &&
                              !waiting_for_blast && !goat_stunned &&
                              (monster->grazing_until_ticks > now ||
                               monster->goat_pacified ||
                               (monster->goat_love_sequence_active &&
                                monster->goat_love_target_id == -1));
    const bool goat_jumping = monster->monster_char == GOAT &&
                              !waiting_for_blast && !goat_stunned &&
                              monster->goat_is_jumping;
    SDL_Texture *monster_texture =
        goat_stunned
            ? getStunnedGoatTexture()
            : getMonsterTexture(monster->monster_char,
                                monster->facing_direction, monster->id,
                                goat_grazing, goat_jumping);
    if (monster_texture == nullptr) {
      continue;
    }

    if (waiting_for_blast) {
      monster->px_coord = getPixelCoord(monster->scheduled_dynamite_blast_coord, 0, 0);
    } else {
      monster->px_coord = getPixelCoord(
          monster->map_coord, element_size * monster->px_delta.x / 100.0,
          element_size * monster->px_delta.y / 100.0);
    }
    sdl_monster_rect = MonsterRenderRect(monster->px_coord, element_size);
    if (goat_stunned) {
      const double sway = std::sin(static_cast<double>(now) / 120.0) *
                          element_size * 0.04;
      sdl_monster_rect.x += static_cast<int>(std::lround(sway));
    }
    if (waiting_for_blast) {
      const Uint32 remaining_ticks = monster->scheduled_dynamite_blast_ticks - now;
      const double blink = std::sin(static_cast<double>(now) / 60.0);
      const Uint8 warning_alpha = static_cast<Uint8>(
          std::clamp(120.0 + 110.0 * std::max(0.0, blink), 0.0, 255.0));
      SDL_SetRenderDrawColor(sdl_renderer, 255, 120, 48, warning_alpha / 2);
      SDL_RenderFillCircle(sdl_renderer, sdl_monster_rect.x + sdl_monster_rect.w / 2,
                           sdl_monster_rect.y + sdl_monster_rect.h / 2,
                           std::max(7, static_cast<int>(element_size * 0.44)));
      const int alpha_mod = std::min(255, 170 + static_cast<int>(remaining_ticks % 80));
      SDL_SetTextureAlphaMod(
          monster_texture, static_cast<Uint8>(alpha_mod));
    } else {
      SDL_SetTextureAlphaMod(monster_texture, 255);
    }
    if (monster->monster_char == GOAT && monster->goat_love_sequence_active) {
      const bool red_flash = (static_cast<int>(now / 80) % 2) == 0;
      SDL_SetTextureColorMod(monster_texture, 255, red_flash ? 72 : 128,
                             red_flash ? 72 : 128);
    }
    SDL_RenderCopy(sdl_renderer, monster_texture, nullptr, &sdl_monster_rect);
    SDL_SetTextureColorMod(monster_texture, 255, 255, 255);
    if (monster->is_electrified) {
      drawElectrifiedMonsterAura(sdl_monster_rect, monster->electrified_visual_seed,
                                 now, monster->electrified_charge_target_id != -1);
    }
    SDL_SetTextureAlphaMod(monster_texture, 255);
    if (monster->monster_char == GOAT && monster->goat_love_sequence_active) {
      const double pulse =
          0.82 + 0.18 * std::sin(static_cast<double>(now) / 105.0);
      drawPulsingHeart(
          sdl_monster_rect.x + sdl_monster_rect.w / 2,
          sdl_monster_rect.y - static_cast<int>(element_size * 0.20),
          std::max(8, static_cast<int>(element_size * 0.38)), 230, pulse);
    }
    if (monster->monster_char == GOAT &&
        monster->goat_state == Monster::GoatState::Stunned &&
        now < monster->goat_stun_until_ticks) {
      drawStunStars(sdl_monster_rect.x + sdl_monster_rect.w / 2,
                    sdl_monster_rect.y -
                        static_cast<int>(element_size * 0.25),
                    static_cast<int>(element_size *
                                     STUN_STARS_ORBIT_RADIUS_CELLS),
                    static_cast<int>(element_size * STUN_STARS_RADIUS_CELLS),
                    now);
    }
  }
}

void Renderer::drawaliens() {
  if (game == nullptr) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  for (const AlienAgent &alien : game->aliens) {
    const bool residue_visible = AlienExplosionResidueVisible(alien, now);
    if (!alien.is_alive && alien.explosion_spawned && !residue_visible) {
      continue;
    }

    SDL_Texture *alien_texture = nullptr;
    SDL_Point source_size{0, 0};
    if (residue_visible && !sdl_alien_explosion_textures.empty()) {
      alien_texture = sdl_alien_explosion_textures.back();
      if (!sdl_alien_explosion_sizes.empty()) {
        source_size = sdl_alien_explosion_sizes.back();
      }
    } else if (!sdl_alien_textures.empty()) {
      const size_t texture_index =
          AlienTextureIndexForFrame(getAlienAnimationFrame(alien, now));
      if (texture_index < sdl_alien_textures.size()) {
        alien_texture = sdl_alien_textures[texture_index];
      }
      if (texture_index < sdl_alien_sizes.size()) {
        source_size = sdl_alien_sizes[texture_index];
      }
    }
    if (alien_texture == nullptr) {
      continue;
    }

    const PixelCoord alien_px = getPixelCoord(alien.coord, 0, 0);
    SDL_Rect alien_rect = AlienRenderRect(alien_px, element_size, source_size);
    if (residue_visible) {
      alien_rect = AlienExplosionRenderRect(
          alien_px, element_size, source_size,
          ALIEN_EXPLOSION_FINAL_FRAME_SCALE);
    }
    SDL_RenderCopy(sdl_renderer, alien_texture, nullptr, &alien_rect);
    if (alien.is_alive) {
      drawAlienThoughtBubble(alien, alien_rect, now);
    }
  }
}

void Renderer::drawfireballs() {
  if (game == nullptr || game->dead) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const int non_character_sprite_lift_px = getNonCharacterSpriteLiftPixels();
  for (const auto &fireball : game->fireballs) {
    const MapCoord next_coord = StepRenderCoord(fireball.current_coord,
                                                fireball.direction);
    const bool next_is_wall =
        ENABLE_3D_VIEW && map != nullptr && next_coord.u >= 0 &&
        next_coord.v >= 0 && next_coord.u < static_cast<int>(rows) &&
        next_coord.v < static_cast<int>(cols) &&
        map->map_entry(static_cast<size_t>(next_coord.u),
                       static_cast<size_t>(next_coord.v)) ==
            ElementType::TYPE_WALL;
    const PixelCoord start_px = getPixelCoord(fireball.current_coord, 0, 0);
    const PixelCoord end_px = getPixelCoord(next_coord, 0, 0);
    const double progress = std::clamp(
        static_cast<double>(now - fireball.segment_started_ticks) /
            static_cast<double>(std::max<Uint32>(1, fireball.step_duration_ms)),
        0.0, 1.0);
    const double rendered_progress = progress * (next_is_wall ? 0.5 : 1.0);

    const int start_center_x = start_px.x + element_size / 2;
    const int start_center_y = start_px.y + element_size / 2;
    const int end_center_x = end_px.x + element_size / 2;
    const int end_center_y = end_px.y + element_size / 2;
    const int center_x =
        start_center_x +
        static_cast<int>((end_center_x - start_center_x) * rendered_progress);
    const int center_y =
        start_center_y +
        static_cast<int>((end_center_y - start_center_y) * rendered_progress) -
        non_character_sprite_lift_px;
    const int trail_x =
        center_x - static_cast<int>((end_center_x - start_center_x) * 0.28);
    const int trail_y =
        center_y - static_cast<int>((end_center_y - start_center_y) * 0.28);
    const int half_extent = std::max(8, element_size / 3);
    const SDL_Rect occlusion_bounds{center_x - half_extent, center_y - half_extent,
                                    half_extent * 2, half_extent * 2};
    const double center_row_cells =
        static_cast<double>(fireball.current_coord.u) + 0.5 +
        static_cast<double>(next_coord.u - fireball.current_coord.u) *
            rendered_progress;

    const auto draw_fireball = [&]() {
      SDL_SetRenderDrawColor(sdl_renderer, 255, 110, 24, 120);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                           std::max(4, element_size / 4));
      SDL_SetRenderDrawColor(sdl_renderer, 255, 190, 70, 175);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                           std::max(3, element_size / 6));
      SDL_SetRenderDrawColor(sdl_renderer, 255, 230, 140, 220);
      SDL_RenderDrawLine(sdl_renderer, trail_x, trail_y, center_x, center_y);
    };
    if (ENABLE_3D_VIEW) {
      drawWithWallOcclusion(occlusion_bounds, center_row_cells, draw_fireball);
      continue;
    }
    draw_fireball();
  }
}

void Renderer::drawslimeballs() {
  if (game == nullptr || game->dead) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const int non_character_sprite_lift_px = getNonCharacterSpriteLiftPixels();
  for (const auto &slimeball : game->slimeballs) {
    const MapCoord next_coord = StepRenderCoord(slimeball.current_coord,
                                                slimeball.direction);
    const bool next_is_wall =
        ENABLE_3D_VIEW && map != nullptr && next_coord.u >= 0 &&
        next_coord.v >= 0 && next_coord.u < static_cast<int>(rows) &&
        next_coord.v < static_cast<int>(cols) &&
        map->map_entry(static_cast<size_t>(next_coord.u),
                       static_cast<size_t>(next_coord.v)) ==
            ElementType::TYPE_WALL;
    const double progress = std::clamp(
        static_cast<double>(now - slimeball.segment_started_ticks) /
            static_cast<double>(std::max<Uint32>(1, slimeball.step_duration_ms)),
        0.0, 1.0);
    const double rendered_progress = progress * (next_is_wall ? 0.5 : 1.0);
    const PixelCoord start_px = getPixelCoord(slimeball.current_coord, 0, 0);
    const PixelCoord end_px = getPixelCoord(next_coord, 0, 0);

    const int start_center_x = start_px.x + element_size / 2;
    const int start_center_y = start_px.y + element_size / 2;
    const int end_center_x = end_px.x + element_size / 2;
    const int end_center_y = end_px.y + element_size / 2;
    const int center_x =
        start_center_x +
        static_cast<int>((end_center_x - start_center_x) * rendered_progress);
    const int center_y =
        start_center_y +
        static_cast<int>((end_center_y - start_center_y) * rendered_progress) -
        non_character_sprite_lift_px;
    const int max_dimension = std::max(
        1, static_cast<int>(std::lround(element_size * kSlimeBallRenderScale)));
    const int half_extent = std::max(6, max_dimension / 2) + 6;
    const SDL_Rect occlusion_bounds{center_x - half_extent, center_y - half_extent,
                                    half_extent * 2, half_extent * 2};
    const double center_row_cells =
        static_cast<double>(slimeball.current_coord.u) + 0.5 +
        static_cast<double>(next_coord.u - slimeball.current_coord.u) *
            rendered_progress;

    const int trail_x =
        center_x - static_cast<int>((end_center_x - start_center_x) * 0.28);
    const int trail_y =
        center_y - static_cast<int>((end_center_y - start_center_y) * 0.28);
    const auto draw_slimeball = [&]() {
      if (sdl_slime_ball_texture != nullptr && sdl_slime_ball_size.x > 0 &&
          sdl_slime_ball_size.y > 0) {
        const double scale =
            static_cast<double>(max_dimension) /
            static_cast<double>(
                std::max(sdl_slime_ball_size.x, sdl_slime_ball_size.y));
        const int draw_width = std::max(
            1, static_cast<int>(std::lround(sdl_slime_ball_size.x * scale)));
        const int draw_height = std::max(
            1, static_cast<int>(std::lround(sdl_slime_ball_size.y * scale)));
        const SDL_Rect draw_rect{center_x - draw_width / 2,
                                 center_y - draw_height / 2, draw_width,
                                 draw_height};

        double angle_degrees = 0.0;
        switch (slimeball.direction) {
        case Directions::Up:
          angle_degrees = -90.0;
          break;
        case Directions::Down:
          angle_degrees = 90.0;
          break;
        case Directions::Left:
          angle_degrees = 180.0;
          break;
        case Directions::Right:
        case Directions::None:
        default:
          angle_degrees = 0.0;
          break;
        }

        SDL_SetTextureAlphaMod(sdl_slime_ball_texture, kSlimeBallBaseAlpha);
        SDL_RenderCopyEx(sdl_renderer, sdl_slime_ball_texture, nullptr,
                         &draw_rect, angle_degrees, nullptr, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(sdl_slime_ball_texture, 255);
        return;
      }

      SDL_SetRenderDrawColor(sdl_renderer, 88, 255, 72, 90);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                           std::max(4, element_size / 4));
      SDL_SetRenderDrawColor(sdl_renderer, 174, 255, 112, 148);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                           std::max(3, element_size / 6));
      SDL_SetRenderDrawColor(sdl_renderer, 148, 240, 96, 165);
      SDL_RenderDrawLine(sdl_renderer, trail_x, trail_y, center_x, center_y);
    };
    if (ENABLE_3D_VIEW) {
      drawWithWallOcclusion(occlusion_bounds, center_row_cells, draw_slimeball);
      continue;
    }
    draw_slimeball();
  }
}

void Renderer::drawgasclouds() {
  if (game == nullptr || game->dead) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  const int non_character_sprite_lift_px = getNonCharacterSpriteLiftPixels();
  for (const auto &cloud : game->gas_clouds) {
    double alpha_factor = 1.0;
    if (cloud.is_fading) {
      alpha_factor =
          1.0 - std::clamp(static_cast<double>(now - cloud.fade_started_ticks) /
                               static_cast<double>(GAS_CLOUD_FADE_MS),
                           0.0, 1.0);
    }

    if (alpha_factor <= 0.0) {
      continue;
    }

    const PixelCoord cloud_px = getPixelCoord(cloud.coord, 0, 0);
    const int center_x = cloud_px.x + element_size / 2;
    const int center_y =
        cloud_px.y + element_size / 2 - non_character_sprite_lift_px;

    const Uint32 elapsed = now - cloud.started_ticks;
    double scale_multiplier = 1.0;
    int drift_x = 0;
    int drift_y = 0;
    if (elapsed >= kFartCloudOpenDurationMs) {
      const double wobble_phase =
          static_cast<double>(now + cloud.animation_seed * 53) / 260.0;
      const double scale_phase =
          static_cast<double>(now + cloud.animation_seed * 97) / 360.0;
      drift_x = static_cast<int>(
          std::lround(std::sin(wobble_phase) * element_size *
                      kFartCloudDriftXFactor));
      drift_y = static_cast<int>(
          std::lround(std::cos(wobble_phase * 1.19) * element_size *
                      kFartCloudDriftYFactor));
      scale_multiplier =
          1.0 + std::sin(scale_phase) * kFartCloudScalePulseAmplitude;
    }

    const int base_max_dimension =
        std::max(1, static_cast<int>(std::lround(element_size *
                                                 kFartCloudRenderScale)));
    const int max_dimension = std::max(
        1, static_cast<int>(std::lround(base_max_dimension * scale_multiplier)));
    const int render_center_x = center_x + drift_x;
    const int render_center_y = center_y + drift_y;
    const int half_extent = std::max(8, max_dimension / 2) + 4;
    const SDL_Rect occlusion_bounds{render_center_x - half_extent,
                                    render_center_y - half_extent,
                                    half_extent * 2, half_extent * 2};
    const auto draw_cloud = [&]() {
      const double wobble_clock =
          static_cast<double>(now + cloud.animation_seed * 67) / 210.0;
      const int base_radius =
          std::max(5, static_cast<int>(std::lround(element_size * 0.23)));
      drawOrganicGasCloud(render_center_x, render_center_y, base_radius,
                          wobble_clock, alpha_factor, cloud.animation_seed,
                          scale_multiplier);
    };
    if (ENABLE_3D_VIEW) {
      drawWithWallOcclusion(
          occlusion_bounds, static_cast<double>(cloud.coord.u) + 0.5, draw_cloud);
      continue;
    }
    draw_cloud();
  }
}

void Renderer::draweffects() {
  if (game == nullptr) {
    return;
  }

  const Uint32 now = SDL_GetTicks();
  for (const auto &effect : game->effects) {
    if (now < effect.started_ticks) {
      continue;
    }

    int center_x = 0;
    int center_y = 0;
    if (ENABLE_3D_VIEW) {
      const SDL_FPoint world_center =
          effect.has_precise_world_center
              ? projectScene(effect.precise_world_center.x,
                             effect.precise_world_center.y, 0.0)
              : projectScene(static_cast<double>(effect.coord.v) + 0.5,
                             static_cast<double>(effect.coord.u) + 0.5, 0.0);
      center_x = static_cast<int>(std::lround(world_center.x));
      center_y = static_cast<int>(std::lround(world_center.y));
    } else if (effect.has_precise_world_center) {
      center_x = static_cast<int>(
          std::lround(static_cast<double>(offset_x) +
                      static_cast<double>(effect.precise_world_center.x) *
                          static_cast<double>(element_size + 1)));
      center_y = static_cast<int>(
          std::lround(static_cast<double>(offset_y) +
                      static_cast<double>(effect.precise_world_center.y) *
                          static_cast<double>(element_size + 1)));
    } else {
      const PixelCoord effect_px = getPixelCoord(effect.coord, 0, 0);
      center_x = effect_px.x + element_size / 2;
      center_y = effect_px.y + element_size / 2;
    }
    const Uint32 elapsed = now - effect.started_ticks;

    if (effect.type == EffectType::AlienExplosion) {
      const double progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(
                             std::max<Uint32>(1, ALIEN_EXPLOSION_DURATION_MS)),
                     0.0, 1.0);
      const double bloom = std::sin(progress * M_PI);
      const int frame_index = std::clamp(
          static_cast<int>(elapsed /
                           std::max<Uint32>(1, ALIEN_EXPLOSION_FRAME_MS)),
          0, kAlienExplosionFrames - 1);
      const double frame_scale =
          frame_index == kAlienExplosionFrames - 1
              ? ALIEN_EXPLOSION_FINAL_FRAME_SCALE
              : 1.0;
      const int render_size = std::max(
          element_size,
          static_cast<int>(std::lround(
              element_size * ALIEN_EXPLOSION_RENDER_SCALE * frame_scale *
              (0.96 + 0.10 * bloom))));

      if (frame_index >= 0 &&
          frame_index < static_cast<int>(sdl_alien_explosion_textures.size())) {
        SDL_Texture *frame_texture =
            sdl_alien_explosion_textures[static_cast<size_t>(frame_index)];
        if (frame_texture != nullptr) {
          const int anchor_y =
              center_y + static_cast<int>(std::lround(element_size * 0.18));
          const SDL_Point frame_size =
              frame_index < static_cast<int>(sdl_alien_explosion_sizes.size())
                  ? sdl_alien_explosion_sizes[static_cast<size_t>(frame_index)]
                  : SDL_Point{0, 0};
          const SDL_Rect dest_rect = BottomAnchoredTextureRect(
              center_x, anchor_y, render_size, frame_size);
          SDL_SetTextureAlphaMod(frame_texture, 255);
          SDL_RenderCopy(sdl_renderer, frame_texture, nullptr, &dest_rect);
          SDL_SetTextureAlphaMod(frame_texture, 255);
        }
      }
    } else if (effect.type == EffectType::AirstrikeExplosion) {
      const Uint32 total_duration = AIRSTRIKE_EXPLOSION_DURATION_MS;
      const int frame_index = std::clamp(
          static_cast<int>(elapsed / std::max<Uint32>(1, AIRSTRIKE_EXPLOSION_FRAME_MS)),
          0, kAirstrikeExplosionFrames - 1);

      if (sdl_airstrike_explosion_texture != nullptr &&
          sdl_airstrike_explosion_size.x > 0 &&
          sdl_airstrike_explosion_size.y > 0) {
        const int src_x =
            (frame_index * sdl_airstrike_explosion_size.x) /
            kAirstrikeExplosionFrames;
        const int src_x_next =
            ((frame_index + 1) * sdl_airstrike_explosion_size.x) /
            kAirstrikeExplosionFrames;
        const SDL_Rect src_rect{src_x, 0, std::max(1, src_x_next - src_x),
                                sdl_airstrike_explosion_size.y};
        const double fade =
            1.0 - std::clamp(static_cast<double>(elapsed) /
                                 static_cast<double>(std::max<Uint32>(1, total_duration)),
                             0.0, 1.0);
        const int render_size = std::max(
            element_size,
            static_cast<int>(std::lround(
                element_size * (1.25 + effect.radius_cells * 0.75))));
        const SDL_Rect dest_rect{center_x - render_size / 2,
                                 center_y - render_size / 2, render_size,
                                 render_size};
        SDL_SetRenderDrawColor(
            sdl_renderer, 255, 176, 84,
            static_cast<Uint8>(std::clamp(76.0 * fade, 0.0, 76.0)));
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                             std::max(8, render_size / 3));
        SDL_SetTextureAlphaMod(
            sdl_airstrike_explosion_texture,
            static_cast<Uint8>(std::clamp(255.0 * fade, 0.0, 255.0)));
        SDL_RenderCopy(sdl_renderer, sdl_airstrike_explosion_texture, &src_rect,
                       &dest_rect);
        SDL_SetTextureAlphaMod(sdl_airstrike_explosion_texture, 255);
      } else {
        const double progress =
            std::clamp(static_cast<double>(elapsed) /
                           static_cast<double>(std::max<Uint32>(1, total_duration)),
                       0.0, 1.0);
        const int radius = std::max(
            6, static_cast<int>(element_size * (0.18 + 0.48 * progress)));
        const Uint8 alpha =
            static_cast<Uint8>(std::clamp(220.0 * (1.0 - progress), 0.0, 220.0));
        SDL_SetRenderDrawColor(sdl_renderer, 255, 132, 36, alpha / 2);
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius + 4);
        SDL_SetRenderDrawColor(sdl_renderer, 255, 232, 140, alpha);
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius);
      }
    } else if (effect.type == EffectType::BiohazardImpactFlash) {
      const double progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(BIOHAZARD_IMPACT_FLASH_DURATION_MS),
                     0.0, 1.0);
      const double pulse = std::sin(progress * kLogoPi);
      const double fade = std::pow(1.0 - progress, 0.28);
      const int outer_radius =
          std::max(5, static_cast<int>(std::lround(
                          element_size * (0.12 + 0.42 * pulse))));
      const int mid_radius =
          std::max(3, static_cast<int>(std::lround(
                          element_size * (0.08 + 0.28 * pulse))));
      const int core_radius =
          std::max(2, static_cast<int>(std::lround(
                          element_size * (0.05 + 0.16 * pulse))));
      const Uint8 glow_alpha = static_cast<Uint8>(
          std::clamp(212.0 * fade, 0.0, 212.0));
      const Uint8 core_alpha = static_cast<Uint8>(
          std::clamp(255.0 * fade, 0.0, 255.0));

      SDL_SetRenderDrawColor(sdl_renderer, 42, 118, 255, glow_alpha / 2);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, outer_radius + 4);
      SDL_SetRenderDrawColor(sdl_renderer, 126, 218, 255, glow_alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, outer_radius);
      SDL_SetRenderDrawColor(sdl_renderer, 212, 244, 255, core_alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, mid_radius);
      SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, core_alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, core_radius);

      for (int spark = 0; spark < 6; ++spark) {
        const double angle = progress * 4.0 + spark * (2.0 * kLogoPi / 6.0);
        const int spark_radius = std::max(
            4, static_cast<int>(std::lround(outer_radius * (0.78 + 0.12 * spark))));
        const int spark_x = center_x +
                            static_cast<int>(std::lround(std::cos(angle) * spark_radius));
        const int spark_y = center_y +
                            static_cast<int>(std::lround(std::sin(angle) * spark_radius));
        SDL_SetRenderDrawColor(sdl_renderer, 186, 238, 255, glow_alpha);
        SDL_RenderDrawLine(sdl_renderer, center_x, center_y, spark_x, spark_y);
      }
    } else if (effect.type == EffectType::PlasmaShockwave) {
      const double progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(PLASMA_SHOCKWAVE_DURATION_MS),
                     0.0, 1.0);
      const double flash = std::max(0.0, 1.0 - progress * 1.9);
      const double burst_phase =
          std::sin(std::clamp(progress / 0.34, 0.0, 1.0) * kLogoPi);
      const int max_radius =
          std::max(element_size * std::max(7, effect.radius_cells),
                   static_cast<int>(std::hypot(
                       static_cast<double>(screen_res_x),
                       static_cast<double>(screen_res_y)) *
                                    0.58));
      const int flash_radius = std::max(
          element_size,
          static_cast<int>(element_size * (0.85 + 2.8 * std::pow(progress, 0.56))));
      const int white_burst_radius = std::max(
          element_size / 2,
          static_cast<int>(std::lround(
              element_size * (0.30 + 4.6 * burst_phase))));
      const std::array<SDL_Color, 7> ring_colors{{
          {28, 86, 255, 255},
          {52, 128, 255, 255},
          {78, 176, 255, 255},
          {108, 228, 255, 255},
          {84, 255, 236, 255},
          {170, 238, 255, 255},
          {218, 246, 255, 255},
      }};

      if (burst_phase > 0.0) {
        SDL_SetRenderDrawColor(
            sdl_renderer, 255, 255, 255,
            static_cast<Uint8>(std::clamp(235.0 * burst_phase, 0.0, 235.0)));
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                             white_burst_radius);
        SDL_SetRenderDrawColor(
            sdl_renderer, 206, 242, 255,
            static_cast<Uint8>(std::clamp(144.0 * burst_phase, 0.0, 144.0)));
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                             white_burst_radius + std::max(5, element_size / 4));
      }

      SDL_SetRenderDrawColor(
          sdl_renderer, 38, 96, 255,
          static_cast<Uint8>(std::clamp(118.0 * flash, 0.0, 118.0)));
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, flash_radius + 18);
      SDL_SetRenderDrawColor(
          sdl_renderer, 204, 242, 255,
          static_cast<Uint8>(std::clamp(164.0 * flash, 0.0, 164.0)));
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, flash_radius);

      for (size_t ring = 0; ring < ring_colors.size(); ++ring) {
        const double ring_progress =
            std::clamp(progress * 1.56 - static_cast<double>(ring) * 0.085, 0.0,
                       1.0);
        if (ring_progress <= 0.0) {
          continue;
        }

        const int ring_radius = std::max(
            10, static_cast<int>(
                    std::lround(max_radius * std::pow(ring_progress, 0.60))));
        const Uint8 alpha = static_cast<Uint8>(std::clamp(
            (240.0 - ring * 14.0) * std::pow(1.0 - ring_progress, 0.34), 0.0,
            255.0));
        const SDL_Color &color = ring_colors[ring];
        for (int thickness = -2; thickness <= 2; ++thickness) {
          SDL_SetRenderDrawColor(
              sdl_renderer, color.r, color.g, color.b,
              static_cast<Uint8>(std::max(0, alpha - std::abs(thickness) * 48)));
          SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                               std::max(1, ring_radius + thickness));
        }

        SDL_SetRenderDrawColor(
            sdl_renderer, color.r, color.g, color.b,
            static_cast<Uint8>(std::clamp(alpha * 0.22, 0.0, 92.0)));
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                             std::max(6, ring_radius / 4));
      }
    } else if (effect.type == EffectType::NuclearExplosion) {
      if (elapsed < NUCLEAR_FLASH_DURATION_MS) {
        const double flash_t =
            static_cast<double>(elapsed) /
            static_cast<double>(std::max<Uint32>(1, NUCLEAR_FLASH_DURATION_MS));
        constexpr double kFlashRiseFraction = 0.12;
        double flash_intensity;
        if (flash_t < kFlashRiseFraction) {
          flash_intensity = flash_t / kFlashRiseFraction;
        } else {
          flash_intensity = std::pow(
              std::max(0.0, 1.0 - (flash_t - kFlashRiseFraction) /
                                      (1.0 - kFlashRiseFraction)),
              1.6);
        }
        const Uint8 flash_alpha = static_cast<Uint8>(std::clamp(
            flash_intensity * static_cast<double>(NUCLEAR_FLASH_PEAK_ALPHA),
            0.0, 255.0));
        if (flash_alpha > 0) {
          SDL_BlendMode prev_blend;
          SDL_GetRenderDrawBlendMode(sdl_renderer, &prev_blend);
          SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(sdl_renderer, NUCLEAR_FLASH_COLOR.r,
                                 NUCLEAR_FLASH_COLOR.g, NUCLEAR_FLASH_COLOR.b,
                                 flash_alpha);
          SDL_Rect flash_rect{0, 0, screen_res_x, screen_res_y};
          SDL_RenderFillRect(sdl_renderer, &flash_rect);
          SDL_SetRenderDrawBlendMode(sdl_renderer, prev_blend);
        }
      }
      const double progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(NUCLEAR_EXPLOSION_TOTAL_DURATION_MS),
                     0.0, 1.0);
      drawNuclearFireball(
          center_x, center_y, elapsed,
          static_cast<Uint32>(effect.started_ticks) ^
              (static_cast<Uint32>(effect.coord.u) * 73856093u) ^
              (static_cast<Uint32>(effect.coord.v) * 19349663u));

      const double ring_progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(
                             std::max<Uint32>(1, NUCLEAR_SMOKE_RING_EXPANSION_MS)),
                     0.0, 1.0);
      if (ring_progress > 0.0) {
        const int ring_radius = std::max(
            element_size * std::max(2, effect.radius_cells),
            static_cast<int>(std::lround(
                static_cast<double>(element_size + 1) *
                NUCLEAR_SMOKE_RING_FINAL_RADIUS_CELLS *
                std::pow(ring_progress, 0.62))));
        const Uint8 ring_alpha = static_cast<Uint8>(std::clamp(
            168.0 * std::pow(1.0 - ring_progress, 0.32), 0.0, 168.0));
        for (int thickness = -6; thickness <= 6; ++thickness) {
          SDL_SetRenderDrawColor(
              sdl_renderer, 132, 126, 118,
              static_cast<Uint8>(
                  std::max(0, ring_alpha - std::abs(thickness) * 12)));
          SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                               std::max(1, ring_radius + thickness));
        }
      }

      const double mushroom_progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(std::max<Uint32>(
                             1, NUCLEAR_MUSHROOM_BUILD_MS)),
                     0.0, 1.0);
      if (mushroom_progress > 0.0) {
        const int stem_height_px = std::max(
            16, static_cast<int>(std::lround(
                    element_size * (0.95 + mushroom_progress *
                                           NUCLEAR_MUSHROOM_STEM_HEIGHT_CELLS *
                                           0.74))));
        const int stem_radius_x = std::max(
            4, static_cast<int>(std::lround(
                   element_size * (0.10 + 0.04 * (1.0 - mushroom_progress)))));
        const int stem_radius_y = std::max(
            10, static_cast<int>(std::lround(stem_height_px * 0.58)));
        const int stem_center_y = center_y - stem_height_px / 2;
        const int cap_radius_x = std::max(
            16, static_cast<int>(std::lround(
                    element_size * (0.95 + mushroom_progress *
                                           NUCLEAR_MUSHROOM_CAP_RADIUS_CELLS *
                                           0.92))));
        const int cap_radius_y = std::max(
            12, static_cast<int>(std::lround(cap_radius_x * 0.44)));
        const int cap_center_y =
            center_y - stem_height_px - std::max(
                                      4, static_cast<int>(std::lround(
                                             element_size *
                                             (0.24 + mushroom_progress * 0.22))));
        const Uint8 mushroom_alpha = static_cast<Uint8>(std::clamp(
            132.0 * (1.0 - progress * 0.50), 0.0, 132.0));
        SDL_RenderFillEllipseAA(
            sdl_renderer, center_x + 1.5, stem_center_y + 2.0,
            static_cast<double>(stem_radius_x + 1),
            static_cast<double>(stem_radius_y + 1),
            SDL_Color{70, 68, 64,
                      static_cast<Uint8>(
                          std::clamp(mushroom_alpha * 0.76, 0.0, 255.0))});
        SDL_RenderFillEllipseAA(
            sdl_renderer, center_x, stem_center_y,
            static_cast<double>(stem_radius_x),
            static_cast<double>(stem_radius_y),
            SDL_Color{98, 94, 90,
                      static_cast<Uint8>(
                          std::clamp(mushroom_alpha * 0.92, 0.0, 255.0))});

        SDL_RenderFillEllipseAA(
            sdl_renderer, center_x + 2.0, cap_center_y + 2.0,
            static_cast<double>(cap_radius_x + 2),
            static_cast<double>(cap_radius_y + 2),
            SDL_Color{66, 64, 62,
                      static_cast<Uint8>(
                          std::clamp(mushroom_alpha * 0.82, 0.0, 255.0))});
        SDL_RenderFillEllipseAA(
            sdl_renderer, center_x, cap_center_y,
            static_cast<double>(cap_radius_x),
            static_cast<double>(cap_radius_y),
            SDL_Color{98, 94, 90, mushroom_alpha});
        SDL_RenderFillEllipseAA(
            sdl_renderer, center_x, cap_center_y - cap_radius_y * 0.22,
            static_cast<double>(cap_radius_x * 0.72),
            static_cast<double>(cap_radius_y * 0.46),
            SDL_Color{134, 128, 122,
                      static_cast<Uint8>(
                          std::clamp(mushroom_alpha * 0.64, 0.0, 255.0))});
        SDL_RenderFillEllipseAA(
            sdl_renderer, center_x, cap_center_y + cap_radius_y * 0.16,
            static_cast<double>(cap_radius_x * 0.50),
            static_cast<double>(cap_radius_y * 0.24),
            SDL_Color{150, 144, 136,
                      static_cast<Uint8>(
                          std::clamp(mushroom_alpha * 0.42, 0.0, 255.0))});
      }
    } else if (effect.type == EffectType::NuclearExplosionB) {
      drawNuclearBExplosion(effect, center_x, center_y, elapsed);
    } else if (effect.type == EffectType::DynamiteExplosion) {
      const double progress =
          std::clamp(static_cast<double>(elapsed) /
                         static_cast<double>(DYNAMITE_EXPLOSION_DURATION_MS),
                     0.0, 1.0);
      const double bloom = std::sin(progress * 3.1415926535);
      const int max_radius =
          std::max(element_size,
                   static_cast<int>(element_size * effect.radius_cells * 1.02));
      const int outer_radius =
          std::max(10, static_cast<int>(max_radius * (0.28 + 0.92 * progress)));
      const int mid_radius =
          std::max(7, static_cast<int>(max_radius * (0.18 + 0.58 * bloom)));
      const int core_radius =
          std::max(4, static_cast<int>(max_radius * (0.08 + 0.28 * bloom)));
      const Uint8 outer_alpha =
          static_cast<Uint8>(std::clamp(168.0 * (1.0 - progress * 0.72), 0.0, 180.0));
      const Uint8 mid_alpha =
          static_cast<Uint8>(std::clamp(210.0 * (1.0 - progress * 0.46), 0.0, 220.0));
      const Uint8 core_alpha =
          static_cast<Uint8>(std::clamp(240.0 * (1.0 - progress * 0.30), 0.0, 255.0));

      SDL_SetRenderDrawColor(sdl_renderer, 255, 78, 20, outer_alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, outer_radius);
      SDL_SetRenderDrawColor(sdl_renderer, 255, 154, 52, mid_alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, mid_radius);
      SDL_SetRenderDrawColor(sdl_renderer, 255, 232, 138, core_alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, core_radius);

      const int spark_count = 12;
      for (int spark = 0; spark < spark_count; spark++) {
        const double angle =
            progress * 5.8 + spark * (2.0 * 3.1415926535 / spark_count);
        const int spark_start =
            std::max(6, static_cast<int>(max_radius * (0.10 + 0.20 * bloom)));
        const int spark_end =
            std::max(spark_start + 4,
                     static_cast<int>(max_radius * (0.56 + 0.46 * progress)));
        const int x1 = center_x + static_cast<int>(std::cos(angle) * spark_start);
        const int y1 = center_y + static_cast<int>(std::sin(angle) * spark_start);
        const int x2 = center_x + static_cast<int>(std::cos(angle) * spark_end);
        const int y2 = center_y + static_cast<int>(std::sin(angle) * spark_end);
        SDL_SetRenderDrawColor(
            sdl_renderer, 255, 224 - spark * 5, 112,
            static_cast<Uint8>(std::clamp(220.0 * (1.0 - progress), 0.0, 220.0)));
        SDL_RenderDrawLine(sdl_renderer, x1, y1, x2, y2);
      }

      SDL_SetRenderDrawColor(
          sdl_renderer, 255, 248, 180,
          static_cast<Uint8>(std::clamp(120.0 * (1.0 - progress), 0.0, 120.0)));
      SDL_RenderDrawCircle(sdl_renderer, center_x, center_y,
                           std::max(outer_radius - element_size / 4, mid_radius + 1));
    } else if (effect.type == EffectType::MonsterExplosion) {
      drawMonsterFireball(center_x, center_y, elapsed,
                          static_cast<Uint32>(effect.started_ticks) ^
                              (static_cast<Uint32>(effect.coord.u) * 73856093u) ^
                              (static_cast<Uint32>(effect.coord.v) * 19349663u));
      const int frame_index = std::clamp(
          static_cast<int>(elapsed / std::max<Uint32>(1, MONSTER_EXPLOSION_FRAME_MS)),
          0, kMonsterExplosionFrames - 1);
      const double fade_in_progress = std::clamp(
          static_cast<double>(elapsed) /
              static_cast<double>(
                  std::max<Uint32>(1, MONSTER_EXPLOSION_FADE_IN_DURATION_MS)),
          0.0, 1.0);
      const double fade_in_opacity =
          MONSTER_EXPLOSION_INITIAL_OPACITY +
          (1.0 - MONSTER_EXPLOSION_INITIAL_OPACITY) * fade_in_progress;
      const int fade_out_tail_frames = std::clamp(
          MONSTER_EXPLOSION_FADE_OUT_TAIL_FRAME_COUNT, 1, kMonsterExplosionFrames);
      const int fade_out_start_frame =
          kMonsterExplosionFrames - fade_out_tail_frames;
      double fade_out_opacity = 1.0;
      if (frame_index >= fade_out_start_frame) {
        const int tail_frame_index = frame_index - fade_out_start_frame;
        const double tail_progress =
            (fade_out_tail_frames <= 1)
                ? 1.0
                : std::clamp(
                      static_cast<double>(tail_frame_index) /
                          static_cast<double>(fade_out_tail_frames - 1),
                      0.0, 1.0);
        fade_out_opacity =
            1.0 + (MONSTER_EXPLOSION_FINAL_OPACITY - 1.0) * tail_progress;
      }
      const double visible_opacity =
          std::clamp(fade_in_opacity * fade_out_opacity, 0.0, 1.0);

      if (sdl_monster_explosion_texture != nullptr &&
          sdl_monster_explosion_size.x > 0 &&
          sdl_monster_explosion_size.y > 0) {
        const int src_x =
            (frame_index * sdl_monster_explosion_size.x) /
            kMonsterExplosionFrames;
        const int src_x_next =
            ((frame_index + 1) * sdl_monster_explosion_size.x) /
            kMonsterExplosionFrames;
        const SDL_Rect src_rect{src_x, 0, std::max(1, src_x_next - src_x),
                                sdl_monster_explosion_size.y};
        const int effect_radius = std::max(1, effect.radius_cells);
        const int render_size = std::max(
            element_size,
            static_cast<int>(std::lround(element_size *
                                         MONSTER_EXPLOSION_RENDER_SCALE *
                                         effect_radius)));
        const int anchor_y =
            center_y + static_cast<int>(std::lround(element_size * 0.18));
        const int glow_radius =
            std::max(8, static_cast<int>(std::lround(render_size * 0.22)));
        SDL_SetRenderDrawColor(
            sdl_renderer, 255, 118, 46,
            static_cast<Uint8>(
                std::clamp(92.0 * visible_opacity, 0.0, 92.0)));
        SDL_RenderFillCircle(
            sdl_renderer, center_x,
            anchor_y - static_cast<int>(std::lround(render_size * 0.08)),
            glow_radius);

        const SDL_Rect dest_rect{center_x - render_size / 2,
                                 anchor_y - render_size, render_size,
                                 render_size};
        SDL_SetTextureAlphaMod(
            sdl_monster_explosion_texture,
            static_cast<Uint8>(
                std::clamp(255.0 * visible_opacity, 0.0, 255.0)));
        SDL_RenderCopy(sdl_renderer, sdl_monster_explosion_texture, &src_rect,
                       &dest_rect);
        SDL_SetTextureAlphaMod(sdl_monster_explosion_texture, 255);
      } else {
        const double progress =
            std::clamp(static_cast<double>(elapsed) /
                           static_cast<double>(MONSTER_EXPLOSION_DURATION_MS),
                       0.0, 1.0);
        const int effect_radius = std::max(1, effect.radius_cells);
        const int outer_radius = std::max(
            5, static_cast<int>(element_size * effect_radius *
                                (0.22 + 0.50 * progress)));
        const int inner_radius = std::max(
            3, static_cast<int>(element_size * effect_radius *
                                (0.12 + 0.26 * progress)));
        const Uint8 alpha = static_cast<Uint8>(
            std::clamp(210.0 * visible_opacity, 0.0, 210.0));
        SDL_SetRenderDrawColor(sdl_renderer, 255, 88, 40, alpha / 2);
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y, outer_radius);
        SDL_SetRenderDrawColor(sdl_renderer, 255, 180, 72, alpha);
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y, inner_radius);
      }
    } else if (effect.type == EffectType::SlimeSplash) {
      const Uint32 animation_duration_ms =
          SLIME_SPLASH_FRAME_MS * SLIME_SPLASH_FRAME_COUNT;
      const int frame_index = std::clamp(
          static_cast<int>(elapsed / std::max<Uint32>(1, SLIME_SPLASH_FRAME_MS)),
          0, SLIME_SPLASH_FRAME_COUNT - 1);
      double alpha_factor = 1.0;
      if (elapsed > animation_duration_ms) {
        alpha_factor =
            1.0 -
            std::clamp(static_cast<double>(elapsed - animation_duration_ms) /
                           static_cast<double>(SLIME_SPLASH_FADE_MS),
                       0.0, 1.0);
      }

      if (sdl_slime_splash_texture != nullptr && sdl_slime_splash_size.x > 0 &&
          sdl_slime_splash_size.y > 0) {
        const int column = frame_index % kSlimeSplashGridColumns;
        const int row = frame_index / kSlimeSplashGridColumns;
        const int src_x =
            (column * sdl_slime_splash_size.x) / kSlimeSplashGridColumns;
        const int src_x_next =
            ((column + 1) * sdl_slime_splash_size.x) / kSlimeSplashGridColumns;
        const int src_y =
            (row * sdl_slime_splash_size.y) / kSlimeSplashGridRows;
        const int src_y_next =
            ((row + 1) * sdl_slime_splash_size.y) / kSlimeSplashGridRows;
        const SDL_Rect src_rect{src_x, src_y, std::max(1, src_x_next - src_x),
                                std::max(1, src_y_next - src_y)};
        const int max_dimension = std::max(
            1, static_cast<int>(std::lround(element_size * kSlimeSplashRenderScale)));
        const double scale =
            static_cast<double>(max_dimension) /
            static_cast<double>(std::max(src_rect.w, src_rect.h));
        const int draw_width =
            std::max(1, static_cast<int>(std::lround(src_rect.w * scale)));
        const int draw_height =
            std::max(1, static_cast<int>(std::lround(src_rect.h * scale)));
        const SDL_Rect dest_rect{center_x - draw_width / 2,
                                 center_y - draw_height / 2, draw_width,
                                 draw_height};

        SDL_SetTextureAlphaMod(
            sdl_slime_splash_texture,
            static_cast<Uint8>(std::clamp(kSlimeSplashBaseAlpha * alpha_factor,
                                          0.0, 255.0)));
        SDL_RenderCopy(sdl_renderer, sdl_slime_splash_texture, &src_rect,
                       &dest_rect);
        SDL_SetTextureAlphaMod(sdl_slime_splash_texture, 255);
      } else {
        const double growth =
            std::clamp(static_cast<double>(std::min(elapsed, animation_duration_ms)) /
                           static_cast<double>(std::max<Uint32>(
                               1, animation_duration_ms)),
                       0.0, 1.0);
        const int radius = std::max(
            6, static_cast<int>(element_size * (0.18 + 0.34 * growth)));
        const Uint8 alpha = static_cast<Uint8>(
            std::clamp(180.0 * alpha_factor, 0.0, 180.0));
        SDL_SetRenderDrawColor(sdl_renderer, 86, 255, 72, alpha / 2);
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius + 3);
        SDL_SetRenderDrawColor(sdl_renderer, 170, 255, 124, alpha);
        SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius);
      }
    } else {
      const double progress =
          std::clamp(static_cast<double>(elapsed) / 180.0, 0.0, 1.0);
      const int radius =
          std::max(3, static_cast<int>(element_size * (0.14 + 0.12 * progress)));
      const Uint8 alpha =
          static_cast<Uint8>(std::clamp(180.0 * (1.0 - progress), 0.0, 180.0));
      SDL_SetRenderDrawColor(sdl_renderer, 255, 210, 150, alpha);
      SDL_RenderFillCircle(sdl_renderer, center_x, center_y, radius);
      SDL_SetRenderDrawColor(sdl_renderer, 210, 190, 180, alpha);
      SDL_RenderDrawLine(sdl_renderer, center_x - radius, center_y - radius,
                         center_x + radius, center_y + radius);
      SDL_RenderDrawLine(sdl_renderer, center_x - radius, center_y + radius,
                         center_x + radius, center_y - radius);
    }
  }
}

void Renderer::drawmap() {
  if (map == nullptr) {
    return;
  }

  if (ENABLE_3D_VIEW) {
    drawmap3DFloors();
    drawNuclearCraters();
    drawmap3DWallTops();
    for (size_t u = 0; u < rows; ++u) {
      for (size_t v = 0; v < cols; ++v) {
        if (map->map_entry(u, v) != ElementType::TYPE_WALL) {
          continue;
        }
        drawWallFrontFace3D(static_cast<int>(u), static_cast<int>(v));
      }
    }
    return;
  }

  SDL_Rect block;
  block.w = element_size;
  block.h = element_size;
  SDL_Texture *floor_texture = getSelectedFloorTexture();
  const SDL_Point floor_texture_size = getSelectedFloorTextureSize();
  SDL_SetRenderDrawColor(sdl_renderer, 50, 50, 50, 0xFF);

  for (size_t i = 0; i < cols; i++) {
    for (size_t j = 0; j < rows; j++) {
      const ElementType entry = map->map_entry(j, i);
      const int x = offset_x + 1 + static_cast<int>(i) * (element_size + 1);
      const int y = offset_y + 1 + static_cast<int>(j) * (element_size + 1);

      switch (entry) {
      case ElementType::TYPE_WALL:
        break;
      case ElementType::TYPE_PATH:
      case ElementType::TYPE_TELEPORTER:
      case ElementType::TYPE_GOODIE:
      case ElementType::TYPE_PACMAN:
      case ElementType::TYPE_MONSTER:
      default:
        block.x = x;
        block.y = y;
        if (floor_texture != nullptr) {
          const SDL_Rect source_rect = MakeFloorTextureSampleRect(
              floor_texture_size, static_cast<int>(j), static_cast<int>(i));
          SDL_RenderCopy(sdl_renderer, floor_texture, &source_rect, &block);
        } else {
          SDL_SetRenderDrawColor(sdl_renderer, COLOR_PATH, 0xFF);
          SDL_RenderFillRect(sdl_renderer, &block);
        }
        break;
      }
    }
  }

  drawNuclearCraters();

  for (size_t i = 0; i < cols; i++) {
    for (size_t j = 0; j < rows; j++) {
      if (map->map_entry(j, i) != ElementType::TYPE_WALL) {
        continue;
      }

      const int x = offset_x + 1 + static_cast<int>(i) * (element_size + 1);
      const int y = offset_y + 1 + static_cast<int>(j) * (element_size + 1);
      sdl_wall_rect = SDL_Rect{x, y, element_size + 1, element_size + 1};
      drawWallTile(
          sdl_wall_rect,
          j > 0 && map->map_entry(j - 1, i) == ElementType::TYPE_WALL,
          i + 1 < cols && map->map_entry(j, i + 1) == ElementType::TYPE_WALL,
          j + 1 < rows && map->map_entry(j + 1, i) == ElementType::TYPE_WALL,
          i > 0 && map->map_entry(j, i - 1) == ElementType::TYPE_WALL, 255,
          static_cast<int>(j * 92821 + i * 68917 + 11), false);
    }
  }
}

SDL_FPoint Renderer::projectScene(double col, double row,
                                  double z_cells) const {
  const double tile = static_cast<double>(element_size) + 1.0;
  const double world_x = col * tile;
  const double world_y = row * tile;
  const double world_z = z_cells * static_cast<double>(wall_height_px);
  SDL_FPoint out;
  out.x = static_cast<float>(offset_x + 1.0 + world_x);
  out.y = static_cast<float>(
      static_cast<double>(scene_origin_y) + world_y * cos_tilt - world_z * sin_tilt);
  return out;
}

void Renderer::drawTexturedQuad(SDL_FPoint tl, SDL_FPoint tr, SDL_FPoint bl,
                                SDL_FPoint br, SDL_Texture *texture,
                                Uint8 shade) {
  const SDL_Color tint{shade, shade, shade, 255};
  SDL_Vertex verts[4];
  verts[0].position = tl;
  verts[0].color = tint;
  verts[0].tex_coord = SDL_FPoint{0.0f, 0.0f};
  verts[1].position = tr;
  verts[1].color = tint;
  verts[1].tex_coord = SDL_FPoint{1.0f, 0.0f};
  verts[2].position = bl;
  verts[2].color = tint;
  verts[2].tex_coord = SDL_FPoint{0.0f, 1.0f};
  verts[3].position = br;
  verts[3].color = tint;
  verts[3].tex_coord = SDL_FPoint{1.0f, 1.0f};
  const int indices[6] = {0, 1, 2, 1, 3, 2};
  SDL_RenderGeometry(sdl_renderer, texture, verts, 4, indices, 6);
}

void Renderer::drawFloorTile3D(int row, int col) {
  SDL_Texture *floor_texture = getSelectedFloorTexture();
  const SDL_Point floor_texture_size = getSelectedFloorTextureSize();
  const SDL_FPoint tl = projectScene(col, row, 0.0);
  const SDL_FPoint tr = projectScene(col + 1.0, row, 0.0);
  const SDL_FPoint bl = projectScene(col, row + 1.0, 0.0);
  const SDL_FPoint br = projectScene(col + 1.0, row + 1.0, 0.0);
  const auto has_wall = [&](int probe_row, int probe_col) {
    return map != nullptr && probe_row >= 0 && probe_col >= 0 &&
           probe_row < static_cast<int>(rows) &&
           probe_col < static_cast<int>(cols) &&
           map->map_entry(static_cast<size_t>(probe_row),
                          static_cast<size_t>(probe_col)) ==
               ElementType::TYPE_WALL;
  };

  const bool wall_up = has_wall(row - 1, col);
  const bool wall_right = has_wall(row, col + 1);
  const bool wall_down = has_wall(row + 1, col);
  const bool wall_left = has_wall(row, col - 1);
  const bool wall_up_left = has_wall(row - 1, col - 1);
  const bool wall_up_right = has_wall(row - 1, col + 1);
  const bool wall_down_left = has_wall(row + 1, col - 1);
  const bool wall_down_right = has_wall(row + 1, col + 1);

  const float row_factor =
      rows > 1 ? static_cast<float>(row) / static_cast<float>(rows - 1) : 0.0f;
  const float tile_variation =
      (TileNoise01(row, col) * 2.0f - 1.0f) *
      static_cast<float>(FLOOR_3D_TILE_VARIATION_STRENGTH);
  const SDL_Color top_base =
      floor_texture != nullptr
          ? LerpColor(FLOOR_3D_TEXTURE_NEAR_COLOR, FLOOR_3D_TEXTURE_FAR_COLOR,
                      row_factor)
          : LerpColor(FLOOR_3D_NEAR_COLOR, FLOOR_3D_FAR_COLOR, row_factor);
  const SDL_Color bottom_base =
      ScaleColor(top_base, floor_texture != nullptr ? 0.90f : 0.82f);

  const auto vertex_factor = [&](float open_bonus, float occlusion) -> float {
    return std::clamp(1.0f + tile_variation + open_bonus -
                          occlusion *
                              static_cast<float>(FLOOR_3D_WALL_OCCLUSION_STRENGTH),
                      0.55f, 1.24f);
  };
  const float edge_highlight =
      static_cast<float>(FLOOR_3D_OPEN_EDGE_HIGHLIGHT_STRENGTH);
  const float top_left_occlusion =
      (wall_up ? 1.0f : 0.0f) + (wall_left ? 1.0f : 0.0f) +
      (wall_up_left ? 0.65f : 0.0f);
  const float top_right_occlusion =
      (wall_up ? 1.0f : 0.0f) + (wall_right ? 1.0f : 0.0f) +
      (wall_up_right ? 0.65f : 0.0f);
  const float bottom_left_occlusion =
      (wall_down ? 0.60f : 0.0f) + (wall_left ? 1.0f : 0.0f) +
      (wall_down_left ? 0.45f : 0.0f);
  const float bottom_right_occlusion =
      (wall_down ? 0.60f : 0.0f) + (wall_right ? 1.0f : 0.0f) +
      (wall_down_right ? 0.45f : 0.0f);

  const SDL_Color top_left_color = ScaleColor(
      top_base, vertex_factor((!wall_up ? edge_highlight : 0.0f) +
                                  (!wall_left ? edge_highlight * 0.45f : 0.0f),
                              top_left_occlusion));
  const SDL_Color top_right_color = ScaleColor(
      top_base, vertex_factor((!wall_up ? edge_highlight : 0.0f) +
                                  (!wall_right ? edge_highlight * 0.45f : 0.0f),
                              top_right_occlusion));
  const SDL_Color bottom_left_color = ScaleColor(
      bottom_base,
      vertex_factor(-0.05f + (!wall_down ? edge_highlight * 0.20f : 0.0f) +
                        (!wall_left ? edge_highlight * 0.15f : 0.0f),
                    bottom_left_occlusion));
  const SDL_Color bottom_right_color = ScaleColor(
      bottom_base,
      vertex_factor(-0.05f + (!wall_down ? edge_highlight * 0.20f : 0.0f) +
                        (!wall_right ? edge_highlight * 0.15f : 0.0f),
                    bottom_right_occlusion));
  const SDL_Rect source_rect =
      MakeFloorTextureSampleRect(floor_texture_size, row, col);
  const float inv_texture_width =
      floor_texture != nullptr && floor_texture_size.x > 0
          ? 1.0f / static_cast<float>(floor_texture_size.x)
          : 0.0f;
  const float inv_texture_height =
      floor_texture != nullptr && floor_texture_size.y > 0
          ? 1.0f / static_cast<float>(floor_texture_size.y)
          : 0.0f;
  const float u0 = static_cast<float>(source_rect.x) * inv_texture_width;
  const float v0 = static_cast<float>(source_rect.y) * inv_texture_height;
  const float u1 =
      static_cast<float>(source_rect.x + source_rect.w) * inv_texture_width;
  const float v1 =
      static_cast<float>(source_rect.y + source_rect.h) * inv_texture_height;
  SDL_Vertex verts[4];
  verts[0].position = tl;
  verts[0].color = top_left_color;
  verts[0].tex_coord = SDL_FPoint{u0, v0};
  verts[1].position = tr;
  verts[1].color = top_right_color;
  verts[1].tex_coord = SDL_FPoint{u1, v0};
  verts[2].position = bl;
  verts[2].color = bottom_left_color;
  verts[2].tex_coord = SDL_FPoint{u0, v1};
  verts[3].position = br;
  verts[3].color = bottom_right_color;
  verts[3].tex_coord = SDL_FPoint{u1, v1};
  const int indices[6] = {0, 1, 2, 1, 3, 2};
  SDL_RenderGeometry(sdl_renderer, floor_texture, verts, 4, indices, 6);
}

void Renderer::drawWallTop3D(int row, int col) {
  const auto has_wall = [&](int probe_row, int probe_col) {
    return probe_row >= 0 && probe_col >= 0 &&
           probe_row < static_cast<int>(rows) &&
           probe_col < static_cast<int>(cols) &&
           map->map_entry(static_cast<size_t>(probe_row),
                          static_cast<size_t>(probe_col)) ==
               ElementType::TYPE_WALL;
  };
  const SDL_Rect top_rect = getWallTopRect(row, col);
  // The front edge of the top face meets the vertical wall face and should
  // stay closed to avoid notches between both faces.
  drawWallTile(top_rect, has_wall(row - 1, col), has_wall(row, col + 1),
               true, has_wall(row, col - 1), 255,
               row * 92821 + col * 68917 + 101, false);
}

SDL_Rect Renderer::getWallTopRect(int row, int col) const {
  const SDL_FPoint top_back_left = projectScene(col, row, 1.0);
  const SDL_FPoint top_back_right = projectScene(col + 1.0, row, 1.0);
  const SDL_FPoint top_front_left = projectScene(col, row + 1.0, 1.0);
  const SDL_FPoint top_front_right = projectScene(col + 1.0, row + 1.0, 1.0);
  return makeProjectedFaceRect(top_back_left, top_back_right, top_front_left,
                               top_front_right);
}

SDL_Rect Renderer::getWallFrontFaceRect(int row, int col) const {
  const SDL_FPoint fl_top = projectScene(col, row + 1.0, 1.0);
  const SDL_FPoint fr_top = projectScene(col + 1.0, row + 1.0, 1.0);
  const SDL_FPoint fl_bot = projectScene(col, row + 1.0, 0.0);
  const SDL_FPoint fr_bot = projectScene(col + 1.0, row + 1.0, 0.0);
  return makeProjectedFaceRect(fl_top, fr_top, fl_bot, fr_bot);
}

void Renderer::drawWithWallOcclusion(const SDL_Rect &draw_bounds,
                                     double row_cells,
                                     const std::function<void()> &draw) {
  if (!ENABLE_3D_VIEW || map == nullptr) {
    draw();
    return;
  }

  const SDL_Rect screen_rect{0, 0, screen_res_x, screen_res_y};
  const SDL_Rect clipped_bounds = intersectRects(draw_bounds, screen_rect);
  if (!hasVisibleArea(clipped_bounds)) {
    return;
  }

  std::vector<SDL_Rect> visible_regions{clipped_bounds};
  const int search_start_row =
      std::max(0, static_cast<int>(std::floor(row_cells)) + 1);
  bool has_occluder = false;
  const auto subtract_occluder = [&](const SDL_Rect &occluder_rect) {
    const SDL_Rect clipped_occluder =
        intersectRects(occluder_rect, clipped_bounds);
    if (!hasVisibleArea(clipped_occluder)) {
      return;
    }

    has_occluder = true;
    std::vector<SDL_Rect> next_regions;
    next_regions.reserve(visible_regions.size() * 4);
    for (const SDL_Rect &region : visible_regions) {
      subtractRect(region, clipped_occluder, next_regions);
    }
    visible_regions = std::move(next_regions);
  };

  for (int wall_row = search_start_row; wall_row < static_cast<int>(rows);
       ++wall_row) {
    for (int col = 0; col < static_cast<int>(cols); ++col) {
      if (map->map_entry(static_cast<size_t>(wall_row),
                         static_cast<size_t>(col)) != ElementType::TYPE_WALL) {
        continue;
      }

      subtract_occluder(getWallTopRect(wall_row, col));
      if (visible_regions.empty()) {
        return;
      }

      if (wall_row + 1 < static_cast<int>(rows) &&
          map->map_entry(static_cast<size_t>(wall_row + 1),
                         static_cast<size_t>(col)) == ElementType::TYPE_WALL) {
        continue;
      }

      subtract_occluder(getWallFrontFaceRect(wall_row, col));
      if (visible_regions.empty()) {
        return;
      }
    }
  }

  if (!has_occluder) {
    draw();
    return;
  }

  for (const SDL_Rect &region : visible_regions) {
    SDL_RenderSetClipRect(sdl_renderer, &region);
    draw();
  }
  SDL_RenderSetClipRect(sdl_renderer, nullptr);
}

void Renderer::drawWallFrontFace3D(int row, int col) {
  const bool has_wall_down =
      row + 1 < static_cast<int>(rows) &&
      map->map_entry(static_cast<size_t>(row + 1), static_cast<size_t>(col)) ==
          ElementType::TYPE_WALL;
  if (has_wall_down) {
    return;
  }

  const auto has_wall = [&](int probe_row, int probe_col) {
    return probe_row >= 0 && probe_col >= 0 &&
           probe_row < static_cast<int>(rows) &&
           probe_col < static_cast<int>(cols) &&
           map->map_entry(static_cast<size_t>(probe_row),
                          static_cast<size_t>(probe_col)) ==
               ElementType::TYPE_WALL;
  };
  const SDL_Rect front_rect = getWallFrontFaceRect(row, col);
  // The top edge of the front face joins the top face and must not be carved.
  drawWallTile(front_rect, true, has_wall(row, col + 1), false,
               has_wall(row, col - 1), 170,
               row * 92821 + col * 68917 + 202, true);
}

void Renderer::drawWallTile3D(int row, int col, bool has_wall_down) {
  drawWallTop3D(row, col);
  if (!has_wall_down) {
    drawWallFrontFace3D(row, col);
  }
}

void Renderer::drawmap3DBase() {
  drawmap3DFloors();
  drawmap3DWallTops();
}

void Renderer::drawmap3DFloors() {
  if (map == nullptr) {
    return;
  }

  for (size_t u = 0; u < rows; ++u) {
    for (size_t v = 0; v < cols; ++v) {
      if (map->map_entry(u, v) == ElementType::TYPE_WALL) {
        continue;
      }
      drawFloorTile3D(static_cast<int>(u), static_cast<int>(v));
    }
  }
}

void Renderer::drawmap3DWallTops() {
  if (map == nullptr) {
    return;
  }

  for (size_t u = 0; u < rows; ++u) {
    for (size_t v = 0; v < cols; ++v) {
      if (map->map_entry(u, v) != ElementType::TYPE_WALL) {
        continue;
      }
      drawWallTop3D(static_cast<int>(u), static_cast<int>(v));
    }
  }
}

void Renderer::drawmap3D() {
  if (map == nullptr) {
    return;
  }

  drawmap3DBase();
  for (size_t u = 0; u < rows; ++u) {
    for (size_t v = 0; v < cols; ++v) {
      if (map->map_entry(u, v) != ElementType::TYPE_WALL) {
        continue;
      }
      drawWallFrontFace3D(static_cast<int>(u), static_cast<int>(v));
    }
  }
}

SDL_Rect Renderer::makeFloorAlignedRect(double col, double row,
                                        double x_offset_factor,
                                        double y_offset_factor,
                                        double width_factor,
                                        double height_factor,
                                        double delta_col_cells,
                                        double delta_row_cells) const {
  const SDL_FPoint top_left =
      projectScene(col + delta_col_cells, row + delta_row_cells, 0.0);
  const SDL_FPoint bottom_right = projectScene(col + 1.0 + delta_col_cells,
                                               row + 1.0 + delta_row_cells, 0.0);
  const double tile_width = static_cast<double>(element_size) + 1.0;
  const double tile_height =
      std::max(1.0, static_cast<double>(bottom_right.y - top_left.y));
  const int x = static_cast<int>(
      std::lround(static_cast<double>(top_left.x) + tile_width * x_offset_factor));
  const int y = static_cast<int>(
      std::lround(static_cast<double>(top_left.y) + tile_height * y_offset_factor));
  const int w =
      std::max(1, static_cast<int>(std::lround(tile_width * width_factor)));
  const int h =
      std::max(1, static_cast<int>(std::lround(tile_height * height_factor)));
  return SDL_Rect{x, y, w, h};
}

SDL_Rect Renderer::makeBillboardRect(double col, double row,
                                     double width_factor,
                                     double height_factor,
                                     double foot_row_factor,
                                     double delta_col_cells,
                                     double delta_row_cells) const {
  const int width =
      std::max(1, static_cast<int>(std::lround(element_size * width_factor)));
  const int height =
      std::max(1, static_cast<int>(std::lround(element_size * height_factor)));
  const SDL_FPoint foot =
      projectScene(col + 0.5 + delta_col_cells,
                   row + foot_row_factor + delta_row_cells, 0.0);
  return SDL_Rect{
      static_cast<int>(std::lround(static_cast<double>(foot.x) - width / 2.0)),
      static_cast<int>(std::lround(static_cast<double>(foot.y) - height)), width,
      height};
}

int Renderer::getNonCharacterSpriteLiftPixels() const {
  if (!ENABLE_3D_VIEW) {
    return 0;
  }

  return std::max(
      1, static_cast<int>(std::lround(static_cast<double>(element_size) *
                                      kNonCharacterSpriteLiftFactor)));
}

SDL_Texture *Renderer::getSelectedFloorTexture() const {
  if (sdl_floor_textures.empty()) {
    return nullptr;
  }

  const int clamped_index = ClampFloorTextureIndex(selected_floor_texture_index);
  if (clamped_index >= static_cast<int>(sdl_floor_textures.size())) {
    return nullptr;
  }
  return sdl_floor_textures[clamped_index];
}

SDL_Point Renderer::getSelectedFloorTextureSize() const {
  if (sdl_floor_texture_sizes.empty()) {
    return SDL_Point{0, 0};
  }

  const int clamped_index = ClampFloorTextureIndex(selected_floor_texture_index);
  if (clamped_index >= static_cast<int>(sdl_floor_texture_sizes.size())) {
    return SDL_Point{0, 0};
  }
  return sdl_floor_texture_sizes[clamped_index];
}

void Renderer::drawFloorSpriteShadow(const SDL_Rect &sprite_rect, Uint8 alpha,
                                     double width_scale,
                                     double height_scale) {
  if (!ENABLE_3D_VIEW || alpha == 0 || sprite_rect.w <= 0 || sprite_rect.h <= 0) {
    return;
  }

  const int center_x = sprite_rect.x + sprite_rect.w / 2;
  const int center_y =
      sprite_rect.y + sprite_rect.h -
      std::max(1, static_cast<int>(std::lround(
                      static_cast<double>(element_size) *
                      CHARACTER_GROUND_SHADOW_Y_OFFSET_FACTOR)));
  const double clamped_width_scale = std::max(0.2, width_scale);
  const double clamped_height_scale = std::max(0.2, height_scale);
  const int outer_radius_x = std::max(
      4, static_cast<int>(std::lround(static_cast<double>(sprite_rect.w) *
                                      CHARACTER_GROUND_SHADOW_WIDTH_FACTOR *
                                      clamped_width_scale)));
  const int outer_radius_y = std::max(
      2, static_cast<int>(std::lround(static_cast<double>(sprite_rect.h) *
                                      CHARACTER_GROUND_SHADOW_HEIGHT_FACTOR *
                                      clamped_height_scale)));
  const int inner_radius_x =
      std::max(3, static_cast<int>(std::lround(outer_radius_x * 0.68)));
  const int inner_radius_y =
      std::max(1, static_cast<int>(std::lround(outer_radius_y * 0.65)));

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  SDL_SetRenderDrawColor(
      sdl_renderer, 90, 88, 100,
      static_cast<Uint8>(std::clamp(static_cast<double>(alpha) * 0.55, 0.0,
                                    255.0)));
  SDL_RenderFillEllipse(sdl_renderer, center_x, center_y, outer_radius_x,
                        outer_radius_y);

  SDL_SetRenderDrawColor(sdl_renderer, 70, 68, 82, alpha);
  SDL_RenderFillEllipse(sdl_renderer, center_x, center_y, inner_radius_x,
                        inner_radius_y);

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::carveRoundedWallCorner(const SDL_Rect &wall_rect, int radius,
                                      bool align_left, bool align_top) {
  if (radius <= 0) {
    return;
  }

  SDL_SetRenderDrawColor(sdl_renderer, COLOR_PATH, 0xFF);
  const double radius_value = static_cast<double>(radius);
  for (int row = 0; row < radius; ++row) {
    const double dy = radius_value - (static_cast<double>(row) + 0.5);
    const double inside_x = std::sqrt(std::max(
        0.0, radius_value * radius_value - dy * dy));
    const int cut_width = std::clamp(
        static_cast<int>(std::ceil(radius_value - inside_x)), 0, radius);
    if (cut_width <= 0) {
      continue;
    }

    SDL_Rect cut_rect{
        align_left ? wall_rect.x : wall_rect.x + wall_rect.w - cut_width,
        align_top ? wall_rect.y + row : wall_rect.y + wall_rect.h - 1 - row,
        cut_width, 1};
    SDL_RenderFillRect(sdl_renderer, &cut_rect);
  }
}

void Renderer::drawWallTile(const SDL_Rect &wall_rect, bool has_wall_up,
                            bool has_wall_right, bool has_wall_down,
                            bool has_wall_left, Uint8 shade, int detail_seed,
                            bool vertical_surface) {
  if (sdl_wall_texture != nullptr) {
    SDL_SetTextureColorMod(sdl_wall_texture, shade, shade, shade);
    SDL_RenderCopy(sdl_renderer, sdl_wall_texture, nullptr, &wall_rect);
    SDL_SetTextureColorMod(sdl_wall_texture, 255, 255, 255);
  } else {
    const auto shade_channel = [&](Uint8 base) {
      return static_cast<Uint8>(std::lround(
          static_cast<double>(base) * static_cast<double>(shade) / 255.0));
    };
    SDL_SetRenderDrawColor(sdl_renderer, shade_channel(118), shade_channel(84),
                           shade_channel(68), 0xFF);
    SDL_RenderFillRect(sdl_renderer, &wall_rect);
  }

  drawWallVegetation(wall_rect, shade, detail_seed, vertical_surface);

  const int max_radius = std::min(wall_rect.w, wall_rect.h) / 2;
  const int corner_radius =
      std::clamp(std::max(3, std::min(wall_rect.w, wall_rect.h) / 4), 0,
                 max_radius);
  if (corner_radius <= 1) {
    return;
  }

  if (!has_wall_up && !has_wall_left) {
    carveRoundedWallCorner(wall_rect, corner_radius, true, true);
  }
  if (!has_wall_up && !has_wall_right) {
    carveRoundedWallCorner(wall_rect, corner_radius, false, true);
  }
  if (!has_wall_down && !has_wall_left) {
    carveRoundedWallCorner(wall_rect, corner_radius, true, false);
  }
  if (!has_wall_down && !has_wall_right) {
    carveRoundedWallCorner(wall_rect, corner_radius, false, false);
  }
}

void Renderer::drawWallVegetation(const SDL_Rect &wall_rect, Uint8 shade,
                                  int detail_seed, bool vertical_surface) {
  if (wall_rect.w < 12 || wall_rect.h < 12) {
    return;
  }

  const int min_dimension = std::min(wall_rect.w, wall_rect.h);
  const int margin = std::max(2, min_dimension / 9);
  const float shade_factor =
      std::clamp(static_cast<float>(shade) / 255.0f, 0.55f, 1.0f);

  SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(sdl_renderer, &previous_blend_mode);
  SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

  const SDL_Color moss_shadow_color = WithAlpha(
      ScaleColor(WALL_MOSS_DARK_COLOR, shade_factor * 0.64f),
      static_cast<Uint8>(std::clamp(WALL_MOSS_BASE_ALPHA * 0.40, 0.0, 255.0)));
  const SDL_Color moss_base_color = WithAlpha(
      ScaleColor(WALL_MOSS_MID_COLOR, shade_factor * 0.88f),
      WALL_MOSS_BASE_ALPHA);
  const SDL_Color moss_highlight_color = WithAlpha(
      ScaleColor(WALL_MOSS_HIGHLIGHT_COLOR, shade_factor * 0.96f),
      static_cast<Uint8>(std::clamp(WALL_MOSS_BASE_ALPHA * 0.48, 0.0, 255.0)));

  const SDL_Color stem_shadow_color = WithAlpha(
      ScaleColor(WALL_VINE_DARK_COLOR, shade_factor * 0.62f),
      static_cast<Uint8>(std::clamp(WALL_VINE_BASE_ALPHA * 0.42, 0.0, 255.0)));
  const SDL_Color stem_base_color = WithAlpha(
      ScaleColor(WALL_VINE_MID_COLOR, shade_factor * 0.96f),
      WALL_VINE_BASE_ALPHA);
  const SDL_Color stem_highlight_color = WithAlpha(
      ScaleColor(WALL_VINE_HIGHLIGHT_COLOR, shade_factor * 1.02f),
      static_cast<Uint8>(std::clamp(WALL_VINE_BASE_ALPHA * 0.62, 0.0, 255.0)));

  const SDL_Color leaf_shadow_color = WithAlpha(
      ScaleColor(WALL_VINE_DARK_COLOR, shade_factor * 0.64f),
      static_cast<Uint8>(std::clamp(WALL_VINE_BASE_ALPHA * 0.44, 0.0, 255.0)));
  const SDL_Color leaf_base_color = WithAlpha(
      ScaleColor(WALL_VINE_MID_COLOR, shade_factor * 1.00f),
      WALL_VINE_BASE_ALPHA);
  const SDL_Color leaf_highlight_color = WithAlpha(
      ScaleColor(WALL_VINE_HIGHLIGHT_COLOR, shade_factor * 1.04f),
      static_cast<Uint8>(std::clamp(WALL_VINE_BASE_ALPHA * 0.72, 0.0, 255.0)));

  auto random_count = [&](int seed, int minimum, int maximum) {
    const int clamped_minimum = std::min(minimum, maximum);
    const int clamped_maximum = std::max(minimum, maximum);
    return clamped_minimum +
           (std::abs(seed) % (clamped_maximum - clamped_minimum + 1));
  };

  const double moss_surface_chance =
      vertical_surface ? WALL_MOSS_SURFACE_CHANCE * 0.68
                       : WALL_MOSS_SURFACE_CHANCE;
  if (HashUnit(detail_seed + 5) < moss_surface_chance) {
    const int patch_count = random_count(
        detail_seed * 17 + 9, WALL_MOSS_PATCH_MIN_COUNT, WALL_MOSS_PATCH_MAX_COUNT);
    for (int patch = 0; patch < patch_count; ++patch) {
      const int patch_seed = detail_seed * 131 + patch * 37;
      const float radius =
          min_dimension *
          (WALL_MOSS_PATCH_RADIUS_MIN_FACTOR +
           static_cast<float>(HashUnit(patch_seed + 1)) *
               (WALL_MOSS_PATCH_RADIUS_MAX_FACTOR -
                WALL_MOSS_PATCH_RADIUS_MIN_FACTOR)) *
          (vertical_surface ? 0.82f : 1.0f);
      const float center_x = static_cast<float>(wall_rect.x + margin) +
                             static_cast<float>(wall_rect.w - margin * 2) *
                                 static_cast<float>(HashUnit(patch_seed + 2));
      const float y_bias =
          vertical_surface ? static_cast<float>(HashUnit(patch_seed + 3)) * 0.78f
                           : static_cast<float>(HashUnit(patch_seed + 3));
      const float center_y = static_cast<float>(wall_rect.y + margin) +
                             static_cast<float>(wall_rect.h - margin * 2) * y_bias;
      const int blob_count = random_count(
          patch_seed + 7, WALL_MOSS_BLOB_MIN_COUNT, WALL_MOSS_BLOB_MAX_COUNT);

      SDL_SetRenderDrawColor(
          sdl_renderer, moss_shadow_color.r, moss_shadow_color.g,
          moss_shadow_color.b,
          static_cast<Uint8>(std::clamp(moss_shadow_color.a * 0.70, 0.0, 255.0)));
      SDL_RenderFillEllipse(
          sdl_renderer, static_cast<int>(std::lround(center_x)),
          static_cast<int>(std::lround(center_y)),
          std::max(2, static_cast<int>(std::lround(radius * 0.82f))),
          std::max(2, static_cast<int>(std::lround(radius * (vertical_surface ? 0.54f
                                                                           : 0.68f)))));

      for (int blob = 0; blob < blob_count; ++blob) {
        const int blob_seed = patch_seed + blob * 19;
        const double angle = HashUnit(blob_seed + 11) * 2.0 * kLogoPi;
        const float orbit =
            radius * static_cast<float>(0.10 + 0.62 * HashUnit(blob_seed + 13));
        const float blob_x =
            center_x + std::cos(angle) * orbit * (vertical_surface ? 0.88f : 1.0f);
        const float blob_y = center_y +
                             std::sin(angle) * orbit *
                                 (vertical_surface ? 0.66f : 0.92f);
        const int blob_radius_x =
            std::max(1, static_cast<int>(std::lround(
                            radius * (0.22 + 0.30 * HashUnit(blob_seed + 17)))));
        const int blob_radius_y =
            std::max(1, static_cast<int>(std::lround(
                            blob_radius_x *
                            (0.60 + 0.28 * HashUnit(blob_seed + 23)))));

        SDL_SetRenderDrawColor(sdl_renderer, moss_shadow_color.r,
                               moss_shadow_color.g, moss_shadow_color.b,
                               moss_shadow_color.a);
        SDL_RenderFillEllipse(sdl_renderer,
                              static_cast<int>(std::lround(blob_x + 0.6f)),
                              static_cast<int>(std::lround(blob_y + 0.6f)),
                              blob_radius_x, blob_radius_y);

        const SDL_Color blob_color =
            LerpColor(moss_base_color, moss_highlight_color,
                      static_cast<float>(HashUnit(blob_seed + 29)) * 0.55f);
        SDL_SetRenderDrawColor(sdl_renderer, blob_color.r, blob_color.g,
                               blob_color.b, blob_color.a);
        SDL_RenderFillEllipse(sdl_renderer,
                              static_cast<int>(std::lround(blob_x)),
                              static_cast<int>(std::lround(blob_y)),
                              blob_radius_x, blob_radius_y);

        SDL_SetRenderDrawColor(
            sdl_renderer, moss_highlight_color.r, moss_highlight_color.g,
            moss_highlight_color.b,
            static_cast<Uint8>(std::clamp(moss_highlight_color.a * 0.55, 0.0, 255.0)));
        SDL_RenderFillEllipse(
            sdl_renderer, static_cast<int>(std::lround(blob_x - blob_radius_x * 0.18f)),
            static_cast<int>(std::lround(blob_y - blob_radius_y * 0.22f)),
            std::max(1, std::max(1, blob_radius_x - 1) / 2),
            std::max(1, std::max(1, blob_radius_y - 1) / 2));
      }
    }
  }

  const double vine_surface_chance =
      vertical_surface ? WALL_VINE_SURFACE_CHANCE
                       : WALL_VINE_SURFACE_CHANCE * 0.88;
  if (HashUnit(detail_seed + 71) < vine_surface_chance) {
    const int vine_count = random_count(detail_seed * 23 + 17, WALL_VINE_MIN_COUNT,
                                        WALL_VINE_MAX_COUNT);
    for (int vine = 0; vine < vine_count; ++vine) {
      const int vine_seed = detail_seed * 197 + vine * 53;
      const int thickness_px = std::max(
          1, static_cast<int>(std::lround(
                 min_dimension *
                 (WALL_VINE_THICKNESS_MIN_FACTOR +
                  (WALL_VINE_THICKNESS_MAX_FACTOR -
                   WALL_VINE_THICKNESS_MIN_FACTOR) *
                      HashUnit(vine_seed + 3)))));
      const int segment_count = random_count(
          vine_seed + 5, WALL_VINE_SEGMENT_MIN_COUNT, WALL_VINE_SEGMENT_MAX_COUNT);
      const float rect_left = static_cast<float>(wall_rect.x + margin);
      const float rect_right = static_cast<float>(wall_rect.x + wall_rect.w - margin);
      const float rect_top = static_cast<float>(wall_rect.y + margin);
      const float rect_bottom =
          static_cast<float>(wall_rect.y + wall_rect.h - margin);

      auto build_path = [&](SDL_FPoint start, int seg_count, int seg_seed_base,
                            float initial_drift_factor) {
        std::vector<SDL_FPoint> pts;
        pts.reserve(static_cast<size_t>(seg_count) + 1);
        pts.push_back(start);
        SDL_FPoint cur = start;
        for (int i = 0; i < seg_count; ++i) {
          const int s = seg_seed_base + i * 31;
          const float step_y =
              -static_cast<float>(wall_rect.h) *
              (WALL_VINE_STEP_MIN_FACTOR +
               static_cast<float>(HashUnit(s + 23)) *
                   (WALL_VINE_STEP_MAX_FACTOR - WALL_VINE_STEP_MIN_FACTOR)) *
              (vertical_surface ? 1.0f : 0.85f);
          const float drift_bias =
              (i == 0) ? initial_drift_factor *
                             static_cast<float>(wall_rect.w)
                       : 0.0f;
          const float step_x =
              static_cast<float>(wall_rect.w) * WALL_VINE_DRIFT_FACTOR *
                  static_cast<float>(HashSigned(s + 29)) *
                  (vertical_surface ? 1.0f : 1.18f) +
              drift_bias;
          cur.x = std::clamp(cur.x + step_x, rect_left, rect_right);
          cur.y = std::clamp(
              cur.y + step_y +
                  static_cast<float>(wall_rect.h) * 0.03f *
                      static_cast<float>(HashSigned(s + 37)),
              rect_top, rect_bottom);
          pts.push_back(cur);
        }
        return pts;
      };

      const float main_start_x =
          rect_left + (rect_right - rect_left) *
                          (0.12f + 0.76f * static_cast<float>(
                                              HashUnit(vine_seed + 11)));
      const SDL_FPoint main_start{main_start_x, rect_bottom};

      struct VinePath {
        std::vector<SDL_FPoint> points;
        int thickness;
      };
      std::vector<VinePath> paths;
      paths.push_back({build_path(main_start, segment_count, vine_seed, 0.0f),
                       thickness_px});

      if (HashUnit(vine_seed + 91) < WALL_VINE_BRANCH_CHANCE &&
          paths[0].points.size() >= 3) {
        const int branch_count = (HashUnit(vine_seed + 93) < 0.50) ? 1 : 2;
        for (int b = 0; b < branch_count; ++b) {
          const int branch_seed = vine_seed + b * 113 + 401;
          const auto &main_pts = paths[0].points;
          const int fork_idx =
              1 + static_cast<int>(std::abs(branch_seed) %
                                   static_cast<int>(main_pts.size() - 2));
          const int branch_segments = random_count(
              branch_seed + 7, WALL_VINE_BRANCH_MIN_SEGMENTS,
              WALL_VINE_BRANCH_MAX_SEGMENTS);
          const float side =
              (HashUnit(branch_seed + 13) < 0.5) ? -1.0f : 1.0f;
          const float drift = side * WALL_VINE_BRANCH_ANGLE_SPREAD *
                              static_cast<float>(
                                  0.06 + 0.10 * HashUnit(branch_seed + 17));
          const int branch_thickness = std::max(
              1, static_cast<int>(std::lround(
                     thickness_px * WALL_VINE_BRANCH_THICKNESS_FACTOR)));
          paths.push_back({build_path(main_pts[fork_idx], branch_segments,
                                      branch_seed + 200, drift),
                           branch_thickness});
        }
      }

      auto draw_vine_disc = [&](float x, float y, int radius,
                                const SDL_Color &color) {
        SDL_RenderFillCircleAA(sdl_renderer, x, y, static_cast<double>(radius),
                               color);
      };

      for (const auto &path : paths) {
        const std::vector<SDL_FPoint> &points = path.points;
        const int thickness_px = path.thickness;
      for (size_t point_index = 1; point_index < points.size(); ++point_index) {
        const SDL_FPoint &from = points[point_index - 1];
        const SDL_FPoint &to = points[point_index];
        const double dx = static_cast<double>(to.x - from.x);
        const double dy = static_cast<double>(to.y - from.y);
        const double length = std::hypot(dx, dy);
        const int samples = std::max(4, static_cast<int>(std::ceil(length / 2.8)));

        SDL_RenderDrawAALine(sdl_renderer, from.x + 0.65, from.y + 0.65,
                             to.x + 0.65, to.y + 0.65, stem_shadow_color);
        SDL_RenderDrawAALine(sdl_renderer, from.x, from.y, to.x, to.y,
                             stem_base_color);
        SDL_RenderDrawAALine(sdl_renderer, from.x - 0.22, from.y - 0.28,
                             to.x - 0.22, to.y - 0.28, stem_highlight_color);

        for (int sample = 0; sample <= samples; ++sample) {
          if (sample % 2 != 0 && thickness_px <= 1) {
            continue;
          }
          const float t = static_cast<float>(sample) / static_cast<float>(samples);
          const float px = from.x + (to.x - from.x) * t;
          const float py = from.y + (to.y - from.y) * t;
          draw_vine_disc(px + 0.55f, py + 0.55f, std::max(1, thickness_px),
                         stem_shadow_color);
          draw_vine_disc(px, py, std::max(1, thickness_px), stem_base_color);
          if (thickness_px > 1) {
            draw_vine_disc(px - thickness_px * 0.16f, py - thickness_px * 0.22f,
                           std::max(1, thickness_px - 1), stem_highlight_color);
          }
        }

        if (HashUnit(vine_seed + static_cast<int>(point_index) * 41) <
            WALL_VINE_LEAF_CHANCE) {
          const SDL_FPoint mid{
              (from.x + to.x) * 0.5f,
              (from.y + to.y) * 0.5f};
          const float inv_length =
              static_cast<float>(length > 0.001 ? 1.0 / length : 0.0);
          const float dir_x = static_cast<float>(dx) * inv_length;
          const float dir_y = static_cast<float>(dy) * inv_length;
          const float perp_x = -dir_y;
          const float perp_y = dir_x;
          const float side =
              HashUnit(vine_seed + static_cast<int>(point_index) * 59) < 0.5 ? -1.0f
                                                                             : 1.0f;
          const int leaf_rx =
              std::max(2, static_cast<int>(std::lround(thickness_px * 2.15f)));
          const int leaf_ry =
              std::max(1, static_cast<int>(std::lround(
                              leaf_rx * (0.58f + 0.20f *
                                                   static_cast<float>(HashUnit(
                                                       vine_seed + static_cast<int>(point_index) * 67))))));
          const float leaf_offset = thickness_px + 1.9f;
          const float leaf_center_x = mid.x + perp_x * side * leaf_offset;
          const float leaf_center_y = mid.y + perp_y * side * leaf_offset;

          SDL_RenderDrawAALine(
              sdl_renderer, mid.x + perp_x * side * (thickness_px * 0.55f),
              mid.y + perp_y * side * (thickness_px * 0.55f),
              leaf_center_x - perp_x * side * 0.35f,
              leaf_center_y - perp_y * side * 0.35f, stem_highlight_color);

          SDL_RenderFillEllipseAA(
              sdl_renderer, leaf_center_x + 1.0f, leaf_center_y + 1.0f,
              static_cast<double>(leaf_rx), static_cast<double>(leaf_ry),
              WithAlpha(
                  leaf_shadow_color,
                  static_cast<Uint8>(std::clamp(leaf_shadow_color.a * 0.92, 0.0,
                                                255.0))));
          SDL_RenderFillEllipseAA(sdl_renderer, leaf_center_x, leaf_center_y,
                                  static_cast<double>(leaf_rx),
                                  static_cast<double>(leaf_ry), leaf_base_color);
          SDL_RenderFillEllipseAA(
              sdl_renderer, leaf_center_x - dir_x * leaf_rx * 0.14f,
              leaf_center_y - dir_y * leaf_rx * 0.18f,
              std::max(1.0, static_cast<double>(leaf_rx) * 0.56),
              std::max(1.0, static_cast<double>(leaf_ry) * 0.48),
              WithAlpha(
                  leaf_highlight_color,
                  static_cast<Uint8>(std::clamp(leaf_highlight_color.a * 0.62, 0.0,
                                                255.0))));
          SDL_RenderDrawAALine(
              sdl_renderer, leaf_center_x - dir_x * leaf_rx * 0.55f,
              leaf_center_y - dir_y * leaf_rx * 0.55f,
              leaf_center_x + dir_x * leaf_rx * 0.55f,
              leaf_center_y + dir_y * leaf_rx * 0.55f, leaf_highlight_color);

          if (HashUnit(vine_seed + static_cast<int>(point_index) * 79) < 0.42) {
            const float secondary_side = -side;
            const float secondary_center_x =
                mid.x + perp_x * secondary_side * (leaf_offset * 0.90f);
            const float secondary_center_y =
                mid.y + perp_y * secondary_side * (leaf_offset * 0.90f);
            const int secondary_leaf_rx =
                std::max(1, static_cast<int>(std::lround(leaf_rx * 0.82f)));
            const int secondary_leaf_ry =
                std::max(1, static_cast<int>(std::lround(leaf_ry * 0.82f)));

            SDL_RenderFillEllipseAA(
                sdl_renderer, secondary_center_x + 1.0f, secondary_center_y + 1.0f,
                static_cast<double>(secondary_leaf_rx),
                static_cast<double>(secondary_leaf_ry),
                WithAlpha(
                    leaf_shadow_color,
                    static_cast<Uint8>(std::clamp(leaf_shadow_color.a * 0.78, 0.0,
                                                  255.0))));
            SDL_RenderFillEllipseAA(
                sdl_renderer, secondary_center_x, secondary_center_y,
                static_cast<double>(secondary_leaf_rx),
                static_cast<double>(secondary_leaf_ry),
                WithAlpha(
                    leaf_base_color,
                    static_cast<Uint8>(std::clamp(leaf_base_color.a * 0.94, 0.0,
                                                  255.0))));
          }
        }
      }
      }
    }
  }

  SDL_SetRenderDrawBlendMode(sdl_renderer, previous_blend_mode);
}

void Renderer::drawteleporters() {
  if (map == nullptr) {
    return;
  }

  for (size_t teleporter_index = 0;
       teleporter_index < map->get_teleporter_pairs().size(); teleporter_index++) {
    const auto &teleporter_pair = map->get_teleporter_pairs()[teleporter_index];
    const std::vector<MapCoord> positions{teleporter_pair.first,
                                          teleporter_pair.second};
    for (size_t position_index = 0; position_index < positions.size();
         position_index++) {
      if (map->map_entry(static_cast<size_t>(positions[position_index].u),
                         static_cast<size_t>(positions[position_index].v)) !=
          ElementType::TYPE_TELEPORTER) {
        continue;
      }
      const PixelCoord teleporter_px = getPixelCoord(positions[position_index], 0, 0);
      SDL_Rect teleporter_rect{teleporter_px.x + int(element_size * 0.08),
                               teleporter_px.y + int(element_size * 0.08),
                               int(element_size * 0.84),
                               int(element_size * 0.84)};
      drawTeleporterGlyph(
          teleporter_rect, teleporter_pair.digit,
          static_cast<int>(teleporter_index * 19 + position_index * 7));
    }
  }
}

void Renderer::drawLayout(const std::vector<std::string> &layout) {
  SDL_Rect block;
  block.w = element_size;
  block.h = element_size;
  SDL_Texture *floor_texture = getSelectedFloorTexture();
  const SDL_Point floor_texture_size = getSelectedFloorTextureSize();

  for (size_t row = 0; row < layout.size(); row++) {
    for (size_t col = 0; col < layout[row].size(); col++) {
      const int x = offset_x + 1 + static_cast<int>(col) * (element_size + 1);
      const int y = offset_y + 1 + static_cast<int>(row) * (element_size + 1);
      if (layout[row][col] == BRICK) {
        const auto has_layout_wall = [&](int probe_row, int probe_col) {
          return probe_row >= 0 && probe_col >= 0 &&
                 static_cast<size_t>(probe_row) < layout.size() &&
                 static_cast<size_t>(probe_col) < layout[probe_row].size() &&
                 layout[probe_row][probe_col] == BRICK;
        };
        sdl_wall_rect = SDL_Rect{x, y, element_size + 1, element_size + 1};
        drawWallTile(sdl_wall_rect, has_layout_wall(static_cast<int>(row) - 1,
                                                    static_cast<int>(col)),
                     has_layout_wall(static_cast<int>(row),
                                     static_cast<int>(col) + 1),
                     has_layout_wall(static_cast<int>(row) + 1,
                                     static_cast<int>(col)),
                     has_layout_wall(static_cast<int>(row),
                                     static_cast<int>(col) - 1),
                     255,
                     static_cast<int>(row * 92821 + col * 68917 + 303),
                     false);
      } else {
        block.x = x;
        block.y = y;
        if (floor_texture != nullptr) {
          const SDL_Rect source_rect = MakeFloorTextureSampleRect(
              floor_texture_size, static_cast<int>(row), static_cast<int>(col));
          SDL_RenderCopy(sdl_renderer, floor_texture, &source_rect, &block);
        } else {
          SDL_SetRenderDrawColor(sdl_renderer, COLOR_PATH, 0xFF);
          SDL_RenderFillRect(sdl_renderer, &block);
        }
      }
    }
  }
}

void Renderer::drawLayoutTeleporters(const std::vector<std::string> &layout) {
  for (size_t row = 0; row < layout.size(); row++) {
    for (size_t col = 0; col < layout[row].size(); col++) {
      if (!Map::IsTeleporterChar(layout[row][col])) {
        continue;
      }

      const int x = offset_x + 1 + static_cast<int>(col) * (element_size + 1);
      const int y = offset_y + 1 + static_cast<int>(row) * (element_size + 1);
      SDL_Rect teleporter_rect{x + int(element_size * 0.08),
                               y + int(element_size * 0.08),
                               int(element_size * 0.84),
                               int(element_size * 0.84)};
      drawTeleporterGlyph(
          teleporter_rect, layout[row][col],
          static_cast<int>(row * layout[row].size() + col));
    }
  }
}

void Renderer::drawTeleporterGlyph(const SDL_Rect &teleporter_rect,
                                   char teleporter_digit,
                                   int animation_seed) {
  const SDL_Color portal_color = TeleporterColor(teleporter_digit);
  const int center_x = teleporter_rect.x + teleporter_rect.w / 2;
  const int center_y = teleporter_rect.y + teleporter_rect.h / 2;
  const int max_radius = std::max(5, teleporter_rect.w / 2 - 1);
  const double time =
      static_cast<double>(SDL_GetTicks() + animation_seed * 113) / 480.0;

  SDL_SetRenderDrawColor(sdl_renderer, portal_color.r / 5, portal_color.g / 5,
                         portal_color.b / 5, 170);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(3, max_radius - 2));

  for (int arm = 0; arm < 3; arm++) {
    for (int step = 0; step < 18; step++) {
      const double progress = static_cast<double>(step) / 17.0;
      const double angle = time + arm * (2.0 * 3.1415926535 / 3.0) +
                           progress * 4.5;
      const double radius = max_radius * (0.18 + progress * 0.72);
      const int point_x =
          center_x + static_cast<int>(std::cos(angle) * radius * 1.05);
      const int point_y =
          center_y + static_cast<int>(std::sin(angle) * radius * 0.72);
      const int point_radius =
          std::max(1, static_cast<int>((1.0 - progress) * max_radius * 0.18));
      const Uint8 alpha = static_cast<Uint8>(
          std::clamp(235.0 * (1.0 - progress * 0.55), 40.0, 235.0));

      SDL_SetRenderDrawColor(sdl_renderer, portal_color.r, portal_color.g,
                             portal_color.b, alpha);
      SDL_RenderFillCircle(sdl_renderer, point_x, point_y, point_radius);

      if (step % 4 == 0) {
        SDL_SetRenderDrawColor(sdl_renderer, 246, 244, 255, alpha / 2 + 40);
        SDL_RenderFillCircle(sdl_renderer, point_x + point_radius / 2,
                             point_y - point_radius / 2, 1);
      }
    }
  }

  SDL_SetRenderDrawColor(sdl_renderer, 245, 242, 232, 165);
  SDL_RenderFillCircle(sdl_renderer, center_x, center_y,
                       std::max(2, max_radius / 5));
  SDL_SetRenderDrawColor(sdl_renderer, portal_color.r, portal_color.g,
                         portal_color.b, 190);
  SDL_RenderDrawCircle(sdl_renderer, center_x, center_y, max_radius);
}

void Renderer::drawStaticPacman() {
  if (map == nullptr) {
    return;
  }

  const MapCoord pacman_coord = map->get_coord_pacman();
  const PixelCoord pacman_px = getPixelCoord(pacman_coord, 0, 0);
  sdl_pacman_rect = SDL_Rect{pacman_px.x + int(element_size * 0.05),
                             pacman_px.y + int(element_size * 0.05),
                             int(element_size * 0.9), int(element_size * 0.9)};
  SDL_Texture *pacman_texture = getPacmanTexture(Directions::Down, false);
  if (pacman_texture != nullptr) {
    SDL_RenderCopy(sdl_renderer, pacman_texture, nullptr, &sdl_pacman_rect);
  }
}

void Renderer::drawStaticMonsters() {
  if (map == nullptr) {
    return;
  }

  for (int i = 0; i < map->get_number_monsters(); i++) {
    SDL_Texture *monster_texture =
        getMonsterTexture(map->get_char_monster(i), Directions::Down, i);
    if (monster_texture == nullptr) {
      continue;
    }

    const PixelCoord monster_px = getPixelCoord(map->get_coord_monster(i), 0, 0);
    sdl_monster_rect = MonsterRenderRect(monster_px, element_size);
    SDL_RenderCopy(sdl_renderer, monster_texture, nullptr, &sdl_monster_rect);
  }
}

void Renderer::drawStaticGoodies() {
  if (map == nullptr) {
    return;
  }

  for (int i = 0; i < map->get_number_goodies(); i++) {
    const PixelCoord goodie_px = getPixelCoord(map->get_coord_goodie(i), 0, 0);
    sdl_goodie_rect =
        SDL_Rect{goodie_px.x + int(element_size * 0.15),
                 goodie_px.y + int(element_size * 0.15),
                 int(element_size * 0.7), int(element_size * 0.7)};
    SDL_RenderCopy(sdl_renderer, sdl_goodie_texture, nullptr, &sdl_goodie_rect);
  }
}

void Renderer::drawLayoutEntities(const std::vector<std::string> &layout) {
  for (size_t row = 0; row < layout.size(); row++) {
    for (size_t col = 0; col < layout[row].size(); col++) {
      const char entry = layout[row][col];
      const int x = offset_x + 1 + static_cast<int>(col) * (element_size + 1);
      const int y = offset_y + 1 + static_cast<int>(row) * (element_size + 1);

      if (entry == GOODIE) {
        sdl_goodie_rect = SDL_Rect{x + int(element_size * 0.15),
                                   y + int(element_size * 0.15),
                                   int(element_size * 0.7),
                                   int(element_size * 0.7)};
        SDL_RenderCopy(sdl_renderer, sdl_goodie_texture, nullptr,
                       &sdl_goodie_rect);
      } else if (entry == PACMAN) {
        sdl_pacman_rect = SDL_Rect{x + int(element_size * 0.05),
                                   y + int(element_size * 0.05),
                                   int(element_size * 0.9),
                                   int(element_size * 0.9)};
        SDL_Texture *pacman_texture = getPacmanTexture(Directions::Down, false);
        if (pacman_texture != nullptr) {
          SDL_RenderCopy(sdl_renderer, pacman_texture, nullptr, &sdl_pacman_rect);
        }
      } else if (entry == MONSTER_FEW || entry == MONSTER_MEDIUM ||
                 entry == MONSTER_MANY || entry == MONSTER_EXTRA ||
                 entry == GOAT) {
        const int animation_seed =
            static_cast<int>(row * layout[row].size() + col);
        SDL_Texture *monster_texture =
            getMonsterTexture(entry, Directions::Down, animation_seed);
        if (monster_texture == nullptr) {
          continue;
        }
        sdl_monster_rect =
            MonsterRenderRect(PixelCoord{x, y}, element_size);
        SDL_RenderCopy(sdl_renderer, monster_texture, nullptr,
                       &sdl_monster_rect);
      }
    }
  }
}

int Renderer::SDL_RenderDrawCircle(SDL_Renderer *renderer, int x, int y,
                                   int radius) {
  int offsetx = 0;
  int offsety = radius;
  int d = radius - 1;
  int status = 0;

  while (offsety >= offsetx) {
    status += SDL_RenderDrawPoint(renderer, x + offsetx, y + offsety);
    status += SDL_RenderDrawPoint(renderer, x + offsety, y + offsetx);
    status += SDL_RenderDrawPoint(renderer, x - offsetx, y + offsety);
    status += SDL_RenderDrawPoint(renderer, x - offsety, y + offsetx);
    status += SDL_RenderDrawPoint(renderer, x + offsetx, y - offsety);
    status += SDL_RenderDrawPoint(renderer, x + offsety, y - offsetx);
    status += SDL_RenderDrawPoint(renderer, x - offsetx, y - offsety);
    status += SDL_RenderDrawPoint(renderer, x - offsety, y - offsetx);

    if (status < 0) {
      status = -1;
      break;
    }

    if (d >= 2 * offsetx) {
      d -= 2 * offsetx + 1;
      offsetx += 1;
    } else if (d < 2 * (radius - offsety)) {
      d += 2 * offsety - 1;
      offsety -= 1;
    } else {
      d += 2 * (offsety - offsetx - 1);
      offsety -= 1;
      offsetx += 1;
    }
  }

  return status;
}

int Renderer::SDL_RenderFillCircle(SDL_Renderer *renderer, int x, int y,
                                   int radius) {
  int offsetx = 0;
  int offsety = radius;
  int d = radius - 1;
  int status = 0;

  while (offsety >= offsetx) {
    status += SDL_RenderDrawLine(renderer, x - offsety, y + offsetx,
                                 x + offsety, y + offsetx);
    status += SDL_RenderDrawLine(renderer, x - offsetx, y + offsety,
                                 x + offsetx, y + offsety);
    status += SDL_RenderDrawLine(renderer, x - offsetx, y - offsety,
                                 x + offsetx, y - offsety);
    status += SDL_RenderDrawLine(renderer, x - offsety, y - offsetx,
                                 x + offsety, y - offsetx);

    if (status < 0) {
      status = -1;
      break;
    }

    if (d >= 2 * offsetx) {
      d -= 2 * offsetx + 1;
      offsetx += 1;
    } else if (d < 2 * (radius - offsety)) {
      d += 2 * offsety - 1;
      offsety -= 1;
    } else {
      d += 2 * (offsety - offsetx - 1);
      offsety -= 1;
      offsetx += 1;
    }
  }

  return status;
}

int Renderer::SDL_RenderFillEllipse(SDL_Renderer *renderer, int x, int y,
                                    int radius_x, int radius_y) {
  if (radius_x <= 0 || radius_y <= 0) {
    return 0;
  }

  int status = 0;
  for (int offset_y = -radius_y; offset_y <= radius_y; ++offset_y) {
    const double normalized_y =
        static_cast<double>(offset_y) / static_cast<double>(radius_y);
    const double horizontal_radius =
        static_cast<double>(radius_x) *
        std::sqrt(std::max(0.0, 1.0 - normalized_y * normalized_y));
    const int span = static_cast<int>(std::lround(horizontal_radius));
    status +=
        SDL_RenderDrawLine(renderer, x - span, y + offset_y, x + span, y + offset_y);
    if (status < 0) {
      return -1;
    }
  }

  return status;
}

void Renderer::SDL_RenderFillCircleAA(SDL_Renderer *renderer, double x, double y,
                                      double radius, SDL_Color color) {
  SDL_RenderFillEllipseAA(renderer, x, y, radius, radius, color);
}

void Renderer::SDL_RenderFillEllipseAA(SDL_Renderer *renderer, double x,
                                       double y, double radius_x,
                                       double radius_y, SDL_Color color) {
  if (renderer == nullptr || radius_x <= 0.0 || radius_y <= 0.0 ||
      color.a == 0 || sdl_solid_disc_texture == nullptr) {
    return;
  }

  SDL_SetTextureColorMod(sdl_solid_disc_texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(sdl_solid_disc_texture, color.a);
  const SDL_FRect dst{
      static_cast<float>(x - radius_x),
      static_cast<float>(y - radius_y),
      static_cast<float>(radius_x * 2.0),
      static_cast<float>(radius_y * 2.0)};
  SDL_RenderCopyF(renderer, sdl_solid_disc_texture, nullptr, &dst);
}

PixelCoord Renderer::getPixelCoord(MapCoord in_coord, int delta_x,
                                   int delta_y) {
  PixelCoord out;
  out.x = offset_x + 1 + in_coord.v * (element_size + 1) + delta_x;
  if (ENABLE_3D_VIEW) {
    const double world_y =
        in_coord.u * (element_size + 1) + static_cast<double>(delta_y);
    out.y = scene_origin_y + static_cast<int>(std::lround(world_y * cos_tilt));
  } else {
    out.y = offset_y + 1 + in_coord.u * (element_size + 1) + delta_y;
  }
  return out;
}

void Renderer::SDL_RenderDrawAALine(SDL_Renderer *renderer, double x0,
                                    double y0, double x1, double y1,
                                    SDL_Color color) {
  auto plot = [&](int px, int py, double brightness) {
    if (brightness <= 0.0) {
      return;
    }
    const double clamped = std::clamp(brightness, 0.0, 1.0);
    const Uint8 alpha = static_cast<Uint8>(std::lround(color.a * clamped));
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
    SDL_RenderDrawPoint(renderer, px, py);
  };

  const bool steep = std::fabs(y1 - y0) > std::fabs(x1 - x0);
  if (steep) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }

  const double dx = x1 - x0;
  const double dy = y1 - y0;
  const double gradient = (dx == 0.0) ? 1.0 : (dy / dx);

  auto fpart = [](double v) { return v - std::floor(v); };
  auto rfpart = [&](double v) { return 1.0 - fpart(v); };

  double xend = std::round(x0);
  double yend = y0 + gradient * (xend - x0);
  double xgap = rfpart(x0 + 0.5);
  const int xpxl1 = static_cast<int>(xend);
  const int ypxl1 = static_cast<int>(std::floor(yend));
  if (steep) {
    plot(ypxl1, xpxl1, rfpart(yend) * xgap);
    plot(ypxl1 + 1, xpxl1, fpart(yend) * xgap);
  } else {
    plot(xpxl1, ypxl1, rfpart(yend) * xgap);
    plot(xpxl1, ypxl1 + 1, fpart(yend) * xgap);
  }
  double intery = yend + gradient;

  xend = std::round(x1);
  yend = y1 + gradient * (xend - x1);
  xgap = fpart(x1 + 0.5);
  const int xpxl2 = static_cast<int>(xend);
  const int ypxl2 = static_cast<int>(std::floor(yend));
  if (steep) {
    plot(ypxl2, xpxl2, rfpart(yend) * xgap);
    plot(ypxl2 + 1, xpxl2, fpart(yend) * xgap);
  } else {
    plot(xpxl2, ypxl2, rfpart(yend) * xgap);
    plot(xpxl2, ypxl2 + 1, fpart(yend) * xgap);
  }

  if (steep) {
    for (int x = xpxl1 + 1; x < xpxl2; ++x) {
      const int iy = static_cast<int>(std::floor(intery));
      plot(iy, x, rfpart(intery));
      plot(iy + 1, x, fpart(intery));
      intery += gradient;
    }
  } else {
    for (int x = xpxl1 + 1; x < xpxl2; ++x) {
      const int iy = static_cast<int>(std::floor(intery));
      plot(x, iy, rfpart(intery));
      plot(x, iy + 1, fpart(intery));
      intery += gradient;
    }
  }
}

void Renderer::createSoftPuffTexture() {
  if (sdl_renderer == nullptr) {
    return;
  }
  if (sdl_soft_puff_texture != nullptr) {
    SDL_DestroyTexture(sdl_soft_puff_texture);
    sdl_soft_puff_texture = nullptr;
  }
  if (sdl_solid_disc_texture != nullptr) {
    SDL_DestroyTexture(sdl_solid_disc_texture);
    sdl_solid_disc_texture = nullptr;
  }

  auto build_disc_texture = [&](int size, bool soft_falloff) -> SDL_Texture * {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
      return nullptr;
    }
    SDL_LockSurface(surface);
    Uint8 *pixels = static_cast<Uint8 *>(surface->pixels);
    const int pitch = surface->pitch;
    const double center = (size - 1) * 0.5;
    const double max_radius = center;
    for (int y = 0; y < size; ++y) {
      Uint8 *row = pixels + y * pitch;
      for (int x = 0; x < size; ++x) {
        const double dx = x - center;
        const double dy = y - center;
        const double dist = std::sqrt(dx * dx + dy * dy) / max_radius;
        double alpha_norm = 0.0;
        if (soft_falloff) {
          if (dist < 1.0) {
            const double t = 1.0 - dist;
            alpha_norm = t * t * (3.0 - 2.0 * t);
          }
        } else {
          const double edge_band = 2.0 / max_radius;
          if (dist <= 1.0 - edge_band) {
            alpha_norm = 1.0;
          } else if (dist < 1.0) {
            alpha_norm = (1.0 - dist) / edge_band;
          }
        }
        const Uint8 alpha = static_cast<Uint8>(
            std::clamp(alpha_norm * 255.0, 0.0, 255.0));
        row[x * 4 + 0] = 255;
        row[x * 4 + 1] = 255;
        row[x * 4 + 2] = 255;
        row[x * 4 + 3] = alpha;
      }
    }
    SDL_UnlockSurface(surface);
    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(sdl_renderer, surface);
    SDL_FreeSurface(surface);
    if (texture != nullptr) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    }
    return texture;
  };

  constexpr int kPuffTextureSize = 64;
  sdl_soft_puff_texture = build_disc_texture(kPuffTextureSize, true);
  if (sdl_soft_puff_texture != nullptr) {
    sdl_soft_puff_texture_size = kPuffTextureSize;
  }

  constexpr int kDiscTextureSize = 256;
  sdl_solid_disc_texture = build_disc_texture(kDiscTextureSize, false);
  if (sdl_solid_disc_texture != nullptr) {
    sdl_solid_disc_texture_size = kDiscTextureSize;
  }
}

void Renderer::recreateSupersampleTarget() {
  if (sdl_supersample_target != nullptr) {
    SDL_DestroyTexture(sdl_supersample_target);
    sdl_supersample_target = nullptr;
  }
  if (sdl_renderer == nullptr || screen_res_x <= 0 || screen_res_y <= 0) {
    return;
  }
  supersample_target_w = screen_res_x * supersample_factor;
  supersample_target_h = screen_res_y * supersample_factor;
  sdl_supersample_target =
      SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_TARGET, supersample_target_w,
                        supersample_target_h);
  if (sdl_supersample_target == nullptr) {
    std::cerr << "Supersample target creation failed: " << SDL_GetError()
              << "\n";
    return;
  }
  SDL_SetTextureBlendMode(sdl_supersample_target, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(sdl_supersample_target, SDL_ScaleModeBest);
}

void Renderer::beginSupersampledFrame() {
  if (sdl_supersample_target == nullptr) {
    return;
  }
  SDL_SetRenderTarget(sdl_renderer, sdl_supersample_target);
  SDL_RenderSetScale(sdl_renderer,
                     static_cast<float>(supersample_factor),
                     static_cast<float>(supersample_factor));
}

void Renderer::endSupersampledFrameAndPresent() {
  if (sdl_supersample_target != nullptr) {
    SDL_RenderSetScale(sdl_renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(sdl_renderer, nullptr);
    SDL_RenderCopy(sdl_renderer, sdl_supersample_target, nullptr, nullptr);
  }
  SDL_RenderPresent(sdl_renderer);
}
