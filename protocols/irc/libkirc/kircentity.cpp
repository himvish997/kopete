/*
    kircentity.cpp - IRC Client

    Copyright (c) 2004      by Michel Hermier <michel.hermier@wanadoo.fr>

    Kopete    (c) 2004      by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kircentity.h"

#include <kdebug.h>

using namespace KIRC;

/**
 * Match a possible user definition:
 * nick!user@host
 * where user and host are optionnal.
 * NOTE: If changes are done to the regexp string, update also the sm_userStrictRegExp regexp string.
 */
const QRegExp Entity::sm_userRegExp(QString::fromLatin1("^([^\\s,:!@]+)(?:(?:!([^\\s,:!@]+))?(?:@([^\\s,!@]+)))?$"));

/**
 * Regexp to match strictly the complete user definition:
 * nick!user@host
 * NOTE: If changes are done to the regexp string, update also the sm_userRegExp regexp string.
 */
const QRegExp Entity::sm_userStrictRegExp(QString::fromLatin1("^([^\\s,:!@]+)!([^\\s,:!@]+)@([^\\s,:!@]+)$"));

const QRegExp Entity::sm_channelRegExp( QString::fromLatin1("^[#!+&][^\\s,]+$") );

Entity::Entity(const QString &name, const EntityType type)
{
	setName(name);
	m_status.type = type;
}

Entity::~Entity()
{
	emit destroyed(this);
}

bool Entity::operator == (const Entity &)
{
	#warning Implement Me
	return false;
}

void Entity::setName(const QString &name)
{
	m_name = name;
	emit updated();
}

QString Entity::name() const
{
	return m_name;
}

QString Entity::host() const
{
	return m_host;
}

EntityStatus Entity::status() const
{
	return m_status;
}

EntityType Entity::type() const
{
	return m_status.type;
}

EntityType Entity::guessType()
{
	m_status.type = guessType(m_name);
	return m_status.type;
}

// FIXME: Implement me
KIRC::EntityType Entity::guessType(const QString &)
{
	return Unknown;
}

void Entity::setCodec(QTextCodec *codec)
{
	m_codec = codec;
	emit updated();
}

QTextCodec *Entity::codec() const
{
	return m_codec;
}

#include "kircentity.moc"
