#pragma once

struct TrackInfo
{
  String name;
  String artistName;
  String coverUrl;
  int duration;
  int progress;
  bool isPlaying;
};

struct FeaturedPlaylist
{
  String name;
  String id;
  String imageUrl;
};

struct Device
{
  String id;
  String name;
};

class SpotifyApi
{
public:
  int errorCount;
  int lastHttpResponseCode;
  SpotifyApi(String, String, String, String);
  TrackInfo getCurrentTrackInfo();
  String refreshAccessToken();
  std::tuple<Device *, size_t> getDevicesList();
  void setActiveDevice(String);
  String getUsername();
  bool performHttpRequestWithRetry(HTTPClient &http, String &apiUrl, String &response);
  void controlSpotify(String command);
  void setVolume(int volume);
  void playPlaylist(String playlistId);
  std::tuple<FeaturedPlaylist *, size_t> getFeaturedPlaylists(int limit, int offset);
  std::tuple<FeaturedPlaylist *, size_t> getUserPlaylists(int limit, int offset);

private:
  String clientId;
  String clientSecret;
  String accessToken;
  String refreshToken;
  void withAuth(HTTPClient &http);
};