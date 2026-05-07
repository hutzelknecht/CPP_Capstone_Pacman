/*
 * PROJECT COMMENT (definitions.h)
 * ---------------------------------------------------------------------------
 * This header is the single source of truth for BobMan's compile-time
 * configuration. It groups feature flags, asset paths, gameplay timing,
 * editor defaults, rendering colors, HUD tuning, and audio analysis values.
 *
 * Design rules for this file:
 * - Preprocessor feature toggles stay macros because #ifdef checks need them.
 * - Regular parameters use typed inline constexpr constants.
 * - Asset paths are stored as data-relative paths and are resolved at runtime
 *   through Paths::GetDataFilePath().
 * - Every value documents its unit, valid range, and the gameplay or visual
 *   consequence of changing it.
 */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <SDL.h>

#include <array>

/**
 * @brief Compile-time feature toggles.
 *
 * Comment out these macros only when the target environment lacks the matching
 * SDL subsystem or font support. They remain macros on purpose because the
 * codebase uses them with #ifdef / #ifndef guards.
 */
#define AUDIO
#define FONT_FINE

/**
 * @brief Tuple-style draw colors intentionally kept as macros.
 *
 * These values are passed directly into SDL_SetRenderDrawColor() at a few
 * call sites that expect a raw channel list instead of an SDL_Color object.
 * Valid values are 0..255 per channel.
 */
#define COLOR_BACK 10, 0, 35
#define COLOR_PACMAN 200, 200, 0
#define COLOR_MONSTER 255, 10, 10
#define COLOR_GOODIE 0, 50, 255
#define COLOR_PATH 40, 40, 40

/**
 * @brief Asset and window configuration.
 *
 * All paths are relative to the bundled `data/` directory. They are resolved
 * at runtime to support both source-tree execution and packaged app bundles.
 */
inline constexpr const char *WINDOW_TITLE = "BobMan";
inline constexpr const char *MAPS_SUBDIRECTORY = "maps";
inline constexpr const char *FONT_ASSET_PATH = "font.ttf";
inline constexpr const char *APP_ICON_RGBA_ASSET_PATH = "app_icon_rgba.png";
inline constexpr const char *APP_ICON_FALLBACK_ASSET_PATH = "app_icon.png";
inline constexpr const char *WALL_TEXTURE_ASSET_PATH = "brick.bmp";
inline constexpr int FLOOR_TEXTURE_OPTION_COUNT = 5;
inline constexpr int FLOOR_TEXTURE_DEFAULT_INDEX = 0;
inline constexpr std::array<const char *, FLOOR_TEXTURE_OPTION_COUNT>
    FLOOR_TEXTURE_ASSET_PATHS{
        "floor_textures/floor_texture_1.jpg",
        "floor_textures/floor_texture_2.jpg",
        "floor_textures/floor_texture_3.jpg",
        "floor_textures/floor_texture_4.jpg",
        "floor_textures/floor_texture_5.jpg",
    };
inline constexpr const char *GOODIE_TEXTURE_ASSET_PATH = "goodie.bmp";
inline constexpr const char *LOGO_BRICK_TEXTURE_PATH = "brick.png";
inline constexpr const char *START_MENU_MONSTER_ASSET_PATH =
    "start_menu_monster.png";
inline constexpr const char *START_MENU_HERO_ASSET_PATH =
    "start_menu_spielfigur.png";
inline constexpr const char *WIN_SCREEN_ASSET_PATH = "win_screen_triumph.png";
inline constexpr const char *LOSE_SCREEN_ASSET_PATH = "lose_screen_defeat.png";
inline constexpr const char *WALKIE_TALKIE_ASSET_PATH = "walkie_talkie.png";
inline constexpr const char *ROCKET_ASSET_PATH = "rocket.png";
inline constexpr const char *AIRSTRIKE_PLANE_ASSET_PATH =
    "airstrike_plane.png";
inline constexpr int HUD_KEYCAP_ASSET_COUNT = 10;
inline constexpr std::array<const char *, HUD_KEYCAP_ASSET_COUNT>
    HUD_KEYCAP_ASSET_PATHS{
        "hud_keycaps/key_0.png",
        "hud_keycaps/key_1.png",
        "hud_keycaps/key_2.png",
        "hud_keycaps/key_3.png",
        "hud_keycaps/key_4.png",
        "hud_keycaps/key_5.png",
        "hud_keycaps/key_6.png",
        "hud_keycaps/key_7.png",
        "hud_keycaps/key_8.png",
        "hud_keycaps/key_9.png",
    };
inline constexpr const char *AIRSTRIKE_EXPLOSION_SHEET_ASSET_PATH =
    "airstrike_explosion_sheet.png";
inline constexpr const char *MONSTER_EXPLOSION_SHEET_ASSET_PATH =
    "monster_explosion_sheet.png";
inline constexpr const char *FART_CLOUD_SHEET_ASSET_PATH =
    "fart_cloud_sheet.png";
inline constexpr const char *SLIME_BALL_ASSET_PATH = "slime_ball.png";
inline constexpr const char *SLIME_OVERLAY_ASSET_PATH = "slime_overlay.png";
inline constexpr const char *SLIME_SPLASH_SHEET_ASSET_PATH =
    "slime_splash_sheet.png";
inline constexpr const char *DYNAMITE_ASSET_PATH = "dynamite.png";
inline constexpr const char *NUCLEAR_BOMB_ASSET_PATH = "nuclear_bomb.png";
inline constexpr const char *NUCLEAR_TARGET_MARKER_ASSET_PATH =
    "nuclear_target_marker.png";
inline constexpr const char *NUCLEAR_CRATER_ASSET_PATH = "nuclear_crater.png";
inline constexpr const char *MENU_MUSIC_PATH = "menu_music.mp3";
inline constexpr const char *DISCO_MUSIC_PATH = "disco_easteregg.mp3";
inline constexpr const char *WIN_MUSIC_PATH = "win_melody.mp3";
inline constexpr const char *LOSE_MUSIC_PATH = "lose_melody.mp3";
inline constexpr const char *COIN_SOUND_PATH = "coin.wav";
inline constexpr const char *WIN_SOUND_PATH = "win.wav";
inline constexpr const char *MONSTER_SHOT_SOUND_PATH = "schuss.wav";
inline constexpr const char *FIREBALL_IMPACT_SOUND_PATH = "einschlag.wav";
inline constexpr const char *SLIME_SHOT_SOUND_PATH = "slime_shot.mp3";
inline constexpr const char *SLIME_IMPACT_SOUND_PATH = "slime_impact.mp3";
inline constexpr const char *MONSTER_EXPLOSION_SOUND_PATH =
    "monsterexplosion.wav";
inline constexpr const char *MONSTER_FART_SOUND_PATH = "monster_fart.mp3";
inline constexpr const char *DYNAMITE_EXPLOSION_SOUND_PATH =
    "dynamitexplosion.wav";
inline constexpr const char *DYNAMITE_IGNITE_SOUND_PATH =
    "dynamite_ignite.mp3";
inline constexpr const char *PLASTIC_EXPLOSIVE_WALL_BREAK_SOUND_PATH =
    "plastic_explosive_wall_break.mp3";
inline constexpr const char *AIRSTRIKE_RADIO_SOUND_PATH =
    "airstrike_radio.wav";
inline constexpr const char *AIRSTRIKE_EXPLOSION_SOUND_PATH =
    "airstrike_explosion.wav";
inline constexpr const char *AIRSTRIKE_PROPELLER_SOUND_PATH =
    "airstrike_propeller.mp3";
inline constexpr const char *ROCKET_LAUNCH_SOUND_PATH = "rocket_launch.mp3";
inline constexpr const char *BIOHAZARD_BEAM_SOUND_PATH = "biohazard_beam.wav";
inline constexpr const char *ELECTRIFIED_MONSTER_ROAR_SOUND_PATH =
    "electrified_monster_roar.mp3";
inline constexpr const char *ELECTRIFIED_MONSTER_IMPACT_SOUND_PATH =
    "electrified_monster_impact.mp3";
inline constexpr const char *NUCLEAR_BOMB_ALARM_SOUND_PATH =
    "nuclear_bomb_alarm.mp3";
inline constexpr const char *NUCLEAR_BOMB_DROP_SOUND_PATH =
    "nuclear_bomb_drop.mp3";
inline constexpr const char *NUCLEAR_BOMB_EXPLOSION_SOUND_PATH =
    "nuclear_bomb_explosion.mp3";
inline constexpr const char *SETTINGS_FILE_NAME = "settings.cfg";

/**
 * @brief Map editor defaults and UI timing.
 *
 * The row and column counts define the exact map canvas size for newly created
 * maps. The end-screen minimum keeps win/loss overlays visible long enough to
 * read before the user can dismiss them.
 */
inline constexpr int EDITOR_SMALL_MAP_ROWS = 13;
inline constexpr int EDITOR_SMALL_MAP_COLS = 22;
inline constexpr int EDITOR_MEDIUM_MAP_ROWS = 17;
inline constexpr int EDITOR_MEDIUM_MAP_COLS = 30;
inline constexpr int EDITOR_LARGE_MAP_ROWS = 21;
inline constexpr int EDITOR_LARGE_MAP_COLS = 38;
inline constexpr int END_SCREEN_MINIMUM_MS = 3000;

/**
 * @brief Startup menu timing and spectrum-surface layout.
 *
 * `GAME_START_COUNTDOWN` is clamped to 3..9 seconds in code so the countdown
 * always fits on screen and remains readable. The spectrum values tune the
 * translucent side surfaces in the start menu; the margin caps how far the
 * FFT curve may expand towards the screen edge and the fill alpha controls the
 * overall intensity.
 *
 * `START_MENU_SPECTRUM_GLASS_BLOCKS` selects an alternative visualization that
 * replaces the spline-curve surface with a horizontal glass-block equalizer.
 * Comment out the macro to fall back to the original spline rendering. The
 * remaining `START_MENU_SPECTRUM_GLASS_*` constants are only consumed when the
 * macro is active and tune the bar/block layout, spacing, corner rounding and
 * the rim/edge bevel that gives each block its 3D look. All distances are in
 * pixels at the reference resolution; the renderer scales them down on small
 * screens so the bars never spill into the menu area.
 */
inline constexpr int GAME_START_COUNTDOWN = 3;
inline constexpr int MENU_MUSIC_FADE_OUT_MS = 2000;
inline constexpr int START_MENU_SPECTRUM_OUTER_MARGIN = 10;
inline constexpr Uint8 START_MENU_SPECTRUM_FILL_ALPHA = 118;

#define START_MENU_SPECTRUM_GLASS_BLOCKS

inline constexpr int START_MENU_SPECTRUM_GLASS_BAR_COUNT = 10;
inline constexpr int START_MENU_SPECTRUM_GLASS_BLOCKS_PER_BAR = 15;
inline constexpr int START_MENU_SPECTRUM_GLASS_BLOCK_GAP_PX = 3;
inline constexpr int START_MENU_SPECTRUM_GLASS_BAR_GAP_PX = 5;
inline constexpr int START_MENU_SPECTRUM_GLASS_PANEL_MARGIN_PX = 22;
inline constexpr int START_MENU_SPECTRUM_GLASS_SCREEN_MARGIN_PX = 16;
inline constexpr int START_MENU_SPECTRUM_GLASS_CORNER_RADIUS_PX = 3;
inline constexpr int START_MENU_SPECTRUM_GLASS_EDGE_BEVEL_PX = 2;
// Skaliert die plastische 3D-Anmutung der Glasbausteine (Diffuse-Highlight,
// Specular-Funkeln, Rand-Refraktion). 0.0f = flach milchig ohne Beleuchtung,
// 1.0f = ausgewogen rundes Quader-Look, 2.0f = überzeichnet plastisch.
inline constexpr float START_MENU_SPECTRUM_GLASS_3D_STRENGTH = 1.0f;

/**
 * @brief Animiertes Liquid-Glass-Logo "BobMan" im Startmenü.
 *
 * Die Schrift wird als echtes 3D-Volumen aus zähflüssigem dunkelblauen Glas
 * gerendert: TTF-Glyph -> per-Pixel-Shader (Glass-Body, Refraktionsrand,
 * Multi-Light-Reflexe) -> Mesh-Warp für die flüssige Bewegung -> Stack aus
 * `DEPTH_SLICES` versetzten Schichten für die Tiefenwirkung. Alle Werte mit
 * "_FACTOR" werden zur Laufzeit mit der Glyph- oder Schrifthöhe multipliziert,
 * damit der Look auf jeder Auflösung konsistent bleibt.
 */

// Transparenz des Buchstabenkörpers. 0.0f = komplett durchsichtig, 1.0f =
// blickdicht. Niedriger = der Menühintergrund schimmert stärker durch und das
// Glas wirkt dünner/transluzenter.
inline constexpr float START_MENU_LOGO_BODY_ALPHA = 0.22f;

// Transparenz des Außenrandes. Zu hoch -> harte Outline (kein Glas-Look),
// zu niedrig -> Silhouette wird unleserlich.
inline constexpr float START_MENU_LOGO_RIM_ALPHA = 0.92f;

// Breite der gläsernen Randzone als Anteil der Glyph-Höhe. Größer = breiter
// Refraktionsrand, dickeres Glas; kleiner = scharfer Schnitt.
inline constexpr float START_MENU_LOGO_RIM_SOFTNESS_FACTOR = 0.025f;

// Innere Refraktion: wie stark die geometrische Mitte jedes Buchstabens zur
// dunkelsten Navy-Farbe gezogen wird. Höher = mehr Volumen / dunklere Mitte.
inline constexpr float START_MENU_LOGO_INNER_DEPTH_BASE = 0.30f;
inline constexpr float START_MENU_LOGO_INNER_DEPTH_RAMP = 0.22f;

// Stärke des cyan-blauen Refraktionsstreifens, der umlaufend am Rand sitzt.
// Höher = stärkerer "lichtbrechender" Glaseindruck am Rand.
inline constexpr float START_MENU_LOGO_REFRACTION_STRENGTH = 0.30f;

