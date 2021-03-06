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

#include "connection.hpp"
#include "messageschannel.hpp"
#include "requestdetails.hpp"

#include <TelepathyQt/Constants>
#include <TelepathyQt/BaseChannel>

#include <QDebug>

#include <QStandardPaths>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTimer>

// Quotient
#include <connection.h>
#include <room.h>
#include <settings.h>
#include <user.h>

#define Q_MATRIX_CLIENT_MAJOR_VERSION 0
#define Q_MATRIX_CLIENT_MINOR_VERSION 1
#define Q_MATRIX_CLIENT_MICRO_VERSION 0
#define Q_MATRIX_CLIENT_VERSION ((Q_MATRIX_CLIENT_MAJOR_VERSION<<16)|(Q_MATRIX_CLIENT_MINOR_VERSION<<8)|(Q_MATRIX_CLIENT_MICRO_VERSION))
#define Q_MATRIX_CLIENT_VERSION_CHECK(major, minor, patch) ((major<<16)|(minor<<8)|(patch))

static const QString secretsDirPath = QLatin1String("/secrets/");
static const QString c_saslMechanismTelepathyPassword = QLatin1String("X-TELEPATHY-PASSWORD");
static const int c_sessionDataFormat = 1;

Tp::AvatarSpec MatrixConnection::getAvatarSpec()
{
    static const auto spec = Tp::AvatarSpec({ QStringLiteral("image/png") },
                                            0 /* minHeight */,
                                            512 /* maxHeight */,
                                            256 /* recommendedHeight */,
                                            0 /* minWidth */,
                                            512 /* maxWidth */,
                                            256 /* recommendedWidth */,
                                            1024 * 1024 /* maxBytes */);
    return spec;
}

Tp::SimpleStatusSpecMap MatrixConnection::getSimpleStatusSpecMap()
{
    static const Tp::SimpleStatusSpecMap map = []() {
        // https://github.com/matrix-org/matrix-doc/blob/master/specification/modules/presence.rst#presence
        Tp::SimpleStatusSpec onlineStatus;
        onlineStatus.type = Tp::ConnectionPresenceTypeAvailable;
        onlineStatus.maySetOnSelf = true;
        onlineStatus.canHaveMessage = false;

        Tp::SimpleStatusSpec unavailableStatus;
        unavailableStatus.type = Tp::ConnectionPresenceTypeAway;
        unavailableStatus.maySetOnSelf = true;
        unavailableStatus.canHaveMessage = false;

        Tp::SimpleStatusSpec offlineStatus;
        offlineStatus.type = Tp::ConnectionPresenceTypeOffline;
        offlineStatus.maySetOnSelf = true;
        offlineStatus.canHaveMessage = false;

        return QMap<QString,Tp::SimpleStatusSpec>({
                                                      { QLatin1String("available"), onlineStatus },
                                                      { QLatin1String("unavailable"), unavailableStatus },
                                                      { QLatin1String("offline"), offlineStatus },
                                                  });
    }();
    return map;
}

Tp::SimplePresence MatrixConnection::mkSimplePresence(MatrixPresence presence, const QString &statusMessage)
{
    switch (presence) {
    case MatrixPresence::Online:
        return { Tp::ConnectionPresenceTypeAvailable, QLatin1String("available"), statusMessage };
    case MatrixPresence::Unavailable:
        return { Tp::ConnectionPresenceTypeAway, QLatin1String("unavailable"), statusMessage };
    case MatrixPresence::Offline:
        return { Tp::ConnectionPresenceTypeOffline, QLatin1String("offline"), statusMessage };
    }
    return { Tp::ConnectionPresenceTypeError, QLatin1String("error"), statusMessage };
}

Tp::RequestableChannelClassSpecList MatrixConnection::getRequestableChannelList()
{
    Tp::RequestableChannelClassSpecList result;
    Tp::RequestableChannelClass personalChat;
    personalChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    personalChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeContact;
    personalChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    personalChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    result.append(personalChat);

    Tp::RequestableChannelClass groupChat;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeRoom;
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    result.append(groupChat);

    return result;
}

