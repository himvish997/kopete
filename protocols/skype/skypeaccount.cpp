/*  This file is part of the KDE project
    Copyright (C) 2005 Michal Vaner <michal.vaner@kdemail.net>
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

#include "skypeaccount.h"
#include "skypeprotocol.h"
#include "skypecontact.h"
#include "skype.h"
#include "skypecalldialog.h"
#include "skypechatsession.h"
#include "skypeconference.h"
#include "skypeactionhandleradaptor.h"

#include <qstring.h>
#include <kopetemetacontact.h>
#include <kopeteonlinestatus.h>
#include <kopetecontactlist.h>
#include <kopetecontact.h>
#include <kopetegroup.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kconfigbase.h>
#include <Q3Dict>
#include <kopetemessage.h>
#include <qprocess.h>

class SkypeAccountPrivate {
	public:
		///The skype protocol pointer
		SkypeProtocol *protocol;
		///ID of this account (means my skype name)
		QString ID;
		///The skype back-end
		Skype skype;
		///The hitchhike mode of incoming messages
		bool hitch;
		///The mark read messages mode
		bool markRead;
		///Search for unread messages on login?
		bool searchForUnread;
		///Do we show call control window?
		bool callControl;
		///Metacontact for all users that aren't in the list
		Kopete::MetaContact notInListUsers;
		///Constructor
		SkypeAccountPrivate(SkypeAccount &account) : skype(account) {};//just an empty constructor
		///Automatic close of call window when the call finishes (in seconds, 0 -> disabled)
		int callWindowTimeout;
		///Are the pings enabled?
		bool pings;
		///What bus are we using, session (0) or system (1)?
		int bus;
		///How long can I keep trying connect to newly started skype, before I give up (seconds)
		int launchTimeout;
		///By what command is the skype started?
		QString skypeCommand;
		///What is my name, by the way?
		QString myName;
		///Do we wait before connecting?
		int waitBeforeConnect;
		///List of chat all chat sessions
		Q3Dict<SkypeChatSession> sessions;
		///Last used chat session
		SkypeChatSession *lastSession;
		///List of the conference calls
		Q3Dict<SkypeConference> conferences;
		///List of existing calls
		Q3Dict<SkypeCallDialog> calls;
		///Shall chat window leave the chat whenit is closed
		bool leaveOnExit;
		///Executed before making the call
		QString startCallCommand;
		///Executed after finished the call
		QString endCallCommand;
		///Wait for the start call command to finitsh?
		bool waitForStartCallCommand;
		///Execute the end call command only if no other calls exists?
		bool endCallCommandOnlyLats;
		///How many calls are opened now?
		int callCount;
		///Command executed on incoming call
		QString incommingCommand;
};

SkypeAccount::SkypeAccount(SkypeProtocol *protocol, const QString& accountID) : Kopete::Account(protocol, accountID ) {
	kDebug() << k_funcinfo << endl;//some debug info

	//keep track of what accounts the protocol has
	protocol->registerAccount(this);

	//the d pointer
	d = new SkypeAccountPrivate(*this);
	d->calls.setAutoDelete(false);
	d->conferences.setAutoDelete(false);
	//remember the protocol, it will be needed
	d->protocol = protocol;

	//load the properties
	KConfigGroup *config = configGroup();
	author = config->readEntry("Authorization");//get the name how to authorize myself
	launchType = config->readEntry("Launch", 0);//launch the skype?
	setScanForUnread(config->readEntry("ScanForUnread", true));
	setCallControl(config->readEntry("CallControl", false));
	setPings(config->readEntry("Pings", true));
	setBus(config->readEntry("Bus", 1));
	setLaunchTimeout(config->readEntry("LaunchTimeout", 30));
	d->myName = config->readEntry("MyselfName", "Skype");
	setSkypeCommand(config->readEntry("SkypeCommand", "skype"));
	setWaitBeforeConnect(config->readEntry("WaitBeforeConnect", 0));
	setLeaveOnExit(config->readEntry("LeaveOnExit", false));
	setStartCallCommand(config->readEntry("StartCallCommand", ""));
	setEndCallCommand(config->readEntry("EndCallCommand", ""));
	setWaitForStartCallCommand(config->readEntry("WaitForStartCallCommand", false));
	setEndCallCommandOnlyForLast(config->readEntry("EndCallCommandOnlyLast", false));
	setIncomingCommand(config->readEntry("IncomingCall", ""));

	//create myself contact
	SkypeContact *_myself = new SkypeContact(this, "Skype", Kopete::ContactList::self()->myself(), false);
	setMyself(_myself);
	//and set default online status (means offline)
	myself()->setOnlineStatus(protocol->Offline);

	//Now, connect the signals
	QObject::connect(&d->skype, SIGNAL(wentOnline()), this, SLOT(wentOnline()));
	QObject::connect(&d->skype, SIGNAL(wentOffline()), this, SLOT(wentOffline()));
	QObject::connect(&d->skype, SIGNAL(wentAway()), this, SLOT(wentAway()));
	QObject::connect(&d->skype, SIGNAL(wentNotAvailable()), this, SLOT(wentNotAvailable()));
	QObject::connect(&d->skype, SIGNAL(wentDND()), this, SLOT(wentDND()));
	QObject::connect(&d->skype, SIGNAL(wentInvisible()), this, SLOT(wentInvisible()));
	QObject::connect(&d->skype, SIGNAL(wentSkypeMe()), this, SLOT(wentSkypeMe()));
	QObject::connect(&d->skype, SIGNAL(statusConnecting()), this, SLOT(statusConnecting()));
	QObject::connect(&d->skype, SIGNAL(newUser(const QString&, int)), this, SLOT(newUser(const QString&, int)));
	QObject::connect(&d->skype, SIGNAL(contactInfo(const QString&, const QString& )), this, SLOT(updateContactInfo(const QString&, const QString& )));
	QObject::connect(&d->skype, SIGNAL(receivedIM(const QString&, const QString&, const QString& )), this, SLOT(receivedIm(const QString&, const QString&, const QString& )));
	QObject::connect(&d->skype, SIGNAL(gotMessageId(const QString& )), this, SLOT(gotMessageId(const QString& )));//every time some ID is known inform the contacts
	QObject::connect(&d->skype, SIGNAL(newCall(const QString&, const QString&)), this, SLOT(newCall(const QString&, const QString&)));
	QObject::connect(&d->skype, SIGNAL(setMyselfName(const QString&)), this, SLOT(setMyselfName(const QString& )));
	QObject::connect(&d->skype, SIGNAL(receivedMultiIM(const QString&, const QString&, const QString&, const QString& )), this, SLOT(receiveMultiIm(const QString&, const QString&, const QString&, const QString& )));
	QObject::connect(&d->skype, SIGNAL(outgoingMessage(const QString&, const QString&)), this, SLOT(sentMessage(const QString&, const QString& )));
	QObject::connect(&d->skype, SIGNAL(groupCall(const QString&, const QString& )), this, SLOT(groupCall(const QString&, const QString& )));
	QObject::connect(Kopete::ContactList::self(), SIGNAL(groupRemoved (Kopete::Group *)), this, SLOT(deleteGroup (Kopete::Group *) ) );
	QObject::connect(Kopete::ContactList::self(), SIGNAL(groupRenamed (Kopete::Group *, const QString& )), this, SLOT(renameGroup (Kopete::Group *, const QString& )) );

	//Set SkypeActionHandler for web SkypeButtons
	new KopeteAdaptor(this);
	QDBusConnection::sessionBus().registerObject("/SkypeActionHandler", this);

	//set values for the connection (should be updated if changed)
	d->skype.setValues(launchType, author);
	setHitchHike(config->readEntry("Hitch", true));
	setMarkRead(config->readEntry("MarkRead", true));//read the modes of account
	d->callWindowTimeout = config->readEntry("CloseWindowTimeout", 4);
	setPings(config->readEntry("Pings", true));
	d->sessions.setAutoDelete(false);
	d->lastSession = 0L;
	d->callCount = 0;
}


SkypeAccount::~SkypeAccount() {
	kDebug() << k_funcinfo << endl;//some debug info

	save();

	d->protocol->unregisterAccount();//This account no longer exists

	//free memory
	delete d;
}

bool SkypeAccount::createContact(const QString &contactID, Kopete::MetaContact *parentContact) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (!contact(contactID)) {//check weather it is not used already
		SkypeContact *newContact = new SkypeContact(this, contactID, parentContact);//create the contact

		return newContact != 0L;//test weather it was created
	} else {
		kDebug() << k_funcinfo << "Contact already exists:" << contactID << endl;//Tell that it is not OK

		return false;
	}
}

void SkypeAccount::setAway(bool away, const QString &reason) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (away)
		setOnlineStatus(d->protocol->Away, reason);
	else
		setOnlineStatus(d->protocol->Online, reason);
}

void SkypeAccount::setOnlineStatus(const Kopete::OnlineStatus &status, const Kopete::StatusMessage &reason, const OnlineStatusOptions& options) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (status == d->protocol->Online)
		d->skype.setOnline();//Go online
	else if (status == d->protocol->Offline)
		d->skype.setOffline();//Go offline
	else if (status == d->protocol->Away)
		d->skype.setAway();
	else if (status == d->protocol->NotAvailable)
		d->skype.setNotAvailable();
	else if (status == d->protocol->DoNotDisturb)
		d->skype.setDND();
	else if (status == d->protocol->Invisible)
		d->skype.setInvisible();
	else if (status == d->protocol->SkypeMe)
		d->skype.setSkypeMe();
	else
		kDebug() << "Unknown online status" << endl;//Just a warning that I do not know that status

	///TODO: Port to kde4
	Q_UNUSED(reason);
	Q_UNUSED(options);
}

void SkypeAccount::setStatusMessage(const Kopete::StatusMessage &statusMessage)
{
	///TODO: Port to kde4
	Q_UNUSED(statusMessage);
}

void SkypeAccount::disconnect() {
	kDebug() << k_funcinfo << endl;//some debug info

	setOnlineStatus(d->protocol->Offline, Kopete::StatusMessage());
}

SkypeContact *SkypeAccount::contact(const QString &id) {
	kDebug() << k_funcinfo << endl;//some debug info

	return static_cast<SkypeContact *>(contacts().value(id));//get the contact and convert it into the skype contact, there are no other contacts anyway
}

void SkypeAccount::connect(const Kopete::OnlineStatus &Status) {
	kDebug() << k_funcinfo << endl;//some debug info

	if ((Status != d->protocol->Online) && (Status != d->protocol->Away) &&
		(Status != d->protocol->NotAvailable) && (Status != d->protocol->DoNotDisturb) &&
		(Status != d->protocol->SkypeMe))//some strange online status, taje a default one
			setOnlineStatus(d->protocol->Online, Kopete::StatusMessage());
	else
		setOnlineStatus(Status, Kopete::StatusMessage());//just change the status

}

void SkypeAccount::save() {
	kDebug() << k_funcinfo << endl;//some debug info

	KConfigGroup *config = configGroup();//get the config
	config->writeEntry("Authorization", author);//write the authorization name
	config->writeEntry("Launch", launchType);//and the launch type
	config->writeEntry("Hitch", getHitchHike());//save the hitch hike messages mode
	config->writeEntry("MarkRead", getMarkRead());//save the Mark read messages mode
	config->writeEntry("ScanForUnread", getScanForUnread());
	config->writeEntry("CallControl", getCallControl());
	config->writeEntry("CloseWindowTimeout", d->callWindowTimeout);
	config->writeEntry("Pings", getPings());
	config->writeEntry("Bus", getBus());
	config->writeEntry("LaunchTimeout", getLaunchTimeout());
	config->writeEntry("SkypeCommand", getSkypeCommand());
	config->writeEntry("MyselfName", d->myName);
	config->writeEntry("WaitBeforeConnect", getWaitBeforeConnect());
	config->writeEntry("LeaveOnExit", leaveOnExit());
	config->writeEntry("StartCallCommand", startCallCommand());
	config->writeEntry("EndCallCommand", endCallCommand());
	config->writeEntry("WaitForStartCallCommand", waitForStartCallCommand());
	config->writeEntry("EndCallCommandOnlyLast", endCallCommandOnlyLast());
	config->writeEntry("IncomingCall", incomingCommand());

	//save it into the skype connection as well
	d->skype.setValues(launchType, author);
}

void SkypeAccount::wentOnline() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->Online);//just set the icon
	d->skype.enablePings(d->pings);
	emit connectionStatus(true);
}

void SkypeAccount::wentOffline() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->Offline);//just change the icon
	emit connectionStatus(false);
}

void SkypeAccount::wentAway() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->Away);//just change the icon
	emit connectionStatus(true);
}

void SkypeAccount::wentNotAvailable() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->NotAvailable);
	emit connectionStatus(true);
}

void SkypeAccount::wentDND() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->DoNotDisturb);
	emit connectionStatus(true);
}

void SkypeAccount::wentInvisible() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->Invisible);
	emit connectionStatus(true);
}

void SkypeAccount::wentSkypeMe() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->SkypeMe);
	emit connectionStatus(true);
}

void SkypeAccount::statusConnecting() {
	kDebug() << k_funcinfo << endl;//some debug info

	myself()->setOnlineStatus(d->protocol->Connecting);
	emit connectionStatus(false);
}

void SkypeAccount::newUser(const QString &name, int groupID) {
	kDebug() << k_funcinfo << QString("name = %1").arg(name) << QString("groupID = %1").arg(groupID) << endl;//some debug info

	if (name == "echo123")// echo123 - Make Test Call has moved to Skype protocol toolbar
		return;

	QString group = d->skype.getGroupName(groupID);

	Kopete::Group * skypeGroup;

	if (group.isEmpty() || groupID == -1) // If skype group hasnt name, in kopete will be in top
		skypeGroup = Kopete::Group::topLevel();
	else {
		skypeGroup = Kopete::ContactList::self()->findGroup(group); //get kopete group by skype group name. If skype group in kopete doesnt exist, create it automatically
		if ( skypeGroup == Kopete::Group::topLevel() ){ //if group in skype has name i18n("Top Level") kopete get top level group, but in skype top level group is group without name
			QList <Kopete::Group *> groups = Kopete::ContactList::self()->groups(); //get all groups
			bool found = false;
			for (QList <Kopete::Group *>::iterator it = groups.begin(); it != groups.end(); ++it ){ //search all groups, if one isnt top level and has skype group name
				if ( (*it)->displayName() == group && (*it) != Kopete::Group::topLevel() ){
					skypeGroup = (*it);
					found = true; //if found skip creating new
				}
			}
			if ( !found ){
				skypeGroup = new Kopete::Group(group); //create new group with name
				Kopete::ContactList::self()->addGroup(skypeGroup); //add this new group to contact list
			}
		}
	}

	Kopete::Contact * contact = contacts().value(name);
	if (contact){
		if (skypeGroup != contact->metaContact()->groups().first()){ // if skype Group is different like kopete group, move metacontact to skype group
			kDebug() << "Moving contact" << name << "to group" << group << endl;
			contact->metaContact()->moveToGroup(contact->metaContact()->groups().first(), skypeGroup);
		}
		return;
	}

	addContact(name, d->skype.getDisplayName(name), skypeGroup, ChangeKABC);
}

void SkypeAccount::prepareContact(SkypeContact *contact) {
	kDebug() << k_funcinfo << endl;//some debug info

	QObject::connect(&d->skype, SIGNAL(updateAllContacts()), contact, SLOT(requestInfo()));//all contacts will know that
	QObject::connect(contact, SIGNAL(infoRequest(const QString& )), &d->skype, SLOT(getContactInfo(const QString& )));//How do we ask for info?
	QObject::connect(this, SIGNAL(connectionStatus(bool )), contact, SLOT(connectionStatus(bool )));
	QObject::connect(contact, SIGNAL(setActionsPossible(bool )), d->protocol, SLOT(updateCallActionStatus()));
}

void SkypeAccount::updateContactInfo(const QString &contact, const QString &change) {
	kDebug() << k_funcinfo << endl;//some debug info

	SkypeContact *cont = static_cast<SkypeContact *> (contacts().value(contact));//get the contact
	if (cont)
		cont->setInfo(change);//give it the message
	else {//it does not yet exist, create it if it is in skype contact list (can be got by buddystatus)
		const QString &type = change.section(' ', 0, 0).trimmed().toUpper();//get the first part of the message, it should be BUDDYSTATUS
		const QString &value = change.section(' ', 1, 1).trimmed();//get the second part if it is some reasonable value
		if ((type == "BUDDYSTATUS") && ((value == "2") || (value == "3"))) {//the user is in skype contact list
			newUser(contact, d->skype.getContactGroupID(contact));
		} else if (type != "BUDDYSTATUS")//this is some other info
			d->skype.getContactBuddy(contact);//get the buddy status for the account and check, if it is in contact list or not
	}
}

bool SkypeAccount::canComunicate() {
	return d->skype.canComunicate();
}

SkypeProtocol * SkypeAccount::protocol() {
	return d->protocol;
}

void SkypeAccount::sendMessage(Kopete::Message &message, const QString &chat) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (chat.isEmpty()) {
		const QString &user = message.to().at(0)->contactId();//get id of the first contact, messages to multiple people are not yet possible
		const QString &body = message.plainBody().trimmed();//get the text of the message

		d->skype.send(user, body);//send it by skype
	} else {
		const QString &body = message.plainBody().trimmed();

		d->skype.sendToChat(chat, body);
	}
}

bool SkypeAccount::getHitchHike() const {
	return d->hitch;
}

bool SkypeAccount::getMarkRead() const {
	return d->markRead;
}

void SkypeAccount::setHitchHike(bool value) {
	d->hitch = value;//save it
	d->skype.setHitchMode(value);//set it in the skype
}

void SkypeAccount::setMarkRead(bool value) {
	d->markRead = value;//remember it
	d->skype.setMarkMode(value);
}

bool SkypeAccount::userHasChat(const QString &userId) {
	SkypeContact *cont = static_cast<SkypeContact *> (contacts().value(userId));//get the contact

	if (cont)//it exists
		return cont->hasChat();//so ask it
	else
		return false;//if it does not exist it can not have a chat opened
}

void SkypeAccount::receivedIm(const QString &user, const QString &message, const QString &messageId) {
	kDebug() << k_funcinfo << "User: " << user << ", message: " << message << endl;//some debug info
	getContact(user)->receiveIm(message, getMessageChat(messageId));//let the contact show the message
}

void SkypeAccount::setScanForUnread(bool value) {
	d->searchForUnread = value;
	d->skype.setScanForUnread(value);
}

bool SkypeAccount::getScanForUnread() const {
	return d->searchForUnread;
}

void SkypeAccount::makeCall(SkypeContact *user) {
	makeCall(user->contactId());
}

void SkypeAccount::makeCall(const QString &users) {
	startCall();
	d->skype.makeCall(users);
}

void SkypeAccount::makeTestCall() {
	makeCall("echo123");
}

bool SkypeAccount::getCallControl() const {
	return d->callControl;
}

void SkypeAccount::setCallControl(bool value) {
	d->callControl = value;
}

void SkypeAccount::newCall(const QString &callId, const QString &userId) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (d->callControl) {//Show the skype call control window
		SkypeCallDialog *dialog = new SkypeCallDialog(callId, userId, this);//It should free itself when it is closed

		QObject::connect(&d->skype, SIGNAL(callStatus(const QString&, const QString& )), dialog, SLOT(updateStatus(const QString&, const QString& )));
		QObject::connect(dialog, SIGNAL(acceptTheCall(const QString& )), &d->skype, SLOT(acceptCall(const QString& )));
		QObject::connect(dialog, SIGNAL(hangTheCall(const QString& )), &d->skype, SLOT(hangUp(const QString& )));
		QObject::connect(dialog, SIGNAL(toggleHoldCall(const QString& )), &d->skype, SLOT(toggleHoldCall(const QString& )));
		QObject::connect(&d->skype, SIGNAL(callError(const QString&, const QString& )), dialog, SLOT(updateError(const QString&, const QString& )));
		QObject::connect(&d->skype, SIGNAL(skypeOutInfo(int, const QString& )), dialog, SLOT(skypeOutInfo(int, const QString& )));
		QObject::connect(dialog, SIGNAL(updateSkypeOut()), &d->skype, SLOT(getSkypeOut()));
		QObject::connect(dialog, SIGNAL(callFinished(const QString& )), this, SLOT(removeCall(const QString& )));

		dialog->show();//Show Call dialog

		d->skype.getSkypeOut();
		d->calls.insert(callId, dialog);
	}

	if ((!d->incommingCommand.isEmpty()) && (d->skype.isCallIncoming(callId))) {
		kDebug() << "Running ring command" << endl;
		QProcess *proc = new QProcess();
		QStringList args = d->incommingCommand.split(' ');
		QString bin = args.takeFirst();
		proc->start(bin, args);
	}
}

bool SkypeAccount::isCallIncoming(const QString &callId) {
	kDebug() << k_funcinfo << endl;//some debug info

	return d->skype.isCallIncoming(callId);
}

void SkypeAccount::setCloseWindowTimeout(int timeout) {
	d->callWindowTimeout = timeout;
}

int SkypeAccount::closeCallWindowTimeout() const {
	return d->callWindowTimeout;
}

QString SkypeAccount::getUserLabel(const QString &userId) {
	kDebug() << k_funcinfo << endl;//some debug info

	if (userId.indexOf(' ') != -1) {//there are more people than just one
		QStringList users = userId.split(' ');
		for (QStringList::iterator it = users.begin(); it != users.end(); ++it) {
			(*it) = getUserLabel((*it).trimmed());
		}
		return users.join("\n");
	}

	Kopete::Contact *cont = contact(userId);

	if (!cont) {
		addContact(userId, QString::null, 0L, Temporary);//create a temporary contact

		cont = (contacts().value(userId));//It should be there now
		if (!cont)
			return userId;//something odd,.but better do nothing than crash
	}

	return QString("%1 (%2)").arg(cont->nickName()).arg(userId);
}

void SkypeAccount::setPings(bool enabled) {
	d->skype.enablePings(enabled);
	d->pings = enabled;
}

bool SkypeAccount::getPings() const {
	return d->pings;
}

int SkypeAccount::getBus() const {
	return d->bus;
}

void SkypeAccount::setBus(int bus) {
	d->bus = bus;
	d->skype.setBus(bus);
}

void SkypeAccount::setLaunchTimeout(int seconds) {
	d->launchTimeout = seconds;
	d->skype.setLaunchTimeout(seconds);
}

int SkypeAccount::getLaunchTimeout() const {
	return d->launchTimeout;
}

void SkypeAccount::setSkypeCommand(const QString &command) {
	d->skypeCommand = command;
	d->skype.setSkypeCommand(command);
}

const QString &SkypeAccount::getSkypeCommand() const {
	return d->skypeCommand;
}

void SkypeAccount::setMyselfName(const QString &name) {
	d->myName = name;
	myself()->setNickName(name);
}

void SkypeAccount::setWaitBeforeConnect(int value) {
	d->waitBeforeConnect = value;
	d->skype.setWaitConnect(value);
}

int SkypeAccount::getWaitBeforeConnect() const {
	return d->waitBeforeConnect;
}

SkypeContact *SkypeAccount::getContact(const QString &userId) {
	SkypeContact *cont = static_cast<SkypeContact *> (contacts().value(userId));//get the contact
	if (!cont) {//We do not know such contact
		addContact(userId, QString::null, 0L, Temporary);//create a temporary contact

		cont = static_cast<SkypeContact *> (contacts().value(userId));//It should be there now
	}
	return cont;
}

void SkypeAccount::prepareChatSession(SkypeChatSession *session) {
	QObject::connect(session, SIGNAL(updateChatId(const QString&, const QString&, SkypeChatSession* )), this, SLOT(setChatId(const QString&, const QString&, SkypeChatSession* )));
	QObject::connect(session, SIGNAL(wantTopic(const QString& )), &d->skype, SLOT(getTopic(const QString& )));
	QObject::connect(&d->skype, SIGNAL(joinUser(const QString&, const QString& )), session, SLOT(joinUser(const QString&, const QString& )));
	QObject::connect(&d->skype, SIGNAL(leftUser(const QString&, const QString&, const QString& )), session, SLOT(leftUser(const QString&, const QString&, const QString& )));
	QObject::connect(&d->skype, SIGNAL(setTopic(const QString&, const QString& )), session, SLOT(setTopic(const QString&, const QString& )));
	QObject::connect(session, SIGNAL(inviteUserToChat(const QString&, const QString& )), &d->skype, SLOT(inviteUser(const QString&, const QString& )));
	QObject::connect(session, SIGNAL(leaveChat(const QString& )), &d->skype, SLOT(leaveChat(const QString& )));
}

void SkypeAccount::setChatId(const QString &oldId, const QString &newId, SkypeChatSession *sender) {
	d->sessions.remove(oldId);//remove the old one
	if (!newId.isEmpty()) {
		d->sessions.insert(newId, sender);
	}
}

bool SkypeAccount::chatExists(const QString &chat) {
	return d->sessions.find(chat);
}

void SkypeAccount::receiveMultiIm(const QString &chatId, const QString &body, const QString &messageId, const QString &user) {
	SkypeChatSession *session = d->sessions.find(chatId);

	if (!session) {
		QStringList users = d->skype.getChatUsers(chatId);
		Kopete::ContactPtrList list;
		for (QStringList::iterator it = users.begin(); it != users.end(); ++it) {
			list.append(getContact(*it));
		}

		session = new SkypeChatSession(this, chatId, list);
	}

	Kopete::Message mes(getContact(user), myself());
	mes.setDirection(Kopete::Message::Inbound);
	mes.setPlainBody(body);
	session->appendMessage(mes);

	Q_UNUSED(messageId);
}

QString SkypeAccount::getMessageChat(const QString &messageId) {
	return d->skype.getMessageChat(messageId);
}

void SkypeAccount::registerLastSession(SkypeChatSession *lastSession) {
	d->lastSession = lastSession;
}

void SkypeAccount::gotMessageId(const QString &messageId) {
	if ((d->lastSession) && (!messageId.isEmpty())) {
		d->lastSession->setChatId(d->skype.getMessageChat(messageId));
	}

	d->lastSession = 0L;
}

void SkypeAccount::sentMessage(const QString &body, const QString &chat) {
	kDebug() << k_funcinfo << "chat: " << chat << endl;//some debug info

	SkypeChatSession *session = d->sessions.find(chat);
	const QStringList &users = d->skype.getChatUsers(chat);
	QList<Kopete::Contact*> *recv = 0L;

	if (!session) {
		if (d->hitch) {
			recv = constructContactList(users);
			if (recv->count() == 1) {
				SkypeContact *cont = static_cast<SkypeContact *> (recv->at(0));
				cont->startChat();
				session = cont->getChatSession();
				session->setChatId(chat);
			} else {
				session = new SkypeChatSession(this, chat, *recv);
			}
		} else {
			return;
		}
	}

	if (!recv)
		recv = constructContactList(users);

	session->sentMessage(recv, body);
	delete recv;
}

QList<Kopete::Contact*> *SkypeAccount::constructContactList(const QStringList &users) {
	QList<Kopete::Contact*> *list= new QList<Kopete::Contact*> ();
	for (QStringList::const_iterator it = users.begin(); it != users.end(); ++it) {
		list->append(getContact(*it));
	}

	return list;
}

void SkypeAccount::groupCall(const QString &callId, const QString &groupId) {
	kDebug() << k_funcinfo << endl;

	//TODO: Find out a way to embet qdialog into another one after creation
	return;

	if (!d->callControl)
		return;

	SkypeConference *conf;
	if (!(conf = d->conferences[groupId])) {//does it already exist?
		conf = new SkypeConference(groupId);//no, create one then..
		d->conferences.insert(groupId, conf);

		QObject::connect(conf, SIGNAL(removeConference(const QString& )), this, SLOT(removeCallGroup(const QString& )));
	}

	conf->embedCall(d->calls[callId]);
}

void SkypeAccount::removeCall(const QString &callId) {
	kDebug() << k_funcinfo << endl;
	d->calls.remove(callId);
}

void SkypeAccount::removeCallGroup(const QString &groupId) {
	kDebug() << k_funcinfo << endl;
	d->conferences.remove(groupId);
}

QString SkypeAccount::createChat(const QString &users) {
	return d->skype.createChat(users);
}

bool SkypeAccount::leaveOnExit() const {
	return d->leaveOnExit;
}

void SkypeAccount::setLeaveOnExit(bool value) {
	d->leaveOnExit = value;
}

void SkypeAccount::chatUser(const QString &userId) {
	SkypeContact *contact = getContact(userId);

	contact->execute();
}

void SkypeAccount::setStartCallCommand(const QString &value) {
	d->startCallCommand = value;
}

void SkypeAccount::setEndCallCommand(const QString &value) {
	d->endCallCommand = value;
}

void SkypeAccount::setWaitForStartCallCommand(bool value) {
	d->waitForStartCallCommand = value;
}
void SkypeAccount::setEndCallCommandOnlyForLast(bool value) {
	d->endCallCommandOnlyLats = value;
}

QString SkypeAccount::startCallCommand() const {
	return d->startCallCommand;
}

QString SkypeAccount::endCallCommand() const {
	return d->endCallCommand;
}

bool SkypeAccount::waitForStartCallCommand() const {
	return d->waitForStartCallCommand;
}

bool SkypeAccount::endCallCommandOnlyLast() const {
	return d->endCallCommandOnlyLats;
}

void SkypeAccount::startCall() {
	kDebug() << k_funcinfo << endl;

	QProcess *proc = new QProcess();
	QStringList args = d->startCallCommand.split(' ');
	QString bin = args.takeFirst();
	if (d->waitForStartCallCommand)
		proc->execute(bin, args);
	else
		proc->start(bin, args);
	++d->callCount;
}

void SkypeAccount::endCall() {
	kDebug() << k_funcinfo << endl;

	if ((--d->callCount == 0) || (!d->endCallCommandOnlyLats)) {
		QProcess *proc = new QProcess();
		QStringList args = d->endCallCommand.split(' ');
		QString bin = args.takeFirst();
		proc->start(bin, args);
	}
	if (d->callCount < 0)
		d->callCount = 0;
}

void SkypeAccount::setIncomingCommand(const QString &command) {
	d->incommingCommand = command;
}

QString SkypeAccount::incomingCommand() const {
	return d->incommingCommand;
}

void SkypeAccount::registerContact(const QString &contactId) {
	kDebug() << k_funcinfo << endl;
	d -> skype.addContact(contactId);
}

void SkypeAccount::removeContact(const QString &contactId) {
	d -> skype.removeContact(contactId);
}

bool SkypeAccount::ableMultiCall() {
	return (d->skype.ableConference());
}

bool SkypeAccount::canAlterAuth() {
	return (d->skype.canComunicate());
}

void SkypeAccount::authorizeUser(const QString &userId) {
	d->skype.setAuthor(userId, Skype::Author);
}

void SkypeAccount::disAuthorUser(const QString &userId) {
	d->skype.setAuthor(userId, Skype::Deny);
}

void SkypeAccount::blockUser(const QString &userId) {
	d->skype.setAuthor(userId, Skype::Block);
}

int SkypeAccount::getAuthor(const QString &contactId) {
	switch (d->skype.getAuthor(contactId)) {
		case Skype::Author:
			return 0;
		case Skype::Deny:
			return 1;
		case Skype::Block:
			return 2;
	}
	return 0;
}

bool SkypeAccount::hasCustomStatusMenu() const {
	kDebug() << k_funcinfo << endl;//some debug info
	return true;
}

void SkypeAccount::fillActionMenu( KActionMenu *actionMenu ) {
	kDebug() << k_funcinfo << endl;//some debug info

	actionMenu->setIcon( myself()->onlineStatus().iconFor(this) );
	actionMenu->menu()->addTitle( QIcon(myself()->onlineStatus().iconFor(this)), i18n("Skype (%1)", accountId()));

	if (d->protocol)
	{
		KAction *setOnline = new KAction( KIcon(QIcon(d->protocol->Online.iconFor(this))), i18n("Online"), this );
		QObject::connect( setOnline, SIGNAL(triggered(bool)), &d->skype, SLOT(setOnline()) );
		actionMenu->addAction(setOnline);

		KAction *setOffline = new KAction( KIcon(QIcon(d->protocol->Offline.iconFor(this))), i18n("Offline"), this );
		QObject::connect( setOffline, SIGNAL(triggered(bool)), &d->skype, SLOT(setOffline()) );
		actionMenu->addAction(setOffline);

		KAction *setAway = new KAction( KIcon(QIcon(d->protocol->Away.iconFor(this))), i18n("Away"), this );
		QObject::connect( setAway, SIGNAL(triggered(bool)), &d->skype, SLOT(setAway()) );
		actionMenu->addAction(setAway);

		KAction *setNotAvailable = new KAction( KIcon(QIcon(d->protocol->NotAvailable.iconFor(this))), i18n("Not Available"), this );
		QObject::connect( setNotAvailable, SIGNAL(triggered(bool)), &d->skype, SLOT(setNotAvailable()) );
		actionMenu->addAction(setNotAvailable);

		KAction *setDND = new KAction( KIcon(QIcon(d->protocol->DoNotDisturb.iconFor(this))), i18n("Do Not Disturb"), this );
		QObject::connect( setDND, SIGNAL(triggered(bool)), &d->skype, SLOT(setDND()) );
		actionMenu->addAction(setDND);

		KAction *setInvisible = new KAction( KIcon(QIcon(d->protocol->Invisible.iconFor(this))), i18n("Invisible"), this );
		QObject::connect( setInvisible, SIGNAL(triggered(bool)), &d->skype, SLOT(setInvisible()) );
		actionMenu->addAction(setInvisible);

		KAction *setSkypeMe = new KAction( KIcon(QIcon(d->protocol->SkypeMe.iconFor(this))), i18n("Skype Me"), this );
		QObject::connect( setSkypeMe, SIGNAL(triggered(bool)), &d->skype, SLOT(setSkypeMe()) );
		actionMenu->addAction(setSkypeMe);

		actionMenu->addSeparator();

		KAction *makeTestCall = new KAction( KIcon("skype_call"), i18n("Make Test Call"), this );
		QObject::connect( makeTestCall, SIGNAL(triggered(bool)), this, SLOT(makeTestCall()) );

		const Kopete::OnlineStatus &myStatus = (myself()) ? myself()->onlineStatus() : protocol()->Offline;
		if (myStatus == protocol()->Offline || myStatus == protocol()->Connecting){
			makeTestCall->setEnabled(false);
		}

		actionMenu->addAction(makeTestCall);

		actionMenu->addSeparator();

		KAction *properties = new KAction( i18n("Properties"), this );
		QObject::connect( properties, SIGNAL(triggered(bool)), this, SLOT(editAccount()) );
		actionMenu->addAction(properties);
	}
}

QString SkypeAccount::getMyselfSkypeName() {
	kDebug() << k_funcinfo << endl;//some debug info
	return d->skype.getMyself();
}

void SkypeAccount::MovedBetweenGroup(SkypeContact *contact) {
	kDebug() << k_funcinfo << endl;//some debug info

	int newGroup = d->skype.getGroupID(contact->metaContact()->groups().first()->displayName());
	int oldGroup = d->skype.getContactGroupID(contact->contactId());

	kDebug() << "oldGroup:" << oldGroup << "newGroup:" << newGroup << endl;

	if ( oldGroup != -1 ){
		kDebug() << "Removing contact" << contact->contactId() << "from group" << d->skype.getContactGroupID(contact->contactId()) << endl;
		d->skype.removeFromGroup(contact->contactId(), oldGroup);
	}

	if ( newGroup == -1 ){
		if ( contact->metaContact()->groups().first() != Kopete::Group::topLevel() ){
			d->skype.createGroup(contact->metaContact()->groups().first()->displayName());
			newGroup = d->skype.getGroupID(contact->metaContact()->groups().first()->displayName());
		} else {
			kDebug() << "Contact is in top level, so in no skype group, skipping" << endl;
			return;
		}
	}

	if ( newGroup != -1 ){
		kDebug() << "Adding contact" << contact->contactId() << "to group" << d->skype.getGroupID(contact->metaContact()->groups().first()->displayName()) << endl;
		d->skype.addToGroup(contact->contactId(), newGroup);
	} else
		kDebug() << "Error: Cant create new skype group" << contact->metaContact()->groups().first()->displayName() << endl;
}

void SkypeAccount::deleteGroup (Kopete::Group * group){
	kDebug() << k_funcinfo << group->displayName() << endl;//some debug info
	int groupID = d->skype.getGroupID( group->displayName() );
	if ( groupID != -1 )
		d->skype.deleteGroup(groupID);
	else
		kDebug() << "Group" << group->displayName() << "in skype doesnt exist, skipping" << endl;
}

void SkypeAccount::renameGroup (Kopete::Group * group, const QString &oldname){
	kDebug() << k_funcinfo << "Renaming skype group" << oldname << "to" << group->displayName() << endl;//some debug info
	int groupID = d->skype.getGroupID( oldname );
	if ( groupID != -1 )
		d->skype.renameGroup( groupID, group->displayName() );
	else
		kDebug() << "Old group" << oldname << "in skype doesnt exist, skipping" << endl;
}

void SkypeAccount::openFileTransfer(const QString &user, const QString &url) {
	kDebug() << k_funcinfo << user << url << endl;
	d->skype.openFileTransfer(user, url);
}

void SkypeAccount::SkypeActionHandler(const QString &message) {
	kDebug() << message;

	if ( message.isEmpty() )
		return;//Empty message

	QString command;
	QString user;

	if ( message.startsWith("callto:", Qt::CaseInsensitive) ) {
		command = "call";
		user = message.section(":", -1).section("/", -1).trimmed();
	} else if ( message.startsWith("tel:", Qt::CaseInsensitive) ) {
		command = "chat";
		user = message.section(":", -1).section("/", -1).trimmed();
	} else if ( message.startsWith("skype:", Qt::CaseInsensitive) ) {
		command = message.section("?", -1).trimmed();
		user = message.section(":", -1).section("?", 0, 0).trimmed();
	} else
		return;//Unknow message

	if ( command.isEmpty() || user.isEmpty() )
		return;//Unknow message

	kDebug() << "user:" << user << "command:" << command;

	if ( command == "add" ) {
		//TODO: Open add user dialog
	} else if ( command == "call" ) {
		makeCall(user);//Start call with user
	} else if ( command == "chat" ) {
		chatUser(user);//Open chat window
	} else if ( command == "sendfile" ) {
		openFileTransfer(user);//Open file transfer dialog
	} else if ( command == "userinfo" ) {//TODO: Open option dialog (with all thisa options instead userinfo) and support unknow contacts who arent in contact list
		if ( ! contact(user) ) {
			addContact(user, QString::null, 0L, Temporary);//create a temporary contact
			if ( ! contact(user) )
				return;//contact arent in contact list - skip it
		}
		contact(user)->slotUserInfo();//Open user info dialog
		//TODO: dont use slot, it freeze dbus, better create new signal
	} else {
		kDebug() << "Unknow command";
		return;//Unknow command
	}
}

#include "skypeaccount.moc"