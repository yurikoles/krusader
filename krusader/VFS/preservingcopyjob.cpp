/***************************************************************************
                    preservingcopyjob.cpp  -  description
                             -------------------
    copyright            : (C) 2005 + by Csaba Karai
    e-mail               : krusader@users.sourceforge.net
    web site             : http://krusader.sourceforge.net
 ---------------------------------------------------------------------------
  Description
 ***************************************************************************

  A

     db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
     88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
     88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
     88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
     88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
     YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                     S o u r c e    F i l e

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "preservingcopyjob.h"

#if 0 

TODO

#include "../defaults.h"
#include "../krusader.h"
#include <utime.h>
#include <kde_file.h>
#include <kio/job.h>
#include <kio/global.h>
#include <kio/jobclasses.h>
#include <kio/jobuidelegate.h>
#include <kuiserverjobtracker.h>
#include <kfileitem.h>
#include <qfile.h>
//Added by qt3to4:
#include <Q3ValueList>
#include <pwd.h>
#include <grp.h>


#if defined( HAVE_POSIX_ACL )
#include <sys/acl.h>
#ifdef HAVE_NON_POSIX_ACL_EXTENSIONS
#include <acl/libacl.h>
#endif
#endif

Attributes::Attributes() 
{
  time = (time_t)-1;
  uid = (uid_t)-1;
  gid = (gid_t)-1;
  mode = (mode_t)-1;
  acl = QString();
}

Attributes::Attributes( time_t tIn, uid_t uIn, gid_t gIn, mode_t modeIn, const QString & aclIn ) 
{
  time = tIn, uid = uIn, gid = gIn, mode = modeIn, acl = aclIn;
}

Attributes::Attributes( time_t tIn, QString user, QString group, mode_t modeIn, const QString & aclIn ) 
{
  time = tIn;
  uid = (uid_t)-1;
  struct passwd* pw = getpwnam(QFile::encodeName( user ));
  if ( pw != 0L )
    uid = pw->pw_uid;
  gid = (gid_t)-1;
  struct group* g = getgrnam(QFile::encodeName( group ));
  if ( g != 0L )
    gid = g->gr_gid;
  mode = modeIn;
  acl = aclIn;
}

PreservingCopyJob::PreservingCopyJob( const KUrl::List& src, const KUrl& dest, CopyMode mode,
  bool asMethod, bool showProgressInfo ) : KIO::CopyJob( src, dest, mode, asMethod )
{
  if( showProgressInfo )
  {
     setUiDelegate(new KIO::JobUiDelegate() );
     KIO::getJobTracker()->registerJob(this);
  }
  if( dest.isLocalFile() )
  {
    connect( this, SIGNAL( aboutToCreate (KIO::Job *, const Q3ValueList< KIO::CopyInfo > &) ),
             this, SLOT( slotAboutToCreate (KIO::Job *, const Q3ValueList< KIO::CopyInfo > &) ) );
    connect( this, SIGNAL( copyingDone( KIO::Job *, const KUrl &, const KUrl &, bool, bool) ),
             this, SLOT( slotCopyingDone( KIO::Job *, const KUrl &, const KUrl &, bool, bool) ) );
    connect( this, SIGNAL( result( KJob * ) ),
             this, SLOT( slotFinished() ) );
  }
}

void PreservingCopyJob::slotAboutToCreate( KIO::Job */*job*/, const Q3ValueList< KIO::CopyInfo > &files )
{
  for ( Q3ValueList< KIO::CopyInfo >::ConstIterator it = files.begin(); it != files.end(); ++it ) {
  
    if( (*it).uSource.isLocalFile() ) {
      KDE_struct_stat stat_p;
      KDE_lstat( (*it).uSource.path(KUrl::RemoveTrailingSlash).local8Bit(),&stat_p);    /* getting the date information */
      
      QString aclStr;
#if defined( HAVE_POSIX_ACL )
      acl_t acl = acl_get_file( (*it).uSource.path(KUrl::RemoveTrailingSlash).local8Bit(), ACL_TYPE_ACCESS );

      bool aclExtended = false;
      if( acl )
      {
#if HAVE_NON_POSIX_ACL_EXTENSIONS
        aclExtended = acl_equiv_mode( acl, 0 );
#else
        acl_entry_t entry;
        int ret = acl_get_entry( acl, ACL_FIRST_ENTRY, &entry );
        while ( ret == 1 ) {
          acl_tag_t currentTag;
          acl_get_tag_type( entry, &currentTag );
          if ( currentTag != ACL_USER_OBJ &&
            currentTag != ACL_GROUP_OBJ &&
            currentTag != ACL_OTHER )
          {
            aclExtended = true;
            break;
          }
          ret = acl_get_entry( acl, ACL_NEXT_ENTRY, &entry );
        }
#endif
      }


      if ( acl && !aclExtended ) {
        acl_free( acl );
        acl = NULL;
      }
      if( acl )
      {
        char *aclString = acl_to_text( acl, 0 );
        aclStr = QString::fromLatin1( aclString );
        acl_free( (void*)aclString );
        acl_free( acl );
      }
#endif
      fileAttributes[ (*it).uSource ] = Attributes( stat_p.st_mtime, stat_p.st_uid, stat_p.st_gid, stat_p.st_mode & 07777, aclStr );
    }
    else {
      time_t mtime = (*it).mtime;
  
      if( mtime != 0 && mtime != ((time_t) -1 ) )       /* is it correct? */
        fileAttributes[ (*it).uSource ].time = mtime;

      int permissions = (*it).permissions;
      fileAttributes[ (*it).uSource ].mode = permissions;
    }
  }
}

