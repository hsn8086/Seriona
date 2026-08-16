#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QStringList>
#include <QVector>

#include <chrono>

#ifndef SERIONA_HAS_BACKEND
#define SERIONA_HAS_BACKEND 0
#endif

#if SERIONA_HAS_BACKEND
#include "seriona/control/control_contracts.h"
#endif

namespace Seriona::App {

class LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(qreal playbackPosition READ playbackPosition WRITE setPlaybackPosition NOTIFY playbackPositionChanged)
    Q_PROPERTY(bool showTranslation READ showTranslation WRITE setShowTranslation NOTIFY showTranslationChanged)
    Q_PROPERTY(QStringList lyricDelimiters READ lyricDelimiters WRITE setLyricDelimiters NOTIFY lyricDelimitersChanged)
    Q_PROPERTY(QString lyricDelimiter READ lyricDelimiter NOTIFY lyricDelimitersChanged) // deprecated: 只读兼容属性，供 MainContent.qml 既有单值绑定编译；T7 迁移后移除
    QML_ELEMENT

public:
    enum Role {
        RawLineRole = Qt::UserRole + 1,
        DisplayLineRole,
        TranslationRole,
        CurrentRole,
        TimestampRole
    };
    Q_ENUM(Role)

    explicit LyricsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const;
    void setCurrentIndex(int index);

    qreal playbackPosition() const;
    void setPlaybackPosition(qreal position);

    bool showTranslation() const;
    void setShowTranslation(bool showTranslation);

    QStringList lyricDelimiters() const;
    void setLyricDelimiters(const QStringList &delimiters);

    // deprecated: 兼容属性，返回 lyricDelimiters 首项；MainContent.qml 迁移到 lyricDelimiters 后移除
    QString lyricDelimiter() const;

#if SERIONA_HAS_BACKEND
    void applyPlayerStateSnapshot(
        const seriona::control::PlayerStateSnapshot &snapshot,
        const seriona::control::LibraryStateSnapshot *library = nullptr);
#endif

    Q_INVOKABLE void selectLyric(int index);
    Q_INVOKABLE void toggleTranslation();

signals:
    void currentIndexChanged();
    void playbackPositionChanged();
    void showTranslationChanged();
    void lyricDelimitersChanged();

private:
    struct Line {
        std::chrono::milliseconds timestamp{0};
        QString text;
    };

    QString displayLine(const QString &line) const;
    QString translationLine(const QString &line) const;
    void clearLyrics();
    void applyMissingTrackSnapshot(const QString &trackId);
    void replaceLyrics(QVector<Line> lines, bool hasTimedLyrics);
    int currentIndexForPlaybackPosition() const;
    void syncCurrentIndexToPlaybackPosition();
    void emitAllLyricsChanged(const QList<int> &roles);
    void emitCurrentRoleChanged(int index);

    QVector<Line> m_lines;
    int m_currentIndex = 0;
    qreal m_playbackPosition = 0.0;
    bool m_hasTimedLyrics = false;
    bool m_showTranslation = true;
    QStringList m_lyricDelimiters = {QStringLiteral(" / ")};
    QString m_visibleTrackId;
};

}