// Globaler Multiplikator für alle eingebackenen Lichtreflexe. 0.0f schaltet
// die Reflexe komplett aus (reines Glas ohne Highlights), 1.0f = Standard,
// >1.0f = überstrahlte Highlights.
inline constexpr float START_MENU_LOGO_LIGHT_INTENSITY = 0.5f;

// Begrenzt die Reflexe auf die "flache Oberseite" des Glaskörpers. Höher =
// Highlights bleiben sauber innen; niedriger = sie laufen über den Rand.
inline constexpr float START_MENU_LOGO_LIGHT_GATE_EXPONENT = 3.3f;

// Sigma der Gauss-Glättung der Glyph-Alpha-Maske (in Pixeln) - rundet scharfe
// Schriftecken ab, bevor Distanztransformation und Alpha berechnet werden.
// Höher = rundere Ecken aber dickere Schrift; 0.0f = Originalkanten.
inline constexpr float START_MENU_LOGO_CORNER_BLUR_SIGMA = 0.3f;

// Glaskörper-Farbverlauf vom Top zur Tiefe. Diese drei Stops definieren die
// Grundfarbe des "dunkelblauen Glases".
inline constexpr SDL_Color START_MENU_LOGO_BODY_TOP_COLOR{130, 164, 238, 255};
inline constexpr SDL_Color START_MENU_LOGO_BODY_MID_COLOR{62, 78, 128, 255};
inline constexpr SDL_Color START_MENU_LOGO_BODY_DEEP_COLOR{54, 60, 92, 255};

// Rand-Farben: Highlight oben (Lichteinfall), Refraktionsstreifen (umlaufend)
// und Outline-Schatten unten (für Silhouette gegen helle Hintergründe).
inline constexpr SDL_Color START_MENU_LOGO_RIM_HIGHLIGHT_COLOR{220, 240, 255,
                                                               255};
inline constexpr SDL_Color START_MENU_LOGO_RIM_REFRACTION_COLOR{90, 168, 240,
                                                                255};
inline constexpr SDL_Color START_MENU_LOGO_RIM_SHADOW_COLOR{2, 6, 26, 255};

// 3D-Extrusion: Anzahl der gestapelten Tiefenschichten. Höher = glattere
// Seitenwände, mehr Render-Aufwand. 6 wirkt low-poly, 30 ultraglatt.
inline constexpr int START_MENU_LOGO_DEPTH_SLICES = 28;

// Richtung, in die die Extrusion vom Vorderfläche aus läuft (Bildschirm-
// koordinaten). (0.55, 0.85) = leicht nach unten/rechts. Vorzeichen ändern
// dreht die Blickrichtung; Länge des Vektors zusammen mit DEPTH_FACTOR
// bestimmt die effektive Tiefe.
inline constexpr float START_MENU_LOGO_DEPTH_DIR_X = 0.05f;
inline constexpr float START_MENU_LOGO_DEPTH_DIR_Y = 0.55f;

// Gesamttiefe der Extrusion als Anteil der Schrifthöhe. 0.0f = flaches 2D-
// Logo, 0.16f = sichtbarer 3D-Quader, 0.30f = klobig.
inline constexpr float START_MENU_LOGO_DEPTH_FACTOR = 0.16f;

// Tiefen-Parallaxe: wie viel weniger die hinteren Schichten beim Wabbeln
// mitschwingen. 0.0f = Stack bewegt sich starr, 1.0f = nur Vorderfläche
// wabbelt während die Rückseite eingefroren wirkt.
inline constexpr float START_MENU_LOGO_DEPTH_PARALLAX = 0.70f;

// Tönung der hintersten Extrusionsschicht. Definiert den Farbeindruck der
// Seitenwände / des inneren Glasvolumens. Dunkler = tieferer Volumeneindruck.
inline constexpr SDL_Color START_MENU_LOGO_BACK_TINT{16, 28, 70, 255};

// Liquid-Warp Amplituden als Anteil von Logo-Breite/-Höhe. Größer = breitere
// Wellen; sichtbar verzerrtes Glas.
inline constexpr float START_MENU_LOGO_WARP_AMP_X_FACTOR = 0.0175f;
inline constexpr float START_MENU_LOGO_WARP_AMP_Y_FACTOR = 0.038f;

// Geschwindigkeitsfaktor der Liquid-Welle. 1.0f = zähflüssiger Standard,
// <1.0f = noch dickflüssiger, >1.0f = quirligere/lockere Bewegung.
inline constexpr double START_MENU_LOGO_WARP_SPEED = 1.0;

// Drop-Shadow unter dem Glas. Offset = Schrifthöhe * SHADOW_OFFSET_FACTOR
// (mindestens 5 px); Alpha = Sichtbarkeit; Farbe = Schattenton.
inline constexpr float START_MENU_LOGO_SHADOW_OFFSET_FACTOR = 0.05f;
inline constexpr Uint8 START_MENU_LOGO_SHADOW_ALPHA = 168;
inline constexpr SDL_Color START_MENU_LOGO_SHADOW_COLOR{6, 18, 86, 255};

// Aufblitzende Glanzlicht-Sterne ("Sparkles") auf der Glasoberfläche.
// SPARKLE_TINT_STRENGTH blendet die drei Sparkle-Schichten (weicher Halo,
// hellblaue Spitze, weißer Kern) linear in Richtung SPARKLE_TINT_COLOR.
// 0.0f = neutral cyan/weiß (klassischer Funkel), 1.0f = vollständig auf den
// Logo-Farbton gefärbt (Sparkle wirkt wie Teil des Glases). SPARKLE_TINT_COLOR
// gibt diesen Farbton vor und sollte am ehesten zu den Glas-Reflexen passen.
inline constexpr SDL_Color START_MENU_LOGO_SPARKLE_TINT_COLOR{140, 188, 240,
                                                              255};
inline constexpr float START_MENU_LOGO_SPARKLE_TINT_STRENGTH = 0.6f;

/**
 * @brief Player life configuration.
 *
 * `PLAYER_DEFAULT_LIVES` is used when no setting has been stored yet.
 * The config menu clamps the runtime value into the inclusive range defined by
 * `PLAYER_LIVES_MIN` and `PLAYER_LIVES_MAX`.
 */
inline constexpr int PLAYER_LIVES_MIN = 1;
inline constexpr int PLAYER_LIVES_MAX = 5;
inline constexpr int PLAYER_DEFAULT_LIVES = 3;

/**
 * @brief Movement speed ratings for Pacman and monsters.
 *
 * These ratings are intentionally kept on a simple 1..10 scale so gameplay can
 * be tuned without touching the movement loops. Higher values mean less delay
 * between micro-steps and therefore faster movement.
 */
inline constexpr int SPEED_PACMAN = 8;
inline constexpr int SPEED_MONSTER = 6;
inline constexpr int SPEED_GOAT = 3;

/**
 * @brief Hitbox radii for visually accurate collision detection.
 *
 * All values are expressed in cell units (1.0 == one cell width). Sprites and
 * projectiles collide when their hitbox circles overlap, so the gameplay
 * matches what the eye sees instead of waiting for cell-coordinate centers
 * to align. Tweaking individual values up or down makes specific entities
 * easier or harder to hit without affecting their visuals.
 */
inline constexpr float PACMAN_HITBOX_RADIUS_CELLS = 0.40f;
inline constexpr float MONSTER_HITBOX_RADIUS_CELLS = 0.40f;
inline constexpr float FIREBALL_HITBOX_RADIUS_CELLS = 0.22f;
inline constexpr float SLIMEBALL_HITBOX_RADIUS_CELLS = 0.24f;
inline constexpr float ROCKET_HITBOX_RADIUS_CELLS = 0.28f;
inline constexpr float BEAM_HITBOX_RADIUS_CELLS = 0.18f;

/**
 * @brief Score and loss-recovery tuning.
 *
 * `SCORE_PER_GOODIE` controls how rewarding each pellet is. The extra-life
 * invulnerability duration is used after consuming one remaining life instead
 * of ending the run immediately. Flicker settings control the visual feedback:
 * the sprite alternates between full opacity and `PACMAN_RECOVERY_ALPHA`.
 * `FINAL_LOSS_SOUND_LEAD_IN_MS` freezes the gameplay scene for two seconds so
 * the final defeat cue can land before the lose screen takes over.
 */
inline constexpr int SCORE_PER_GOODIE = 10;
inline constexpr Uint32 PACMAN_EXTRA_LIFE_INVULNERABLE_MS = 5000;
inline constexpr Uint8 PACMAN_RECOVERY_ALPHA = 128;
inline constexpr Uint32 PACMAN_RECOVERY_FLICKER_PHASE_MS = 120;
inline constexpr Uint32 FINAL_LOSS_SOUND_LEAD_IN_MS = 2000;

/**
 * @brief Pickup, projectile, status, and effect timing.
 *
 * All values are measured in milliseconds unless noted otherwise. Raising
 * visible durations makes pickups easier to collect; lowering cooldowns makes
 * enemies more aggressive; increasing radii or durations makes explosives and
 * status effects more punishing.
 */
inline constexpr Uint32 MONSTER_ANIMATION_FRAME_MS = 180;
inline constexpr Uint32 GOAT_ANIMATION_FRAME_MS = 150;
inline constexpr Uint32 GOAT_GRAZING_ANIMATION_FRAME_MS = 220;
inline constexpr Uint32 GOAT_JUMP_ANIMATION_FRAME_MS = 90;
inline constexpr int GOAT_JUMP_STEP_DELAY_MS = 1;
inline constexpr Uint32 GOAT_GRAZING_MIN_INTERVAL_MS = 8000;
inline constexpr Uint32 GOAT_GRAZING_MAX_INTERVAL_MS = 18000;
inline constexpr Uint32 GOAT_GRAZING_MIN_DURATION_MS = 5500;
inline constexpr Uint32 GOAT_GRAZING_MAX_DURATION_MS = 15000;
// Goat charge / slide / collision behavior
inline constexpr int GOAT_SLIDE_TILES = 4;
inline constexpr Uint32 GOAT_RECOVERY_PAUSE_MS = 700;
inline constexpr Uint32 GOAT_STUN_DURATION_MS = 5000;
inline constexpr Uint32 PACMAN_STUN_DURATION_MS = 5000;
inline constexpr Uint32 GOAT_PLAYER_GRACE_MS = 20000;
inline constexpr float GOAT_RED_DUST_CLOUD_RADIUS_CELLS = 3.6f;
inline constexpr int GOAT_RED_DUST_PUFF_COUNT = 34;
inline constexpr int STUN_STARS_COUNT = 5;
inline constexpr float STUN_STARS_ORBIT_RADIUS_CELLS = 0.65f;
inline constexpr float STUN_STARS_RADIUS_CELLS = 0.13f;
inline constexpr float STUN_STARS_ORBIT_PERIOD_MS = 2600.0f;
inline constexpr Uint32 FIREBALL_STEP_DURATION_MS = 160;
inline constexpr Uint32 MONSTER_GAS_MIN_INTERVAL_MS = 40000;
inline constexpr Uint32 MONSTER_GAS_MAX_INTERVAL_MS = 60000;
// Total green-cloud lifetime (visible + fade). GAS_CLOUD_ACTIVE_MS is
// derived so the whole cloud is gone by GAS_CLOUD_TOTAL_LIFETIME_MS.
inline constexpr Uint32 GAS_CLOUD_TOTAL_LIFETIME_MS = 20000;
inline constexpr Uint32 GAS_CLOUD_FADE_MS = 4000;
inline constexpr Uint32 GAS_CLOUD_ACTIVE_MS =
    GAS_CLOUD_TOTAL_LIFETIME_MS - GAS_CLOUD_FADE_MS;
