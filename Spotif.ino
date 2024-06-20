#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <time.h>
#include <vector>
#include "spotify_api.h"

#ifndef WIFI_SSID
#define WIFI_SSID "Menahem"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "203788583"
#endif

// Spotify API credentials
static String spotifyClientId = "CLIENT_ID";
static String spotifyClientSecret = "CLIENT_SECRET";
static String accessToken = "ACCESS_TOKEN";
static String refreshToken = "REFRESH_TOKEN";

const char *spotifyBaseApiUrl = "https://api.spotify.com/v1";

const uint32_t C_SPOTIFY_GREEN = 0x1DB954;
const uint32_t C_SPOTIFY_BLACK = 0x000000;
const uint32_t C_WHITE = 0xFFFFFF;

// Variables to store current track information
int currentTrackDuration = 0;
int currentTrackProgress = 0;
bool isPlaying = false;
int httpErrorCount = 0;
const int maxErrorCount = 3;
String currentTrackImageUrl = "";
static unsigned long lastActivityTime = millis();

// LVGL objects for the display
lv_obj_t *labelTrack = nullptr;
lv_obj_t *labelArtist = nullptr;
lv_obj_t *barProgress = nullptr;
lv_obj_t *labelCurrentTime = nullptr;
lv_obj_t *labelTotalTime = nullptr;
lv_obj_t *btnContainer = nullptr;
lv_obj_t *btnPlayPause = nullptr;
lv_obj_t *btnNext = nullptr;
lv_obj_t *btnPrevious = nullptr;
lv_obj_t *btnVolumeDown = nullptr;
lv_obj_t *btnVolumeUp = nullptr;
lv_obj_t *labelNowPlaying = nullptr;
lv_obj_t *screen1 = nullptr;
lv_obj_t *screen2 = nullptr;
lv_obj_t *labelScreen2 = nullptr;
lv_obj_t *deviceList = nullptr;
lv_obj_t *splashScreen = nullptr;
lv_obj_t *imgTrack = nullptr;
lv_obj_t *labelUsername = nullptr;
lv_obj_t *labelBrowse = nullptr;
lv_obj_t *btnSettings = nullptr;
lv_obj_t *labelLoadStep = nullptr;  // New label for load step
lv_obj_t *browseScreen = nullptr;

// Task handles for updating track information and displaying the current track
TaskHandle_t updateTrackInfoTaskHandle = NULL;
TaskHandle_t fetchTrackImageTaskHandle = NULL;
TaskHandle_t fetchPlaylistImageTaskHandle = NULL;

// Create an instance of the LilyGo_Class for the AMOLED display
LilyGo_Class amoled;

// Create an instance of Spotify API
SpotifyApi apiClient(spotifyClientId, spotifyClientSecret, accessToken, refreshToken);

// Function declarations
void createPlayingNowScreen();
void createSettingsScreen();
void createBrowseScreen();
void WiFiEvent(WiFiEvent_t event);
void updateTrackInfo(void *parameter);
void fetchTrackImage(void *parameter);
void fetchPlaylistImage(void *parameter);
void connectToWiFi();
void checkAndReconnectWiFi();
void dimScreen();
void wakeScreen();

// Helper function to create buttons
lv_obj_t *createButton(lv_obj_t *parent, const char *symbol, lv_event_cb_t event_cb, int width = 90, int height = 60, lv_color_t bg_color = lv_color_hex(0xFFFFFF)) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);
  lv_obj_set_style_bg_color(btn, bg_color, 0);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, symbol);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
  if (bg_color.full == lv_color_hex(C_WHITE).full) {
    lv_obj_set_style_text_color(label, lv_color_hex(C_SPOTIFY_BLACK), 0);
  }
  lv_obj_center(label);
  lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
  return btn;
}

