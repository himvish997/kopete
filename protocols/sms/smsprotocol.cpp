/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "smsprotocol.h"
#include "smspreferences.h"
#include "smscontact.h"
#include "smsaddcontactpage.h"
#include "kopetemetacontact.h"
#include <kgenericfactory.h>
#include <kmessagebox.h>
#include <kdebug.h>

K_EXPORT_COMPONENT_FACTORY( kopete_sms, KGenericFactory<SMSProtocol> );

SMSProtocol::SMSProtocol( QObject *parent, const char *name, const QStringList& /*args*/)
: KopeteProtocol( parent, name )
{
	if (s_protocol)
		kdDebug() << "SMSProtocol::SMSProtocol: WARNING s_protocol already defined!" << endl;
	else
		s_protocol = this;
	
	new SMSPreferences("sms_protocol", this);

	QString protocolId = this->id();

	m_mySelf = new SMSContact(protocol(), "", "", 0);
}

SMSProtocol::~SMSProtocol()
{
	s_protocol = 0L;
}

bool SMSProtocol::unload()
{
	return KopeteProtocol::unload();
}

void SMSProtocol::Connect()
{
}

void SMSProtocol::Disconnect()
{
}

bool SMSProtocol::isConnected() const
{
	return true;
}


void SMSProtocol::setAway(void)
{
}

void SMSProtocol::setAvailable(void)
{
}

bool SMSProtocol::isAway(void) const
{
	return false;
}

KopeteContact* SMSProtocol::myself() const
{
	return m_mySelf;
}

QString SMSProtocol::protocolIcon() const
{
	return "metacontact_online";
}

AddContactPage *SMSProtocol::createAddContactWidget(QWidget *parent)
{
	return (new SMSAddContactPage(this,parent));
}

SMSContact* SMSProtocol::addContact( const QString nr , const QString name, KopeteMetaContact *m)
{
	SMSContact* c = new SMSContact(protocol(), nr, name, m);
	m->addContact(c);
	return c;
}

SMSProtocol* SMSProtocol::s_protocol = 0L;

SMSProtocol* SMSProtocol::protocol()
{
	return s_protocol;
}

void SMSProtocol::serialize( KopeteMetaContact *metaContact)
{
	QStringList stream;
	
	QPtrList<KopeteContact> contacts = metaContact->contacts();
	for( KopeteContact *c = contacts.first(); c ; c = contacts.next() )
	{
		if ( c->protocol()->id() != this->id() )
			continue;

		SMSContact *g = static_cast<SMSContact*>(c);

		if (g)
		{
			stream << g->id() << g->displayName();
			if (g->serviceName() != QString::null)
			{
				stream << g->serviceName();
				if (g->servicePrefsString() != QString::null)
					stream << g->servicePrefsString();
				else
					stream << " ";
			}
			else
				stream << " " << " ";
		}
	}
	metaContact->setPluginData(this, stream);
}

void SMSProtocol::deserialize( KopeteMetaContact *metaContact,
	const QStringList &strList )
{
	QString protocolId = this->id();

	uint idx = 0;
	while( idx < strList.size() )
	{
		QString nr = strList[ idx ];
		QString name = strList[ idx+1];
		QString serviceName = strList[ idx+2 ];
		QString servicePrefs = strList[ idx+3 ];

		SMSContact* c = addContact(nr, name, metaContact);
		if (serviceName != " " && serviceName != QString::null)
		{
			c->setServiceName(serviceName);
			if (servicePrefs != " " && servicePrefs != QString::null)
				c->setServicePrefsString(servicePrefs);
		}
		idx += 4;
	}
}


#include "smsprotocol.moc"



/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