inline constexpr int GAS_CLOUD_MIN_PARTICLE_COUNT = 6;
inline constexpr int GAS_CLOUD_MAX_PARTICLE_COUNT = 10;
// Visual tuning of the blue monsters' green fart cloud.
inline constexpr double GAS_CLOUD_OPACITY_SCALE = 1.05;
inline constexpr double GAS_CLOUD_SPREAD_SCALE = 1.30;
inline constexpr Uint32 BLUE_POTION_SPAWN_MIN_INTERVAL_MS = 20000;
inline constexpr Uint32 BLUE_POTION_SPAWN_MAX_INTERVAL_MS = 60000;
inline constexpr Uint32 BLUE_POTION_VISIBLE_MS = 15000;
inline constexpr Uint32 BLUE_POTION_FADE_MS = 1000;
inline constexpr Uint32 DYNAMITE_SPAWN_MIN_INTERVAL_MS = 5000;
inline constexpr Uint32 DYNAMITE_SPAWN_MAX_INTERVAL_MS = 10000;
inline constexpr Uint32 DYNAMITE_VISIBLE_MS = 60000;
inline constexpr Uint32 DYNAMITE_FADE_MS = 1000;
inline constexpr Uint32 PLASTIC_EXPLOSIVE_SPAWN_MIN_INTERVAL_MS = 5000;
inline constexpr Uint32 PLASTIC_EXPLOSIVE_SPAWN_MAX_INTERVAL_MS = 10000;
inline constexpr Uint32 PLASTIC_EXPLOSIVE_VISIBLE_MS = 60000;
inline constexpr Uint32 PLASTIC_EXPLOSIVE_FADE_MS = 1000;
inline constexpr Uint32 WALKIE_TALKIE_SPAWN_MIN_INTERVAL_MS = 12000;
inline constexpr Uint32 WALKIE_TALKIE_SPAWN_MAX_INTERVAL_MS = 22000;
inline constexpr Uint32 WALKIE_TALKIE_VISIBLE_MS = 60000;
inline constexpr Uint32 WALKIE_TALKIE_FADE_MS = 1000;
inline constexpr Uint32 ROCKET_SPAWN_MIN_INTERVAL_MS = 8000;
inline constexpr Uint32 ROCKET_SPAWN_MAX_INTERVAL_MS = 16000;
inline constexpr Uint32 ROCKET_VISIBLE_MS = 60000;
inline constexpr Uint32 ROCKET_FADE_MS = 1000;
inline constexpr Uint32 BIOHAZARD_SPAWN_MIN_INTERVAL_MS = 14000;
inline constexpr Uint32 BIOHAZARD_SPAWN_MAX_INTERVAL_MS = 26000;
inline constexpr Uint32 BIOHAZARD_VISIBLE_MS = 60000;
inline constexpr Uint32 BIOHAZARD_FADE_MS = 1000;
inline constexpr Uint32 NUCLEAR_BOMB_SPAWN_MIN_INTERVAL_MS = 18000;
inline constexpr Uint32 NUCLEAR_BOMB_SPAWN_MAX_INTERVAL_MS = 32000;
inline constexpr Uint32 NUCLEAR_BOMB_VISIBLE_MS = 60000;
inline constexpr Uint32 NUCLEAR_BOMB_FADE_MS = 1000;
inline constexpr Uint32 LOVE_POTION_SPAWN_MIN_INTERVAL_MS = 10000;
inline constexpr Uint32 LOVE_POTION_SPAWN_MAX_INTERVAL_MS = 18000;
inline constexpr Uint32 LOVE_POTION_VISIBLE_MS = 60000;
inline constexpr Uint32 LOVE_POTION_FADE_MS = 1000;
inline constexpr Uint32 LIFE_PICKUP_SPAWN_MIN_INTERVAL_MS = 25000;
inline constexpr Uint32 LIFE_PICKUP_SPAWN_MAX_INTERVAL_MS = 45000;
inline constexpr Uint32 LIFE_PICKUP_FADE_MS = 1000;
inline constexpr Uint32 DISCO_PICKUP_SPAWN_MIN_INTERVAL_MS = 30000;
inline constexpr Uint32 DISCO_PICKUP_SPAWN_MAX_INTERVAL_MS = 60000;
inline constexpr Uint32 DISCO_PICKUP_VISIBLE_MS = 60000;
inline constexpr Uint32 DISCO_PICKUP_FADE_MS = 1000;
inline constexpr Uint32 DISCO_DIM_DOWN_MS = 1200;
inline constexpr Uint32 DISCO_BALL_DROP_MS = 1600;
inline constexpr Uint32 DISCO_BRIGHTEN_MS = 1300;
inline constexpr Uint32 DISCO_BALL_RAISE_MS = 1500;
inline constexpr Uint32 DISCO_MUSIC_FADE_OUT_MS = 1800;
inline constexpr double DISCO_ROTATION_PERIOD_MS = 2800.0;
inline constexpr double DISCO_ROTATION_SPEED_MIN = 0.25;
inline constexpr double DISCO_ROTATION_SPEED_MAX = 5.0;
inline constexpr double DISCO_ROTATION_SPEED_STEP = 0.15;
inline constexpr Uint8 DISCO_MAX_DIM_ALPHA = 165;
inline constexpr double DISCO_AXIS_TILT_DEGREES = -18.0;
inline constexpr int DISCO_LIGHT_SPOT_COUNT = 240;
inline constexpr double DISCO_LIGHT_SPOT_GOLDEN_ANGLE_RADIANS =
    2.399963229728653;
inline constexpr double DISCO_LIGHT_SPOT_HASH_A = 12.9898;
inline constexpr double DISCO_LIGHT_SPOT_HASH_B = 78.233;
inline constexpr double DISCO_LIGHT_SPOT_HASH_SCALE = 43758.5453;
inline constexpr int DISCO_LIGHT_SPOT_RADIAL_STEP = 37;
inline constexpr double DISCO_LIGHT_SPOT_RADIAL_BASE = 0.35;
inline constexpr double DISCO_LIGHT_SPOT_RADIAL_JITTER_SCALE = 0.45;
inline constexpr double DISCO_LIGHT_SPOT_ORBIT_WOBBLE = 0.18;
inline constexpr double DISCO_LIGHT_SPOT_ORBIT_WOBBLE_FREQUENCY = 2.0;
inline constexpr double DISCO_LIGHT_SPOT_FIELD_RADIUS_X = 0.88;
inline constexpr double DISCO_LIGHT_SPOT_FIELD_RADIUS_Y = 0.78;
inline constexpr double DISCO_LIGHT_SPOT_PERSPECTIVE_BASE = 0.62;
inline constexpr double DISCO_LIGHT_SPOT_PERSPECTIVE_SWING = 0.38;
inline constexpr double DISCO_LIGHT_SPOT_DEPTH_DENOMINATOR = 1.4;
inline constexpr double DISCO_LIGHT_SPOT_DEPTH_SCALE_MIN = -0.18;
inline constexpr double DISCO_LIGHT_SPOT_DEPTH_SCALE_MAX = 0.18;
inline constexpr int DISCO_LIGHT_SPOT_MIN_WIDTH_PX = 82;
inline constexpr int DISCO_LIGHT_SPOT_MIN_HEIGHT_PX = 44;
inline constexpr double DISCO_LIGHT_SPOT_WIDTH_CELLS = 3.35;
inline constexpr double DISCO_LIGHT_SPOT_WIDTH_JITTER_CELLS = 1.00;
inline constexpr double DISCO_LIGHT_SPOT_HEIGHT_SCALE = 0.54;
inline constexpr double DISCO_LIGHT_SPOT_HEIGHT_RADIAL_SCALE = 0.10;
inline constexpr double DISCO_LIGHT_SPOT_ALPHA_BASE = 58.0;
inline constexpr double DISCO_LIGHT_SPOT_ALPHA_MIN_FACTOR = 0.70;
inline constexpr double DISCO_LIGHT_SPOT_ALPHA_SWING = 0.30;
inline constexpr double DISCO_LIGHT_SPOT_ALPHA_PHASE_SCALE = 1.7;
inline constexpr double DISCO_LIGHT_SPOT_ALPHA_RADIAL_FADE = 0.18;
inline constexpr double DISCO_LIGHT_SPOT_ALPHA_MAX = 105.0;
inline constexpr int DISCO_LIGHT_SPOT_HALO_EXPAND_DIVISOR = 4;
inline constexpr int DISCO_LIGHT_SPOT_HALO_ALPHA_DIVISOR = 2;
inline constexpr std::array<SDL_Color, 6> DISCO_LIGHT_SPOT_COLORS{{
    {255, 56, 86, 255}, {64, 190, 255, 255}, {255, 230, 72, 255},
    {90, 255, 150, 255}, {226, 96, 255, 255}, {255, 142, 54, 255},
}};
inline constexpr int DISCO_BALL_MIN_RADIUS_PX = 136;
inline constexpr double DISCO_BALL_RADIUS_CELLS = 4.2;
inline constexpr double DISCO_BALL_SCREEN_Y_SCALE = 0.94;
inline constexpr double DISCO_BALL_CORNER_CLIP_RADIUS = 1.03;
inline constexpr double DISCO_BALL_BACKFACE_CULL_Z = -0.04;
inline constexpr int DISCO_BALL_BACKGROUND_RING_COUNT = 8;
inline constexpr double DISCO_BALL_BACKGROUND_RING_SHRINK = 0.88;
inline constexpr int DISCO_BALL_BACKGROUND_SHADE_BASE = 62;
inline constexpr int DISCO_BALL_BACKGROUND_SHADE_STEP = 18;
inline constexpr int DISCO_BALL_LATITUDE_COUNT = 22;
inline constexpr int DISCO_BALL_LONGITUDE_COUNT = 56;
inline constexpr double DISCO_BALL_FRAGMENT_THETA_HALF_SCALE = 0.515;
inline constexpr double DISCO_BALL_FRAGMENT_PHI_HALF_SCALE = 0.515;
inline constexpr double DISCO_BALL_AMBIENT_BASE = 0.16;
inline constexpr double DISCO_BALL_VIEW_LIGHT_SCALE = 0.22;
inline constexpr double DISCO_BALL_RED_BASE = 54.0;
inline constexpr double DISCO_BALL_GREEN_BASE = 60.0;
inline constexpr double DISCO_BALL_BLUE_BASE = 72.0;
inline constexpr double DISCO_BALL_RED_VIEW_SCALE = 72.0;
inline constexpr double DISCO_BALL_GREEN_VIEW_SCALE = 78.0;
inline constexpr double DISCO_BALL_BLUE_VIEW_SCALE = 88.0;
inline constexpr double DISCO_BALL_DIFFUSE_COLOR_SCALE = 0.32;
inline constexpr double DISCO_BALL_SPECULAR_COLOR_SCALE = 1.08;
inline constexpr double DISCO_BALL_SPECULAR_POWER = 46.0;
inline constexpr double DISCO_BALL_FACET_NOISE_BASE = 0.88;
inline constexpr double DISCO_BALL_FACET_NOISE_SWING = 0.18;
inline constexpr double DISCO_BALL_FACET_NOISE_LON_SCALE = 12.13;
inline constexpr double DISCO_BALL_FACET_NOISE_LAT_SCALE = 7.71;
inline constexpr double DISCO_BALL_TILE_ALPHA_BASE = 0.62;
inline constexpr double DISCO_BALL_TILE_ALPHA_LIGHT_SCALE = 0.38;
inline constexpr double DISCO_BALL_SPECULAR_GLINT_THRESHOLD = 0.18;
inline constexpr double DISCO_BALL_SPECULAR_GLINT_ALPHA_MAX = 190.0;
inline constexpr double DISCO_BALL_SPECULAR_GLINT_RADIUS_CELLS = 0.011;
inline constexpr double DISCO_BALL_SPECULAR_GLINT_MIN_RADIUS_PX = 1.0;
inline constexpr int DISCO_BALL_EDGE_ALPHA_DIVISOR = 10;

struct DiscoDirectionalLight {
  double x;
  double y;
  double z;
  SDL_Color color;
  double diffuse;
  double specular;
};

inline constexpr std::array<DiscoDirectionalLight, 5>
    DISCO_BALL_DIRECTIONAL_LIGHTS{{
        {-0.72, -0.58, 0.92, {120, 200, 255, 255}, 0.42, 1.10},
        {0.80, -0.38, 0.72, {255, 96, 160, 255}, 0.34, 0.95},
        {-0.18, 0.86, 0.62, {255, 230, 82, 255}, 0.28, 0.75},
        {0.58, 0.46, 0.82, {104, 255, 170, 255}, 0.26, 0.82},
        {0.06, -0.96, 0.48, {226, 132, 255, 255}, 0.22, 0.88},
    }};