MatrixConnection::MatrixConnection(const QDBusConnection &dbusConnection, const QString &cmName,
                                   const QString &protocolName, const QVariantMap &parameters)
    : Tp::BaseConnection(dbusConnection, cmName, protocolName, parameters)
{
    qDebug() << Q_FUNC_INFO << parameters;

    /* Connection.Interface.Contacts */
    contactsIface = Tp::BaseConnectionContactsInterface::create();
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this, &MatrixConnection::getContactAttributes));
    contactsIface->setContactAttributeInterfaces({
                                                     TP_QT_IFACE_CONNECTION,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS,
                                                 });
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    /* Connection.Interface.Aliasing */
    m_aliasingIface = Tp::BaseConnectionAliasingInterface::create();
    m_aliasingIface->setGetAliasesCallback(Tp::memFun(this, &MatrixConnection::getAliases));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(m_aliasingIface));

    /* Connection.Interface.SimplePresence */
    m_simplePresenceIface = Tp::BaseConnectionSimplePresenceInterface::create();
    m_simplePresenceIface->setStatuses(getSimpleStatusSpecMap());
    m_simplePresenceIface->setSetPresenceCallback(Tp::memFun(this, &MatrixConnection::setPresence));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(m_simplePresenceIface));

    /* Connection.Interface.ContactList */
    m_contactListIface = Tp::BaseConnectionContactListInterface::create();
    m_contactListIface->setContactListPersists(true);
    m_contactListIface->setCanChangeContactList(true);
    m_contactListIface->setDownloadAtConnection(true);
    m_contactListIface->setGetContactListAttributesCallback(Tp::memFun(this, &MatrixConnection::getContactListAttributes));
    m_contactListIface->setRequestSubscriptionCallback(Tp::memFun(this, &MatrixConnection::requestSubscription));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(m_contactListIface));

#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 8)
    /* Connection.Interface.ContactGroups */
    Tp::BaseConnectionContactGroupsInterfacePtr groupsIface = Tp::BaseConnectionContactGroupsInterface::create();
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(groupsIface));
#endif

    /* Connection.Interface.Requests */
    m_requestsIface = Tp::BaseConnectionRequestsInterface::create(this);
    m_requestsIface->requestableChannelClasses = getRequestableChannelList().bareClasses();

    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(m_requestsIface));

    setConnectCallback(Tp::memFun(this, &MatrixConnection::doConnect));
    setInspectHandlesCallback(Tp::memFun(this, &MatrixConnection::inspectHandles));
    setCreateChannelCallback(Tp::memFun(this, &MatrixConnection::createChannelCB));
    setRequestHandlesCallback(Tp::memFun(this, &MatrixConnection::requestHandles));

    m_user = parameters.value(QLatin1String("user")).toString();
    m_password = parameters.value(QLatin1String("password")).toString();
    m_deviceId = parameters.value(QLatin1String("device"), QStringLiteral("HomePC")).toString();
    m_server = parameters.value(QLatin1String("server"), QStringLiteral("https://matrix.org")).toString();

    /* Connection.Interface.Avatars */
    m_avatarsIface = Tp::BaseConnectionAvatarsInterface::create();
    m_avatarsIface->setAvatarDetails(MatrixConnection::getAvatarSpec());
    m_avatarsIface->setGetKnownAvatarTokensCallback(Tp::memFun(this, &MatrixConnection::getKnownAvatarTokens));
    m_avatarsIface->setRequestAvatarsCallback(Tp::memFun(this, &MatrixConnection::requestAvatars));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(m_avatarsIface));

    connect(this, &MatrixConnection::disconnected, this, &MatrixConnection::doDisconnect);
}

MatrixConnection::~MatrixConnection()
{
}

void MatrixConnection::doConnect(Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << m_user << m_password << m_deviceId;
    setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonRequested);

    m_connection = new Quotient::Connection(QUrl(m_server));
    connect(m_connection, &Quotient::Connection::connected, this, &MatrixConnection::onConnected);
    connect(m_connection, &Quotient::Connection::syncDone, this, &MatrixConnection::onSyncDone);
    connect(m_connection, &Quotient::Connection::loginError, [](const QString &error) {
        qDebug() << "Login error: " << error;
    });
//    connect(m_connection, &Quotient::Connection::networkError, [](size_t nextAttempt, int inMilliseconds) {
//        qDebug() << "networkError: " << nextAttempt << "millis" << inMilliseconds;
//    });
    connect(m_connection, &Quotient::Connection::resolveError, [](const QString &error) {
        qDebug() << "Resolve error: " << error;
    });
    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixConnection::processNewRoom);

    if (loadSessionData()) {
        qDebug() << Q_FUNC_INFO << "connectWithToken" << m_user << m_accessToken << m_deviceId;
        m_connection->connectWithToken(m_userId, QString::fromLatin1(m_accessToken), m_deviceId);
    } else {
        m_connection->connectToServer(m_user, m_password, m_deviceId);
    }
}

