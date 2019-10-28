//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.h
//
// Purpose - uPnp Content Directory Extension for Music
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSMusic_H_
#define UPnpCDSMusic_H_

#include <QString>

#include "upnpcds.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
class MSqlQuery;
class UPnpCDSMusic : public UPnpCDSExtension
{
    public:

        UPnpCDSMusic();
        virtual ~UPnpCDSMusic() = default;

    protected:

        bool IsBrowseRequestForUs( UPnpCDSRequest *pRequest ) override; // UPnpCDSExtension
        bool IsSearchRequestForUs( UPnpCDSRequest *pRequest ) override; // UPnpCDSExtension

        void CreateRoot ( ) override; // UPnpCDSExtension

        bool LoadMetadata( const UPnpCDSRequest *pRequest,
                            UPnpCDSExtensionResults *pResults,
                            IDTokenMap tokens,
                            QString currentToken ) override; // UPnpCDSExtension
        bool LoadChildren( const UPnpCDSRequest *pRequest,
                           UPnpCDSExtensionResults *pResults,
                           IDTokenMap tokens,
                           QString currentToken ) override; // UPnpCDSExtension

    private:

        QUrl             m_URIBase;

        void             PopulateArtworkURIS( CDSObject *pItem,
                                              int songID );

        bool             LoadArtists(const UPnpCDSRequest *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     IDTokenMap tokens);
        bool             LoadAlbums(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);
        bool             LoadGenres(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);
        bool             LoadTracks(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);
        bool             LoadPlaylists(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);

        // Common code helpers
        QString BuildWhereClause( QStringList clauses,
                                  IDTokenMap tokens );
        void    BindValues ( MSqlQuery &query,
                             IDTokenMap tokens );
};

#endif