void PreservingCopyJob::slotResult( Job *job ) {
  if( !job->error() ) {
    if( job->inherits( "KIO::StatJob" ) ) {       /* Unfortunately KIO forgets to set times when the file is in the */
      KUrl url = ((KIO::SimpleJob *)job)->url();  /* base directory. That's why we capture every StatJob and set the */
                                                /* time manually. */
      KIO::UDSEntry entry = static_cast<KIO::StatJob*>(job)->statResult();      
      KFileItem kfi(entry, url );
    
#if defined( HAVE_POSIX_ACL )
      fileAttributes[ url ] = Attributes( kfi.time( KIO::UDSEntry::UDS_MODIFICATION_TIME ), kfi.user(), kfi.group(), kfi.mode(), kfi.ACL().asString() );
#else
      fileAttributes[ url ] = Attributes( kfi.time( KIO::UDSEntry::UDS_MODIFICATION_TIME ), kfi.user(), kfi.group(), kfi.mode(), QString() );
#endif
    }
  }

  CopyJob::slotResult( job );

  QList<KJob*> subjob = subjobs();
  for( unsigned j=0; j != subjob.count(); j++ ) {
    if( subjob[ j ]->inherits( "KIO::ListJob" ) ) {
      disconnect( subjob[ j ], SIGNAL( entries (KIO::Job *, const KIO::UDSEntryList &) ),
                  this, SLOT( slotListEntries (KIO::Job *, const KIO::UDSEntryList &) ) );
      connect( subjob[ j ], SIGNAL( entries (KIO::Job *, const KIO::UDSEntryList &) ),
                  this, SLOT( slotListEntries (KIO::Job *, const KIO::UDSEntryList &) ) );
    }
  }
}

void PreservingCopyJob::slotListEntries(KIO::Job *job, const KIO::UDSEntryList &list) {
  KIO::UDSEntryList::const_iterator it = list.begin();
  KIO::UDSEntryList::const_iterator end = list.end();
  for (; it != end; ++it) {
    KUrl url = ((KIO::SimpleJob *)job)->url();
    QString relName, user, group;
    time_t mtime = (time_t)-1;
    mode_t mode = 0755;
    QString acl;

    if( (*it).contains( KIO::UDSEntry::UDS_URL ) )
      relName = KUrl( (*it).stringValue( KIO::UDSEntry::UDS_URL ) ).fileName();
    else if( (*it).contains( KIO::UDSEntry::UDS_NAME ) )
      relName = (*it).stringValue( KIO::UDSEntry::UDS_NAME );

    if( (*it).contains( KIO::UDSEntry::UDS_MODIFICATION_TIME ) )
      mtime = (time_t)((*it).numberValue( KIO::UDSEntry::UDS_MODIFICATION_TIME ));

    if( (*it).contains( KIO::UDSEntry::UDS_USER ) )
      user = (*it).stringValue( KIO::UDSEntry::UDS_USER );

    if( (*it).contains( KIO::UDSEntry::UDS_GROUP ) )
      group = (*it).stringValue( KIO::UDSEntry::UDS_GROUP );

    if( (*it).contains( KIO::UDSEntry::UDS_ACCESS ) )
      mode = ((*it).numberValue( KIO::UDSEntry::UDS_ACCESS ));

#if defined( HAVE_POSIX_ACL )
    if( (*it).contains( KIO::UDSEntry::UDS_ACL_STRING ) )
      acl = (*it).stringValue( KIO::UDSEntry::UDS_ACL_STRING );
#endif

    url.addPath( relName );

    fileAttributes[ url ] = Attributes( mtime, user, group, mode, acl );
  }
}

