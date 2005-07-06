/*
    irccontact.cpp - IRC Contact

    Copyright (c) 2002      by Nick Betcher <nbetcher@kde.org>
    Copyright (c) 2004      by Michel Hermier <michel.hermier@wanadoo.fr>

    Kopete    (c) 2002-2004 by the Kopete developers <kopete-devel@kde.org>

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
#include <qregexp.h>

#include <qtimer.h>
#include <qtextcodec.h>

#include "ircaccount.h"
#include "kopeteglobal.h"
#include "kopeteuiglobal.h"
#include "kopetemetacontact.h"
#include "kopeteview.h"
#include "ircusercontact.h"
#include "irccontact.h"
#include "ircprotocol.h"
#include "ircservercontact.h"
#include "irccontactmanager.h"
#include "ksparser.h"

IRCContact::IRCContact(IRCAccount *account, KIRC::EntityPtr entity, Kopete::MetaContact *metac, const QString& icon)
	: Kopete::Contact(account, entity->name(), metac, icon),
	  m_chatSession(0)
{
}

IRCContact::IRCContact(IRCContactManager *contactManager, const QString &nick, Kopete::MetaContact *metac, const QString& icon)
	: Kopete::Contact(contactManager->account(), nick, metac, icon),
	  m_nickName(nick),
	  m_chatSession(0)
{
	KIRC::Engine *engine = kircEngine();

	// Contact list display name
	setProperty( Kopete::Global::Properties::self()->nickName(), m_nickName );

	// IRCContactManager stuff
	QObject::connect(contactManager, SIGNAL(privateMessage(IRCContact *, IRCContact *, const QString &)),
			this, SLOT(privateMessage(IRCContact *, IRCContact *, const QString &)));

	// Kopete::ChatSessionManager stuff
	mMyself.append( static_cast<Kopete::Contact*>( this ) );

	// KIRC stuff
	QObject::connect(engine, SIGNAL(incomingNickChange(const QString &, const QString &)),
			this, SLOT( slotNewNickChange(const QString&, const QString&)));
	QObject::connect(engine, SIGNAL(successfullyChangedNick(const QString &, const QString &)),
			this, SLOT(slotNewNickChange(const QString &, const QString &)));
	QObject::connect(engine, SIGNAL(incomingQuitIRC(const QString &, const QString &)),
			this, SLOT( slotUserDisconnected(const QString&, const QString&)));

	QObject::connect(engine, SIGNAL(statusChanged(KIRC::Engine::Status)),
			this, SLOT(updateStatus()));

	engine->setCodec( m_nickName, codec() );
}

IRCContact::~IRCContact()
{
//	kdDebug(14120) << k_funcinfo << m_nickName << endl;
	if (metaContact() && metaContact()->isTemporary() && !isChatting(m_chatSession))
		metaContact()->deleteLater();

	emit destroyed(this);
}

IRCAccount *IRCContact::ircAccount() const
{
	return static_cast<IRCAccount *>(account());
}

KIRC::Engine *IRCContact::kircEngine() const
{
	return ircAccount()->engine();
}

bool IRCContact::isReachable()
{
	if (onlineStatus().status() != Kopete::OnlineStatus::Offline &&
		onlineStatus().status() != Kopete::OnlineStatus::Unknown)
		return true;

	return false;
}

const QString IRCContact::caption() const
{
	return QString::null;
}
/*
const QString IRCContact::formatedName() const
{
	return QString::null;
}
*/
void IRCContact::updateStatus()
{
}

void IRCContact::privateMessage(IRCContact *, IRCContact *, const QString &)
{
}

void IRCContact::setCodec(const QTextCodec *codec)
{
	kircEngine()->setCodec(m_nickName, codec);
	metaContact()->setPluginData(m_protocol, QString::fromLatin1("Codec"), QString::number(codec->mibEnum()));
}

const QTextCodec *IRCContact::codec()
{
	QString codecId = metaContact()->pluginData(m_protocol, QString::fromLatin1("Codec"));
	QTextCodec *codec = ircAccount()->codec();

	if( !codecId.isEmpty() )
	{
		bool test = true;
		uint mib = codecId.toInt(&test);
		if (test)
			codec = QTextCodec::codecForMib(mib);
		else
			codec = QTextCodec::codecForName(codecId.latin1());
	}

	if( !codec )
		return kircEngine()->codec();

	return codec;
}

