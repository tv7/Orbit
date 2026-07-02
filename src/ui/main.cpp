// ORBIT entry point. Inits Qt, installs the QNetwork HTTP fetcher into the
// core, exposes the Backend to QML, and loads the UI. No sidecar, no web view —
// the whole app is this one native process.

#include "Backend.h"
#include "QtFetcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTextStream>
#include <QTranslator>

// Mirror every Qt log message (incl. QML load errors) to startup.log next to the
// exe, so failures are visible even in the windowed build. Temporary debug aid.
static void logToFile(QtMsgType, const QMessageLogContext&, const QString& msg) {
    static QFile f(QCoreApplication::applicationDirPath() + "/startup.log");
    static bool open = f.open(QIODevice::Append | QIODevice::Text);
    if (open) { QTextStream(&f) << msg << '\n'; f.flush(); }
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("ORBIT");
    app.setApplicationName("ORBIT");
    qInstallMessageHandler(logToFile);

    // Window/taskbar icon (Alt-Tab, titlebar). The exe's Explorer icon comes from
    // the .rc-embedded orbit.ico; this covers the runtime side.
    {
        QIcon icon;
        for (int s : {16, 24, 32, 48, 64, 128, 256})
            icon.addFile(QString(":/icons/orbit-%1.png").arg(s), QSize(s, s));
        app.setWindowIcon(icon);
    }

    // Load the bundled UI fonts and make Manrope the default (ORBIT body font), so
    // the look matches the design instead of falling back to a system font.
    for (const QString& f : {
             ":/fonts/space-grotesk-500.ttf", ":/fonts/space-grotesk-600.ttf",
             ":/fonts/space-grotesk-700.ttf",
             ":/fonts/manrope-400.ttf", ":/fonts/manrope-500.ttf",
             ":/fonts/manrope-600.ttf", ":/fonts/manrope-700.ttf", ":/fonts/manrope-800.ttf",
             ":/fonts/hanken-grotesk-400.ttf", ":/fonts/hanken-grotesk-500.ttf",
             ":/fonts/hanken-grotesk-600.ttf", ":/fonts/hanken-grotesk-700.ttf",
             ":/fonts/geist-500.ttf", ":/fonts/geist-600.ttf",
             ":/fonts/cairo-arabic-400.ttf", ":/fonts/cairo-arabic-600.ttf",
             ":/fonts/cairo-arabic-700.ttf"})
        QFontDatabase::addApplicationFont(f);
    {
        QFont base("Manrope");
        base.setPixelSize(14);
        app.setFont(base);
    }

    // core/ does HTTP through this injected fetcher (keeps core Qt-free).
    ss::ui::installQtFetcher();

    ss::ui::Backend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);

    // Arabic UI catalog (qsTr strings; ar.qm bundled as a resource). Applied when
    // the persisted language is "ar" and toggled live from Settings — retranslate()
    // re-evaluates every qsTr binding, and Main.qml mirrors layout via backend.rtl.
    static QTranslator arTranslator;
    const bool arLoaded = arTranslator.load(":/i18n/ar.qm");
    auto applyLanguage = [&app, &engine, arLoaded](const QString& lang) {
        if (!arLoaded) return;
        if (lang == "ar") app.installTranslator(&arTranslator);
        else app.removeTranslator(&arTranslator);
        engine.retranslate();
    };
    if (backend.language() == "ar" && arLoaded) app.installTranslator(&arTranslator);
    QObject::connect(&backend, &ss::ui::Backend::languageChanged, &app,
                     [&backend, applyLanguage] { applyLanguage(backend.language()); });

    // Surface QML load failures explicitly instead of silently exiting.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { qWarning("QML failed to load — see messages above / startup.log"); });
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError>& ws) { for (const auto& w : ws) qWarning("%s", qPrintable(w.toString())); });

    engine.loadFromModule("Orbit", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning("No root QML object created — startup aborted.");
        return -1;
    }

    backend.refresh();  // initial scan -> stateChanged populates the grid
    return app.exec();
}
