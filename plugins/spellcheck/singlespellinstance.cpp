/*
    spellcheckplugin.cpp

    Kopete Spell Checking plugin

    Copyright (c) 2003 by Jason Keirstead   <jason@keirstead.org>

    Kopete    (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/
#include <qtextedit.h>
#include <kpopupmenu.h>

#include <kdebug.h>
#include <kspell.h>
#include <klocale.h>

#include "kopeteviewmanager.h"
#include "singlespellinstance.h"
#include "spellcheckpreferences.h"
#include "spellcheckplugin.h"

SingleSpellInstance::SingleSpellInstance( SpellCheckPlugin *plugin, KopeteView *myView ) : QObject( 0L )
{
	mView = myView;
	mPlugin = plugin;

	//If this object was created, we know we have a QTextEdit, so this is safe
	t = static_cast<QTextEdit*>( mView->editWidget() );
	t->installEventFilter( this );
	t->viewport()->installEventFilter( this );

	//Define our word seperator regexp
	//We can't use \b because QT barfs when trying to split on it
	mBound = QRegExp( QString::fromLatin1("[\\s\\W]") );

	connect( dynamic_cast<QObject*>(mView), SIGNAL( destroyed() ), this, SLOT( slotViewDestroyed() ) );
}

SingleSpellInstance::~SingleSpellInstance()
{
	kdDebug() << k_funcinfo << "Destroying single speller instance" << endl;
	mPlugin->singleSpellers.remove( this );
}

void SingleSpellInstance::slotViewDestroyed()
{
	delete this;
}

void SingleSpellInstance::slotUpdateTextEdit()
{
	QString plainTextContents = t->text();

	//Save the selection and cursor positions
	int parIdx = 1, txtIdx = 1;
	int selParFrom = 1, selTxtFrom = 1, selParTo = 1, selTxtTo = 1;
	t->getSelection(&selParFrom, &selTxtFrom, &selParTo, &selTxtTo);
	t->getCursorPosition(&parIdx, &txtIdx);

	//Don't use red if the current color is too red
	QString highlightColor;
	if( t->paletteForegroundColor().red() < 250 )
		highlightColor = QString::fromLatin1("red");
	else
		highlightColor = QString::fromLatin1("blue");

	QStringList words = QStringList::split( mBound, plainTextContents );
	if( words.count() > 0 )
	{
		for( QStringList::Iterator it = words.begin(); it != words.end(); ++it )
		{
			if( mReplacements.contains(*it) )
			{
				plainTextContents.replace( QRegExp( QString::fromLatin1("\\b(%1)\\b").arg( *it ) ),
					QString::fromLatin1("<font color=\"" + highlightColor + "\">") + *it +
					QString::fromLatin1("</font>") );
			}
		}
	}

	//Update the highlighting
	t->setTextFormat( QTextEdit::RichText );
	t->setText( plainTextContents );
	t->setTextFormat( QTextEdit::PlainText );

	//Restore the selection and cursor positions
	t->setCursorPosition( parIdx, txtIdx );
	if( selParFrom > -1 )
		t->setSelection( selParFrom, selTxtFrom, selParTo, selTxtTo );
}

void SingleSpellInstance::misspelling( const QString &originalword, const QStringList &suggestions, unsigned int )
{
	//kdDebug() << k_funcinfo << originalword << "IS MISSPELLED!" << endl;

	if( !mReplacements.contains( originalword ) )
		mReplacements[originalword] = suggestions;

	slotUpdateTextEdit();
}

bool SingleSpellInstance::eventFilter(QObject *o, QEvent *e)
{
	QWidget *w = static_cast<QWidget*>( o );

	switch( e->type() )
	{
		//Keypress event, to do the actual checking
		case QEvent::KeyPress:
		{
			QKeyEvent *event = (QKeyEvent*) e;

			//Only spellcheck when we hit a word delimiter
			if( !QChar( event->ascii() ).isLetterOrNumber() )
				mPlugin->speller()->check( t->text(), false );

			//Update highlighting
			slotUpdateTextEdit();

			break;
		}

		//ContextMenu, to enable right click replace menu
		case QEvent::ContextMenu:
		{
			QContextMenuEvent *event = (QContextMenuEvent*) e;

			int para = 1, charPos, firstSpace, lastSpace;

			//Get the character at the position of the click
			charPos = t->charAt( event->pos(), &para );
			QString paraText = t->text( para );

			if( !paraText.at(charPos).isSpace() )
			{
				//Get word right clicked on
				firstSpace = paraText.findRev( mBound, charPos ) + 1;
				lastSpace = paraText.find( mBound, charPos );
				if( lastSpace == -1 )
					lastSpace = paraText.length();
				QString word = paraText.mid( firstSpace, lastSpace - firstSpace );

				//Continue if this word was misspelled
				if( !word.isEmpty() && mReplacements.contains( word ) )
				{
					KPopupMenu p;
					p.insertTitle( i18n("Suggestions") );

					//Add the suggestions to the popup menu
					QStringList reps = mReplacements[word];
					if( reps.count() > 0 )
					{
						int listPos = 0;
						for ( QStringList::Iterator it = reps.begin(); it != reps.end(); ++it ) {
							p.insertItem( *it, listPos );
							listPos++;
						}
					}
					else
					{
						p.insertItem( QString::fromLatin1("No Suggestions"), -2 );
					}

					//Execute the popup inline
					int id = p.exec( w->mapToGlobal( event->pos() ) );

					if( id > -1 )
					{
						//Save the cursor position
						int parIdx = 1, txtIdx = 1;
						t->getCursorPosition(&parIdx, &txtIdx);

						//Put in our replacement
						QString txtContents = t->text();
						QString newContents = txtContents.left(firstSpace) + mReplacements[word][id] +
							txtContents.right( txtContents.length() - lastSpace );
						t->setText( newContents );

						//Restore the cursor position
						if( txtIdx > lastSpace )
							txtIdx += newContents.length() - txtContents.length();
						t->setCursorPosition(parIdx, txtIdx);

						//Update highlighting
						slotUpdateTextEdit();
					}

					//Cancel original event
					return true;
				}
			}

			break;
		}
		default:
			break;
	}

	//Return false so events keep propegating
	return false;
}


#include "singlespellinstance.moc"
