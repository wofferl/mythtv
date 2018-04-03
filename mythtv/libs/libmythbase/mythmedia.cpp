// C header
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

// Qt Headers
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QTextStream>

// MythTV headers
#include "mythmedia.h"
#include "mythconfig.h"
#include "mythlogging.h"
#include "mythmiscutil.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"

using namespace std;

#ifdef _WIN32
#   define O_NONBLOCK 0
#endif

#define LOC QString("MythMediaDevice:")

static const QString PATHTO_PMOUNT("/usr/bin/pmount");
static const QString PATHTO_PUMOUNT("/usr/bin/pumount");
#if CONFIG_DARWIN
    static const QString PATHTO_MOUNT("/sbin/mount");
#else
    static const QString PATHTO_MOUNT("/bin/mount");
#endif
static const QString PATHTO_UNMOUNT("/bin/umount");
static const QString PATHTO_MOUNTS("/proc/mounts");

#if CONFIG_DARWIN
#   define USE_MOUNT_COMMAND
#endif

const char* MythMediaDevice::MediaStatusStrings[] =
{
    "MEDIASTAT_ERROR",
    "MEDIASTAT_UNKNOWN",
    "MEDIASTAT_UNPLUGGED",
    "MEDIASTAT_OPEN",
    "MEDIASTAT_NODISK",
    "MEDIASTAT_UNFORMATTED",
    "MEDIASTAT_USEABLE",
    "MEDIASTAT_NOTMOUNTED",
    "MEDIASTAT_MOUNTED"
};

const char* MythMediaDevice::MediaErrorStrings[] =
{
    "MEDIAERR_OK",
    "MEDIAERR_FAILED",
    "MEDIAERR_UNSUPPORTED"
};

QEvent::Type MythMediaEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

ext_to_media_t MythMediaDevice::s_ext_to_media;

MythMediaDevice::MythMediaDevice(QObject* par, const char* DevicePath,
                                 bool SuperMount,  bool AllowEject)
               : QObject(par)
{
    m_DevicePath = DevicePath;
    m_AllowEject = AllowEject;
    m_Locked = false;
    m_DeviceHandle = -1;
    m_SuperMount = SuperMount;
    m_Status = MEDIASTAT_UNKNOWN;
    m_MediaType = MEDIATYPE_UNKNOWN;
    m_RealDevice = getSymlinkTarget(m_DevicePath);
}

bool MythMediaDevice::openDevice()
{
    // Sanity check
    if (isDeviceOpen())
        return true;

    QByteArray dev = m_DevicePath.toLocal8Bit();
    m_DeviceHandle = open(dev.constData(), O_RDONLY | O_NONBLOCK);

    return isDeviceOpen();
}

bool MythMediaDevice::closeDevice()
{
    // Sanity check
    if (!isDeviceOpen())
        return true;

    int ret = close(m_DeviceHandle);
    m_DeviceHandle = -1;

    return (ret != -1) ? true : false;
}

bool MythMediaDevice::isDeviceOpen() const
{
    return (m_DeviceHandle >= 0) ? true : false;
}

bool MythMediaDevice::performMountCmd(bool DoMount)
{
    if (DoMount && isMounted())
    {
#ifdef Q_OS_MAC
        // Not an error - DiskArbitration has already mounted the device.
        // AddDevice calls mount() so onDeviceMounted() can get mediaType.
        onDeviceMounted();
#else
        LOG(VB_MEDIA, LOG_ERR, "MythMediaDevice::performMountCmd(true)"
                               " - Logic Error? Device already mounted.");
        return true;
#endif
    }

    if (isDeviceOpen())
        closeDevice();

    if (!m_SuperMount)
    {
        QString MountCommand;

        // Build a command line for mount/unmount and execute it...
        // Is there a better way to do this?
        if (QFile(PATHTO_PMOUNT).exists() && QFile(PATHTO_PUMOUNT).exists())
            MountCommand = QString("%1 %2")
                .arg((DoMount) ? PATHTO_PMOUNT : PATHTO_PUMOUNT)
                .arg(m_DevicePath);
        else
            MountCommand = QString("%1 %2")
                .arg((DoMount) ? PATHTO_MOUNT : PATHTO_UNMOUNT)
                .arg(m_DevicePath);

        LOG(VB_MEDIA, LOG_INFO, QString("Executing '%1'").arg(MountCommand));
        int ret = myth_system(MountCommand, kMSDontBlockInputDevs);
        if (ret !=  GENERIC_EXIT_OK)
        {
            usleep(300000);
            LOG(VB_MEDIA, LOG_INFO, QString("Retrying '%1'").arg(MountCommand));
            ret = myth_system(MountCommand, kMSDontBlockInputDevs);
        }
        if (ret == GENERIC_EXIT_OK)
        {
            if (DoMount)
            {
                // we cannot tell beforehand what the pmount mount point is
                // so verify the mount status of the device
                // In the case that m_DevicePath is a symlink to a device
                // in /etc/fstab then pmount delegates to mount which
                // performs the mount asynchronously so we must wait a bit
                usleep(1000000-1);
                for (int tries = 2; !findMountPath() && tries > 0; --tries)
                {
                    LOG(VB_MEDIA, LOG_INFO,
                        QString("Repeating '%1'").arg(MountCommand));
                    myth_system(MountCommand, kMSDontBlockInputDevs);
                    usleep(500000);
                }
                if (!findMountPath())
                {
                    LOG(VB_MEDIA, LOG_ERR, "performMountCmd() attempted to"
                                           " find mounted media, but failed?");
                    return false;
                }
                onDeviceMounted(); // Identify disk type & content
                LOG(VB_GENERAL, LOG_INFO,
                        QString("Detected MediaType ") + MediaTypeString());
            }
            else
                onDeviceUnmounted();

            return true;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to %1 %2.")
                    .arg(DoMount ? "mount" : "unmount").arg(m_DevicePath));
    }
    else
    {
        LOG(VB_MEDIA, LOG_INFO, "Disk inserted on a supermount device");
        // If it's a super mount then the OS will handle mounting /  unmounting.
        // We just need to give derived classes a chance to perform their
        // mount / unmount logic.
        if (DoMount)
        {
            onDeviceMounted();
            LOG(VB_GENERAL, LOG_INFO,
                    QString("Detected MediaType ") + MediaTypeString());
        }
        else
            onDeviceUnmounted();

        return true;
    }
    return false;
}

/** \fn MythMediaDevice::DetectMediaType(void)
 *  \brief Returns guessed media type based on file extensions.
 */
MythMediaType MythMediaDevice::DetectMediaType(void)
{
    ext_cnt_t ext_cnt;

    if (!ScanMediaType(m_MountPath, ext_cnt))
    {
        LOG(VB_MEDIA, LOG_NOTICE,
            QString("No files with extensions found in '%1'")
                .arg(m_MountPath));
        return MEDIATYPE_UNKNOWN;
    }

    QMap<uint, uint> media_cnts;

    // convert raw counts to composite mediatype counts
    ext_cnt_t::const_iterator it = ext_cnt.begin();
    for (; it != ext_cnt.end(); ++it)
    {
        ext_to_media_t::const_iterator found = s_ext_to_media.find(it.key());
        if (found != s_ext_to_media.end())
        {
            LOG(VB_MEDIA, LOG_INFO, QString("DetectMediaType %1 (%2)")
                .arg(MediaTypeString(found.value())).arg(it.key()));
            media_cnts[*found] += *it;
        }
        else
        {
            LOG(VB_MEDIA, LOG_NOTICE, QString(
                    "DetectMediaType(this=0x%1) unknown file type %1")
                .arg(quintptr(this),0,16).arg(it.key()));
        }
    }

    // break composite mediatypes into constituent components
    uint mediatype = 0;

    QMap<uint, uint>::const_iterator cit = media_cnts.begin();
    for (; cit != media_cnts.end(); ++cit)
    {
        for (uint key = 1; key != MEDIATYPE_END; key <<= 1)
        {
            if (key & cit.key())
                mediatype |= key;
        }
    }

    return mediatype ? (MythMediaType)mediatype : MEDIATYPE_UNKNOWN;
}

/**
 *  \brief Recursively scan directories and create an associative array
 *         with the number of times we've seen each extension.
 */
bool MythMediaDevice::ScanMediaType(const QString &directory, ext_cnt_t &cnt)
{
    QDir d(directory);
    if (!d.exists())
        return false;

    d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = d.entryInfoList();

    for( QFileInfoList::iterator it = list.begin();
                                 it != list.end();
                               ++it )
    {
        QFileInfo &fi = *it;

        if (fi.isSymLink())
            continue;

        if (fi.isDir())
        {
            ScanMediaType(fi.absoluteFilePath(), cnt);
            continue;
        }

        const QString ext = fi.suffix();
        if (!ext.isEmpty())
            cnt[ext.toLower()]++;
    }

    return !cnt.empty();
}

/** \fn MythMediaDevice::RegisterMediaExtensions(uint,const QString&)
 *  \brief Used to register media types with extensions.
 *
 *  \param mediatype  MythMediaType flag.
 *  \param extensions Comma separated list of extensions like 'mp3,ogg,flac'.
 */
// static
void MythMediaDevice::RegisterMediaExtensions(uint mediatype,
                                              const QString &extensions)
{
    const QStringList list = extensions.split(",");
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        s_ext_to_media[*it] |= mediatype;
}

MythMediaError MythMediaDevice::eject(bool open_close)
{
    (void) open_close;

#if CONFIG_DARWIN
    QString  command = "diskutil eject " + m_DevicePath;

    int ret = myth_system(command, kMSRunBackground);
    return MEDIAERR_OK;
#endif

    return MEDIAERR_UNSUPPORTED;
}

bool MythMediaDevice::isSameDevice(const QString &path)
{
#ifdef Q_OS_MAC
    // The caller may be using a raw device instead of the BSD 'leaf' name
    if (path == "/dev/r" + m_DevicePath)
        return true;
#endif

    return (path == m_DevicePath);
}

void MythMediaDevice::setSpeed(int speed)
{
    setDeviceSpeed(m_DevicePath.toLocal8Bit().constData(), speed);
}

MythMediaError MythMediaDevice::lock()
{
    // We just open the device here, which may or may not do the trick,
    // derived classes can do more...
    if (openDevice())
    {
        m_Locked = true;
        return MEDIAERR_OK;
    }
    m_Locked = false;
    return MEDIAERR_FAILED;
}

MythMediaError MythMediaDevice::unlock()
{
    m_Locked = false;

    return MEDIAERR_OK;
}

/// \brief Tells us if m_DevicePath is a mounted device.
bool MythMediaDevice::isMounted(bool Verify)
{
    if (Verify)
        return findMountPath();
    else
        return (m_Status == MEDIASTAT_MOUNTED);
}

/// \brief Try to find a mount of m_DevicePath in the mounts file.
bool MythMediaDevice::findMountPath()
{
    if (m_DevicePath.isEmpty())
    {
        LOG(VB_MEDIA, LOG_ERR, "findMountPath() - logic error, no device path");
        return false;
    }

#ifdef USE_MOUNT_COMMAND
    // HACK. TODO: replace with something using popen()?
    if (myth_system(PATHTO_MOUNT + " > /tmp/mounts") != GENERIC_EXIT_OK)
        return false;
    QFile mountFile("/tmp/mounts");
#else
    QFile mountFile(PATHTO_MOUNTS);
#endif

    // Try to open the mounts file so we can search it for our device.
    if (!mountFile.open(QIODevice::ReadOnly))
        return false;

    QString     debug;
    QTextStream stream(&mountFile);

    for (;;)
    {
        QString mountPoint;
        QString deviceName;


#ifdef USE_MOUNT_COMMAND
        // Extract mount point and device name from something like:
        //   /dev/disk0s3 on / (hfs, local, journaled)   - Mac OS X
        //   /dev/hdd on /tmp/AAA BBB type udf (ro)      - Linux
        stream >> deviceName;
        mountPoint = stream.readLine();
        mountPoint.remove(" on ");
        mountPoint.remove(QRegExp(" type \\w.*"));   // Linux
        mountPoint.remove(QRegExp(" \\(\\w.*"));     // Mac OS X
#else
        // Extract the mount point and device name.
        stream >> deviceName >> mountPoint;
        stream.readLine(); // skip the rest of the line
#endif

        if (deviceName.isNull())
            break;

        if (deviceName.isEmpty())
            continue;

        if (!deviceName.startsWith("/dev/"))
            continue;

        QStringList deviceNames;
        getSymlinkTarget(deviceName, &deviceNames);

#if CONFIG_DARWIN
        // match short-style BSD node names:
        if (m_DevicePath.startsWith("disk"))
            deviceNames << deviceName.mid(5);   // remove 5 chars - /dev/
#endif

        // Deal with escaped spaces
        if (mountPoint.contains("\\040"))
            mountPoint.replace("\\040", " ");


        if (deviceNames.contains(m_DevicePath) ||
            deviceNames.contains(m_RealDevice)  )
        {
            m_MountPath = mountPoint;
            mountFile.close();
            return true;
        }

        if (VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_DEBUG))
            debug += QString("                 %1 | %2\n")
                     .arg(deviceName, 16).arg(mountPoint);
    }

    mountFile.close();

    if (VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_DEBUG))
    {
        debug = LOC + ":findMountPath() - mount of '"
                + m_DevicePath + "' not found.\n"
                + "                 Device name/type | Current mountpoint\n"
                + "                 -----------------+-------------------\n"
                + debug
                + "                 =================+===================";
        LOG(VB_MEDIA, LOG_DEBUG, debug);
    }

    return false;
}