void PreservingCopyJob::slotCopyingDone( KIO::Job *, const KUrl &from, const KUrl &to, bool postpone, bool)
{
  if( postpone ) { // the directories are stamped at the last step, so if it's a directory, we postpone
    unsigned i=0;
    QString path = to.path( KUrl::RemoveTrailingSlash );

    for( ; i != directoriesToStamp.count(); i++ ) // sort the URL-s to avoid parent time stamp modification
      if( path >= directoriesToStamp[ i ].path( KUrl::RemoveTrailingSlash ) )
        break;

    directoriesToStamp.insert( directoriesToStamp.at( i ), to );
    originalDirectories.insert( originalDirectories.at( i ), from );
  }
  else if( fileAttributes.count( from ) ) {
    Attributes attrs = fileAttributes[ from ];
    fileAttributes.remove( from );
    
    time_t mtime = attrs.time;
   
    if( to.isLocalFile() )
    {
      if( mtime != 0 && mtime != ((time_t) -1 ) )
      {
        struct utimbuf timestamp;

        timestamp.actime  = time( 0 );
        timestamp.modtime = mtime;

        utime( (const char *)( to.path( KUrl::RemoveTrailingSlash ).local8Bit() ), &timestamp );
      }

      if( attrs.uid != (uid_t)-1 )
        chown( (const char *)( to.path( KUrl::RemoveTrailingSlash ).local8Bit() ), attrs.uid, (gid_t)-1 );
      if( attrs.gid != (gid_t)-1 )
        chown( (const char *)( to.path( KUrl::RemoveTrailingSlash ).local8Bit() ), (uid_t)-1, attrs.gid );

      if( attrs.mode != (mode_t) -1 )
        chmod( (const char *)( to.path( KUrl::RemoveTrailingSlash ).local8Bit() ), attrs.mode );

#if defined( HAVE_POSIX_ACL )
      if( !attrs.acl.isNull() )
      {
        acl_t acl = acl_from_text( attrs.acl.toLatin1() );
        if( acl && !acl_valid( acl ) )
          acl_set_file( to.path( KUrl::RemoveTrailingSlash ).local8Bit(), ACL_TYPE_ACCESS, acl );
        if( acl )
          acl_free( acl );
      }
#endif
    }
  }
}

void PreservingCopyJob::slotFinished() {
  for( unsigned i=0; i != directoriesToStamp.count(); i++ ) {
    KUrl from = originalDirectories[ i ];
    KUrl to = directoriesToStamp[ i ];

    slotCopyingDone( 0, from, to, false, false );
  }
}

KIO::CopyJob * PreservingCopyJob::createCopyJob( PreserveMode pmode, const KUrl::List& src, const KUrl& dest, CopyMode mode, bool asMethod, bool showProgressInfo )
{
  if( ! dest.isLocalFile() )
    pmode = PM_NONE;
  if( mode == KIO::CopyJob::Link )
    pmode = PM_NONE;

  switch( pmode )
  {
  case PM_PRESERVE_ATTR:
    return new PreservingCopyJob( src, dest, mode, asMethod, showProgressInfo );
  case PM_DEFAULT:
    {
      KConfigGroup grp( krConfig, "Advanced" );
      bool preserve = grp.readEntry( "PreserveAttributes", _PreserveAttributes );

      if( preserve )
        return new PreservingCopyJob( src, dest, mode, asMethod, showProgressInfo );
      else
      {
        KIO::CopyJob *res = new KIO::CopyJob( src, dest, mode, asMethod );
        if( showProgressInfo )
        {
          res->setUiDelegate(new KIO::JobUiDelegate() );
          KIO::getJobTracker()->registerJob(res);
        }
        return res;
      }
    }
  case PM_NONE:
  default:
    {
      KIO::CopyJob *res = new KIO::CopyJob( src, dest, mode, asMethod );
      if( showProgressInfo )
      {
        res->setUiDelegate(new KIO::JobUiDelegate() );
        KIO::getJobTracker()->registerJob(res);
      }
      return res;
    }
  }
}

#endif


KIO::CopyJob * PreservingCopyJob::createCopyJob( PreserveMode /* pmode */, const KUrl::List& src, const KUrl& dest, KIO::CopyJob::CopyMode mode, bool /* asMethod */, bool showProgressInfo )
{
  switch( mode ) {
    case KIO::CopyJob::Copy:
        return KIO::copy( src, dest, showProgressInfo?KIO::DefaultFlags:KIO::HideProgressInfo );
    case KIO::CopyJob::Move:
        return KIO::move( src, dest, showProgressInfo?KIO::DefaultFlags:KIO::HideProgressInfo );
    case KIO::CopyJob::Link:
        return KIO::link( src, dest, showProgressInfo?KIO::DefaultFlags:KIO::HideProgressInfo );
    default:
      return 0;
  }
}

#include "preservingcopyjob.moc"
