//
// Created by karpen on 11/5/25.
//

#include "TrackOverlay.h"
#include "SpotifyAPI.h"
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>
#include <QPushButton>
#include <QBuffer>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <iostream>

TrackOverlay::TrackOverlay(QWidget *parent) :
    QWidget(parent),
    albumArtLabel(nullptr),
    trackLabel(nullptr),
    artistLabel(nullptr),
    networkManager(nullptr),
    playPause(nullptr),
    nextTrack(nullptr),
    backTrack(nullptr),
    btnLayout(nullptr),
    isDragging(false),
    dragStartPosition()
{
    std::cout << "TrackOverlay constructor started" << std::endl;

    setWindowFlags(Qt::WindowStaysOnTopHint |
                   Qt::Tool |
                   Qt::X11BypassWindowManagerHint);

    setStyleSheet(R"(
        TrackOverlay {
            background: #141414;
            color: white;
        }
    )");

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &TrackOverlay::onImageDownloaded);

    mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 12, 12, 12);
    mainLayout->setSpacing(12);

    albumArtLabel = new QLabel(this);
    albumArtLabel->setFixedSize(64, 64);

    albumArtLabel->setAlignment(Qt::AlignCenter);
    albumArtLabel->setPixmap(getDefaultAlbumArt());

    textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);
    textLayout->setContentsMargins(0, 0, 0, 0);

    btnLayout = new QGridLayout();
    btnLayout->setSpacing(4);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    trackLabel = new QLabel("No track playing", this);
    artistLabel = new QLabel("--", this);

    playPause = new QPushButton("⏸", this);
    nextTrack = new QPushButton("⏵", this);
    backTrack = new QPushButton("⏴", this);

    trackLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    artistLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    trackLabel->setWordWrap(true);
    trackLabel->setMaximumWidth(220);
    trackLabel->setMinimumWidth(220);

    textLayout->addWidget(trackLabel);
    textLayout->addWidget(artistLabel);
    textLayout->addStretch();

    playPause->setMaximumWidth(30);
    playPause->setMaximumHeight(30);
    playPause->setHidden(true);

    nextTrack->setMaximumWidth(30);
    nextTrack->setMaximumHeight(30);
    nextTrack->setHidden(true);

    backTrack->setMaximumWidth(30);
    backTrack->setMaximumHeight(30);
    backTrack->setHidden(true);

    btnLayout->addWidget(playPause, 0, 1);
    btnLayout->addWidget(nextTrack, 0, 2);
    btnLayout->addWidget(backTrack, 0, 0);

    mainLayout->addWidget(albumArtLabel);
    mainLayout->addLayout(textLayout);
    mainLayout->addLayout(btnLayout);

    connect(playPause, &QPushButton::clicked, this, [this]() {
        if (isPlaying) {
            playPause->setText("⏸");
            spotify_api_->controlPlayback(PlayBackAction::PAUSE, nullptr);
        }
        else {
            playPause->setText("▶");

            spotify_api_->controlPlayback(PlayBackAction::PLAY, nullptr);
        }
    });

    connect(nextTrack, &QPushButton::clicked, this, [this]() {
        spotify_api_->controlPlayback(PlayBackAction::NEXT, nullptr);
    });

    connect(backTrack, &QPushButton::clicked, this, [this]() {
        spotify_api_->controlPlayback(PlayBackAction::PREVIOUS, nullptr);
    });

    setDefaultStyles();

    setFixedSize(330, 88);

    setAutoFillBackground(true);

    if (QApplication::primaryScreen()) {
        auto screenGeometry = QApplication::primaryScreen()->availableGeometry();
        move(screenGeometry.right() - width() - 20, 20);
    }

    std::cout << "TrackOverlay constructor completed" << std::endl;
}

TrackOverlay::~TrackOverlay() {
    std::cout << "TrackOverlay destructor called" << std::endl;
    if (spotify_api_) {
        spotify_api_->stopPolling();
    }
}

void TrackOverlay::updateTrackInfo(const SpotifyTrack &track) {
    std::cout << "=== updateTrackInfo called ===" << std::endl;
    std::cout << "Track: '" << track.name << "'" << std::endl;
    std::cout << "Artist: '" << track.artist << "'" << std::endl;
    std::cout << "Image URL: '" << track.imageUrl << "'" << std::endl;
    std::cout << "IsPlaying: " << track.isPlaying << std::endl;

    QString trackText = QString::fromStdString(track.name.empty() ? "No track" : track.name);
    QString artistText = QString::fromStdString(track.artist.empty() ? "Unknown artist" : track.artist);

    if (trackText.length() > 35) {
        trackText = trackText.left(32) + "...";
    }
    if (artistText.length() > 40) {
        artistText = artistText.left(37) + "...";
    }

    isPlaying = track.isPlaying;

    playPause->setHidden(false);
    nextTrack->setHidden(false);
    backTrack->setHidden(false);

    trackLabel->setText(trackText);
    artistLabel->setText(artistText);

    if (!track.imageUrl.empty()) {
        std::cout << "Loading album art from: " << track.imageUrl << std::endl;
        loadAlbumArt(track.imageUrl);
    } else {
        std::cout << "No image URL, using default album art" << std::endl;
        albumArtLabel->setPixmap(getDefaultAlbumArt());
    }

    if(track.isPlaying) {
        playPause->setText("⏸");

        setStyleSheet(R"(
            TrackOverlay {
                background: #141414;
                color: white;
            }
        )");
    } else {
        playPause->setText("▶");

        setStyleSheet(R"(
            TrackOverlay {
                background: #141414;
                color: white;
            }
        )");
    }

    update();
    repaint();

    std::cout << "=== updateTrackInfo completed ===" << std::endl;
}

