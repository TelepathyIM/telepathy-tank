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

#include <QCoreApplication>

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/Constants>
#include <TelepathyQt/Debug>

#include "protocol.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("TelepathyIM"));
    app.setApplicationName(QLatin1String("telepathy-tank"));

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    Tp::BaseProtocolPtr protocol = Tp::BaseProtocol::create<MatrixProtocol>(QLatin1String("matrix"));
    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(QLatin1String("tank"));

    protocol->setEnglishName(QLatin1String("Matrix"));
    protocol->setIconName(QLatin1String("telepathy-tank"));
    protocol->setVCardField(QLatin1String("x-matrix"));

    if (!cm->addProtocol(protocol)) {
        qCritical() << "Unable to add" << protocol->name() << "protocol";
        return 1;
    }
    if (!cm->registerObject()) {
        qCritical() << "Unable to register the cm service";
        return 2;
    }

    return app.exec();
}
