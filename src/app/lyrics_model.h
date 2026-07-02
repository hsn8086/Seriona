#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QQmlEngine>
#include <QStringList>

namespace Seriona::App {

class LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(bool showTranslation READ showTranslation WRITE setShowTranslation NOTIFY showTranslationChanged)
    Q_PROPERTY(QString lyricDelimiter READ lyricDelimiter WRITE setLyricDelimiter NOTIFY lyricDelimiterChanged)
    Q_PROPERTY(bool advancing READ advancing WRITE setAdvancing NOTIFY advancingChanged)
    QML_ELEMENT

public:
    enum Role {
        RawLineRole = Qt::UserRole + 1,
        DisplayLineRole,
        TranslationRole,
        CurrentRole
    };
    Q_ENUM(Role)

    explicit LyricsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const;
    void setCurrentIndex(int index);

    bool showTranslation() const;
    void setShowTranslation(bool showTranslation);

    QString lyricDelimiter() const;
    void setLyricDelimiter(const QString &delimiter);

    bool advancing() const;
    void setAdvancing(bool advancing);

    // future backend hook: sync current lyric row with playback progress and line selection.
    Q_INVOKABLE void selectLyric(int index);
    // future backend hook: toggle lyric translation preference from settings/backend state.
    Q_INVOKABLE void toggleTranslation();
    // future backend hook: advance the highlighted lyric row from playback progress updates.
    Q_INVOKABLE void advanceLyric();

signals:
    void currentIndexChanged();
    void showTranslationChanged();
    void lyricDelimiterChanged();
    void advancingChanged();

private:
    QString displayLine(const QString &line) const;
    QString translationLine(const QString &line) const;
    void emitAllLyricsChanged(const QList<int> &roles);
    void emitCurrentRoleChanged(int index);

    QStringList m_lines;
    int m_currentIndex = 0;
    bool m_showTranslation = true;
    QString m_lyricDelimiter = QStringLiteral(" / ");
    bool m_advancing = false;
    QTimer m_advanceTimer;
};

}
