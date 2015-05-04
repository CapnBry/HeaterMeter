#include "pebble.h"

#define NUM_PROBES 4
#define PROBE_NAME_LEN 12
#define PROBE_TEMP_LEN 7
#define IMSG_BUFFER_SIZE (1+NUM_PROBES*(7+7+PROBE_NAME_LEN+1+PROBE_TEMP_LEN+1))
#define OMSG_BUFFER_SIZE 32

static Window *s_main_window;
static TextLayer *s_temperature_layer[NUM_PROBES];
static TextLayer *s_name_layer[NUM_PROBES];
static TextLayer *s_errmsg;
static TextLayer *s_bluetooth_layer;

static AppTimer *s_refresh_timer;

enum AppSyncKeys {
  KEY_NAME_0 = 0,     // TUPLE_CSTRING
  KEY_TEMP_0 = 1,     // TUPLE_CSTRING
  KEY_NAME_1 = 2,     // TUPLE_CSTRING
  KEY_TEMP_1 = 3,     // TUPLE_CSTRING
  KEY_NAME_2 = 4,     // TUPLE_CSTRING
  KEY_TEMP_2 = 5,     // TUPLE_CSTRING
  KEY_NAME_3 = 6,     // TUPLE_CSTRING
  KEY_TEMP_3 = 7,     // TUPLE_CSTRING
  KEY_PCOMMAND = 8,   // TUPLE_UINT8
  KEY_ERRMSG = 9,    // TUPLE_CSTRING
  KEY_REFRESH_INTERVAL = 10, // TUPLE_UINT16
};

enum AppPCommands {
  PCMD_REFRESH = 0x00,
};

static char g_ProbeNames[NUM_PROBES][PROBE_NAME_LEN+1];
static char g_ProbeTemps[NUM_PROBES][PROBE_TEMP_LEN+1];
static char g_ErrMsg[32];
static uint16_t g_RefreshInterval;

// forwards
static void requestRefresh(void);
static void startRefreshTimer(void);

static void stopRefreshTimer(void) {
  if (s_refresh_timer) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "app_timer_cancel=%p", s_refresh_timer);
    app_timer_cancel(s_refresh_timer);
    s_refresh_timer = NULL;
  }
}

static void refresh_callback(void *data) {
  requestRefresh();
  startRefreshTimer();
}

static void startRefreshTimer(void) {
  uint16_t interval = g_RefreshInterval < 5 ? 5 : g_RefreshInterval;
  s_refresh_timer = app_timer_register(interval * 1000UL, &refresh_callback, NULL);
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "app_timer_register=%p", s_refresh_timer);
}

static void clearErrMsg(void) {
  if (g_ErrMsg[0]) {
    g_ErrMsg [0]= 0;
    layer_set_hidden(text_layer_get_layer(s_errmsg), true);
  }
}

static void setErrMsg(char const * const msg) {
  strcpy(g_ErrMsg, msg);
  text_layer_set_text(s_errmsg, g_ErrMsg);
  layer_set_hidden(text_layer_get_layer(s_errmsg), false);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox_received_callback");
  //stopRefreshTimer();
  
  Tuple *t = dict_read_first(iterator);
  while(t != NULL) {
    uint8_t probe = t->key/2;
    switch (t->key) {
        case KEY_NAME_0:
        case KEY_NAME_1:
        case KEY_NAME_2:
        case KEY_NAME_3:
          strcpy(g_ProbeNames[probe], t->value->cstring);
          text_layer_set_text(s_name_layer[probe], g_ProbeNames[probe]);
          clearErrMsg();
          break;

        case KEY_TEMP_0:
        case KEY_TEMP_1:
        case KEY_TEMP_2:
        case KEY_TEMP_3:
          // if blank string (\0 only) then that probe is "off"
          if (t->length == 1)
            strcpy(g_ProbeTemps[probe], "off");
          else
          {
            strcpy(g_ProbeTemps[probe], t->value->cstring);
            g_ProbeTemps[probe][t->length-1] = '\xc2';
            g_ProbeTemps[probe][t->length] = '\xb0';
            g_ProbeTemps[probe][t->length+1] = '\0';
            //uint8_t i;
            //for (i=0; i<t->length; ++i)
            //  APP_LOG(APP_LOG_LEVEL_DEBUG, "%u", (uint8_t)t->value->cstring [i]);
          }
          text_layer_set_text(s_temperature_layer[probe], g_ProbeTemps[probe]);
          clearErrMsg();
          break;
          
        case KEY_ERRMSG:
          setErrMsg(t->value->cstring);
          break;
          
        case KEY_REFRESH_INTERVAL:
          stopRefreshTimer();
          g_RefreshInterval = t->value->int32;
          startRefreshTimer();
          break;
    }

    t = dict_read_next(iterator);
  }

  //startRefreshTimer();
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "AppMessage Sync Error: %d", reason);
}

