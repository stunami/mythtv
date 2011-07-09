#include <vector>

#include <QList>

#include "programinfo.h"
#include "mythlogging.h"
#include "jobqueue.h"

#include "lookup.h"

LookerUpper::LookerUpper() :
    m_busyRecList(QList<ProgramInfo*>())
{
    m_metadataFactory = new MetadataFactory(this);
}

LookerUpper::~LookerUpper()
{
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
                                        const QDateTime starttime)
{
    ProgramInfo *pginfo = new ProgramInfo(chanid, starttime);

    if (!pginfo)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "No valid program info for supplied chanid/starttime");
        return;
    }

    m_busyRecList.append(pginfo);
    m_metadataFactory->Lookup(pginfo, false, false);
}

void LookerUpper::HandleAllRecordings()
{
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for( int n = 0; n < (int)progList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(progList[n]));
        if (pginfo->GetInetRef().isEmpty() ||
            (!pginfo->GetSubtitle().isEmpty() &&
              (pginfo->GetSeason() == 0) &&
              (pginfo->GetEpisode() == 0)))
        {
            QString msg = QString("Looking up: %1 %2").arg(pginfo->GetTitle())
                                           .arg(pginfo->GetSubtitle());
            LOG(VB_GENERAL, LOG_INFO, msg);

            m_busyRecList.append(pginfo);
            m_metadataFactory->Lookup(pginfo, false, false);
        }
    }
}

void LookerUpper::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        LOG(VB_GENERAL, LOG_INFO, "Unable to match this title, too many possible matches. "
                                  "You may wish to manually set the season, episode, and "
                                  "inetref in the 'Watch Recordings' screen.");

        MetadataFactoryMultiResult *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);

        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->results;

        if (list.count() >= 1)
        {
            ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(list.takeFirst()->GetData());

            if (pginfo)
                m_busyRecList.removeAll(pginfo);
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

        ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(lookup->GetData());

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

        pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
        pginfo->SaveInetRef(lookup->GetInetref());

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

        ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(lookup->GetData());

        // This null check could hang us as this pginfo would then never be removed
        if (!pginfo)
            return;

        m_busyRecList.removeAll(pginfo);
    }
}