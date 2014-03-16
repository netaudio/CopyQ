/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "app.h"

#include "common/common.h"
#include "platform/platformnativeinterface.h"
#ifdef Q_OS_UNIX
#   include "platform/unix/unixsignalhandler.h"
#endif

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QVariant>
#ifdef HAS_TESTS
#   include <QProcessEnvironment>
#endif

namespace {

void installTranslator(const QString &filename, const QString &directory)
{
    QTranslator *translator = new QTranslator(qApp);
    translator->load(filename, directory);
    QCoreApplication::installTranslator(translator);
}

void installTranslator()
{
    QString locale = QSettings().value("Options/language").toString();
    if (locale.isEmpty())
        locale = QLocale::system().name();

    installTranslator("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    installTranslator("copyq_" + locale, ":/translations");

    QLocale::setDefault(QLocale(locale));
}

} // namespace

App::App(QCoreApplication *application, const QString &sessionName)
    : m_app(application)
    , m_exitCode(0)
    , m_closed(false)
{
    QString session("copyq");
    if ( !sessionName.isEmpty() ) {
        session += "-" + sessionName;
        m_app->setProperty( "CopyQ_session_name", QVariant(sessionName) );
    }

#ifdef HAS_TESTS
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString testId = env.value("COPYQ_TEST_ID");
    bool testing = !testId.isEmpty();
    if (testing) {
        session += ".test";
        m_app->setProperty("CopyQ_test_id", testId);
    }
#endif

    QCoreApplication::setOrganizationName(session);
    QCoreApplication::setApplicationName(session);

#ifdef Q_OS_UNIX
    if ( !UnixSignalHandler::create(m_app.data()) )
        log( QString("Failed to create handler for Unix signals!"), LogError );
#endif

    createPlatformNativeInterface()->loadSettings();

    installTranslator();

#ifdef HAS_TESTS
    // Reset settings on first run of each test case.
    if ( testing && env.contains("COPYQ_TEST_SETTINGS") ) {
        QSettings settings;
        settings.clear();

        QVariant testSettings;
        const QByteArray data = QByteArray::fromBase64( env.value("COPYQ_TEST_SETTINGS").toLatin1() );
        QDataStream input(data);
        input >> testSettings;
        const QVariantMap testSettingsMap = testSettings.toMap();

        settings.beginGroup("Plugins");
        settings.beginGroup(testId);
        foreach (const QString &key, testSettingsMap.keys())
            settings.setValue(key, testSettingsMap[key]);
        settings.endGroup();
        settings.endGroup();

        settings.setValue("CopyQ_test_id", testId);
    }
#endif
}

App::~App()
{
}

int App::exec()
{
    if ( wasClosed() ) {
        m_app->processEvents();
        return m_exitCode;
    }

    return m_app->exec();
}

void App::exit(int exitCode)
{
    if ( wasClosed() )
        return;

    QCoreApplication::exit(exitCode);
    m_exitCode = exitCode;
    m_closed = true;
}

bool App::wasClosed() const
{
    return m_closed;
}
