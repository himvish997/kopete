/*
    networkconfigwidget.h - IRC Network configurator widget.

    Copyright (c) 2002      by Nick Betcher <nbetcher@kde.org>
    Copyright (c) 2003-2005 by Michel Hermier <michel.hermier@wanadoo.fr>

    Kopete    (c) 2002-2005 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef IRCNETWORKCONFIGWIDGET_H
#define IRCNETWORKCONFIGWIDGET_H

#include "networkconfig.h"

#include "ircnetwork.h"

#include <qdict.h>

class IRCNetworkConfigWidget
	: public NetworkConfig
{
	Q_OBJECT

public:
	IRCNetworkConfigWidget(QWidget *parent, WFlags flags = 0);
	~IRCNetworkConfigWidget();

	QDict<IRCNetwork> &networks(){ return m_networks; }
	void addNetwork( IRCNetwork *network );

	void editNetworks( const QString &networkName = QString::null );

signals:
	void networkConfigUpdated( const QString &selectedNetwork );

private slots:
	// FIXME: All the code for managing the networks list should be in another class - Will
	void slotUpdateNetworkConfig();
	void slotUpdateNetworkHostConfig();
	void slotMoveServerUp();
	void slotMoveServerDown();
	void slotDeleteNetwork();
	void slotDeleteHost();
	void slotNewNetwork();
	void slotRenameNetwork();
	void slotNewHost();
	void slotHostPortChanged( int value );
	// end of network list specific code

private:
	// FIXME: All the code for managing the networks list should be in another class - Will
	void storeCurrentNetwork();
	void storeCurrentHost();

	QString m_uiCurrentNetworkSelection;
	QString m_uiCurrentHostSelection;
	// end of network list specific code

	QDict<IRCNetwork> m_networks;
	QDict<IRCHost> m_hosts;
};

#endif
