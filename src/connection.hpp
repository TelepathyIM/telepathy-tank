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

#ifndef TANK_MATRIX_CONNECTION_HPP
#define TANK_MATRIX_CONNECTION_HPP

#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>

#include <QHash>

namespace QMatrixClient
{

class Room;
class User;
class RoomEvent;
class ConnectionData;

class SyncJob;
class SyncData;
class RoomMessagesJob;
class PostReceiptJob;
class ForgetRoomJob;
class MediaThumbnailJob;
class JoinRoomJob;
class UploadContentJob;
class GetContentJob;
class DownloadFileJob;

class Connection;

} // QMatrixClient

struct PseudoContact {
    PseudoContact() = default;
    PseudoContact(const PseudoContact &contact) = default;
    PseudoContact(QMatrixClient::User *u, QMatrixClient::Room *r = nullptr)
        : user(u),
          room(r)
    {
    }
    QMatrixClient::User *user = nullptr;
    QMatrixClient::Room *room = nullptr;
};

class MatrixConnection : public Tp::BaseConnection
{
    Q_OBJECT
public:
    enum class MatrixPresence {
        Online,
        Offline,
        Unavailable,
    };
//    online : The default state when the user is connected to an event stream.
//    unavailable : The user is not reachable at this time e.g. they are idle.
//    offline : The user is not connected to an event stream or is explicitly suppressing their profile information from being sent.

    MatrixConnection(const QDBusConnection &dbusConnection,
            const QString &cmName, const QString &protocolName,
            const QVariantMap &parameters);
    ~MatrixConnection();

    static Tp::AvatarSpec getAvatarSpec();
    static Tp::SimpleStatusSpecMap getSimpleStatusSpecMap();
    static Tp::SimplePresence mkSimplePresence(MatrixPresence presence, const QString &statusMessage = QString());
    static Tp::RequestableChannelClassSpecList getRequestableChannelList();

    void doConnect(Tp::DBusError *error);
    void doDisconnect();

    QStringList inspectHandles(uint handleType, const Tp::UIntList &handles, Tp::DBusError *error);
    Tp::UIntList requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error);

    Tp::BaseChannelPtr createChannelCB(const QVariantMap &request, Tp::DBusError *error);

    Tp::ContactAttributesMap getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error);
    Tp::ContactAttributesMap getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error);

    void requestSubscription(const Tp::UIntList &handles, const QString &message, Tp::DBusError *error);

    Tp::ContactInfoFieldList requestContactInfo(uint handle, Tp::DBusError *error);
    Tp::ContactInfoFieldList getUserInfo(const quint32 userId) const;
    Tp::ContactInfoMap getContactInfo(const Tp::UIntList &contacts, Tp::DBusError *error);

    Tp::AliasMap getAliases(const Tp::UIntList &handles, Tp::DBusError *error = 0);
    QString getContactAlias(uint handle) const;

    Tp::SimplePresence getPresence(uint handle);
    uint setPresence(const QString &status, const QString &message, Tp::DBusError *error);

    QMatrixClient::Connection *matrix() const { return m_connection; }

signals:
    void messageReceived(const QString &sender, const QString &message);
    void chatDetailsChanged(quint32 chatId, const Tp::UIntList &handles);

protected:
    Tp::BaseChannelPtr createRoomListChannel();

protected slots:
    void onConnected();
    void onSyncDone();
    void onUserAvatarChanged(QMatrixClient::User *user);

public:
    bool loadSessionData();
    bool saveSessionData() const;

    void processNewRoom(QMatrixClient::Room *room);
    uint ensurePseudoContact(QMatrixClient::User *user, QMatrixClient::Room *room);

    QMatrixClient::User *getUser(uint handle) const;
    QMatrixClient::User *getUser(const QString &id) const;
    QMatrixClient::Room *getRoom(uint handle) const;
    uint getContactHandle(QMatrixClient::User *user);
    uint getDirectContactHandle(QMatrixClient::Room *room);
    uint getRoomHandle(QMatrixClient::Room *room);

    uint ensureHandle(QMatrixClient::User *user);
    uint ensureHandle(QMatrixClient::Room *room);
    uint ensureContactHandle(const QString &identifier);

    void prefetchRoomHistory(QMatrixClient::Room *room, Tp::HandleType type, uint handle);

    void startMechanismWithData_authCode(const QString &mechanism, const QByteArray &data, Tp::DBusError *error);
    void startMechanismWithData_password(const QString &mechanism, const QByteArray &data, Tp::DBusError *error);

    void requestAvatars(const Tp::UIntList &handles, Tp::DBusError *error);
    Tp::AvatarTokenMap getKnownAvatarTokens(const Tp::UIntList &handles, Tp::DBusError *error);

    void requestAvatarsImpl(const Tp::UIntList &handles);

    Tp::BaseConnectionContactsInterfacePtr contactsIface;
    Tp::BaseConnectionSimplePresenceInterfacePtr m_simplePresenceIface;
    Tp::BaseConnectionContactListInterfacePtr m_contactListIface;
    Tp::BaseConnectionContactInfoInterfacePtr contactInfoIface;
    Tp::BaseConnectionAvatarsInterfacePtr m_avatarsIface;
    Tp::BaseConnectionAliasingInterfacePtr m_aliasingIface;
    Tp::BaseConnectionAddressingInterfacePtr addressingIface;
    Tp::BaseConnectionRequestsInterfacePtr m_requestsIface;
    Tp::BaseChannelSASLAuthenticationInterfacePtr saslIface_password;

    QMatrixClient::Connection *m_connection = nullptr;
    QHash<uint, PseudoContact> m_contacts; // Handle to contact
    QStringList m_contactIds;
    QStringList m_roomIds;

    QString m_user;
    QString m_password;
    QString m_server;

    QString m_userId;
    QByteArray m_accessToken;
    QString m_homeServer;
    QString m_deviceId;

};

#endif // TANK_MATRIX_CONNECTION_HPP
