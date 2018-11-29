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

#ifndef TANK_MATRIX_PROTOCOL_HPP
#define TANK_MATRIX_PROTOCOL_HPP

#include <TelepathyQt/BaseProtocol>

class MatrixProtocol : public Tp::BaseProtocol
{
    Q_OBJECT
    Q_DISABLE_COPY(MatrixProtocol)

public:
    MatrixProtocol(const QDBusConnection &dbusConnection, const QString &name);
    virtual ~MatrixProtocol();

private:
    Tp::BaseConnectionPtr createConnection(const QVariantMap &parameters, Tp::DBusError *error);
    QString identifyAccount(const QVariantMap &parameters, Tp::DBusError *error);
    QString normalizeContact(const QString &contactId, Tp::DBusError *error);

    // Proto.I.Addressing
    QString normalizeVCardAddress(const QString &vCardField, const QString vCardAddress,
            Tp::DBusError *error);
    QString normalizeContactUri(const QString &uri, Tp::DBusError *error);

    Tp::BaseProtocolAddressingInterfacePtr addrIface;
    Tp::BaseProtocolAvatarsInterfacePtr avatarsIface;
    Tp::BaseProtocolPresenceInterfacePtr presenceIface;

};

#endif // TANK_MATRIX_PROTOCOL_HPP
