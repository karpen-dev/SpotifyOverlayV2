//
// Created by karpen on 11/5/25.
//

#include "TrackOverlay.h"
#include "SpotifyAPI.h"
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>
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
    isDragging(false),
    dragStartPosition()
{
    std::cout << "TrackOverlay constructor started" << std::endl;

    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::Tool |
                   Qt::X11BypassWindowManagerHint);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setStyleSheet(R"(
        TrackOverlay {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(30, 30, 30, 230),
                stop:1 rgba(20, 20, 20, 230));
            color: white;
            border-radius: 12px;
            border: 1px solid rgba(255, 255, 255, 50);
        }
    )");

    auto shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 160));
    shadowEffect->setOffset(0, 4);
    setGraphicsEffect(shadowEffect);

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &TrackOverlay::onImageDownloaded);

    mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    albumArtLabel = new QLabel(this);
    albumArtLabel->setFixedSize(64, 64);
    albumArtLabel->setStyleSheet(R"(
        QLabel {
            border-radius: 8px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(60, 60, 60, 255),
                stop:1 rgba(40, 40, 40, 255));
            border: 1px solid rgba(255, 255, 255, 30);
        }
    )");
    albumArtLabel->setAlignment(Qt::AlignCenter);
    albumArtLabel->setPixmap(getDefaultAlbumArt());

    textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);
    textLayout->setContentsMargins(0, 0, 0, 0);

    trackLabel = new QLabel("No track playing", this);
    artistLabel = new QLabel("--", this);

    trackLabel->setStyleSheet(R"(
        QLabel {
            font-weight: 600;
            font-size: 14px;
            color: white;
            background: transparent;
            border: none;
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
            border: none;
            padding: 0;
            margin: 0;
        }
    )");

    trackLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    artistLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    trackLabel->setWordWrap(true);
    trackLabel->setMaximumWidth(220);
    trackLabel->setMinimumWidth(220);

    textLayout->addWidget(trackLabel);
    textLayout->addWidget(artistLabel);
    textLayout->addStretch();

    mainLayout->addWidget(albumArtLabel);
    mainLayout->addLayout(textLayout);

    setFixedSize(320, 88);

    if (QApplication::primaryScreen()) {
        auto screenGeometry = QApplication::primaryScreen()->availableGeometry();
        move(screenGeometry.right() - width() - 20, 20);
    }

    setCursor(Qt::OpenHandCursor);

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
        setStyleSheet(R"(
            TrackOverlay {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 rgba(50, 90, 140, 230),
                    stop:1 rgba(40, 70, 120, 230));
                color: white;
                border-radius: 12px;
                border: 1px solid rgba(100, 150, 255, 100);
            }
        )");
    } else {
        setStyleSheet(R"(
            TrackOverlay {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 rgba(30, 30, 30, 230),
                    stop:1 rgba(20, 20, 20, 230));
                color: white;
                border-radius: 12px;
                border: 1px solid rgba(255, 255, 255, 50);
            }
        )");
    }

    update();
    repaint();

    std::cout << "=== updateTrackInfo completed ===" << std::endl;
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

void TrackOverlay::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        dragStartPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();

        setCursor(Qt::ClosedHandCursor);
    } else {
        QWidget::mousePressEvent(event);
    }
}

void TrackOverlay::mouseMoveEvent(QMouseEvent *event) {
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPos = event->globalPosition().toPoint() - dragStartPosition;

        QRect screenGeometry = QApplication::primaryScreen()->availableGeometry();
        newPos.setX(qMax(0, qMin(newPos.x(), screenGeometry.width() - width())));
        newPos.setY(qMax(0, qMin(newPos.y(), screenGeometry.height() - height())));

        move(newPos);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void TrackOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void TrackOverlay::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
}