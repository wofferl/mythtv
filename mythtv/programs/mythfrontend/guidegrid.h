// -*- Mode: c++ -*-
#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

// c++
#include <vector>
using namespace std;

// qt
#include <QString>
#include <QDateTime>
#include <QEvent>
#include <QLinkedList>

// myth
#include "mythscreentype.h"
#include "programinfo.h"
#include "channelgroup.h"
#include "channelutil.h"
#include "mythuiguidegrid.h"
#include "mthreadpool.h"
#include "tv_play.h"

// mythfrontend
#include "schedulecommon.h"

using namespace std;

class ProgramInfo;
class QTimer;
class MythUIButtonList;
class MythUIGuideGrid;

typedef vector<ChannelInfo>   db_chan_list_t;
typedef vector<db_chan_list_t> db_chan_list_list_t;
typedef ProgramInfo *ProgInfoGuideArray[MAX_DISPLAY_CHANS][MAX_DISPLAY_TIMES];

class JumpToChannel;
class JumpToChannelListener
{
  public:
    virtual ~JumpToChannelListener() {}
    virtual void GoTo(int start, int cur_row) = 0;
    virtual void SetJumpToChannel(JumpToChannel *ptr) = 0;
    virtual int  FindChannel(uint chanid, const QString &channum,
                             bool exact = true) const = 0;
};

class JumpToChannel : public QObject
{
    Q_OBJECT

  public:
    JumpToChannel(
        JumpToChannelListener *parent, const QString &start_entry,
        int start_chan_idx, int cur_chan_idx, uint rows_disp);

    bool ProcessEntry(const QStringList &actions, const QKeyEvent *e);

    QString GetEntry(void) const { return m_entry; }

  public slots:
    virtual void deleteLater(void);

  private:
    ~JumpToChannel() {}
    bool Update(void);

  private:
    JumpToChannelListener *m_listener;
    QString  m_entry;
    int      m_previous_start_channel_index;
    int      m_previous_current_channel_index;
    uint     m_rows_displayed;
    QTimer  *m_timer; // audited ref #5318

    static const uint kJumpToChannelTimeout = 3500; // ms
};

// GuideUIElement encapsulates the arguments to
// MythUIGuideGrid::SetProgramInfo().  The elements are prepared in a
// background thread and then sent via an event to the UI thread for
// rendering.
class GuideUIElement {
public:
    GuideUIElement(int row, int col, const QRect &area,
                   const QString &title, const QString &category,
                   int arrow, int recType, int recStat, bool selected)
        : m_row(row), m_col(col), m_area(area), m_title(title),
          m_category(category), m_arrow(arrow), m_recType(recType),
          m_recStat(recStat), m_selected(selected) {}
    const int m_row;
    const int m_col;
    const QRect m_area;
    const QString m_title;
    const QString m_category;
    const int m_arrow;
    const int m_recType;
    const int m_recStat;
    const bool m_selected;
};

class GuideGrid : public ScheduleCommon, public JumpToChannelListener
{
    Q_OBJECT

  public:
    // Use this function to instantiate a guidegrid instance.
    static void RunProgramGuide(uint           startChanId,
                                const QString &startChanNum,
                                const QDateTime &startTime,
                                TV            *player = NULL,
                                bool           embedVideo = false,
                                bool           allowFinder = true,
                                int            changrpid = -1);

    ChannelInfoList GetSelection(void) const;

    virtual void GoTo(int start, int cur_row);
    virtual void SetJumpToChannel(JumpToChannel *ptr);

    bool Create(void);
    virtual void Load(void);
    virtual void Init(void);
    bool keyPressEvent(QKeyEvent *event);
    bool gestureEvent(MythGestureEvent *event);

    virtual void aboutToShow();
    virtual void aboutToHide();
    // Allow class GuideUpdateProgramRow to figure out whether the
    // current start time/channel coordinates are the same, so that it can
    // skip the work if not.
    uint GetCurrentStartChannel(void) const { return m_currentStartChannel; }
    QDateTime GetCurrentStartTime(void) const { return m_currentStartTime; }

  protected slots:
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();

    void toggleGuideListing();
    void toggleChannelFavorite(int grpid = -1);
    void ChannelGroupMenu(int mode = 0);
    void generateListings();

    void enter();

    void showProgFinder();
    void channelUpdate();
    void volumeUpdate(bool);
    void toggleMute(const bool muteIndividualChannels = false);

