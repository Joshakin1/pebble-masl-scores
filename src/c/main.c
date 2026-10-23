// MASL Scores - watch app
// Shows a scrollable list of MASL games. Press Select to refresh.

#include <pebble.h>

#define MAX_GAMES 30
#define TEAM_LEN 20
#define SCORE_LEN 4
#define SUBTITLE_LEN 40

// Each row: away team line, home team line, then date/status.
#define ROW_HEIGHT 62
// Height of the single "MASL Scores / status" row shown before games arrive.
#define EMPTY_ROW_HEIGHT 44
#define LINE_HEIGHT 22
#define SCORE_W 30
#define LOGO_W 24
#define LOGO_H 20
// Logos are color only; at this size they don't survive black and white.
#define LOGO_SPACE PBL_IF_COLOR_ELSE(LOGO_W + 4, 0)
#define MARGIN PBL_IF_ROUND_ELSE(20, 4)

typedef struct {
  char away[TEAM_LEN];
  char away_score[SCORE_LEN];
  char home[TEAM_LEN];
  char home_score[SCORE_LEN];
  char subtitle[SUBTITLE_LEN];
} Game;

#if defined(PBL_COLOR)
typedef struct {
  const char *name;  // short team name as it appears in the ticker
  uint32_t resource_id;
  GBitmap *bitmap;
} TeamLogo;

static TeamLogo s_logos[] = {
  { "Ambush", RESOURCE_ID_LOGO_AMBUSH, NULL },
  { "Blast", RESOURCE_ID_LOGO_BLAST, NULL },
  { "Comets", RESOURCE_ID_LOGO_COMETS, NULL },
  { "Sockers", RESOURCE_ID_LOGO_SOCKERS, NULL },
  { "Spirits", RESOURCE_ID_LOGO_SPIRITS, NULL },
  { "Stars", RESOURCE_ID_LOGO_STARS, NULL },
  { "Strykers", RESOURCE_ID_LOGO_STRYKERS, NULL },
  { "UCFC", RESOURCE_ID_LOGO_UCFC, NULL },
  { "Wave", RESOURCE_ID_LOGO_WAVE, NULL },
};
#define NUM_LOGOS ARRAY_LENGTH(s_logos)
#endif

static Window *s_window;
static MenuLayer *s_menu_layer;
static GFont s_team_font;
static GFont s_status_font;
static Game s_games[MAX_GAMES];
static int s_game_count = 0;
static char s_status[48] = "Loading...";

// ---------- Logos ----------

static void load_logos(void) {
#if defined(PBL_COLOR)
  for (size_t i = 0; i < NUM_LOGOS; i++) {
    s_logos[i].bitmap = gbitmap_create_with_resource(s_logos[i].resource_id);
  }
#endif
}

static void unload_logos(void) {
#if defined(PBL_COLOR)
  for (size_t i = 0; i < NUM_LOGOS; i++) {
    gbitmap_destroy(s_logos[i].bitmap);
    s_logos[i].bitmap = NULL;
  }
#endif
}

// Returns NULL for unknown teams and on black-and-white watches.
static GBitmap *find_logo(const char *team) {
#if defined(PBL_COLOR)
  for (size_t i = 0; i < NUM_LOGOS; i++) {
    if (strcmp(s_logos[i].name, team) == 0) return s_logos[i].bitmap;
  }
#endif
  return NULL;
}

// ---------- Messaging ----------

static void request_refresh(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_Refresh, 1);
    app_message_outbox_send();
    snprintf(s_status, sizeof(s_status), "Refreshing...");
    if (s_game_count == 0) menu_layer_reload_data(s_menu_layer);
  }
}

// Copy up to (max - 1) chars of src[start..end) into dest.
static void copy_field(char *dest, size_t max, const char *start, const char *end) {
  size_t len = (size_t)(end - start);
  if (len >= max) len = max - 1;
  memcpy(dest, start, len);
  dest[len] = '\0';
}

// Copy the field starting at p (up to the next '|' or end) into dest and
// return the start of the following field.
static const char *next_field(const char *p, const char *end, char *dest, size_t max) {
  const char *sep = p;
  while (sep < end && *sep != '|') sep++;
  copy_field(dest, max, p, sep);
  return (sep < end) ? sep + 1 : end;
}

// Payload format, one game per line: "away|awayScore|home|homeScore|subtitle"
static void parse_games(const char *payload) {
  s_game_count = 0;
  const char *line = payload;
  while (*line && s_game_count < MAX_GAMES) {
    const char *line_end = strchr(line, '\n');
    if (!line_end) line_end = line + strlen(line);

    Game *g = &s_games[s_game_count];
    const char *p = line;
    p = next_field(p, line_end, g->away, TEAM_LEN);
    p = next_field(p, line_end, g->away_score, SCORE_LEN);
    p = next_field(p, line_end, g->home, TEAM_LEN);
    p = next_field(p, line_end, g->home_score, SCORE_LEN);
    next_field(p, line_end, g->subtitle, SUBTITLE_LEN);
    s_game_count++;

    line = (*line_end) ? line_end + 1 : line_end;
  }
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *games = dict_find(iter, MESSAGE_KEY_Games);
  if (games) {
    parse_games(games->value->cstring);
    s_status[0] = '\0';
  }
  Tuple *status = dict_find(iter, MESSAGE_KEY_Status);
  if (status) {
    snprintf(s_status, sizeof(s_status), "%s", status->value->cstring);
  }
  menu_layer_reload_data(s_menu_layer);
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

// ---------- Menu ----------

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  return s_game_count > 0 ? s_game_count : 1;
}

static int16_t get_cell_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return s_game_count > 0 ? ROW_HEIGHT : EMPTY_ROW_HEIGHT;
}

// One team line: [logo] Name ........ score
static void draw_team(GContext *ctx, int x, int y, int w, const char *team, const char *score) {
  GBitmap *logo = find_logo(team);
  if (logo) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, logo, GRect(x, y + 1, LOGO_W, LOGO_H));
  }
  // Gothic glyphs sit a few pixels below the top of their box.
  graphics_draw_text(ctx, team, s_team_font,
                     GRect(x + LOGO_SPACE, y - 3, w - LOGO_SPACE - SCORE_W, LINE_HEIGHT),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, score, s_team_font,
                     GRect(x + w - SCORE_W, y - 3, SCORE_W, LINE_HEIGHT),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  if (s_game_count == 0) {
    menu_cell_basic_draw(ctx, cell, "MASL Scores", s_status, NULL);
    return;
  }
  Game *g = &s_games[index->row];
  GRect bounds = layer_get_bounds(cell);
  int x = MARGIN;
  int w = bounds.size.w - 2 * MARGIN;

  draw_team(ctx, x, 0, w, g->away, g->away_score);
  draw_team(ctx, x, LINE_HEIGHT, w, g->home, g->home_score);
  graphics_draw_text(ctx, g->subtitle, s_status_font,
                     GRect(x, 2 * LINE_HEIGHT - 2, w, ROW_HEIGHT - 2 * LINE_HEIGHT),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void select_click(MenuLayer *menu, MenuIndex *index, void *context) {
  request_refresh();
  vibes_short_pulse();
}

// ---------- Window ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_team_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_status_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  load_logos();

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
  });
#if defined(PBL_COLOR)
  // Light enough that dark logos (e.g. Ambush) stay visible when selected.
  menu_layer_set_highlight_colors(s_menu_layer, GColorPictonBlue, GColorBlack);
#endif
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  unload_logos();
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(4096, 64);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
