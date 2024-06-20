# Spotify Remote Controller

## Overview

The Spotify Remote Controller is an ESP32 project that allows you to control your Spotify playback using a custom hardware interface. It features an AMOLED display, WiFi connectivity, and integration with the Spotify API to fetch and display track information, playlists, and devices.

## Tokens
The app is working with Spotify OAuth, which means you need to:
- Create a new app through: `https://developer.spotify.com/dashboard`
- Go to app settings, copy `ClientID` and `ClientSecret`
- Paste them into `Spotif.ino`
- Go to `https://accounts.spotify.com/authorize?client_id=<ClientID>&response_type=code&redirect_uri=http://localhost:5137&scope=user-read-playback-state%20user-modify-playback-state`
- Copy the code from the URL
- Use Postman to make a call to `https://accounts.spotify.com/api/token` with the following body:
```
{
    "grant_type": "authorization_code",
    "code": "<code>",
    "redirect_uri": "http://localhost:5137",
    "client_id": "<ClientID>",
    "client_secret": "<ClientSecret>"
}
```
- Copy the `access_token` and `refresh_token` from the response
- Paste it into `Spotif.ino`

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your changes.