/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and Digia.  For licensing terms and
 ** conditions see http://qt.digia.com/licensing.  For further information
 ** use the contact form at http://qt.digia.com/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 2.1 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.LGPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU Lesser General Public License version 2.1 requirements
 ** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Digia gives you certain additional
 ** rights.  These rights are described in the Digia Qt LGPL Exception
 ** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
 **
 ** GNU General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU
 ** General Public License version 3.0 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.GPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU General Public License version 3.0 requirements will be
 ** met: http://www.gnu.org/copyleft/gpl.html.
 **
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include "qwinthumbnailtoolbar.h"
#include "qwinthumbnailtoolbar_p.h"
#include "qwinthumbnailtoolbutton.h"
#include "qwinthumbnailtoolbutton_p.h"
#include "windowsguidsdefs_p.h"

#include <QWindow>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

#include "qwinevent.h"
#include "qwinfunctions.h"
#include "qwineventfilter_p.h"

#ifndef THBN_CLICKED
#  define THBN_CLICKED 0x1800
#endif

QT_BEGIN_NAMESPACE

static const int windowsLimitedThumbbarSize = 7;

/*!
    \class QWinThumbnailToolBar
    \inmodule QtWinExtras
    \since 5.2
    \brief The QWinThumbnailToolBar class allows manipulating the thumbnail toolbar of a window.

    Applications can embed a toolbar in the thumbnail of a window, which is
    shown when hovering over its taskbar icon. A thumbnail toolbar may provide
    quick access to the commands of a window without requiring the user to restore
    or activate the window.

    \image thumbbar.png Media player thumbnail toolbar

    The following example code illustrates how to use the functions in the
    QWinThumbnailToolBar and QWinThumbnailToolButton class to implement a
    thumbnail toolbar:

    \snippet code/thumbbar.cpp thumbbar_cpp

    \sa QWinThumbnailToolButton
 */

/*!
    Constructs a QWinThumbnailToolBar with the specified \a parent.

    If \a parent is an instance of QWindow, it is automatically
    assigned as the thumbnail toolbar's \l window.
 */
QWinThumbnailToolBar::QWinThumbnailToolBar(QObject *parent) :
    QObject(parent), d_ptr(new QWinThumbnailToolBarPrivate)
{
    Q_D(QWinThumbnailToolBar);
    d->q_ptr = this;
    QWinEventFilter::setup();
    setWindow(qobject_cast<QWindow *>(parent));
}

/*!
    Destroys and clears the QWinThumbnailToolBar.
 */
QWinThumbnailToolBar::~QWinThumbnailToolBar()
{
}

/*!
    \property QWinThumbnailToolBar::window
    \brief the window whose thumbnail toolbar is manipulated
 */
void QWinThumbnailToolBar::setWindow(QWindow *window)
{
    Q_D(QWinThumbnailToolBar);
    if (d->window != window) {
        if (d->window) {
            d->window->removeEventFilter(d);
            d->clearToolbar();
        }
        d->window = window;
        if (d->window) {
            d->window->installEventFilter(d);
            if (d->window->isVisible()) {
                d->initToolbar();
                d->_q_scheduleUpdate();
            }
        }
    }
}

QWindow *QWinThumbnailToolBar::window() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->window;
}

/*!
    Adds a \a button to the thumbnail toolbar.

    \note The number of buttons is limited to \c 7.
 */
void QWinThumbnailToolBar::addButton(QWinThumbnailToolButton *button)
{
    Q_D(QWinThumbnailToolBar);
    if (d->buttonList.size() >= windowsLimitedThumbbarSize) {
        qWarning() << "Cannot add " << button << " maximum number of buttons ("
                   << windowsLimitedThumbbarSize << ") reached.";
        return;
    }
    if (button && button->d_func()->toolbar != this) {
        if (button->d_func()->toolbar) {
            button->d_func()->toolbar->removeButton(button);
        }
        button->d_func()->toolbar = this;
        connect(button, SIGNAL(changed()), this, SLOT(_q_scheduleUpdate()));
        d->buttonList.append(button);
        d->_q_scheduleUpdate();
    }
}

/*!
    Removes the \a button from the thumbnail toolbar.
 */
void QWinThumbnailToolBar::removeButton(QWinThumbnailToolButton *button)
{
    Q_D(QWinThumbnailToolBar);
    if (button && d->buttonList.contains(button)) {
        button->d_func()->toolbar = 0;
        disconnect(button, SIGNAL(changed()), this, SLOT(_q_scheduleUpdate()));
        d->buttonList.removeAll(button);
        d->_q_scheduleUpdate();
    }
}

