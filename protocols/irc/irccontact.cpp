/*
    irccontact.cpp - IRC Contact

    Copyright (c) 2002      by Nick Betcher <nbetcher@kde.org>

    Kopete    (c) 2002      by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <kdebug.h>
#include <klocale.h>
#include <qstringlist.h>
#include <qregexp.h>

#include "ircchannelcontact.h"
#include "ircidentity.h"

#include "kirc.h"
#include "ksparser.h"
#include "kopetemessagemanager.h"
#include "kopetemessagemanagerfactory.h"
#include "kopetemetacontact.h"
#include "kopeteviewmanager.h"
#include "ircusercontact.h"
#include "irccontact.h"

struct whoIsInfo
{
	QString userName;
	QString hostName;
	QString realName;
	QString serverName;
	QString serverInfo;
	QStringList channels;
	unsigned long idle;
	bool isOperator;
};

IRCContact::IRCContact(IRCIdentity *identity, const QString &nick, KopeteMetaContact *metac) :
	KopeteContact((KopeteProtocol *)identity->protocol(), nick, metac )
{
	mIdentity = identity;
	mEngine = mIdentity->engine();
	mMetaContact = metac;
	mMsgManager = 0L;
	mNickName = nick;
	mParser = new KSParser();

	// Contact list display name
	setDisplayName(mNickName);

	// KopeteMessageManagerFactory stuff
	mContact.append((KopeteContact *)this);
	mMyself.append((KopeteContact *)identity->mySelf());

	QObject::connect(mEngine, SIGNAL(incomingAction(const QString &, const QString &, const QString &)), this, SLOT(slotNewAction(const QString &, const QString &, const QString &)));
	QObject::connect(mEngine, SIGNAL(incomingMessage(const QString &, const QString &, const QString &)), this, SLOT(slotNewMessage(const QString &, const QString &, const QString &)));
	QObject::connect(mEngine, SIGNAL(incomingWhoIsUser(const QString &, const QString &, const QString &, const QString &)) ,
		this, SLOT( slotNewWhoIsUser(const QString &, const QString &, const QString &, const QString &) ) );

	QObject::connect(mEngine, SIGNAL(incomingWhoIsServer(const QString &, const QString &, const QString &)), this, SLOT(slotNewWhoIsServer(const QString &, const QString &, const QString &)));
	QObject::connect(mEngine, SIGNAL(incomingWhoIsOperator(const QString &)), this, SLOT(slotNewWhoIsOperator(const QString &)));
	QObject::connect(mEngine, SIGNAL(incomingWhoIsIdle(const QString &, unsigned long )), this, SLOT(slotNewWhoIsIdle(const QString &, unsigned long )));
	QObject::connect(mEngine, SIGNAL(incomingWhoIsChannels(const QString &, const QString &)), this, SLOT(slotNewWhoIsChannels(const QString &, const QString &)));
	QObject::connect(mEngine, SIGNAL(incomingEndOfWhois(const QString &)), this, SLOT( slotWhoIsComplete(const QString &)));
	QObject::connect(mEngine, SIGNAL(incomingNickChange(const QString &, const QString &)), this, SLOT( slotNewNickChange(const QString&, const QString&)));
	QObject::connect(mEngine, SIGNAL(incomingQuitIRC(const QString &, const QString &)), this, SLOT( slotUserDisconnected(const QString&, const QString&)));
	QObject::connect(mEngine, SIGNAL(incomingCtcpReply(const QString &, const QString &, const QString &)), this, SLOT( slotNewCtcpReply(const QString&, const QString &, const QString &)));
	QObject::connect(mEngine, SIGNAL(connectionClosed()), this, SLOT(slotConnectionClosed()));
	isConnected = false;
}

IRCContact::~IRCContact()
{
	delete mParser;
}

void IRCContact::slotConnectionClosed()
{
	setOnlineStatus( KopeteContact::Offline );
}

bool IRCContact::processMessage( const KopeteMessage &msg )
{
	QStringList commandLine = QStringList::split( QRegExp( QString::fromLatin1("\\s+") ), msg.plainBody() );
	QString commandArgs = msg.plainBody().section( QRegExp( QString::fromLatin1("\\s+") ), 1 );
	uint commandCount = commandLine.count();

	if( commandLine.first().startsWith( QString::fromLatin1("/") ) )
	{
		QString command = commandLine.first().right( commandLine.first().length() - 1 ).lower();

		if( mEngine->isLoggedIn() )
		{
			// These commands only work when we are connected
			if( command == QString::fromLatin1("nick") && commandCount > 1 )
				mIdentity->successfullyChangedNick( QString::null, *commandLine.at(1) );

			else if( command == QString::fromLatin1("me") && commandCount > 1 )
				mEngine->actionContact( displayName(), commandArgs );

			else if( command == QString::fromLatin1("topic") && inherits("IRCChannelContact") )
			{
				IRCChannelContact *chan = static_cast<IRCChannelContact*>( this );
				if( commandCount > 1 )
					chan->setTopic( *commandLine.at(1) );
				else
				{
					KopeteMessage msg((KopeteContact*)this, mContact, i18n("Topic for %1 is %2").arg(mNickName).arg(chan->topic()), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
					manager()->appendMessage(msg);
				}
			}
			else if( command == QString::fromLatin1("mode") && commandCount > 2 )
			{
				mEngine->changeMode( *commandLine.at(1), commandArgs.section(' ', 1) );
			}
			else if( command == QString::fromLatin1("whois") && commandCount > 1 )
			{
				mEngine->whoisUser( *commandLine.at(1) );
			}
			else if( command == QString::fromLatin1("query") && commandCount > 1 )
			{
				if( !(*commandLine.at(1)).startsWith( QString::fromLatin1("#") ) )
					mIdentity->findUser( *commandLine.at(1) )->startChat();
				else
				{
					KopeteMessage msg((KopeteContact*)this, mContact, i18n("\"%1\" is an invaid nickname. Nicknames must not start with '#'.").arg(*commandLine.at(1)), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
					manager()->appendMessage(msg);
				}
			}
			else if( command == QString::fromLatin1("join") && commandCount > 1 )
			{
				if( (*commandLine.at(1)).startsWith( QString::fromLatin1("#") ) )
					mIdentity->findChannel( *commandLine.at(1) )->startChat();
				else
				{
					KopeteMessage msg((KopeteContact*)this, mContact, i18n("\"%1\" is an invaid channel. Channels must start with '#'.").arg(*commandLine.at(1)), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
					manager()->appendMessage(msg);
				}
			}
			else if( command == QString::fromLatin1("part") )
			{
				KopeteViewManager::viewManager()->view( manager(), true )->closeView();
			}
			else
			{
				KopeteMessage msg((KopeteContact*)this, mContact, i18n("\"%1\" is an unrecognized command.").arg(command), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
				manager()->appendMessage(msg);
			}
		}

		// A /command returns false to stop further processing
		return false;
	}

	return true;
}

void IRCContact::slotUserDisconnected( const QString &user, const QString &reason)
{
	QString nickname = user.section('!', 0, 0);
	KopeteContact *c = locateUser( nickname );
	if ( c )
	{
		KopeteMessage msg(c, mContact, i18n("User %1 has quit (\"%2\")").arg(nickname).arg(reason), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
		manager()->appendMessage(msg);
		manager()->removeContact( c, true );
		c->setOnlineStatus( KopeteContact::Offline );
		mIdentity->unregisterUser( nickname );
	}
}

void IRCContact::slotNewMessage(const QString &originating, const QString &target, const QString &message)
{
	//kdDebug(14120) << k_funcinfo << "originating is " << originating << " target is " << target << endl;
	if ( isConnected && target.lower() == mNickName.lower() )
	{
		QString nickname = originating.section('!', 0, 0);
		KopeteContact *user = locateUser( nickname );
		if ( user )
		{
			KopeteMessage msg( user, mContact, message, KopeteMessage::Inbound, KopeteMessage::PlainText, KopeteMessage::Chat );
			msg.setBody( mParser->parse( msg.escapedBody() ), KopeteMessage::RichText );
			manager()->appendMessage(msg);
		}
	}
}

void IRCContact::slotNewAction(const QString &originating, const QString &target, const QString &message)
{
	//kdDebug(14120) << k_funcinfo << "originating is " << originating << " target is " << target << endl;
	if ( isConnected && target.lower() == mNickName.lower())
	{
		QString nickname = originating.section('!', 0, 0);
		KopeteContact *user = locateUser( nickname );
		if ( user )
		{
			KopeteMessage msg( user, mContact, message, KopeteMessage::Action, KopeteMessage::PlainText, KopeteMessage::Chat );
			manager()->appendMessage(msg);
		}
		else if( mIdentity->mySelf()->nickName().lower() == originating.lower() )
		{
			KopeteMessage msg( (KopeteContact*)mIdentity->mySelf(), mContact, message, KopeteMessage::Action, KopeteMessage::PlainText, KopeteMessage::Chat );
			manager()->appendMessage(msg);
		}

	}
}

void IRCContact::slotNewWhoIsUser(const QString &nickname, const QString &username, const QString &hostname, const QString &realname)
{
	KopeteContact *user = locateUser( nickname );
	if( user )
	{
		kdDebug(14120) << k_funcinfo << endl;
		mWhoisMap[nickname] = new whoIsInfo;
		mWhoisMap[nickname]->isOperator = false;
		mWhoisMap[nickname]->userName = username;
		mWhoisMap[nickname]->hostName = hostname;
		mWhoisMap[nickname]->realName = realname;
	}
}

void IRCContact::slotNewWhoIsServer(const QString &nickname, const QString &servername, const QString &serverinfo)
{
	if( mWhoisMap.contains(nickname) )
	{
		mWhoisMap[nickname]->serverName = servername;
		mWhoisMap[nickname]->serverInfo = serverinfo;
	}
}

void IRCContact::slotNewWhoIsIdle(const QString &nickname, unsigned long idle)
{
	if( mWhoisMap.contains(nickname) )
		mWhoisMap[nickname]->idle = idle;
}

void IRCContact::slotNewWhoIsOperator(const QString &nickname)
{
	if( mWhoisMap.contains(nickname) )
		mWhoisMap[nickname]->isOperator = true;
}

void IRCContact::slotNewWhoIsChannels(const QString &nickname, const QString &channel)
{
	if( mWhoisMap.contains(nickname) )
		mWhoisMap[nickname]->channels.append( channel );
}

void IRCContact::slotWhoIsComplete(const QString &nickname)
{
	if( mWhoisMap.contains(nickname) )
	{
		whoIsInfo *w = mWhoisMap[nickname];
		KopeteMessage msg;
		KopeteContact *c = locateUser( nickname );

		//User info
		msg = KopeteMessage( c, mContact, QString::fromLatin1("[%1] (%2@%3) : %4\n").arg(nickname).arg(w->userName).arg(w->hostName).arg(w->realName), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat );
		manager()->appendMessage(msg);

		if( w->isOperator )
		{
			msg = KopeteMessage( c, mContact, i18n("[%1] is an IRC operator").arg(nickname), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat );
			manager()->appendMessage(msg);
		}

		//Channels
		QString channelText;
		for(QStringList::Iterator it = w->channels.begin(); it != w->channels.end(); ++it)
			channelText += *it + QString::fromLatin1(" \n");
		msg = KopeteMessage( c, mContact, QString::fromLatin1("[%1] %2").arg(nickname).arg(channelText), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat );
		manager()->appendMessage(msg);

		//Server
		msg = KopeteMessage( c, mContact, QString::fromLatin1("[%1] %2 : %3\n").arg(nickname).arg(w->serverName).arg(w->serverInfo), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat );
		manager()->appendMessage(msg);

		//Idle
		msg = KopeteMessage( c, mContact, i18n("[%1] idle %2\n").arg(nickname).arg( QString::number(w->idle) ), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat );
		manager()->appendMessage(msg);

		//End
		msg = KopeteMessage( c, mContact,  i18n("[%1] End of WHOIS list.").arg(nickname), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat );
		manager()->appendMessage(msg);

		delete w;
	}
}

void IRCContact::slotNewNickChange( const QString &oldnickname, const QString &newnickname)
{
	IRCContact *user = static_cast<IRCContact*>( locateUser( oldnickname) );
	if( user )
	{
		user->setNickName( newnickname );
		//If we are tracking name changes...
		user->setDisplayName( newnickname );

		KopeteMessage msg((KopeteContact *)this, mContact, i18n("%1 is now known as %2").arg(oldnickname).arg(newnickname), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
		manager()->appendMessage(msg);
	}
}

void IRCContact::slotNewCtcpReply(const QString &type, const QString &target, const QString &messageReceived)
{
	//kdDebug(14120) << k_funcinfo << target << endl;
	if( target == mNickName )
	{
		KopeteView *activeView = KopeteViewManager::viewManager()->activeView();
		if( activeView )
		{
			KopeteMessage msg((KopeteContact *)this, mContact, i18n("CTCP %1 REPLY: %2").arg(type).arg(messageReceived), KopeteMessage::Internal, KopeteMessage::PlainText, KopeteMessage::Chat);
			activeView->msgManager()->appendMessage(msg);
		}
	}
}

void IRCContact::slotSendMsg(KopeteMessage &message, KopeteMessageManager *)
{
	if( !isConnected )
	{
		messageQueue.append( message );
		mEngine->joinChannel(mNickName);
	}
	else
	{
		if( processMessage( message ) )
		{
			// If the above was false, there was a server command
			mEngine->messageContact(mNickName, message.plainBody());
			message.setBg( QColor() );
			message.setFg( QColor() );
			manager()->appendMessage(message);
		}

		manager()->messageSucceeded();
	}
}

KopeteContact *IRCContact::locateUser( const QString &nick )
{
	//kdDebug(14120) << k_funcinfo << "Find nick " << nick << endl;
	if( isConnected )
	{
		KopeteContactPtrList mMembers = manager()->members();
		for( KopeteContact *it = mMembers.first(); it; it = mMembers.next() )
		{
			if( static_cast<IRCContact*>(it)->nickName() == nick )
				return it;
		}
	}

	return 0L;
}

const QString IRCContact::caption() const
{
	return mNickName;
}

#include "irccontact.moc"
