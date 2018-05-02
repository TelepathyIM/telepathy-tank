/*
    This file is part of the telepathy-tank connection manager.
    Copyright (C) 2018 Alexandr Akulich <akulichalexander@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "debug.hpp"

#include <TelepathyQt/BaseDebug>

static QPointer<Tp::BaseDebug> debugInterfacePtr;
static QtMessageHandler defaultMessageHandler = 0;

void debugViaDBusInterface(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!debugInterfacePtr.isNull()) {
        QString domain(QLatin1String("%1:%2, %3"));
        QByteArray fileName = QByteArray::fromRawData(context.file, qstrlen(context.file));

        static const char *namesToWrap[] = {
            "tank",
            "telepathy-qt"
        };

        for (int i = 0; i < 2; ++i) {
            int index = fileName.indexOf(namesToWrap[i]);
            if (index < 0) {
                continue;
            }

            fileName = fileName.mid(index);
            break;
        }

        domain = domain.arg(QString::fromLocal8Bit(fileName)).arg(context.line).arg(QString::fromLatin1(context.function));
        QString message = msg;
        if (message.startsWith(QLatin1String(context.function))) {
            message = message.mid(qstrlen(context.function));
            if (message.startsWith(QLatin1Char(' '))) {
                message.remove(0, 1);
            }
        }

        switch (type) {
        case QtDebugMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelDebug, message);
            break;
        case QtInfoMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelInfo, message);
            break;
        case QtWarningMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelWarning, message);
            break;
        case QtCriticalMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelCritical, message);
            break;
        case QtFatalMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelError, message);
            break;
        }
    }

    if (defaultMessageHandler) {
        defaultMessageHandler(type, context, msg);
        return;
    }

    const QString logMessage = qFormatLogMessage(type, context, msg);

    if (logMessage.isNull()) {
        return;
    }

    fprintf(stderr, "%s\n", logMessage.toLocal8Bit().constData());
    fflush(stderr);
}

bool enableDebugInterface(const QString &dbusObjectName)
{
    if (!debugInterfacePtr.isNull()) {
        return debugInterfacePtr->isRegistered();
    }

    debugInterfacePtr = new Tp::BaseDebug();
    debugInterfacePtr->setGetMessagesLimit(-1);

    if (!debugInterfacePtr->registerObject(dbusObjectName)) {
        return false;
    }

    defaultMessageHandler = qInstallMessageHandler(debugViaDBusInterface);
    return true;
}