inline constexpr int DISCO_CHAIN_TOP_Y = -6;
inline constexpr double DISCO_CHAIN_TOP_ATTACHMENT_RADIUS_Y_SCALE = 0.94;
inline constexpr double DISCO_CHAIN_BOTTOM_PADDING_CELLS = 1.0 / 18.0;
inline constexpr int DISCO_CHAIN_BOTTOM_MIN_PADDING_PX = 6;
inline constexpr double DISCO_CHAIN_LINK_PHASE_STEP_RADIANS_SCALE = 0.62;
inline constexpr int DISCO_CHAIN_LINK_MIN_STEP_PX = 10;
inline constexpr int DISCO_CHAIN_LINK_STEP_CELL_DIVISOR = 3;
inline constexpr int DISCO_CHAIN_DEPTH_RADIUS_DIVISOR = 24;
inline constexpr int DISCO_CHAIN_DEPTH_MIN_PX = 4;
inline constexpr int DISCO_CHAIN_LINK_MIN_WIDTH_PX = 7;
inline constexpr int DISCO_CHAIN_LINK_MIN_HEIGHT_PX = 13;
inline constexpr double DISCO_CHAIN_LINK_WIDTH_BASE = 0.035;
inline constexpr double DISCO_CHAIN_LINK_WIDTH_FACING_SCALE = 0.030;
inline constexpr double DISCO_CHAIN_LINK_HEIGHT_BASE = 0.050;
inline constexpr double DISCO_CHAIN_LINK_HEIGHT_FACING_SCALE = 0.014;
inline constexpr double DISCO_CHAIN_BACK_ALPHA_BASE = 96.0;
inline constexpr double DISCO_CHAIN_BACK_ALPHA_SCALE = 70.0;
inline constexpr double DISCO_CHAIN_METAL_ALPHA_BASE = 160.0;
inline constexpr double DISCO_CHAIN_METAL_ALPHA_SCALE = 72.0;
inline constexpr double DISCO_CHAIN_HIGHLIGHT_ALPHA_BASE = 70.0;
inline constexpr double DISCO_CHAIN_HIGHLIGHT_ALPHA_SCALE = 95.0;
inline constexpr double DISCO_DANCE_JUMP_HEIGHT_CELLS = 0.20;
inline constexpr double DISCO_DANCE_JUMP_MIN_PX = 4.0;
inline constexpr double DISCO_DANCE_BEAT_DIVISOR_MS = 85.0;
inline constexpr double DISCO_DANCE_GATE_DIVISOR_MS = 240.0;
inline constexpr double DISCO_DANCE_GATE_THRESHOLD = -0.55;
inline constexpr int DISCO_PACMAN_DANCE_PHASE_SEED_OFFSET = 41;
inline constexpr int DISCO_DANCE_PHASE_SEED_MULTIPLIER_A = 17;
inline constexpr int DISCO_DANCE_PHASE_SEED_MULTIPLIER_B = 29;
inline constexpr int DISCO_MONSTER_DANCE_ID_MULTIPLIER = 73;
inline constexpr Uint32 DISCO_PICKUP_RETRY_DELAY_MS = 1000;
inline constexpr Uint32 DISCO_PICKUP_ANIMATION_SEED_MODULUS = 991;
inline constexpr int DISCO_PICKUP_ANIMATION_SEED_ROW_MULTIPLIER = 127;
inline constexpr int DISCO_PICKUP_ANIMATION_SEED_COL_MULTIPLIER = 89;
inline constexpr Uint32 DISCO_EASTER_EGG_ANIMATION_SEED_MODULUS = 997;
inline constexpr int DISCO_EASTER_EGG_ANIMATION_SEED_ROW_MULTIPLIER = 131;
inline constexpr int DISCO_EASTER_EGG_ANIMATION_SEED_COL_MULTIPLIER = 97;
inline constexpr int DISCO_PICKUP_RENDER_ANIMATION_SEED_MULTIPLIER = 61;
inline constexpr double DISCO_PICKUP_RENDER_ANIMATION_DIVISOR_MS = 150.0;
inline constexpr double DISCO_PICKUP_PULSE_BASE = 0.86;
inline constexpr double DISCO_PICKUP_PULSE_SWING = 0.16;
inline constexpr double DISCO_PICKUP_PULSE_FREQUENCY = 1.4;
inline constexpr double DISCO_PICKUP_GLOW_ALPHA_BASE = 140.0;
inline constexpr double DISCO_PICKUP_GLOW_ALPHA_MAX = 210.0;
inline constexpr double DISCO_PICKUP_BODY_ALPHA_BASE = 255.0;
inline constexpr double DISCO_PICKUP_GLOW_RADIUS_CELLS = 0.35;
inline constexpr double DISCO_PICKUP_RING_RADIUS_CELLS = 0.46;
inline constexpr int DISCO_PICKUP_GLOW_MIN_RADIUS_PX = 8;
inline constexpr int DISCO_PICKUP_RING_MIN_RADIUS_PX = 10;
inline constexpr double DISCO_PICKUP_ICON_SIZE_CELLS = 0.80;
inline constexpr int DISCO_PICKUP_ICON_MIN_SIZE_PX = 12;
inline constexpr SDL_Color DISCO_PICKUP_GLOW_COLOR{160, 216, 255, 255};
inline constexpr SDL_Color DISCO_PICKUP_RING_COLOR{255, 250, 205, 255};
inline constexpr double DISCO_PICKUP_ICON_CHAIN_TOP_DIVISOR = 12.0;
inline constexpr double DISCO_PICKUP_ICON_CHAIN_BOTTOM_DIVISOR = 4.0;
inline constexpr double DISCO_PICKUP_ICON_CHAIN_STEP_DIVISOR = 10.0;
inline constexpr double DISCO_PICKUP_ICON_CHAIN_LINK_X_DIVISOR = 18.0;
inline constexpr double DISCO_PICKUP_ICON_CHAIN_LINK_W_DIVISOR = 9.0;
inline constexpr double DISCO_PICKUP_ICON_CHAIN_LINK_H_DIVISOR = 11.0;
inline constexpr double DISCO_PICKUP_ICON_BALL_RADIUS_DIVISOR = 3.0;
inline constexpr double DISCO_PICKUP_ICON_BALL_CENTER_Y_DIVISOR = 4.0;
inline constexpr double DISCO_PICKUP_ICON_ROTATION_DIVISOR = 8.0;
inline constexpr SDL_Color DISCO_PICKUP_ICON_CHAIN_COLOR{210, 220, 226, 255};
inline constexpr double DISCO_CHAIN_SHADOW_OFFSET_X_PX = 1.4;
inline constexpr double DISCO_CHAIN_SHADOW_OFFSET_Y_PX = 1.8;
inline constexpr double DISCO_CHAIN_SHADOW_RADIUS_X_SCALE = 0.56;
inline constexpr double DISCO_CHAIN_SHADOW_RADIUS_Y_SCALE = 0.58;
inline constexpr double DISCO_CHAIN_OUTER_RADIUS_X_SCALE = 0.52;
inline constexpr double DISCO_CHAIN_OUTER_RADIUS_Y_SCALE = 0.56;
inline constexpr double DISCO_CHAIN_INNER_RADIUS_X_SCALE = 0.29;
inline constexpr double DISCO_CHAIN_INNER_RADIUS_Y_SCALE = 0.34;
inline constexpr double DISCO_CHAIN_HIGHLIGHT_X0_SCALE = -0.32;
inline constexpr double DISCO_CHAIN_HIGHLIGHT_Y0_SCALE = -0.32;
inline constexpr double DISCO_CHAIN_HIGHLIGHT_X1_SCALE = 0.20;
inline constexpr double DISCO_CHAIN_HIGHLIGHT_Y1_SCALE = -0.44;
inline constexpr double DISCO_CHAIN_SHADOW_LINE_X0_SCALE = 0.30;
inline constexpr double DISCO_CHAIN_SHADOW_LINE_Y0_SCALE = 0.28;
inline constexpr double DISCO_CHAIN_SHADOW_LINE_X1_SCALE = 0.08;
inline constexpr double DISCO_CHAIN_SHADOW_LINE_Y1_SCALE = 0.44;
inline constexpr SDL_Color DISCO_CHAIN_SHADOW_COLOR{22, 26, 34, 255};
inline constexpr SDL_Color DISCO_CHAIN_METAL_COLOR{108, 118, 130, 255};
inline constexpr SDL_Color DISCO_CHAIN_INNER_COLOR{10, 12, 18, 210};
inline constexpr SDL_Color DISCO_CHAIN_HIGHLIGHT_COLOR{238, 246, 255, 255};
inline constexpr SDL_Color DISCO_CHAIN_SHADOW_LINE_COLOR{42, 48, 58, 150};
inline constexpr Uint32 GOAT_LOVE_HEART_VISIBLE_MS = 4100;
inline constexpr Uint32 GOAT_MONSTER_DUST_LIFETIME_MS = 4800;
inline constexpr int GOAT_MONSTER_DUST_PUFF_COUNT = 56;
inline constexpr int LOVE_SMOKE_RANGE_CELLS = 5;
inline constexpr Uint32 LOVE_SMOKE_STEP_DURATION_MS = 140;
inline constexpr int LOVE_SMOKE_TRAIL_PUFF_COUNT = 12;
inline constexpr int LOVE_SMOKE_IMPACT_PUFF_COUNT = 48;
inline constexpr Uint32 LOVE_SMOKE_TRAIL_LIFETIME_MS = 900;
inline constexpr Uint32 LOVE_SMOKE_IMPACT_LIFETIME_MS = 1500;
inline constexpr float LOVE_SMOKE_TRAIL_INITIAL_RADIUS_CELLS = 0.16f;
inline constexpr float LOVE_SMOKE_TRAIL_FINAL_RADIUS_CELLS = 0.55f;
inline constexpr float LOVE_SMOKE_IMPACT_INITIAL_RADIUS_CELLS = 0.20f;
inline constexpr float LOVE_SMOKE_IMPACT_FINAL_RADIUS_CELLS = 0.85f;
inline constexpr Uint8 LOVE_SMOKE_TRAIL_INITIAL_ALPHA = 80;
inline constexpr Uint8 LOVE_SMOKE_IMPACT_INITIAL_ALPHA = 31;
inline constexpr SDL_Color LOVE_SMOKE_COLOR{255, 50, 70, 255};
inline constexpr SDL_Color LOVE_SMOKE_HIGHLIGHT_COLOR{240, 130, 150, 255};
inline constexpr Uint32 NUCLEAR_BOMB_ALARM_MS = 2400;
inline constexpr Uint32 NUCLEAR_BOMB_FALL_MS = 1400;
inline constexpr float NUCLEAR_BOMB_INITIAL_RENDER_SCALE_CELLS = 12.0f;
inline constexpr float NUCLEAR_BOMB_FINAL_RENDER_SCALE_CELLS = 0.72f;
inline constexpr double NUCLEAR_BOMB_EXPLOSION_GAIN = 3.0;
inline constexpr Uint32 BIOHAZARD_BEAM_ACTIVE_MS = 3000;
inline constexpr Uint32 BIOHAZARD_BEAM_MIN_VISIBLE_MS = 1000;
inline constexpr Uint32 BIOHAZARD_HIT_SEQUENCE_MS = 1000;
inline constexpr Uint32 BIOHAZARD_IMPACT_FLASH_DURATION_MS =
    BIOHAZARD_HIT_SEQUENCE_MS;
inline constexpr int BIOHAZARD_CHARGE_STEP_DELAY_MS = 1;
inline constexpr double BIOHAZARD_SIDE_BEAM_RAISE_FACTOR = 0.18;
inline constexpr double ELECTRIFIED_MONSTER_ROAR_GAIN = 2.2;
inline constexpr Uint32 ELECTRIFIED_ATTACK_FLICKER_FRAME_MS = 22;
inline constexpr int ELECTRIFIED_ATTACK_BOLT_MIN_COUNT = 18;
inline constexpr int ELECTRIFIED_ATTACK_BOLT_MAX_COUNT = 24;
inline constexpr float ELECTRIFIED_ATTACK_BOLT_LENGTH_MIN_FACTOR = 0.30f;
inline constexpr float ELECTRIFIED_ATTACK_BOLT_LENGTH_MAX_FACTOR = 0.72f;
inline constexpr float ELECTRIFIED_ATTACK_BRANCH_CHANCE = 0.72f;
inline constexpr Uint32 PLASMA_SHOCKWAVE_DURATION_MS = 1350;
inline constexpr int PLASMA_SHOCKWAVE_RADIUS_CELLS = 10;
inline constexpr Uint32 DYNAMITE_COUNTDOWN_MS = 5000;
inline constexpr Uint32 DYNAMITE_EXPLOSION_DURATION_MS = 2000;
inline constexpr int DYNAMITE_EXPLOSION_RADIUS_CELLS = 3;
inline constexpr float DYNAMITE_EXPLOSION_SMOKE_RADIUS_CELLS = 3.0f;
inline constexpr float DYNAMITE_EXPLOSION_SMOKE_DENSITY_MULTIPLIER = 6.5f;
inline constexpr float PLASTIC_EXPLOSIVE_SMOKE_RADIUS_CELLS = 1.5f;
inline constexpr float PLASTIC_EXPLOSIVE_SMOKE_DENSITY_MULTIPLIER = 5.5f;
inline constexpr Uint32 DYNAMITE_CHAIN_BASE_DELAY_MS = 140;
inline constexpr Uint32 DYNAMITE_CHAIN_STEP_DELAY_MS = 80;
inline constexpr Uint32 MONSTER_EXPLOSION_FRAME_MS = 35;
inline constexpr int MONSTER_EXPLOSION_FRAME_COUNT = 24;
inline constexpr Uint32 MONSTER_EXPLOSION_FADE_IN_DURATION_MS =
    MONSTER_EXPLOSION_FRAME_MS * 2;
inline constexpr int MONSTER_EXPLOSION_FADE_OUT_TAIL_FRAME_COUNT = 5;
inline constexpr Uint32 MONSTER_EXPLOSION_DURATION_MS =
    MONSTER_EXPLOSION_FRAME_MS * MONSTER_EXPLOSION_FRAME_COUNT;

/**
 * @brief Tuning for the debris/smoke trail that accompanies a monster
 *  explosion. Counts are inclusive; speeds and radii are expressed in cells
 *  (1 cell = one map tile). Lifetimes are milliseconds.
 */
inline constexpr int MONSTER_EXPLOSION_PARTICLE_MIN_COUNT = 18;
inline constexpr int MONSTER_EXPLOSION_PARTICLE_MAX_COUNT = 38;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_MIN_SPEED_CELLS_PER_SEC =
    1.0f;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_MAX_SPEED_CELLS_PER_SEC =
    4.5f;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_GRAVITY_CELLS_PER_SEC2 =
    3.0f;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_INITIAL_UPWARD_BIAS = 0.60f;
inline constexpr Uint32 MONSTER_EXPLOSION_PARTICLE_LIFETIME_MS = 400;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_RADIUS_CELLS = 0.13f;
inline constexpr int MONSTER_EXPLOSION_PARTICLE_BLOB_MIN_COUNT = 8;
inline constexpr int MONSTER_EXPLOSION_PARTICLE_BLOB_MAX_COUNT = 10;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_BLOB_OFFSET_FACTOR = 0.85f;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_BLOB_RADIUS_MIN_FACTOR =
    0.35f;
inline constexpr float MONSTER_EXPLOSION_PARTICLE_BLOB_RADIUS_MAX_FACTOR =
    0.65f;
inline constexpr Uint8 MONSTER_EXPLOSION_PARTICLE_INITIAL_ALPHA = 220;
inline constexpr SDL_Color MONSTER_EXPLOSION_PARTICLE_COLOR{56, 52, 50, 255};
inline constexpr SDL_Color MONSTER_EXPLOSION_PARTICLE_HIGHLIGHT_COLOR{96, 90,
                                                                     86, 255};
inline constexpr Uint32 MONSTER_EXPLOSION_SMOKE_SPAWN_INTERVAL_MS = 32;
inline constexpr Uint32 MONSTER_EXPLOSION_SMOKE_LIFETIME_MS = 1500;
inline constexpr float MONSTER_EXPLOSION_SMOKE_INITIAL_RADIUS_CELLS = 0.10f;
inline constexpr float MONSTER_EXPLOSION_SMOKE_FINAL_RADIUS_CELLS = 0.46f;
inline constexpr Uint8 MONSTER_EXPLOSION_SMOKE_INITIAL_ALPHA = 165;
inline constexpr SDL_Color MONSTER_EXPLOSION_SMOKE_COLOR{82, 76, 72, 255};
inline constexpr SDL_Color MONSTER_EXPLOSION_SMOKE_HIGHLIGHT_COLOR{132, 124,
                                                                  118, 255};
inline constexpr int MONSTER_EXPLOSION_SMOKE_BLOB_MIN_COUNT = 5;
inline constexpr int MONSTER_EXPLOSION_SMOKE_BLOB_MAX_COUNT = 9;
inline constexpr float MONSTER_EXPLOSION_SMOKE_BLOB_OFFSET_FACTOR = 0.95f;
inline constexpr float MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.45f;
inline constexpr float MONSTER_EXPLOSION_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.10f;
inline constexpr float MONSTER_EXPLOSION_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.04f;
inline constexpr float MONSTER_EXPLOSION_SMOKE_WOBBLE_FREQUENCY_HZ = 1.4f;

inline constexpr int PLASTIC_EXPLOSIVE_WALL_DEBRIS_MIN_COUNT = 12;
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DEBRIS_MAX_COUNT = 26;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_MIN_SPEED_CELLS_PER_SEC = 2.9f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_MAX_SPEED_CELLS_PER_SEC = 9.3f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_GRAVITY_CELLS_PER_SEC2 = 10.4f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_INITIAL_UPWARD_BIAS = 1.32f;
inline constexpr Uint32 PLASTIC_EXPLOSIVE_WALL_DEBRIS_LIFETIME_MS = 4520;
inline constexpr float PLASTIC_EXPLOSIVE_WALL_DEBRIS_RADIUS_CELLS = 0.12f;
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_MIN_COUNT = 3;
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_MAX_COUNT = 9;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_OFFSET_FACTOR = 0.68f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_RADIUS_MIN_FACTOR = 0.42f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_BLOB_RADIUS_MAX_FACTOR = 0.94f;
inline constexpr Uint8 PLASTIC_EXPLOSIVE_WALL_DEBRIS_INITIAL_ALPHA = 232;
inline constexpr SDL_Color PLASTIC_EXPLOSIVE_WALL_DEBRIS_COLOR{118, 88, 76,
                                                               255};
