/*  This file is part of the KDE project
    Copyright (C) 2003 Matthias Kretz <kretz@kde.org>

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

#include "kplugininfo.h"
#include <ksimpleconfig.h>
#include <ktrader.h>
#include <kdebug.h>
#include <kconfigbase.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kservice.h>

class KPluginInfo::KPluginInfoPrivate
{
    public:
        KPluginInfoPrivate()
            : hidden( false )
            , enabledbydefault( false )
            , pluginenabled( false )
            , config( 0 )
            {}

        ~KPluginInfoPrivate()
        {
            delete config;
        }

        QString specfile; // the filename of the file containing all the info
        QString name;
        QString comment;
        QString icon;
        QString author;
        QString email;
        QString pluginname; // the name attribute in the .rc file
        QString version;
        QString website; // URL to the website of the plugin/author
        QString category;
        QString license;
        QStringList dependencies;

        bool hidden;
        bool enabledbydefault;
        bool pluginenabled;

        KConfig * config;
        QString configgroup;
        KService::Ptr service;
        QValueList<KService::Ptr> kcmservices;
};

KPluginInfo::KPluginInfo( const QString & filename, const char* resource )
: d( new KPluginInfoPrivate )
{
    KConfig file( filename, true, true, resource );

    d->specfile = filename;

    if( filename.endsWith( QString::fromAscii( ".desktop" ) ) )
    {
        file.setDesktopGroup();
        d->hidden = file.readBoolEntry( "Hidden", false );
        if( d->hidden )
            return;

        d->name = file.readEntry( "Name" );
        d->comment = file.readEntry( "Comment" );
        d->icon = file.readEntry( "Icon" );
        d->author = file.readEntry( "X-KDE-PluginInfo-Author" );
        d->email = file.readEntry( "X-KDE-PluginInfo-Email" );
        d->pluginname = file.readEntry( "X-KDE-PluginInfo-Name" );
        d->version = file.readEntry( "X-KDE-PluginInfo-Version" );
        d->website = file.readEntry( "X-KDE-PluginInfo-Website" );
        d->category = file.readEntry( "X-KDE-PluginInfo-Category" );
        d->license = file.readEntry( "X-KDE-PluginInfo-License" );
        d->dependencies = file.readListEntry( "X-KDE-PluginInfo-Depends" );
        d->enabledbydefault = file.readBoolEntry(
                "X-KDE-PluginInfo-EnabledByDefault", false );
    }
    else if( filename.endsWith( QString::fromAscii( ".plugin" ) ) )
    { // provided for noatun style .plugin files compatibility

        d->name = file.readEntry( "Name" );
        d->comment = file.readEntry( "Comment" );
        d->icon = file.readEntry( "Icon" );
        d->author = file.readEntry( "Author" );
        d->email = file.readEntry( "Email" );
        d->pluginname = file.readPathEntry( "Filename" );
        // no version
        d->website = file.readEntry( "Site" );
        d->category = file.readEntry( "Type" );
        d->license = file.readEntry( "License" );
        d->dependencies = file.readListEntry( "Require" );
    }
    d->kcmservices = KTrader::self()->query( "KCModule", "'" + d->pluginname +
            "' in [X-KDE-ParentComponents]" );
    kdDebug( 703 ) << "found " << d->kcmservices.count() << " offers for " <<
        d->pluginname << endl;
}

KPluginInfo::KPluginInfo( const KService::Ptr service )
: d( new KPluginInfoPrivate )
{
    d->service = service;
    d->specfile = service->desktopEntryPath();

    QVariant tmp = service->property( "Hidden" );
    if( tmp.isValid() && tmp.toBool() == true )
    {
        d->hidden = true;
        return;
    }

    d->name = service->name();
    d->comment = service->comment();
    d->icon = service->icon();
    d->author = service->property( "X-KDE-PluginInfo-Author" ).toString();
    d->email = service->property( "X-KDE-PluginInfo-Email" ).toString();
    d->pluginname = service->property( "X-KDE-PluginInfo-Name" ).toString();
    d->version = service->property( "X-KDE-PluginInfo-Version" ).toString();
    d->website = service->property( "X-KDE-PluginInfo-Website" ).toString();
    d->category = service->property( "X-KDE-PluginInfo-Category" ).toString();
    d->license = service->property( "X-KDE-PluginInfo-License" ).toString();
    d->dependencies =
        service->property( "X-KDE-PluginInfo-Depends" ).toStringList();
    tmp = service->property( "X-KDE-PluginInfo-EnabledByDefault" );
    d->enabledbydefault = tmp.isValid() ? tmp.toBool() : false;
    d->kcmservices = KTrader::self()->query( "KCModule", "'" + d->pluginname +
            "' in [X-KDE-ParentComponents]" );
}

//X KPluginInfo::KPluginInfo()
//X : d( new KPluginInfoPrivate )
//X {
//X     d->hidden = true;
//X }

KPluginInfo::~KPluginInfo()
{
    delete d;
}

QValueList<KPluginInfo*> KPluginInfo::fromServices( const KService::List & services, KConfig * config, const QString & group )
{
    QValueList<KPluginInfo*> infolist;
    KPluginInfo * info;
    for( KService::List::ConstIterator it = services.begin();
            it != services.end(); ++it )
    {
        info = new KPluginInfo( *it );
        info->setConfig( config, group );
        infolist += info;
    }
    return infolist;
}

QValueList<KPluginInfo*> KPluginInfo::fromFiles( const QStringList & files, KConfig * config, const QString & group )
{
    QValueList<KPluginInfo*> infolist;
    for( QStringList::ConstIterator it = files.begin(); it != files.end(); ++it )
    {
        KPluginInfo * info = new KPluginInfo( *it );
        info->setConfig( config, group );
        infolist += info;
    }
    return infolist;
}

QValueList<KPluginInfo*> KPluginInfo::fromKPartsInstanceName( const QString & name, KConfig * config, const QString & group )
{
    QStringList files = KGlobal::dirs()->findAllResources( "data", name +
            "/kpartplugins/*.desktop", true, false );
    return fromFiles( files, config, group );
}

bool KPluginInfo::isHidden() const
{
    return d->hidden;
}

void KPluginInfo::setPluginEnabled( bool enabled )
{
    kdDebug( 703 ) << k_funcinfo << endl;
    d->pluginenabled = enabled;
}

bool KPluginInfo::pluginEnabled() const
{
    kdDebug( 703 ) << k_funcinfo << endl;
    return d->pluginenabled;
}

bool KPluginInfo::pluginEnabledByDefault() const
{
    kdDebug( 703 ) << k_funcinfo << endl;
    return d->enabledbydefault;
}

const QString & KPluginInfo::name() const
{
    return d->name;
}

const QString & KPluginInfo::comment() const
{
    return d->comment;
}

const QString & KPluginInfo::icon() const
{
    return d->icon;
}

const QString & KPluginInfo::specfile() const
{
    return d->specfile;
}

const QString & KPluginInfo::author() const
{
    return d->author;
}

const QString & KPluginInfo::email() const
{
    return d->email;
}

const QString & KPluginInfo::category() const
{
    return d->category;
}

const QString & KPluginInfo::pluginname() const
{
    return d->pluginname;
}

const QString & KPluginInfo::version() const
{
    return d->version;
}

const QString & KPluginInfo::website() const
{
    return d->website;
}

const QString & KPluginInfo::license() const
{
    return d->license;
}

const QStringList & KPluginInfo::dependencies() const
{
    return d->dependencies;
}

KService::Ptr KPluginInfo::service() const
{
    return d->service;
}

const QValueList<KService::Ptr> & KPluginInfo::kcmServices() const
{
    return d->kcmservices;
}

void KPluginInfo::setConfig( KConfig * config, const QString & group )
{
    d->config = config;
    d->configgroup = group;
}

KConfig * KPluginInfo::config() const
{
    return d->config;
}

const QString & KPluginInfo::configgroup() const
{
    return d->configgroup;
}

QVariant KPluginInfo::property( const QString & key ) const
{
    if( d->service )
        return d->service->property( key );
    else
        return QVariant();
}

QVariant KPluginInfo::operator[]( const QString & key ) const
{
    return property( key );
}

void KPluginInfo::save( KConfigGroup * config )
{
    kdDebug( 703 ) << k_funcinfo << endl;
    if( 0 == config )
    {
        if( 0 == d->config )
        {
            kdWarning( 703 ) << "no KConfigGroup, cannot save" << endl;
            return;
        }
        d->config->setGroup( d->configgroup );
        d->config->writeEntry( d->pluginname + "Enabled", pluginEnabled() );
    }
    else
        config->writeEntry( d->pluginname + "Enabled", pluginEnabled() );
}

void KPluginInfo::load( KConfigGroup * config )
{
    kdDebug( 703 ) << k_funcinfo << endl;
    if( 0 == config )
    {
        if( 0 == d->config )
        {
            kdWarning( 703 ) << "no KConfigGroup, cannot load" << endl;
            return;
        }
        d->config->setGroup( d->configgroup );
        setPluginEnabled( d->config->readBoolEntry( d->pluginname + "Enabled", pluginEnabledByDefault() ) );
    }
    else
        setPluginEnabled( config->readBoolEntry( d->pluginname + "Enabled", pluginEnabledByDefault() ) );
}

void KPluginInfo::defaults()
{
    kdDebug( 703 ) << k_funcinfo << endl;
    setPluginEnabled( pluginEnabledByDefault() );
}

// vim: sw=4 sts=4 et