void MatrixConnection::doDisconnect()
{
    if (!m_connection) {
        return;
    }
    m_connection->stopSync();
    setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonRequested);
}

QStringList MatrixConnection::inspectHandles(uint handleType, const Tp::UIntList &handles, Tp::DBusError *error)
{
    QStringList *knownIds = nullptr;
    switch (handleType) {
    case Tp::HandleTypeContact:
        knownIds = &m_contactIds;
        break;
    case Tp::HandleTypeRoom:
        knownIds = &m_roomIds;
        break;
    default:
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("Unsupported handle type"));
        return {};
    }
    QStringList result;
    result.reserve(handles.count());
    for (const uint handle : handles) {
        if ((handle == 0) || (handle > static_cast<uint>(knownIds->count()))) {
            if (error) {
                error->set(TP_QT_ERROR_INVALID_HANDLE, QStringLiteral("Invalid handle"));
                return {};
            }
        }
        result.append(knownIds->at(handle - 1));
    }
    return result;
}

Tp::UIntList MatrixConnection::requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error)
{
    QStringList *knownIds = nullptr;
    switch (handleType) {
    case Tp::HandleTypeContact:
        knownIds = &m_contactIds;
        break;
    case Tp::HandleTypeRoom:
        knownIds = &m_roomIds;
        break;
    default:
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("Unsupported handle type"));
        return {};
    }
    Tp::UIntList result;
    result.reserve(identifiers.count());
    for (const QString &id : identifiers) {
        int handle = knownIds->indexOf(id) + 1;
        if (handle == 0) {
            if (error) {
                error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("Unknown identifier"));
                return {};
            }
        }
        result.append(handle);
    }
    return result;
}

Tp::BaseChannelPtr MatrixConnection::createChannelCB(const QVariantMap &request, Tp::DBusError *error)
{
    const RequestDetails details = request;

    if (details.channelType() == TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST) {
        return createRoomListChannel();
    }

    if (details.channelType() != TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("Unsupported channel type"));
        return Tp::BaseChannelPtr();
    }
    const Tp::HandleType targetHandleType = details.targetHandleType();
    const uint targetHandle = details.getTargetHandle(this);
    const QString targetID = details.getTargetIdentifier(this);

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
    case Tp::HandleTypeRoom:
        break;
    case Tp::HandleTypeNone:
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("Target handle type is not present in the request details"));
        return Tp::BaseChannelPtr();
    default:
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("Unknown target handle type"));
        return Tp::BaseChannelPtr();
    }

    if (targetID.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QStringLiteral("Unknown target identifier"));
        return Tp::BaseChannelPtr();
    }

    Quotient::Room *targetRoom = nullptr;
    if (targetHandleType == Tp::HandleTypeContact) {
        DirectContact contact = getDirectContact(targetHandle);
        if (!contact.isValid()) {
            error->set(TP_QT_ERROR_NOT_IMPLEMENTED, QStringLiteral("Requested single chat does not exist yet "
                                                                   "(and DirectChat creation is not supported yet"));
            return Tp::BaseChannelPtr();
        }
        targetRoom = contact.room;
    } else {
        targetRoom = getRoom(targetHandle);
    }

    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, details.channelType(), targetHandleType, targetHandle);
    baseChannel->setTargetID(targetID);
    baseChannel->setRequested(details.isRequested());

    if (details.channelType() == TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        qDebug() << Q_FUNC_INFO << "creating channel for the room:" << targetRoom;
        if (targetRoom) {
            MatrixMessagesChannelPtr messagesChannel = MatrixMessagesChannel::create(this, targetRoom, baseChannel.data());
            baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(messagesChannel));
        }
    }

    return baseChannel;
}

Tp::BaseChannelPtr MatrixConnection::createRoomListChannel()
{
    return Tp::BaseChannelPtr();
}

Tp::ContactAttributesMap MatrixConnection::getContactListAttributes(const QStringList &interfaces,
                                                                    bool hold, Tp::DBusError *error)
{
    Q_UNUSED(hold)
    return getContactAttributes(m_directContacts.keys(), interfaces, error);
}

Tp::ContactAttributesMap MatrixConnection::getContactAttributes(const Tp::UIntList &handles,
                                                                const QStringList &interfaces,
                                                                Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << handles << interfaces;
    Tp::ContactAttributesMap contactAttributes;

    for (auto handle : handles) {
        Quotient::User *user = getUser(handle);
        if (!user) {
            qWarning() << Q_FUNC_INFO << "No user for handle" << handle;
            continue;
        }
        contactAttributes[handle] = {};
        QVariantMap &attributes = contactAttributes[handle];

        // Attributes for all kind of contacts
        const QString id = handle == selfHandle() ? selfID() : user->id();
        attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = id;
        if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS)) {
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS + QLatin1String("/token")]
                    = QVariant::fromValue(user->avatarUrl().toString());
        }
        if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE)) {
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")]
                    = QVariant::fromValue(mkSimplePresence(MatrixPresence::Online));
        }
        if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING)) {
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING + QLatin1String("/alias")]
                    = QVariant::fromValue(getContactAlias(handle));
        }
        if (handle == selfHandle()) {
            continue;
        }

        // Attributes not applicable for the self contact
        if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST)) {
            const Tp::SubscriptionState state = m_directContacts.contains(handle) ? Tp::SubscriptionStateYes : Tp::SubscriptionStateNo;
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = state;
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = state;
        }
        // Attributes are taken by reference, no need to assign
    }
    qDebug() << contactAttributes;
    qDebug() << contactAttributes.count();
    return contactAttributes;
}

void MatrixConnection::requestSubscription(const Tp::UIntList &handles, const QString &message, Tp::DBusError *error)
{
}

Tp::AliasMap MatrixConnection::getAliases(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << contacts;
    Tp::AliasMap aliases;
    for (uint handle : contacts) {
        aliases[handle] = getContactAlias(handle);
    }
    return aliases;
}

QString MatrixConnection::getContactAlias(uint handle) const
{
    const Quotient::User *user = getUser(handle);
    if (!user) {
        return QString();
    }
    return user->displayname();
}

Tp::SimplePresence MatrixConnection::getPresence(uint handle)
{
    return {};
}

uint MatrixConnection::setPresence(const QString &status, const QString &message, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << status << "ret" << selfHandle();
    const Tp::SimpleStatusSpec spec = getSimpleStatusSpecMap().value(status);
    if (!spec.maySetOnSelf) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QStringLiteral("The requested presence can not be set on self contact"));
        return 0;
    }
    Tp::SimplePresence presence;
    presence.type = spec.type;
    presence.status = status;
    presence.statusMessage = message;
    m_simplePresenceIface->setPresences(Tp::SimpleContactPresences({{selfHandle(), presence}}));
    return selfHandle();
}

void MatrixConnection::onAboutToAddNewMessages(Quotient::RoomEventsRange events)
{
    for (auto &event : events) {
        Quotient::RoomMessageEvent *message = dynamic_cast<Quotient::RoomMessageEvent *>(event.get());
        if (message) {
            Quotient::Room *room = qobject_cast<Quotient::Room *>(sender());
            if (!room) {
                continue;
            }
            MatrixMessagesChannelPtr textChannel = getMatrixMessagesChannelPtr(room);
            if (!textChannel) {
                qDebug() << Q_FUNC_INFO << "Error, channel is not a TextChannel?";
                continue;
            }
            textChannel->processMessageEvent(message);
        }
    }
}

void MatrixConnection::onConnected()
{
    m_userId = m_connection->userId();

    uint selfId = ensureContactHandle(m_userId);
    if (selfId != 1) {
        qWarning() << "Self ID seems to be set too late";
    }
    setSelfContact(selfId, m_userId);

    setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);
    m_contactListIface->setContactListState(Tp::ContactListStateWaiting);

    qDebug() << Q_FUNC_INFO;
    saveSessionData();

    m_connection->syncLoop();
}

void MatrixConnection::onSyncDone()
{
    qDebug() << Q_FUNC_INFO;
    const auto rooms = m_connection->rooms(Quotient::JoinState::Join); // TODO: any state
    for (Quotient::Room *room : rooms) {
        processNewRoom(room);
    }
    m_contactListIface->setContactListState(Tp::ContactListStateSuccess);
}

