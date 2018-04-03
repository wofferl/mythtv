#ifndef _TV_REMOTE_UTIL_H_
#define _TV_REMOTE_UTIL_H_

#include <QStringList>
#include <QDateTime>

#include <vector>
using namespace std;

#include "mythtvexp.h"

class ProgramInfo;
class RemoteEncoder;
class InputInfo;

/// recording status stuff
class TunerStatus
{
  public:
    uint      id;
    bool      isRecording;
    QString   channame;
    QString   title;
    QString   subtitle;
    QDateTime startTime;
    QDateTime endTime;
};

MTV_PUBLIC uint RemoteGetState(uint inputid);
MTV_PUBLIC uint RemoteGetFlags(uint inputid);
MTV_PUBLIC bool RemoteRecordPending(
    uint inputid, const ProgramInfo *pginfo, int secsleft, bool hasLater);
MTV_PUBLIC bool RemoteStopLiveTV(uint inputid);
MTV_PUBLIC bool RemoteStopRecording(uint inputid);
MTV_PUBLIC void RemoteStopRecording(const ProgramInfo *pginfo);
MTV_PUBLIC void RemoteCancelNextRecording(uint inputid, bool cancel);
vector<InputInfo>
RemoteRequestFreeInputInfo(uint excluded_input);
MTV_PUBLIC int RemoteGetFreeRecorderCount(void);
MTV_PUBLIC RemoteEncoder *RemoteRequestRecorder(void);
MTV_PUBLIC RemoteEncoder *RemoteRequestNextFreeRecorder(int curr);
MTV_PUBLIC RemoteEncoder *RemoteRequestFreeRecorderFromList
(const QStringList &qualifiedRecorders, uint excluded_input);
MTV_PUBLIC RemoteEncoder *RemoteGetExistingRecorder(const ProgramInfo *pginfo);
MTV_PUBLIC RemoteEncoder *RemoteGetExistingRecorder(int recordernum);
MTV_PUBLIC vector<uint>
RemoteRequestFreeRecorderList(uint excluded_input);
MTV_PUBLIC vector<uint>
RemoteRequestFreeInputList(uint excluded_input);
MTV_PUBLIC bool RemoteIsBusy(uint inputid, InputInfo &busy_input);

MTV_PUBLIC bool RemoteGetRecordingStatus(
    vector<TunerStatus> *tunerList = NULL, bool list_inactive = false);

#endif // _TV_REMOTE_UTIL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
