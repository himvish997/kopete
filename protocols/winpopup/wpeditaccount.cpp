/***************************************************************************
                          yahooaddcontact.cpp  -  description
                             -------------------
    begin                : Fri Apr 26 2002
    copyright            : (C) 2002 by Gav Wood
    email                : gav@kde.org

    Based on code from   : (C) 2002 by Duncan Mac-Vicar Prett
    email                : duncan@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// QT Includes
#include <qcheckbox.h>

// KDE Includes
#include <kdebug.h>
#include <klocale.h>
#include <kurlrequester.h>
#include <klineedit.h>
#include <kmessagebox.h>

// Kopete Includes
#include <addcontactpage.h>

// Local Includes
#include "wpaccount.h"
#include "wpeditaccount.h"

WPEditAccount::WPEditAccount(WPProtocol *protocol, KopeteAccount *theAccount, QWidget *parent, const char *name): WPEditAccountBase(parent), EditAccountWidget(theAccount)
{
	kdDebug(14180) << "WPEditAccount::WPEditAccount(<protocol>, <theAccount>, <parent>, " << name << ")";

	theProtocol = protocol;
	if(m_account)
	{	mHostName->setText(m_account->accountId());
		mAutoConnect->setChecked(m_account->autoLogin());
		mHostName->setReadOnly(true);
	}
	else
	{	QString theHostName = "";
		QFile infile("/etc/hostname");
		if(infile.open(IO_ReadOnly))
		{	QTextStream in(&infile);
			char c;
			for(in >> c; c != '.' && (!infile.atEnd()); in >> c)
				theHostName = theHostName + char((c >= 65 && c < 91) ? c : (c - 32));
			infile.close();
		}
		else
			theHostName = "LOCALHOST";
		mHostName->setText(theHostName);
	}

	show();
}

void WPEditAccount::installSamba()
{
	theProtocol->installSamba();
}

bool WPEditAccount::validateData()
{
	kdDebug(14180) << "WPEditAccount::validateData()";

	if(mHostName->text() == "")
	{	KMessageBox::sorry(this, i18n("<qt>You must enter a valid screen name.</qt>"), i18n("WP"));
		return false;
	}

	return true;
}

KopeteAccount *WPEditAccount::apply()
{
	kdDebug(14180) << "WPEditAccount::apply()";

	if(!m_account)
		m_account = new WPAccount(theProtocol, mHostName->text());
//	else
//		m_account->setAccountId(mHostName->text());
	m_account->setAutoLogin(mAutoConnect->isChecked());
	
	return m_account;
}

#include "wpeditaccount.moc"
/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