void MatrixConnection::onUserAvatarChanged(Quotient::User *user)
{
    QByteArray outData;
    QBuffer output(&outData);
    const QImage ava = user->avatar(64, 64);
    qDebug() << Q_FUNC_INFO << ava.isNull();
    if (ava.isNull()) {
        return;
    }
    ava.save(&output, "png");
    m_avatarsIface->avatarRetrieved(ensureHandle(user), user->avatarUrl().toString(), outData, QStringLiteral("image/png"));
    qDebug() << Q_FUNC_INFO << "retrieved";
}

bool MatrixConnection::loadSessionData()
{
    qDebug() << Q_FUNC_INFO << QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath + m_user;
    QFile secretFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath + m_user);
    if (!secretFile.open(QIODevice::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << "Unable to open file" << "for account" << m_user;
        return false;
    }
    const QByteArray data = secretFile.readAll();
    qDebug() << Q_FUNC_INFO << m_user << "(" << data.size() << "bytes)";
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    const int format = doc.object().value("format").toInt();
    if (format > c_sessionDataFormat) {
        qWarning() << Q_FUNC_INFO << "Unsupported file format" << format;
        return false;
    }

    QJsonObject session = doc.object().value("session").toObject();
    m_accessToken = QByteArray::fromHex(session.value(QLatin1String("accessToken")).toVariant().toByteArray());
    m_userId = session.value(QLatin1String("userId")).toString();
    m_homeServer = session.value(QLatin1String("homeServer")).toString();
    m_deviceId = session.value(QLatin1String("deviceId")).toString();

    return !m_accessToken.isEmpty();
}

bool MatrixConnection::saveSessionData() const
{
    if (!m_connection) {
        return false;
    }

    QJsonObject sessionObject;
    sessionObject.insert(QLatin1String("accessToken"), QString::fromLatin1(m_connection->accessToken().toHex()));
    sessionObject.insert(QLatin1String("userId"), m_connection->userId());
    sessionObject.insert(QLatin1String("homeServer"), m_connection->homeserver().toString());
    sessionObject.insert(QLatin1String("deviceId"), m_connection->deviceId());

    QJsonObject rootObject;
    rootObject.insert("session", sessionObject);
    rootObject.insert("format", c_sessionDataFormat);
    QJsonDocument doc(rootObject);

    const QByteArray data = doc.toJson(QJsonDocument::Indented);

    QDir dir;
    dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath);
    QFile secretFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath + m_user);
    if (!secretFile.open(QIODevice::WriteOnly)) {
        qWarning() << Q_FUNC_INFO << "Unable to save the session data to file" << "for account" << m_user;
        return false;
    }

    qDebug() << Q_FUNC_INFO << m_user << "(" << data.size() << "bytes)";
    return secretFile.write(data) == data.size();
}

void MatrixConnection::processNewRoom(Quotient::Room *room)
{
    qDebug() << Q_FUNC_INFO << room;
    qDebug() << room->displayName() << room->topic();
    qDebug() << room->memberNames();
    if (room->isDirectChat()) {
        // Single user room
        for (Quotient::User *user : room->users()) {
            if (user == room->localUser()) {
                continue;
            }
            ensureDirectContact(user, room);
        }
    } else {
        ensureHandle(room);
    }
    connect(room, &Quotient::Room::aboutToAddNewMessages,
            this, &MatrixConnection::onAboutToAddNewMessages,
            Qt::UniqueConnection);
}

uint MatrixConnection::ensureDirectContact(Quotient::User *user, Quotient::Room *room)
{
    qDebug() << Q_FUNC_INFO << user->id() << user->displayname();
    const uint handle = ensureHandle(user);
    m_directContacts.insert(handle, DirectContact(user, room));
    return handle;
}