// Function to create the "Playing Now" screen
void createPlayingNowScreen() {
  // Create screen1
  screen1 = lv_obj_create(NULL);
  lv_scr_load(screen1);

  // Set the LVGL theme to dark
  lv_theme_t *dark_theme = lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_GREY), lv_palette_main(LV_PALETTE_BLUE), true, LV_FONT_DEFAULT);
  lv_disp_set_theme(NULL, dark_theme);

  // Create LVGL objects for the display
  lv_obj_t *topContainer = lv_obj_create(screen1);
  lv_obj_set_width(topContainer, lv_obj_get_width(screen1));
  lv_obj_align(topContainer, LV_ALIGN_TOP_MID, 0, 0);  // Adjusted alignment to stick to the top
  lv_obj_set_flex_flow(topContainer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(topContainer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_bg_color(topContainer, lv_color_hex(C_SPOTIFY_GREEN), 0);  // Set background color to the same as the play button
  lv_obj_set_style_radius(topContainer, 0, 0);                                // Remove border radius
  lv_obj_set_style_border_width(topContainer, 0, 0);                          // Remove border
  lv_obj_set_height(topContainer, LV_SIZE_CONTENT);                           // Set height to fit content

  labelUsername = lv_label_create(topContainer);  // Changed parent to topContainer
  lv_obj_set_style_text_font(labelUsername, &lv_font_montserrat_28, 0);
  lv_label_set_text_fmt(labelUsername, "Hello, %s", apiClient.getUsername().c_str());

  btnSettings = createButton(
    topContainer, LV_SYMBOL_SETTINGS, [](lv_event_t *e) {
      wakeScreen();
      createSettingsScreen();
    },
    40, 40, lv_color_hex(C_WHITE));

  imgTrack = lv_img_create(screen1);
  lv_obj_set_size(imgTrack, 64, 64);
  lv_obj_align(imgTrack, LV_ALIGN_TOP_LEFT, 10, 135);      // Moved 10 px above
  lv_obj_set_style_radius(imgTrack, LV_RADIUS_CIRCLE, 0);  // Make it round

  lv_obj_t *textContainer = lv_obj_create(screen1);
  lv_obj_remove_style_all(textContainer);
  lv_obj_set_width(textContainer, lv_obj_get_width(screen1) - 94);
  lv_obj_align(textContainer, LV_ALIGN_TOP_LEFT, 84, 135);  // Moved 10 px above

  lv_obj_set_flex_flow(textContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(textContainer, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_height(textContainer, LV_SIZE_CONTENT);  // Set height to fit content

  labelTrack = lv_label_create(textContainer);
  lv_obj_set_style_text_font(labelTrack, &lv_font_montserrat_32, 0);
  lv_label_set_long_mode(labelTrack, LV_LABEL_LONG_SCROLL_CIRCULAR);  // Enable marquee effect for long text

  labelArtist = lv_label_create(textContainer);
  lv_obj_set_style_text_font(labelArtist, &lv_font_montserrat_22, 0);  // Set height to fit content
  lv_label_set_long_mode(labelArtist, LV_LABEL_LONG_WRAP);             // Enable line wrapping

  btnContainer = lv_obj_create(screen1);
  lv_obj_set_size(btnContainer, lv_obj_get_width(screen1), 120);
  lv_obj_align(btnContainer, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(btnContainer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btnContainer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(btnContainer, lv_color_hex(C_SPOTIFY_BLACK), 0);
  lv_obj_set_style_radius(btnContainer, 0, 0);
  lv_obj_set_style_border_width(btnContainer, 0, 0);        // Remove border
  lv_obj_clear_flag(btnContainer, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

  btnVolumeDown = createButton(btnContainer, LV_SYMBOL_VOLUME_MID, [](lv_event_t *e) {
    wakeScreen();
    apiClient.setVolume(0);
  });
  btnPrevious = createButton(btnContainer, LV_SYMBOL_PREV, [](lv_event_t *e) {
    wakeScreen();
    apiClient.controlSpotify("previous");
  });
  btnPlayPause = createButton(
    btnContainer, isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, [](lv_event_t *e) {
      wakeScreen();
      apiClient.controlSpotify(isPlaying ? "pause" : "play");
    },
    110, 70, lv_color_hex(C_SPOTIFY_GREEN));
  btnNext = createButton(btnContainer, LV_SYMBOL_NEXT, [](lv_event_t *e) {
    wakeScreen();
    apiClient.controlSpotify("next");
  });
  btnVolumeUp = createButton(btnContainer, LV_SYMBOL_VOLUME_MAX, [](lv_event_t *e) {
    wakeScreen();
    apiClient.setVolume(100);
  });

  barProgress = lv_bar_create(screen1);
  lv_obj_set_size(barProgress, lv_obj_get_width(screen1) - 20, 20);
  lv_obj_align(barProgress, LV_ALIGN_BOTTOM_MID, 0, -140);  // Moved 30 px above

  lv_obj_t *timeContainer = lv_obj_create(screen1);
  lv_obj_remove_style_all(timeContainer);
  lv_obj_set_size(timeContainer, lv_obj_get_width(screen1) - 20, 40);
  lv_obj_align_to(timeContainer, barProgress, LV_ALIGN_OUT_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(timeContainer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(timeContainer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  labelCurrentTime = lv_label_create(timeContainer);
  lv_obj_set_style_text_font(labelCurrentTime, &lv_font_montserrat_24, 0);

  labelTotalTime = lv_label_create(timeContainer);
  lv_obj_set_style_text_font(labelTotalTime, &lv_font_montserrat_24, 0);

  // Create a task to update track information
  if (!updateTrackInfoTaskHandle) {
    xTaskCreate(updateTrackInfo, "UpdateTrackInfo", 5 * 1024, NULL, 12, &updateTrackInfoTaskHandle);
  }

  // Add swipe gesture to switch between screens
  lv_obj_add_event_cb(
    screen1, [](lv_event_t *e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT) {
          createSettingsScreen();
        } else if (dir == LV_DIR_TOP) {
          createBrowseScreen();
        }
      }
    },
    LV_EVENT_GESTURE, NULL);

  // Add touch event to wake screen
  lv_obj_add_event_cb(
    screen1, [](lv_event_t *e) {
      wakeScreen();
    },
    LV_EVENT_CLICKED, NULL);
}

void createBrowseScreen() {
  browseScreen = lv_obj_create(NULL);
  lv_scr_load(browseScreen);

  // Create LVGL objects for the display
  lv_obj_t *topBrowseContainer = lv_obj_create(browseScreen);
  lv_obj_set_width(topBrowseContainer, lv_obj_get_width(screen1));
  lv_obj_align(topBrowseContainer, LV_ALIGN_TOP_MID, 0, 0);  // Adjusted alignment to stick to the top
  lv_obj_set_flex_flow(topBrowseContainer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(topBrowseContainer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_bg_color(topBrowseContainer, lv_color_hex(C_SPOTIFY_GREEN), 0);  // Set background color to the same as the play button
  lv_obj_set_style_radius(topBrowseContainer, 0, 0);                                // Remove border radius
  lv_obj_set_style_border_width(topBrowseContainer, 0, 0);                          // Remove border
  lv_obj_set_height(topBrowseContainer, LV_SIZE_CONTENT);                           // Set height to fit content

  labelBrowse = lv_label_create(topBrowseContainer);
  lv_label_set_text(labelBrowse, "Browse");
  lv_obj_set_style_text_font(labelBrowse, &lv_font_montserrat_28, 0);

  // Create a list to display featured playlists
  lv_obj_t *playlistList = lv_list_create(browseScreen);
  lv_obj_set_size(playlistList, lv_obj_get_width(browseScreen) - 20, lv_obj_get_height(browseScreen) - 75);
  lv_obj_align(playlistList, LV_ALIGN_TOP_LEFT, 10, 75);

  // Fetch featured playlists from the API client
  auto [playlists, playlistCount] = apiClient.getFeaturedPlaylists(5, 0);
  checkAndReconnectWiFi();
  if (playlists == nullptr || playlistCount == 0) {
    Serial.println("No playlists found or failed to fetch playlists.");
    return;
  }

  // Add playlists to the list
  for (size_t i = 0; i < playlistCount; ++i) {
    lv_obj_t *list_btn = lv_list_add_btn(playlistList, NULL, playlists[i].name.c_str());
    char *playlistId = new char[playlists[i].id.length() + 1];
    strcpy(playlistId, playlists[i].id.c_str());
    lv_obj_set_user_data(list_btn, (void *)playlistId);  // Use actual id as user data

    // Create an image object for the playlist cover
    lv_obj_t *img = lv_img_create(list_btn);
    lv_obj_set_size(img, 64, 64);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 10, 0);

    // Fetch the image from the remote URL in a non-blocking task
    if (!fetchPlaylistImageTaskHandle) {
      xTaskCreate(fetchPlaylistImage, "FetchPlaylistImage", 5 * 1024, (void *)img, 12, &fetchPlaylistImageTaskHandle);
    }

    lv_obj_add_event_cb(
      list_btn, [](lv_event_t *e) {
        lv_obj_t *btn = lv_event_get_target(e);
        const char *playlistId = (const char *)lv_obj_get_user_data(btn);
        apiClient.playPlaylist(String(playlistId));
        checkAndReconnectWiFi();
        Serial.println("Selected playlist ID: " + String(playlistId));
        lv_scr_load(screen1);  // Move back to playing now screen
      },
      LV_EVENT_CLICKED, NULL);
  }

  // Clean up allocated memory for playlists
  delete[] playlists;

  // Add swipe gesture to switch between screens
  lv_obj_add_event_cb(
    browseScreen, [](lv_event_t *e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_BOTTOM) {
          lv_scr_load(screen1);  // Move back to playing now screen
        }
      }
    },
    LV_EVENT_GESTURE, NULL);

  // Add touch event to wake screen
  lv_obj_add_event_cb(
    browseScreen, [](lv_event_t *e) {
      wakeScreen();
    },
    LV_EVENT_CLICKED, NULL);
}

// Function to create the settings screen
void createSettingsScreen() {
  // Create screen2
  screen2 = lv_obj_create(NULL);
  lv_scr_load(screen2);  // Ensure the screen is loaded before adding objects

  lv_obj_t *topDevicesContainer = lv_obj_create(screen2);
  lv_obj_set_width(topDevicesContainer, lv_obj_get_width(screen1));
  lv_obj_align(topDevicesContainer, LV_ALIGN_TOP_MID, 0, 0);  // Adjusted alignment to stick to the top
  lv_obj_set_flex_flow(topDevicesContainer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(topDevicesContainer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_bg_color(topDevicesContainer, lv_color_hex(C_SPOTIFY_GREEN), 0);  // Set background color to the same as the play button
  lv_obj_set_style_radius(topDevicesContainer, 0, 0);                                // Remove border radius
  lv_obj_set_style_border_width(topDevicesContainer, 0, 0);                          // Remove border
  lv_obj_set_height(topDevicesContainer, LV_SIZE_CONTENT);                           // Set height to fit content

  labelScreen2 = lv_label_create(topDevicesContainer);
  lv_label_set_text(labelScreen2, "Available Devices:");
  lv_obj_set_style_text_font(labelScreen2, &lv_font_montserrat_28, 0);

  deviceList = lv_list_create(screen2);
  if (deviceList == nullptr) {
    Serial.println("Failed to create device list");
    return;
  }
  lv_obj_set_size(deviceList, lv_obj_get_width(screen2) - 20, lv_obj_get_height(screen2) - 75);
  lv_obj_align(deviceList, LV_ALIGN_TOP_LEFT, 10, 75);

  Serial.println("Fetching devices list...");

  // Fetch devices from the API client
  auto [devices, deviceCount] = apiClient.getDevicesList();
  checkAndReconnectWiFi();
  if (devices == nullptr || deviceCount == 0) {
    Serial.println("No devices found or failed to fetch devices.");
    return;
  }

  // Add devices to the list
  for (size_t i = 0; i < deviceCount; ++i) {
    lv_obj_t *list_btn = lv_list_add_btn(deviceList, LV_SYMBOL_AUDIO, devices[i].name.c_str());
    char *deviceId = new char[devices[i].id.length() + 1];
    strcpy(deviceId, devices[i].id.c_str());
    lv_obj_set_user_data(list_btn, (void *)deviceId);  // Use actual id as user data
    lv_obj_add_event_cb(
      list_btn, [](lv_event_t *e) {
        lv_obj_t *btn = lv_event_get_target(e);
        const char *deviceId = (const char *)lv_obj_get_user_data(btn);
        apiClient.setActiveDevice(String(deviceId));
        checkAndReconnectWiFi();
        Serial.println("Selected device ID: " + String(deviceId));
      },
      LV_EVENT_CLICKED, NULL);
  }

  // Clean up allocated memory for devices
  delete[] devices;

  // Add swipe gesture to switch between screens
  lv_obj_add_event_cb(
    screen2, [](lv_event_t *e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) {
          lv_scr_load(screen1);
        }
      }
    },
    LV_EVENT_GESTURE, NULL);

  // Add touch event to wake screen
  lv_obj_add_event_cb(
    screen2, [](lv_event_t *e) {
      wakeScreen();
    },
    LV_EVENT_CLICKED, NULL);
}

// Function to show the splash screen
void showSplashScreen() {
  splashScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(splashScreen, lv_color_hex(C_SPOTIFY_GREEN), 0);
  lv_obj_t *label = lv_label_create(splashScreen);
  lv_label_set_text(label, "Spotify Remote Controller");
  lv_obj_set_style_text_color(label, lv_color_hex(C_WHITE), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
  lv_obj_center(label);

  // Create and position the load step label
  labelLoadStep = lv_label_create(splashScreen);
  lv_label_set_text(labelLoadStep, "Loading...");
  lv_obj_set_style_text_color(labelLoadStep, lv_color_hex(C_WHITE), 0);
  lv_obj_set_style_text_font(labelLoadStep, &lv_font_montserrat_24, 0);
  lv_obj_align(labelLoadStep, LV_ALIGN_BOTTOM_MID, 0, -20);

  lv_scr_load(splashScreen);
  lv_task_handler();  // Ensure the splash screen is rendered
}

// Function to set up the device
void setup() {
  Serial.begin(115200);
  Serial.println("============================================");
  Serial.println("Welcome to spotify player");
  Serial.println("============================================");

  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);

  // Initialize the AMOLED display
  bool rslt = false;
  rslt = amoled.beginAMOLED_241();

  if (!rslt) {
    while (1) {
      Serial.println("The board model cannot be detected, please raise the Core Debug Level to an error");
      delay(500);
    }
  }

  // Initialize LVGL helper
  beginLvglHelper(amoled);

  showSplashScreen();

  // Connect to WiFi
  connectToWiFi();

  lastActivityTime = millis();

  createPlayingNowScreen();
}

void updateLoadStep(const char *step) {
  if (labelLoadStep) {
    lv_label_set_text(labelLoadStep, step);
    lv_task_handler();
  }
}

// Function to handle the main loop
void loop() {
  // Handle LVGL tasks
  lv_task_handler();
  delay(1);

  // Check for inactivity and dim screen if needed
  if (millis() - lastActivityTime > 20000) {
    dimScreen();
  }
}

// Function to handle WiFi events
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      Serial.println("Completed scan for access points");
      break;
    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("WiFi client started");
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      Serial.println("WiFi client stopped");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("Connected to access point");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("Disconnected from WiFi access point");
      break;
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
      Serial.println("Authentication mode of access point has changed");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("Obtained IP address: ");
      Serial.println(WiFi.localIP());

      if (!updateTrackInfoTaskHandle) {
        xTaskCreate(updateTrackInfo, "UpdateTrackInfo", 5 * 1024, NULL, 12, &updateTrackInfoTaskHandle);
      }

      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("Lost IP address and IP address is reset to 0");
      break;
    default:
      break;
  }
}

// Function to update track information
void updateTrackInfo(void *parameter) {
  for (;;) {
    TrackInfo trackInfo = apiClient.getCurrentTrackInfo();
    checkAndReconnectWiFi();

    isPlaying = trackInfo.isPlaying;

    lv_label_set_text(labelTrack, trackInfo.name.c_str());
    lv_label_set_text(labelArtist, trackInfo.artistName.c_str());
    lv_label_set_text(lv_obj_get_child(btnPlayPause, NULL), trackInfo.isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    if (trackInfo.coverUrl != currentTrackImageUrl) {
      currentTrackImageUrl = trackInfo.coverUrl;
      if (!fetchTrackImageTaskHandle) {
        xTaskCreate(fetchTrackImage, "FetchTrackImage", 5 * 1024, (void *)currentTrackImageUrl.c_str(), 12, &fetchTrackImageTaskHandle);
      }
    }

    currentTrackDuration = trackInfo.duration;
    currentTrackProgress = trackInfo.progress;
    if (currentTrackDuration != 0) {
      lv_bar_set_value(barProgress, (currentTrackProgress * 100) / currentTrackDuration, LV_ANIM_OFF);
    }

    int currentTimeSec = currentTrackProgress / 1000;
    int totalTimeSec = currentTrackDuration / 1000;
    char currentTimeStr[10];
    char totalTimeStr[10];
    snprintf(currentTimeStr, sizeof(currentTimeStr), "%02d:%02d", currentTimeSec / 60, currentTimeSec % 60);
    snprintf(totalTimeStr, sizeof(totalTimeStr), "%02d:%02d", totalTimeSec / 60, totalTimeSec % 60);
    lv_label_set_text(labelCurrentTime, currentTimeStr);
    lv_label_set_text(labelTotalTime, totalTimeStr);

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Function to fetch track image
void fetchTrackImage(void *parameter) {
  const char *imageUrl = (const char *)parameter;
  HTTPClient http;
  http.begin(imageUrl);
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    int len = http.getSize();
    uint8_t *buffer = (uint8_t *)malloc(len);
    if (buffer) {
      WiFiClient *stream = http.getStreamPtr();
      stream->readBytes(buffer, len);
      // Save the original image in ROM
      lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)malloc(sizeof(lv_img_dsc_t));
      if (img_dsc) {
        img_dsc->data = buffer;
        img_dsc->data_size = len;
        img_dsc->header.always_zero = 0;
        img_dsc->header.w = 64;
        img_dsc->header.h = 64;
        img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
        lv_img_set_src(imgTrack, img_dsc);
      } else {
        free(buffer);
      }
    }
  } else {
    Serial.println("Failed to fetch track image, error: " + String(httpResponseCode));
    if (httpResponseCode == -1) {
      Serial.println("HTTP request failed with error: -1, reconnecting WiFi...");
      connectToWiFi();
    }
  }
  http.end();
  fetchTrackImageTaskHandle = NULL;
  vTaskDelete(NULL);
}

// Function to fetch playlist image
void fetchPlaylistImage(void *parameter) {
  lv_obj_t *img = (lv_obj_t *)parameter;
  const char *imageUrl = (const char *)lv_obj_get_user_data(img);
  HTTPClient http;
  http.begin(imageUrl);
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    int len = http.getSize();
    uint8_t *buffer = (uint8_t *)malloc(len);
    if (buffer) {
      WiFiClient *stream = http.getStreamPtr();
      stream->readBytes(buffer, len);
      // Save the original image in ROM
      lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)malloc(sizeof(lv_img_dsc_t));
      if (img_dsc) {
        img_dsc->data = buffer;
        img_dsc->data_size = len;
        img_dsc->header.always_zero = 0;
        img_dsc->header.w = 64;
        img_dsc->header.h = 64;
        img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
        lv_img_set_src(img, img_dsc);
      } else {
        free(buffer);
      }
    }
  } else {
    Serial.println("Failed to fetch playlist image, error: " + String(httpResponseCode));
    if (httpResponseCode == -1) {
      Serial.println("HTTP request failed with error: -1, reconnecting WiFi...");
      connectToWiFi();
    }
  }
  http.end();
  fetchPlaylistImageTaskHandle = NULL;
  vTaskDelete(NULL);
}

// Function to connect to WiFi
void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    updateLoadStep("Connecting to WiFi...");
    delay(1000);
  }
  updateLoadStep("Connected to WiFi");
  delay(500);
}

// Function to check and reconnect WiFi if needed
void checkAndReconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectToWiFi();
  }
}

// Function to dim the screen
void dimScreen() {
  amoled.setBrightness(15);
}

// Function to wake the screen
void wakeScreen() {
  amoled.setBrightness(100);
  lastActivityTime = millis();  // Reset inactivity timer
}
