/***************************************************************************
                          wpcontact.cpp  -  description
                             -------------------
    begin                : Fri Apr 12 2002
    copyright            : (C) 2002 by Gav Wood
    email                : gav@indigoarchive.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Qt Includes
#include <qdatetime.h>
#include <qfont.h>
#include <qregexp.h>

// KDE Includes
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kpopupmenu.h>

// Kopete Includes
#include "kopetestdaction.h"
#include "kopetemessage.h"
#include "kopetemessagemanager.h"
#include "kopetecontactlist.h"
#include "kopetemetacontact.h"

// Local Includes
#include "wpcontact.h"
#include "wpprotocol.h"
#include "wpdebug.h"

WPContact::WPContact(WPProtocol *protocol, const QString &host, KopeteMetaContact *parent) : KopeteContact(protocol, host, parent)
{
	DEBUG(WPDMETHOD, "WPContact::WPContact(" << host << ", <protocol>, <parent>)");

	QString newDisplayName;
	for(unsigned i = 0; i < host.length(); i++)
		if(!i)
			newDisplayName += host[i].upper();
		else
			newDisplayName += host[i].lower();

	setDisplayName(newDisplayName);
	myProtocol = protocol;
	myHost = host;

	// Initialise and start the periodical checking for contact's status
	myIsOnline = true;
	emit statusChanged(this, status());
	slotCheckStatus();
	connect(&checkStatus, SIGNAL(timeout()), this, SLOT(slotCheckStatus()));
	checkStatus.start(1000, false);

	m_manager = 0L;

	// Set up the context menu
	myActionCollection = new KActionCollection(this);
}

KopeteMessageManager* WPContact::manager()
{
	if( !m_manager )
	{
		// Set up the message managers
		QPtrList<KopeteContact> singleContact;
		singleContact.append(this);

		KGlobal::config()->setGroup("WinPopup");
		if(KGlobal::config()->readBoolEntry("EmailDefault", true))
			m_manager = KopeteMessageManagerFactory::factory()->create(myProtocol->myself(), singleContact, myProtocol, KopeteMessageManager::Email);
		else
			m_manager = KopeteMessageManagerFactory::factory()->create(myProtocol->myself(), singleContact, myProtocol, KopeteMessageManager::ChatWindow);

		connect(m_manager, SIGNAL(messageSent(const KopeteMessage &, KopeteMessageManager *)), this, SLOT(slotSendMessage(const KopeteMessage &)));
		connect(m_manager, SIGNAL(messageSent(const KopeteMessage &, KopeteMessageManager *)), m_manager, SLOT(appendMessage(const KopeteMessage &)));
		connect(m_manager, SIGNAL(destroyed()), this, SLOT(slotMessageManagerDestroyed()));
	}

	return m_manager;
}

void WPContact::slotMessageManagerDestroyed()
{
	m_manager = 0L;
}

void WPContact::slotCheckStatus()
{
//	DEBUG(WPDMETHOD, "WPContact::slotCheckStatus()");

	bool oldIsOnline = myIsOnline;
	if(myProtocol)
		myIsOnline = myProtocol->checkHost(myHost);
	else
		myIsOnline = false;
	if(oldIsOnline != myIsOnline)
		emit statusChanged(this, status());
}

void WPContact::execute()
{
	DEBUG(WPDMETHOD, "WPContact::execute()");

	manager()->readMessages();
	manager()->slotSendEnabled(true);
}

void WPContact::slotNewMessage(const QString &Body, const QDateTime &Arrival)
{
	DEBUG(WPDMETHOD, "WPContact::slotNewMessage(" << Body << ", " << Arrival.toString() << ")");

	QPtrList<KopeteContact> contactList;
	contactList.append(myProtocol->myself());

	QRegExp subj("^Subject: ([^\n]*)\n(.*)$");
	if(subj.search(Body) == -1)
		manager()->appendMessage(KopeteMessage(this, contactList, Body, KopeteMessage::Inbound));
	else
	{
		manager()->appendMessage(KopeteMessage(this, contactList, subj.cap(2), subj.cap(1), KopeteMessage::Inbound));
		manager()->slotSendEnabled(false);
	}
}

void WPContact::slotSendMessage(const KopeteMessage& message)
{
	DEBUG(WPDMETHOD, "WPContact::slotSendMessage(<message>)");
	
	QString Message = (message.subject() != "" ? "Subject: " + message.subject() + "\n" : QString("")) + message.plainBody();
	myProtocol->slotSendMessage(Message, dynamic_cast<WPContact *>(message.to().first())->host());
}

#include "wpcontact.moc"

