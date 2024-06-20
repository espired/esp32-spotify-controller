#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "spotify_api.h"

const int HTTP_RESPONSE_CODE_UNAUTHORIZED = 401;
const int HTTP_RESPONSE_CODE_OK = 200;
const int HTTP_RESPONSE_CODE_NO_CONTENT = 204;
const char *spotifyApiUrl = "https://api.spotify.com/v1";

SpotifyApi::SpotifyApi(String cid, String cs, String ac, String rt)
{
  this->clientId = cid;
  this->clientSecret = cs;
  this->accessToken = ac;
  this->refreshToken = rt;
  this->lastHttpResponseCode = 0;
}

void SpotifyApi::withAuth(HTTPClient &http)
{
  http.addHeader("Authorization", "Bearer " + accessToken);
}

String SpotifyApi::refreshAccessToken()
{
  HTTPClient http;
  http.begin("https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "grant_type=refresh_token&refresh_token=" + refreshToken + "&client_id=" + clientId + "&client_secret=" + clientSecret;
  int httpResponseCode = http.POST(httpRequestData);
  lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode != HTTP_RESPONSE_CODE_OK)
  {
    Serial.println("Got error " + String(httpResponseCode));
    Serial.println("Failed to refresh access token");
    return "";
  }

  String response = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, response);
  return doc["access_token"].as<String>();
}

bool SpotifyApi::performHttpRequestWithRetry(HTTPClient &http, String &apiUrl, String &response)
{
  accessToken = SpotifyApi::refreshAccessToken();

  if (accessToken == "")
  {
    Serial.println("Failed to refresh access token");
    return false;
  }

  http.begin(apiUrl);
  SpotifyApi::withAuth(http);
  int httpResponseCode = http.GET();
  lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode > 0)
  {
    response = http.getString();
    return true;
  }
  else
  {
    Serial.println("HTTP request failed with error: " + String(httpResponseCode));
    return false;
  }
}

TrackInfo SpotifyApi::getCurrentTrackInfo()
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/player/currently-playing";
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);

  TrackInfo trackInfo;
  String response;

  int httpResponseCode = http.GET();
  lastHttpResponseCode = httpResponseCode;
  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    if (performHttpRequestWithRetry(http, apiUrl, response))
    {
      SpotifyApi::errorCount = 0;

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);

      trackInfo.name = doc["item"]["name"].as<String>();
      JsonArray artists = doc["item"]["artists"].as<JsonArray>();
      String artistNames = "";
      for (JsonVariant artist : artists) {
        if (artistNames.length() > 0) {
          artistNames += ", ";
        }
        artistNames += artist["name"].as<String>();
      }
      trackInfo.artistName = artistNames;
      trackInfo.coverUrl = doc["item"]["album"]["images"][2]["url"].as<String>();
      trackInfo.duration = doc["item"]["duration_ms"].as<int>();
      trackInfo.progress = doc["progress_ms"].as<int>();
      trackInfo.isPlaying = doc["is_playing"].as<bool>();
    }
    else
    {
      SpotifyApi::errorCount++;
    }
  }
  else if (httpResponseCode > 0)
  {
    response = http.getString();
    SpotifyApi::errorCount = 0;

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    trackInfo.name = doc["item"]["name"].as<String>();
    trackInfo.artistName = doc["item"]["artists"][0]["name"].as<String>();
    trackInfo.coverUrl = doc["item"]["album"]["images"][2]["url"].as<String>();
    trackInfo.duration = doc["item"]["duration_ms"].as<int>();
    trackInfo.progress = doc["progress_ms"].as<int>();
    trackInfo.isPlaying = doc["is_playing"].as<bool>();
  }
  else
  {
    Serial.println("HTTP request failed with error: " + String(httpResponseCode));
    SpotifyApi::errorCount++;
  }

  http.end();
  return trackInfo;
}

