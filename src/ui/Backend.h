// Backend — the single QObject QML talks to. Collapses server.py's Bridge + the
// web/bridge.js shim into one in-process object: methods replace POST /api/<m>,
// Qt signals replace the SSE stream. Heavy work (scan/cover/launch) runs on a
// QThreadPool; results come back as signals (queued onto the GUI thread).

#pragma once

#include "GameModel.h"
#include "GameFilterModel.h"
#include "../core/stores/store.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace ss::ui {

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* games READ games CONSTANT)
    Q_PROPERTY(QAbstractItemModel* allGames READ allGames CONSTANT)
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY stateChanged)
    Q_PROPERTY(QVariantList stores READ stores NOTIFY stateChanged)
    Q_PROPERTY(QString currentAccount READ currentAccount NOTIFY stateChanged)
    Q_PROPERTY(int gameCount READ gameCount NOTIFY stateChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool launching READ launching NOTIFY launchingChanged)
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(bool rtl READ rtl NOTIFY languageChanged)
    Q_PROPERTY(bool onboarded READ onboarded NOTIFY onboardedChanged)
    // CINEMA-era surface: hero banner mode, offline default, run-at-startup and
    // the library scan timestamp (Settings > Library "Last scan").
    Q_PROPERTY(QString heroMode READ heroMode NOTIFY heroModeChanged)
    Q_PROPERTY(bool offlineDefault READ offlineDefault NOTIFY offlineDefaultChanged)
    Q_PROPERTY(bool autostartSupported READ autostartSupported CONSTANT)
    Q_PROPERTY(bool runAtStartup READ runAtStartup NOTIFY runAtStartupChanged)
    Q_PROPERTY(qint64 lastScanTime READ lastScanTime NOTIFY stateChanged)
    // Update check (GitHub Releases). appVersion is this build; the rest describe
    // the newest release found by checkForUpdates().
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString updateState READ updateState NOTIFY updateChanged)  // ""|checking|latest|available|error
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateChanged)
    Q_PROPERTY(QString updateUrl READ updateUrl NOTIFY updateChanged)
    Q_PROPERTY(QString updateNotes READ updateNotes NOTIFY updateChanged)