/*!
    Sets the list of \a buttons in the thumbnail toolbar.

    \note Any existing buttons are replaced.
 */
void QWinThumbnailToolBar::setButtons(const QList<QWinThumbnailToolButton *> &buttons)
{
    Q_D(QWinThumbnailToolBar);
    d->buttonList.clear();
    Q_FOREACH (QWinThumbnailToolButton *button, buttons)
        addButton(button);
    d->_q_updateToolbar();
}

/*!
    Returns the list of buttons in the thumbnail toolbar.
 */
QList<QWinThumbnailToolButton *> QWinThumbnailToolBar::buttons() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->buttonList;
}

/*!
    \property QWinThumbnailToolBar::count
    \brief the number of buttons in the thumbnail toolbar

    \note The number of buttons is limited to \c 7.
 */
int QWinThumbnailToolBar::count() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->buttonList.size();
}

/*!
    Removes all buttons from the thumbnail toolbar.
 */
void QWinThumbnailToolBar::clear()
{
    setButtons(QList<QWinThumbnailToolButton *>());
}

static inline ITaskbarList4 *createTaskbarList()
{
    ITaskbarList4 *result = 0;
    HRESULT hresult = CoCreateInstance(CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, qIID_ITaskbarList4, reinterpret_cast<void **>(&result));
    if (FAILED(hresult)) {
        const QString err = QtWin::errorStringFromHresult(hresult);
        qWarning("QWinThumbnailToolBar: qIID_ITaskbarList4 was not created: %#010x, %s.",
                 (unsigned)hresult, qPrintable(err));
        return 0;
    }
    hresult = result->HrInit();
    if (FAILED(hresult)) {
        result->Release();
        const QString err = QtWin::errorStringFromHresult(hresult);
        qWarning("QWinThumbnailToolBar: qIID_ITaskbarList4 was not initialized: %#010x, %s.",
                 (unsigned)hresult, qPrintable(err));
        return 0;
    }
    return result;
}

