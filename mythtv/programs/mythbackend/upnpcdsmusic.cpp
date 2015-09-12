//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.cpp
//
// Purpose - uPnp Content Directory Extension for Music
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#include <climits>

#include <QFileInfo>

#include "upnpcdsmusic.h"
#include "httprequest.h"
#include "mythcorecontext.h"

/*
   Music                            Music
    - All Music                     Music/All
      + <Track 1>                   Music/All/item?Id=1
      + <Track 2>
      + <Track 3>
    - PlayLists
    - By Artist                     Music/artist
      - <Artist 1>                  Music/artist/artistKey=Pink Floyd
        - <Album 1>                 Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
          + <Track 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/item?Id=1
          + <Track 2>
    - By Album
      - <Album 1>
        + <Track 1>
        + <Track 2>
    - By Recently Added
      + <Track 1>
      + <Track 2>
    - By Genre
      - By Artist                   Music/artist
        - <Artist 1>                Music/artist/artistKey=Pink Floyd
          - <Album 1>               Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall
            + <Track 1>             Music/artist/artistKey=Pink Floyd/album/albumKey=The Wall/item?Id=1
            + <Track 2>
*/

UPnpCDSRootInfo UPnpCDSMusic::g_RootNodes[] =
{
    {   "All Music",
        "*",
        "SELECT song_id as id, "
          "name, "
          "1 as children "
            "FROM music_songs song "
            "%1 "
            "ORDER BY name",
        "", "name" },

#if 0
// This is currently broken... need to handle list of items with single parent
// (like 'All Music')

    {   "Recently Added",
        "*",
        "SELECT song_id id, "
          "name, "
          "1 as children "
            "FROM music_songs song "
            "%1 "
            "ORDER BY name",
        "WHERE (DATEDIFF( CURDATE(), date_modified ) <= 30 ) ", "" },
#endif
    {   "Playlists",
            "playlist.playlist_id",
            "SELECT p.playlist_id as id, "
              "p.playlist_name as name, "
              " count( song.song_id ) as children "
                "FROM music_playlists p join music_songs song on FIND_IN_SET(song.song_id, p.playlist_songs) "
                "WHERE p.playlist_name NOT LIKE '%storage' "
                "%1 "
                "GROUP BY p.playlist_id "
                "ORDER BY p.playlist_name ",
            "AND FIND_IN_SET(song.song_id, (:KEY))" },

    {   "Top Played Albums",
            "song.album_id",
            "SELECT a.album_id as id, "
              "a.album_name as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.album_id "
                "ORDER BY sum( song.numplays ) DESC LIMIT 50 ",
            "WHERE song.album_id=:KEY" },

    {   "Least Played Albums",
            "song.album_id",
            "SELECT a.album_id as id, "
              "a.album_name as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.album_id "
                "ORDER BY sum( song.numplays ) LIMIT 50 ",
            "WHERE song.album_id=:KEY" },

    {   "Recently Played Albums",
            "song.album_id",
            "SELECT a.album_id as id, "
              "a.album_name as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.album_id "
                "ORDER BY song.lastplay DESC LIMIT 50 ",
            "WHERE song.album_id=:KEY" },
    {   "Recently Added Albums",
            "song.album_id",
            "SELECT a.album_id as id, "
              "a.album_name as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.album_id "
                "ORDER BY song.date_entered DESC LIMIT 50 ",
            "WHERE song.album_id=:KEY" },
    {   "By Album",
        "song.album_id",
        "SELECT a.album_id as id, "
          "a.album_name as name, "
          "count( song.album_id ) as children "
            "FROM music_songs song join music_albums a on a.album_id = song.album_id "
            "%1 "
            "GROUP BY a.album_id "
            "ORDER BY a.album_name",
        "WHERE song.album_id=:KEY", "album.album_name" },
    {   "Top Played Artists",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.artist_id "
                "ORDER BY sum( song.numplays ) DESC LIMIT 50 ",
            "WHERE song.artist_id=:KEY" },
    {   "Top Played Artists",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.artist_id "
                "ORDER BY sum( song.numplays ) DESC LIMIT 50 ",
            "WHERE song.artist_id=:KEY" },

    {   "Least Played Artists",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.artist_id "
                "ORDER BY sum( song.numplays ) LIMIT 50 ",
            "WHERE song.artist_id=:KEY" },

    {   "Random Artists",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY song.artist_id "
                "ORDER BY RAND() ",
            "WHERE song.artist_id=:KEY" },
        {   "By Album A-F",
            "song.album_id",
            "SELECT a.album_id as id, "
              "SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.album_name REGEXP '^[A-F]' "
                "%1 "
                "GROUP BY SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) "
                "ORDER BY a.album_name",
            "AND song.album_id=:KEY " },

        {   "By Album G-L",
            "song.album_id",
            "SELECT a.album_id as id, "
              "SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.album_name REGEXP '^[G-L]' "
                "%1 "
                "GROUP BY SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) "
                "ORDER BY a.album_name",
            "AND song.album_id=:KEY " },

        {   "By Album M-R",
            "song.album_id",
            "SELECT a.album_id as id, "
              "SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.album_name REGEXP '^[M-R]' "
                "%1 "
                "GROUP BY SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) "
                "ORDER BY a.album_name",
            "AND song.album_id=:KEY " },

        {   "By Album S-Z",
            "song.album_id",
            "SELECT a.album_id as id, "
              "SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) as name, "
              "count( song.album_id ) as children "
                "FROM music_songs song join music_albums a on a.album_id = song.album_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.album_name REGEXP '^[S-Z]' "
                "%1 "
                "GROUP BY SUBSTRING_INDEX(SUBSTRING_INDEX(d.path,'/',3),'/',-1) "
                "ORDER BY a.album_name",
            "AND song.album_id=:KEY " },

        {   "By Artist A-F",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.artist_name REGEXP '^[A-F]' "
                "%1 "
                "GROUP BY  a.artist_id "
                "ORDER BY a.artist_name",
            "AND song.artist_id=:KEY" },

        {   "By Artist G-L",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.artist_name REGEXP '^[G-L]' "
                "%1 "
                "GROUP BY  a.artist_id "
                "ORDER BY a.artist_name",
            "AND song.artist_id=:KEY" },

        {   "By Artist M-R",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.artist_name REGEXP '^[M-R]' "
                "%1 "
                "GROUP BY  a.artist_id "
                "ORDER BY a.artist_name",
            "AND song.artist_id=:KEY" },
        {   "By Artist S-Z",
            "song.artist_id",
            "SELECT a.artist_id as id, "
              "a.artist_name as name, "
              "count( song.artist_id ) as children "
                "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "WHERE a.artist_name REGEXP '^[S-Z]' "
                "%1 "
                "GROUP BY  a.artist_id "
                "ORDER BY a.artist_name",
            "AND song.artist_id=:KEY" },

    {   "By Genre",
            "song.genre_id",
            "SELECT g.genre_id as id, "
              "g.genre as name, "
              "count( song.genre_id ) as children "
                "FROM music_songs song join music_genres g on g.genre_id = song.genre_id "
                "join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY g.genre_id "
                "ORDER BY sum( g.genre ) DESC LIMIT 50 ",
            "WHERE song.genre_id=:KEY" },

    {   "By Directory",
            "directory_id",
            "SELECT d.directory_id as id, "
            "IF( path REGEXP '^[^/]+/[^/]+/[^/]+$',SUBSTRING_INDEX(SUBSTRING_INDEX(path,'/',3),'/',-2), "
                                                  "SUBSTRING_INDEX(SUBSTRING_INDEX(path,'/',4),'/',-3)) "
                "as name, "
              "count( song.directory_id ) as children "
                "FROM music_songs song join music_directories d on d.directory_id = song.directory_id "
                "%1 "
                "GROUP BY d.directory_id "
                "ORDER BY d.path",
            "WHERE song.directory_id=:KEY" },
#if 0
    {   "By Artist",
        "artist_id",
        "SELECT a.artist_id as id, "
          "a.artist_name as name, "
          "count( distinct song.artist_id ) as children "
            "FROM music_songs song join music_artists a on a.artist_id = song.artist_id "
            "%1 "
            "GROUP BY a.artist_id "
            "ORDER BY a.artist_name",
        "WHERE song.artist_id=:KEY", "" },

{   "By Genre",
        "genre_id",
        "SELECT g.genre_id as id, "
          "genre as name, "
          "count( distinct song.genre_id ) as children "
            "FROM music_songs song join music_genres g on g.genre_id = song.genre_id "
            "%1 "
            "GROUP BY g.genre_id "
            "ORDER BY g.genre",
        "WHERE song.genre_id=:KEY", "" },
#endif

};

