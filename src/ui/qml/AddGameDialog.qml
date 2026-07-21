// Add / edit a Custom game — a modal overlay driven by AppState.addGameOpen. The
// user picks a game executable (required), an optional display name, launch
// options and a cover image; Save persists via Backend.addCustomGame /
// updateCustomGame. Opened for a fresh add (AppState.editGame === null) or to edit
// an existing Custom row. Matches the CINEMA card look.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Orbit

Item {
    id: root
    anchors.fill: parent
    visible: AppState.addGameOpen
    z: 60

    readonly property bool editing: AppState.editGame !== null

    // editable field state
    property string gName: ""
    property string gExe: ""
    property string gArgs: ""
    property string gCover: ""

    function load() {
        if (root.editing) {
            var e = backend.customGame(AppState.editGame.launchId);
            gName = e.name || "";
            gExe = e.exe || "";
            gArgs = e.args || "";
            gCover = e.coverPath || "";
        } else {
            gName = ""; gExe = ""; gArgs = ""; gCover = "";
        }
    }
    onVisibleChanged: if (visible) load()

    function save() {
        if (gExe.length === 0) return;
        if (root.editing)
            backend.updateCustomGame(AppState.editGame.launchId, gName, gExe, gArgs, gCover);
        else
            backend.addCustomGame(gName, gExe, gArgs, gCover);
        AppState.closeAddGame();
    }

    // dimmer — click outside the card closes
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea { anchors.fill: parent; onClicked: AppState.closeAddGame() }
    }

    FileDialog {
        id: exePicker
        title: qsTr("Choose the game program")
        nameFilters: [qsTr("Programs (*.exe)"), qsTr("All files (*)")]
        onAccepted: {
            root.gExe = backend.urlToLocalFile(selectedFile);
            if (root.gName.length === 0) {
                var base = root.gExe.replace(/\\/g, "/").split("/").pop();
                root.gName = base.replace(/\.[^.]+$/, "");   // strip extension
            }
        }
    }
    FileDialog {
        id: coverPicker
        title: qsTr("Choose a cover image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp)"), qsTr("All files (*)")]
        onAccepted: root.gCover = backend.urlToLocalFile(selectedFile)
    }

    // card
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 60)
        implicitHeight: card.implicitHeight + 44
        radius: Theme.rLg
        color: Qt.rgba(0.055, 0.070, 0.094, 0.98)
        border.width: 1; border.color: Theme.line

        // swallow clicks so the dimmer's MouseArea doesn't close it
        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: card
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 14

            Label {
                text: root.editing ? qsTr("Edit game") : qsTr("Add a game")
                color: Theme.text
                font.family: Theme.fontDisplay; font.pixelSize: 20; font.weight: Font.Bold
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Add any game ORBIT didn't detect. Pick its program file and it'll launch straight from your library.")
                color: Theme.faint; wrapMode: Text.WordWrap
                font.family: Theme.fontBody; font.pixelSize: 12
            }

            // ---- game file (required) ----
            FieldLabel { text: qsTr("Game file") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                FieldBox {
                    Layout.fillWidth: true
                    text: root.gExe.length ? root.gExe : qsTr("No file chosen")
                    placeholder: root.gExe.length === 0
                }
                DialogButton { label: qsTr("Browse…"); onClicked: exePicker.open() }
            }

            // ---- name ----
            FieldLabel { text: qsTr("Name") }
            TextField {
                id: nameField
                Layout.fillWidth: true
                text: root.gName
                onTextEdited: root.gName = text
                placeholderText: qsTr("Display name")
                color: Theme.text; placeholderTextColor: Theme.faint
                font.family: Theme.fontBody; font.pixelSize: 13
                selectByMouse: true
                background: Rectangle { radius: Theme.rSm; color: Theme.fill
                    border.width: 1; border.color: nameField.activeFocus ? Theme.accent : Theme.line }
                leftPadding: 12; rightPadding: 12; topPadding: 9; bottomPadding: 9
            }

            // ---- launch options (optional) ----
            FieldLabel { text: qsTr("Launch options (optional)") }
            TextField {
                id: argsField
                Layout.fillWidth: true
                text: root.gArgs
                onTextEdited: root.gArgs = text
                placeholderText: qsTr("e.g. -windowed -novid")
                color: Theme.text; placeholderTextColor: Theme.faint
                font.family: Theme.fontBody; font.pixelSize: 13
                selectByMouse: true
                background: Rectangle { radius: Theme.rSm; color: Theme.fill
                    border.width: 1; border.color: argsField.activeFocus ? Theme.accent : Theme.line }
                leftPadding: 12; rightPadding: 12; topPadding: 9; bottomPadding: 9
            }

            // ---- cover (optional) ----
            FieldLabel { text: qsTr("Cover image (optional)") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                FieldBox {
                    Layout.fillWidth: true
                    text: root.gCover.length ? root.gCover
                        : qsTr("Auto — from the game's files, else its icon")
                    placeholder: root.gCover.length === 0
                }
                DialogButton { label: qsTr("Choose…"); onClicked: coverPicker.open() }
                DialogButton { visible: root.gCover.length > 0; label: qsTr("Clear")
                    onClicked: root.gCover = "" }
            }

            // ---- actions ----
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10
                Item { Layout.fillWidth: true }
                DialogButton { label: qsTr("Cancel"); onClicked: AppState.closeAddGame() }
                DialogButton {
                    primary: true
                    enabled: root.gExe.length > 0
                    label: root.editing ? qsTr("Save") : qsTr("Add game")
                    onClicked: root.save()
                }
            }
        }
    }

    // ---- small building blocks ----
    component FieldLabel: Label {
        Layout.fillWidth: true
        color: Theme.muted
        font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
        font.letterSpacing: Theme.tracking(0.6)
    }

    component FieldBox: Rectangle {
        property alias text: boxLabel.text
        property bool placeholder: false
        Layout.preferredHeight: 38
        radius: Theme.rSm
        color: Theme.fill
        border.width: 1; border.color: Theme.line
        Label {
            id: boxLabel
            anchors.fill: parent
            anchors.leftMargin: 12; anchors.rightMargin: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
            color: parent.placeholder ? Theme.faint : Theme.text
            font.family: Theme.fontBody; font.pixelSize: 12
        }
    }

    component DialogButton: Rectangle {
        id: btn
        property string label: ""
        property bool primary: false
        signal clicked()
        // `enabled` is the built-in Item property — a disabled button dims and its
        // MouseArea stops receiving clicks automatically.
        implicitWidth: btnLabel.implicitWidth + 28
        implicitHeight: 36
        radius: 18
        opacity: enabled ? 1 : 0.4
        color: primary ? (btnHover.containsMouse ? "#e8ecf5" : "#ffffff")
             : (btnHover.containsMouse ? Theme.fillHover : Theme.fill)
        border.width: primary ? 0 : 1; border.color: Theme.line
        Label {
            id: btnLabel; anchors.centerIn: parent; text: btn.label
            color: btn.primary ? "#0b0d12" : Theme.text
            font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold
        }
        MouseArea {
            id: btnHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }
}
