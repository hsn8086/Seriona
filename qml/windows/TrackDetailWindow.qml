import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Seriona

// 曲目/文件夹详情窗口（T14）：数据来自 Sidebar delegate 的 model role 字段（entryData），
// 播放次数/星级经 appFacade.trackStats（T16 数据层，按 trackId 持久化）。
// 星级只读显示 + 可点击编辑（setRating）；无记录显示 0 次 / 未评级。
Window {
    id: root
    objectName: "trackDetailWindow"

    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    width: 380
    height: 600

    required property AppFacade appFacade
    // Sidebar delegate 收集的条目数据（与 LibraryModel role 同源，不新增后端调用）
    property var entryData: ({})
    readonly property bool isFolder: !!root.entryData && !!root.entryData.isFolder
    readonly property string trackId: !!root.entryData && root.entryData.trackId ? root.entryData.trackId : ""
    readonly property int playCount: root.trackId.length > 0 ? appFacade.trackStats.playCountFor(root.trackId) : 0
    readonly property int rating: root.trackId.length > 0 ? appFacade.trackStats.ratingFor(root.trackId) : 0
    readonly property string entryTitle: !!root.entryData && root.entryData.title ? root.entryData.title : ""
    readonly property string entryName: !!root.entryData && root.entryData.name ? root.entryData.name : ""
    readonly property string entryArtist: !!root.entryData && root.entryData.artist ? root.entryData.artist : ""
    readonly property string entryAlbum: !!root.entryData && root.entryData.album ? root.entryData.album : ""
    readonly property string entryArtwork: !!root.entryData && root.entryData.artworkSource ? root.entryData.artworkSource : ""
    readonly property string formatText: {
        var parts = [];
        if (!!root.entryData && root.entryData.format && root.entryData.format.length > 0)
            parts.push(root.entryData.format);
        if (!!root.entryData && root.entryData.sampleRate > 0)
            parts.push((root.entryData.sampleRate / 1000) + "kHz");
        if (!!root.entryData && root.entryData.bitDepth > 0)
            parts.push(root.entryData.bitDepth + "bit");
        return parts.join(" · ");
    }

    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: Theme.surfaceColor
        radius: Theme.radiusLarge
        border.color: Theme.borderColor
        border.width: 1
        focus: true

        Keys.onEscapePressed: root.close()

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Title Bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: "transparent"

                Text {
                    text: root.isFolder ? qsTr("文件夹详情") : qsTr("歌曲详情")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    anchors.centerIn: parent
                }

                StyleButton {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spacing12
                    anchors.verticalCenter: parent.verticalCenter
                    iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                    buttonWidth: 28
                    buttonHeight: 28
                    iconSize: 12
                    textColor: Theme.textSecondary
                    onClicked: root.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Theme.spacing24
                Layout.rightMargin: Theme.spacing24
                Layout.topMargin: Theme.spacing16
                Layout.bottomMargin: Theme.spacing16
                spacing: Theme.spacing12

                // Cover
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 132
                    radius: Theme.radiusMedium
                    color: root.isFolder ? Theme.accentColor : Theme.raisedSurfaceColor
                    antialiasing: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: root.isFolder ? 26 : 0
                        source: root.isFolder
                            ? "qrc:/qt/qml/Seriona/qml/assets/folder.svg"
                            : root.entryArtwork
                        sourceSize: root.isFolder ? Qt.size(80, 80) : Qt.size(132, 132)
                        fillMode: root.isFolder ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                        asynchronous: true
                        visible: !root.isFolder && status === Image.Ready

                        layer.enabled: !root.isFolder
                        layer.effect: OpacityMask {
                            maskSource: Rectangle {
                                width: 132
                                height: 132
                                radius: Theme.radiusMedium
                            }
                        }
                    }

                    ColorOverlay {
                        anchors.fill: parent
                        anchors.margins: root.isFolder ? 26 : 0
                        source: parent
                        color: Theme.textOnAccent
                        visible: root.isFolder
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusMedium
                        color: Theme.baseColor
                        visible: !root.isFolder && root.entryArtwork.length === 0

                        Text {
                            anchors.centerIn: parent
                            text: "♫"
                            color: Theme.textOnAccent
                            font.pixelSize: 40
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    text: root.isFolder
                        ? root.entryName
                        : root.entryTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSubtitle
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    visible: !root.isFolder
                    text: root.entryArtist.length > 0 && root.entryAlbum.length > 0
                        ? root.entryArtist + " - " + root.entryAlbum
                        : root.entryArtist + root.entryAlbum
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderSubtle
                }

                // 内容区可滚动（T14 修复 D）：长路径/高缩放下内容超高时纵向滚动，永不裁剪
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        columns: 2
                        columnSpacing: Theme.spacing16
                        rowSpacing: Theme.spacing8

                        Repeater {
                            model: root.isFolder
                                ? [
                                    {label: qsTr("上级目录"), value: root.entryData && root.entryData.parentName ? root.entryData.parentName : ""},
                                    {label: qsTr("包含歌曲"), value: root.entryData && root.entryData.songCount ? qsTr("%1 首").arg(root.entryData.songCount) : ""},
                                    {label: qsTr("总时长"), value: root.entryData && root.entryData.duration ? root.entryData.duration : ""}
                                  ]
                                : [
                                    {label: qsTr("艺术家"), value: root.entryArtist},
                                    {label: qsTr("专辑"), value: root.entryAlbum},
                                    {label: qsTr("年份"), value: root.entryData && root.entryData.year ? String(root.entryData.year) : "—"},
                                    {label: qsTr("时长"), value: root.entryData && root.entryData.duration ? root.entryData.duration : ""},
                                    {label: qsTr("格式"), value: root.formatText},
                                    {label: qsTr("文件路径"), value: root.entryData && root.entryData.path ? root.entryData.path : ""},
                                    {label: qsTr("播放次数"), value: qsTr("%1 次").arg(root.playCount)}
                                  ]

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacing8

                                Text {
                                    Layout.preferredWidth: 88
                                    text: modelData.label
                                    color: Theme.detailLabelColor
                                    font.pixelSize: Theme.fontCaption
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.value
                                    color: Theme.detailValueColor
                                    font.pixelSize: Theme.fontCaption
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                }
                            }
                        }
                    }
                }

                // 星级（可点击编辑；0 = 未评级）
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    visible: !root.isFolder

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("评分")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: Theme.spacing8

                        Repeater {
                            model: 5

                            Text {
                                id: starIcon
                                text: "★"
                                color: (index + 1) <= root.rating ? Theme.ratingColor : Theme.ratingUnselectedColor
                                font.pixelSize: Theme.fontHeading
                                scale: starArea.pressed ? 1.25 : (starArea.containsMouse ? 1.15 : 1.0)

                                Behavior on scale {
                                    NumberAnimation { duration: Theme.animationFast }
                                }
                                Behavior on color {
                                    ColorAnimation { duration: Theme.animationFast }
                                }

                                MouseArea {
                                    id: starArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appFacade.trackStats.setRating(root.trackId, index + 1)
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    visible: !root.isFolder && root.rating === 0
                    text: qsTr("未评级")
                    color: Theme.textDisabled
                    font.pixelSize: Theme.fontCaption
                }
            }
        }
    }
}