void TrackOverlay::setAccessToken(const std::string &token) {
    std::cout << "setAccessToken called, token length: " << token.length() << std::endl;

    if (token.empty()) {
        std::cerr << "ERROR: Empty access token!" << std::endl;
        return;
    }

    if (!spotify_api_) {
        try {
            spotify_api_ = std::make_unique<SpotifyAPI>();
            std::cout << "SpotifyAPI created successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create SpotifyAPI: " << e.what() << std::endl;
            return;
        }
    }

    if (spotify_api_) {
        spotify_api_->setAccessToken(token);
        std::cout << "Access token set in SpotifyAPI" << std::endl;
    }
}

void TrackOverlay::startPolling(int intervalSeconds) {
    std::cout << "startPolling called with interval: " << intervalSeconds << " seconds" << std::endl;

    if (spotify_api_) {
        spotify_api_->setTrackCallback([this](const SpotifyTrack& track) {
            std::cout << "SpotifyAPI callback received track update" << std::endl;

            QTimer::singleShot(0, this, [this, track]() {
                std::cout << "Executing track update in main thread..." << std::endl;
                this->updateTrackInfo(track);
            });
        });

        spotify_api_->setErrorCallback([](const std::string& error) {
            std::cerr << "Spotify API Error: " << error << std::endl;
        });

        spotify_api_->startPolling(std::chrono::seconds(intervalSeconds));
        std::cout << "Polling started successfully" << std::endl;

    } else {
        std::cerr << "ERROR: Cannot start polling - SpotifyAPI not initialized" << std::endl;

        SpotifyTrack errorTrack;
        errorTrack.name = "API Not Ready";
        errorTrack.artist = "Check authentication";
        updateTrackInfo(errorTrack);
    }
}

void TrackOverlay::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    painter.fillPath(path, QColor(20, 20, 20));

    QWidget::paintEvent(event);
}

void TrackOverlay::onImageDownloaded(QNetworkReply* reply) {
    std::cout << "Image download completed" << std::endl;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();

        QPixmap albumArt;
        if (albumArt.loadFromData(imageData)) {
            QPixmap roundedArt(64, 64);
            roundedArt.fill(Qt::transparent);

            QPainter painter(&roundedArt);
            painter.setRenderHint(QPainter::Antialiasing);

            QPainterPath path;
            path.addRoundedRect(0, 0, 64, 64, 8, 8);
            painter.setClipPath(path);

            QPixmap scaledArt = albumArt.scaled(64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            painter.drawPixmap(0, 0, scaledArt);

            albumArtLabel->setPixmap(roundedArt);
            std::cout << "Album art loaded and scaled successfully" << std::endl;
        } else {
            std::cerr << "Failed to load image from downloaded data" << std::endl;
            albumArtLabel->setPixmap(getDefaultAlbumArt());
        }
    } else {
        std::cerr << "Failed to download album art: " << reply->errorString().toStdString() << std::endl;
        albumArtLabel->setPixmap(getDefaultAlbumArt());
    }

    reply->deleteLater();
}

void TrackOverlay::setDefaultStyles() const {
    albumArtLabel->setStyleSheet(R"(
        QLabel {
            background: #141414;
            margin-left: 5px;
        }
    )");

    playPause->setStyleSheet(R"(
        QPushButton {
            font-weight: 600;
            font-size: 16px;
            color: white;
            background: transparent;
            padding: 5px;
            margin: 0;
        }
    )");

    nextTrack->setStyleSheet(R"(
        QPushButton {
            font-weight: 600;
            font-size: 16px;
            color: white;
            background: transparent;
            padding: 5px;
            margin: 0;
        }
    )");

    backTrack->setStyleSheet(R"(
        QPushButton {
            font-weight: 600;
            font-size: 16px;
            color: white;
            background: transparent;
            padding: 5px;
            margin: 0;
        }
    )");

    trackLabel->setStyleSheet(R"(
        QLabel {
            font-weight: 600;
            font-size: 14px;
            color: white;
            background: transparent;
            padding: 0;
            margin: 0;
        }
    )");

    artistLabel->setStyleSheet(R"(
        QLabel {
            font-weight: 400;
            font-size: 12px;
            color: #b0b0b0;
            background: transparent;
            padding: 0;
            margin: 0;
        }
    )");
}

void TrackOverlay::loadAlbumArt(const std::string& imageUrl) {
    QString url = QString::fromStdString(imageUrl);

    if (!url.isEmpty()) {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::User, url);
        networkManager->get(request);
    } else {
        albumArtLabel->setPixmap(getDefaultAlbumArt());
    }
}

QPixmap TrackOverlay::getDefaultAlbumArt() {
    QPixmap defaultArt(64, 64);
    defaultArt.fill(Qt::transparent);

    QPainter painter(&defaultArt);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient gradient(0, 0, 0, 64);
    gradient.setColorAt(0, QColor(80, 80, 80));
    gradient.setColorAt(1, QColor(50, 50, 50));

    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(120, 120, 120), 1));
    painter.drawRoundedRect(1, 1, 62, 62, 8, 8);

    painter.setPen(QPen(QColor(200, 200, 200), 2));
    QFont font = painter.font();
    font.setPointSize(20);
    painter.setFont(font);
    painter.drawText(defaultArt.rect(), Qt::AlignCenter, "♪");

    return defaultArt;
}
