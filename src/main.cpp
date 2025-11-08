//
// Created by karpen on 11/5/25.
//

#include <QTimer>
#include <iostream>

#include "TrackOverlay.h"
#include "AuthManager.h"
#include "ConfigManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    std::cout << "Starting Spotify Overlay..." << std::endl;

    TrackOverlay overlay;
    std::cout << "Overlay created" << std::endl;

    overlay.setAttribute(Qt::WA_QuitOnClose, true);

    overlay.show();
    std::cout << "Overlay shown" << std::endl;

    auto& config = ConfigManager::getInstance();
    if(!config.loadConfig()) {
        std::cerr << "Error: Failed to load configuration!" << std::endl;

        SpotifyTrack errorTrack;
        errorTrack.name = "Configuration Error";
        errorTrack.artist = "Check config.ini";
        overlay.updateTrackInfo(errorTrack);

        return app.exec();
    }

    std::cout << "Configuration loaded" << std::endl;

    QTimer::singleShot(100, [&overlay]() {
        std::cout << "Starting Spotify initialization..." << std::endl;

        try {
            auto& config = ConfigManager::getInstance();
            AuthManager authManager(config.getClientId(), config.getClientSecret());

            if(!authManager.isAuthenticated()) {
                std::cout << "Not authenticated, starting authentication..." << std::endl;
                if(!authManager.authenticate()) {
                    std::cerr << "Authentication failed!" << std::endl;

                    SpotifyTrack authError;
                    authError.name = "Auth Failed";
                    authError.artist = "Check credentials";
                    overlay.updateTrackInfo(authError);
                    return;
                }
                std::cout << "Authentication successful!" << std::endl;
            } else {
                std::cout << "Already authenticated" << std::endl;
            }

            const auto& tokens = authManager.getTokens();
            overlay.setAccessToken(tokens.accessToken);
            overlay.startPolling(3);

            std::cout << "Spotify initialization completed" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Exception during Spotify init: " << e.what() << std::endl;
        }
    });

    std::cout << "Starting application event loop..." << std::endl;
    int result = app.exec();
    std::cout << "Application event loop finished with code: " << result << std::endl;

    return result;
}