
#include "lookup.h"

#include <vector>

#include <QList>

#include "programinfo.h"
#include "recordingrule.h"
#include "mythlogging.h"
#include "jobqueue.h"
#include "remoteutil.h"
#include "mythcorecontext.h"

#include "metadataimagehelper.h"

LookerUpper::LookerUpper() :
    m_busyRecList(QList<ProgramInfo*>()),
    m_updaterules(false), m_updateartwork(false)
{
    m_metadataFactory = new MetadataFactory(this);
}

LookerUpper::~LookerUpper()
{
    while (!m_busyRecList.isEmpty())
        delete m_busyRecList.takeFirst();
}

bool LookerUpper::StillWorking()
{
    if (m_metadataFactory->IsRunning() ||
        m_busyRecList.count())
    {
        return true;
    }

    return false;
}

void LookerUpper::HandleSingleRecording(const uint chanid,
                                        const QDateTime &starttime,
                                        bool updaterules)
{
    ProgramInfo *pginfo = new ProgramInfo(chanid, starttime);

    if (!pginfo)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "No valid program info for supplied chanid/starttime");
        return;
    }

    m_updaterules = updaterules;
    m_updateartwork = true;

    m_busyRecList.append(pginfo);
    m_metadataFactory->Lookup(pginfo, true, m_updateartwork, false);
}

void LookerUpper::HandleAllRecordings(bool updaterules)
{
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    m_updaterules = updaterules;

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for( int n = 0; n < (int)progList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(progList[n]));
        if ((pginfo->GetRecordingGroup() != "Deleted") &&
            (pginfo->GetRecordingGroup() != "LiveTV") &&
            (pginfo->GetInetRef().isEmpty() ||
            (!pginfo->GetSubtitle().isEmpty() &&
            (pginfo->GetSeason() == 0) &&
            (pginfo->GetEpisode() == 0))))
        {
            QString msg = QString("Looking up: %1 %2").arg(pginfo->GetTitle())
                                           .arg(pginfo->GetSubtitle());
            LOG(VB_GENERAL, LOG_INFO, msg);

            m_busyRecList.append(pginfo);
            m_metadataFactory->Lookup(pginfo, true, false, false);
        }
        else
            delete pginfo;
    }
}

void LookerUpper::HandleAllRecordingRules()
{
    m_updaterules = true;

    vector<ProgramInfo *> recordingList;

    RemoteGetAllScheduledRecordings(recordingList);

    for( int n = 0; n < (int)recordingList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(recordingList[n]));
        if (pginfo->GetInetRef().isEmpty())
        {
            QString msg = QString("Looking up: %1 %2").arg(pginfo->GetTitle())
                                           .arg(pginfo->GetSubtitle());
            LOG(VB_GENERAL, LOG_INFO, msg);

            m_busyRecList.append(pginfo);
            m_metadataFactory->Lookup(pginfo, true, false, true);
        }
        else
            delete pginfo;
    }
}

void LookerUpper::HandleAllArtwork(bool aggressive)
{
    m_updateartwork = true;

    if (aggressive)
        m_updaterules = true;

    // First, handle all recording rules w/ inetrefs
    vector<ProgramInfo *> recordingList;

    RemoteGetAllScheduledRecordings(recordingList);
    int maxartnum = 3;

    for( int n = 0; n < (int)recordingList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(recordingList[n]));
        bool dolookup = true;

        if (pginfo->GetInetRef().isEmpty())
            dolookup = false;
        if (dolookup || aggressive)
        {
            ArtworkMap map = GetArtwork(pginfo->GetInetRef(), pginfo->GetSeason(), true);
            if (map.isEmpty() || (aggressive && map.count() < maxartnum))
            {
                QString msg = QString("Looking up artwork for recording rule: %1 %2")
                                               .arg(pginfo->GetTitle())
                                               .arg(pginfo->GetSubtitle());
                LOG(VB_GENERAL, LOG_INFO, msg);

                m_busyRecList.append(pginfo);
                m_metadataFactory->Lookup(pginfo, true, true, true);
                continue;
            }
        }
        delete pginfo;
    }

    // Now, Attempt to fill in the gaps for recordings
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for( int n = 0; n < (int)progList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(progList[n]));

        bool dolookup = true;

        LookupType type = GuessLookupType(pginfo);

        if (type == kProbableMovie)
           maxartnum = 2;

        if ((!aggressive && type == kProbableGenericTelevision) ||
             pginfo->GetRecordingGroup() == "Deleted" ||
             pginfo->GetRecordingGroup() == "LiveTV")
            dolookup = false;
        if (dolookup || aggressive)
        {
            ArtworkMap map = GetArtwork(pginfo->GetInetRef(), pginfo->GetSeason(), true);
            if (map.isEmpty() || (aggressive && map.count() < maxartnum))
            {
               QString msg = QString("Looking up artwork for recording: %1 %2")
                                           .arg(pginfo->GetTitle())
                                           .arg(pginfo->GetSubtitle());
                LOG(VB_GENERAL, LOG_INFO, msg);

                m_busyRecList.append(pginfo);
                m_metadataFactory->Lookup(pginfo, true, true, aggressive);
                continue;
            }
        }
        delete pginfo;
    }

}

