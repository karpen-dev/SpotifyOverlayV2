//
// Created by karpen on 11/5/25.
//

#ifndef SPOTIFYOVERLAY_TRACKOVERLAY_H
#define SPOTIFYOVERLAY_TRACKOVERLAY_H

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
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
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void paintEvent(QPaintEvent *event);

private slots:
    void onImageDownloaded(QNetworkReply* reply);

private:
    QLabel *albumArtLabel;
    QLabel *trackLabel;
    QLabel *artistLabel;
    QHBoxLayout *mainLayout;
    QVBoxLayout *textLayout;
    std::unique_ptr<SpotifyAPI> spotify_api_;
    QNetworkAccessManager *networkManager;

    bool isDragging = false;
    QPoint dragStartPosition;
    QPoint dragPosition;

    void loadAlbumArt(const std::string& imageUrl);
    QPixmap getDefaultAlbumArt();
};

#endif //SPOTIFYOVERLAY_TRACKOVERLAY_H