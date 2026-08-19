#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace Seriona::App {

// 播放次数/星级轻量持久化（需求 8 / T16）：
// QSettings 按 trackId 存储，无后端依赖（纯前端本地数据，不进入扫描器/后端数据库）。
// - playCount：歌曲每次"开始播放"（轨道切换/PlaybackEnded 后续播）时经 recordPlayback 自增；
//   无记录 = 0。
// - rating：用户可点星设置（1..5），0 = 清除评级（未评级，默认不显示半星）。
// 正式 QML 经 appFacade.trackStats 访问；T14 详情窗口据此显示/编辑星级。
class TrackStatsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TrackStatsController is owned by AppFacade")

public:
    explicit TrackStatsController(QObject *parent = nullptr);

    // 播放次数（无记录 = 0）
    Q_INVOKABLE int playCountFor(const QString &trackId) const;
    // 播放事件：该曲目自增一次并立即持久化（PlaybackEnded / 轨道切换的计数点）
    Q_INVOKABLE void recordPlayback(const QString &trackId);

    // 星级（无记录 = 0 = 未评级）
    Q_INVOKABLE int ratingFor(const QString &trackId) const;
    // 点星设置（钳制到 0..5；0 清除评级）
    Q_INVOKABLE void setRating(const QString &trackId, int rating);

signals:
    void playCountChanged(const QString &trackId, int count);
    void ratingChanged(const QString &trackId, int rating);

private:
    bool validTrackId(const QString &trackId) const;
};

}
