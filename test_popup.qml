import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 400; height: 400; visible: true
    Button {
        id: btn
        text: "Click"
        x: 350
        y: 10
        onClicked: menu.open()
        Menu {
            id: menu
            x: (btn.width - width) / 2
            y: btn.height + 10
            
            property point mappedCenter: mapFromItem(btn, btn.width / 2, 0)
            
            MenuItem { text: "Item 1" }
            onOpened: {
                console.log("Menu x:", x);
                console.log("Menu global X:", mapToItem(null, 0, 0).x);
                console.log("Button global X:", btn.mapToItem(null, 0, 0).x);
                console.log("Mapped center x:", mappedCenter.x);
            }
        }
        Component.onCompleted: menu.open()
    }
}
