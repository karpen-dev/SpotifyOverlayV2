//
// Created by karpen on 10/28/25.
//

#include "../include/SpotifyAPI.h"
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>

using namespace web;
using namespace web::http;
using namespace web::http::client;

class SpotifyAPI::Impl {
public:
    std::string accessToken;
    std::atomic<bool> polling{false};
    std::thread pollingThread;
    TrackCallback trackCallback_;
    ErrorCallback errorCallback_;

    Impl() = default;
};

SpotifyAPI::SpotifyAPI() : pImpl_(std::make_unique<Impl>()) {}

SpotifyAPI::~SpotifyAPI() {
    stopPolling();
}

void SpotifyAPI::setAccessToken(const std::string& token) const {
    pImpl_->accessToken = token;
    std::cout << "Access token set: " << token.substr(0, 10) << "..." << std::endl;
}

void SpotifyAPI::setTrackCallback(const TrackCallback &callback) const {
    pImpl_->trackCallback_ = callback;
}

void SpotifyAPI::setErrorCallback(const ErrorCallback &callback) const {
    pImpl_->errorCallback_ = callback;
}

void SpotifyAPI::getCurrentTrack(TrackCallback success, ErrorCallback error) const {
    if (pImpl_->accessToken.empty()) {
        if (pImpl_->errorCallback_) {
            pImpl_->errorCallback_("Not authenticated");
        }
        return;
    }

    std::thread([this]() {
        try {
            http_client client("https://api.spotify.com/v1");
            http_request request(methods::GET);

            request.set_request_uri("/me/player/currently-playing");
            request.headers().add("Authorization", "Bearer " + pImpl_->accessToken);
            request.headers().add("Accept", "application/json");

            if (const auto response = client.request(request).get(); response.status_code() == status_codes::OK) {
                auto json = response.extract_json().get();

                SpotifyTrack track;
                track.isPlaying = json["is_playing"].as_bool();

                auto item = json["item"];
                track.name = item["name"].as_string();

                if (auto artists = item["artists"]; artists.size() > 0) {
                    track.artist = artists[0]["name"].as_string();

                    for (size_t i = 1; i < artists.size(); ++i) {
                        track.artist += ", " + artists[i]["name"].as_string();
                    }
                }

                auto album = item["album"];
                if (auto images = album["images"]; images.size() > 0) {
                    track.imageUrl = images[0]["url"].as_string();
                }

                std::cout << "Retrieved track: " << track.name << " - " << track.artist << std::endl;

                if (pImpl_->trackCallback_) {
                    pImpl_->trackCallback_(track);
                }
            } else if (response.status_code() == status_codes::NoContent) {
                if (pImpl_->trackCallback_) {
                    pImpl_->trackCallback_(SpotifyTrack("Not Playing", "", "", false));
                }
            } else if (response.status_code() == status_codes::Unauthorized) {
                std::cerr << "Authentication expired" << std::endl;
                if (pImpl_->errorCallback_) {
                    pImpl_->errorCallback_("Authentication expired");
                }
            } else if (response.status_code() == status_codes::Forbidden) {
                std::cerr << "Insufficient permissions" << std::endl;
                if (pImpl_->errorCallback_) {
                    pImpl_->errorCallback_("Insufficient permissions");
                }
            } else if (response.status_code() == status_codes::TooManyRequests) {
                std::cerr << "Rate limit exceeded" << std::endl;
                if (pImpl_->errorCallback_) {
                    pImpl_->errorCallback_("Rate limit exceeded");
                }
            } else {
                std::cerr << "HTTP error: " << response.status_code() << std::endl;
                if (pImpl_->errorCallback_) {
                    pImpl_->errorCallback_("HTTP " + std::to_string(response.status_code()));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in getCurrentTrack: " << e.what() << std::endl;
            if (pImpl_->errorCallback_) {
                pImpl_->errorCallback_(e.what());
            }
        }
    }).detach();
}

void SpotifyAPI::controlPlayback(PlayBackAction action, std::function<void(bool)> callback) const {
    if (pImpl_->accessToken.empty()) {
        std::cerr << "Cannot control playback: not authenticated" << std::endl;
        if (callback) callback(false);
        return;
    }

    std::thread([this, action, callback]() {
        try {
            http_client client("https://api.spotify.com/v1");
            std::string endpoint;
            method method;

            switch (action) {
                case PlayBackAction::PLAY:
                    method = methods::PUT;
                    endpoint = "/me/player/play";
                    std::cout << "Sending play command" << std::endl;
                    break;

                case PlayBackAction::PAUSE:
                    method = methods::PUT;
                    endpoint = "/me/player/pause";
                    std::cout << "Sending pause command" << std::endl;
                    break;

                case PlayBackAction::NEXT:
                    method = methods::POST;
                    endpoint = "/me/player/next";
                    std::cout << "Sending next track command" << std::endl;
                    break;

                case PlayBackAction::PREVIOUS:
                    method = methods::POST;
                    endpoint = "/me/player/previous";
                    std::cout << "Sending previous track command" << std::endl;
                    break;

                default:
                    std::cerr << "Unknown playback action: " << static_cast<int>(action) << std::endl;
                    if (callback) callback(false);
                    return;
            }

            const auto request = std::make_unique<http_request>(method);
            request->set_request_uri(endpoint);
            request->headers().add("Authorization", "Bearer " + pImpl_->accessToken);
            request->headers().add("Content-Type", "application/json");

            if (method == methods::PUT && endpoint == "/me/player/play") {
                const json::value body;
                request->set_body(body);
            }

            const auto response = client.request(*request).get();

            const bool success = (response.status_code() == status_codes::OK ||
                          response.status_code() == status_codes::NoContent ||
                          response.status_code() == status_codes::Accepted);

            if (success) {
                std::cout << "Playback control successful" << std::endl;

                if (action != PlayBackAction::TOGGLE) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    getCurrentTrack(nullptr, nullptr);
                }
            } else {
                std::cerr << "Playback control failed with status: " << response.status_code() << std::endl;

                try {
                    const auto errorBody = response.extract_string().get();
                    std::cerr << "Error response: " << errorBody << std::endl;
                } catch (...) {
                    // Ignore errors in error extraction
                }
            }

            if (callback) {
                callback(success);
            }

        } catch (const std::exception& e) {
            std::cerr << "Exception in controlPlayback: " << e.what() << std::endl;
            if (callback) {
                callback(false);
            }
        }
    }).detach();
}


void SpotifyAPI::startPolling(std::chrono::seconds interval) const {
    if (pImpl_->polling) {
        std::cout << "Polling already started" << std::endl;
        return;
    }

    pImpl_->polling = true;
    pImpl_->pollingThread = std::thread([this, interval]() {
        std::cout << "Starting track polling every " << interval.count() << " seconds" << std::endl;

        while (pImpl_->polling) {
            try {
                getCurrentTrack(nullptr, nullptr);

                auto sleepTime = std::chrono::milliseconds(100);
                auto totalSleep = std::chrono::milliseconds(0);
                while (pImpl_->polling && totalSleep < interval) {
                    std::this_thread::sleep_for(sleepTime);
                    totalSleep += sleepTime;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error in polling thread: " << e.what() << std::endl;
                if (pImpl_->polling) {
                    std::this_thread::sleep_for(interval);
                }
            }
        }

        std::cout << "Polling thread stopped" << std::endl;
    });
}

void SpotifyAPI::stopPolling() const {
    if (pImpl_->polling) {
        std::cout << "Stopping track polling" << std::endl;
        pImpl_->polling = false;
        if (pImpl_->pollingThread.joinable()) {
            pImpl_->pollingThread.join();
        }
    }
}

void SpotifyAPI::getPlaybackState(std::function<void(const SpotifyTrack&)> callback) const {
    getCurrentTrack(nullptr, nullptr);
}

void SpotifyAPI::setVolume(int volumePercent, std::function<void(bool)> callback) const {
    if (pImpl_->accessToken.empty()) {
        if (callback) callback(false);
        return;
    }

    std::thread([this, volumePercent, callback]() {
        try {
            http_client client("https://api.spotify.com/v1");
            http_request request(methods::PUT);

            std::stringstream uri;
            uri << "/me/player/volume?volume_percent=" << volumePercent;
            request.set_request_uri(uri.str());
            request.headers().add("Authorization", "Bearer " + pImpl_->accessToken);

            const auto response = client.request(request).get();

            const bool success = (response.status_code() == status_codes::OK ||
                          response.status_code() == status_codes::NoContent);

            if (callback) {
                callback(success);
            }

            if (success) {
                std::cout << "Volume set to " << volumePercent << "%" << std::endl;
            } else {
                std::cerr << "Volume control failed: " << response.status_code() << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "Volume control error: " << e.what() << std::endl;
            if (callback) {
                callback(false);
            }
        }
    }).detach();
}

void SpotifyAPI::seekToPosition(int positionMs, const std::function<void(bool)>& callback) const {
    if (pImpl_->accessToken.empty()) {
        if (callback) callback(false);
        return;
    }

    std::thread([this, positionMs, callback]() {
        try {
            http_client client("https://api.spotify.com/v1");
            http_request request(methods::PUT);

            std::stringstream uri;
            uri << "/me/player/seek?position_ms=" << positionMs;
            request.set_request_uri(uri.str());
            request.headers().add("Authorization", "Bearer " + pImpl_->accessToken);

            const auto response = client.request(request).get();

            const bool success = (response.status_code() == status_codes::OK ||
                          response.status_code() == status_codes::NoContent);

            if (callback) {
                callback(success);
            }

            if (success) {
                std::cout << "Seeked to position: " << positionMs << "ms" << std::endl;
            } else {
                std::cerr << "Seek failed: " << response.status_code() << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "Seek error: " << e.what() << std::endl;
            if (callback) {
                callback(false);
            }
        }
    }).detach();
}

bool SpotifyAPI::isPolling() const {
    return pImpl_->polling;
}