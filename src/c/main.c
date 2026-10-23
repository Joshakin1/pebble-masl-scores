// MASL Scores - watch app
// Shows a scrollable list of MASL games. Press Select to refresh.

#include <pebble.h>

#define MAX_GAMES 30
#define TITLE_LEN 40
#define SUBTITLE_LEN 40

typedef struct {
  char title[TITLE_LEN];
  char subtitle[SUBTITLE_LEN];
} Game;

static Window *s_window;
static MenuLayer *s_menu_layer;
static Game s_games[MAX_GAMES];
static int s_game_count = 0;
static char s_status[48] = "Loading...";

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

// Payload format: "title|subtitle\ntitle|subtitle\n..."
static void parse_games(const char *payload) {
  s_game_count = 0;
  const char *line = payload;
  while (*line && s_game_count < MAX_GAMES) {
    const char *line_end = strchr(line, '\n');
    if (!line_end) line_end = line + strlen(line);

    const char *sep = line;
    while (sep < line_end && *sep != '|') sep++;

    Game *g = &s_games[s_game_count];
    copy_field(g->title, TITLE_LEN, line, sep);
    if (sep < line_end) {
      copy_field(g->subtitle, SUBTITLE_LEN, sep + 1, line_end);
    } else {
      g->subtitle[0] = '\0';
    }
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

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  if (s_game_count == 0) {
    menu_cell_basic_draw(ctx, cell, "MASL Scores", s_status, NULL);
    return;
  }
  Game *g = &s_games[index->row];
  menu_cell_basic_draw(ctx, cell, g->title, g->subtitle, NULL);
}

static void select_click(MenuLayer *menu, MenuIndex *index, void *context) {
  request_refresh();
  vibes_short_pulse();
}

// ---------- Window ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .draw_row = draw_row,
    .select_click = select_click,
  });
#if defined(PBL_COLOR)
  menu_layer_set_highlight_colors(s_menu_layer, GColorDarkGreen, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
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
