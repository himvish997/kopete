/*
    motionawayplugin.cpp

    Kopete Motion Detector Auto-Away plugin

    Copyright (c) 2002 by Duncan Mac-Vicar Prett   <duncan@kde.org>

    Contains code from motion.c ( Detect changes in a video stream )
    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
    Distributed under the GNU public license version 2
    See also the file 'COPYING.motion'

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

#include <kdebug.h>
#include <kgenericfactory.h>
#include <kstandarddirs.h>

#include <qtimer.h>

#include "motionawayplugin.h"
#include "motionawaypreferences.h"
#include "kopeteaway.h"
#include "kopeteaccountmanager.h"

// motion.c includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>

/* The following is a mandrake 9 hack. Mandrake 9
 * doesn't define this 64 bit types (we need GNU C lib
 * because we use long long and warning - gcc extensions.
 */
#if !defined(__u64) && defined(__GNUC__)
//#warning "defining __u64"
typedef unsigned long long __u64;
#endif
#if !defined(__s64) && defined(__GNUC__)
//#warning "defining __s64"
typedef __signed__ long long __s64;
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,50)
#define _LINUX_TIME_H
#endif
#include <linux/videodev.h>

#define DEF_WIDTH			352
#define DEF_HEIGHT		288
#define DEF_QUALITY		50
#define DEF_CHANGES		5000

#define DEF_POLL_INTERVAL 1500

#define DEF_GAP			60*5 /* 5 minutes */

#define NORM_DEFAULT		0
#define IN_DEFAULT		8

K_EXPORT_COMPONENT_FACTORY( kopete_motionaway, KGenericFactory<MotionAwayPlugin> );

MotionAwayPlugin::MotionAwayPlugin( QObject *parent, const char *name,
	const QStringList &/*args*/ )
	: KopetePlugin( parent, name )
{
	kdDebug(14305) << k_funcinfo << "Called." << endl;
	/* This should be read from config someday may be */
	m_width = DEF_WIDTH;
	m_height = DEF_HEIGHT;
	m_quality = DEF_QUALITY;
	m_maxChanges = DEF_CHANGES;
	m_gap = DEF_GAP;

	/* We haven't took the first picture yet */
	m_tookFirst = false;

	m_captureTimer = new QTimer(this);
	m_awayTimer = new QTimer(this);

	mPrefs = new MotionAwayPreferences ( "camera_umount", this );
	connect( mPrefs, SIGNAL(saved()), this, SLOT(slotSettingsChanged()) );
	connect( m_captureTimer, SIGNAL(timeout()), this, SLOT(slotCapture()) );
	connect( m_awayTimer, SIGNAL(timeout()), this, SLOT(slotTimeout()) );

	signal(SIGCHLD, SIG_IGN);

	m_imageRef.resize( m_width * m_height * 3);
	m_imageNew.resize( m_width * m_height * 3);
	m_imageOld.resize( m_width * m_height * 3);
	m_imageOut.resize( m_width * m_height * 3);


	kdDebug(14305) << k_funcinfo << "Opening Video4Linux Device" << endl;

	m_deviceHandler = open( (mPrefs->device()).latin1() , O_RDWR);

	if (m_deviceHandler < 0)
	{
		kdDebug(14305) << k_funcinfo << "Can't open Video4Linux Device" << endl;
	}
	else
	{
        kdDebug(14305) << k_funcinfo << "Worked! Setting Capture timers!" << endl;
		/* Capture first image, or we will get a alarm on start */
		getImage (m_deviceHandler, m_imageRef, DEF_WIDTH, DEF_HEIGHT, IN_DEFAULT, NORM_DEFAULT,
	    	VIDEO_PALETTE_RGB24);

        /* We have the first image now */
		m_tookFirst = true;
		m_wentAway = false;

		m_captureTimer->start( DEF_POLL_INTERVAL );
		m_awayTimer->start( mPrefs->awayTimeout() * 60 * 1000 );
	}
}

MotionAwayPlugin::~MotionAwayPlugin()
{
    kdDebug(14305) << k_funcinfo << "Closing Video4Linux Device" << endl;
	close (m_deviceHandler);
	kdDebug(14305) << k_funcinfo << "Freeing memory" << endl;
}

