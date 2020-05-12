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

#include "messageschannel.hpp"
#include "connection.hpp"

#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>
#include <QJsonDocument>

// Quotient
#include <connection.h>
#include <room.h>
#include <user.h>
#include <csapi/typing.h>
#include <events/typingevent.h>

MatrixMessagesChannel::MatrixMessagesChannel(MatrixConnection *connection, Quotient::Room *room, Tp::BaseChannel *baseChannel)
    : Tp::BaseChannelTextType(baseChannel),
      m_connection(connection),
      m_room(room),
      m_targetHandle(baseChannel->targetHandle()),
      m_targetHandleType(baseChannel->targetHandleType()),
      m_targetId(baseChannel->targetID())
{
    QStringList supportedContentTypes = QStringList() << QStringLiteral("text/plain");
    const QList<uint> messageTypes = {
        Tp::ChannelTextMessageTypeNormal,
        Tp::ChannelTextMessageTypeDeliveryReport,
    };

    uint messagePartSupportFlags = Tp::MessageSendingFlagReportDelivery | Tp::MessageSendingFlagReportRead;
    uint deliveryReportingSupport = Tp::DeliveryReportingSupportFlagReceiveFailures
            | Tp::DeliveryReportingSupportFlagReceiveSuccesses
            | Tp::DeliveryReportingSupportFlagReceiveRead;

    // setMessageAcknowledgedCallback(Tp::memFun(this, &MatrixMessagesChannel::messageAcknowledged));

    m_messagesIface = Tp::BaseChannelMessagesInterface::create(this,
                                                               supportedContentTypes,
                                                               messageTypes,
                                                               messagePartSupportFlags,
                                                               deliveryReportingSupport);

    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_messagesIface));
    m_messagesIface->setSendMessageCallback(Tp::memFun(this, &MatrixMessagesChannel::sendMessage));

    m_chatStateIface = Tp::BaseChannelChatStateInterface::create();
    m_chatStateIface->setSetChatStateCallback(Tp::memFun(this, &MatrixMessagesChannel::setChatState));
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_chatStateIface));

    if (m_targetHandleType == Tp::HandleTypeRoom) {
        Tp::ChannelGroupFlags groupFlags = 0;
        m_groupIface = Tp::BaseChannelGroupInterface::create();
        m_groupIface->setGroupFlags(groupFlags);
        m_groupIface->setSelfHandle(connection->selfHandle());
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_groupIface));

        // We have to plug the iface before use set members
        Tp::UIntList members;
        members.reserve(m_room->users().count());
        for (Quotient::User *member : m_room->users()) {
            members.append(m_connection->ensureHandle(member));
        }
        m_groupIface->setMembers(members, {});


        m_roomIface = Tp::BaseChannelRoomInterface::create(m_room->displayName(),
                                                           /* server name */ QString(),
                                                           /* creator */ QString(),
                                                           /* creatorHandle */ 0,
                                                           /* creationTimestamp */ QDateTime());
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomIface));
        
        m_roomConfigIface = Tp::BaseChannelRoomConfigInterface::create();
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomConfigIface));
        m_roomConfigIface->setTitle(room->displayName());
        m_roomConfigIface->setDescription(room->topic());
    }

    connect(m_room, &Quotient::Room::pendingEventChanged, this, &MatrixMessagesChannel::onPendingEventChanged);
    connect(m_room, &Quotient::Room::typingChanged, this, &MatrixMessagesChannel::onTypingChanged);
    connect(m_room, &Quotient::Room::readMarkerForUserMoved, this, &MatrixMessagesChannel::onReadMarkerForUserMoved);
    connect(m_room, &Quotient::Room::displaynameChanged, this, &MatrixMessagesChannel::onDisplayNameChanged);
    connect(m_room, &Quotient::Room::topicChanged, this, &MatrixMessagesChannel::onTopicChanged);
}

