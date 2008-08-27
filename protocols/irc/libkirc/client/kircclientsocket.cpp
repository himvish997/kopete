/*
    kircclient.cpp - IRC Client

    Copyright (c) 2002      by Nick Betcher <nbetcher@kde.org>
    Copyright (c) 2003-2007 by Michel Hermier <michel.hermier@gmail.com>

    Kopete    (c) 2002-2007 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kircclientsocket.moc"
#include "kircclientsocket_p.moc"

#include "kircentity.h"
#include "kircstdmessages.h"

#include <QtNetwork/QSslSocket>

using namespace KIrc;

ClientSocketPrivate::ClientSocketPrivate(ClientSocket *socket)
	: SocketPrivate(socket)
	, failedNickOnLogin(false)
{
	connect(this, SIGNAL(receivedMessage(KIrc::Message &)),
		this, SLOT(onReceivedMessage(KIrc::Message &)));
}

void ClientSocketPrivate::socketStateChanged(QAbstractSocket::SocketState newstate)
{
	Q_Q(Socket);

	switch(newstate)
	{
	case QAbstractSocket::ConnectedState:
		setConnectionState(Socket::Authentifying);
/* 
		// If password is given for this server, send it now, and don't expect a reply
		const KUrl &url = this->url();

		if (url.hasPass())
			StdMessage::pass(this, url.pass());

#ifdef __GNUC__
		#warning make the following string arguments static const
#endif
		StdMessage::user(this, url.user(), StdCommands::Normal, url.queryItem(URL_REALNAME));
		StdMessage::nick(this, url.queryItem(URL_NICKNAME));
*/
		break;
	default:
		SocketPrivate::socketStateChanged(newstate);
		break;
	}
}



ClientSocket::ClientSocket(Context *context)
	: Socket(context, new ClientSocketPrivate(this))
{
//	d->entities << d->server << d->self;

//	d->versionString = QString::fromLatin1("Anonymous client using the KIRC engine.");
//	d->userString = QString::fromLatin1("Response not supplied by user.");
//	d->sourceString = QString::fromLatin1("Unknown client using the KIRC engine.");
}

ClientSocket::~ClientSocket()
{
//	StdCommands::quit(this, QLatin1String("KIRC Deleted"));
}

Entity::Ptr ClientSocket::server()
{
	Q_D(ClientSocket);

	return d->server;
}

QUrl ClientSocket::url() const
{
	Q_D(const ClientSocket);

	return d->url;
}

void ClientSocket::connectToServer(const QUrl &url)
{
	Q_D(Socket);

	QTcpSocket *socket;

	if ( url.scheme() == "irc" )
	{
		socket = new QTcpSocket(this);
//		socket->setSocketFlags( KExtendedSocket::inputBufferedSocket | KExtendedSocket::inetSocket );
		connectToServer(url, socket);
	}
	else if ( url.scheme() == "ircs" )
	{
		socket = new QSslSocket(this);
//		socket->setSocketFlags( KExtendedSocket::inetSocket );
		connectToServer(url, socket);
	}
	else
	{
//		#warning FIXME: send an event here to reflect the error
	}
}

void ClientSocket::connectToServer(const QUrl &url, QAbstractSocket *socket)
{
	Q_D(Socket);

	close();

	if (!socket)
	{
//		#warning FIXME: send an event here to reflect the error
	}

	QString host = url.host();
	if(host.isEmpty())
		host = "localhost";

	int port = url.port();

	if (port == -1)
	{
		// Make the port being guessed by the socket (look into /etc/services)
		//port = url.scheme();
		;
	}

//	the given url is now validated
	d->url = url;
	d->setSocket(socket);

#ifdef __GNUC__
	#warning FIXME: send an event here to reflect connection state
#endif

	socket->connectToHost(host, port);
}
