// MASL Scores - watch app
// Shows a scrollable list of MASL games and the division standings. Select
// switches between the two screens; a long press of Select refreshes.

#include <pebble.h>

#define MAX_GAMES 30
#define MAX_STANDINGS 16
#define TEAM_LEN 20
#define SCORE_LEN 4
#define STAT_LEN 5
#define SUBTITLE_LEN 40

// Each game row: away team line, home team line, then date/status.
#define ROW_HEIGHT 62
// Each standings row: rank, logo, team and points, then stat columns
// underneath, keyed by the header.
#define STANDINGS_ROW_HEIGHT 36
// Last standings row: "Tiebreakers" title plus the order, wrapping to two
// lines on 144px screens.
#define TIEBREAK_ROW_HEIGHT 48
#define NUM_STAT_COLUMNS 4
// Height of a single title/status row shown before data arrives.
#define EMPTY_ROW_HEIGHT 44
#define LINE_HEIGHT 22
#define SCORE_W 30
#define RANK_W 16
#define PTS_W 26
#define LOGO_W 24
#define LOGO_H 20
// Logos are color only; at this size they don't survive black and white.
#define LOGO_SPACE PBL_IF_COLOR_ELSE(LOGO_W + 4, 0)
#define MARGIN PBL_IF_ROUND_ELSE(20, 4)

// Selected-row color, set from the phone's settings page (0xRRGGBB).
#define PERSIST_KEY_HIGHLIGHT 1
#define DEFAULT_HIGHLIGHT 0x55AAFF  // GColorPictonBlue

typedef struct {
  char away[TEAM_LEN];
  char away_score[SCORE_LEN];
  char home[TEAM_LEN];
  char home_score[SCORE_LEN];
  char subtitle[SUBTITLE_LEN];
} Game;

typedef struct {
  char rank[STAT_LEN];
  char team[TEAM_LEN];
  char games_played[STAT_LEN];
  char wins[STAT_LEN];
  char losses[STAT_LEN];
  char ot_losses[STAT_LEN];
  char points[STAT_LEN];
  char reg_wins[STAT_LEN];
  char diff[STAT_LEN];
} Standing;

typedef struct {
  const char *label;
  int width;
  GTextAlignment align;
} StatColumn;

// Games played and record, then the main tiebreakers after points in order
// (head-to-head is skipped). Columns that don't fit the screen width are
// dropped from the end; all four fit on a 144px screen (114 of 116px).
// W-L-OT gets the most room so its bold label isn't cut off.
static const StatColumn s_stat_columns[NUM_STAT_COLUMNS] = {
  { "GP", 18, GTextAlignmentLeft },
  { "W-L-OT", 52, GTextAlignmentCenter },
  { "RW", 20, GTextAlignmentRight },
  { "GD", 24, GTextAlignmentRight },
};

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

static GColor s_highlight_bg;
static GColor s_highlight_fg;
#endif

static Window *s_window;
static MenuLayer *s_menu_layer;
static Window *s_standings_window;
static MenuLayer *s_standings_menu;  // NULL while the standings window is closed
static GFont s_team_font;
static GFont s_status_font;
static GFont s_header_font;
static Game s_games[MAX_GAMES];
static int s_game_count = 0;
static char s_status[48] = "Loading...";
static Standing s_standings[MAX_STANDINGS];
static int s_standing_count = 0;
static char s_standings_status[48] = "Loading...";

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

static void draw_logo(GContext *ctx, const char *team, int x, int y) {
  GBitmap *logo = find_logo(team);
  if (logo) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, logo, GRect(x, y, LOGO_W, LOGO_H));
  }
}

// ---------- Highlight color ----------

static void style_menu(MenuLayer *menu) {
#if defined(PBL_COLOR)
  if (!menu) return;
  menu_layer_set_highlight_colors(menu, s_highlight_bg, s_highlight_fg);
  layer_mark_dirty(menu_layer_get_layer(menu));
#endif
}

static void set_highlight(int32_t hex) {
#if defined(PBL_COLOR)
  s_highlight_bg = GColorFromHEX(hex);
  // Black text on light colors, white on dark (each channel is 0-3).
  int brightness = 299 * s_highlight_bg.r + 587 * s_highlight_bg.g + 114 * s_highlight_bg.b;
  s_highlight_fg = (brightness > 1500) ? GColorBlack : GColorWhite;
  style_menu(s_menu_layer);
  style_menu(s_standings_menu);
#endif
}

