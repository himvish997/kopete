/*
  aimcontactbase.cpp  -  AIM Contact Base

  Copyright (c) 2003 by Will Stephenson
  Copyright (c) 2006 by Roman Jarosz <kedgedev@centrum.cz>
  Kopete    (c) 2002-2006 by the Kopete developers  <kopete-devel@kde.org>

  *************************************************************************
  *                                                                       *
  * This program is free software; you can redistribute it and/or modify  *
  * it under the terms of the GNU General Public License as published by  *
  * the Free Software Foundation; either version 2 of the License, or     *
  * (at your option) any later version.                                   *
  *                                                                       *
  *************************************************************************
*/
#include "aimcontactbase.h"

#include <QtXml/QDomDocument>
#include <QtXml/QDomNodeList>
#include "kopetechatsession.h"

#include "oscaraccount.h"

//liboscar
#include "oscarutils.h"

AIMContactBase::AIMContactBase( Kopete::Account* account, const QString& name, Kopete::MetaContact* parent,
                        const QString& icon, const OContact& ssiItem )
: OscarContact(account, name, parent, icon, ssiItem )
{	
	m_mobile = false;
	// Set the last autoresponse time to the current time yesterday
	m_lastAutoresponseTime = QDateTime::currentDateTime().addDays(-1);
}

AIMContactBase::~AIMContactBase()
{
}

QString AIMContactBase::sanitizedMessage( const QString& message )
{
	QDomDocument doc;
	QString domError;
	int errLine = 0, errCol = 0;
	doc.setContent( message, false, &domError, &errLine, &errCol );
	if ( !domError.isEmpty() ) //error parsing, do nothing
	{
		kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "error from dom document conversion: "
			<< domError << endl;
		return message;
	}
	else
	{
		kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "conversion to dom document successful."
			<< "looking for font tags" << endl;
		QDomNodeList fontTagList = doc.elementsByTagName( "font" );
		if ( fontTagList.count() == 0 )
		{
			kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "No font tags found. Returning normal message" << endl;
			return message;
		}
		else
		{
			kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Found font tags. Attempting replacement" << endl;
			uint numFontTags = fontTagList.count();
			for ( uint i = 0; i < numFontTags; i++ )
			{
				QDomNode fontNode = fontTagList.item(i);
				QDomElement fontEl;
				if ( !fontNode.isNull() && fontNode.isElement() )
					fontEl = fontTagList.item(i).toElement();
				else
					continue;
				if ( fontEl.hasAttribute( "back" ) )
				{
					kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Found attribute to replace. Doing replacement" << endl;
					QString backgroundColor = fontEl.attribute( "back" );
					backgroundColor.insert( 0, "background-color: " );
					backgroundColor.append( ';' );
					fontEl.setAttribute( "style", backgroundColor );
					fontEl.removeAttribute( "back" );
				}
			}
		}
	}
	kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "sanitized message is " << doc.toString();
	return doc.toString();
}

void AIMContactBase::sendAutoResponse(Kopete::Message& msg)
{
	// The target time is 2 minutes later than the last message
	int delta = m_lastAutoresponseTime.secsTo( QDateTime::currentDateTime() );
	kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Last autoresponse time: " << m_lastAutoresponseTime << endl;
	kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Current time: " << QDateTime::currentDateTime() << endl;
	kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Difference: " << delta << endl;
	// Check to see if we're past that time
	if(delta > 120)
	{
		kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Sending auto response" << endl;
		
		// This code was yoinked straight from OscarContact::slotSendMsg()
		// If only that slot wasn't private, but I'm not gonna change it right now.
		Oscar::Message message;
		
		if ( m_details.hasCap( CAP_UTF8 ) )
		{
			message.setText( Oscar::Message::UCS2, msg.plainBody() );
		}
		else
		{
			QTextCodec* codec = contactCodec();
			message.setText( Oscar::Message::UserDefined, msg.plainBody(), codec );
		}
		
		message.setTimestamp( msg.timestamp() );
		message.setSender( mAccount->accountId() );
		message.setReceiver( mName );
		message.setChannel( 0x01 );
		
		// isAuto defaults to false
		mAccount->engine()->sendMessage( message, true);
		kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Sent auto response" << endl;
		manager(Kopete::Contact::CanCreate)->appendMessage(msg);
		manager(Kopete::Contact::CanCreate)->messageSucceeded();
		// Update the last autoresponse time
		m_lastAutoresponseTime = QDateTime::currentDateTime();
	}
	else
	{
		kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "Not enough time since last autoresponse, NOT sending" << endl;
	}
}

void AIMContactBase::slotSendMsg(Kopete::Message& message, Kopete::ChatSession *)
{
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

	s.remove ( QRegExp ( QString::fromLatin1("<br>$") ) ); 
	
	kDebug(OSCAR_GEN_DEBUG) << k_funcinfo << "sending " << s << endl;
	
	// XXX Need to check for message size?
	
	if ( m_details.hasCap( CAP_UTF8 ) )
		msg.setText( Oscar::Message::UCS2, s );
	else
		msg.setText( Oscar::Message::UserDefined, s, contactCodec() );
	
	msg.setReceiver(mName);
	msg.setTimestamp(message.timestamp());
	msg.setChannel(0x01);
	
	mAccount->engine()->sendMessage(msg);
	
	// Show the message we just sent in the chat window
	manager(Kopete::Contact::CanCreate)->appendMessage(message);
	manager(Kopete::Contact::CanCreate)->messageSucceeded();
}

#include "aimcontactbase.moc"
//kate: tab-width 4; indent-mode csands;