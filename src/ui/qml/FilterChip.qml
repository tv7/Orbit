// Clickable filter pill for the library grid: optional brand dot + label.
// Selected = solid white (CINEMA's inverted pill), idle = ghost outline.
import QtQuick
import QtQuick.Controls
import Orbit

Rectangle {
    id: chip
    property string label: ""
    property string dotColor: ""       // "" = no dot
    property bool selected: false
    signal clicked()

    implicitWidth: row.implicitWidth + 28
    implicitHeight: 32
    radius: height / 2
    color: selected ? "#ffffff"
         : hover.containsMouse ? Theme.fillHover : "transparent"
    border.width: 1
    border.color: selected ? "#ffffff" : Theme.line

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 7
        Rectangle {
            visible: chip.dotColor.length > 0
            width: 7; height: 7; radius: 3.5; color: chip.dotColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: chip.label
            color: chip.selected ? "#0b0d12" : Theme.muted
            font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.Bold
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: chip.clicked()
    }
}
