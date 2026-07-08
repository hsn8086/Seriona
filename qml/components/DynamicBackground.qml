import QtQuick
import Qt5Compat.GraphicalEffects
import Seriona

Item {
    id: root
    anchors.fill: parent

    required property PlaybackController playbackController

    property color color0: playbackController.gradientColor0
    property color color1: playbackController.gradientColor1
    property color color2: playbackController.gradientColor2

    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
    }

    LinearGradient {
        anchors.fill: parent
        start: Qt.point(0, 0)
        end: Qt.point(width, height)
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(root.color0.r, root.color0.g, root.color0.b, 0.55)
            }
            GradientStop {
                position: 0.7071
                color: "transparent"
            }
        }
    }

    LinearGradient {
        anchors.fill: parent
        start: Qt.point(width, 0)
        end: Qt.point(0, height)
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(root.color1.r, root.color1.g, root.color1.b, 0.55)
            }
            GradientStop {
                position: 0.7071
                color: "transparent"
            }
        }
    }

    LinearGradient {
        anchors.fill: parent
        start: Qt.point(0, height)
        end: Qt.point(width, 0)
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(root.color2.r, root.color2.g, root.color2.b, 0.55)
            }
            GradientStop {
                position: 0.7071
                color: "transparent"
            }
        }
    }

    Behavior on color0 {
        ColorAnimation {
            duration: Theme.colorTransitionDuration
        }
    }
    Behavior on color1 {
        ColorAnimation {
            duration: Theme.colorTransitionDuration
        }
    }
    Behavior on color2 {
        ColorAnimation {
            duration: Theme.colorTransitionDuration
        }
    }
}
