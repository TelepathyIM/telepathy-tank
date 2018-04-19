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

#include "protocol.hpp"
#include "connection.hpp"

#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QVariantMap>
#include <QDebug>

MatrixProtocol::MatrixProtocol(const QDBusConnection &dbusConnection, const QString &name)
    : BaseProtocol(dbusConnection, name)
{
    qDebug() << Q_FUNC_INFO;
    setParameters({
                      Tp::ProtocolParameter(QLatin1String("user"), QLatin1String("s"), Tp::ConnMgrParamFlagRequired),
                      Tp::ProtocolParameter(QLatin1String("password"), QLatin1String("s"), Tp::ConnMgrParamFlagRequired | Tp::ConnMgrParamFlagSecret),
                      Tp::ProtocolParameter(QLatin1String("device"), QLatin1String("s"), Tp::ConnMgrParamFlagHasDefault, QStringLiteral("pc")),
                      Tp::ProtocolParameter(QLatin1String("server"), QLatin1String("s"), Tp::ConnMgrParamFlagRequired), // homeserver
                  });

    setRequestableChannelClasses(MatrixConnection::getRequestableChannelList());

    // callbacks
    setCreateConnectionCallback(memFun(this, &MatrixProtocol::createConnection));
    setIdentifyAccountCallback(memFun(this, &MatrixProtocol::identifyAccount));
    setNormalizeContactCallback(memFun(this, &MatrixProtocol::normalizeContact));

    addrIface = Tp::BaseProtocolAddressingInterface::create();
    addrIface->setAddressableVCardFields({ QStringLiteral("x-matrix") });
    addrIface->setAddressableUriSchemes({ QStringLiteral("matrix") });
    addrIface->setNormalizeVCardAddressCallback(memFun(this, &MatrixProtocol::normalizeVCardAddress));
    addrIface->setNormalizeContactUriCallback(memFun(this, &MatrixProtocol::normalizeContactUri));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(addrIface));

    avatarsIface = Tp::BaseProtocolAvatarsInterface::create();
    avatarsIface->setAvatarDetails(Tp::AvatarSpec(/* supportedMimeTypes */ QStringList() << QLatin1String("image/jpeg"),
                                                  /* minHeight */ 0, /* maxHeight */ 160, /* recommendedHeight */ 160,
                                                  /* minWidth */ 0, /* maxWidth */ 160, /* recommendedWidth */ 160,
                                                  /* maxBytes */ 10240));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(avatarsIface));

    presenceIface = Tp::BaseProtocolPresenceInterface::create();
    presenceIface->setStatuses(Tp::PresenceSpecList(MatrixConnection::getSimpleStatusSpecMap()));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(presenceIface));
}

MatrixProtocol::~MatrixProtocol()
{
}

Tp::BaseConnectionPtr MatrixProtocol::createConnection(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << parameters;
    Q_UNUSED(error)

    Tp::BaseConnectionPtr newConnection = Tp::BaseConnection::create<MatrixConnection>(QLatin1String("tank"), name(), parameters);

    return newConnection;
}

QString MatrixProtocol::identifyAccount(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << parameters;
    error->set(QLatin1String("IdentifyAccount.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MatrixProtocol::normalizeContact(const QString &contactId, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << contactId;
    error->set(QLatin1String("NormalizeContact.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MatrixProtocol::normalizeVCardAddress(const QString &vcardField, const QString vcardAddress,
        Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << vcardField << vcardAddress;
    error->set(QLatin1String("NormalizeVCardAddress.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MatrixProtocol::normalizeContactUri(const QString &uri, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << uri;
    error->set(QLatin1String("NormalizeContactUri.Error.NotImplemented"), QLatin1String(""));
    return QString();
}
