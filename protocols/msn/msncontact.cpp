/*
    msncontact.cpp - MSN Contact

    Copyright (c) 2002 Duncan Mac-Vicar Prett <duncan@kde.org>
              (c) 2002 Ryan Cumming           <bodnar42@phalynx.dhs.org>
              (c) 2002 Martijn Klingens       <klingens@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <kaction.h>
#include <kdebug.h>
#include <klocale.h>
#include <kpopupmenu.h>

#include "kmsnservice.h"
//#include "kopetechatwindow.h" // Debug, Martijn
#include "kopetestdaction.h"
#include "msncontact.h"
#include "msnprotocol.h"

// Constructor for no-groups
MSNContact::MSNContact( const QString &msnId, const QString &nickname,
	const QString &group, MSNProtocol *protocol )
	: KopeteContact( protocol )
{
	initContact( msnId, nickname, group, protocol );
}

void MSNContact::initContact( const QString &msnId, const QString &nickname,
	const QString &group, const MSNProtocol *protocol)
{
	m_actionRemove = 0L;
	m_actionRemoveFromGroup = 0L;
	m_actionChat = 0L;
	m_actionInfo = 0L;
	m_actionHistory = 0L;
	m_actionMove = 0L;
	m_actionCopy = 0L;

	historyDialog = 0L;

	m_protocol = protocol;
	m_msnId = msnId;
	m_nickname = nickname;
	m_group =  group;
	hasLocalGroup = false;

	// We connect this signal so that we can tell when a user's status changes
	connect( protocol, SIGNAL( updateContact( QString, uint ) ),
				this, SLOT( slotUpdateContact( QString, uint ) ) );
	connect( protocol, SIGNAL( contactRemoved( QString, QString ) ),
				this, SLOT( slotContactRemoved( QString, QString ) ) );

	connect ( this, SIGNAL(chatToUser(QString)), protocol->msnService(), SLOT( slotStartChatSession(QString)) );
	connect ( protocol, SIGNAL( connectedToService( bool ) ), this, SLOT( slotDeleteMySelf( bool ) ) );

	setName( nickname );
	slotUpdateContact( m_msnId, m_protocol->contactStatus( m_msnId ) );
}

void MSNContact::showContextMenu(QPoint point)
{
	popup = new KPopupMenu();
	popup->insertTitle( i18n( "%1 (%2)" ).arg( name() ).arg( msnId() ) );

	// Chat with user
	if( !m_actionChat )
	{
		m_actionChat = KopeteStdAction::sendMessage(
			this, SLOT( slotChatThisUser() ), this, "m_actionChat" );
	}
	m_actionChat->plug( popup );
	popup->insertSeparator();

	// View History
	if( !m_actionHistory )
	{
		m_actionHistory = KopeteStdAction::viewHistory( this,
			SLOT( slotViewHistory() ), this, "m_actionHistory" );
	}
	m_actionHistory->plug( popup );
	popup->insertSeparator();

	// Move Contact
	if( !m_actionMove )
	{
		m_actionMove = KopeteStdAction::moveContact( this,
			SLOT( slotMoveThisUser() ), this, "m_actionMove" );
	}
	m_actionMove->plug( popup );

	// Copy Contact
	if( !m_actionCopy )
	{
		m_actionCopy = new KListAction( i18n( "Copy Contact" ), "editcopy", 0,
			this, SLOT( slotCopyThisUser() ), this, "m_actionCopy" );
	}
	m_actionCopy->setItems( m_protocol->groups() );
	m_actionCopy->plug( popup );

	// Remove From Group
	if( !m_actionRemoveFromGroup )
	{
		m_actionRemoveFromGroup = new KAction( i18n( "Remove From Group" ),
			"edittrash", 0, this, SLOT( slotRemoveFromGroup() ),
			this, "m_actionRemoveFromGroup" );
	}
	m_actionRemoveFromGroup->plug( popup );

	// Remove Contact
	if( !m_actionRemove )
	{
		m_actionRemove = KopeteStdAction::deleteContact( this,
			SLOT( slotRemoveThisUser() ), this, "m_actionRemove" );
	}
	m_actionRemove->plug( popup );

	popup->popup( point );
}

void MSNContact::execute()
{
//	( new KopeteChatWindow( this, this, QString::null, 0 ) )->show(); // Debug, Martijn
	emit chatToUser( m_msnId );
}

void MSNContact::slotChatThisUser()
{
	emit chatToUser( m_msnId );
}

void MSNContact::slotRemoveThisUser()
{
	m_protocol->removeContact( m_msnId );
	delete this;
}

void MSNContact::slotRemoveFromGroup()
{
	m_protocol->removeFromGroup( m_msnId, m_group );
}

void MSNContact::slotMoveThisUser()
{
	if( m_actionMove )
		m_protocol->moveContact( this, m_actionMove->currentText() );
}

void MSNContact::slotCopyThisUser()
{
	if( m_actionCopy )
		m_protocol->copyContact( m_msnId, m_actionCopy->currentText() );
}

void MSNContact::slotContactRemoved(QString handle, QString group)
{
	if ( (handle == m_msnId) && ( group == m_group ) )
	{
		delete this;
	}
}

void MSNContact::slotUpdateContact ( QString handle, uint status)
{
	if (handle != m_msnId) // not our contact
		return;

	if ( status == mStatus ) // no statuschange
		return;

	kdDebug() << "MSN Plugin: Contact " << handle <<" request update (" << status << ")\n";
	mStatus = status;
	QString tmppublicname = m_protocol->publicName( handle );

	if (mStatus == BLO)
		setName( i18n("%1 (Blocked)").arg(tmppublicname) );
	else
		setName( tmppublicname );

	emit statusChanged();
}

void MSNContact::slotDeleteMySelf(bool connected)
{
	if (!connected)
	{
		delete this;
	}
}


void MSNContact::slotViewHistory()
{
	kdDebug() << "MSN Plugin: slotViewHistory()" << endl;

	if (historyDialog != 0L)
	{
		historyDialog->raise();
	}
	else
	{
		historyDialog = new KopeteHistoryDialog(QString("kopete/msn_logs/%1.log").arg(m_msnId), name(), true, 50, 0, "MSNHistoryDialog");

		connect ( historyDialog, SIGNAL(closing()), this, SLOT(slotCloseHistoryDialog()) );
		connect ( historyDialog, SIGNAL(destroyed()), this, SLOT(slotHistoryDialogClosing()) );
	}
}

void MSNContact::slotCloseHistoryDialog()
{
	kdDebug() << "MSN Plugin: slotCloseHistoryDialog()" << endl;
	delete historyDialog;
}

void MSNContact::slotHistoryDialogClosing()
{
	kdDebug() << "MSN Plugin: slotHistoryDialogClosing()" << endl;
	if (historyDialog != 0L)
	{
		historyDialog = 0L;
	}
}

MSNContact::ContactStatus MSNContact::status() const
{
	switch ( mStatus )
	{
		case NLN: // Online
		{
			return Online;
			break;
		}
		case BSY: // Busy
		case IDL: // Idle
		case AWY: // Away from computer
		case PHN: // On the phone
		case BRB: // Be right back
		case LUN: // Out to lunch
		{
			return Away;
			break;
		}

		default:
		{
			return Offline;
			break;
		}
	}

}

QString MSNContact::statusText() const
{
	switch ( mStatus )
	{
		case BLO: // blocked
		{
			return i18n("Blocked");
			break;
		}
		case NLN: // Online
		{
			return i18n("Online");
			break;
		}
		case BSY: // Busy
		{
			return i18n("Busy");
			break;
		}
		case IDL: // Idle
		{
			return i18n("Idle");
			break;
		}
		case AWY: // Away from computer
		{
			return i18n("Away From Computer");
			break;
		}
		case PHN: // On the phone
		{
			return i18n("On The Phone");
			break;
		}
		case BRB: // Be right back
		{
			return i18n("Be Right Back");
			break;
		}
		case LUN: // Out to lunch
		{
			return i18n("Out To Lunch");
			break;
		}

		default:
		{
			return i18n("Offline");
		}
	}
}

QString MSNContact::statusIcon() const
{
	switch ( mStatus )
	{
		case NLN: // Online
		{
			return "msn_online";
			break;
		}
		case BSY: // Busy
		case PHN: // On the phone
		{
			return "msn_na";
			break;
		}
		case IDL: // Idle
		case AWY: // Away from computer
		case BRB: // Be right back
		case LUN: // Out to lunch
		{
			return "msn_away";
			break;
		}
	}

	return "msn_offline";
}

int MSNContact::importance() const
{
	switch ( mStatus )
	{
		case BLO: // blocked
		{
			return 1;
			break;
		}
		case NLN: // Online
		{
			return 20;
			break;
		}
		case BSY: // Busy
		{
			return 13;
			break;
		}
		case IDL: // Idle
		{
			return 15;
			break;
		}
		case AWY: // Away from computer
		{
			return 10;
			break;
		}
		case PHN: // On the phone
		{
			return 12;
			break;
		}
		case BRB: // Be right back
		{
			return 14;
			break;
		}
		case LUN: // Out to lunch
		{
			return 11;
			break;
		}

		default:
		{
			return 0;
		}
	}
}

QString MSNContact::msnId() const
{
	return m_msnId;
}

QString MSNContact::nickname() const
{
	return m_nickname;
}

QString MSNContact::group() const
{
	return m_group;
}

#include "msncontact.moc"

// vim: noet ts=4 sts=4 sw=4:

