#include "compat.h"
#include "mythdisplay.h"
#include "mythmainwindow.h"

#include <QApplication>
#include <QDesktopWidget>

#if defined(Q_OS_MAC)
#import "util-osx.h"
#elif USING_X11
#include "mythxdisplay.h"
#endif

#ifdef Q_OS_ANDROID
#include <QtAndroidExtras>
#include <mythlogging.h>
#define LOC      QString("DispInfo: ")
#endif

#define VALID_RATE(rate) (rate > 20.0 && rate < 200.0)

static float fix_rate(int video_rate)
{
    static const float default_rate = 1000000.0f / 60.0f;
    float fixed = default_rate;
    if (video_rate > 0)
    {
        fixed = video_rate / 2;
        if (fixed < default_rate)
            fixed = default_rate;
    }
    return fixed;
}

WId MythDisplay::GetWindowID(void)
{
    WId win = 0;
    QWidget *main_widget = (QWidget*)MythMainWindow::getMainWindow();
    if (main_widget)
        win = main_widget->winId();
    return win;
}

DisplayInfo MythDisplay::GetDisplayInfo(int video_rate)
{
    DisplayInfo ret;

#if defined(Q_OS_MAC)
    CGDirectDisplayID disp = GetOSXDisplay(GetWindowID());
    if (!disp)
        return ret;

    CFDictionaryRef ref = CGDisplayCurrentMode(disp);
    float rate = 0.0f;
    if (ref)
        rate = get_float_CF(ref, kCGDisplayRefreshRate);

    if (VALID_RATE(rate))
        ret.rate = 1000000.0f / rate;
    else
        ret.rate = fix_rate(video_rate);

    CGSize size_in_mm = CGDisplayScreenSize(disp);
    ret.size = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);

    uint width  = (uint)CGDisplayPixelsWide(disp);
    uint height = (uint)CGDisplayPixelsHigh(disp);
    ret.res     = QSize(width, height);

#elif defined(Q_OS_WIN)
    HDC hdc = GetDC((HWND)GetWindowID());
    int rate = 0;
    if (hdc)
    {
        rate       = GetDeviceCaps(hdc, VREFRESH);
        int width  = GetDeviceCaps(hdc, HORZSIZE);
        int height = GetDeviceCaps(hdc, VERTSIZE);
        ret.size   = QSize((uint)width, (uint)height);
        width      = GetDeviceCaps(hdc, HORZRES);
        height     = GetDeviceCaps(hdc, VERTRES);
        ret.res    = QSize((uint)width, (uint)height);
    }

    if (VALID_RATE(rate))
    {
        // see http://support.microsoft.com/kb/2006076
        switch (rate)
        {
            case 23:  ret.rate = 41708; break; // 23.976Hz
            case 29:  ret.rate = 33367; break; // 29.970Hz
            case 47:  ret.rate = 20854; break; // 47.952Hz
            case 59:  ret.rate = 16683; break; // 59.940Hz
            case 71:  ret.rate = 13903; break; // 71.928Hz
            case 119: ret.rate = 8342;  break; // 119.880Hz
            default:  ret.rate = 1000000.0f / (float)rate;
        }
    }
    else
        ret.rate = fix_rate(video_rate);

#elif USING_X11
    MythXDisplay *disp = OpenMythXDisplay();
    if (!disp)
        return ret;

    float rate = disp->GetRefreshRate();
    if (VALID_RATE(rate))
        ret.rate = 1000000.0f / rate;
    else
        ret.rate = fix_rate(video_rate);
    ret.res  = disp->GetDisplaySize();
    ret.size = disp->GetDisplayDimensions();
    delete disp;
#elif defined(Q_OS_ANDROID)
    QAndroidJniEnvironment env;
    QAndroidJniObject activity = QtAndroid::androidActivity();
    QAndroidJniObject windowManager =  activity.callObjectMethod("getWindowManager", "()Landroid/view/WindowManager;");
    QAndroidJniObject display =  windowManager.callObjectMethod("getDefaultDisplay", "()Landroid/view/Display;");
    QAndroidJniObject displayMetrics("android/util/DisplayMetrics");
    display.callMethod<void>("getRealMetrics", "(Landroid/util/DisplayMetrics;)V", displayMetrics.object());
    // check if passed or try a different method
    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        display.callMethod<void>("getMetrics", "(Landroid/util/DisplayMetrics;)V", displayMetrics.object());
    }
    float xdpi = displayMetrics.getField<jfloat>("xdpi");
    float ydpi = displayMetrics.getField<jfloat>("ydpi");
    int height = displayMetrics.getField<jint>("heightPixels");
    int width = displayMetrics.getField<jint>("widthPixels");
    float rate = display.callMethod<jfloat>("getRefreshRate");
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("rate:%1 h:%2 w:%3 xdpi:%4 ydpi:%5")
        .arg(rate).arg(height).arg(width)
        .arg(xdpi).arg(ydpi)
        );

    if (VALID_RATE(rate))
        ret.rate = 1000000.0f / rate;
    else
        ret.rate = fix_rate(video_rate);
    ret.res = QSize((uint)width, (uint)height);
    ret.size = QSize((uint)width, (uint)height);
    if (xdpi > 0 && ydpi > 0)
        ret.size = QSize((uint)width/xdpi*25.4, (uint)height/ydpi*25.4);
#endif
    return ret;
}

int MythDisplay::GetNumberXineramaScreens(void)
{
    int nr_xinerama_screens = 0;
    QDesktopWidget *m_desktop = QApplication::desktop();
    if (m_desktop) {
        nr_xinerama_screens = m_desktop->numScreens();
    }
    return nr_xinerama_screens;
}
