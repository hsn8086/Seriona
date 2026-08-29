import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Seriona

Item {
    id: root

    required property var navigationController
    required property AppFacade appFacade
    required property LibraryController libraryController

    readonly property bool scanRunning: libraryController.scanStatus === "running"
    readonly property bool scanError: libraryController.scanStatus === "error"
    readonly property string scanMessage: scanRunning
        ? (libraryController.totalSongCount > 0
            ? qsTr("正在扫描：%1 / %2").arg(libraryController.scannedSongCount).arg(libraryController.totalSongCount)
            : qsTr("正在扫描曲库…"))
        : scanError
            ? (libraryController.lastError.length > 0 ? libraryController.lastError : qsTr("曲库加载失败，请重试"))
            : qsTr("选择一个音乐文件夹开始构建曲库")

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceColor
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.paddingLarge * 2, 320)
        spacing: Theme.spacing16

        // 应用图标：直接展示，放大显示细节（mipmap 保证缩小/等比渲染平滑）。
        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 200
            source: "qrc:/qt/qml/Seriona/qml/assets/app-icon-256.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Seriona")
            color: Theme.textPrimary
            font.pixelSize: 30
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: root.scanMessage
            color: root.scanError ? Theme.dangerColor : Theme.textSecondary
            font.pixelSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Button {
            id: restoreButton
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: qsTr("恢复播放列表")
            enabled: !root.scanRunning
            hoverEnabled: true
            onClicked: root.appFacade.restorePlaylistFromStartup()

            contentItem: Text {
                text: restoreButton.text
                color: Theme.playButtonText
                font.pixelSize: Theme.fontTitle
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.radiusMedium
                color: restoreButton.pressed ? Qt.darker(Theme.playButtonBg, 1.2) : restoreButton.hovered ? Qt.darker(Theme.playButtonBg, 1.1) : Theme.playButtonBg
                Behavior on color { ColorAnimation { duration: Theme.animationFast } }
            }
        }

        Button {
            id: addFolderButton
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: qsTr("添加文件夹")
            enabled: !root.scanRunning
            hoverEnabled: true
            onClicked: startupFolderDialog.open()

            contentItem: Text {
                text: addFolderButton.text
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.radiusMedium
                color: addFolderButton.pressed ? Theme.pressedColor : addFolderButton.hovered ? Theme.hoverColor : Theme.baseColor
                border.color: Theme.borderColor
                border.width: 1
                Behavior on color { ColorAnimation { duration: Theme.animationFast } }
            }
        }
    }

    Platform.FolderDialog {
        id: startupFolderDialog
        title: qsTr("选择音乐文件夹")

        onAccepted: {
            if (root.appFacade.scanLibrary(folder))
                root.navigationController.addFolderFromStartup();
        }
    }
}