    void deleteRule();

    void Close();
    void customEvent(QEvent *event);

  protected:
    GuideGrid(MythScreenStack *parentStack,
              uint chanid, const QString &channum,
              const QDateTime &startTime,
              TV *player = NULL,
              bool embedVideo = false,
              bool allowFinder = true,
              int changrpid = -1);
   ~GuideGrid();
    virtual ProgramInfo *GetCurrentProgram(void) const
        { return m_programInfos[m_currentRow][m_currentCol]; };

  private slots:
    void updateTimeout(void);
    void refreshVideo(void);
    void updateInfo(void);
    void updateChannels(void);
    void updateJumpToChannel(void);

  private:

    enum MoveVector {
        kScrollUp,
        kScrollDown,
        kScrollLeft,
        kScrollRight,
        kPageUp,
        kPageDown,
        kPageLeft,
        kPageRight,
        kDayLeft,
        kDayRight,
    };

    void moveLeftRight(MoveVector movement);
    void moveUpDown(MoveVector movement);
    void moveToTime(QDateTime datetime);

    void ShowMenu(void);
    void ShowRecordingMenu(void);
    void ShowJumpToTime(void);

    int  FindChannel(uint chanid, const QString &channum,
                     bool exact = true) const;

    void fillChannelInfos(bool gotostartchannel = true);
    void fillTimeInfos(void);
    void fillProgramInfos(bool useExistingData = false);
    // Set row=-1 to fill all rows.
    void fillProgramRowInfos(int row, bool useExistingData);
public:
    // These need to be public so that the helper classes can operate.
    ProgramList *getProgramListFromProgram(int chanNum);
    void updateProgramsUI(unsigned int firstRow, unsigned int numRows,
                          int progPast,
                          const QVector<ProgramList*> &proglists,
                          const ProgInfoGuideArray &programInfos,
                          const QLinkedList<GuideUIElement> &elements);
    void updateChannelsNonUI(QVector<ChannelInfo *> &chinfos,
                             QVector<bool> &unavailables);
    void updateChannelsUI(const QVector<ChannelInfo *> &chinfos,
                          const QVector<bool> &unavailables);
private:

    void setStartChannel(int newStartChannel);

    ChannelInfo       *GetChannelInfo(uint chan_idx, int sel = -1);
    const ChannelInfo *GetChannelInfo(uint chan_idx, int sel = -1) const;
    uint                 GetChannelCount(void) const;
    int                  GetStartChannelOffset(int row = -1) const;

    ProgramList GetProgramList(uint chanid) const;
    uint GetAlternateChannelIndex(uint chan_idx, bool with_same_channum) const;
    void updateDateText(void);

  private:
    int   m_selectRecThreshold;

    bool m_allowFinder;
    db_chan_list_list_t m_channelInfos;
    QMap<uint,uint>      m_channelInfoIdx;

    vector<ProgramList*> m_programs;
    ProgInfoGuideArray m_programInfos;
    ProgramList  m_recList;

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    QDateTime m_currentEndTime;
    uint      m_currentStartChannel;
    uint      m_startChanID;
    QString   m_startChanNum;

    int m_currentRow;
    int m_currentCol;

    bool    m_sortReverse;

    int  m_channelCount;
    int  m_timeCount;
    bool m_verticalLayout;

    QDateTime m_firstTime;
    QDateTime m_lastTime;

    TV     *m_player;
    bool    m_usingNullVideo;
    bool    m_embedVideo;
    QTimer *m_previewVideoRefreshTimer; // audited ref #5318
    void    EmbedTVWindow(void);
    void    HideTVWindow(void);
    QRect   m_videoRect;

    QString m_channelOrdering;

    QTimer *m_updateTimer; // audited ref #5318

    MThreadPool       m_threadPool;

    int               m_changrpid;
    ChannelGroupList  m_changrplist;

    QMutex            m_jumpToChannelLock;
    JumpToChannel    *m_jumpToChannel;

    MythUIButtonList *m_timeList;
    MythUIButtonList *m_channelList;
    MythUIGuideGrid  *m_guideGrid;
    MythUIText       *m_dateText;
    MythUIText       *m_longdateText;
    MythUIText       *m_jumpToText;
    MythUIText       *m_changroupname;
    MythUIImage      *m_channelImage;
};

#endif
