//
// Created by karpen on 10/28/25.
//

#ifndef SPOTIFYOVERLAY_AUTHMANAGER_H
#define SPOTIFYOVERLAY_AUTHMANAGER_H

#pragma once

#include <string>
#include <functional>
#include <memory>
#include "Types.h"

class AuthManager {
public:
    using AuthCallback = std::function<void(const AuthTokens&)>;

    AuthManager(const std::string& clientId, const std::string& clientSecret);
    ~AuthManager();

    bool authenticate();
    bool refreshTokens(const std::string& refreshToken);
    [[nodiscard]] bool isAuthenticated() const { return !tokens_.accessToken.empty(); }
    [[nodiscard]] const AuthTokens& getTokens() const { return tokens_; }
    void setAuthCallback(const AuthCallback &callback) { authCallback_ = callback; }

private:
    std::string  clientId_;
    std::string clientSecret_;
    AuthTokens tokens_;
    AuthCallback authCallback_;

    std::string redirectUri_ = "http://127.0.0.1:8888/callback";
    std::string scope_ = "user-read-currently-playing user-modify-playback-state";

    void startAuthServer() const;
    void stopAuthServer() const;
    [[nodiscard]] std::string buildAuthUrl() const;
    bool exchangeCodeForTokens(const std::string& code);

    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

#endif //SPOTIFYOVERLAY_AUTHMANAGER_H