int MotionAwayPlugin::getImage(int _dev, QByteArray &_image, int _width, int _height, int _input, int _norm,  int _fmt)
{
	struct video_capability vid_caps;
	struct video_channel vid_chnl;
	struct video_window vid_win;
	struct pollfd video_fd;

	// Just to avoid a warning
	_fmt = 0;

	if (ioctl (_dev, VIDIOCGCAP, &vid_caps) == -1)
	{
		perror ("ioctl (VIDIOCGCAP)");
		return (-1);
	}
	/* Set channels and norms, NOT TESTED my philips cam doesn't have a
	 * tuner. */
	if (_input != IN_DEFAULT)
	{
		vid_chnl.channel = -1;
		if (ioctl (_dev, VIDIOCGCHAN, &vid_chnl) == -1)
		{
			perror ("ioctl (VIDIOCGCHAN)");
		}
		else
		{
			vid_chnl.channel = _input;
			vid_chnl.norm    = _norm;

			if (ioctl (_dev, VIDIOCSCHAN, &vid_chnl) == -1)
			{
				perror ("ioctl (VIDIOCSCHAN)");
				return (-1);
			}
		}
	}
	/* Set image size */
	if (ioctl (_dev, VIDIOCGWIN, &vid_win) == -1)
		return (-1);

	vid_win.width=_width;
	vid_win.height=_height;

	if (ioctl (_dev, VIDIOCSWIN, &vid_win) == -1)
		return (-1);

	/* Check if data available on the video device */
	video_fd.fd = _dev;
	video_fd.events = 0;
	video_fd.events |= POLLIN;
	video_fd.revents = 0;

	poll(&video_fd, 1, 0);

	if (video_fd.revents & POLLIN) {
		/* Read an image */
		return read (_dev, _image.data() , _width * _height * 3);
	} else {
		return (-1);
	}
}

void MotionAwayPlugin::slotCapture()
{
	/* Should go on forever... emphasis on 'should' */
	if ( getImage ( m_deviceHandler, m_imageNew, m_width, m_height, IN_DEFAULT, NORM_DEFAULT,
	    VIDEO_PALETTE_RGB24) == m_width * m_height *3 )
	{
	    int diffs = 0;
        if ( m_tookFirst )
		{
			/* Make a differences picture in image_out */
			for (int i=0; i< m_width * m_height * 3 ; i++)
			{
				m_imageOut[i]= m_imageOld[i]- m_imageNew[i];
				if ((signed char)m_imageOut[i] > 32 || (signed char)m_imageOut[i] < -32)
				{
					m_imageOld[i] = m_imageNew[i];
					diffs++;
				}
				else
				{
					m_imageOut[i] = 0;
				}
			}
		}
		else
		{
			/* First picture: new image is now the old */
			for (int i=0; i< m_width * m_height * 3; i++)
				m_imageOld[i] = m_imageNew[i];
		}

		/* The cat just walked in :) */
		if (diffs > m_maxChanges)
		{
            kdDebug(14305) << k_funcinfo << "Motion Detected. [" << diffs << "] Reseting Timeout" << endl;

			/* If we were away, now we are available again */
			if ( mPrefs->goAvailable() && !KopeteAway::globalAway() && m_wentAway)
			{
				slotActivity();
			}

			/* We reset the away timer */
            m_awayTimer->stop();
			m_awayTimer->start( mPrefs->awayTimeout() * 60 * 1000 );
		}

		/* Old image slowly decays, this will make it even harder on
			slow moving object to stay undetected */
		/*
		for (i=0; i<m_width*m_height*3; i++)
		{
			image_ref[i]=(image_ref[i]+image_new[i])/2;
		}
		*/
	}
	else
	{
		m_captureTimer->stop();
	}
}

void MotionAwayPlugin::slotActivity()
{
	kdDebug(14305) << k_funcinfo << "User activity!, going available" << endl;
	m_wentAway = false;
	KopeteAccountManager::manager()->setAvailableAll();
}

void MotionAwayPlugin::slotTimeout()
{
	if(!KopeteAway::globalAway() && !m_wentAway)
	{
		kdDebug(14305) << k_funcinfo << "Timeout and no user activity, going away" << endl;
		m_wentAway = true;
		KopeteAccountManager::manager()->setAwayAll();
	}
}

void MotionAwayPlugin::slotSettingsChanged()
{
	m_awayTimer->changeInterval(mPrefs->awayTimeout() * 60 * 1000);
}

#include "motionawayplugin.moc"
// vim: set noet ts=4 sts=4 sw=4:
