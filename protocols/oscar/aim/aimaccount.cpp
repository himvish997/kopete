/*
   aimaccount.cpp  -  Oscar Protocol Plugin, AIM part

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

#include <qdom.h>

#include <kdebug.h>
#include <kconfig.h>
#include <kdialog.h>
#include <klocale.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <ktoggleaction.h>
#include <kicon.h>

#include "kopeteawayaction.h"
#include "kopetepassword.h"
#include "kopetestdaction.h"
#include "kopeteuiglobal.h"
#include "kopetecontactlist.h"
#include "kopetemetacontact.h"
#include "kopeteprotocol.h"
#include "kopetechatsessionmanager.h"
#include "kopeteview.h"
#include <kopeteuiglobal.h>

#include "aimprotocol.h"
#include "aimaccount.h"
#include "aimchatsession.h"
#include "aimcontact.h"
#include "icqcontact.h"
#include "aimuserinfo.h"
#include "aimjoinchat.h"
#include "oscarmyselfcontact.h"

#include "oscarutils.h"
#include "client.h"
#include "contactmanager.h"
#include "oscarsettings.h"


const Oscar::DWORD AIM_ONLINE = 0x0;
const Oscar::DWORD AIM_AWAY = 0x1;

namespace Kopete { class MetaContact; }

AIMMyselfContact::AIMMyselfContact( AIMAccount *acct )
: OscarMyselfContact( acct )
{
	m_acct = acct;
}

void AIMMyselfContact::userInfoUpdated()
{
	Oscar::DWORD extendedStatus = details().extendedStatus();
	kDebug( OSCAR_AIM_DEBUG ) << k_funcinfo << "extendedStatus is " << QString::number( extendedStatus, 16 ) << endl;
	AIM::Presence presence = AIM::Presence::fromOscarStatus( extendedStatus, details().userClass() );
	setOnlineStatus( presence.toOnlineStatus() );
	setProperty( Kopete::Global::Properties::self()->statusMessage(), static_cast<AIMAccount*>( account() )->engine()->statusMessage() );
}

void AIMMyselfContact::setOwnProfile( const QString& newProfile )
{
	m_profileString = newProfile;
	if ( m_acct->isConnected() )
		m_acct->engine()->updateProfile( newProfile );
}

QString AIMMyselfContact::userProfile()
{
	return m_profileString;
}

Kopete::ChatSession* AIMMyselfContact::manager( Kopete::Contact::CanCreateFlags canCreate,
		Oscar::WORD exchange, const QString& room )
{
	kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << endl;
	Kopete::ContactPtrList chatMembers;
	chatMembers.append( this );
	Kopete::ChatSession* genericManager = 0L;
	genericManager = Kopete::ChatSessionManager::self()->findChatSession( account()->myself(), chatMembers, protocol() );
	AIMChatSession* session = dynamic_cast<AIMChatSession*>( genericManager );

	if ( !session && canCreate == Contact::CanCreate )
	{
		session = new AIMChatSession( this, chatMembers, account()->protocol(), exchange, room );
		session->setEngine( m_acct->engine() );

		connect( session, SIGNAL( messageSent( Kopete::Message&, Kopete::ChatSession* ) ),
				this, SLOT( sendMessage( Kopete::Message&, Kopete::ChatSession* ) ) );
		m_chatRoomSessions.append( session );
	}
	return session;
}

void AIMMyselfContact::chatSessionDestroyed( Kopete::ChatSession* session )
{
	m_chatRoomSessions.removeAll( session );
}

void AIMMyselfContact::sendMessage( Kopete::Message& message, Kopete::ChatSession* session )
{
	kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "sending a message" << endl;
	//TODO: remove duplication - factor into a message utils class or something
	Oscar::Message msg;
	QString s;

	if (message.plainBody().isEmpty()) // no text, do nothing
		return;
	//okay, now we need to change the message.escapedBody from real HTML to aimhtml.
	//looking right now for docs on that "format".
	//looks like everything except for alignment codes comes in the format of spans

	//font-style:italic -> <i>
	//font-weight:600 -> <b> (anything > 400 should be <b>, 400 is not bold)
	//text-decoration:underline -> <u>
	//font-family: -> <font face="">
	//font-size:xxpt -> <font ptsize=xx>

	s=message.escapedBody();
	s.replace ( QRegExp( QString::fromLatin1("<span style=\"([^\"]*)\">([^<]*)</span>")),
			QString::fromLatin1("<style>\\1;\"\\2</style>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)font-style:italic;([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("<i><style>\\1\\2\"\\3</style></i>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)font-weight:600;([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("<b><style>\\1\\2\"\\3</style></b>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)text-decoration:underline;([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("<u><style>\\1\\2\"\\3</style></u>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)font-family:([^;]*);([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("<font face=\"\\2\"><style>\\1\\3\"\\4</style></font>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)font-size:([^p]*)pt;([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("<font ptsize=\"\\2\"><style>\\1\\3\"\\4</style></font>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)color:([^;]*);([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("<font color=\"\\2\"><style>\\1\\3\"\\4</style></font>"));

	s.replace ( QRegExp( QString::fromLatin1("<style>([^\"]*)\"([^<]*)</style>")),
			QString::fromLatin1("\\2"));

	//okay now change the <font ptsize="xx"> to <font size="xx">

	//0-9 are size 1
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"\\d\">")),
			QString::fromLatin1("<font size=\"1\">"));
	//10-11 are size 2
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"1[01]\">")),
			QString::fromLatin1("<font size=\"2\">"));
	//12-13 are size 3
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"1[23]\">")),
			QString::fromLatin1("<font size=\"3\">"));
	//14-16 are size 4
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"1[456]\">")),
			QString::fromLatin1("<font size=\"4\">"));
	//17-22 are size 5
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"(?:1[789]|2[012])\">")),
			QString::fromLatin1("<font size=\"5\">"));
	//23-29 are size 6
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"2[3456789]\">")),QString::fromLatin1("<font size=\"6\">"));
	//30- (and any I missed) are size 7
	s.replace ( QRegExp ( QString::fromLatin1("<font ptsize=\"[^\"]*\">")),QString::fromLatin1("<font size=\"7\">"));

	s.replace ( QRegExp ( QString::fromLatin1("<br[ /]*>")), QString::fromLatin1("<br>") );

	kDebug(14190) << k_funcinfo << "sending "
		<< s << endl;

	msg.setSender( contactId() );
	msg.setText( Oscar::Message::UserDefined, s, m_acct->defaultCodec() );
	msg.setTimestamp(message.timestamp());
	msg.setChannel(0x03);
	msg.addProperty( Oscar::Message::ChatRoom );

	AIMChatSession* aimSession = dynamic_cast<AIMChatSession*>( session );
	if ( !aimSession )
	{
		kWarning(OSCAR_AIM_DEBUG) << "couldn't convert to AIM chat room session!" << endl;
		session->messageSucceeded();
		return;
	}
	msg.setExchange( aimSession->exchange() );
	msg.setChatRoom( aimSession->roomName() );

	m_acct->engine()->sendMessage( msg );
	//session->appendMessage( message );
	session->messageSucceeded();
}


AIMAccount::AIMAccount(Kopete::Protocol *parent, QString accountID)
	: OscarAccount(parent, accountID, false)
{
	kDebug(14152) << k_funcinfo << accountID << ": Called."<< endl;
	AIMMyselfContact* mc = new AIMMyselfContact( this );
	setMyself( mc );
	mc->setOnlineStatus( AIM::Presence( AIM::Presence::Offline ).toOnlineStatus() );
	QString profile = configGroup()->readEntry( "Profile",
			i18n( "Visit the Kopete website at <a href=\"http://kopete.kde.org\">http://kopete.kde.org</a>") );
	mc->setOwnProfile( profile );
	mInitialStatusMessage.clear();

	m_joinChatDialog = 0;
	QObject::connect( engine(), SIGNAL( chatRoomConnected( WORD, const QString& ) ),
			this, SLOT( connectedToChatRoom( WORD, const QString& ) ) );

	QObject::connect( engine(), SIGNAL( userJoinedChat( Oscar::WORD, const QString&, const QString& ) ),
			this, SLOT( userJoinedChat( Oscar::WORD, const QString&, const QString& ) ) );

	QObject::connect( engine(), SIGNAL( userLeftChat( Oscar::WORD, const QString&, const QString& ) ),
			this, SLOT( userLeftChat( Oscar::WORD, const QString&, const QString& ) ) );

}

AIMAccount::~AIMAccount()
{
}

AIMProtocol* AIMAccount::protocol() const
{
	return static_cast<AIMProtocol*>(OscarAccount::protocol());
}

AIM::Presence AIMAccount::presence()
{
	return AIM::Presence::fromOnlineStatus( myself()->onlineStatus() );
}

OscarContact *AIMAccount::createNewContact( const QString &contactId, Kopete::MetaContact *parentContact, const OContact& ssiItem )
{
	if ( QRegExp("[\\d]+").exactMatch( contactId ) )
	{
		ICQContact* contact = new ICQContact( this, contactId, parentContact, QString::null, ssiItem );

		if ( !ssiItem.alias().isEmpty() )
			contact->setProperty( Kopete::Global::Properties::self()->nickName(), ssiItem.alias() );

		if ( isConnected() )
			contact->loggedIn();

		return contact;
	}
	else
	{
		AIMContact* contact = new AIMContact( this, contactId, parentContact, QString::null, ssiItem );

		if ( !ssiItem.alias().isEmpty() )
			contact->setProperty( Kopete::Global::Properties::self()->nickName(), ssiItem.alias() );

		return contact;
	}
}

KActionMenu* AIMAccount::actionMenu()
{
	KActionMenu *mActionMenu = Kopete::Account::actionMenu();

	mActionMenu->addSeparator();

	KAction* m_joinChatAction = new KAction( i18n( "Join Chat..." ), this );
        //, "join_a_chat" );
	QObject::connect( m_joinChatAction, SIGNAL(triggered(bool)), this, SLOT(slotJoinChat()) );
	mActionMenu->addAction( m_joinChatAction );

	KAction* m_editInfoAction = new KAction( KIcon("identity"), i18n( "Edit User Info..." ), this );
        //, "actionEditInfo" );
	QObject::connect( m_editInfoAction, SIGNAL(triggered(bool)), this, SLOT(slotEditInfo()) );
	mActionMenu->addAction( m_editInfoAction );

	KToggleAction* actionInvisible = new KToggleAction( i18n( "In&visible" ), this );
        //, "actionInvisible" );
	AIM::Presence pres( presence().type(), presence().flags() | AIM::Presence::Invisible );
	actionInvisible->setIcon( KIcon( pres.toOnlineStatus().iconFor( this ) ) );
	actionInvisible->setChecked( (presence().flags() & AIM::Presence::Invisible) == AIM::Presence::Invisible );
	QObject::connect( actionInvisible, SIGNAL(triggered(bool)), this, SLOT(slotToggleInvisible()) );
	mActionMenu->addAction( actionInvisible );

	return mActionMenu;
}

void AIMAccount::setPresenceFlags( AIM::Presence::Flags flags, const QString &message )
{
	AIM::Presence pres = presence();
	kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "new flags=" << (int)flags << ", old type="
	                        << (int)pres.flags() << ", new message=" << message << endl;
	setPresenceTarget( AIM::Presence( pres.type(), flags ), message );
}

void AIMAccount::setPresenceType( AIM::Presence::Type type, const QString &message )
{
	AIM::Presence pres = presence();
	kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "new type=" << (int)type << ", old type="
	                        << (int)pres.type() << ", new message=" << message << endl;
	setPresenceTarget( AIM::Presence( type, pres.flags() ), message );
}

void AIMAccount::setPresenceTarget( const AIM::Presence &newPres, const QString &message )
{
	bool targetIsOffline = (newPres.type() == AIM::Presence::Offline);
	bool accountIsOffline = ( presence().type() == AIM::Presence::Offline ||
	                          myself()->onlineStatus() == protocol()->statusManager()->connectingStatus() );

	if ( targetIsOffline )
	{
		OscarAccount::disconnect();
		// allow toggling invisibility when offline
		myself()->setOnlineStatus( newPres.toOnlineStatus() );
	}
	else if ( accountIsOffline )
	{
		mInitialStatusMessage = message;
		OscarAccount::connect( newPres.toOnlineStatus() );
	}
	else
	{
		engine()->setStatus( newPres.toOscarStatus(), message );
	}
}

void AIMAccount::setOnlineStatus( const Kopete::OnlineStatus& status, const Kopete::StatusMessage &reason )
{
	if ( status.status() == Kopete::OnlineStatus::Invisible )
	{
		// called from outside, i.e. not by our custom action menu entry...

		if ( presence().type() == AIM::Presence::Offline )
		{
			// ...when we are offline go online invisible.
			setPresenceTarget( AIM::Presence( AIM::Presence::Online, AIM::Presence::Invisible ) );
		}
		else
		{
			// ...when we are not offline set invisible.
			setPresenceFlags( AIM::Presence::Invisible );
		}
	}
	else
	{
		setPresenceType( AIM::Presence::fromOnlineStatus( status ).type(), reason.message() );
	}
}

void AIMAccount::setStatusMessage( const Kopete::StatusMessage& statusMessage )
{
}

void AIMAccount::setUserProfile(const QString &profile)
{
	kDebug(14152) << k_funcinfo << "called." << endl;
	AIMMyselfContact* aimmc = dynamic_cast<AIMMyselfContact*>( myself() );
	if ( aimmc )
		aimmc->setOwnProfile( profile );
	configGroup()->writeEntry( QString::fromLatin1( "Profile" ), profile );
}

void AIMAccount::slotEditInfo()
{
	if ( !isConnected() )
	{
		KMessageBox::sorry( Kopete::UI::Global::mainWidget(),
				i18n( "Editing your user info is not possible because "
					"you are not connected." ),
				i18n( "Unable to edit user info" ) );
		return;
	}
	AIMUserInfoDialog *myInfo = new AIMUserInfoDialog(static_cast<AIMContact *>( myself() ), this);
	myInfo->exec(); // This is a modal dialog
}

void AIMAccount::slotToggleInvisible()
{
	using namespace AIM;
	if ( (presence().flags() & Presence::Invisible) == Presence::Invisible )
		setPresenceFlags( presence().flags() & ~Presence::Invisible );
	else
		setPresenceFlags( presence().flags() | Presence::Invisible );
}

void AIMAccount::slotJoinChat()
{
	if ( !isConnected() )
	{
		KMessageBox::sorry( Kopete::UI::Global::mainWidget(),
				i18n( "Joining an AIM chat room is not possible because "
					"you are not connected." ),
				i18n( "Unable to Join AIM Chat Room" ) );
		return;
	}

	//get the exchange info
	//create the dialog
	//join the chat room
	if ( !m_joinChatDialog )
	{
		m_joinChatDialog = new AIMJoinChatUI( this, Kopete::UI::Global::mainWidget() );
		QObject::connect( m_joinChatDialog, SIGNAL( closing( int ) ),
				this, SLOT( joinChatDialogClosed( int ) ) );
		QList<int> list = engine()->chatExchangeList();
		m_joinChatDialog->setExchangeList( list );
		m_joinChatDialog->show();
	}
	else
		m_joinChatDialog->raise();
}

void AIMAccount::joinChatDialogClosed( int code )
{
	if ( code == QDialog::Accepted )
	{
		//join the chat
		kDebug(14152) << k_funcinfo << "chat accepted." << endl;
		engine()->joinChatRoom( m_joinChatDialog->roomName(),
				m_joinChatDialog->exchange().toInt() );
	}

	m_joinChatDialog->delayedDestruct();
	m_joinChatDialog = 0L;
}

void AIMAccount::loginActions()
{
	OscarAccount::loginActions();

	using namespace AIM::PrivacySettings;
	int privacySetting = this->configGroup()->readEntry( "PrivacySetting", int(AllowAll) );
	this->setPrivacySettings( privacySetting );
}

void AIMAccount::disconnected( DisconnectReason reason )
{
	kDebug( OSCAR_AIM_DEBUG ) << k_funcinfo << "Attempting to set status offline" << endl;
	myself()->setOnlineStatus( AIM::Presence( AIM::Presence::Offline, presence().flags() ).toOnlineStatus() );

	QHash<QString, Kopete::Contact*> contactList = contacts();
	foreach( Kopete::Contact* c, contactList.values() )
	{
		OscarContact* oc = dynamic_cast<OscarContact*>( c );
		if ( oc )
			oc->userOffline( oc->contactId() );
	}

	OscarAccount::disconnected( reason );
}

void AIMAccount::messageReceived( const Oscar::Message& message )
{
	kDebug(14152) << k_funcinfo << " Got a message, calling OscarAccount::messageReceived" << endl;
	// Want to call the parent to do everything else
	if ( message.channel() != 0x0003 )
	{
		OscarAccount::messageReceived(message);

		// Check to see if our status is away, and send an away message
		// Might be duplicate code from the parent class to get some needed information
		// Perhaps a refactoring is needed.
		kDebug(14152) << k_funcinfo << "Checking to see if I'm online.." << endl;
		if( myself()->onlineStatus().status() == Kopete::OnlineStatus::Away )
		{
			QString sender = Oscar::normalize( message.sender() );
			AIMContact* aimSender = dynamic_cast<AIMContact *> ( contacts()[sender] ); //should exist now
			if ( !aimSender )
			{
				kWarning(OSCAR_RAW_DEBUG) << "For some reason, could not get the contact "
					<< "That this message is from: " << message.sender() << ", Discarding message" << endl;
				return;
			}
			// Create, or get, a chat session with the contact
			Kopete::ChatSession* chatSession = aimSender->manager( Kopete::Contact::CanCreate );

			// get the away message we have set
			QString msg = engine()->statusMessage();
			kDebug(14152) << k_funcinfo << "Got away message: " << msg << endl;
			// Create the message
			Kopete::Message chatMessage( myself(), aimSender, msg, Kopete::Message::Outbound,
					Kopete::Message::RichText );
			kDebug(14152) << k_funcinfo << "Sending autoresponse" << endl;
			// Send the message
			aimSender->sendAutoResponse( chatMessage );
		}
	}
	else
	{
		kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "have chat message" << endl;
		//handle chat room messages separately
		QList<Kopete::ChatSession*> chats = Kopete::ChatSessionManager::self()->sessions();
		QList<Kopete::ChatSession*>::iterator it,  itEnd = chats.end();
		for ( it = chats.begin(); it != itEnd; ++it )
		{
			Kopete::ChatSession* kcs = ( *it );
			AIMChatSession* session = dynamic_cast<AIMChatSession*>( kcs );
			if ( !session )
				continue;

			if ( session->exchange() == message.exchange() &&
					Oscar::normalize( session->roomName() ) ==
					Oscar::normalize( message.chatRoom() ) )
			{
				kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "found chat session for chat room" << endl;
				OscarContact* ocSender = static_cast<OscarContact*>(contacts()[Oscar::normalize( message.sender() )]);
				//sanitize;
				QString sanitizedMsg = ocSender->sanitizedMessage( message.text( defaultCodec() ) );

				Kopete::ContactPtrList me;
				me.append( myself() );
				Kopete::Message chatMessage( message.timestamp(), ocSender, me, sanitizedMsg,
						Kopete::Message::Inbound, Kopete::Message::RichText );

				session->appendMessage( chatMessage );
			}
		}
	}
}

void AIMAccount::connectedToChatRoom( Oscar::WORD exchange, const QString& room )
{
	kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "Creating chat room session" << endl;
	Kopete::ContactPtrList emptyList;
	AIMMyselfContact* me = static_cast<AIMMyselfContact*>( myself() );
	AIMChatSession* session = static_cast<AIMChatSession*>( me->manager( Kopete::Contact::CanCreate,
				exchange, room ) );
	session->setDisplayName( room );
	if ( session->view( true ) )
		session->raiseView();
}

void AIMAccount::userJoinedChat( Oscar::WORD exchange, const QString& room, const QString& contact )
{
	if ( Oscar::normalize( contact ) == Oscar::normalize( myself()->contactId() ) )
		return;

	kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "user " << contact << " has joined the chat" << endl;
	QList<Kopete::ChatSession*> chats = Kopete::ChatSessionManager::self()->sessions();
	QList<Kopete::ChatSession*>::iterator it, itEnd = chats.end();
	for ( it = chats.begin(); it != itEnd; ++it )
	{
		Kopete::ChatSession* kcs = ( *it );
		AIMChatSession* session = dynamic_cast<AIMChatSession*>( kcs );
		if ( !session )
			continue;

		kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << session->exchange() << " " << exchange << endl;
		kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << session->roomName() << " " << room << endl;
		if ( session->exchange() == exchange && session->roomName() == room )
		{
			kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "found correct chat session" << endl;
			Kopete::Contact* c;
			if ( contacts()[Oscar::normalize( contact )] )
				c = contacts()[Oscar::normalize( contact )];
			else
			{
				Kopete::MetaContact* mc = addContact( Oscar::normalize( contact ),
						contact, 0, Kopete::Account::Temporary );
				if ( !mc )
					kWarning(OSCAR_AIM_DEBUG) << "Unable to add contact for chat room" << endl;

				c = mc->contacts().first();
				c->setNickName( contact );
			}

			kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "adding contact" << endl;
			session->addContact( c, AIM::Presence( AIM::Presence::Online ).toOnlineStatus(), true /* suppress */ );
		}
	}
}

