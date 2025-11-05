//
// Created by karpen on 10/28/25.
//

#ifndef SPOTIFYOVERLAY_CONFIGMANAGER_H
#define SPOTIFYOVERLAY_CONFIGMANAGER_H

#pragma once

#include <string>
#include "Types.h"

class ConfigManager {
public:
    static ConfigManager& getInstance();

    bool loadConfig();
    [[nodiscard]] bool saveConfig() const;
    bool loadTokens(AuthTokens& tokens) const;
    bool saveTokens(const AuthTokens& tokens);

    [[nodiscard]] std::string getClientId() const { return clientId_; }
    [[nodiscard]] std::string getClientSecret() const { return clientSecret_; }
    void setCredentials(const std::string& clientId, const std::string& clientSecret);

private:
    ConfigManager() = default;

    std::string configPath_ = "config.ini";
    std::string tokensPath_ = "tokens.json";

    std::string clientId_;
    std::string clientSecret_;
};

#endif //SPOTIFYOVERLAY_CONFIGMANAGER_H