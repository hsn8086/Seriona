import QtQuick
import QtQuick.Controls.Basic
import Seriona

Window {
    id: root

    default property alias content: rootPage.children

    flags: Qt.Popup | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    property int menuWidth: 160
    property string arrowDirection: "up"
    property Item targetItem: null
    property double lastClosedAt: 0
    property int lastHeight: 0
    property int anchoredX: 0
    property bool restoringX: false

    readonly property int arrowSize: 12
    readonly property int arrowOffset: 12
    readonly property int contentPadding: 4

    width: menuWidth + contentPadding * 2
    height: pageStack.implicitHeight + contentPadding * 2 + arrowOffset

    Behavior on height {
        NumberAnimation { duration: Theme.animationDuration; easing.type: Easing.OutCubic }
    }

    onHeightChanged: {
        if (visible && arrowDirection === "down" && lastHeight > 0)
            y -= height - lastHeight;
        lastHeight = height;
    }

    onXChanged: {
        if (visible && !restoringX && x !== anchoredX) {
            restoringX = true;
            x = anchoredX;
            restoringX = false;
        }
    }

    function pushPage(title, pageComponent) {
        pageStack.push(subMenuPageComponent, {
            "pageTitle": title,
            "pageComponent": pageComponent
        });
    }

    function popPage() {
        if (pageStack.depth > 1)
            pageStack.pop();
    }

    function resetPage() {
        while (pageStack.depth > 1)
            pageStack.pop(null, StackView.Immediate);
    }

    Item {
        id: backgroundLayer
        anchors.fill: parent
        opacity: root.visible ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: Theme.animationDuration; easing.type: Easing.OutCubic }
        }

        Rectangle {
            width: root.arrowSize + 2
            height: root.arrowSize + 2
            color: "#15FFFFFF"
            rotation: 45
            x: (parent.width - width) / 2
            y: root.arrowDirection === "up" ? root.arrowOffset - height / 2 - 1 : body.y + body.height - height / 2 + 1
        }

        Rectangle {
            id: body
            anchors.fill: parent
            anchors.topMargin: root.arrowDirection === "up" ? root.arrowOffset : 0
            anchors.bottomMargin: root.arrowDirection === "down" ? root.arrowOffset : 0
            color: Theme.mainColor
            radius: 8
            border.color: "#15FFFFFF"
            border.width: 1
        }

        Rectangle {
            width: root.arrowSize
            height: root.arrowSize
            color: Theme.mainColor
            rotation: 45
            x: (parent.width - width) / 2
            y: root.arrowDirection === "up" ? root.arrowOffset - height / 2 : body.y + body.height - height / 2
        }
    }

    StackView {
        id: pageStack
        x: root.contentPadding
        y: root.contentPadding + (root.arrowDirection === "up" ? root.arrowOffset : 0)
        width: root.menuWidth
        height: implicitHeight
        clip: true
        implicitHeight: currentItem ? currentItem.implicitHeight : 0
        initialItem: rootPage

        pushEnter: Transition {
            id: pushEnterTransition
            PropertyAnimation { target: pushEnterTransition.ViewTransition.item; property: "x"; from: pageStack.width; to: 0; duration: Theme.animationDuration; easing.type: Easing.OutCubic }
        }
        pushExit: Transition {
            id: pushExitTransition
            PropertyAnimation { target: pushExitTransition.ViewTransition.item; property: "x"; from: 0; to: -pageStack.width; duration: Theme.animationDuration; easing.type: Easing.OutCubic }
        }
        popEnter: Transition {
            id: popEnterTransition
            PropertyAnimation { target: popEnterTransition.ViewTransition.item; property: "x"; from: -pageStack.width; to: 0; duration: Theme.animationDuration; easing.type: Easing.OutCubic }
        }
        popExit: Transition {
            id: popExitTransition
            PropertyAnimation { target: popExitTransition.ViewTransition.item; property: "x"; from: 0; to: pageStack.width; duration: Theme.animationDuration; easing.type: Easing.OutCubic }
        }

        Column {
            id: rootPage

            property var menu: root

            width: pageStack.width
            spacing: 0

            StackView.onActivated: x = 0
        }

        Component {
            id: subMenuPageComponent

            Column {
                id: subMenuPage

                property var menu: root
                property string pageTitle: ""
                property Component pageComponent: null

                width: pageStack.width
                spacing: 0

                StackView.onActivated: x = 0

                Item {
                    width: parent.width
                    height: 40

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 6
                        color: backMouseArea.containsMouse ? Theme.hoverColor : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: Theme.animationDuration }
                        }
                    }

                    Text {
                        text: qsTr("‹")
                        color: Theme.textColor
                        font.pixelSize: 22
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: subMenuPage.pageTitle
                        color: Theme.textColor
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        anchors.fill: parent
                        anchors.leftMargin: 38
                        anchors.rightMargin: 38
                    }

                    MouseArea {
                        id: backMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.popPage()
                    }
                }

                Loader {
                    width: parent.width
                    sourceComponent: subMenuPage.pageComponent
                }
            }
        }
    }

    function showAtTarget() {
        if (!targetItem || !targetItem.Window.window)
            return;

        // 显示前重置页面栈（替代 onVisibleChanged 中的重置）：
        // 窗口销毁过程中 visible 变化会触发绑定，此时 StackView 内部
        // 视图正在销毁，pop() 会访问已删除的过渡 item 导致崩溃（SEGV）。
        root.resetPage();

        root.transientParent = targetItem.Window.window;

        var topCenter = targetItem.mapToGlobal(targetItem.width / 2, 0);
        var bottomCenter = targetItem.mapToGlobal(targetItem.width / 2, targetItem.height);
        root.anchoredX = Math.round(topCenter.x - root.width / 2);
        root.x = root.anchoredX;
        root.y = arrowDirection === "up"
                ? Math.round(bottomCenter.y + 12)
                : Math.round(topCenter.y - root.height - 12);
        root.lastHeight = root.height;
        root.show();
        root.requestActivate();
    }

    function toggle() {
        if (!root.visible && Date.now() - lastClosedAt < 150)
            return;

        if (root.visible) {
            root.close();
        } else {
            root.showAtTarget();
        }
    }

    onVisibleChanged: {
        if (!visible) {
            // 不要在这里重置页面栈：窗口销毁（应用退出）时 setVisible(false)
            // 会触发本处理器，此时对 StackView 调用 pop() 会在过渡机制中
            // 访问已销毁的 QObject（QQuickPropertyAnimation::createTransitionActions
            // -> ExternalRefCountData::getAndRef），导致 SIGSEGV。
            // 页面栈重置已移至 showAtTarget()。
            lastClosedAt = Date.now();
        }
    }

    onActiveChanged: {
        if (!active && visible)
            close();
    }
}
