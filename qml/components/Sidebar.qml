import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects
import Seriona

Item {
    id: root
    width: Theme.sidebarWidth

    signal closeClicked()

    // Properties to control shadow visibility
    property bool isDockCapable: false
    property bool isSidebarOpen: false

    RectangularGlow {
        id: shadow
        anchors.fill: contentRect
        glowRadius: 10
        spread: 0.2
        color: "#80000000"
        visible: !root.isDockCapable && root.isSidebarOpen
        z: -1
    }

    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: Theme.sidebarBackgroundColor

        // Right border line
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: "#15FFFFFF"
            z: 2
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top Bar
            Rectangle {
                Layout.fillWidth: true
                height: 50
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    anchors.rightMargin: 15
                    spacing: 8

                    StyleButton {
                        iconSource: "qrc:/qt/qml/Seriona/qml/assets/arrow_back.svg"
                        buttonWidth: 40
                        buttonHeight: 40
                        iconSize: 20
                        enabled: pageStack.depth > 1
                        opacity: enabled ? 1.0 : 0.3
                        Behavior on opacity { NumberAnimation { duration: Theme.animationDuration } }
                        onClicked: pageStack.pop()
                    }

                    StyleButton {
                        iconSource: "qrc:/qt/qml/Seriona/qml/assets/search.svg"
                        buttonWidth: 40
                        buttonHeight: 40
                        iconSize: 20
                    }

                    Text {
                        Layout.fillWidth: true
                        text: pageStack.currentItem ? pageStack.currentItem.folderName : qsTr("My Music")
                        color: Theme.textColor
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    StyleButton {
                        id: sidebarMoreBtn
                        iconSource: "qrc:/qt/qml/Seriona/qml/assets/more_vert.svg"
                        buttonWidth: 40
                        buttonHeight: 40
                        iconSize: 20
                        onClicked: sidebarMenu.open()

                        BubbleMenu {
                            id: sidebarMenu
                            targetItem: sidebarMoreBtn
                            
                            x: (sidebarMoreBtn.width - width) / 2
                            y: sidebarMoreBtn.height + 12

                            MenuItem { text: qsTr("Sort by Name") }
                            MenuItem { text: qsTr("Sort by Date") }
                            MenuItem { text: qsTr("Refresh") }
                        }
                    }

                    StyleButton {
                        iconSource: "qrc:/qt/qml/Seriona/qml/assets/close.svg"
                        buttonWidth: 40
                        buttonHeight: 40
                        iconSize: 20
                        onClicked: root.closeClicked()
                    }
                }

                // Bottom separator for top bar
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: "#10FFFFFF"
                }
            }

            StackView {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                initialItem: playlistPageComponent

                pushEnter: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: pageStack.width
                        to: 0
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                pushExit: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: 0
                        to: -pageStack.width
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                popEnter: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: -pageStack.width
                        to: 0
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
                popExit: Transition {
                    PropertyAnimation {
                        property: "x"
                        from: 0
                        to: pageStack.width
                        duration: Theme.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        // Floating Action Button (FAB)
        StyleButton {
            id: fab
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 15
            buttonWidth: 40
            buttonHeight: 40
            iconSize: 20
            iconSource: "qrc:/qt/qml/Seriona/qml/assets/my_location.svg"
            baseColor: Theme.accentColor
            textColor: "white"
            z: 10

            onClicked: console.log("Locate playing song")

            // Shadow for FAB
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                horizontalOffset: 0
                verticalOffset: 4
                radius: 8
                samples: 17
                color: "#80000000"
            }
        }
    }

    Component {
        id: playlistPageComponent
        ListView {
            id: playlistView
            property string folderName: "My Music"
            property var pageModel: mockModel
            model: pageModel
            spacing: 2
            topMargin: Theme.paddingMedium
            bottomMargin: 80 // Space for FAB
            clip: true

            delegate: ItemDelegate {
                id: delegate
                width: playlistView.width
                topPadding: 8
                bottomPadding: 8
                leftPadding: 15
                rightPadding: 15

                onClicked: {
                    if (model.type === "folder") {
                        pageStack.push(playlistPageComponent, { "folderName": model.name, "pageModel": mockSubModel })
                    } else {
                        console.log("Play " + model.title)
                    }
                }

                background: Rectangle {
                    color: delegate.visualFocus || delegate.hovered ? Theme.hoverColor : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                contentItem: RowLayout {
                    spacing: 12

                    // Left: Thumbnail/Cover
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 44
                        height: 44
                        radius: 8
                        color: model.type === "folder" ? Theme.accentColor : Theme.mainColor
                        clip: true

                        Image {
                            id: folderThumbIcon
                            anchors.fill: parent
                            anchors.margins: model.type === "folder" ? 10 : 0
                            source: model.type === "folder" ? "qrc:/qt/qml/Seriona/qml/assets/folder.svg" : ""
                            fillMode: Image.PreserveAspectFit
                            visible: false
                        }
                        
                        ColorOverlay {
                            anchors.fill: folderThumbIcon
                            source: folderThumbIcon
                            color: "white"
                            visible: model.type === "folder"
                        }

                        // Placeholder for cover art
                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "#20FFFFFF"
                            visible: model.type === "file"
                            Text {
                                anchors.centerIn: parent
                                text: "♫"
                                color: "white"
                                font.pixelSize: 22
                            }
                        }
                    }

                    // Right: Info
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 1

                        // Top: Name/Title
                        Text {
                            Layout.fillWidth: true
                            text: model.type === "file" ? model.title : model.name
                            color: Theme.textColor
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        // Middle: Artist-Album / Parent Folder
                        Text {
                            Layout.fillWidth: true
                            text: model.type === "file" ? (model.artist + " - " + model.album) : model.parentName
                            color: Theme.secondaryTextColor
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        // Bottom: Stream Info / Folder Stats
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            // File specific info
                            RowLayout {
                                visible: model.type === "file"
                                spacing: 4
                                Text {
                                    text: model.duration || ""
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                    visible: model.format !== undefined
                                }
                                Text {
                                    text: model.format || ""
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                    visible: model.sampleRate > 44100
                                }
                                Text {
                                    text: (model.sampleRate / 1000) + "kHz"
                                    color: Theme.accentColor
                                    font.pixelSize: 10
                                    visible: model.sampleRate > 44100
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                    visible: model.bitDepth > 16
                                }
                                Text {
                                    text: model.bitDepth + "bit"
                                    color: Theme.accentColor
                                    font.pixelSize: 10
                                    visible: model.bitDepth > 16
                                }
                            }

                            // Folder specific info
                                RowLayout {
                                    visible: model.type === "folder"
                                    spacing: 6
                                    Item {
                                        Layout.preferredWidth: 10
                                        Layout.preferredHeight: 10
                                        Image {
                                            id: musicNoteIcon
                                            anchors.fill: parent
                                            source: "qrc:/qt/qml/Seriona/qml/assets/music_note.svg"
                                            sourceSize: Qt.size(10, 10)
                                            fillMode: Image.PreserveAspectFit
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.fill: musicNoteIcon
                                            source: musicNoteIcon
                                            color: Theme.secondaryTextColor
                                            opacity: 0.7
                                        }
                                    }
                                    Text {
                                    text: model.songCount + " Songs"
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: "|"
                                    color: "#30FFFFFF"
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: model.duration || ""
                                    color: Theme.secondaryTextColor
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }

                // Small folder icon in bottom-right for folders
                Image {
                    id: folderBadgeIcon
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 4
                    source: "qrc:/qt/qml/Seriona/qml/assets/folder.svg"
                    sourceSize: Qt.size(12, 12)
                    visible: false
                }
                ColorOverlay {
                    anchors.fill: folderBadgeIcon
                    source: folderBadgeIcon
                    color: "white"
                    opacity: 0.3
                    visible: model.type === "folder"
                }
            }
        }
    }

    ListModel {
        id: mockSubModel
        ListElement { type: "file"; title: "Sub Song 1"; artist: "Artist A"; album: "Album X"; duration: "03:45"; format: "FLAC"; sampleRate: 44100; bitDepth: 16 }
        ListElement { type: "file"; title: "Sub Song 2"; artist: "Artist B"; album: "Album Y"; duration: "04:20"; format: "MP3"; sampleRate: 44100; bitDepth: 16 }
    }

    ListModel {
        id: mockModel
        ListElement { type: "folder"; name: "Hi-Res Collection"; parentName: "Music"; songCount: 128; duration: "12:45:30" }
        ListElement { type: "file"; title: "Stairway to Heaven"; artist: "Led Zeppelin"; album: "Led Zeppelin IV"; duration: "08:02"; format: "FLAC"; sampleRate: 96000; bitDepth: 24 }
        ListElement { type: "file"; title: "Bohemian Rhapsody"; artist: "Queen"; album: "A Night at the Opera"; duration: "05:55"; format: "WAV"; sampleRate: 192000; bitDepth: 24 }
        ListElement { type: "folder"; name: "Rock Classics"; parentName: "Music"; songCount: 45; duration: "03:12:00" }
        ListElement { type: "file"; title: "Imagine"; artist: "John Lennon"; album: "Imagine"; duration: "03:03"; format: "MP3"; sampleRate: 44100; bitDepth: 16 }
        ListElement { type: "file"; title: "Hotel California"; artist: "Eagles"; album: "Hotel California"; duration: "06:30"; format: "FLAC"; sampleRate: 48000; bitDepth: 24 }
        ListElement { type: "folder"; name: "Jazz Essentials"; parentName: "Music"; songCount: 32; duration: "02:45:15" }
    }
}