static int32_t load_highlight(void) {
  return persist_exists(PERSIST_KEY_HIGHLIGHT)
      ? persist_read_int(PERSIST_KEY_HIGHLIGHT) : DEFAULT_HIGHLIGHT;
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

static void request_standings(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_GetStandings, 1);
    app_message_outbox_send();
    snprintf(s_standings_status, sizeof(s_standings_status), "Loading...");
    if (s_standing_count == 0 && s_standings_menu) menu_layer_reload_data(s_standings_menu);
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

// Payload format, one team per line: "rank|team|gp|w|l|ot|pts|rw|diff"
static void parse_standings(const char *payload) {
  s_standing_count = 0;
  const char *line = payload;
  while (*line && s_standing_count < MAX_STANDINGS) {
    const char *line_end = strchr(line, '\n');
    if (!line_end) line_end = line + strlen(line);

    Standing *s = &s_standings[s_standing_count];
    const char *p = line;
    p = next_field(p, line_end, s->rank, STAT_LEN);
    p = next_field(p, line_end, s->team, TEAM_LEN);
    p = next_field(p, line_end, s->games_played, STAT_LEN);
    p = next_field(p, line_end, s->wins, STAT_LEN);
    p = next_field(p, line_end, s->losses, STAT_LEN);
    p = next_field(p, line_end, s->ot_losses, STAT_LEN);
    p = next_field(p, line_end, s->points, STAT_LEN);
    p = next_field(p, line_end, s->reg_wins, STAT_LEN);
    next_field(p, line_end, s->diff, STAT_LEN);
    s_standing_count++;

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
  Tuple *standings = dict_find(iter, MESSAGE_KEY_Standings);
  if (standings) {
    parse_standings(standings->value->cstring);
    s_standings_status[0] = '\0';
  }
  Tuple *standings_status = dict_find(iter, MESSAGE_KEY_StandingsStatus);
  if (standings_status) {
    snprintf(s_standings_status, sizeof(s_standings_status), "%s",
             standings_status->value->cstring);
  }
  Tuple *highlight = dict_find(iter, MESSAGE_KEY_HighlightColor);
  if (highlight) {
    persist_write_int(PERSIST_KEY_HIGHLIGHT, highlight->value->int32);
    set_highlight(highlight->value->int32);
  }
  menu_layer_reload_data(s_menu_layer);
  if (s_standings_menu) menu_layer_reload_data(s_standings_menu);
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

// ---------- Standings window ----------

// After the teams, one more row lists the full MASL tiebreak order.
static uint16_t standings_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  return s_standing_count > 0 ? s_standing_count + 1 : 1;
}

static int16_t standings_cell_height(MenuLayer *menu, MenuIndex *index, void *context) {
  if (s_standing_count == 0) return EMPTY_ROW_HEIGHT;
  return index->row < s_standing_count ? STANDINGS_ROW_HEIGHT : TIEBREAK_ROW_HEIGHT;
}

static int16_t standings_header_height(MenuLayer *menu, uint16_t section, void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

// The stat columns under each team, within width w. The header draws its
// labels with the same function so the key lines up with the numbers.
static void draw_stat_columns(GContext *ctx, GFont font, int x, int y, int w, int h,
                              const char *values[NUM_STAT_COLUMNS]) {
  int col_x = x;
  for (int i = 0; i < NUM_STAT_COLUMNS; i++) {
    int col_w = s_stat_columns[i].width;
    if (col_x + col_w > x + w) break;
    graphics_draw_text(ctx, values[i], font, GRect(col_x, y, col_w, h),
                       GTextOverflowModeTrailingEllipsis, s_stat_columns[i].align, NULL);
    col_x += col_w;
  }
}

static void standings_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *context) {
  GRect bounds = layer_get_bounds(cell);
  int x = MARGIN;
  int w = bounds.size.w - 2 * MARGIN;
  int stats_x = x + RANK_W + 4;
  // Keys the stat columns on each row's second line. Points (right side of the
  // first line) aren't labeled.
  const char *labels[NUM_STAT_COLUMNS];
  for (int i = 0; i < NUM_STAT_COLUMNS; i++) labels[i] = s_stat_columns[i].label;
  draw_stat_columns(ctx, s_header_font, stats_x, -2, w - (stats_x - x), bounds.size.h, labels);
}

static void draw_tiebreak_row(GContext *ctx, int x, int w) {
  graphics_draw_text(ctx, "Tiebreakers", s_header_font, GRect(x, -2, w, 16),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "Pts > RW > W > H2H > GD > GF > GA", s_status_font,
                     GRect(x, 12, w, TIEBREAK_ROW_HEIGHT - 12),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void standings_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  if (s_standing_count == 0) {
    menu_cell_basic_draw(ctx, cell, "Standings", s_standings_status, NULL);
    return;
  }
  GRect bounds = layer_get_bounds(cell);
  int x = MARGIN;
  int w = bounds.size.w - 2 * MARGIN;
  if (index->row >= s_standing_count) {
    draw_tiebreak_row(ctx, x, w);
    return;
  }
  Standing *s = &s_standings[index->row];
  int name_x = x + RANK_W + 4;

  // Gothic glyphs sit a few pixels below the top of their box.
  graphics_draw_text(ctx, s->rank, s_team_font, GRect(x, -3, RANK_W, LINE_HEIGHT),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
  draw_logo(ctx, s->team, name_x, 1);
  graphics_draw_text(ctx, s->team, s_team_font,
                     GRect(name_x + LOGO_SPACE, -3, w - (name_x - x) - LOGO_SPACE - PTS_W, LINE_HEIGHT),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, s->points, s_team_font, GRect(x + w - PTS_W, -3, PTS_W, LINE_HEIGHT),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);

  char record[3 * STAT_LEN + 2];
  snprintf(record, sizeof(record), "%s-%s-%s", s->wins, s->losses, s->ot_losses);
  const char *values[NUM_STAT_COLUMNS] = {
    s->games_played, record, s->reg_wins, s->diff,
  };
  // Starts under the logo (not after it) and uses the full width, so more
  // columns fit.
  draw_stat_columns(ctx, s_status_font, name_x, LINE_HEIGHT - 3, w - (name_x - x),
                    STANDINGS_ROW_HEIGHT - LINE_HEIGHT + 3, values);
}

// Select goes back to the schedule; a long press reloads the standings.
static void standings_select_click(MenuLayer *menu, MenuIndex *index, void *context) {
  window_stack_pop(true);
}

static void standings_select_long_click(MenuLayer *menu, MenuIndex *index, void *context) {
  request_standings();
  vibes_short_pulse();
}

static void standings_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_standings_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_standings_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = standings_num_rows,
    .get_cell_height = standings_cell_height,
    .get_header_height = standings_header_height,
    .draw_header = standings_draw_header,
    .draw_row = standings_draw_row,
    .select_click = standings_select_click,
    .select_long_click = standings_select_long_click,
  });
  style_menu(s_standings_menu);
  menu_layer_set_click_config_onto_window(s_standings_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_standings_menu));
  request_standings();
}

