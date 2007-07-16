/*
    kopetedeletecontacttask.h - Kopete Delete Contact Task

    Copyright (c) 2007      by Michaël Larouche      <larouche@kde.org>

    Kopete    (c) 2002-2007 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/
#include "kopetedeletecontacttask.h"

// Qt includes

// KDE includes
#include <kdebug.h>

// Kopete includes
#include <kopeteaccount.h>
#include <kopetecontact.h>

namespace Kopete
{

class DeleteContactTask::Private
{
public:
};

DeleteContactTask::DeleteContactTask(QObject *parent)
 : Kopete::ContactTaskBase(parent), d(new Private)
{
}

DeleteContactTask::DeleteContactTask(Kopete::Contact *contact)
 : Kopete::ContactTaskBase(contact), d(new Private)
{
}

DeleteContactTask::~DeleteContactTask()
{
	delete d;
}

void DeleteContactTask::start()
{
	Q_ASSERT( !contact() );

	if( contact()->account() && !contact()->account()->isConnected() )
	{
		kDebug(14010) << k_funcinfo << "ERROR: Network is unavailable" << endl;

		setError( DeleteContactTask::NetworkUnavailableError );
		emitResult();

		return;
	}

	if( subjobs().empty() )
	{
		kDebug(14010) << k_funcinfo << "WARNING: This task does not contain protocol delete children task. Using default behavior, calling obsolete deleteContact()" << endl;

		contact()->deleteContact();
		emitResult();

		return;
	}

	// Call default implemention which execute sub jobs
	Kopete::Task::start();
}

void DeleteContactTask::slotResult(KJob *job)
{
	KCompositeJob::slotResult(job);
	// We executed all subjobs, delete the contact from memory.
	// This is the default behavior from Kopete::Contact old deleteContact()
	// method.
	if( !job->error() && subjobs().empty() )
	{
		kDebug(14010) << k_funcinfo << "Deleting Kopete::Contact from memory" << endl;
		contact()->deleteLater();
		emitResult();
	}
}

QString DeleteContactTask::taskType() const
{
	return QString("DeleteContactTask");
}

} // namespace Kopete

#include "kopetedeletecontacttask.moc"