public:
    explicit Backend(QObject* parent = nullptr);

    QAbstractItemModel* games() { return &proxy_; }
    // The unfiltered scanned model — shelves layer their own GameFilter over it.
    QAbstractItemModel* allGames() { return &model_; }
    QVariantList accounts() const { return accounts_; }
    QVariantList stores() const { return stores_; }
    QString currentAccount() const { return currentAccount_; }
    int gameCount() const { return model_.rowCount(); }
    bool scanning() const { return scanning_; }
    bool launching() const { return launching_; }
    QString language() const { return language_; }
    bool rtl() const { return language_ == "ar"; }
    bool onboarded() const { return onboarded_; }
    QString heroMode() const { return heroMode_; }
    bool offlineDefault() const { return offlineDefault_; }
    bool autostartSupported() const;
    bool runAtStartup() const { return runAtStartup_; }
    qint64 lastScanTime() const { return lastScanTime_; }
    QString appVersion() const;
    QString updateState() const { return updateState_; }
    bool updateAvailable() const { return updateState_ == "available"; }
    QString latestVersion() const { return latestVersion_; }
    QString updateUrl() const { return updateUrl_; }
    QString updateNotes() const { return updateNotes_; }

    // ---- invoked from QML (each returns immediately, works off-thread) ----
    Q_INVOKABLE void refresh();                       // ~ request_state
    Q_INVOKABLE void requestCover(qint64 appid);      // ~ request_cover -> coverReady
    Q_INVOKABLE void requestHero(qint64 appid);       // wide art -> heroReady
    Q_INVOKABLE void play(qint64 appid, bool offline);// ~ play
    Q_INVOKABLE void cancel();                        // ~ cancel
    Q_INVOKABLE void addAccount();                    // ~ add_account
    Q_INVOKABLE void pinToAccount(qint64 appid, const QString& steamid64);  // override
    // Custom (user-added) games: add an arbitrary exe to the library, edit or
    // remove one. Each persists to custom_games.json and rescans.
    Q_INVOKABLE void addCustomGame(const QString& name, const QString& exePath,
                                   const QString& args, const QString& coverPath);
    Q_INVOKABLE void updateCustomGame(const QString& launchId, const QString& name,
                                      const QString& exePath, const QString& args,
                                      const QString& coverPath);
    Q_INVOKABLE void removeCustomGame(const QString& launchId);
    // Convert a FileDialog "file://" URL to a native local path (for the add dialog).
    Q_INVOKABLE QString urlToLocalFile(const QUrl& url) const { return url.toLocalFile(); }
    // The full stored entry for a custom game (name/exe/args/coverPath), so the
    // edit dialog can prefill fields the display model doesn't carry. Empty if none.
    Q_INVOKABLE QVariantMap customGame(const QString& launchId) const;
    Q_INVOKABLE void switchTo(const QString& steamid64);  // switch+restart Steam, NO launch
    Q_INVOKABLE void setHeroMode(const QString& mode);    // "last" | "random" (persists)
    Q_INVOKABLE void setOfflineDefault(bool value);       // persists
    Q_INVOKABLE void setRunAtStartup(bool value);         // HKCU Run key (Windows)
    Q_INVOKABLE qint64 coverCacheSize() const;            // bytes under data/covers/
    Q_INVOKABLE void clearCoverCache();                   // wipe it (emits status)
    // Search / filter / sort drive the proxy model (web/app.js visibleGames()).
    Q_INVOKABLE void setSearch(const QString& text);
    Q_INVOKABLE void setAccountFilter(const QString& filter);
    Q_INVOKABLE void setStoreFilter(const QString& store);
    Q_INVOKABLE void setSortOrder(const QString& order);   // "az" | "za"
    Q_INVOKABLE void setLanguage(const QString& lang);     // ~ set_language (persists)
    Q_INVOKABLE void completeOnboarding();                 // persist onboarded=true
    // Ask GitHub for the latest release. `manual` = the user pressed the Settings
    // button (always reports; ignores a skipped version); false = the quiet startup
    // check (suppresses the banner for a version the user chose to skip).
    Q_INVOKABLE void checkForUpdates(bool manual = false);
    Q_INVOKABLE void openDownloadPage();                   // open the release in a browser
    Q_INVOKABLE void skipThisUpdate();                     // hide the banner for this version

signals:
    void stateChanged();
    void scanningChanged();
    void launchingChanged();
    void languageChanged();
    void onboardedChanged();
    void heroModeChanged();
    void offlineDefaultChanged();
    void runAtStartupChanged();
    void updateChanged();
    void coverReady(qint64 appid, const QString& dataUrl);
    void heroReady(qint64 appid, const QString& dataUrl);
    void launchStarted();
    void status(const QString& message);
    void launchDone(bool ok, const QString& message);

private:
    void buildState();          // runs on a worker; emits stateChanged
    void setScanning(bool v);
    void setLaunching(bool v);

    GameModel model_;
    GameFilterModel proxy_;
    QVariantList accounts_;
    QVariantList stores_;
    QString currentAccount_;
    bool scanning_ = false;
    bool launching_ = false;
    QString language_;
    bool onboarded_ = false;
    QString heroMode_ = "last";
    bool offlineDefault_ = false;
    bool runAtStartup_ = false;
    qint64 lastScanTime_ = 0;
    QString updateState_;       // ""|checking|latest|available|error
    QString latestVersion_;
    QString updateUrl_;
    QString updateNotes_;

    // A game's identity for launch/cover routing. Steam games are keyed by their
    // real appid; non-Steam stores (Epic, …) have no numeric id, so Backend hands
    // them a stable synthetic id (see buildState) and routes by `store`/`launchId`.
    struct GameRef {
        Store store = Store::Steam;
        qint64 appid = 0;          // real Steam appid, or 0 for non-Steam
        QString launchId;          // store-specific token (Epic AppName, …)
        QString name;
        QString coverHint;         // per-store art hint (see model.h::Game)
    };
    std::map<qint64, GameRef> gameIndex_;   // synthetic-or-real id -> game, for play/cover
    std::mutex indexMutex_;

    std::vector<std::unique_ptr<IStore>> storeImpls_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> launchGuard_{false};   // one launch at a time
    QThreadPool pool_;
};

}  // namespace ss::ui