int UPnpCDSMusic::g_nRootCount = sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSMusic::GetRootInfo( int nIdx )
{
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]);

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSMusic::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::GetTableName( QString sColumn )
{
    QString sTableName = "";

    if(sColumn.startsWith("playlist"))
    {
        sTableName = "music_playlists playlist";
    }
    else
    {
        sTableName = "music_songs song";
    }
    return sTableName;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSMusic::GetItemListSQL( QString sColumn )
{
    QString sItemListSQL = "";

    sItemListSQL = "SELECT song.song_id as intid, artist.artist_name as artist, "         \
                       "album.album_name as album, song.name as title, "                  \
                       "genre.genre, song.year, song.track as tracknum, "                 \
                       "song.description, song.filename, song.length "                    \
                    "FROM music_songs song "                                              \
                       " join music_artists artist on artist.artist_id = song.artist_id " \
                       " join music_albums album on album.album_id = song.album_id "      \
                       " join music_genres genre on  genre.genre_id = song.genre_id ";

    if(sColumn.startsWith("playlist"))
        sItemListSQL.append(" join music_playlists playlist on FIND_IN_SET(song.song_id, playlist.playlist_songs) ");

    return sItemListSQL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int     nId    = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE song.song_id=:ID " )
                      .arg( GetItemListSQL() );

    query.prepare( sSQL );

    query.bindValue( ":ID", (int)nId );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // Xbox360 compatibility code.

    if (pRequest->m_eClient == CDS_ClientXBox && 
        pRequest->m_sContainerID == "7")
    {
        pRequest->m_sObjectId = "Music";

        LOG(VB_UPNP, LOG_INFO,
            "UPnpCDSMusic::IsBrowseRequestForUs - Yes, ContainerId == 7");

        return true;
    }

    if ((pRequest->m_sObjectId.isEmpty()) && 
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSMusic::IsBrowseRequestForUs - Not sure... Calling base class.");

    return UPnpCDSExtension::IsBrowseRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSMusic::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // XBox 360 compatibility code

    if (pRequest->m_eClient == CDS_ClientXBox && 
        pRequest->m_sContainerID == "7")
    {
        pRequest->m_sObjectId       = "Music/1";
        pRequest->m_sSearchCriteria = "object.container.album.musicAlbum";
        pRequest->m_sSearchList.append( pRequest->m_sSearchCriteria );

        LOG(VB_UPNP, LOG_INFO, "UPnpCDSMusic::IsSearchRequestForUs... Yes.");

        return true;
    }

    if (pRequest->m_sContainerID == "4")
    {
        pRequest->m_sObjectId       = "Music";
        pRequest->m_sSearchCriteria = "object.item.audioItem.musicTrack";
        pRequest->m_sSearchList.append( pRequest->m_sSearchCriteria );

        LOG(VB_UPNP, LOG_INFO, "UPnpCDSMusic::IsSearchRequestForUs... Yes.");

        return true;
    }

    if ((pRequest->m_sObjectId.isEmpty()) && 
        (!pRequest->m_sContainerID.isEmpty()))
        pRequest->m_sObjectId = pRequest->m_sContainerID;

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSMusic::IsSearchRequestForUs.. Don't know, calling base class.");

    return UPnpCDSExtension::IsSearchRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSMusic::AddItem( const UPnpCDSRequest    *pRequest, 
                            const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef,
                            MSqlQuery               &query )
{
    QString        sName;

    int            nId          = query.value( 0).toInt();
    QString        sArtist      = query.value( 1).toString();
    QString        sAlbum       = query.value( 2).toString();
    QString        sTitle       = query.value( 3).toString();
    QString        sGenre       = query.value( 4).toString();
    int            nYear        = query.value( 5).toInt();
    int            nTrackNum    = query.value( 6).toInt();
    QString        sDescription = query.value( 7).toString();
    QString        sFileName    = query.value( 8).toString();
    uint           nLength      = query.value( 9).toInt();
    uint64_t       nFileSize    = (quint64)query.value(10).toULongLong();

#if 0
    if ((nNodeIdx == 0) || (nNodeIdx == 1))
    {
        sName = QString( "%1-%2:%3" )
                   .arg( sArtist)
                   .arg( sAlbum )
                   .arg( sTitle );
    }
    else
#endif
        sName = sTitle;


    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------

#if 0
    if (!m_mapBackendIp.contains( sHostName ))
        m_mapBackendIp[ sHostName ] = gCoreContext->GetSettingOnHost( "BackendServerIp", sHostName);

    if (!m_mapBackendPort.contains( sHostName ))
        m_mapBackendPort[ sHostName ] = gCoreContext->GetSettingOnHost("BackendStatusPort", sHostName);
#endif

    QString sServerIp   = gCoreContext->GetBackendServerIP4();
    int sPort           = gCoreContext->GetBackendStatusPort();

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sURIBase   = QString( "http://%1:%2/Content/" )
                            .arg( sServerIp )
                            .arg( sPort     );

    // send &quest; as workaround for reciva radio
    QString sURIParams = QString( "&quest;Id=%1" )
                            .arg( nId );


    QString sId        = QString( "Music/1/item%1")
                            .arg( sURIParams );

    CDSObject *pItem   = CDSObject::CreateMusicTrack( sId,
                                                      sName,
                                                      sObjectId );
    pItem->m_bRestricted  = true;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "NOT_WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2")
                            .arg( m_sExtensionId )
                            .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    pItem->SetPropValue( "genre"                , sGenre      );
    pItem->SetPropValue( "description"          , sTitle      );
    pItem->SetPropValue( "longDescription"      , sDescription);

    pItem->SetPropValue( "artist"               ,  sArtist    );
    pItem->SetPropValue( "album"                ,  sAlbum     );
    pItem->SetPropValue( "originalTrackNumber"  ,  QString::number(nTrackNum));
    if (nYear > 0 && nYear < 9999)
        pItem->SetPropValue( "date",  QDate(nYear,1,1).toString(Qt::ISODate));

#if 0
    pObject->AddProperty( new Property( "publisher"       , "dc"   ));
    pObject->AddProperty( new Property( "language"        , "dc"   ));
    pObject->AddProperty( new Property( "relation"        , "dc"   ));
    pObject->AddProperty( new Property( "rights"          , "dc"   ));


    pObject->AddProperty( new Property( "playlist"            , "upnp" ));
    pObject->AddProperty( new Property( "storageMedium"       , "upnp" ));
    pObject->AddProperty( new Property( "contributor"         , "dc"   ));
    pObject->AddProperty( new Property( "date"                , "dc"   ));
#endif

    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Music Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------

    QFileInfo fInfo( sFileName );

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.suffix() );
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetMusic%2").arg( sURIBase   )
                                                .arg( sURIParams );

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

    nLength /= 1000;

    QString sDur;

    sDur.sprintf("%02d:%02d:%02d",
                  (nLength / 3600) % 24,
                  (nLength / 60) % 60,
                  nLength % 60);

    pRes->AddAttribute( "duration"  , sDur      );
    if (nFileSize > 0)
        pRes->AddAttribute( "size"      , QString::number( nFileSize) );
}

// vim:ts=4:sw=4:ai:et:si:sts=4