static void standings_window_unload(Window *window) {
  menu_layer_destroy(s_standings_menu);
  s_standings_menu = NULL;
}

// ---------- Game list ----------

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  return s_game_count > 0 ? s_game_count : 1;
}

static int16_t get_cell_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return s_game_count > 0 ? ROW_HEIGHT : EMPTY_ROW_HEIGHT;
}

// One team line: [logo] Name ........ score
static void draw_team(GContext *ctx, int x, int y, int w, const char *team, const char *score) {
  draw_logo(ctx, team, x, y + 1);
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

// Select opens the standings; a long press refreshes the schedule.
static void select_click(MenuLayer *menu, MenuIndex *index, void *context) {
  window_stack_push(s_standings_window, true);
}

static void select_long_click(MenuLayer *menu, MenuIndex *index, void *context) {
  request_refresh();
  vibes_short_pulse();
}

// ---------- Window ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_team_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_status_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_header_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  load_logos();

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .get_cell_height = get_cell_height,
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
  });
  set_highlight(load_highlight());
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  unload_logos();
}

static void init(void) {
  s_standings_window = window_create();
  window_set_window_handlers(s_standings_window, (WindowHandlers) {
    .load = standings_window_load,
    .unload = standings_window_unload,
  });

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
  window_destroy(s_standings_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
