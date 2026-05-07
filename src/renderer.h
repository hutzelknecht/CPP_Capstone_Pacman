/*
 * PROJECT COMMENT (renderer.h)
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

#ifndef RENDERER_H
#define RENDERER_H

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "definitions.h"
#include "frametimer.h"
#include "globaltypes.h"
#include "inline_callable.h"
#include "textcache.h"

class Map;
class Game;
struct WallRubblePiece;
struct NuclearCrater;

class Renderer {
public:
  Renderer(Map *map, Game *game);
  Renderer(size_t rows, size_t cols);
  ~Renderer();
  void SetScene(Map *map, Game *game);
  void SetFloorTextureIndex(int floor_texture_index);

  // Main render path for the running game.
  void Render(bool show_pause_overlay = false,
              bool show_exit_dialog = false,
              int exit_dialog_selected = 0);
  // Menu screens for the startup flow.
  void RenderStartMenu(int selected_item, const std::string &map_name,
                       const std::string &status_message);
  void RenderConfigMenu(int selected_item, Difficulty difficulty,
                        int player_lives, int floor_texture_index);
  void RenderMapSelectionMenu(const std::vector<std::string> &map_names,
                              int selected_index);
  void RenderEditorSelectionMenu(const std::vector<std::string> &items,
                                 int selected_index);
  void RenderEditorSizeSelectionMenu(int selected_index);
  void RenderEditor(const std::vector<std::string> &layout,
                    const std::string &map_name, MapCoord cursor,
                    const std::string &warning_message,
                    bool show_exit_dialog, int exit_dialog_selected,
                    bool show_name_dialog, const std::string &name_input,
                    const std::string &name_dialog_message);
  // Countdown before the game starts.
  void RenderCountdown(int seconds_left);
  // Toggle live frame-time overlay (top-left corner).
  void SetFrameStatsOverlay(bool enabled, const FrameTimer::Stats *stats);
  // Helper for the editor: screen coordinate => map cell.
  bool TryGetLayoutCoordFromScreen(int screen_x, int screen_y,
                                   MapCoord &coord) const;

private:
  void initializeRenderer(size_t row_count, size_t col_count);
  void updateSceneLayout(size_t row_count, size_t col_count);
  void renderFrame(bool show_hud);
  void renderLayoutFrame(const std::vector<std::string> &layout);
  void drawWallTile(const SDL_Rect &wall_rect, bool has_wall_up,
                    bool has_wall_right, bool has_wall_down,
                    bool has_wall_left, Uint8 shade = 255,
                    int detail_seed = 0, bool vertical_surface = false);
  void carveRoundedWallCorner(const SDL_Rect &wall_rect, int radius,
                              bool align_left, bool align_top);
  void drawWallVegetation(const SDL_Rect &wall_rect, Uint8 shade,
                          int detail_seed, bool vertical_surface);
  void drawmap();
  void drawmap3D();
  void drawmap3DBase();
  void drawmap3DFloors();
  void drawmap3DWallTops();
  void drawFloorTile3D(int row, int col);
  void drawWallTile3D(int row, int col, bool has_wall_down);
  void drawWallTop3D(int row, int col);
  SDL_Rect getWallTopRect(int row, int col) const;
  void drawWallFrontFace3D(int row, int col);
  SDL_Rect getWallFrontFaceRect(int row, int col) const;
  void drawWithWallOcclusion(const SDL_Rect &draw_bounds, double row_cells,
                             const std::function<void()> &draw);
  void drawTexturedQuad(SDL_FPoint tl, SDL_FPoint tr, SDL_FPoint bl,
                        SDL_FPoint br, SDL_Texture *texture, Uint8 shade);
  SDL_FPoint projectScene(double col, double row, double z_cells) const;
  SDL_Rect makeFloorAlignedRect(double col, double row, double x_offset_factor,
                                double y_offset_factor, double width_factor,
                                double height_factor,
                                double delta_col_cells = 0.0,
                                double delta_row_cells = 0.0) const;
  SDL_Rect makeBillboardRect(double col, double row, double width_factor,
                             double height_factor, double foot_row_factor,
                             double delta_col_cells = 0.0,
                             double delta_row_cells = 0.0) const;
  int getNonCharacterSpriteLiftPixels() const;
  SDL_Texture *getSelectedFloorTexture() const;
  SDL_Point getSelectedFloorTextureSize() const;
  void drawFloorSpriteShadow(const SDL_Rect &sprite_rect, Uint8 alpha,
                             double width_scale = 1.0,
                             double height_scale = 1.0);
  void drawLayout(const std::vector<std::string> &layout);
  void drawteleporters();
  void drawLayoutTeleporters(const std::vector<std::string> &layout);
  void drawTeleporterGlyph(const SDL_Rect &teleporter_rect, char teleporter_digit,
                           int animation_seed);
  void drawbonusflask();
  void drawdynamitepickup();
  void drawplasticexplosivepickup();
  void drawwalkietalkiepickup();
  void drawrocketpickup();
  void drawbiohazardpickup();
  void drawnuclearbombpickup();
  void drawlovepotionpickup();
  void drawDiscoEasterEggOverlay(Uint32 now);
  void drawDiscoPickupIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                           double animation_clock);
  void drawDiscoBall(int center_x, int center_y, int radius_px,
                     double rotation_phase, bool spinning, Uint8 alpha);
  void drawDiscoLightSpots(int axis_x, int axis_y, double rotation_phase,
                           double intensity);
  void drawNuclearBombTargetMarker();
  void drawActiveNuclearBombDrop();
  void drawpacman();
  void drawmonsters();
  void drawgoodies();
  void renderGoodieSparkle(class Goodie *goodie, const SDL_Rect &sprite_rect,
                           Uint32 now);
  void drawplaceddynamites();
  void drawplacedplasticexplosive();
  void drawactiveairstrike();
  void drawrockets();
  void drawbiohazardbeam();
  void drawStaticPacman();
  void drawStaticMonsters();
  void drawStaticGoodies();
  void drawLayoutEntities(const std::vector<std::string> &layout);
  void drawgasclouds();
  void drawfireballs();
  void drawslimeballs();
  void draweffects();
  void drawExplosionParticles();
  void drawNuclearCraters();
  void drawNuclearCrater(const NuclearCrater &crater);
  void drawNuclearCraterGreenCloud(const NuclearCrater &crater, Uint32 now);
  void drawWallRubble();
  void drawWallRubblePiece(const WallRubblePiece &piece);
  void buildWallRubblePieceVertices(const WallRubblePiece &piece,
                                    SDL_FPoint (&out_vertices)[4]) const;
  SDL_Rect getWallRubblePieceBounds(const WallRubblePiece &piece) const;
  void drawMonsterFireball(int center_x, int center_y, Uint32 elapsed,
                           Uint32 seed);
  void drawNuclearFireball(int center_x, int center_y, Uint32 elapsed,
                           Uint32 seed);
  void drawNuclearBFireball(int center_x, int center_y, Uint32 elapsed,
                            Uint32 seed);
  void drawNuclearBBaseFire(int center_x, int center_y, Uint32 elapsed,
                            Uint32 seed);
  void drawNuclearBStemFire(int center_x, int center_y, Uint32 elapsed,
                            Uint32 seed, double stem_progress,
                            double total_progress);
  void drawNuclearBCapFire(int center_x, int center_y, Uint32 elapsed,
                           Uint32 seed, double stem_progress,
                           double total_progress);
  void drawNuclearBGroundDebris(int center_x, int center_y, Uint32 elapsed,
                                Uint32 seed);
  void drawNuclearBExplosion(const struct GameEffect &effect, int center_x,
                             int center_y, Uint32 elapsed);
  void drawDefeatEffect();
  void drawDefeatOverlay();
  void drawVictoryOverlay();
  void drawEndScreenOverlay(const std::string &title_text, SDL_Texture *texture,
                            const SDL_Point &texture_size,
                            const SDL_Color &glow_color);
  void drawDwarf(const SDL_Rect &dwarf_rect, Directions facing_direction,
                 double gait_phase, bool walking);
  void drawPacmanShield(int center_x, int center_y, int base_radius,
                        double pulse_clock);
  void drawStunStars(int center_x, int center_y, int orbit_radius_px,
                     int star_radius_px, Uint32 now);
  void drawPulsingHeart(int center_x, int center_y, int size_px, Uint8 alpha,
                        double pulse);
  SDL_Texture *getStunnedGoatTexture();
  void drawWalkieTalkieIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                            double animation_clock);
  void drawAirstrikePlane(const SDL_FPoint &center, double angle_degrees,
                          int max_dimension, double wobble_phase);
  void drawRocketIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                      double animation_clock);
  void drawBiohazardIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                         double animation_clock, double rotation_degrees,
                         double scale);
  void drawNuclearBombIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                           double rotation_degrees);
  void drawNuclearTargetMarkerIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                                   double scale);
  void drawLovePotionIcon(const SDL_Rect &icon_rect, Uint8 alpha,
                          double animation_clock);
  void drawRocketFlight(const SDL_FPoint &center, Directions direction,
                        int max_dimension, int frame_index, Uint8 alpha);
  void drawDynamiteIcon(const SDL_Rect &icon_rect, bool lit_fuse,
                        double animation_clock, Uint8 alpha,
                        double fuse_burn_progress);
  void drawOrganicGasCloud(int center_x, int center_y, int base_size,
                           double animation_clock, double alpha_factor,
                           int animation_seed, double scale_multiplier);
  void drawPacmanGasCloudlets(const SDL_Rect &anchor_rect,
                              double alpha_factor, Uint32 now);
  void drawPlasticExplosiveIcon(const SDL_Rect &icon_rect, double animation_clock,
                                Uint8 alpha, bool armed);
  void drawElectrifiedMonsterAura(const SDL_Rect &monster_rect,
                                  int animation_seed, Uint32 now,
                                  bool aggressive_mode);
  void drawhud();
  void drawDimmer(Uint8 alpha);
  void drawPanel(const SDL_Rect &panel, const SDL_Color &fill_color,
                 const SDL_Color &border_color);
  void drawStartMenuSpectrum(const SDL_Rect &panel);
  void drawStartMenuSpectrumGlassBlocks(const SDL_Rect &panel);
  void drawGlassBlock(const SDL_Rect &block_rect, const SDL_Color &color,
                      float lit_amount);
  void ensureGlassBlockTextures(int block_w, int block_h);
  void drawStartMenuOverlay(int selected_item,
                            const std::string &map_name,
                            const std::string &status_message);
  void drawConfigMenuOverlay(int selected_item, Difficulty difficulty,
                             int player_lives, int floor_texture_index);
  void drawMapSelectionOverlay(const std::vector<std::string> &map_names,
                               int selected_index);
  void drawEditorSelectionOverlay(const std::vector<std::string> &items,
                                  int selected_index);
  void drawEditorSizeSelectionOverlay(int selected_index);
  void drawEditorOverlay(const std::string &map_name, MapCoord cursor,
                         const std::string &warning_message,
                         bool show_exit_dialog, int exit_dialog_selected,
                         bool show_name_dialog, const std::string &name_input,
                         const std::string &name_dialog_message);
  void drawCountdownOverlay(int seconds_left);
  void drawGameplayPauseOverlay(bool show_exit_dialog,
                                int exit_dialog_selected);
  void drawFrameStatsOverlay();
  void rebuildWallOcclusionLookupIfNeeded();
  void renderSimpleText(TTF_Font *font, const std::string &text,
                        const SDL_Color &color, int center_x, int top_y);
  void renderTextLeft(TTF_Font *font, const std::string &text,
                      const SDL_Color &color, int left_x, int top_y);
  void renderStartLogo(TTF_Font *font, const std::string &text, int center_x,
                       int top_y);
  void renderBrickText(TTF_Font *font, const std::string &text, int center_x,
                       int top_y, const SDL_Color &outline_color);
  SDL_Texture *loadTrimmedTexture(const std::string &path,
                                  SDL_Point &trimmed_size);
  SDL_Texture *loadTexturePreserveCanvas(const std::string &path,
                                         SDL_Point &texture_size);
  SDL_Texture *loadTrimmedChromaKeyTexture(const std::string &path,
                                           SDL_Point &trimmed_size,
                                           Uint8 tolerance);
  void renderDecorativeTexture(SDL_Texture *texture,
                               const SDL_Rect &destination,
                               const SDL_Color &glow_color,
                               double sway_phase,
                               double sway_strength_x,
                               double sway_strength_y,
                               double sway_speed);
  SDL_Surface *createStartLogoSurface(TTF_Font *font, const std::string &text);
  SDL_Texture *getPacmanTexture(Directions facing_direction,
                                bool walking) const;
  int getPacmanAnimationFrame(bool walking) const;
  SDL_Texture *getMonsterTexture(char monster_char,
                                 Directions facing_direction,
                                 int animation_seed,
                                 bool grazing = false,
                                 bool jumping = false);
  int getMonsterAnimationFrame(char monster_char, int animation_seed,
                               bool grazing = false,
                               bool jumping = false) const;
  SDL_Surface *createBrickTextSurface(TTF_Font *font, const std::string &text,
                                      const SDL_Color &outline_color);
  Uint32 readPixel(SDL_Surface *surface, int x, int y);
  void writePixel(SDL_Surface *surface, int x, int y, Uint32 pixel);

  int SDL_RenderDrawCircle(SDL_Renderer *, int, int, int);
  int SDL_RenderFillCircle(SDL_Renderer *, int, int, int);
  int SDL_RenderFillEllipse(SDL_Renderer *, int, int, int, int);
  void SDL_RenderFillCircleAA(SDL_Renderer *renderer, double x, double y,
                              double radius, SDL_Color color);
  void SDL_RenderFillEllipseAA(SDL_Renderer *renderer, double x, double y,
                               double radius_x, double radius_y,
                               SDL_Color color);
  void SDL_RenderDrawAALine(SDL_Renderer *renderer, double x0, double y0,
                            double x1, double y1, SDL_Color color);
  PixelCoord getPixelCoord(MapCoord, int, int);

  void beginSupersampledFrame();
  void endSupersampledFrameAndPresent();
  void recreateSupersampleTarget();
  void createSoftPuffTexture();

  SDL_Texture *sdl_supersample_target = nullptr;
  int supersample_factor = 1;
  int supersample_target_w = 0;
  int supersample_target_h = 0;

  int screen_res_x;
  int screen_res_y;
  int element_size;
  int hud_top_y;
  int offset_x;
  int offset_y;
  int scene_origin_y;
  int wall_height_px;
  double cos_tilt;
  double sin_tilt;
  size_t rows;
  size_t cols;
  Map *map;
  Game *game;
  SDL_Window *sdl_window;
  SDL_Renderer *sdl_renderer;

  SDL_Surface *sdl_wall_surface;
  SDL_Texture *sdl_wall_texture;
  SDL_Rect sdl_wall_rect;
  std::vector<SDL_Texture *> sdl_floor_textures;
  std::vector<SDL_Point> sdl_floor_texture_sizes;
  int selected_floor_texture_index;

  SDL_Surface *sdl_goodie_surface;
  SDL_Texture *sdl_goodie_texture;
  SDL_Rect sdl_goodie_rect;

  std::vector<SDL_Surface *> sdl_monster_surfaces;
  std::vector<SDL_Texture *> sdl_monster_textures;
  std::vector<SDL_Surface *> sdl_goat_grazing_surfaces;
  std::vector<SDL_Texture *> sdl_goat_grazing_textures;
  std::vector<SDL_Surface *> sdl_goat_jumping_surfaces;
  std::vector<SDL_Texture *> sdl_goat_jumping_textures;
  SDL_Rect sdl_monster_rect;

  std::vector<SDL_Surface *> sdl_pacman_surfaces;
  std::vector<SDL_Texture *> sdl_pacman_textures;
  SDL_Rect sdl_pacman_rect;

  SDL_Surface *sdl_logo_brick_surface;
  SDL_Texture *sdl_dynamite_texture;
  SDL_Point sdl_dynamite_size;
  SDL_Texture *sdl_nuclear_crater_texture;
  SDL_Point sdl_nuclear_crater_size;
  SDL_Texture *sdl_nuclear_bomb_texture;
  SDL_Point sdl_nuclear_bomb_size;
  SDL_Texture *sdl_nuclear_target_marker_texture;
  SDL_Point sdl_nuclear_target_marker_size;
  SDL_Texture *sdl_start_menu_monster_texture;
  SDL_Point sdl_start_menu_monster_size;
  SDL_Texture *sdl_start_menu_hero_texture;
  SDL_Point sdl_start_menu_hero_size;
  SDL_Texture *sdl_win_screen_texture;
  SDL_Point sdl_win_screen_size;
  SDL_Texture *sdl_lose_screen_texture;
  SDL_Point sdl_lose_screen_size;
  SDL_Texture *sdl_walkie_talkie_texture;
  SDL_Point sdl_walkie_talkie_size;
  SDL_Texture *sdl_rocket_texture;
  SDL_Point sdl_rocket_size;
  std::vector<SDL_Texture *> sdl_rocket_flight_textures;
  std::vector<SDL_Point> sdl_rocket_flight_sizes;
  SDL_Texture *sdl_airstrike_plane_texture;
  SDL_Point sdl_airstrike_plane_size;
  std::array<SDL_Texture *, HUD_KEYCAP_ASSET_COUNT> sdl_hud_keycap_textures{};
  std::array<SDL_Point, HUD_KEYCAP_ASSET_COUNT> sdl_hud_keycap_sizes{};
  SDL_Texture *sdl_airstrike_explosion_texture;
  SDL_Point sdl_airstrike_explosion_size;
  SDL_Texture *sdl_monster_explosion_texture;
  SDL_Point sdl_monster_explosion_size;
  SDL_Texture *sdl_fart_cloud_texture;
  SDL_Point sdl_fart_cloud_size;
  SDL_Texture *sdl_slime_ball_texture;
  SDL_Point sdl_slime_ball_size;
  SDL_Texture *sdl_slime_overlay_texture;
  SDL_Point sdl_slime_overlay_size;
  SDL_Texture *sdl_slime_splash_texture;
  SDL_Point sdl_slime_splash_size;
  SDL_Texture *sdl_soft_puff_texture = nullptr;
  int sdl_soft_puff_texture_size = 0;
  SDL_Texture *sdl_solid_disc_texture = nullptr;
  int sdl_solid_disc_texture_size = 0;
  SDL_Texture *sdl_glass_block_base_texture = nullptr;
  SDL_Texture *sdl_glass_block_glow_texture = nullptr;
  int sdl_glass_block_texture_w = 0;
  int sdl_glass_block_texture_h = 0;

  TTF_Font *sdl_font_hud;
  TTF_Font *sdl_font_menu;
  TTF_Font *sdl_font_logo;
  TTF_Font *sdl_font_display;

  SDL_Color sdl_font_color;
  SDL_Color sdl_font_back_color;

  int texW;
  int texH;

  bool frame_stats_overlay_enabled_ = false;
  const FrameTimer::Stats *frame_stats_ = nullptr;
  TextCache text_cache_;

  struct DepthDrawCommand {
    float depth = 0.0f;
    size_t order = 0;
    InlineCallable<192> draw;
  };
  std::vector<DepthDrawCommand> depth_commands_;

  std::vector<int> wall_top_y_lookup_;
  bool wall_top_y_dirty_ = true;
};

#endif
