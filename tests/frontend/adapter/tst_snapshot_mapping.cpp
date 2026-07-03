#include <QObject>
#include <QString>
#include <QtTest/QTest>

namespace {
enum class FakePlaybackState {
    Playing,
    Paused,
};

struct FakePlayerSnapshot {
    FakePlaybackState state;
    QString title;
    QString artist;
    QString album;
    int positionMs;
    int durationMs;
};

struct QtFacingSnapshot {
    bool isPlaying;
    QString songTitle;
    QString artistName;
    QString albumName;
    QString currentPosition;
    QString totalDuration;
};

QString formatMilliseconds(int value)
{
    return QStringLiteral("%1:%2").arg(value / 60000).arg((value / 1000) % 60, 2, 10, QLatin1Char('0'));
}

QtFacingSnapshot mapFakeSnapshot(const FakePlayerSnapshot &snapshot)
{
    return {
        snapshot.state == FakePlaybackState::Playing,
        snapshot.title,
        snapshot.artist,
        snapshot.album,
        formatMilliseconds(snapshot.positionMs),
        formatMilliseconds(snapshot.durationMs),
    };
}
}

class SnapshotMappingTest : public QObject {
    Q_OBJECT

private slots:
    void mapsPlayingSnapshotToQtFacingProperties();
};

void SnapshotMappingTest::mapsPlayingSnapshotToQtFacingProperties()
{
    const FakePlayerSnapshot snapshot{
        FakePlaybackState::Playing,
        QStringLiteral("Seriona Echo"),
        QStringLiteral("Adapter Fixture"),
        QStringLiteral("Contract Smoke"),
        42000,
        185000,
    };

    const QtFacingSnapshot mapped = mapFakeSnapshot(snapshot);

    QCOMPARE(mapped.isPlaying, true);
    QCOMPARE(mapped.songTitle, QStringLiteral("Seriona Echo"));
    QCOMPARE(mapped.artistName, QStringLiteral("Adapter Fixture"));
    QCOMPARE(mapped.albumName, QStringLiteral("Contract Smoke"));
    QCOMPARE(mapped.currentPosition, QStringLiteral("0:42"));
    QCOMPARE(mapped.totalDuration, QStringLiteral("3:05"));
}

QTEST_GUILESS_MAIN(SnapshotMappingTest)

#include "tst_snapshot_mapping.moc"