MatrixMessagesChannelPtr MatrixConnection::getMatrixMessagesChannelPtr(Quotient::Room *room)
{
    MatrixMessagesChannelPtr textChannel;
    uint handleType = room->isDirectChat() ? Tp::HandleTypeContact : Tp::HandleTypeRoom;
    uint handle = room->isDirectChat() ? getDirectContactHandle(room) : getRoomHandle(room);

    if (!handle) {
        qWarning() << Q_FUNC_INFO << "Unknown room" << room->id();
        return textChannel;
    }

    bool yoursChannel;
    Tp::DBusError error;
    Tp::BaseChannelPtr channel = ensureChannel(
                QVariantMap({
                                { TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType"), handleType },
                                { TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"), handle },
                                { TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType"), TP_QT_IFACE_CHANNEL_TYPE_TEXT },
                            }),
                yoursChannel, /* suppress handle */ false, &error);

    if (error.isValid()) {
        qWarning() << "ensureChannel failed:" << error.name() << " " << error.message();
        return textChannel;
    }

    textChannel = MatrixMessagesChannelPtr::dynamicCast(channel->interface(TP_QT_IFACE_CHANNEL_TYPE_TEXT));
    return textChannel;
}

void MatrixConnection::prefetchHistory(Quotient::Room *room)
{
    if (room->messageEvents().begin() == room->messageEvents().end()) {
        return;
    }

    MatrixMessagesChannelPtr textChannel = getMatrixMessagesChannelPtr(room);

    if (!textChannel) {
        qDebug() << "Error, channel is not a TextChannel?";
        return;
    }
    textChannel->fetchHistory();
}

Quotient::User *MatrixConnection::getUser(uint handle) const
{
    if (handle == 0 || handle > static_cast<uint>(m_contactIds.count())) {
        qWarning() << Q_FUNC_INFO << "Invalid handle";
        return nullptr;
    }
    if (handle == selfHandle()) {
        return m_connection->user();
    }
    const QString id = m_contactIds.at(handle - 1);
    return m_connection->user(id);
}

Quotient::User *MatrixConnection::getUser(const QString &id) const
{
    if (id == selfID()) {
        return m_connection->user();
    }
    return m_connection->user(id);
}

DirectContact MatrixConnection::getDirectContact(uint contactHandle) const
{
    return m_directContacts.value(contactHandle);
}

Quotient::Room *MatrixConnection::getRoom(uint handle) const
{
    if (handle == 0 || handle > static_cast<uint>(m_roomIds.count())) {
        qWarning() << Q_FUNC_INFO << "Invalid handle";
        return nullptr;
    }
    const QString id = m_roomIds.at(handle - 1);
    return m_connection->room(id);
}

uint MatrixConnection::getContactHandle(Quotient::User *user)
{
    return m_contactIds.indexOf(user->id()) + 1;
}

uint MatrixConnection::getDirectContactHandle(Quotient::Room *room)
{
    for (uint handle : m_directContacts.keys()) {
        if (m_directContacts.value(handle).room == room) {
            return handle;
        }
    }
    return 0;
}

uint MatrixConnection::getRoomHandle(Quotient::Room *room)
{
    return m_roomIds.indexOf(room->id()) + 1;
}

uint MatrixConnection::ensureHandle(Quotient::User *user)
{
    uint index = getContactHandle(user);
    if (index != 0) {
        return index;
    }
    m_contactIds.append(user->id());
    return m_contactIds.count();
}

uint MatrixConnection::ensureHandle(Quotient::Room *room)
{
    uint index = getRoomHandle(room);
    if (index != 0) {
        return index;
    }
    m_roomIds.append(room->id());
    return m_roomIds.count();
}

uint MatrixConnection::ensureContactHandle(const QString &identifier)
{
    int index = m_contactIds.indexOf(identifier);
    if (index < 0) {
        m_contactIds.append(identifier);
        return m_contactIds.count();
    }
    return index + 1;
}

void MatrixConnection::requestAvatars(const Tp::UIntList &handles, Tp::DBusError *error)
{
    requestAvatarsImpl(handles);
}

Tp::AvatarTokenMap MatrixConnection::getKnownAvatarTokens(const Tp::UIntList &handles, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << handles;
    if (error->isValid()) {
        return {};
    }
    Tp::AvatarTokenMap result;
    for (uint handle : handles) {
        const Quotient::User *user = getUser(handle);
        if (user && user->avatarUrl().isValid()) {
            result.insert(handle, user->avatarUrl().toString());
        }
    }
    return result;
}

void MatrixConnection::requestAvatarsImpl(const Tp::UIntList &handles)
{
    qDebug() << Q_FUNC_INFO << handles;
    for (auto handle : handles) {
        Quotient::User *user = getUser(handle);
        if (!user) {
            continue;
        }
        connect(user, &Quotient::User::avatarChanged, this, &MatrixConnection::onUserAvatarChanged);
        onUserAvatarChanged(user);
    }
}
