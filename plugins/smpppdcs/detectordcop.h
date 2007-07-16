/*
    detectordcop.h
 
    Copyright (c) 2004-2006 by Heiko Schaefer        <heiko@rangun.de>
 
    Kopete    (c) 2002-2006 by the Kopete developers <kopete-devel@kde.org>
 
    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; version 2 of the License.               *
    *                                                                       *
    *************************************************************************
*/

#ifndef DETECTORDCOP_H
#define DETECTORDCOP_H

#include "detector.h"

#include <QByteArray>

class IConnector;

/**
	@author Heiko Sch&auml;fer <heiko@rangun.de>
*/
class DetectorDCOP : public Detector {

public:
    explicit DetectorDCOP(IConnector * connector);
    virtual ~DetectorDCOP();

protected:

    enum KInternetDCOPState {
        CONNECTED,
        DISCONNECTED,
        ERROR
    };

    QByteArray getKInternetDCOP() const;
    KInternetDCOPState getConnectionStatusDCOP() const;

protected:
    static QByteArray  m_kinternetApp;

private:
    Q_DISABLE_COPY(DetectorDCOP)
};

#endif
