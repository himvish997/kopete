/*
    oscarconnection.h  -  Implementation of an oscar connection

    Copyright (c) 2002 by Tom Linsky <twl6@po.cwru.edu>
    Kopete    (c) 2002-2003 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "oscarconnection.h"

#include <kdebug.h>
#include <klocale.h>

OscarConnection::OscarConnection(const QString &connName, ConnectionType type,
	const QByteArray &cookie, QObject *parent, const char *name) : QObject(parent, name)
{
	mConnName = connName;
	mConnType = type;
	//mSN = sn;
	mCookie.duplicate(cookie);


	#ifdef USE_KEXTSOCK
	mSocket = new KExtendedSocket();
	mSocket->setSocketFlags(KExtendedSocket::inetSocket |
		KExtendedSocket::bufferedSocket);

	connect(mSocket, SIGNAL(connectionSuccess()),
		this, SLOT(slotSocketConnected()));
	connect(mSocket, SIGNAL(connectionFailed(int)),
		this, SLOT(slotSocketError(int)));
	connect(mSocket, SIGNAL(closed(int)), this, SLOT(slotSocketClosed()));
	mSocket->enableWrite(false);
	mSocket->enableRead(true);

	#else // libqt-addon

	mSocket = new QBufferedSocket(QString::null, QString::null, this, "mSocket");
	//mSocket->setSocketFlags(QSocketBase::Keepalive);

	connect(mSocket, SIGNAL(connected(const QResolverEntry &)),
		this, SLOT(slotSocketConnected()));
	connect(mSocket, SIGNAL(gotError(int)), this, SLOT(slotSocketError(int)));
	connect(mSocket, SIGNAL(closed()), this, SLOT(slotSocketClosed()));

	#endif

	connect(mSocket, SIGNAL(readyRead()), this, SLOT(slotRead()));
}


OscarConnection::~OscarConnection()
{
//	kdDebug(14150) << k_funcinfo << "Called." << endl;
}


void OscarConnection::setSN(const QString &newSN)
{
	mSN = newSN;
}


const QString &OscarConnection::getSN() const
{
	return mSN;
}


OscarConnection::ConnectionStatus OscarConnection::socketStatus() const
{
	#ifdef USE_KEXTSOCK
	switch(mSocket->socketStatus())
	{
		case (KExtendedSocket::connecting):
			return Connecting;
		case (KExtendedSocket::connected):
			return Connected;
		default:
			break;
	}

	#else

	switch(mSocket->state())
	{
		case (QClientSocketBase::HostLookup):
		case (QClientSocketBase::Connecting):
			return Connecting;
		case (QClientSocketBase::Open):
			return Connected;
		default:
			break;
	}
	#endif

	return Disconnected;
}


void OscarConnection::connectTo(const QString &host, const QString &port)
{
	#ifdef USE_KEXTSOCK
	mSocket->setAddress(host, port);
	mSocket->connect();
	#else
	mSocket->connect(host, port);
	#endif
}


QString OscarConnection::localHost() const
{
	#ifdef USE_KEXTSOCK
	return mSocket->localAddress()->nodeName();
	#else
	return mSocket->localResults().nodeName();
	#endif
}


QString OscarConnection::localPort() const
{
	#ifdef USE_KEXTSOCK
	return mSocket->localAddress()->serviceName();
	#else
	return mSocket->localResults().serviceName();
	#endif
}


QString OscarConnection::peerHost() const
{
	#ifdef USE_KEXTSOCK
	return mSocket->host();
	#else
	return mSocket->peerResults().nodeName();
	#endif
}


QString OscarConnection::peerPort() const
{
	#ifdef USE_KEXTSOCK
	return mSocket->port();
	#else
	return mSocket->peerResults().serviceName();
	#endif
}


void OscarConnection::slotRead()
{
	kdDebug(14150) << k_funcinfo << "NOT IMPLEMENTED IN THIS OBJECT!" << endl;
}


void OscarConnection::sendIM(const QString &/*message*/, bool /*isAuto*/)
{
	kdDebug(14150) << k_funcinfo << "NOT IMPLEMENTED IN THIS OBJECT!" << endl;
}


void OscarConnection::sendTypingNotify(TypingNotify /*notifyType*/)
{
	kdDebug(14150) << k_funcinfo << "NOT IMPLEMENTED IN THIS OBJECT!" << endl;
}

void OscarConnection::sendFileSendRequest()
{
	kdDebug(14150) << k_funcinfo << "NOT IMPLEMENTED IN THIS OBJECT!" << endl;
}


// ============================================================================


void OscarConnection::slotSocketConnected()
{
	kdDebug(14150) << k_funcinfo << "Socket is now connected" << endl;
	emit socketConnected(connectionName());
}


void OscarConnection::slotSocketClosed()
{
	kdDebug(14150) << k_funcinfo << "Connection with '" <<
		connectionName() << "' closed" << endl;

	//emit protocolError(i18n("Connection with %1 lost").arg(mSocket->host()), 1);
	//emit connectionClosed(connectionName());
	emit socketClosed(connectionName());
}


void OscarConnection::slotSocketError(int errornum)
{
	kdDebug(14150) << k_funcinfo << "SOCKET ERROR: " << errornum << endl;
	mSocket->closeNow();
	emit socketError(connectionName(), errornum);
}


#include "oscarconnection.moc"
// vim: set noet ts=4 sts=4 sw=4:
