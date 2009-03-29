/*  This file is part of the KDE project
    Copyright (C) 2005 Kopete Developers <kopete-devel@kde.org>
    Copyright (C) 2008-2009 Pali Rohár <pali.rohar@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

*/
#include "skypeconnection.h"
#include "clientadaptor.h"

#include <QtDBus>
#include <QVariant>
#include <qstringlist.h>
#include <qtimer.h>
#include <kdebug.h>
#include <klocale.h>
#include <unistd.h>

typedef enum {
	cfConnected,
	cfNotConnected,
	cfNameSent,
	cfProtocolSent,
	cfWaitingStart
} connFase;

class SkypeConnectionPrivate {
	public:
		///Are we connected/connecting?
		connFase fase;
		///How will we be known to skype?
		QString appName;
		///What is the protocol version used (wanted if not connected yet)
		int protocolVer;
		///Bus type 0 - session, 1 - system
		int bus;
		///This timer will keep trying until Skype starts
		QTimer *startTimer;
		///How much time rest? (until I say starting skype did not work)
		int timeRemaining;
		///Wait a while before we conect to just-started skype?
		int waitBeforeConnect;
		///Skype process
		QProcess skypeProcess;
};

SkypeConnection::SkypeConnection() {
	kDebug() << k_funcinfo << endl;//some debug info

	d = new SkypeConnectionPrivate;//create the d pointer
	d->fase = cfNotConnected;//not connected yet
	d->startTimer = 0L;
	connect(this, SIGNAL(received(const QString&)), this, SLOT(parseMessage(const QString&)));//look into all messages
}

SkypeConnection::~SkypeConnection() {
	kDebug() << k_funcinfo << endl;//some debug info

	disconnectSkype();//disconnect before you leave
	if ( d->skypeProcess.state() != QProcess::NotRunning )//if we started skype process kill it
		d->skypeProcess.kill();
	QProcess::execute("bash -c \"pkill -6 -U $USER -x skype.*\"");//try find skype process (skype, skype.real, ...) and kill it if we dont start skype or use skype.real wrapper
	delete d;//Remove the D pointer
}

QString SkypeConnection::convertMessage(const QList <QVariant> messagelist) {
	QString message;
	for ( QList <QVariant>::const_iterator it = messagelist.begin(); it != messagelist.end(); ++it ) {
		message.append((*it).toString());//get all messages to one string
		message.append(" ");//separate messages with space
	}
	return message.trimmed();
}

QDBusConnection SkypeConnection::getBus() {
	//0 - session bus, 1 - system bus, default:0
	if ( d->bus == 1 )
		return QDBusConnection::systemBus();
	return QDBusConnection::sessionBus();
}

void SkypeConnection::startLogOn() {
	kDebug() << k_funcinfo << endl;//some debug info

	if (d->startTimer) {
		d->startTimer->deleteLater();
		d->startTimer = 0L;
	}

	QDBusMessage reply = QDBusInterface("com.Skype.API", "/com/Skype", "com.Skype.API", getBus()).call("Invoke", "PING");
	QString replystring = convertMessage(reply.arguments());

	if ( replystring != "PONG" ){
		emit error(i18n("Could not ping Skype"));
		disconnectSkype(crLost);
		emit connectionDone(seNoSkype, 0);
		return;
	}

	d->fase = cfNameSent;
	send(QString("NAME %1").arg(d->appName));
}

void SkypeConnection::parseMessage(const QString &message) {
	kDebug() << k_funcinfo << endl;//some debug info

	switch (d->fase) {
		case cfNameSent: {

			if (message == "OK") {//Accepted by skype
				d->fase = cfProtocolSent;//Sending the protocol
				send(QString("PROTOCOL %1").arg(d->protocolVer));//send the protocol version
			} else {//Not accepted by skype
				emit error(i18n("Skype did not accept this application"));//say there is an error
				emit connectionDone(seAuthorization, 0);//Problem with authorization
				disconnectSkype(crLost);//Lost the connection
			}
			break;
		}
		case cfProtocolSent: {
			if (message.contains(QString("PROTOCOL"), Qt::CaseSensitivity(false))) {//This one inform us what protocol do we use
				bool ok;
				int version = message.section(' ', 1, 1).trimmed().toInt(&ok, 0);//take out the protocol version and make it int
				if (!ok) {
					emit error(i18n("Skype API syntax error"));
					emit connectionDone(seUnknown, 0);
					disconnectSkype(crLost);//lost the connection
					return;//I have enough
				}
				d->protocolVer = version;//this will be the used version of protocol
				d->fase = cfConnected;
				emit connectionDone(seSuccess, version);//tell him that we are connected at last
			} else {//the API is not ready yet, try later
				emit error(i18n("Skype API not ready yet, wait a bit longer"));
				emit connectionDone(seUnknown, 0);
				disconnectSkype(crLost);
				return;
			}
			break;//Other messages are ignored, waiting for the protocol response
		}
	}
}

void SkypeConnection::Notify(const QString &message){
	kDebug() << k_funcinfo << "Got message:" << message << endl;//show what we have got
	emit received(message);
}

void SkypeConnection::connectSkype(const QString &start, const QString &appName, int protocolVer, int bus, int launchTimeout, int waitBeforeConnect, const QString &name, const QString &pass) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (d->fase != cfNotConnected)
		return;

	d->appName = appName;
	d->protocolVer = protocolVer;
	d->bus = bus;

	new ClientAdaptor(this);
	QDBusConnection::sessionBus().registerObject("/com/Skype/Client", this); //Register skype client to dbus for receiving messages to slot Notify

	{
		QDBusInterface interface("com.Skype.API", "/com/Skype", "com.Skype.API", getBus());
		QDBusMessage reply = interface.call("Invoke", "PING");
		QString replystring = convertMessage(reply.arguments());

		bool started = interface.isValid();
		bool loggedin = replystring == "PONG";

		if ( ! started || ! loggedin ){
			if ( ! started && ! start.isEmpty() ) {//try starting Skype by the given command
				QStringList args = start.split(' ');
				QString skypeBin = args.takeFirst();
				if ( !name.isEmpty() && !pass.isEmpty() ){
					args << "--pipelogin";
				}
				kDebug() << k_funcinfo << "Starting skype process" << skypeBin << "with parms" << args << endl;
				d->skypeProcess.start(skypeBin, args);
				if ( !name.isEmpty() && !pass.isEmpty() ){
					kDebug() << k_funcinfo << "Sending login name:" << name << endl;
					d->skypeProcess.write(name.trimmed().toLocal8Bit());
					d->skypeProcess.write(" ");
					kDebug() << k_funcinfo << "Sending password" << endl;
					d->skypeProcess.write(pass.trimmed().toLocal8Bit());
					d->skypeProcess.closeWriteChannel();
				}
				d->skypeProcess.waitForStarted();
				kDebug() << k_funcinfo << "Skype process state:" << d->skypeProcess.state() << "Skype process error:" << d->skypeProcess.error() << endl;
				if ( d->skypeProcess.state() != QProcess::Running || d->skypeProcess.error() == QProcess::FailedToStart ) {
					emit error(i18n("Could not launch Skype.\nYou need to install the original dynamic linked Skype version 2.0 binary from http://www.skype.com"));
					disconnectSkype(crLost);
					emit connectionDone(seNoSkype, 0);
					return;
				}
			} else {
				if ( start.isEmpty() ){
					emit error(i18n("Could not find Skype.\nYou need to install the original dynamic linked Skype version 2.0 binary from http://www.skype.com"));
					disconnectSkype(crLost);
					emit connectionDone(seNoSkype, 0);
					return;
				}
			}
			d->fase = cfWaitingStart;
			d->startTimer = new QTimer();
			connect(d->startTimer, SIGNAL(timeout()), this, SLOT(tryConnect()));
			d->startTimer->start(1000);
			d->timeRemaining = launchTimeout;
			d->waitBeforeConnect = waitBeforeConnect;
			return;
		}
	}

	startLogOn();
}