void MatrixMessagesChannel::sendDeliveryReport(Tp::DeliveryStatus tpDeliveryStatus, const QString &deliveryToken)
{
    Tp::MessagePartList partList;
    
    Tp::MessagePart header;
    header[QStringLiteral("message-sender")]    = QDBusVariant(m_targetHandle);
    header[QStringLiteral("message-sender-id")] = QDBusVariant(m_targetId);
    header[QStringLiteral("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeDeliveryReport);
    header[QStringLiteral("delivery-status")]   = QDBusVariant(tpDeliveryStatus);
    header[QStringLiteral("delivery-token")]    = QDBusVariant(deliveryToken);
    partList << header;

    addReceivedMessage(partList);
}

void MatrixMessagesChannel::onPendingEventChanged(int pendingEventIndex)
{
    // Delivery Report message
    // https://telepathy.freedesktop.org/spec/Channel_Interface_Messages.html#Enum:Delivery_Status
    // https://matrix.org/docs/spec/client_server/r0.4.0.html#put-matrix-client-r0-rooms-roomid-send-eventtype-txnid

    const Quotient::PendingEventItem &pendingEvent = m_room->pendingEvents().at(pendingEventIndex);
    Tp::DeliveryStatus tpDeliveryStatus;
    switch (pendingEvent.deliveryStatus()) {
    case Quotient::EventStatus::ReachedServer:
        tpDeliveryStatus = Tp::DeliveryStatusAccepted;
        break;
    case Quotient::EventStatus::SendingFailed:
        tpDeliveryStatus = Tp::DeliveryStatusTemporarilyFailed;
        break;
    default:
        tpDeliveryStatus = Tp::DeliveryStatusUnknown;
        break;
    }

    sendDeliveryReport(tpDeliveryStatus, pendingEvent.event()->id());
}

void MatrixMessagesChannel::onReadMarkerForUserMoved(Quotient::User *user, const QString &fromEventId, const QString &toEventId)
{
    Q_UNUSED(user);
    QStringList tokens;
    for (auto eventIt = m_room->findInTimeline(fromEventId); eventIt < m_room->findInTimeline(toEventId); ++eventIt) {
        sendDeliveryReport(Tp::DeliveryStatusRead, eventIt->event()->id());
        tokens.append(eventIt->event()->id());
    }
#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 8)
    Tp::DBusError error;
    acknowledgePendingMessages(tokens, &error);
#endif // TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 8)
}

void MatrixMessagesChannel::onDisplayNameChanged(Quotient::Room *room, const QString &oldName)
{
    m_roomConfigIface->setTitle(room->displayName());
}

void MatrixMessagesChannel::onTopicChanged()
{
    m_roomConfigIface->setDescription(m_room->topic());
}

void MatrixMessagesChannel::sendChatStateNotification(uint state)
{
    m_room->connection()->
            callApi<Quotient::SetTypingJob>
            (Quotient::BackgroundRequest,
             m_connection->matrix()->user()->id(), m_room->id(), (state == Tp::ChannelChatStateComposing));
}

MatrixMessagesChannelPtr MatrixMessagesChannel::create(MatrixConnection *connection, Quotient::Room *room, Tp::BaseChannel *baseChannel)
{
    return MatrixMessagesChannelPtr(new MatrixMessagesChannel(connection, room, baseChannel));
}

QString MatrixMessagesChannel::sendMessage(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error)
{
    QString content;
    for (const Tp::MessagePart &part : messageParts) {
        if (part.contains(QStringLiteral("content-type"))
                && part.value(QStringLiteral("content-type")).variant().toString() == QStringLiteral("text/plain")
                && part.contains(QStringLiteral("content"))) {
            content = part.value(QStringLiteral("content")).variant().toString();
            break;
        }
    }

    QString txnId = m_room->postPlainText(content);
    return txnId;
}

void MatrixMessagesChannel::processMessageEvent(const Quotient::RoomMessageEvent *event)
{
    QJsonDocument doc(event->originalJsonObject());
    qDebug().noquote() << Q_FUNC_INFO << "Process message" << doc.toJson(QJsonDocument::Indented);
    bool silent = true;
    Tp::MessagePart header;
    header[QStringLiteral("message-token")] = QDBusVariant(event->id());
    header[QStringLiteral("message-sent")] = QDBusVariant(event->timestamp().toMSecsSinceEpoch() / 1000);
    header[QStringLiteral("message-received")] = QDBusVariant(event->timestamp().toMSecsSinceEpoch() / 1000);
    header[QStringLiteral("message-type")] = QDBusVariant(Tp::ChannelTextMessageTypeNormal);
    if (event->senderId() == m_connection->matrix()->user()->id()) {
        header[QStringLiteral("message-sender")] = QDBusVariant(m_connection->selfHandle());
        header[QStringLiteral("message-sender-id")] = QDBusVariant(m_connection->selfID());
    } else {
        header[QStringLiteral("message-sender")] = QDBusVariant(m_connection->ensureContactHandle(event->senderId()));
        header[QStringLiteral("message-sender-id")] = QDBusVariant(event->senderId());
        if (m_targetHandleType == Tp::HandleTypeContact)
            silent = false;
    }

    /* Redacted deleted message */
    // https://matrix.org/docs/spec/client_server/r0.4.0.html#id259
    if (event->isRedacted())
        header[QStringLiteral("delivery-status")] = QDBusVariant(Tp::DeliveryStatusDeleted);

    /* Read markers */
    QList<Quotient::User*> usersAtEventId = m_room->usersAtEventId(event->id());
    const int selfReceipts = usersAtEventId.contains(m_connection->matrix()->user()) ? 1 : 0;
    const bool hasOtherReceipts = usersAtEventId.count() > selfReceipts;
    if (hasOtherReceipts) {
        header[QStringLiteral("delivery-status")] = QDBusVariant(Tp::DeliveryStatusRead);
    }
    
    if (silent) {
        header[QStringLiteral("silent")] = QDBusVariant(silent);
    }
    
    /* Text message */
    Tp::MessagePartList body;
    Tp::MessagePart text;

    text[QStringLiteral("content-type")] = QDBusVariant(QStringLiteral("text/plain"));
    text[QStringLiteral("content")] = QDBusVariant(event->isRedacted() ? event->redactionReason() : event->plainBody());
    body << text;

    Tp::MessagePartList partList;
    partList << header << body;
    addReceivedMessage(partList);
}

void MatrixMessagesChannel::fetchHistory()
{
    for (auto eventIt = m_room->messageEvents().begin(); eventIt < m_room->messageEvents().end(); ++eventIt) {
        const Quotient::RoomMessageEvent *event = eventIt->viewAs<Quotient::RoomMessageEvent>();
        if (event) {
            processMessageEvent(event);
        }
    }
}

void MatrixMessagesChannel::onTypingChanged()
{
    if (m_room->usersTyping().isEmpty()) {
        for (auto user: m_room->users()) {
            const uint handle = m_connection->ensureContactHandle(user->id());
            m_chatStateIface->chatStateChanged(handle, Tp::ChannelChatStateActive);
        }
        return;
    }
    for (auto user: m_room->usersTyping()) {
        const uint handle = m_connection->ensureContactHandle(user->id());
        m_chatStateIface->chatStateChanged(handle, Tp::ChannelChatStateComposing);
    }
}

void MatrixMessagesChannel::reactivateLocalTyping()
{
    sendChatStateNotification(Tp::ChannelChatStateComposing);
}

void MatrixMessagesChannel::setChatState(uint state, Tp::DBusError *error)
{
    Q_UNUSED(error);

    if (!m_localTypingTimer) {
        m_localTypingTimer = new QTimer(this);
        constexpr int c_chatStateResendInterval = 5000;
        m_localTypingTimer->setInterval(c_chatStateResendInterval);
        connect(m_localTypingTimer, &QTimer::timeout, this, &MatrixMessagesChannel::reactivateLocalTyping);
    }

    if (state == Tp::ChannelChatStateComposing) {
        m_localTypingTimer->start();
    } else {
        m_localTypingTimer->stop();
    }
    
    m_room->markAllMessagesAsRead();
    
    sendChatStateNotification(state);
}
