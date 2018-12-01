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

// QMatrixClient
#include <connection.h>
#include <room.h>
#include <user.h>

MatrixMessagesChannel::MatrixMessagesChannel(MatrixConnection *connection, QMatrixClient::Room *room, Tp::BaseChannel *baseChannel)
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
    // m_messagesIface->setSendMessageCallback(Tp::memFun(this, &MatrixMessagesChannel::sendMessage));

    // m_chatStateIface = Tp::BaseChannelChatStateInterface::create();
    // m_chatStateIface->setSetChatStateCallback(Tp::memFun(this, &MatrixMessagesChannel::setChatState));
    // baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_chatStateIface));

    if (m_targetHandleType == Tp::HandleTypeRoom) {
        Tp::ChannelGroupFlags groupFlags = 0;
        m_groupIface = Tp::BaseChannelGroupInterface::create();
        m_groupIface->setGroupFlags(groupFlags);
        m_groupIface->setSelfHandle(connection->selfHandle());
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_groupIface));

        // We have to plug the iface before use set members
        Tp::UIntList members;
        members.reserve(m_room->users().count());
        for (QMatrixClient::User *member : m_room->users()) {
            members.append(m_connection->ensureHandle(member));
        }
        m_groupIface->setMembers(members, {});


        m_roomIface = Tp::BaseChannelRoomInterface::create(m_room->displayName(),
                                                           /* server name */ QString(),
                                                           /* creator */ QString(),
                                                           /* creatorHandle */ 0,
                                                           /* creationTimestamp */ QDateTime());
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomIface));
    }
}

void MatrixMessagesChannel::onPendingEventChanged(int pendingEventIndex)
{
    // Delivery Report message
    // https://telepathy.freedesktop.org/spec/Channel_Interface_Messages.html#Enum:Delivery_Status
    // https://matrix.org/docs/spec/client_server/r0.4.0.html#put-matrix-client-r0-rooms-roomid-send-eventtype-txnid

    const QMatrixClient::PendingEventItem &pendingEvent = m_room->pendingEvents().at(pendingEventIndex);
    Tp::DeliveryStatus tpDeliveryStatus;
    switch (pendingEvent.deliveryStatus()) {
    case QMatrixClient::EventStatus::ReachedServer:
        tpDeliveryStatus = Tp::DeliveryStatusAccepted;
        break;
    case QMatrixClient::EventStatus::SendingFailed:
        tpDeliveryStatus = Tp::DeliveryStatusTemporarilyFailed;
        break;
    default:
        tpDeliveryStatus = Tp::DeliveryStatusUnknown;
        break;
    }

    Tp::MessagePartList partList;

    Tp::MessagePart header;
    header[QStringLiteral("message-sender")]    = QDBusVariant(m_targetHandle);
    header[QStringLiteral("message-sender-id")] = QDBusVariant(m_targetId);
    header[QStringLiteral("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeDeliveryReport);
    header[QStringLiteral("delivery-status")]   = QDBusVariant(tpDeliveryStatus);
    header[QStringLiteral("delivery-token")]    = QDBusVariant(pendingEvent.event()->transactionId());
    partList << header;

    addReceivedMessage(partList);
}

MatrixMessagesChannelPtr MatrixMessagesChannel::create(MatrixConnection *connection, QMatrixClient::Room *room, Tp::BaseChannel *baseChannel)
{
    return MatrixMessagesChannelPtr(new MatrixMessagesChannel(connection, room, baseChannel));
}

QString MatrixMessagesChannel::sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error)
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

    connect(m_room, &QMatrixClient::Room::pendingEventChanged, this, &MatrixMessagesChannel::onPendingEventChanged);
    QString txnId = m_room->postPlainText(content);
    return txnId;
}