static void bluetooth_connection_callback(bool connected) {
  layer_set_hidden(text_layer_get_layer(s_bluetooth_layer), connected);
}

static void sendPCommand(uint8_t cmd) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, KEY_PCOMMAND, cmd);
  app_message_outbox_send();
}

static void requestRefresh(void) {
  sendPCommand(PCMD_REFRESH);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
    
  for (uint8_t i=0; i<NUM_PROBES; ++i) {
    s_temperature_layer[i] = text_layer_create(GRect(0, 0+(36*i), bounds.size.w, 36));
    text_layer_set_text_color(s_temperature_layer[i], i == 0 ? GColorBlack : GColorWhite);
    text_layer_set_background_color(s_temperature_layer[i], i == 0 ? GColorWhite : GColorClear);
    text_layer_set_font(s_temperature_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text_alignment(s_temperature_layer[i], GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(s_temperature_layer[i]));

    s_name_layer[i] = text_layer_create(GRect(0, 7+(36*i), bounds.size.w, 36));
    text_layer_set_text_color(s_name_layer[i], i == 0 ? GColorBlack : GColorWhite);
    text_layer_set_background_color(s_name_layer[i], GColorClear);
    text_layer_set_font(s_name_layer[i], fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_name_layer[i], GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(s_name_layer[i]));
  }

  // Create the errmsg last so it ends up on top of the other children
  s_errmsg = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  text_layer_set_text_color(s_errmsg, GColorWhite);
  text_layer_set_background_color(s_errmsg, GColorBlack);
  text_layer_set_font(s_errmsg, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_errmsg, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_errmsg, GTextOverflowModeWordWrap);
  setErrMsg("Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_errmsg));
  
  // created offscreen then scrolled in
  s_bluetooth_layer = text_layer_create(GRect(0, bounds.size.h-14, bounds.size.w, 14));
  text_layer_set_text_color(s_bluetooth_layer, GColorWhite);
  text_layer_set_background_color(s_bluetooth_layer, GColorBlack);
  text_layer_set_font(s_bluetooth_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_bluetooth_layer, GTextAlignmentCenter);
  text_layer_set_text(s_bluetooth_layer, "Bluetooth Disconnected");
  layer_add_child(window_layer, text_layer_get_layer(s_bluetooth_layer));

  bluetooth_connection_service_subscribe(bluetooth_connection_callback);
  bluetooth_connection_callback(bluetooth_connection_service_peek());
}

static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  //Window *window = (Window *)context;
  stopRefreshTimer();
  requestRefresh();
  startRefreshTimer();
}

static void window_unload(Window *window) {
  bluetooth_connection_service_unsubscribe();
  stopRefreshTimer();

  for (uint8_t i=0; i<NUM_PROBES; ++i) {
    text_layer_destroy(s_name_layer[i]);
    text_layer_destroy(s_temperature_layer[i]);
  }
  text_layer_destroy(s_errmsg);
  text_layer_destroy(s_bluetooth_layer); 
}

static void config_provider(Window *window) {
  // single click / repeat-on-hold config:
  window_single_click_subscribe(BUTTON_ID_SELECT, down_single_click_handler);
}

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_fullscreen(s_main_window, false);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_main_window, true);
  window_set_click_config_provider(s_main_window, (ClickConfigProvider)config_provider);

  app_message_open(IMSG_BUFFER_SIZE, OMSG_BUFFER_SIZE);
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
}

static void deinit(void) {
  window_destroy(s_main_window);
  //app_sync_deinit(&s_sync);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
