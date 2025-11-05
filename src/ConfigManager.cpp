//
// Created by karpen on 10/28/25.
//

#include "../include/ConfigManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ConfigManager &ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig() {
    try {
        std::ifstream file(configPath_);
        if (!file.is_open()) {
            clientId_ = "you_clinet_id";
            clientSecret_ = "you_client_secret";
            return saveConfig();
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                if (key == "client.id") clientId_ = value;
                else if (key == "client.secret") clientSecret_ = value;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::saveConfig() const {
    try {
        std::ofstream file(configPath_);
        if (!file.is_open()) return false;

        file << "# Spotify API Configuration\n";
        file << "client.id=" << clientId_ << "\n";
        file << "client.secret=" << clientSecret_ << "\n";

        return true;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::loadTokens(AuthTokens &tokens) const {
    try {
        std::ifstream file (tokensPath_);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        tokens.accessToken = j.value("access_token", "");
        tokens.refreshToken = j.value("refresh_token", "");

        const auto lastUpdated = j.value("last_updated", 0);
        tokens.lastUpdate = std::chrono::system_clock::from_time_t(lastUpdated);

        return tokens.isValid();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::saveTokens(const AuthTokens &tokens) {
    try {
        json j;
        j["access_token"] = tokens.accessToken;
        j["refresh_token"] = tokens.refreshToken;
        j["last_updated"] = std::chrono::system_clock::to_time_t(tokens.lastUpdate);

        std::ofstream file(tokensPath_);
        if (!file.is_open()) return false;

        file << j.dump(4);

        return true;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::setCredentials(const std::string &clientId, const std::string &clientSecret) {
    clientId_ = clientId;
    clientSecret_ = clientSecret;

    if (!saveConfig()) {
        std::cerr << "ERROR::CONFIGMANAGER::CONFIG_SAVE_ERROR" << std::endl;
    }
}

