/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
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

#include "qquickplatformfiledialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <QWindow>
#include <QQuickView>
#include <QQuickWindow>

QT_BEGIN_NAMESPACE

/*!
    \qmltype FileDialog
    \instantiates QQuickPlatformFileDialog
    \inqmlmodule QtQuick.Dialogs 1
    \ingroup dialogs
    \brief Dialog component for choosing files from a local filesystem.
    \since 5.1

    FileDialog provides a basic file chooser: it allows the user to select
    existing files and/or directories, or create new filenames. The dialog is
    initially invisible. You need to set the properties as desired first, then
    set \l visible to true or call \l open().

    Here is a minimal example to open a file dialog and exit after the user
    chooses a file:

    \qml
    import QtQuick 2.2
    import QtQuick.Dialogs 1.0

    FileDialog {
        id: fileDialog
        title: "Please choose a file"
        onAccepted: {
            console.log("You chose: " + fileDialog.fileUrls)
            Qt.quit()
        }
        onRejected: {
            console.log("Canceled")
            Qt.quit()
        }
        Component.onCompleted: visible = true
    }
    \endqml

    A FileDialog window is automatically transient for its parent window. So
    whether you declare the dialog inside an \l Item or inside a \l Window, the
    dialog will appear centered over the window containing the item, or over
    the Window that you declared.

    The implementation of FileDialog will be a platform file dialog if
    possible. If that isn't possible, then it will try to instantiate a
    \l QFileDialog. If that also isn't possible, then it will fall back to a QML
    implementation, DefaultFileDialog.qml. In that case you can customize the
    appearance by editing this file. DefaultFileDialog.qml contains a Rectangle
    to hold the dialog's contents, because certain embedded systems do not
    support multiple top-level windows. When the dialog becomes visible, it
    will automatically be wrapped in a Window if possible, or simply reparented
    on top of the main window if there can only be one window.
*/

/*!
    \qmlsignal QtQuick::Dialogs::FileDialog::accepted

    This signal is emitted when the user has finished using the
    dialog. You can then inspect the \l fileUrl or \l fileUrls properties to
    get the selection.

    Example:

    \qml
    FileDialog {
        onAccepted: { console.log("Selected file: " + fileUrl) }
    }
    \endqml

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::Dialogs::FileDialog::rejected

    This signal is emitted when the user has dismissed the dialog,
    either by closing the dialog window or by pressing the Cancel button.

    The corresponding handler is \c onRejected.
*/

/*!
    \class QQuickPlatformFileDialog
    \inmodule QtQuick.Dialogs
    \internal

    \brief The QQuickPlatformFileDialog class provides a file dialog

    The dialog is implemented via the QPlatformFileDialogHelper when possible;
    otherwise it falls back to a QFileDialog or a QML implementation.

    \since 5.1
*/

/*!
    Constructs a file dialog with parent window \a parent.
*/
QQuickPlatformFileDialog::QQuickPlatformFileDialog(QObject *parent) :
    QQuickAbstractFileDialog(parent)
{
}

/*!
    Destroys the file dialog.
*/
QQuickPlatformFileDialog::~QQuickPlatformFileDialog()
{
    if (m_dlgHelper)
        m_dlgHelper->hide();
    delete m_dlgHelper;
}

QPlatformFileDialogHelper *QQuickPlatformFileDialog::helper()
{
    QQuickItem *parentItem = qobject_cast<QQuickItem *>(parent());
    if (parentItem)
        m_parentWindow = parentItem->window();

    if ( !m_dlgHelper && QGuiApplicationPrivate::platformTheme()->
            usePlatformNativeDialog(QPlatformTheme::FileDialog) ) {
        m_dlgHelper = static_cast<QPlatformFileDialogHelper *>(QGuiApplicationPrivate::platformTheme()
           ->createPlatformDialogHelper(QPlatformTheme::FileDialog));
        if (!m_dlgHelper)
            return m_dlgHelper;
        connect(m_dlgHelper, SIGNAL(directoryEntered(QUrl)), this, SIGNAL(folderChanged()));
        connect(m_dlgHelper, SIGNAL(filterSelected(QString)), this, SIGNAL(filterSelected()));
        connect(m_dlgHelper, SIGNAL(accept()), this, SLOT(accept()));
        connect(m_dlgHelper, SIGNAL(reject()), this, SLOT(reject()));
    }

    return m_dlgHelper;
}

/*!
    \qmlproperty bool FileDialog::visible

    This property holds whether the dialog is visible. By default this is
    false.

    \sa modality
*/

/*!
    \qmlproperty Qt::WindowModality FileDialog::modality

    Whether the dialog should be shown modal with respect to the window
    containing the dialog's parent Item, modal with respect to the whole
    application, or non-modal.

    By default it is \c Qt.WindowModal.

    Modality does not mean that there are any blocking calls to wait for the
    dialog to be accepted or rejected; it's only that the user will be
    prevented from interacting with the parent window and/or the application
    windows at the same time. You probably need to write an onAccepted handler
    to actually load or save the chosen file.
*/

/*!
    \qmlmethod void FileDialog::open()

    Shows the dialog to the user. It is equivalent to setting \l visible to
    true.
*/

/*!
    \qmlmethod void FileDialog::close()

    Closes the dialog.
*/

/*!
    \qmlproperty string FileDialog::title

    The title of the dialog window.
*/

/*!
    \qmlproperty bool FileDialog::selectExisting

    Whether only existing files or directories can be selected.

    By default, this property is true. This property must be set to the desired
    value before opening the dialog. Setting this property to false implies
    that the dialog is for naming a file to which to save something, or naming
    a folder to be created; therefore \l selectMultiple must be false.
*/

/*!
    \qmlproperty bool FileDialog::selectMultiple

    Whether more than one filename can be selected.

    By default, this property is false. This property must be set to the
    desired value before opening the dialog. Setting this property to true
    implies that \l selectExisting must be true.
*/

/*!
    \qmlproperty bool FileDialog::selectFolder

    Whether the selected item should be a folder.

    By default, this property is false. This property must be set to the
    desired value before opening the dialog. Setting this property to true
    implies that \l selectMultiple must be false and \l selectExisting must be
    true.
*/

/*!
    \qmlproperty url FileDialog::folder

    The path to the currently selected folder. Setting this property before
    invoking open() will cause the file browser to be initially positioned on
    the specified folder.

    The value of this property is also updated after the dialog is closed.

    By default, the url is empty.
*/

/*!
    \qmlproperty list<string> FileDialog::nameFilters

    A list of strings to be used as file name filters. Each string can be a
    space-separated list of filters; filters may include the ? and * wildcards.
    The list of filters can also be enclosed in parentheses and a textual
    description of the filter can be provided.

    For example:

    \qml
    FileDialog {
        nameFilters: [ "Image files (*.jpg *.png)", "All files (*)" ]
    }
    \endqml

    \note Directories are not excluded by filters.
    \sa selectedNameFilter
*/

/*!
    \qmlproperty string FileDialog::selectedNameFilter

    Which of the \l nameFilters is currently selected.

    This property can be set before the dialog is visible, to set the default
    name filter, and can also be set while the dialog is visible to set the
    current name filter. It is also updated when the user selects a different
    filter.
*/

/*!
    \qmlproperty url FileDialog::fileUrl

    The path of the file which was selected by the user.

    \note This property is set only if exactly one file was selected. In all
    other cases, it will be empty.

    \sa fileUrls
*/

/*!
    \qmlproperty list<url> FileDialog::fileUrls

    The list of file paths which were selected by the user.
*/

QT_END_NAMESPACE
