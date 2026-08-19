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
        color: Theme.backgroundColor
        radius: Theme.borderRadius
        border.color: "#30FFFFFF"
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
                    color: Theme.textColor
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    anchors.centerIn: parent
                }

                StyleButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                    buttonWidth: 24
                    buttonHeight: 24
                    iconSize: 14
                    onClicked: root.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#30FFFFFF"
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                Layout.bottomMargin: 20
                spacing: 14

                // Cover
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 132
                    radius: 14
                    color: root.isFolder ? Theme.accentColor : Theme.mainColor
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
                                radius: 14
                            }
                        }
                    }

                    ColorOverlay {
                        anchors.fill: parent
                        anchors.margins: root.isFolder ? 26 : 0
                        source: parent
                        color: "white"
                        visible: root.isFolder
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 14
                        color: "#20FFFFFF"
                        visible: !root.isFolder && root.entryArtwork.length === 0

                        Text {
                            anchors.centerIn: parent
                            text: "♫"
                            color: "white"
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
                    color: Theme.textColor
                    font.pixelSize: 17
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
                    color: Theme.secondaryTextColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#15FFFFFF"
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
                        columnSpacing: 16
                        rowSpacing: 10

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
                                spacing: 8

                                Text {
                                    Layout.preferredWidth: 88
                                    text: modelData.label
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 12
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.value
                                    color: Theme.textColor
                                    font.pixelSize: 12
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
                    Layout.preferredHeight: 26
                    visible: !root.isFolder

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("评分")
                        color: Theme.secondaryTextColor
                        font.pixelSize: 12
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 6

                        Repeater {
                            model: 5

                            Text {
                                text: "★"
                                color: (index + 1) <= root.rating ? Theme.accentColor : "#40FFFFFF"
                                font.pixelSize: 22

                                MouseArea {
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
                    color: Theme.secondaryTextColor
                    font.pixelSize: 12
                }
            }
        }
    }
}