void LookerUpper::CopyRuleInetrefsToRecordings()
{
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for( int n = 0; n < (int)progList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(progList[n]));
        if (pginfo && pginfo->GetInetRef().isEmpty())
        {
            RecordingRule *rule = new RecordingRule();
            rule->m_recordID = pginfo->GetRecordingRuleID();
            rule->Load();
            if (!rule->m_inetref.isEmpty())
            {
                QString msg = QString("%1").arg(pginfo->GetTitle());
                if (!pginfo->GetSubtitle().isEmpty())
                    msg += QString(": %1").arg(pginfo->GetSubtitle());
                msg += " has no inetref, but its recording rule does. Copying...";
                LOG(VB_GENERAL, LOG_INFO, msg);
                pginfo->SaveInetRef(rule->m_inetref);
            }
            delete rule;
        }
        delete pginfo;
    }
}

void LookerUpper::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        MetadataFactoryMultiResult *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);

        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->results;

        if (list.count() > 1)
        {
            int yearindex = -1;
            MetadataLookup *exactTitleMeta = NULL;
            QDate exactTitleDate;
            float exactTitlePopularity = 0.0;
            bool foundMatchWithArt = false;

            for (int p = 0; p != list.size(); ++p)
            {
                ProgramInfo *pginfo = list[p]->GetData().value<ProgramInfo *>();

                if (pginfo && (QString::compare(pginfo->GetTitle(), list[p]->GetBaseTitle(), Qt::CaseInsensitive)) == 0)
                {
                    bool hasArtwork = (((list[p]->GetArtwork(kArtworkFanart)).size() != 0) ||
                                       ((list[p]->GetArtwork(kArtworkCoverart)).size() != 0) ||
                                       ((list[p]->GetArtwork(kArtworkBanner)).size() != 0));

                    // After the first exact match, prefer any more popular one.
                    // Most of the Movie database entries have Popularity fields.
                    // The TV series database generally has no Popularity values specified,
                    // so if none are found so far in the search, pick the most recently
                    // released entry with artwork. Also, if the first exact match had
                    // no artwork, prefer any later exact match with artwork.
                    if ((exactTitleMeta == NULL) ||
                        (hasArtwork &&
                         ((!foundMatchWithArt) ||
                          ((list[p]->GetPopularity() > exactTitlePopularity)) ||
                          ((exactTitlePopularity == 0.0) && (list[p]->GetReleaseDate() > exactTitleDate)))))
                    {
                        // remember the most popular or most recently released exact match
                        exactTitleDate = list[p]->GetReleaseDate();
                        exactTitlePopularity = list[p]->GetPopularity();
                        exactTitleMeta = list[p];
                    }
                }

                if (pginfo && !pginfo->GetSeriesID().isEmpty() &&
                    pginfo->GetSeriesID() == (list[p])->GetTMSref())
                {
                    MetadataLookup *lookup = list[p];
                    if (lookup->GetSubtype() != kProbableGenericTelevision)
                        pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
                    pginfo->SaveInetRef(lookup->GetInetref());
                    m_busyRecList.removeAll(pginfo);
                    return;
                }
                else if (pginfo && pginfo->GetYearOfInitialRelease() != 0 &&
                         (list[p])->GetYear() != 0 &&
                         pginfo->GetYearOfInitialRelease() == (list[p])->GetYear())
                {
                    if (yearindex != -1)
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Multiple results matched on year. No definite "
                                      "match could be found.");
                        m_busyRecList.removeAll(pginfo);
                        return;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Matched from multiple results based on year. ");
                        yearindex = p;
                    }
                }
            }

            if (yearindex > -1)
            {
                MetadataLookup *lookup = list[yearindex];
                ProgramInfo *pginfo = lookup->GetData().value<ProgramInfo *>();
                if (lookup->GetSubtype() != kProbableGenericTelevision)
                    pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
                pginfo->SaveInetRef(lookup->GetInetref());
                m_busyRecList.removeAll(pginfo);
                return;
            }

            if (exactTitleMeta != NULL)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Best match released %1").arg(exactTitleDate.toString()));
                MetadataLookup *lookup = exactTitleMeta;
                ProgramInfo *pginfo = exactTitleMeta->GetData().value<ProgramInfo *>();
                if (lookup->GetSubtype() != kProbableGenericTelevision)
                    pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
                pginfo->SaveInetRef(lookup->GetInetref());
                m_busyRecList.removeAll(pginfo);
                return;
            }

            LOG(VB_GENERAL, LOG_INFO, "Unable to match this title, too many possible matches. "
                                      "You may wish to manually set the season, episode, and "
                                      "inetref in the 'Watch Recordings' screen.");

            ProgramInfo *pginfo = list[0]->GetData().value<ProgramInfo *>();

            if (pginfo)
            {
                m_busyRecList.removeAll(pginfo);
            }
        }
    }
    else if (levent->type() == MetadataFactorySingleResult::kEventType)
    {
        MetadataFactorySingleResult *mfsr =
            dynamic_cast<MetadataFactorySingleResult*>(levent);

        if (!mfsr)
            return;

        MetadataLookup *lookup = mfsr->result;

        if (!lookup)
            return;

        ProgramInfo *pginfo = lookup->GetData().value<ProgramInfo *>();

        // This null check could hang us as this pginfo would then never be
        // removed
        if (!pginfo)
            return;

        LOG(VB_GENERAL, LOG_DEBUG, "I found the following data:");
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Input Title: %1").arg(pginfo->GetTitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Input Sub:   %1").arg(pginfo->GetSubtitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Title:       %1").arg(lookup->GetTitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Subtitle:    %1").arg(lookup->GetSubtitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Season:      %1").arg(lookup->GetSeason()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Episode:     %1").arg(lookup->GetEpisode()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Inetref:     %1").arg(lookup->GetInetref()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        User Rating: %1").arg(lookup->GetUserRating()));

        if (lookup->GetSubtype() != kProbableGenericTelevision)
            pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
        pginfo->SaveInetRef(lookup->GetInetref());

        if (m_updaterules)
        {
            RecordingRule *rule = new RecordingRule();
            if (rule)
            {
                rule->LoadByProgram(pginfo);
                if (rule->m_inetref.isEmpty() &&
                    (rule->m_searchType == kNoSearch))
                {
                    rule->m_inetref = lookup->GetInetref();
                }
                rule->m_season = lookup->GetSeason();
                rule->m_episode = lookup->GetEpisode();
                rule->Save();

                delete rule;
            }
        }

        if (m_updateartwork)
        {
            ArtworkMap map = lookup->GetDownloads();
            SetArtwork(lookup->GetInetref(),
                       lookup->GetIsCollection() ? 0 : lookup->GetSeason(),
                       gCoreContext->GetMasterHostName(), map);
        }

        m_busyRecList.removeAll(pginfo);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        MetadataFactoryNoResult *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);

        if (!mfnr)
            return;

        MetadataLookup *lookup = mfnr->result;

        if (!lookup)
            return;

        ProgramInfo *pginfo = lookup->GetData().value<ProgramInfo *>();

        // This null check could hang us as this pginfo would then never be removed
        if (!pginfo)
            return;

        m_busyRecList.removeAll(pginfo);
    }
}
