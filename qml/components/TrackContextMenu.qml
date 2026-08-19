import QtQuick
import QtQuick.Controls.Basic
import Seriona

// 播放列表右键菜单（T14）：Sidebar delegate 右键弹出，含 详情 / 添加到下一首播放 /
// 从队列移除（命令层，队列视图上下文 T15 接入）/ 删除（确认弹窗 → appFacade.deleteTarget）。
// 菜单定位走 BubbleMenu.showAtGlobal（鼠标全局坐标，箭头向上）。
Item {
    id: root

    required property AppFacade appFacade

    // 队列视图右键上下文（T15）：true 时"从队列移除"菜单项可见；
    // 文件夹视图右键时保持 false（由 Sidebar 绑定 queueViewActive）。
    property bool queueContext: false

    // Sidebar delegate 右键时收集的条目数据（LibraryModel role 同源 + 文件路径）
    property var entryData: ({})

    // 打开右键菜单：data 为条目数据对象；globalX/globalY 为鼠标全局坐标。
    function openForEntry(data, globalX, globalY) {
        root.entryData = data;
        contextMenu.showAtGlobal(globalX, globalY);
    }

    // 定位锚点（BubbleMenu 需要 targetItem 提供 transientParent 与 geometry）
    Item {
        id: menuAnchor
        width: 1
        height: 1
        visible: false
    }

    BubbleMenu {
        id: contextMenu
        objectName: "trackContextMenuPopup"
        menuWidth: 170
        arrowDirection: "up"
        targetItem: menuAnchor

        BubbleMenuItem {
            id: detailItem
            text: qsTr("详情")
            onTriggered: {
                contextMenu.close();
                detailWindow.entryData = root.entryData;
                detailWindow.show();
            }
        }

        BubbleMenuItem {
            text: qsTr("添加到下一首播放")
            visible: !root.entryData.isFolder
            onTriggered: {
                contextMenu.close();
                root.appFacade.playNextTrack(root.entryData.trackId);
            }
        }

        BubbleMenuItem {
            // 队列视图右键上下文（T15）：queueContext（= Sidebar.queueViewActive）
            // 为 true 时可见；queueIndex 由队列条目右键传入（队列列表下标）。
            id: removeFromQueueItem
            objectName: "removeFromQueueItem"
            text: qsTr("从队列移除")
            visible: root.queueContext
            onTriggered: {
                contextMenu.close();
                root.appFacade.removeFromQueue(root.entryData.queueIndex);
            }
        }

        BubbleMenuItem {
            text: root.entryData.isFolder ? qsTr("删除文件夹") : qsTr("删除歌曲")
            onTriggered: {
                contextMenu.close();
                deleteDialog.targetName = root.entryData.isFolder ? root.entryData.name : root.entryData.title;
                deleteDialog.isFolder = root.entryData.isFolder;
                deleteDialog.trackCount = root.entryData.isFolder ? root.entryData.songCount : 0;
                deleteDialog.open();
            }
        }
    }

    ConfirmDeleteDialog {
        id: deleteDialog

        onConfirmed: {
            root.appFacade.deleteTarget(root.entryData.path, root.entryData.isFolder);
        }
    }

    TrackDetailWindow {
        id: detailWindow
        appFacade: root.appFacade
    }
}