void AIMAccount::userLeftChat( Oscar::WORD exchange, const QString& room, const QString& contact )
{
	if ( Oscar::normalize( contact ) == Oscar::normalize( myself()->contactId() ) )
		return;

	QList<Kopete::ChatSession*> chats = Kopete::ChatSessionManager::self()->sessions();
	QList<Kopete::ChatSession*>::iterator it, itEnd = chats.end();
	for ( it = chats.begin(); it != itEnd; ++it )
	{
		Kopete::ChatSession* kcs = ( *it );
		AIMChatSession* session = dynamic_cast<AIMChatSession*>( kcs );
		if ( !session )
			continue;

		if ( session->exchange() == exchange && session->roomName() == room )
		{
			//delete temp contact
			Kopete::Contact* c = contacts()[Oscar::normalize( contact )];
			if ( !c )
			{
				kWarning(OSCAR_AIM_DEBUG) << k_funcinfo << "couldn't find the contact that's left the chat!" << endl;
				continue;
			}
			session->removeContact( c );
			Kopete::MetaContact* mc = c->metaContact();
			if ( mc->isTemporary() )
			{
				mc->removeContact( c );
				delete c;
				delete mc;
			}
		}
	}
}


void AIMAccount::connectWithPassword( const QString &password )
{
	if ( password.isNull() )
		return;

	kDebug(14152) << k_funcinfo << "accountId='" << accountId() << "'" << endl;

	Kopete::OnlineStatus status = initialStatus();
	if ( status == Kopete::OnlineStatus() && status.status() == Kopete::OnlineStatus::Unknown )
		//use default online in case of invalid online status for connecting
		status = Kopete::OnlineStatus( Kopete::OnlineStatus::Online );

	AIM::Presence pres = AIM::Presence::fromOnlineStatus( status );
	bool accountIsOffline = ( presence().type() == AIM::Presence::Offline ||
	                          myself()->onlineStatus() == protocol()->statusManager()->connectingStatus() );

	if ( accountIsOffline )
	{
		kDebug(14152) << k_funcinfo << "Logging in as " << accountId() << endl ;
		myself()->setOnlineStatus( protocol()->statusManager()->connectingStatus() );

		// Get the screen name for this account
		QString screenName = accountId();
		QString server = configGroup()->readEntry( "Server", QString::fromLatin1( "login.oscar.aol.com" ) );
		uint port = configGroup()->readEntry( "Port", 5190 );
		Connection* c = setupConnection( server, port );

		//set up the settings for the account
		Oscar::Settings* oscarSettings = engine()->clientSettings();
		oscarSettings->setFileProxy( configGroup()->readEntry( "FileProxy", false ) );
		oscarSettings->setFirstPort( configGroup()->readEntry( "FirstPort", 5190 ) );
		oscarSettings->setLastPort( configGroup()->readEntry( "LastPort", 5199 ) );
		oscarSettings->setTimeout( configGroup()->readEntry( "Timeout", 10 ) );

		Oscar::DWORD status = pres.toOscarStatus();
		engine()->setStatus( status, mInitialStatusMessage );
		updateVersionUpdaterStamp();
		engine()->start( server, port, accountId(), password.left(16) );
		engine()->connectToServer( c, server, true /* doAuth */ );

		mInitialStatusMessage.clear();
	}
}

