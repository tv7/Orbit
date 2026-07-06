import QtQuick
import QtQuick.Controls.Basic

// ORBIT-styled scrollbar: a slim glass pill that brightens + widens on hover,
// shows a faint track while in use, and fades out when idle. Built on the Basic
// style (custom contentItem/background isn't supported on native styles).
// Use as: `ScrollBar.vertical: AppScrollBar {}`.
ScrollBar {
    id: bar
    policy: ScrollBar.AsNeeded
    minimumSize: 0.08
    padding: 2

    implicitWidth: vertical ? 12 : 0    // generous hit area; the pill inside is slim
    implicitHeight: horizontal ? 12 : 0

    // fade away when idle (Behavior keeps it from popping)
    opacity: policy === ScrollBar.AlwaysOn || active ? 1 : 0
    visible: size < 1
    Behavior on opacity { NumberAnimation { duration: 260 } }

    contentItem: Rectangle {
        implicitWidth: bar.hovered || bar.pressed ? 7 : 4
        implicitHeight: implicitWidth
        radius: Math.min(width, height) / 2
        color: bar.pressed ? Qt.rgba(1, 1, 1, 0.48)
             : bar.hovered ? Qt.rgba(1, 1, 1, 0.32)
             : Qt.rgba(1, 1, 1, 0.16)
        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on implicitWidth { NumberAnimation { duration: 120 } }
    }

    background: Rectangle {
        radius: Math.min(width, height) / 2
        color: Qt.rgba(1, 1, 1, bar.hovered || bar.pressed ? 0.05 : 0)
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