void MatrixMessagesChannel::processMessageEvent(const QMatrixClient::RoomMessageEvent *event)
{
    QJsonDocument doc(event->originalJsonObject());
    qDebug().noquote() << Q_FUNC_INFO << "Process message" << doc.toJson(QJsonDocument::Indented);
    Tp::MessagePart header;
    // mark all messages as scrollback
    header[QStringLiteral("scrollback")] = QDBusVariant(true);
    header[QStringLiteral("message-token")] = QDBusVariant(event->id());
    header[QStringLiteral("message-sent")]  = QDBusVariant(event->timestamp().toMSecsSinceEpoch() / 1000);
    header[QStringLiteral("message-received")] = QDBusVariant(event->timestamp().toMSecsSinceEpoch() / 1000);
    if (event->senderId() == m_connection->matrix()->user()->id()) {
        header[QStringLiteral("message-sender")]   = QDBusVariant(m_connection->selfHandle());
        header[QStringLiteral("message-sender-id")] = QDBusVariant(m_connection->selfID());
    } else {
        header[QStringLiteral("message-sender")]   = QDBusVariant(m_connection->ensureContactHandle(event->senderId()));
        header[QStringLiteral("message-sender-id")] = QDBusVariant(event->senderId());
    }

    /* Text message */
    Tp::MessagePartList body;
    Tp::MessagePart text;

    const QJsonObject content = event->originalJsonObject().value(QLatin1String("content")).toObject();
    const QString format = content.value(QLatin1String("format")).toString();
    if (format == QLatin1String("org.matrix.custom.html")) {
        static const QRegularExpression replyPattern("^.{0,3}<a href=\"(?<url>.*)\">Reply to (?<replyTo>.*)</a>:.{0,4}"
                                                     "<br.{0,2}><blockquote>(?<quote>.*)</blockquote>"
                                                     "<p>(?<message>.*)</p>");
        static const bool optimize = [](std::initializer_list<const QRegularExpression *> expressions) {
            for (const QRegularExpression *expr : expressions) {
                expr->optimize();
            }
            return true;
        }({&replyPattern});
        Q_UNUSED(optimize)

        /* Typical formatted_body looks like this:
         * "<i>
         * <a href=\"https://matrix.to/#/!aUBwzaVnlGSXhlADmf:gazizova.net/$152529106016730XcGge:t2bot.io\">Reply to username</a>:
         * </i>
         * <br />
         * <blockquote>The original message text</blockquote>
         * <p>This message text</p>" */

        const QString formatted = content.value(QLatin1String("formatted_body")).toString();
        const QRegularExpressionMatch match = replyPattern.match(formatted);
        qDebug().noquote() << "treat" << formatted << "against" << replyPattern.pattern() << match.isValid();
        if (match.isValid()) {
            const QString url = match.captured("url");
            // const QString replyTo = match.capturedRef("replyTo");
            // const QString quote = match.captured("quote");
            // const QString message = match.captured("message");
            const int thisRoomIndex = url.indexOf(m_targetId);
            qDebug() << "URL:" << url << thisRoomIndex << "this room: " << thisRoomIndex;
            if (thisRoomIndex > 0) {
                const QString replyMessageToken = url.mid(m_targetId.length() + thisRoomIndex + 1);
                header[QStringLiteral("reply-to-message-token")] = QDBusVariant(replyMessageToken);
            }
            // This is not going to work as-is because of bug in TelepathyQt
            // Tp::MessagePart replyInterfaceText;
            // replyInterfaceText[QStringLiteral("content-type")] = QDBusVariant(QStringLiteral("text/plain"));
            // replyInterfaceText[QStringLiteral("alternative")] = QDBusVariant(QStringLiteral("reply-to"));
            // replyInterfaceText[QStringLiteral("content")] = QDBusVariant(replyTo);
            // replyInterfaceText[QStringLiteral("interface")] = QDBusVariant(TP_QT_IFACE_CHANNEL + ".Interface.Reply");
            //
            // Tp::MessagePart replyPlainText;
            // replyPlainText[QStringLiteral("content-type")] = QDBusVariant(QStringLiteral("text/plain"));
            // replyPlainText[QStringLiteral("alternative")] = QDBusVariant(QStringLiteral("reply-to"));
            // replyPlainText[QStringLiteral("content")] = QDBusVariant(QStringLiteral("Reply to %1:/n").arg(replyTo));
            //
            // Tp::MessagePart quoteText;
            // quoteText[QStringLiteral("content-type")] = QDBusVariant(QStringLiteral("text/plain"));
            // quoteText[QStringLiteral("alternative")] = QDBusVariant(QStringLiteral("reply-quote"));
            // quoteText[QStringLiteral("content")] = QDBusVariant(quote);
            // quoteText[QStringLiteral("interface")] = QDBusVariant(TP_QT_IFACE_CHANNEL + ".Interface.Reply");
            //
            text[QStringLiteral("content-type")] = QDBusVariant(QStringLiteral("text/plain"));
            // text[QStringLiteral("content")]      = QDBusVariant(message);
            text[QStringLiteral("content")]      = QDBusVariant(event->plainBody());
        }
    } else {
        text[QStringLiteral("content-type")] = QDBusVariant(QStringLiteral("text/plain"));
        text[QStringLiteral("content")]      = QDBusVariant(event->plainBody());
    }
    body << text;

    Tp::MessagePartList partList;
    header[QStringLiteral("message-type")]  = QDBusVariant(Tp::ChannelTextMessageTypeNormal);
    partList << header << body;
    addReceivedMessage(partList);
}

void MatrixMessagesChannel::fetchHistory()
{
    for (auto eventIt = m_room->messageEvents().begin(); eventIt < m_room->messageEvents().end(); ++eventIt) {
        const QMatrixClient::RoomMessageEvent *event = eventIt->viewAs<QMatrixClient::RoomMessageEvent>();
        if (event) {
            processMessageEvent(event);
        }
    }
}