QWinThumbnailToolBarPrivate::QWinThumbnailToolBarPrivate() :
    QObject(0), updateScheduled(false), window(0), pTbList(createTaskbarList()), q_ptr(0)
{
    buttonList.reserve(windowsLimitedThumbbarSize);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

QWinThumbnailToolBarPrivate::~QWinThumbnailToolBarPrivate()
{
    if (pTbList)
        pTbList->Release();
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

void QWinThumbnailToolBarPrivate::initToolbar()
{
#if !defined(_MSC_VER) || _MSC_VER >= 1600
    if (!pTbList || !window)
        return;
    THUMBBUTTON buttons[windowsLimitedThumbbarSize];
    initButtons(buttons);
    HRESULT hresult = pTbList->ThumbBarAddButtons(reinterpret_cast<HWND>(window->winId()), windowsLimitedThumbbarSize, buttons);
    if (FAILED(hresult))
        qWarning() << msgComFailed("ThumbBarAddButtons", hresult);
#else
    // ITaskbarList3::ThumbBarAddButtons() has a different signature in SDK 6.X
    Q_UNIMPLEMENTED();
#endif
}

void QWinThumbnailToolBarPrivate::clearToolbar()
{
    if (!pTbList || !window)
        return;
    THUMBBUTTON buttons[windowsLimitedThumbbarSize];
    initButtons(buttons);
    HRESULT hresult = pTbList->ThumbBarUpdateButtons(reinterpret_cast<HWND>(window->winId()), windowsLimitedThumbbarSize, buttons);
    if (FAILED(hresult))
        qWarning() << msgComFailed("ThumbBarUpdateButtons", hresult);
}

void QWinThumbnailToolBarPrivate::_q_updateToolbar()
{
    updateScheduled = false;
    if (!pTbList || !window)
        return;
    THUMBBUTTON buttons[windowsLimitedThumbbarSize];
    initButtons(buttons);
    const int thumbbarSize = qMin(buttonList.size(), windowsLimitedThumbbarSize);
    // filling from the right fixes some strange bug which makes last button bg look like first btn bg
    for (int i = (windowsLimitedThumbbarSize - thumbbarSize); i < windowsLimitedThumbbarSize; i++) {
        QWinThumbnailToolButton *button = buttonList.at(i - (windowsLimitedThumbbarSize - thumbbarSize));
        buttons[i].dwFlags = static_cast<THUMBBUTTONFLAGS>(makeNativeButtonFlags(button));
        buttons[i].dwMask  = static_cast<THUMBBUTTONMASK>(makeButtonMask(button));
        if (!button->icon().isNull()) {;
            buttons[i].hIcon = QtWin::toHICON(button->icon().pixmap(GetSystemMetrics(SM_CXSMICON)));
            if (!buttons[i].hIcon)
                buttons[i].hIcon = (HICON)LoadImage(0, IDI_APPLICATION, IMAGE_ICON, SM_CXSMICON, SM_CYSMICON, LR_SHARED);
        }
        if (!button->toolTip().isEmpty()) {
            buttons[i].szTip[button->toolTip().left(sizeof(buttons[i].szTip)/sizeof(buttons[i].szTip[0]) - 1).toWCharArray(buttons[i].szTip)] = 0;
        }
    }
    HRESULT hresult = pTbList->ThumbBarUpdateButtons(reinterpret_cast<HWND>(window->winId()), windowsLimitedThumbbarSize, buttons);
    if (FAILED(hresult))
        qWarning() << msgComFailed("ThumbBarUpdateButtons", hresult);
    freeButtonResources(buttons);
}

void QWinThumbnailToolBarPrivate::_q_scheduleUpdate()
{
    if (updateScheduled)
        return;
    Q_Q(QWinThumbnailToolBar);
    updateScheduled = true;
    QTimer::singleShot(0, q, SLOT(_q_updateToolbar()));
}

bool QWinThumbnailToolBarPrivate::eventFilter(QObject *object, QEvent *event)
{
    if (object == window && event->type() == QWinEvent::TaskbarButtonCreated) {
        initToolbar();
        _q_scheduleUpdate();
    }
    return QObject::eventFilter(object, event);
}

bool QWinThumbnailToolBarPrivate::nativeEventFilter(const QByteArray &, void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    if (window && msg->message == WM_COMMAND && HIWORD(msg->wParam) == THBN_CLICKED && msg->hwnd == reinterpret_cast<HWND>(window->winId())) {
        int buttonId = LOWORD(msg->wParam);
        buttonId = buttonId - (windowsLimitedThumbbarSize - qMin(windowsLimitedThumbbarSize, buttonList.size()));
        buttonList.at(buttonId)->click();
        if (result)
            *result = 0;
        return true;
    }
    return false;
}

void QWinThumbnailToolBarPrivate::initButtons(THUMBBUTTON *buttons)
{
    for (int i = 0; i < windowsLimitedThumbbarSize; i++) {
        memset(&buttons[i], 0, sizeof buttons[i]);
        buttons[i].iId = i;
        buttons[i].dwFlags = THBF_HIDDEN;
        buttons[i].dwMask  = THB_FLAGS;
    }
}

int QWinThumbnailToolBarPrivate::makeNativeButtonFlags(const QWinThumbnailToolButton *button)
{
    int nativeFlags = 0;
    if (button->isEnabled())
        nativeFlags |= THBF_ENABLED;
    else
        nativeFlags |= THBF_DISABLED;
    if (button->dismissOnClick())
        nativeFlags |= THBF_DISMISSONCLICK;
    if (button->isFlat())
        nativeFlags |= THBF_NOBACKGROUND;
    if (!button->isVisible())
        nativeFlags |= THBF_HIDDEN;
    if (!button->isInteractive())
        nativeFlags |= THBF_NONINTERACTIVE;
    return nativeFlags;
}

int QWinThumbnailToolBarPrivate::makeButtonMask(const QWinThumbnailToolButton *button)
{
    int mask = 0;
    mask |= THB_FLAGS;
    if (!button->icon().isNull())
        mask |= THB_ICON;
    if (!button->toolTip().isEmpty())
        mask |= THB_TOOLTIP;
    return mask;
}

void QWinThumbnailToolBarPrivate::freeButtonResources(THUMBBUTTON *buttons)
{
    for (int i = 0; i < windowsLimitedThumbbarSize; i++) {
        if (buttons[i].hIcon)
            DeleteObject(buttons[i].hIcon);
    }
}

QString QWinThumbnailToolBarPrivate::msgComFailed(const char *function, HRESULT hresult)
{
    return QString::fromLatin1("QWinThumbnailToolBar: %1() failed: #%2: %3")
            .arg(QLatin1String(function))
            .arg((unsigned)hresult, 10, 16, QLatin1Char('0'))
            .arg(QtWin::errorStringFromHresult(hresult));
}

QT_END_NAMESPACE

#include "moc_qwinthumbnailtoolbar.cpp"
