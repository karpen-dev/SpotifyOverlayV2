//
// Created by karpen on 10/28/25.
//

#ifndef SPOTIFYOVERLAY_SPOTIFYAPI_H
#define SPOTIFYOVERLAY_SPOTIFYAPI_H

#pragma once

#include <string>
#include <memory>
#include <functional>
#include "Types.h"

class SpotifyAPI {
public:
    using TrackCallback = std::function<void(const SpotifyTrack&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    SpotifyAPI();
    ~SpotifyAPI();

    void setAccessToken(const std::string &token) const;
    void getCurrentTrack(TrackCallback success, ErrorCallback error) const;
    void stopPolling() const;
    void controlPlayback(PlayBackAction action, std::function<void(bool)> callback) const;
    void startPolling(std::chrono::seconds interval) const;
    void getPlaybackState(std::function<void(const SpotifyTrack &)> callback) const;
    void setVolume(int volumePercent, std::function<void(bool)> callback) const;
    void seekToPosition(int positionMs, const std::function<void(bool)> &callback) const;
    bool isPolling() const;
    void setTrackCallback(const TrackCallback &callback) const;
    void setErrorCallback(const ErrorCallback &callback) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

#endif //SPOTIFYOVERLAY_SPOTIFYAPI_H