void AIMAccount::setPrivacySettings( int privacy )
{
	using namespace AIM::PrivacySettings;

	Oscar::BYTE privacyByte = 0x01;
	Oscar::DWORD userClasses = 0xFFFFFFFF;

	switch ( privacy )
	{
		case AllowAll:
			privacyByte = 0x01;
			break;
		case BlockAll:
			privacyByte = 0x02;
			break;
		case AllowPremitList:
			privacyByte = 0x03;
			break;
		case BlockDenyList:
			privacyByte = 0x04;
			break;
		case AllowMyContacts:
			privacyByte = 0x05;
			break;
		case BlockAIM:
			privacyByte = 0x01;
			userClasses = 0x00000004;
			break;
	}

	this->setPrivacyTLVs( privacyByte, userClasses );
}

void AIMAccount::setPrivacyTLVs( Oscar::BYTE privacy, Oscar::DWORD userClasses )
{
	ContactManager* ssi = engine()->ssiManager();
	OContact item = ssi->findItem( QString::null, ROSTER_VISIBILITY );

	QList<Oscar::TLV> tList;

	tList.append( TLV( 0x00CA, 1, (char *)&privacy ) );
	tList.append( TLV( 0x00CB, sizeof(userClasses), (char *)&userClasses ) );

	if ( !item )
	{
		kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "Adding new privacy TLV item" << endl;
		OContact s( QString::null, 0, ssi->nextContactId(), ROSTER_VISIBILITY, tList );
		engine()->modifyContactItem( item, s );
	}
	else
	{ //found an item
		OContact s(item);

		if ( Oscar::updateTLVs( s, tList ) == true )
		{
			kDebug(OSCAR_AIM_DEBUG) << k_funcinfo << "Updating privacy TLV item" << endl;
			engine()->modifyContactItem( item, s );
		}
	}
}

#include "aimaccount.moc"
//kate: tab-width 4; indent-mode csands;