MythMediaStatus MythMediaDevice::setStatus( MythMediaStatus NewStatus,
                                            bool CloseIt )
{
    MythMediaStatus OldStatus = m_Status;

    m_Status = NewStatus;

    // If the status is changed we need to take some actions
    // depending on the old and new status.
    if (NewStatus != OldStatus)
    {
        LOG(VB_MEDIA, LOG_DEBUG,
            QString("MythMediaDevice::setStatus %1 %2->%3")
            .arg(getDevicePath()).arg(MediaStatusStrings[OldStatus])
            .arg(MediaStatusStrings[NewStatus]));
        switch (NewStatus)
        {
            // the disk is not / should not be mounted.
            case MEDIASTAT_ERROR:
            case MEDIASTAT_OPEN:
            case MEDIASTAT_NODISK:
            case MEDIASTAT_NOTMOUNTED:
                if (isMounted())
                    unmount();
                break;
            case MEDIASTAT_UNKNOWN:
            case MEDIASTAT_USEABLE:
            case MEDIASTAT_MOUNTED:
            case MEDIASTAT_UNPLUGGED:
            case MEDIASTAT_UNFORMATTED:
                // get rid of the compiler warning...
                break;
        }

        // Don't fire off transitions to / from unknown states
        if (m_Status != MEDIASTAT_UNKNOWN && OldStatus != MEDIASTAT_UNKNOWN)
            emit statusChanged(OldStatus, this);
    }


    if (CloseIt)
        closeDevice();

    return m_Status;
}

void MythMediaDevice::clearData()
{
    m_VolumeID.clear();
    m_KeyID.clear();
    m_MediaType = MEDIATYPE_UNKNOWN;
}

QString MythMediaDevice::MediaTypeString()
{
    return MediaTypeString(m_MediaType);
}

QString MythMediaDevice::MediaTypeString(uint type)
{
    // MediaType is a bitmask.
    QString mediatype;
    for (uint u = MEDIATYPE_UNKNOWN; u != MEDIATYPE_END; u <<= 1)
    {
        QString s;
        if (u & type & MEDIATYPE_UNKNOWN)
            s = "MEDIATYPE_UNKNOWN";
        else if (u & type & MEDIATYPE_DATA)
            s = "MEDIATYPE_DATA";
        else if (u & type & MEDIATYPE_MIXED)
            s = "MEDIATYPE_MIXED";
        else if (u & type & MEDIATYPE_AUDIO)
            s = "MEDIATYPE_AUDIO";
        else if (u & type & MEDIATYPE_DVD)
            s = "MEDIATYPE_DVD";
        else if (u & type & MEDIATYPE_BD)
            s = "MEDIATYPE_BD";
        else if (u & type & MEDIATYPE_VCD)
            s = "MEDIATYPE_VCD";
        else if (u & type & MEDIATYPE_MMUSIC)
            s = "MEDIATYPE_MMUSIC";
        else if (u & type & MEDIATYPE_MVIDEO)
            s = "MEDIATYPE_MVIDEO";
        else if (u & type & MEDIATYPE_MGALLERY)
            s = "MEDIATYPE_MGALLERY";
        else
            continue;

        if (mediatype.isEmpty())
            mediatype = s;
        else
            mediatype += "|" + s;
    }

    return mediatype;
}
