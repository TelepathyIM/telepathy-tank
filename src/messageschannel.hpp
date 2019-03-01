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

#ifndef TANK_MESSAGES_CHANNEL_HPP
#define TANK_MESSAGES_CHANNEL_HPP

#include <QPointer>

#include <TelepathyQt/BaseChannel>
#include <events/roommessageevent.h>

class QTimer;

class MatrixMessagesChannel;
class MatrixConnection;

namespace QMatrixClient
{
// TODO: Cleanup
class Room;
class User;
class RoomEvent;
class ConnectionData;

class SyncJob;
class SyncData;
class RoomMessageEvent;
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

typedef Tp::SharedPtr<MatrixMessagesChannel> MatrixMessagesChannelPtr;

class MatrixMessagesChannel : public Tp::BaseChannelTextType
{
    Q_OBJECT
public:
    static MatrixMessagesChannelPtr create(MatrixConnection *connection, QMatrixClient::Room *room, Tp::BaseChannel *baseChannel);

    QString sendMessage(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error);
    // void messageAcknowledged(const QString &messageId);
    void setChatState(uint state, Tp::DBusError *error);

    void fetchHistory();
    void processMessageEvent(const QMatrixClient::RoomMessageEvent *event);

private:
    MatrixMessagesChannel(MatrixConnection *connection, QMatrixClient::Room *room, Tp::BaseChannel *baseChannel);

    void sendDeliveryReport(Tp::DeliveryStatus tpDeliveryStatus, const QString &deliveryToken);
    void onPendingEventChanged(int pendingEventIndex);
    void onReadMarkerForUserMoved(QMatrixClient::User* user, QString fromEventId, QString toEventId);
    void onTypingChanged();
    void reactivateLocalTyping();
    void sendChatStateNotification(uint state);

    MatrixConnection *m_connection = nullptr;
    QMatrixClient::Room *m_room = nullptr;

    uint m_targetHandle;
    uint m_targetHandleType;
    uint m_selfHandle;
    QString m_targetId;

    Tp::BaseChannelTextTypePtr m_channelTextType;
    Tp::BaseChannelMessagesInterfacePtr m_messagesIface;
    Tp::BaseChannelChatStateInterfacePtr m_chatStateIface;
    Tp::BaseChannelGroupInterfacePtr m_groupIface;
    Tp::BaseChannelRoomInterfacePtr m_roomIface;
    Tp::BaseChannelRoomConfigInterfacePtr m_roomConfigIface;

    QTimer *m_localTypingTimer = nullptr;
};

#endif // TANK_MESSAGES_CHANNEL_HPP
