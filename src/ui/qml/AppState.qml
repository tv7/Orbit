// Shared, mutable UI state for the ORBIT shell. A singleton so any view can read
// or drive navigation without threading properties through every parent. The
// backend (games/accounts/launch) stays the source of truth; this only holds the
// view-layer state the CINEMA design needs (which screen, which game, overlays).
pragma Singleton
import QtQuick
import Orbit

QtObject {
    id: app

    // "library" | "detail" | "accounts" | "settings"
    property string view: "library"
    property string previousView: "library"

    // The game shown on the detail hero (a plain JS object copied from the model
    // row: appid, name, store, accountId, accountName, accountColor, mapped,
    // fullyInstalled, playtime, lastPlayed).
    property var selected: null

    // Launch-offline toggle (Steam only; other stores ignore it). Seeded from the
    // persisted default in Main.qml on startup.
    property bool offline: false

    // Ctrl-K search palette.
    property bool paletteOpen: false

    // First-run onboarding overlay step: "" (closed) | "welcome" | "connect" | "done".
    property string onboarding: ""
    function startOnboarding() { onboarding = "welcome" }
    function obGo(step) { onboarding = step }

    // Add/Edit custom-game dialog. `addGameOpen` shows it; `editGame` is the game
    // being edited (a Custom row) or null when adding a fresh one.
    property bool addGameOpen: false
    property var editGame: null
    function openAddGame()      { editGame = null; addGameOpen = true }
    function openEditGame(game) { editGame = game; addGameOpen = true }
    function closeAddGame()     { addGameOpen = false; editGame = null }

    function open(game) {
        selected = game;
        previousView = view;
        view = "detail";
        paletteOpen = false;
    }
    function back() {
        view = (previousView === "detail" ? "library" : previousView);
    }
    function go(v) {
        if (view !== v) { previousView = view; view = v; }
        paletteOpen = false;
    }
}
