/*
    p2p.h - msn p2p protocol

    Copyright (c) 2003-2005 by Olivier Goffart        <ogoffart@kde.org>
    Copyright (c) 2005      by Gregg Edghill          <gregg.edghill@gmail.com>

    Kopete    (c) 2002-2007 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef P2P_H
#define P2P_H

// Qt includes
#include <qobject.h>
#include "messageformatter.h"

#include "kopete_export.h"

#include <config-kopete.h> // probably not needed?

namespace Kopete { class Transfer; }
namespace Kopete { class FileTransferInfo; }
namespace P2P { class Dispatcher; }
namespace KNetwork { class KBufferedSocket; }
class QFile;

/**
@author Kopete Developers
*/
namespace System{
	class Guid
	{
		public:
			~Guid(){}
			static Guid newGuid();
			QString toString();

		private:
			Guid(){}
	};
}

namespace P2P{

	enum TransferType { UserDisplayIcon = 1, File = 2, WebcamType=4};
	enum TransferDirection { Incoming = 1, Outgoing = 8};
	enum MessageType { BYE, OK, DECLINE, ERROR, INVITE };

	enum CommunicationState
	{
		Invitation   = 1,
		Negotiation  = 2,
		DataTransfer = 8,
		Finished     = 16
	};

	struct TransportHeader
	{
		quint32 sessionId;
		quint32 identifier;
		qint64  dataOffset;
		qint64  totalDataSize;
		quint32 dataSize;
		quint32 flag;
		quint32 ackSessionIdentifier;
		quint32 ackUniqueIdentifier;
		qint64  ackDataSize;
	};

	struct Message
	{
		public:
			QString mimeVersion;
			QString contentType;
			QString destination;
			QString source;
			TransportHeader header;
			QByteArray body;
			qint32 applicationIdentifier;
			bool attachApplicationIdentifier;
	};

	class Uid
	{
		public: static QString createUid();
	};

	class KOPETE_MSN_SHARED_EXPORT TransferContext : public QObject
	{	Q_OBJECT
		public:
			virtual ~TransferContext();

			void acknowledge(const Message& message);
			virtual void acknowledged() = 0;
			void error();
			virtual void processMessage(const P2P::Message& message) = 0;
			void sendDataPreparation();
			void sendMessage(MessageType type, const QString& content=QString(), qint32 flag=0, qint32 appId=0);
			void setType(TransferType type);

		public:
			quint32 m_sessionId;
			quint32 m_identifier;
			QFile   *m_file;
			quint32 m_transactionId;
			quint32 m_ackSessionIdentifier;
			quint32 m_ackUniqueIdentifier;
			Kopete::Transfer *m_transfer;
			QString  m_branch;
			QString  m_callId;
			QString  m_object;


		public slots:
			void abort();
			void readyWrite();

		protected:
			TransferContext(const QString& contact, P2P::Dispatcher *dispatcher,quint32 sessionId);
			void sendData(const QByteArray& bytes);
			void sendMessage(P2P::Message& outbound, const QByteArray& body);
			virtual void readyToSend();

			quint32 m_baseIdentifier;
			TransferDirection m_direction;
			P2P::Dispatcher *m_dispatcher;
			bool m_isComplete;
			qint64 m_offset;
			qint64 m_totalDataSize;
			P2P::MessageFormatter m_messageFormatter;
			QString m_recipient;
			QString m_sender;
			KNetwork::KBufferedSocket *m_socket;
   			CommunicationState m_state;
   			TransferType m_type;
	};
}

#endif