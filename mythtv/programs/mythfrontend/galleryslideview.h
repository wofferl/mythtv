//! \file
//! \brief Slideshow screen

#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

#include "gallerytransitions.h"
#include "galleryinfo.h"


class MythMenu;
class FlatView;

//! Type of slide show
enum ImageSlideShowType {
    kBrowseSlides       = 0,
    kNormalSlideShow    = 1,
    kRecursiveSlideShow = 2
};


//! Slideshow screen
class GallerySlideView : public MythScreenType
{
    Q_OBJECT
public:
    GallerySlideView(MythScreenStack *parent, const char *name, bool editsAllowed);
    ~GallerySlideView();
    bool Create();

public slots:
    void Start(ImageSlideShowType type, int parentId, int selectedId = 0);
    void Close();
    void Pulse();

signals:
    void ImageSelected(int);

private:
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *);
    void MenuMain();
    void MenuTransforms(MythMenu &);
    void Suspend();
    void Release();
    void Transform(ImageFileTransform);
    void Zoom(int = 0);
    void Pan(QPoint = QPoint(0, 0));
    void SetStatus(QString msg, bool delay = false);
    void ClearStatus(Slide &next);

private slots:
    void ShowPrevSlide(int inc = 1);
    void ShowNextSlide(int inc = 1, bool useTransition = true);
    void SlideAvailable(int count);
    void TransitionComplete();
    void ShowSlide(int direction = 0);
    void Stop();
    void Play(bool useTransition = true);
    void RepeatOn(int on = 1)   { gCoreContext->SaveSetting("GalleryRepeat", on); }
    void RepeatOff()            { RepeatOn(0); }
    void ShowInfo();
    void HideInfo();
    void ShowCaptions();
    void HideCaptions();
    void PlayVideo();
    void ShowStatus();

private:
    // Theme widgets
    MythUIImage *m_uiImage;
    MythUIText  *m_uiStatus, *m_uiSlideCount, *m_uiCaptionText, *m_uiHideCaptions;

    ImageManagerFe &m_mgr;  //!< Manages the images
    FlatView       *m_view; //!< List of images comprising the slideshow

    TransitionRegistry m_availableTransitions; //!< Transitions available
    Transition        &m_transition;           //!< Selected transition
    //! Instant transition that is always used for start-up & image updates
    TransitionNone     m_updateTransition;

    SlideBuffer m_slides;        //!< A queue of slides used to display images.
    InfoList    m_infoList;      //!< Image details overlay
    int         m_slideShowTime; //!< Time to display a slide in a slideshow
    QTimer      m_timer;         //!< Slide duration timer
    QTimer      m_delay;         //!< Status delay timer
    QString     m_statusText;    //!< Text to display as status
    bool        m_playing;       //!< True when slideshow is running
    bool        m_suspended;     //!< True when transition is running or video playing
    bool        m_showCaptions;  //!< If true, captions are shown
    bool        m_transitioning; //!< True when a transition is in progress
    bool        m_editsAllowed;  //!< True when edits are enabled
};

#endif // GALLERYWIDGET_H