inline constexpr SDL_Color
    PLASTIC_EXPLOSIVE_WALL_DEBRIS_HIGHLIGHT_COLOR{174, 136, 118, 255};
inline constexpr Uint32 PLASTIC_EXPLOSIVE_WALL_DUST_SPAWN_INTERVAL_MS = 28;
inline constexpr Uint32 PLASTIC_EXPLOSIVE_WALL_DUST_LIFETIME_MS = 800;
inline constexpr float PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_RADIUS_CELLS = 0.10f;
inline constexpr float PLASTIC_EXPLOSIVE_WALL_DUST_FINAL_RADIUS_CELLS = 0.62f;
inline constexpr Uint8 PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_ALPHA = 46;
inline constexpr SDL_Color PLASTIC_EXPLOSIVE_WALL_DUST_COLOR{146, 78, 66, 255};
inline constexpr SDL_Color
    PLASTIC_EXPLOSIVE_WALL_DUST_HIGHLIGHT_COLOR{196, 122, 108, 255};
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_MIN_COUNT = 4;
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_MAX_COUNT = 8;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_OFFSET_FACTOR = 0.98f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_RADIUS_MIN_FACTOR = 0.48f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DUST_BLOB_RADIUS_MAX_FACTOR = 1.18f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DUST_WOBBLE_AMPLITUDE_CELLS = 0.05f;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DUST_WOBBLE_FREQUENCY_HZ = 1.2f;
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_PUFF_MIN_COUNT = 8;
inline constexpr int PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_PUFF_MAX_COUNT = 14;
inline constexpr float
    PLASTIC_EXPLOSIVE_WALL_DUST_INITIAL_SPREAD_CELLS = 0.34f;

// Atombomben-Fragmente: gleiche Logik wie Mauer-Debris, aber graue
// Truemmerstuecke und graue Rauchspuren. Riesige Anzahl an Partikeln, die in
// parabelfoermiger Flugbahn weggeschleudert werden.
inline constexpr int NUCLEAR_BOMB_DEBRIS_MIN_COUNT = 180;
inline constexpr int NUCLEAR_BOMB_DEBRIS_MAX_COUNT = 240;
inline constexpr float NUCLEAR_BOMB_DEBRIS_MIN_SPEED_CELLS_PER_SEC = 7.5f;
inline constexpr float NUCLEAR_BOMB_DEBRIS_MAX_SPEED_CELLS_PER_SEC = 24.0f;
inline constexpr float NUCLEAR_BOMB_DEBRIS_GRAVITY_CELLS_PER_SEC2 = 12.4f;
inline constexpr float NUCLEAR_BOMB_DEBRIS_INITIAL_UPWARD_BIAS = 0.25f;
inline constexpr Uint32 NUCLEAR_BOMB_DEBRIS_LIFETIME_MS = 5200;
inline constexpr float NUCLEAR_BOMB_DEBRIS_RADIUS_CELLS = 0.22f;
inline constexpr int NUCLEAR_BOMB_DEBRIS_BLOB_MIN_COUNT = 9;
inline constexpr int NUCLEAR_BOMB_DEBRIS_BLOB_MAX_COUNT = 22;
inline constexpr float NUCLEAR_BOMB_DEBRIS_BLOB_OFFSET_FACTOR = 0.68f;
inline constexpr float NUCLEAR_BOMB_DEBRIS_BLOB_RADIUS_MIN_FACTOR = 0.52f;
inline constexpr float NUCLEAR_BOMB_DEBRIS_BLOB_RADIUS_MAX_FACTOR = 1.44f;
inline constexpr Uint8 NUCLEAR_BOMB_DEBRIS_INITIAL_ALPHA = 202;
inline constexpr SDL_Color NUCLEAR_BOMB_DEBRIS_COLOR{96, 92, 88, 255};
inline constexpr SDL_Color NUCLEAR_BOMB_DEBRIS_HIGHLIGHT_COLOR{162, 156, 150,
                                                                255};
inline constexpr Uint32 NUCLEAR_BOMB_DUST_SPAWN_INTERVAL_MS = 12;
inline constexpr Uint32 NUCLEAR_BOMB_DUST_LIFETIME_MS = 1500;
inline constexpr float NUCLEAR_BOMB_DUST_INITIAL_RADIUS_CELLS = 0.18f;
inline constexpr float NUCLEAR_BOMB_DUST_FINAL_RADIUS_CELLS = 0.95f;
inline constexpr Uint8 NUCLEAR_BOMB_DUST_INITIAL_ALPHA = 150;
inline constexpr SDL_Color NUCLEAR_BOMB_DUST_COLOR{110, 108, 104, 255};
inline constexpr SDL_Color NUCLEAR_BOMB_DUST_HIGHLIGHT_COLOR{170, 166, 160,
                                                              255};
inline constexpr int NUCLEAR_BOMB_DUST_BLOB_MIN_COUNT = 4;
inline constexpr int NUCLEAR_BOMB_DUST_BLOB_MAX_COUNT = 8;
inline constexpr float NUCLEAR_BOMB_DUST_BLOB_OFFSET_FACTOR = 0.98f;
inline constexpr float NUCLEAR_BOMB_DUST_BLOB_RADIUS_MIN_FACTOR = 0.48f;
inline constexpr float NUCLEAR_BOMB_DUST_BLOB_RADIUS_MAX_FACTOR = 1.18f;
inline constexpr float NUCLEAR_BOMB_DUST_WOBBLE_AMPLITUDE_CELLS = 0.05f;
inline constexpr float NUCLEAR_BOMB_DUST_WOBBLE_FREQUENCY_HZ = 1.2f;
inline constexpr float NUCLEAR_BOMB_DEBRIS_SPAWN_SPREAD_CELLS = 1.4f;

inline constexpr int EXPLOSION_SMOKE_CLOUD_MIN_PUFF_COUNT = 14;
inline constexpr int EXPLOSION_SMOKE_CLOUD_MAX_PUFF_COUNT = 22;
inline constexpr float EXPLOSION_SMOKE_CLOUD_RING_MIN_RADIUS_CELLS = 0.55f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_RING_INNER_RADIUS_FACTOR = 0.48f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_RING_OUTER_RADIUS_FACTOR = 0.82f;
inline constexpr Uint32 EXPLOSION_SMOKE_CLOUD_LIFETIME_MS = 1200;
inline constexpr float EXPLOSION_SMOKE_CLOUD_INITIAL_RADIUS_CELLS = 0.09f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_FINAL_RADIUS_CELLS = 0.34f;
inline constexpr Uint8 EXPLOSION_SMOKE_CLOUD_INITIAL_ALPHA = 84;
inline constexpr SDL_Color EXPLOSION_SMOKE_CLOUD_COLOR{78, 74, 72, 255};
inline constexpr SDL_Color EXPLOSION_SMOKE_CLOUD_HIGHLIGHT_COLOR{120, 114, 108,
                                                                 255};
inline constexpr int EXPLOSION_SMOKE_CLOUD_BLOB_MIN_COUNT = 4;
inline constexpr int EXPLOSION_SMOKE_CLOUD_BLOB_MAX_COUNT = 7;
inline constexpr float EXPLOSION_SMOKE_CLOUD_BLOB_OFFSET_FACTOR = 0.86f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_BLOB_RADIUS_MIN_FACTOR = 0.44f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_BLOB_RADIUS_MAX_FACTOR = 0.98f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_WOBBLE_AMPLITUDE_CELLS = 0.03f;
inline constexpr float EXPLOSION_SMOKE_CLOUD_WOBBLE_FREQUENCY_HZ = 1.0f;

/**
 * @brief Rocket impact smoke and blast tuning.
 *
 * The rocket impact uses a separate, larger smoke ring that surrounds a
 * deliberately empty center so the bright explosion core stays readable.
 * Compared to the generic blast smoke, this variant spawns more puffs over a
 * larger radius, but each puff is more transparent and fades faster so the
 * scene clears quickly.
 */
inline constexpr int ROCKET_EXPLOSION_VISIBLE_RADIUS_CELLS = 3;
inline constexpr int ROCKET_BLAST_SMOKE_MIN_PUFF_COUNT = 28;
inline constexpr int ROCKET_BLAST_SMOKE_MAX_PUFF_COUNT = 40;
inline constexpr float ROCKET_BLAST_SMOKE_RING_INNER_RADIUS_CELLS = 1.05f;
inline constexpr float ROCKET_BLAST_SMOKE_RING_OUTER_RADIUS_CELLS = 2.20f;
inline constexpr Uint32 ROCKET_BLAST_SMOKE_LIFETIME_MS = 700;
inline constexpr float ROCKET_BLAST_SMOKE_INITIAL_RADIUS_CELLS = 0.14f;
inline constexpr float ROCKET_BLAST_SMOKE_FINAL_RADIUS_CELLS = 0.46f;
inline constexpr Uint8 ROCKET_BLAST_SMOKE_INITIAL_ALPHA = 150;
inline constexpr SDL_Color ROCKET_BLAST_SMOKE_COLOR{86, 80, 76, 255};
inline constexpr SDL_Color ROCKET_BLAST_SMOKE_HIGHLIGHT_COLOR{132, 124, 118, 255};
inline constexpr int ROCKET_BLAST_SMOKE_BLOB_MIN_COUNT = 4;
inline constexpr int ROCKET_BLAST_SMOKE_BLOB_MAX_COUNT = 7;
inline constexpr float ROCKET_BLAST_SMOKE_BLOB_OFFSET_FACTOR = 0.86f;
inline constexpr float ROCKET_BLAST_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.42f;
inline constexpr float ROCKET_BLAST_SMOKE_BLOB_RADIUS_MAX_FACTOR = 0.96f;
inline constexpr float ROCKET_BLAST_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.05f;
inline constexpr float ROCKET_BLAST_SMOKE_WOBBLE_FREQUENCY_HZ = 1.4f;

inline constexpr Uint32 ROCKET_TRAIL_SMOKE_SPAWN_INTERVAL_MS = 48;
inline constexpr int ROCKET_TRAIL_SMOKE_PUFFS_PER_SPAWN = 2;
inline constexpr float ROCKET_TRAIL_SMOKE_BACK_OFFSET_CELLS = 0.34f;
inline constexpr float ROCKET_TRAIL_SMOKE_LATERAL_SPREAD_CELLS = 0.10f;
inline constexpr float ROCKET_TRAIL_SMOKE_LONGITUDINAL_JITTER_CELLS = 0.08f;
inline constexpr Uint32 ROCKET_TRAIL_SMOKE_LIFETIME_MS = 620;
inline constexpr float ROCKET_TRAIL_SMOKE_INITIAL_RADIUS_CELLS = 0.09f;
inline constexpr float ROCKET_TRAIL_SMOKE_FINAL_RADIUS_CELLS = 0.30f;
inline constexpr Uint8 ROCKET_TRAIL_SMOKE_INITIAL_ALPHA = 56;
inline constexpr SDL_Color ROCKET_TRAIL_SMOKE_COLOR{92, 92, 96, 255};
inline constexpr SDL_Color ROCKET_TRAIL_SMOKE_HIGHLIGHT_COLOR{156, 156, 162, 255};
inline constexpr int ROCKET_TRAIL_SMOKE_BLOB_MIN_COUNT = 4;
inline constexpr int ROCKET_TRAIL_SMOKE_BLOB_MAX_COUNT = 7;
inline constexpr float ROCKET_TRAIL_SMOKE_BLOB_OFFSET_FACTOR = 0.84f;
inline constexpr float ROCKET_TRAIL_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.42f;
inline constexpr float ROCKET_TRAIL_SMOKE_BLOB_RADIUS_MAX_FACTOR = 0.94f;
inline constexpr float ROCKET_TRAIL_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.035f;
inline constexpr float ROCKET_TRAIL_SMOKE_WOBBLE_FREQUENCY_HZ = 1.2f;

inline constexpr int NUCLEAR_EXPLOSION_RADIUS_CELLS = 3;
inline constexpr Uint32 NUCLEAR_EXPLOSION_FIREBALL_BURN_MS = 1450;
inline constexpr Uint32 NUCLEAR_EXPLOSION_FIREBALL_COLLAPSE_MS = 520;
inline constexpr Uint32 NUCLEAR_EXPLOSION_FIREBALL_DURATION_MS =
    NUCLEAR_EXPLOSION_FIREBALL_BURN_MS +
    NUCLEAR_EXPLOSION_FIREBALL_COLLAPSE_MS;
inline constexpr Uint32 NUCLEAR_EXPLOSION_TOTAL_DURATION_MS = 7200;
inline constexpr Uint32 NUCLEAR_SMOKE_RING_EXPANSION_MS = 1;
inline constexpr Uint32 NUCLEAR_SMOKE_RING_SPAWN_INTERVAL_MS = 52;
inline constexpr int NUCLEAR_SMOKE_RING_PUFFS_PER_WAVE = 72;

inline constexpr Uint32 NUCLEAR_FLASH_DURATION_MS = 950;
inline constexpr Uint8 NUCLEAR_FLASH_PEAK_ALPHA = 230;
inline constexpr SDL_Color NUCLEAR_FLASH_COLOR{145, 145, 240, 180};
inline constexpr float NUCLEAR_FIREBALL_START_RADIUS_CELLS =
    static_cast<float>(NUCLEAR_EXPLOSION_RADIUS_CELLS);
inline constexpr float NUCLEAR_FIREBALL_PEAK_RADIUS_CELLS = 4.8f;
inline constexpr float NUCLEAR_CRATER_RADIUS_CELLS =
    NUCLEAR_FIREBALL_PEAK_RADIUS_CELLS * 0.63f;