Kopete::ChatSession *IRCContact::manager(Kopete::Contact::CanCreateFlags canCreate)
{
	IRCAccount *account = ircAccount();
	KIRC::Engine *engine = kircEngine();

	if (canCreate == Kopete::Contact::CanCreate && !m_chatSession)
	{
		if( engine->status() == KIRC::Engine::Idle && dynamic_cast<IRCServerContact*>(this) == 0 )
			account->connect();

		m_chatSession = Kopete::ChatSessionManager::self()->create(account->myself(), mMyself, account->protocol());
		m_chatSession->setDisplayName(caption());

		QObject::connect(m_chatSession, SIGNAL(messageSent(Kopete::Message&, Kopete::ChatSession *)),
			this, SLOT(slotSendMsg(Kopete::Message&, Kopete::ChatSession *)));
		QObject::connect(m_chatSession, SIGNAL(closing(Kopete::ChatSession *)),
			this, SLOT(chatSessionDestroyed()));

		initConversation();
	}

	return m_chatSession;
}

void IRCContact::chatSessionDestroyed()
{
	m_chatSession = 0;

	if (metaContact()->isTemporary() && !isChatting())
		deleteLater();
}

void IRCContact::slotUserDisconnected(const QString &user, const QString &reason)
{
	if (m_chatSession)
	{
		QString nickname = user.section('!', 0, 0);
		Kopete::Contact *c = locateUser( nickname );
		if ( c )
		{
			m_chatSession->removeContact(c, i18n("Quit: \"%1\" ").arg(reason), Kopete::Message::RichText);
			c->setOnlineStatus(m_protocol->m_UserStatusOffline);
		}
	}
}

void IRCContact::setNickName( const QString &nickname )
{
	kdDebug(14120) << k_funcinfo << m_nickName << " changed to " << nickname << endl;
	m_nickName = nickname;
	Kopete::Contact::setNickName( nickname );
}

void IRCContact::slotNewNickChange(const QString &oldnickname, const QString &newnickname)
{
	IRCAccount *account = ircAccount();

	IRCContact *user = static_cast<IRCContact*>( locateUser(oldnickname) );
	if( user )
	{
		user->setNickName( newnickname );

		//If the user is in our contact list, then change the notify list nickname
		if (!user->metaContact()->isTemporary())
		{
			account->contactManager()->removeFromNotifyList( oldnickname );
			account->contactManager()->addToNotifyList( newnickname );
		}
	}
}

void IRCContact::slotSendMsg(Kopete::Message &message, Kopete::ChatSession *)
{
	QString htmlString = message.escapedBody();

	if (htmlString.find(QString::fromLatin1("</span")) > -1)
	{
		QRegExp findTags( QString::fromLatin1("<span style=\"(.*)\">(.*)</span>") );
		findTags.setMinimal( true );
		int pos = 0;

		while (pos >= 0)
		{
			pos = findTags.search(htmlString);
			if (pos > -1)
			{
				QString styleHTML = findTags.cap(1);
				QString replacement = findTags.cap(2);
				QStringList styleAttrs = QStringList::split(';', styleHTML);

				for (QStringList::Iterator attrPair = styleAttrs.begin(); attrPair != styleAttrs.end(); ++attrPair)
				{
					QString attribute = (*attrPair).section(':',0,0);
					QString value = (*attrPair).section(':',1);

					if( attribute == QString::fromLatin1("color") )
					{
						int ircColor = KSParser::colorForHTML( value );
						if( ircColor > -1 )
							replacement.prepend( QString( QChar(0x03) ).append( QString::number(ircColor) ) ).append( QChar( 0x03 ) );
					}
					else if( attribute == QString::fromLatin1("font-weight") && value == QString::fromLatin1("600") )
						replacement.prepend( QChar(0x02) ).append( QChar(0x02) );
					else if( attribute == QString::fromLatin1("text-decoration")  && value == QString::fromLatin1("underline") )
						replacement.prepend( QChar(31) ).append( QChar(31) );
				}

				htmlString = htmlString.left( pos ) + replacement + htmlString.mid( pos + findTags.matchedLength() );
			}
		}
	}

	htmlString = Kopete::Message::unescape(htmlString);

	if (htmlString.find('\n') > -1)
	{
		QStringList messages = QStringList::split( '\n', htmlString );

		for( QStringList::Iterator it = messages.begin(); it != messages.end(); ++it )
		{
			Kopete::Message msg(message.from(), message.to(), Kopete::Message::escape(sendMessage(*it)), message.direction(),
			                    Kopete::Message::RichText, CHAT_VIEW, message.type());

			msg.setBg(QColor());
			msg.setFg(QColor());

			appendMessage(msg);
			manager(Kopete::Contact::CanCreate)->messageSucceeded();
		}
	}
	else
	{
		message.setBody( Kopete::Message::escape(sendMessage( htmlString )), Kopete::Message::RichText );

		message.setBg( QColor() );
		message.setFg( QColor() );

		appendMessage(message);
		manager(Kopete::Contact::CanCreate)->messageSucceeded();
	}
}

