//
// Created by karpen on 11/5/25.
//

#ifndef SPOTIFYOVERLAY_TRACKOVERLAY_H
#define SPOTIFYOVERLAY_TRACKOVERLAY_H

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QApplication>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <memory>

#include "SpotifyAPI.h"

class SpotifyAPI;
struct SpotifyTrack;

class TrackOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit TrackOverlay(QWidget *parent = nullptr);
    ~TrackOverlay() override;

    void updateTrackInfo(const SpotifyTrack& track);
    void setAccessToken(const std::string& token);
    void startPolling(int intervalSeconds = 5);

protected:

    void paintEvent(QPaintEvent *event);

private slots:
    void onImageDownloaded(QNetworkReply* reply);
    void setDefaultStyles() const;

private:
    QLabel *albumArtLabel;
    QLabel *trackLabel;
    QLabel *artistLabel;

    QHBoxLayout *mainLayout{};
    QVBoxLayout *textLayout;
    QGridLayout *btnLayout;

    QPushButton *playPause;
    QPushButton *nextTrack;
    QPushButton *backTrack;

    std::unique_ptr<SpotifyAPI> spotify_api_;
    QNetworkAccessManager *networkManager;

    bool isPlaying{};

    bool isDragging = false;
    QPoint dragStartPosition;
    QPoint dragPosition;

    void loadAlbumArt(const std::string& imageUrl);
    QPixmap getDefaultAlbumArt();
};

#endif //SPOTIFYOVERLAY_TRACKOVERLAY_H