inline constexpr float NUCLEAR_CRATER_Y_RADIUS_FACTOR = 0.78f;
inline constexpr int NUCLEAR_CRATER_EDGE_SAMPLE_STEPS = 5;
inline constexpr Uint8 NUCLEAR_CRATER_BASE_ALPHA = 160;
inline constexpr SDL_Color NUCLEAR_CRATER_OUTER_COLOR{54, 43, 36, 255};
inline constexpr SDL_Color NUCLEAR_CRATER_INNER_COLOR{16, 14, 13, 255};
inline constexpr SDL_Color NUCLEAR_CRATER_GLOW_COLOR{118, 70, 38, 255};
inline constexpr Uint32 NUCLEAR_CRATER_REVEAL_DELAY_MS = 120;
inline constexpr SDL_Color NUCLEAR_CRATER_GREEN_CLOUD_COLOR{86, 220, 92, 255};
inline constexpr Uint8 NUCLEAR_CRATER_GREEN_CLOUD_PEAK_ALPHA = 24;
inline constexpr float NUCLEAR_CRATER_GREEN_CLOUD_RADIUS_FACTOR = 1.35f;
inline constexpr Uint32 NUCLEAR_CRATER_GREEN_CLOUD_FADE_IN_MS = 350;
// Total lifetime of the green crater cloud (visible + fade-out). Adjust
// here to change how long the radioactive cloud persists.
inline constexpr Uint32 NUCLEAR_CRATER_GREEN_CLOUD_LIFETIME_MS = 20000;
inline constexpr Uint32 NUCLEAR_CRATER_GREEN_CLOUD_FADE_OUT_MS = 4000;
// Number of blob puffs in the cloud (lower = more compact / clustered).
inline constexpr int NUCLEAR_CRATER_GREEN_CLOUD_BLOB_COUNT = 14;
inline constexpr float NUCLEAR_FIREBALL_TEXTURE_OFFSET_FACTOR = 0.52f;
inline constexpr float NUCLEAR_FIREBALL_TEXTURE_BLOB_MIN_FACTOR = 0.18f;
inline constexpr float NUCLEAR_FIREBALL_TEXTURE_BLOB_MAX_FACTOR = 0.42f;
inline constexpr float NUCLEAR_FIREBALL_WOBBLE_AMPLITUDE_CELLS = 0.045f;
inline constexpr float NUCLEAR_FIREBALL_WOBBLE_FREQUENCY_HZ = 0.8f;
inline constexpr int NUCLEAR_FIREBALL_OUTER_BLOB_COUNT = 4;
inline constexpr int NUCLEAR_FIREBALL_MID_BLOB_COUNT = 20;
inline constexpr int NUCLEAR_FIREBALL_CORE_BLOB_COUNT = 14;
inline constexpr int NUCLEAR_FIREBALL_FLAME_TONGUE_COUNT = 88;
inline constexpr float NUCLEAR_SMOKE_RING_FINAL_RADIUS_CELLS = 18.0f;
inline constexpr float NUCLEAR_SMOKE_RING_THICKNESS_CELLS = 1.05f;
inline constexpr Uint32 NUCLEAR_CENTER_SMOKE_EMISSION_MS = 4600;
inline constexpr Uint32 NUCLEAR_CENTER_SMOKE_SPAWN_INTERVAL_MS = 72;
inline constexpr int NUCLEAR_CENTER_SMOKE_PUFFS_PER_SPAWN = 11;
inline constexpr float NUCLEAR_CENTER_SMOKE_SPREAD_CELLS = 1.45f;
inline constexpr Uint32 NUCLEAR_MUSHROOM_BUILD_MS = 1900;
inline constexpr Uint32 NUCLEAR_MUSHROOM_SPAWN_INTERVAL_MS = 68;
inline constexpr int NUCLEAR_MUSHROOM_STEM_PUFFS_PER_SPAWN = 4;
inline constexpr int NUCLEAR_MUSHROOM_CAP_PUFFS_PER_SPAWN = 9;
inline constexpr float NUCLEAR_MUSHROOM_STEM_HEIGHT_CELLS = 12.4f;
inline constexpr float NUCLEAR_MUSHROOM_CAP_RADIUS_CELLS = 5.2f;
inline constexpr float NUCLEAR_MUSHROOM_CAP_THICKNESS_CELLS = 2.3f;

inline constexpr Uint32 NUCLEAR_RING_SMOKE_LIFETIME_MS = 1500;
inline constexpr float NUCLEAR_RING_SMOKE_INITIAL_RADIUS_CELLS = 0.22f;
inline constexpr float NUCLEAR_RING_SMOKE_FINAL_RADIUS_CELLS = 0.86f;
inline constexpr Uint8 NUCLEAR_RING_SMOKE_INITIAL_ALPHA = 108;
inline constexpr SDL_Color NUCLEAR_RING_SMOKE_COLOR{92, 88, 84, 255};
inline constexpr SDL_Color NUCLEAR_RING_SMOKE_HIGHLIGHT_COLOR{150, 144, 138,
                                                              255};
inline constexpr int NUCLEAR_RING_SMOKE_BLOB_MIN_COUNT = 6;
inline constexpr int NUCLEAR_RING_SMOKE_BLOB_MAX_COUNT = 11;
inline constexpr float NUCLEAR_RING_SMOKE_BLOB_OFFSET_FACTOR = 1.08f;
inline constexpr float NUCLEAR_RING_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.46f;
inline constexpr float NUCLEAR_RING_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.18f;
inline constexpr float NUCLEAR_RING_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.06f;
inline constexpr float NUCLEAR_RING_SMOKE_WOBBLE_FREQUENCY_HZ = 1.15f;
inline constexpr float NUCLEAR_RING_SMOKE_VERTICAL_RISE_CELLS = 0.20f;

inline constexpr Uint32 NUCLEAR_CORE_SMOKE_LIFETIME_MS = 2550;
inline constexpr float NUCLEAR_CORE_SMOKE_INITIAL_RADIUS_CELLS = 0.28f;
inline constexpr float NUCLEAR_CORE_SMOKE_FINAL_RADIUS_CELLS = 1.22f;
inline constexpr Uint8 NUCLEAR_CORE_SMOKE_INITIAL_ALPHA = 138;
inline constexpr SDL_Color NUCLEAR_CORE_SMOKE_COLOR{82, 78, 74, 255};
inline constexpr SDL_Color NUCLEAR_CORE_SMOKE_HIGHLIGHT_COLOR{142, 138, 132,
                                                              255};
inline constexpr int NUCLEAR_CORE_SMOKE_BLOB_MIN_COUNT = 6;
inline constexpr int NUCLEAR_CORE_SMOKE_BLOB_MAX_COUNT = 12;
inline constexpr float NUCLEAR_CORE_SMOKE_BLOB_OFFSET_FACTOR = 1.02f;
inline constexpr float NUCLEAR_CORE_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.48f;
inline constexpr float NUCLEAR_CORE_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.24f;
inline constexpr float NUCLEAR_CORE_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.075f;
inline constexpr float NUCLEAR_CORE_SMOKE_WOBBLE_FREQUENCY_HZ = 0.96f;
inline constexpr float NUCLEAR_CORE_SMOKE_VERTICAL_RISE_CELLS = 1.9f;

inline constexpr Uint32 NUCLEAR_MUSHROOM_SMOKE_LIFETIME_MS = 1850;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_INITIAL_RADIUS_CELLS = 0.26f;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_FINAL_RADIUS_CELLS = 1.48f;
inline constexpr Uint8 NUCLEAR_MUSHROOM_SMOKE_INITIAL_ALPHA = 128;
inline constexpr SDL_Color NUCLEAR_MUSHROOM_SMOKE_COLOR{76, 72, 70, 255};
inline constexpr SDL_Color NUCLEAR_MUSHROOM_SMOKE_HIGHLIGHT_COLOR{
    134, 128, 122, 255};
inline constexpr int NUCLEAR_MUSHROOM_SMOKE_BLOB_MIN_COUNT = 6;
inline constexpr int NUCLEAR_MUSHROOM_SMOKE_BLOB_MAX_COUNT = 13;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_BLOB_OFFSET_FACTOR = 1.16f;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.46f;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.32f;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.09f;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_WOBBLE_FREQUENCY_HZ = 0.84f;
inline constexpr float NUCLEAR_MUSHROOM_SMOKE_VERTICAL_RISE_CELLS = 4.6f;

// === Atomexplosion Variante B (Lichtblitz, Sonnen-Feuerball, abrollender
// Rauchring + Rauchteppich, sich aufdrehende Pilzwolke mit Feuersäule) ===
inline constexpr Uint32 NUCLEAR_B_EXPLOSION_TOTAL_DURATION_MS = 7800;
inline constexpr Uint32 NUCLEAR_B_FLASH_DURATION_MS = 720;
inline constexpr Uint8 NUCLEAR_B_FLASH_PEAK_ALPHA = 245;
inline constexpr SDL_Color NUCLEAR_B_FLASH_COLOR{222, 232, 255, 200};

inline constexpr Uint32 NUCLEAR_B_EXPLOSION_FIREBALL_BURN_MS = 820;
inline constexpr Uint32 NUCLEAR_B_EXPLOSION_FIREBALL_COLLAPSE_MS = 720;
inline constexpr Uint32 NUCLEAR_B_EXPLOSION_FIREBALL_DURATION_MS =
    NUCLEAR_B_EXPLOSION_FIREBALL_BURN_MS +
    NUCLEAR_B_EXPLOSION_FIREBALL_COLLAPSE_MS;
inline constexpr float NUCLEAR_B_FIREBALL_START_RADIUS_CELLS = 1.4f;
inline constexpr float NUCLEAR_B_FIREBALL_PEAK_RADIUS_CELLS = 6.6f;
inline constexpr float NUCLEAR_B_FIREBALL_TEXTURE_OFFSET_FACTOR = 0.55f;
inline constexpr float NUCLEAR_B_FIREBALL_TEXTURE_BLOB_MIN_FACTOR = 0.16f;
inline constexpr float NUCLEAR_B_FIREBALL_TEXTURE_BLOB_MAX_FACTOR = 0.46f;
inline constexpr float NUCLEAR_B_FIREBALL_WOBBLE_AMPLITUDE_CELLS = 0.075f;
inline constexpr float NUCLEAR_B_FIREBALL_WOBBLE_FREQUENCY_HZ = 1.6f;
inline constexpr int NUCLEAR_B_FIREBALL_OUTER_BLOB_COUNT = 12;
inline constexpr int NUCLEAR_B_FIREBALL_MID_BLOB_COUNT = 26;
inline constexpr int NUCLEAR_B_FIREBALL_CORE_BLOB_COUNT = 18;
inline constexpr int NUCLEAR_B_FIREBALL_FLAME_TONGUE_COUNT = 132;

// Schnell abrollender Rauchring
inline constexpr Uint32 NUCLEAR_B_SMOKE_RING_EXPANSION_MS = 1300;
inline constexpr Uint32 NUCLEAR_B_SMOKE_RING_SPAWN_INTERVAL_MS = 32;
inline constexpr int NUCLEAR_B_SMOKE_RING_PUFFS_PER_WAVE = 56;
inline constexpr float NUCLEAR_B_SMOKE_RING_FINAL_RADIUS_CELLS = 13.5f;
inline constexpr float NUCLEAR_B_SMOKE_RING_THICKNESS_CELLS = 0.55f;
inline constexpr float NUCLEAR_B_SMOKE_RING_ROLL_SPEED_RAD_PER_SEC = 7.2f;

inline constexpr Uint32 NUCLEAR_B_RING_SMOKE_LIFETIME_MS = 1700;
inline constexpr float NUCLEAR_B_RING_SMOKE_INITIAL_RADIUS_CELLS = 0.34f;
inline constexpr float NUCLEAR_B_RING_SMOKE_FINAL_RADIUS_CELLS = 1.05f;
inline constexpr Uint8 NUCLEAR_B_RING_SMOKE_INITIAL_ALPHA = 178;
inline constexpr SDL_Color NUCLEAR_B_RING_SMOKE_COLOR{96, 90, 86, 255};
inline constexpr SDL_Color NUCLEAR_B_RING_SMOKE_HIGHLIGHT_COLOR{172, 162, 152,
                                                                255};
inline constexpr int NUCLEAR_B_RING_SMOKE_BLOB_MIN_COUNT = 7;
inline constexpr int NUCLEAR_B_RING_SMOKE_BLOB_MAX_COUNT = 12;
inline constexpr float NUCLEAR_B_RING_SMOKE_BLOB_OFFSET_FACTOR = 1.12f;
inline constexpr float NUCLEAR_B_RING_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.42f;
inline constexpr float NUCLEAR_B_RING_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.18f;
inline constexpr float NUCLEAR_B_RING_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.07f;
inline constexpr float NUCLEAR_B_RING_SMOKE_WOBBLE_FREQUENCY_HZ = 1.30f;
inline constexpr float NUCLEAR_B_RING_SMOKE_VERTICAL_RISE_CELLS = 0.18f;

// Rauchteppich, der hinter dem Ring stehen bleibt
inline constexpr Uint32 NUCLEAR_B_TRAIL_SPAWN_INTERVAL_MS = 48;
inline constexpr int NUCLEAR_B_TRAIL_PUFFS_PER_WAVE = 18;
inline constexpr Uint32 NUCLEAR_B_TRAIL_SMOKE_LIFETIME_MS = 3800;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_INITIAL_RADIUS_CELLS = 0.45f;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_FINAL_RADIUS_CELLS = 0.90f;
inline constexpr Uint8 NUCLEAR_B_TRAIL_SMOKE_INITIAL_ALPHA = 96;
inline constexpr SDL_Color NUCLEAR_B_TRAIL_SMOKE_COLOR{82, 78, 74, 255};
inline constexpr SDL_Color NUCLEAR_B_TRAIL_SMOKE_HIGHLIGHT_COLOR{144, 138, 130,
                                                                 255};
inline constexpr int NUCLEAR_B_TRAIL_SMOKE_BLOB_MIN_COUNT = 5;
inline constexpr int NUCLEAR_B_TRAIL_SMOKE_BLOB_MAX_COUNT = 9;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_BLOB_OFFSET_FACTOR = 0.95f;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.42f;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.10f;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.04f;
inline constexpr float NUCLEAR_B_TRAIL_SMOKE_WOBBLE_FREQUENCY_HZ = 0.65f;

