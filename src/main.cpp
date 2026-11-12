// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KIconTheme>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "controller.h"
#include "pacnewhelper-version.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("pacnew-helper");

    // Native look on Plasma, with a sane fallback elsewhere.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    if (qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE"))
        QApplication::setStyle(u"breeze"_s);

    KAboutData about(u"pacnew-helper"_s,
                     i18n("Pacnew Helper"),
                     QStringLiteral(PACNEWHELPER_VERSION_STRING),
                     i18n("Review and apply updated system configuration files"),
                     KAboutLicense::GPL_V3,
                     i18n("© 2026 Pacnew Helper contributors"));
    about.setDesktopFileName(u"gay.geeks.pacnewhelper"_s);
    about.setHomepage(u"https://github.com/cptn-cosmo/pacnew-helper"_s);
    about.setProgramLogo(QIcon::fromTheme(u"gay.geeks.pacnewhelper"_s, QIcon::fromTheme(u"system-software-update"_s)));
    KAboutData::setApplicationData(about);
    QGuiApplication::setWindowIcon(QIcon::fromTheme(u"gay.geeks.pacnewhelper"_s, QIcon::fromTheme(u"system-software-update"_s)));

    KCrash::initialize();

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    const QCommandLineOption rootOption(u"root"_s,
                                        i18n("Work on a copy of a system in the given folder instead of the real one (demo and testing; "
                                             "no administrator rights needed)."),
                                        i18n("folder"));
    parser.addOption(rootOption);
    // Development aids, used for tests and README screenshots.
    QCommandLineOption screenshotOption(u"screenshot"_s, u"Save a screenshot of the window to <file> and quit."_s, u"file"_s);
    screenshotOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption openOption(u"open"_s, u"Open <path> for review after scanning."_s, u"path"_s);
    openOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption pageOption(u"page"_s, u"Show <page> (history, about, summary:<action>) after scanning."_s, u"page"_s);
    pageOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions({screenshotOption, openOption, pageOption});
    parser.process(app);
    about.processCommandLine(&parser);

    const QString root = parser.isSet(rootOption) ? QDir(parser.value(rootOption)).absolutePath() : u"/"_s;
    if (!QDir(root).exists()) {
        qCritical().noquote() << i18n("The folder %1 does not exist.", root);
        return 2;
    }

    // Only one window for the real system; demo instances may run alongside.
    std::unique_ptr<KDBusService> service;
    if (root == u"/")
        service = std::make_unique<KDBusService>(KDBusService::Unique);

    Controller controller(root);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    qmlRegisterSingletonInstance("gay.geeks.pacnewhelper.app", 1, 0, "App", &controller);
    engine.setInitialProperties({
        {u"startPath"_s, parser.value(openOption)},
        {u"startPage"_s, parser.value(pageOption)},
    });
    engine.loadFromModule("gay.geeks.pacnewhelper", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());

    if (service) {
        QObject::connect(service.get(), &KDBusService::activateRequested, window, [window] {
            window->show();
            window->raise();
            window->requestActivate();
        });
    }

    if (parser.isSet(screenshotOption)) {
        const QString file = parser.value(screenshotOption);
        // Once the scan is done, give the pages time to load and settle.
        QObject::connect(&controller, &Controller::scanningChanged, window, [&controller, window, file] {
            if (controller.scanning())
                return;
            QTimer::singleShot(2000, window, [window, file] {
                const bool ok = window->grabWindow().save(file);
                QCoreApplication::exit(ok ? 0 : 1);
            });
        });
    }

    controller.rescan();
    return app.exec();
}