std::tuple<Device *, size_t> SpotifyApi::getDevicesList()
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/player/devices";
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);

  String response;
  int httpResponseCode = http.GET();
  lastHttpResponseCode = httpResponseCode;
  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    if (!performHttpRequestWithRetry(http, apiUrl, response))
    {
      Serial.println("Failed to get devices list");
      http.end();
      return std::make_tuple(nullptr, 0);
    }
  }
  else if (httpResponseCode <= 0)
  {
    Serial.println("HTTP request failed with error: " + String(httpResponseCode));
    http.end();
    return std::make_tuple(nullptr, 0);
  }
  else
  {
    response = http.getString();
  }

  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);

  JsonArray devices = doc["devices"].as<JsonArray>();
  Device *deviceList = new (std::nothrow) Device[devices.size()];
  if (deviceList == nullptr)
  {
    Serial.println("Failed to allocate memory for device list");
    http.end();
    return std::make_tuple(nullptr, 0);
  }

  int index = 0;
  for (JsonVariant device : devices)
  {
    deviceList[index].id = device["id"].as<String>();
    deviceList[index].name = device["name"].as<String>();
    index++;
  }
  http.end();

  Serial.print("Number of devices: ");
  Serial.println(devices.size());

  return std::make_tuple(deviceList, devices.size());
}

void SpotifyApi::setActiveDevice(String deviceId)
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/player";
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"device_ids\":[\"" + String(deviceId) + "\"]}";
  int httpResponseCode = http.PUT(payload);
  lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    String response;
    if (performHttpRequestWithRetry(http, apiUrl, response))
    {
      Serial.println("Successfully set the active device.");
    }
    else
    {
      Serial.printf("Failed to set active device, error: %d with id: %s \n", httpResponseCode, deviceId);
    }
  }
  else if (httpResponseCode != HTTP_RESPONSE_CODE_NO_CONTENT)
  {
    Serial.printf("Failed to set active device, error: %d with id: %s \n", httpResponseCode, deviceId);
  }
  else
  {
    Serial.println("Successfully set the active device.");
  }

  http.end();
}

String SpotifyApi::getUsername()
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me";
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);

  String response;
  int httpResponseCode = http.GET();
  lastHttpResponseCode = httpResponseCode;
  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    if (performHttpRequestWithRetry(http, apiUrl, response))
    {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      http.end();

      return doc["display_name"].as<String>();
    }
    else
    {
      Serial.println("Failed to get username");
      http.end();
      return "Unknown User";
    }
  }
  else if (httpResponseCode > 0)
  {
    response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    http.end();

    return doc["display_name"].as<String>();
  }
  else
  {
    Serial.println("HTTP request failed with error: " + String(httpResponseCode));
    http.end();
    return "Unknown User";
  }
}

void SpotifyApi::controlSpotify(String command)
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/player/" + command;
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);
  http.addHeader("Content-Type", "application/json");

  String response;
  int httpResponseCode;

  if (command == "play" || command == "pause")
  {
    httpResponseCode = http.PUT("{}");
  }
  else if (command == "next" || command == "previous")
  {
    httpResponseCode = http.POST("{}");
  }
  lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    if (!performHttpRequestWithRetry(http, apiUrl, response))
    {
      Serial.printf("Failed to send %s command\n", command.c_str());
    }
    else
    {
      Serial.printf("Successfully sent %s command to Spotify\n", command.c_str());
    }
  }
  else if (httpResponseCode > 0)
  {
    Serial.printf("Successfully sent %s command to Spotify\n", command.c_str());
  }
  else
  {
    Serial.printf("Failed to send %s command\n", command.c_str());
  }

  http.end();
}

void SpotifyApi::setVolume(int volume)
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/player/volume?volume_percent=" + String(volume);
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT("");
  lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    String response;
    if (!performHttpRequestWithRetry(http, apiUrl, response))
    {
      Serial.printf("Failed to set volume to %d\n", volume);
    }
    else
    {
      Serial.printf("Successfully set volume to %d\n", volume);
    }
  }
  else if (httpResponseCode > 0)
  {
    Serial.printf("Successfully set volume to %d\n", volume);
  }
  else
  {
    Serial.printf("Failed to set volume to %d\n", volume);
  }

  http.end();
}

