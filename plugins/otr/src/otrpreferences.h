/***************************************************************************
 *   Copyright (C) 2007 by Michael Zanetti
 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef OTRPREFERECES_H
#define OTRPREFERECES_H

#include <kcmodule.h>
#include "otrlconfinterface.h"
#include "ui_otrprefs.h"

/**
 * Preference widget for the OTR plugin
 * @author Michael Zanetti
 */

class OTRPreferences : public KCModule  {
   Q_OBJECT

public:
	OTRPreferences(QWidget *parent = 0, const QVariantList &args = QVariantList());
	~OTRPreferences();

private:
	Ui::OTRPrefsUI *preferencesDialog;
	OtrlConfInterface *otrlConfInterface;
	QMap<int, int> privKeys;

private slots: // Public slots
	void generateFingerprint();
	void showPrivFingerprint(int accountnr);
	void verifyFingerprint();
	void fillFingerprints();
	void updateButtons(int row, int col, int prevRow, int prevCol);
	void forgetFingerprint();

};

#endif