// Rauchsäule
inline constexpr Uint32 NUCLEAR_B_STEM_BUILD_MS = 2400;
inline constexpr Uint32 NUCLEAR_B_STEM_SPAWN_INTERVAL_MS = 42;
inline constexpr int NUCLEAR_B_STEM_PUFFS_PER_SPAWN = 4;
inline constexpr float NUCLEAR_B_STEM_HEIGHT_CELLS = 13.5f;
inline constexpr float NUCLEAR_B_STEM_RADIUS_CELLS = 0.78f;
inline constexpr float NUCLEAR_B_STEM_RISE_SPEED_CELLS_PER_SEC = 6.5f;

inline constexpr Uint32 NUCLEAR_B_STEM_SMOKE_LIFETIME_MS = 2400;
inline constexpr float NUCLEAR_B_STEM_SMOKE_INITIAL_RADIUS_CELLS = 0.34f;
inline constexpr float NUCLEAR_B_STEM_SMOKE_FINAL_RADIUS_CELLS = 1.30f;
inline constexpr Uint8 NUCLEAR_B_STEM_SMOKE_INITIAL_ALPHA = 138;
inline constexpr SDL_Color NUCLEAR_B_STEM_SMOKE_COLOR{72, 68, 66, 255};
inline constexpr SDL_Color NUCLEAR_B_STEM_SMOKE_HIGHLIGHT_COLOR{132, 126, 122,
                                                                255};
inline constexpr int NUCLEAR_B_STEM_SMOKE_BLOB_MIN_COUNT = 6;
inline constexpr int NUCLEAR_B_STEM_SMOKE_BLOB_MAX_COUNT = 11;
inline constexpr float NUCLEAR_B_STEM_SMOKE_BLOB_OFFSET_FACTOR = 1.05f;
inline constexpr float NUCLEAR_B_STEM_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.46f;
inline constexpr float NUCLEAR_B_STEM_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.22f;
inline constexpr float NUCLEAR_B_STEM_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.10f;
inline constexpr float NUCLEAR_B_STEM_SMOKE_WOBBLE_FREQUENCY_HZ = 0.85f;
inline constexpr float NUCLEAR_B_STEM_SMOKE_VERTICAL_RISE_CELLS = 1.0f;

// Pilzkappe (Rauchpartikel folgen einer Torus-Bahn:
// vom Säulenkopf nach außen, dann am Außenrand nach unten -> typische Pilzform)
inline constexpr Uint32 NUCLEAR_B_CAP_SPAWN_INTERVAL_MS = 45;
inline constexpr int NUCLEAR_B_CAP_PUFFS_PER_SPAWN = 14;
inline constexpr float NUCLEAR_B_CAP_RADIUS_CELLS = 6.0f;
inline constexpr float NUCLEAR_B_CAP_HEIGHT_OFFSET_CELLS = 1.2f;
inline constexpr float NUCLEAR_B_CAP_CURL_DROP_CELLS = 5.0f;

inline constexpr Uint32 NUCLEAR_B_CAP_SMOKE_LIFETIME_MS = 2700;
inline constexpr float NUCLEAR_B_CAP_SMOKE_INITIAL_RADIUS_CELLS = 0.45f;
inline constexpr float NUCLEAR_B_CAP_SMOKE_FINAL_RADIUS_CELLS = 2.05f;
inline constexpr Uint8 NUCLEAR_B_CAP_SMOKE_INITIAL_ALPHA = 168;
inline constexpr SDL_Color NUCLEAR_B_CAP_SMOKE_COLOR{74, 70, 68, 255};
inline constexpr SDL_Color NUCLEAR_B_CAP_SMOKE_HIGHLIGHT_COLOR{138, 130, 126,
                                                               255};
inline constexpr int NUCLEAR_B_CAP_SMOKE_BLOB_MIN_COUNT = 9;
inline constexpr int NUCLEAR_B_CAP_SMOKE_BLOB_MAX_COUNT = 18;
inline constexpr float NUCLEAR_B_CAP_SMOKE_BLOB_OFFSET_FACTOR = 1.22f;
inline constexpr float NUCLEAR_B_CAP_SMOKE_BLOB_RADIUS_MIN_FACTOR = 0.46f;
inline constexpr float NUCLEAR_B_CAP_SMOKE_BLOB_RADIUS_MAX_FACTOR = 1.34f;
inline constexpr float NUCLEAR_B_CAP_SMOKE_WOBBLE_AMPLITUDE_CELLS = 0.12f;
inline constexpr float NUCLEAR_B_CAP_SMOKE_WOBBLE_FREQUENCY_HZ = 0.95f;
inline constexpr float NUCLEAR_B_CAP_SMOKE_VERTICAL_RISE_CELLS = 0.0f;

// Feuersäule, die hinter dem Säulenrauch durchschimmert
inline constexpr int NUCLEAR_B_FIRE_GLOW_BLOB_COUNT = 42;
inline constexpr float NUCLEAR_B_FIRE_GLOW_RADIUS_CELLS = 0.85f;
inline constexpr SDL_Color NUCLEAR_B_FIRE_GLOW_INNER_COLOR{255, 226, 110, 255};
inline constexpr SDL_Color NUCLEAR_B_FIRE_GLOW_OUTER_COLOR{226, 78, 18, 255};

// Persistenter Feuerball am Sockel der Säule
inline constexpr float NUCLEAR_B_BASE_FIRE_RADIUS_CELLS = 3.6f;
inline constexpr int NUCLEAR_B_BASE_FIRE_BLOB_COUNT = 44;
inline constexpr int NUCLEAR_B_BASE_FIRE_TONGUE_COUNT = 32;
inline constexpr SDL_Color NUCLEAR_B_BASE_FIRE_HALO_COLOR{180, 50, 16, 255};
inline constexpr SDL_Color NUCLEAR_B_BASE_FIRE_OUTER_COLOR{220, 80, 22, 255};
inline constexpr SDL_Color NUCLEAR_B_BASE_FIRE_MID_COLOR{255, 138, 38, 255};
inline constexpr SDL_Color NUCLEAR_B_BASE_FIRE_INNER_COLOR{255, 232, 132, 255};

// Feuer in der unteren Hälfte der Pilzkappe
inline constexpr int NUCLEAR_B_CAP_FIRE_BLOB_COUNT = 32;
inline constexpr float NUCLEAR_B_CAP_FIRE_RADIUS_CELLS = 0.85f;

// Bodenschutt (kleine dunkle Brocken um den Krater)
inline constexpr int NUCLEAR_B_GROUND_DEBRIS_COUNT = 80;
inline constexpr float NUCLEAR_B_GROUND_DEBRIS_INNER_RADIUS_CELLS = 1.4f;
inline constexpr float NUCLEAR_B_GROUND_DEBRIS_OUTER_RADIUS_CELLS = 5.5f;
inline constexpr float NUCLEAR_B_GROUND_DEBRIS_MIN_HALF_SIZE_CELLS = 0.05f;
inline constexpr float NUCLEAR_B_GROUND_DEBRIS_MAX_HALF_SIZE_CELLS = 0.13f;
inline constexpr SDL_Color NUCLEAR_B_GROUND_DEBRIS_DARK_COLOR{72, 54, 46, 255};
inline constexpr SDL_Color NUCLEAR_B_GROUND_DEBRIS_LIGHT_COLOR{152, 120, 96,
                                                                255};

inline constexpr int WALL_RUBBLE_CENTER_MIN_COUNT = 16;
inline constexpr int WALL_RUBBLE_CENTER_MAX_COUNT = 25;
inline constexpr int WALL_RUBBLE_NEIGHBOR_ORTHOGONAL_MIN_COUNT = 3;
inline constexpr int WALL_RUBBLE_NEIGHBOR_ORTHOGONAL_MAX_COUNT = 8;
inline constexpr int WALL_RUBBLE_NEIGHBOR_DIAGONAL_MIN_COUNT = 0;
inline constexpr int WALL_RUBBLE_NEIGHBOR_DIAGONAL_MAX_COUNT = 2;
inline constexpr float WALL_RUBBLE_MIN_HALF_SIZE_CELLS = 0.040f;
inline constexpr float WALL_RUBBLE_MAX_HALF_SIZE_CELLS = 0.10f;
inline constexpr float WALL_RUBBLE_MIN_ASPECT_RATIO = 0.55f;
inline constexpr float WALL_RUBBLE_MAX_ASPECT_RATIO = 1.75f;
inline constexpr Uint8 WALL_RUBBLE_BASE_ALPHA = 224;
inline constexpr SDL_Color WALL_RUBBLE_DARK_COLOR{84, 62, 54, 255};
inline constexpr SDL_Color WALL_RUBBLE_LIGHT_COLOR{164, 132, 110, 255};

inline constexpr double WALL_MOSS_SURFACE_CHANCE = 0.79;
inline constexpr int WALL_MOSS_PATCH_MIN_COUNT = 1;
inline constexpr int WALL_MOSS_PATCH_MAX_COUNT = 4;
inline constexpr int WALL_MOSS_BLOB_MIN_COUNT = 4;
inline constexpr int WALL_MOSS_BLOB_MAX_COUNT = 7;
inline constexpr float WALL_MOSS_PATCH_RADIUS_MIN_FACTOR = 0.045f;
inline constexpr float WALL_MOSS_PATCH_RADIUS_MAX_FACTOR = 0.15f;
inline constexpr Uint8 WALL_MOSS_BASE_ALPHA = 138;
inline constexpr SDL_Color WALL_MOSS_DARK_COLOR{42, 76, 34, 255};
inline constexpr SDL_Color WALL_MOSS_MID_COLOR{70, 126, 54, 255};
inline constexpr SDL_Color WALL_MOSS_HIGHLIGHT_COLOR{132, 188, 92, 255};

inline constexpr double WALL_VINE_SURFACE_CHANCE = 0.34;
inline constexpr int WALL_VINE_MIN_COUNT = 1;
inline constexpr int WALL_VINE_MAX_COUNT = 2;
inline constexpr int WALL_VINE_SEGMENT_MIN_COUNT = 6;
inline constexpr int WALL_VINE_SEGMENT_MAX_COUNT = 11;
inline constexpr float WALL_VINE_THICKNESS_MIN_FACTOR = 0.014f;
inline constexpr float WALL_VINE_THICKNESS_MAX_FACTOR = 0.034f;
inline constexpr float WALL_VINE_DRIFT_FACTOR = 0.14f;
inline constexpr float WALL_VINE_STEP_MIN_FACTOR = 0.09f;
inline constexpr float WALL_VINE_STEP_MAX_FACTOR = 0.15f;
inline constexpr double WALL_VINE_LEAF_CHANCE = 0.34;
inline constexpr double WALL_VINE_BRANCH_CHANCE = 0.45;
inline constexpr int WALL_VINE_BRANCH_MIN_SEGMENTS = 2;
inline constexpr int WALL_VINE_BRANCH_MAX_SEGMENTS = 4;
inline constexpr float WALL_VINE_BRANCH_ANGLE_SPREAD = 0.85f;
inline constexpr float WALL_VINE_BRANCH_THICKNESS_FACTOR = 0.72f;
inline constexpr Uint8 WALL_VINE_BASE_ALPHA = 195;
inline constexpr SDL_Color WALL_VINE_DARK_COLOR{40, 82, 36, 255};
inline constexpr SDL_Color WALL_VINE_MID_COLOR{78, 144, 62, 255};
inline constexpr SDL_Color WALL_VINE_HIGHLIGHT_COLOR{146, 204, 104, 255};

inline constexpr Uint32 MONSTER_EXPLOSION_FIREBALL_DURATION_MS = 720;
inline constexpr Uint32 MONSTER_EXPLOSION_FIREBALL_EXPANSION_MS = 170;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_PEAK_RADIUS_CELLS = 1.05f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_LATE_GROWTH = 0.18f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_FADE_START_PROGRESS = 0.45f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_BLOB_OFFSET_FACTOR = 0.55f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_BLOB_RADIUS_MIN_FACTOR = 0.50f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_BLOB_RADIUS_MAX_FACTOR = 0.95f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_WOBBLE_AMPLITUDE_CELLS = 0.07f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_WOBBLE_FREQUENCY_HZ = 5.0f;

inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_HALO_COLOR{255, 96, 32, 255};
inline constexpr float MONSTER_EXPLOSION_FIREBALL_HALO_RADIUS_FACTOR = 1.45f;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_HALO_ALPHA = 90;

inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_OUTER_COLOR{170, 32, 14, 255};
inline constexpr float MONSTER_EXPLOSION_FIREBALL_OUTER_RADIUS_FACTOR = 1.00f;
inline constexpr int MONSTER_EXPLOSION_FIREBALL_OUTER_BLOB_COUNT = 11;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_OUTER_ALPHA = 165;

inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_RED_COLOR{226, 64, 24, 255};
inline constexpr float MONSTER_EXPLOSION_FIREBALL_RED_RADIUS_FACTOR = 0.78f;
inline constexpr int MONSTER_EXPLOSION_FIREBALL_RED_BLOB_COUNT = 10;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_RED_ALPHA = 210;

inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_ORANGE_COLOR{255, 138, 38, 255};
inline constexpr float MONSTER_EXPLOSION_FIREBALL_ORANGE_RADIUS_FACTOR = 0.56f;
inline constexpr int MONSTER_EXPLOSION_FIREBALL_ORANGE_BLOB_COUNT = 9;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_ORANGE_ALPHA = 235;

inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_YELLOW_COLOR{255, 222, 108, 255};
inline constexpr float MONSTER_EXPLOSION_FIREBALL_YELLOW_RADIUS_FACTOR = 0.36f;
inline constexpr int MONSTER_EXPLOSION_FIREBALL_YELLOW_BLOB_COUNT = 7;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_YELLOW_ALPHA = 245;

inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_CORE_COLOR{255, 250, 224, 255};
inline constexpr float MONSTER_EXPLOSION_FIREBALL_CORE_RADIUS_FACTOR = 0.22f;
inline constexpr int MONSTER_EXPLOSION_FIREBALL_CORE_BLOB_COUNT = 5;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_CORE_ALPHA = 255;

inline constexpr int MONSTER_EXPLOSION_FIREBALL_FLARE_COUNT = 14;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_FLARE_INNER_FACTOR = 0.55f;
inline constexpr float MONSTER_EXPLOSION_FIREBALL_FLARE_OUTER_FACTOR = 1.30f;
inline constexpr Uint8 MONSTER_EXPLOSION_FIREBALL_FLARE_ALPHA = 200;
inline constexpr SDL_Color MONSTER_EXPLOSION_FIREBALL_FLARE_COLOR{255, 232, 152, 255};

inline constexpr Uint32 ROCKET_STEP_DURATION_MS = 90;
inline constexpr Uint32 ROCKET_ANIMATION_FRAME_MS = 90;
inline constexpr float ROCKET_FLIGHT_EXTRA_LIFT_FACTOR = 0.12f;
inline constexpr int AIRSTRIKE_BOMB_COUNT = 10;
inline constexpr Uint32 AIRSTRIKE_RADIO_DELAY_MS = 2000;
inline constexpr Uint32 AIRSTRIKE_BOMB_FALL_MS = 320;
inline constexpr Uint32 AIRSTRIKE_PLANE_CELL_TRAVEL_MS = 105;
inline constexpr Uint32 AIRSTRIKE_PLANE_MIN_FLIGHT_MS = 2200;
inline constexpr Uint32 AIRSTRIKE_PROPELLER_FADE_OUT_MS = 650;
inline constexpr Uint32 AIRSTRIKE_EXPLOSION_FRAME_MS = 110;
inline constexpr int AIRSTRIKE_EXPLOSION_FRAME_COUNT = 5;
inline constexpr Uint32 AIRSTRIKE_EXPLOSION_DURATION_MS =
    AIRSTRIKE_EXPLOSION_FRAME_MS * AIRSTRIKE_EXPLOSION_FRAME_COUNT;
inline constexpr int AIRSTRIKE_EXPLOSION_RADIUS_CELLS = 1;
inline constexpr Uint32 PACMAN_INVULNERABLE_MS = 30000;
inline constexpr Uint32 PACMAN_PARALYSIS_MS = 3000;
inline constexpr Uint32 SLIME_OVERLAY_HOLD_MS = 2000;
inline constexpr Uint32 SLIME_OVERLAY_FADE_MS = 1000;
inline constexpr Uint32 SLIME_SPLASH_FRAME_MS = 80;
inline constexpr int SLIME_SPLASH_FRAME_COUNT = 9;
inline constexpr Uint32 SLIME_SPLASH_FADE_MS = 1000;
inline constexpr Uint32 SLIME_SPLASH_TOTAL_DURATION_MS =
    SLIME_SPLASH_FRAME_MS * SLIME_SPLASH_FRAME_COUNT + SLIME_SPLASH_FADE_MS;
inline constexpr Uint32 TELEPORT_ANIMATION_PHASE_MS = 1000;
inline constexpr Uint32 MONSTER_EXPLOSION_EFFECT_DURATION_MS =
    MONSTER_EXPLOSION_DURATION_MS;
inline constexpr Uint32 WALL_IMPACT_EFFECT_DURATION_MS = 180;
inline constexpr Uint32 GOODIE_SPARKLE_MIN_INTERVAL_MS = 3000;
inline constexpr Uint32 GOODIE_SPARKLE_MAX_INTERVAL_MS = 10000;
inline constexpr Uint32 GOODIE_SPARKLE_DURATION_MS = 420;
inline constexpr double PLASTIC_EXPLOSIVE_MONSTER_HIT_RADIUS_CELLS = 0.72;
inline constexpr float AIRSTRIKE_PLANE_MARGIN_CELLS = 2.0f;
inline constexpr float AIRSTRIKE_PATH_SAMPLES_PER_CELL = 14.0f;

/**
 * @brief Difficulty preset values used to derive enemy behavior.
 *
 * `monster_speed_offset_from_base` is added to `SPEED_MONSTER` and clamped to
 * at least 1 at runtime. Positive spawn interval scales make extra pickups
 * rarer; values below 1.0 make them appear more frequently.
 */
struct DifficultyTuningPreset {
  int monster_speed_offset_from_base;
  double monster_step_delay_scale;
  Uint32 fireball_cooldown_ms;
  Uint32 fireball_step_duration_ms;
  Uint32 pickup_visible_ms;
  double extra_spawn_interval_scale;
};

inline constexpr DifficultyTuningPreset TRAINING_DIFFICULTY_TUNING{
    -4, 1.5, 15000, 330, 60000, 0.75};
inline constexpr DifficultyTuningPreset EASY_DIFFICULTY_TUNING{
    -3, 1.0, 15000, 220, 60000, 0.75};
inline constexpr DifficultyTuningPreset MEDIUM_DIFFICULTY_TUNING{
    -2, 1.0, 10000, 160, 40000, 1.0};
inline constexpr DifficultyTuningPreset HARD_DIFFICULTY_TUNING{
    0, 1.0, 6500, 120, 20000, 1.4};

/**
 * @brief Rendering palette used for HUD, menus, and effects.
 *
 * All channel values are 0..255. Changing these constants lets the whole UI
 * be recolored without touching renderer logic.
 */
inline constexpr SDL_Color HUD_TEXT_COLOR{243, 236, 222, 255};
inline constexpr SDL_Color MENU_TEXT_COLOR{225, 223, 218, 255};
inline constexpr SDL_Color SELECTED_MENU_TEXT_COLOR{255, 248, 238, 255};
inline constexpr SDL_Color STATUS_TEXT_COLOR{255, 189, 163, 255};
inline constexpr SDL_Color WARNING_TEXT_COLOR{255, 110, 110, 255};
inline constexpr SDL_Color PANEL_FILL_COLOR{10, 6, 18, 205};
inline constexpr SDL_Color PANEL_BORDER_COLOR{196, 130, 92, 255};
inline constexpr SDL_Color START_MENU_PANEL_FILL_COLOR{10, 6, 18, 118};
inline constexpr SDL_Color START_MENU_PANEL_BORDER_COLOR{196, 130, 92, 180};
inline constexpr SDL_Color BRICK_OUTLINE_COLOR{78, 20, 14, 255};
inline constexpr SDL_Color SHIELD_TEXT_COLOR{116, 220, 255, 255};
inline constexpr SDL_Color START_LOGO_HIGHLIGHT_COLOR{238, 252, 255, 255};
inline constexpr SDL_Color START_LOGO_LIGHT_COLOR{88, 234, 255, 255};
inline constexpr SDL_Color START_LOGO_MID_COLOR{44, 156, 255, 255};
inline constexpr SDL_Color START_LOGO_DEEP_COLOR{18, 70, 222, 255};
inline constexpr SDL_Color START_LOGO_OUTLINE_COLOR{6, 18, 102, 255};
inline constexpr SDL_Color TELEPORTER_BLUE_COLOR{78, 160, 255, 255};
inline constexpr SDL_Color TELEPORTER_RED_COLOR{255, 104, 104, 255};
inline constexpr SDL_Color TELEPORTER_GREEN_COLOR{110, 205, 118, 255};
inline constexpr SDL_Color TELEPORTER_GREY_COLOR{172, 176, 186, 255};
inline constexpr SDL_Color TELEPORTER_YELLOW_COLOR{244, 214, 88, 255};

/**
 * @brief Rendering animation and sprite-layout tuning.
 *
 * Frame counts must match the shipped sprite sheets. Scale values are
 * multiplicative factors relative to one map cell. Alpha values control the
 * baseline opacity of overlays and particle sprites. The floor and shadow
 * settings below tune the visual depth of the tilted 3D gameplay view.
 */
inline constexpr int PACMAN_FRAMES_PER_DIRECTION = 4;
inline constexpr int MONSTER_FRAMES_PER_DIRECTION = 4;
inline constexpr int ROCKET_FLIGHT_FRAME_COUNT = 2;
inline constexpr int FART_CLOUD_FRAME_COUNT = 4;
inline constexpr int SLIME_SPLASH_GRID_COLUMNS = 3;
inline constexpr int SLIME_SPLASH_GRID_ROWS = 3;
inline constexpr Uint32 PACMAN_WALK_FRAME_MS = 140;
inline constexpr Uint32 FART_CLOUD_OPEN_DURATION_MS = 320;
inline constexpr Uint32 FART_CLOUD_FRAME_MS =
    FART_CLOUD_OPEN_DURATION_MS / FART_CLOUD_FRAME_COUNT;
inline constexpr Uint8 SLIME_BALL_BASE_ALPHA = 176;
inline constexpr Uint8 SLIME_OVERLAY_BASE_ALPHA = 140;
inline constexpr Uint8 SLIME_SPLASH_BASE_ALPHA = 176;
inline constexpr double MONSTER_RENDER_SCALE = 1.19;
inline constexpr double MONSTER_EXPLOSION_RENDER_SCALE = 1.10;
inline constexpr double MONSTER_EXPLOSION_INITIAL_OPACITY = 0.50;
inline constexpr double MONSTER_EXPLOSION_FINAL_OPACITY = 0.10;
inline constexpr double FART_CLOUD_RENDER_SCALE = 0.70;
inline constexpr double SLIME_BALL_RENDER_SCALE = 0.90;
inline constexpr double SLIME_OVERLAY_RENDER_SCALE = 1.22;
inline constexpr double SLIME_SPLASH_RENDER_SCALE = 1.26;
inline constexpr double FART_CLOUD_DRIFT_X_FACTOR = 0.06;
inline constexpr double FART_CLOUD_DRIFT_Y_FACTOR = 0.15;
inline constexpr double FART_CLOUD_SCALE_PULSE_AMPLITUDE = 0.14;
inline constexpr double ROCKET_SPRITE_ANGLE_OFFSET_DEGREES = 0.0;
inline constexpr SDL_Color FLOOR_3D_NEAR_COLOR{62, 60, 72, 255};
inline constexpr SDL_Color FLOOR_3D_FAR_COLOR{34, 34, 42, 255};
inline constexpr SDL_Color FLOOR_3D_TEXTURE_NEAR_COLOR{228, 226, 220, 255};
inline constexpr SDL_Color FLOOR_3D_TEXTURE_FAR_COLOR{178, 176, 170, 255};
inline constexpr int FLOOR_TEXTURE_SAMPLE_SIZE_PX = 96;
inline constexpr int FLOOR_TEXTURE_SAMPLE_STRIDE_PX = 48;
inline constexpr double FLOOR_3D_TILE_VARIATION_STRENGTH = 0.08;
inline constexpr double FLOOR_3D_WALL_OCCLUSION_STRENGTH = 0.14;
inline constexpr double FLOOR_3D_OPEN_EDGE_HIGHLIGHT_STRENGTH = 0.09;
inline constexpr Uint8 CHARACTER_GROUND_SHADOW_BASE_ALPHA = 95;
inline constexpr double CHARACTER_GROUND_SHADOW_WIDTH_FACTOR = 0.42;
inline constexpr double CHARACTER_GROUND_SHADOW_HEIGHT_FACTOR = 0.15;
inline constexpr double CHARACTER_GROUND_SHADOW_Y_OFFSET_FACTOR = 0.06;
inline constexpr Uint8 WALKIE_TALKIE_ICON_ALPHA = 255;
inline constexpr Uint8 HUD_ICON_ALPHA = 255;

/**
 * @brief Audio mixer and spectrum-analyser configuration.
 *
 * Sample rate and channel count define the PCM format used for synth-generated
 * sounds. Spectrum values tune the retro menu equalizer; the min/max dB window
 * decides how aggressively quiet sounds are suppressed or amplified visually.
 */
inline constexpr int MENU_SPECTRUM_BAND_COUNT = 32;
inline constexpr int AUDIO_SAMPLE_RATE_HZ = 44100;
inline constexpr int AUDIO_OUTPUT_CHANNEL_COUNT = 2;
inline constexpr int AUDIO_MIX_CHUNK_SIZE = 2048;
inline constexpr int AUDIO_ALLOCATED_CHANNEL_COUNT = 32;
inline constexpr int AUDIO_RESERVED_CHANNEL_COUNT = 1;
inline constexpr double MENU_SPECTRUM_MIN_FREQUENCY_HZ = 40.0;
inline constexpr double MENU_SPECTRUM_MAX_FREQUENCY_HZ = 12000.0;
inline constexpr double MENU_SPECTRUM_MIN_DB = -52.0;
inline constexpr double MENU_SPECTRUM_MAX_DB = -8.0;
inline constexpr int MENU_SPECTRUM_MAX_FRAMES = 4096;

/**
 * @brief Map file symbols.
 *
 * These characters define the serialized map format and the keyboard shortcuts
 * used by the editor. Changing them requires existing map files to be updated
 * to the same symbol set.
 */
inline constexpr char BRICK = 'x';
inline constexpr char PATH = '.';
inline constexpr char GOODIE = 'G';
inline constexpr char PACMAN = 'P';
inline constexpr char MONSTER_FEW = 'M';
inline constexpr char MONSTER_MEDIUM = 'N';
inline constexpr char MONSTER_MANY = 'O';
inline constexpr char MONSTER_EXTRA = 'K';
inline constexpr char GOAT = 'z';

/**
 * @brief Tilted 3D gameplay projection.
 *
 * When enabled, the gameplay renderer uses an oblique parallel projection:
 * walls are extruded upward from the map as textured cuboids while entities
 * remain billboarded 2D sprites drawn on top of the tilted floor. The tilt
 * angle rotates the camera backward around the horizontal axis (0 degrees =
 * classic top-down, 90 degrees = side view). The wall-height factor sets
 * the extrusion height as a multiple of one map tile. Menus, HUD, and the
 * map editor continue to render in pure 2D and are unaffected by this flag.
 */
inline constexpr bool ENABLE_3D_VIEW = true;
inline constexpr double VIEW_3D_TILT_DEGREES = 30.0;
inline constexpr double VIEW_3D_WALL_HEIGHT_FACTOR = 1.0;

#endif
