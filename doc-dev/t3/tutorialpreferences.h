/*  This file is part of the KDE project
    Copyright (C) 2007 Will Stephenson <wstephenson@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef TUTORIAL_PREFERENCES_H
#define TUTORIAL_PREFERENCES_H

#include <KCModule>

#include "ui_tutorialprefs.h"

class TutorialPreferences : public KCModule
{
    Q_OBJECT
public:
    explicit TutorialPreferences(QWidget *parent = nullptr, const QStringList &args = QStringList());
    ~TutorialPreferences();
    virtual void save();
    virtual void load();

private:
    Ui_TutorialPrefsUI ui;
};

#endif