std::tuple<FeaturedPlaylist *, size_t> SpotifyApi::getFeaturedPlaylists(int limit, int offset)
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/browse/featured-playlists?limit=" + String(limit) + "&offset=" + String(offset) + "&locale=en_US";
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);

  String response;
  int httpResponseCode = http.GET();
  lastHttpResponseCode = httpResponseCode;
  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    if (!performHttpRequestWithRetry(http, apiUrl, response))
    {
      Serial.println("Failed to get featured playlists");
      http.end();
      return std::make_tuple(nullptr, 0);
    }
  }
  else if (httpResponseCode <= 0)
  {
    Serial.println("HTTP request failed with error: " + String(httpResponseCode));
    http.end();
    return std::make_tuple(nullptr, 0);
  }
  else
  {
    response = http.getString();
  }

  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);

  JsonArray playlists = doc["playlists"]["items"].as<JsonArray>();
  FeaturedPlaylist *playlistList = new (std::nothrow) FeaturedPlaylist[playlists.size()];
  if (playlistList == nullptr)
  {
    Serial.println("Failed to allocate memory for playlist list");
    http.end();
    return std::make_tuple(nullptr, 0);
  }

  int index = 0;
  for (JsonVariant playlist : playlists)
  {
    playlistList[index].name = playlist["name"].as<String>();
    playlistList[index].id = playlist["id"].as<String>();
    playlistList[index].imageUrl = playlist["images"][2]["url"].as<String>();
    index++;
  }

  http.end();
  return std::make_tuple(playlistList, playlists.size());
}

void SpotifyApi::playPlaylist(String playlistId)
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/player/play";
  http.begin(apiUrl);
  SpotifyApi::withAuth(http);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"context_uri\":\"spotify:playlist:" + playlistId + "\"}";
  int httpResponseCode = http.PUT(payload);
  lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode == HTTP_RESPONSE_CODE_UNAUTHORIZED)
  {
    String response;
    if (!performHttpRequestWithRetry(http, apiUrl, response))
    {
      Serial.println("Failed to play playlist");
    }
  }
  else if (httpResponseCode <= 0)
  {
    Serial.println("HTTP request failed with error: " + String(httpResponseCode));
  }

  http.end();
}

std::tuple<FeaturedPlaylist *, size_t> SpotifyApi::getUserPlaylists(int limit, int offset)
{
  HTTPClient http;
  String apiUrl = String(spotifyApiUrl) + "/me/playlists?limit=" + String(limit) + "&offset=" + String(offset);
  http.begin(apiUrl);
  withAuth(http);

  int httpResponseCode = http.GET();
  this->lastHttpResponseCode = httpResponseCode;

  if (httpResponseCode != HTTP_CODE_OK)
  {
    Serial.println("Failed to get user playlists, error: " + String(httpResponseCode));
    http.end();
    return std::make_tuple(nullptr, 0);
  }

  String response = http.getString();
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, response);
  JsonArray playlists = doc["items"].as<JsonArray>();

  size_t playlistCount = playlists.size();
  FeaturedPlaylist *playlistList = new FeaturedPlaylist[playlistCount];

  if (playlistList == nullptr)
  {
    Serial.println("Failed to allocate memory for playlist list");
    http.end();
    return std::make_tuple(nullptr, 0);
  }

  int index = 0;
  for (JsonVariant playlist : playlists)
  {
    playlistList[index].name = playlist["name"].as<String>();
    playlistList[index].id = playlist["id"].as<String>();
    playlistList[index].imageUrl = playlist["images"][0]["url"].as<String>();
    index++;
  }

  http.end();
  return std::make_tuple(playlistList, playlistCount);
}
