//
// Created by karpen on 10/28/25.
//

#ifndef SPOTIFYOVERLAY_TYPES_H
#define SPOTIFYOVERLAY_TYPES_H

#pragma once

#include <string>
#include <chrono>
#include <utility>

struct SpotifyTrack {
    std::string name;
    std::string artist;
    std::string imageUrl;
    bool isPlaying;

    explicit SpotifyTrack (std::string  name = "", std::string  artist = "",
        std::string  imageUrl = "", const bool isPlaying = false)
            : name(std::move(name)), artist(std::move(artist)), imageUrl(std::move(imageUrl)), isPlaying(isPlaying) {}
};

struct AuthTokens {
    std::string accessToken;
    std::string refreshToken;
    std::chrono::system_clock::time_point lastUpdate;

    AuthTokens()
        : accessToken(""),
          refreshToken(""),
          lastUpdate(std::chrono::system_clock::time_point{}) {}

    AuthTokens(const std::string& access, const std::string& refresh, const std::chrono::system_clock::time_point& update)
        : accessToken(access),
          refreshToken(refresh),
          lastUpdate(update) {}

    [[nodiscard]] bool isValid() const {
        return !accessToken.empty() &&
            std::chrono::system_clock::now() - lastUpdate < std::chrono::hours(24);
    }
};

enum class PlayBackAction {
    PLAY,
    PAUSE,
    NEXT,
    PREVIOUS,
    TOGGLE
};

#endif //SPOTIFYOVERLAY_TYPES_H