void SkypeConnection::disconnectSkype(skypeCloseReason reason) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (d->startTimer) {
		d->startTimer->stop();
		d->startTimer->deleteLater();
		d->startTimer = 0L;
	}

	d->fase = cfNotConnected;//No longer connected
	emit connectionDone(seCanceled, 0);
	emit connectionClosed(reason);//we disconnect
}

QString SkypeConnection::operator %(const QString &message) {
	kDebug() << k_funcinfo << "Send message:" << message << endl;//some debug info

	if (d->fase == cfNotConnected)
		return "";//not connected, posibly because of earlier error, do not show it again

	QDBusInterface interface("com.Skype.API", "/com/Skype", "com.Skype.API", getBus());
	QDBusMessage reply = interface.call("Invoke", message);

	if ( interface.lastError().type() != QDBusError::NoError && interface.lastError().type() != QDBusError::Other ){//There was some error
		if ( message == "PING" )
			emit error(i18n("Could not ping Skype.\nMaybe Skype not running.\nError while sending a message to Skype (%1).", QDBusError::errorString(interface.lastError().type())));//say there was the error
		else
			emit error(i18n("Error while sending a message to Skype (%1).", QDBusError::errorString(interface.lastError().type())));//say there was the error
		if (d->fase != cfConnected)
			emit connectionDone(seUnknown, 0);//Connection attempt finished with error
		disconnectSkype(crLost);//lost the connection
		return "";//this is enough, no more errors please..
	}

	QString replystring = convertMessage(reply.arguments());

	if ( message == "PING" && replystring != "PONG" ){
		emit error(i18n("Could not ping Skype.\nYou are logged out from Skype, please log in."));
		emit connectionDone(seNoSkype, 0);
		disconnectSkype(crLost);//lost the connection
		return "";//this is enough, no more errors please..
	}

	kDebug() << "Reply message:" << replystring << endl;//show what we have received
	return replystring;//ok, just return it
}

void SkypeConnection::send(const QString &message) {
	kDebug() << k_funcinfo << endl;//some debug info

	QString reply = *this % message;
	if ( ! reply.isEmpty() )
		emit received(reply);
}

SkypeConnection &SkypeConnection::operator <<(const QString &message) {
	send(message);//just send it
	return *this;//and return yourself
}

bool SkypeConnection::connected() const {
	kDebug() << k_funcinfo << endl;//some debug info

	return d->fase == cfConnected;
}

int SkypeConnection::protocolVer() const {
	kDebug() << k_funcinfo << endl;//some debug info

	return d->protocolVer;//just give him the protocol version
}

void SkypeConnection::tryConnect() {
	kDebug() << k_funcinfo << endl;//some debug info

	{
		QDBusInterface interface("com.Skype.API", "/com/Skype", "com.Skype.API", getBus());
		QDBusMessage reply = interface.call("Invoke", "PING");
		QString replystring = convertMessage(reply.arguments());

		bool started = interface.isValid();
		bool loggedin = replystring == "PONG";

		if ( ! started || ! loggedin ){
			if (--d->timeRemaining == 0) {
				d->startTimer->stop();
				d->startTimer->deleteLater();
				d->startTimer = 0L;
				if ( !started )
					emit error(i18n("Could not find Skype\nYou need to install the original dynamic linked Skype version 2.0 binary from http://www.skype.com"));
				else
					emit error(i18n("Please login to Skype first"));
				disconnectSkype(crLost);
				emit connectionDone(seNoSkype, 0);
				return;
			}
			return;//Maybe next time
		}
	}

	d->startTimer->stop();
	d->startTimer->deleteLater();
	d->startTimer = 0L;
	if (d->waitBeforeConnect) {
		QTimer::singleShot(1000 * d->waitBeforeConnect, this, SLOT(startLogOn()));
		//Skype does not like being bothered right after it's start, give it a while to breathe
	} else
		startLogOn();//OK, it's your choise
}

#include "skypeconnection.moc"