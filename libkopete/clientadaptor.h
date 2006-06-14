/*
 * This file was generated by dbusidl2cpp version 0.5
 * when processing input file org.kde.kopete.Client.xml
 *
 * dbusidl2cpp is Copyright (C) 2006 Trolltech AS. All rights reserved.
 *
 * This is an auto-generated file.
 */

#ifndef CLIENTADAPTOR_H_214701150286725
#define CLIENTADAPTOR_H_214701150286725

#include <QtCore/QObject>
#include <dbus/qdbus.h>
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;

/*
 * Adaptor class for interface org.kde.kopete.Client
 */
class ClientAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kopete.Client")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"org.kde.kopete.Client\" >\n"
"    <method name=\"networks\" >\n"
"      <arg direction=\"out\" type=\"as\" />\n"
"    </method>\n"
"    <method name=\"status\" >\n"
"      <arg direction=\"in\" type=\"s\" name=\"host\" />\n"
"      <arg direction=\"out\" type=\"i\" />\n"
"    </method>\n"
"    <method name=\"request\" >\n"
"      <arg direction=\"in\" type=\"s\" name=\"host\" />\n"
"      <arg direction=\"in\" type=\"b\" name=\"userInitiated\" />\n"
"      <arg direction=\"out\" type=\"i\" />\n"
"    </method>\n"
"    <method name=\"reportFailure\" >\n"
"      <arg direction=\"in\" type=\"s\" name=\"host\" />\n"
"      <arg direction=\"out\" type=\"b\" />\n"
"    </method>\n"
"    <signal name=\"statusChange\" >\n"
"      <arg type=\"s\" />\n"
"      <arg type=\"i\" />\n"
"    </signal>\n"
"    <signal name=\"shutdownRequested\" >\n"
"      <arg type=\"s\" />\n"
"    </signal>\n"
"  </interface>\n"
        "")
public:
    ClientAdaptor(QObject *parent);
    virtual ~ClientAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    QStringList networks();
    bool reportFailure(const QString &host);
    int request(const QString &host, bool userInitiated);
    int status(const QString &host);
Q_SIGNALS: // SIGNALS
    void shutdownRequested(const QString &in0);
    void statusChange(const QString &in0, int in1);
};

#endif