QString IRCContact::sendMessage( const QString &msg )
{
	QString newMessage = msg;
	uint trueLength = msg.length() + m_nickName.length() + 12;
	if( trueLength > 512 )
	{
		//TODO: tell them it is truncated
		kdWarning() << "Message was to long (" << trueLength << "), it has been truncated to 512 characters" << endl;
		newMessage.truncate( 512 - ( m_nickName.length() + 12 ) );
	}

	kircEngine()->privmsg(m_nickName, newMessage );

	return newMessage;
}

Kopete::Contact *IRCContact::locateUser(const QString &nick)
{
	IRCAccount *account = ircAccount();

	if (m_chatSession)
	{
		if( nick == account->mySelf()->nickName() )
			return account->mySelf();
		else
		{
			Kopete::ContactPtrList mMembers = m_chatSession->members();
			for (Kopete::Contact *it = mMembers.first(); it; it = mMembers.next())
			{
				if (static_cast<IRCContact*>(it)->nickName() == nick)
					return it;
			}
		}
	}
	return 0;
}

bool IRCContact::isChatting(Kopete::ChatSession *avoid) const
{
	IRCAccount *account = ircAccount();

	if (!account)
		return false;

	QValueList<Kopete::ChatSession*> sessions = Kopete::ChatSessionManager::self()->sessions();
	for (QValueList<Kopete::ChatSession*>::Iterator it= sessions.begin(); it!=sessions.end() ; ++it)
	{
	  if( (*it) != avoid && (*it)->account() == account &&
			   (*it)->members().contains(this) )
		{
			return true;
		}
	}
	return false;
}

void IRCContact::deleteContact()
{
	kdDebug(14120) << k_funcinfo << m_nickName << endl;

	delete m_chatSession;

	if (!isChatting())
	{
		kdDebug(14120) << k_funcinfo << "will delete " << m_nickName << endl;
		Kopete::Contact::deleteContact();
	}
	else
	{
		metaContact()->removeContact(this);
		Kopete::MetaContact *m = new Kopete::MetaContact();
		m->setTemporary(true);
		setMetaContact(m);
	}
}

void IRCContact::appendMessage(Kopete::Message &msg)
{
	manager(Kopete::Contact::CanCreate)->appendMessage(msg);
}

KopeteView *IRCContact::view()
{
	if (m_chatSession)
		return m_chatSession->view(false);
	return 0L;
}
void IRCContact::serialize(QMap<QString, QString> & /*serializedData*/, QMap<QString, QString> &addressBookData)
{
	// write the
	addressBookData[ protocol()->addressBookIndexField() ] = ( contactId() + QChar(0xE120) + account()->accountId() );
}

void IRCContact::receivedMessage( KIRC::Engine::ServerMessageType type,
				const KIRC::EntityPtr &from,
				const KIRC::EntityPtrList &to,
				const QString &msg)
{
	if (to.contains(m_entity))
	{
		IRCContact *fromContact = ircAccount()->getContact(from);
		Kopete::Message message(fromContact, manager()->members(), msg, Kopete::Message::Inbound,
					Kopete::Message::RichText, CHAT_VIEW);
		appendMessage(message);
	}
}

#include "irccontact.moc"
