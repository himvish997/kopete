/*
    netmeetingplugin.h

    Copyright (c) 2003 by Olivier Goffart <ogoffart@tiscalinet.be>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/



#ifndef NetMeetingPLUGIN_H
#define NetMeetingPLUGIN_H

#include "kopeteplugin.h"

class KopeteMessageManager;
class MSNMessageManager;
class MSNContact;
class MSNInvitation;


class NetMeetingPlugin : public KopetePlugin
{
	Q_OBJECT

public:
	NetMeetingPlugin( QObject *parent, const char *name, const QStringList &args );
	~NetMeetingPlugin();

	virtual KActionCollection *customChatActions(KopeteMessageManager*);

private slots:
	void slotStartInvitation();
	void slotPluginLoaded(KopetePlugin*);
	void slotInvitation(MSNInvitation*& invitation,  const QString &bodyMSG , long unsigned int cookie , MSNMessageManager* msnMM , MSNContact* c );


private:
	KActionCollection *m_actions;
	MSNMessageManager *m_currentKopeteMessageManager;

};

#endif

