/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "qquickdialog_p.h"
#include <QQuickItem>
#include <QQmlEngine>
#include <QStandardPaths>
#include <private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Dialog
    \instantiates QQuickDialog
    \inqmlmodule QtQuick.Dialogs
    \ingroup dialogs
    \brief A generic QtQuick dialog wrapper with standard buttons.
    \since 5.3

    The purpose of Dialog is to wrap arbitrary content into a \e {dialog window}
    including a row of platform-tailored buttons.

    The \l contentItem is the default property (the only allowed child
    element), and items declared inside the Dialog will actually be children of
    another Item inside the \c contentItem. The row of \l standardButtons will
    also be inside \c contentItem below the declared content, and Dialog will
    attempt to size itself to fit the content and the buttons.

    Alternatively it is possible to bind \l contentItem to a custom Item, in
    which case there will be no buttons, no margins, and the custom content
    will fill the whole dialog.  This is much like creating a \l Window,
    except that on platforms which do not support showing multiple windows,
    the window borders will be simulated and it will be shown in same scene.

    \note do not attempt to bind the width or height of the dialog to the width
    or height of its content, because Dialog already tries to size itself
    to the content. If your goal is to change or eliminate the margins, you
    must override \l contentItem. If your goal is simply to show a window
    (whether modal or not), and your platform supports it, it is simpler to use
    \l Window instead.
*/

/*!
    \qmlsignal Dialog::accepted()

    This signal is emitted by \l accept().

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal Dialog::rejected()

    This signal is emitted by \l reject().

    The corresponding handler is \c onRejected.
*/

/*!
    \qmlsignal Dialog::discard()

    This signal is emitted when the user has pressed the \gui Discard button.

    The corresponding handler is \c onDiscard.
*/

/*!
    \qmlsignal Dialog::help()

    This signal is emitted when the user has pressed the \gui Help button.
    Depending on platform, the dialog may not be automatically dismissed
    because the help that your application provides may need to be relevant to
    the text shown in this dialog in order to assist the user in making a
    decision. However on other platforms it's not possible to show a dialog and
    a help window at the same time. If you want to be sure that the dialog will
    close, you can set \l visible to \c false in your handler.

    The corresponding handler is \c onHelp.
*/

/*!
    \qmlsignal Dialog::yes()

    This signal is emitted when the user has pressed any button which has
    the \l {QMessageBox::YesRole} {YesRole}: \gui Yes or \gui {Yes to All}.

    The corresponding handler is \c onYes.
*/

/*!
    \qmlsignal Dialog::no()

    This signal is emitted when the user has pressed any button which has
    the \l {QMessageBox::NoRole} {NoRole}: \gui No or \gui {No to All}.

    The corresponding handler is \c onNo.
*/

/*!
    \qmlsignal Dialog::apply()

    This signal is emitted when the user has pressed the \gui Apply button.

    The corresponding handler is \c onApply.
*/

/*!
    \qmlsignal Dialog::reset()

    This signal is emitted when the user has pressed any button which has
    the \l {QMessageBox::ResetRole} {ResetRole}: \gui Reset or \gui {Restore Defaults}.

    The corresponding handler is \c onReset.
*/

/*!
    \qmlproperty bool Dialog::visible

    This property holds whether the dialog is visible. By default this is
    \c false.

    \sa modality
*/

/*!
    \qmlproperty Qt::WindowModality Dialog::modality

    Whether the dialog should be shown modal with respect to the window
    containing the dialog's parent Item, modal with respect to the whole
    application, or non-modal.

    By default it is \c Qt.WindowModal.

    Modality does not mean that there are any blocking calls to wait for the
    dialog to be accepted or rejected: only that the user will be prevented
    from interacting with the parent window or the application windows
    until the dialog is dismissed.
*/

/*!
    \qmlmethod void Dialog::open()

    Shows the dialog to the user. It is equivalent to setting \l visible to
    \c true.
*/

/*!
    \qmlmethod void Dialog::close()

    Closes the dialog.
*/

/*!
    \qmlproperty string Dialog::title

    The title of the dialog window.
*/

/*!
    \class QQuickDialog
    \inmodule QtQuick.Dialogs
    \internal

    The QQuickDialog class represents a container for arbitrary
    dialog contents.

    \since 5.3
*/

/*!
    Constructs a dialog wrapper with parent window \a parent.
*/
QQuickDialog::QQuickDialog(QObject *parent)
    : QQuickAbstractDialog(parent)
    , m_enabledButtons(Ok)
    , m_clickedButton(NoButton)
{
}


/*!
    Destroys the dialog wrapper.
*/
QQuickDialog::~QQuickDialog()
{
}

QJSValue QQuickDialog::__standardButtonsLeftModel()
{
    updateStandardButtons();
    return m_standardButtonsLeftModel;
}

QJSValue QQuickDialog::__standardButtonsRightModel()
{
    updateStandardButtons();
    return m_standardButtonsRightModel;
}

void QQuickDialog::setVisible(bool v)
{
    if (v)
        m_clickedButton = NoButton;
    QQuickAbstractDialog::setVisible(v);
}

void QQuickDialog::updateStandardButtons()
{
    if (m_standardButtonsRightModel.isUndefined()) {
        QJSEngine *engine = qmlEngine(this);
        // Managed objects so no need to destroy any existing ones
        m_standardButtonsLeftModel = engine->newArray();
        m_standardButtonsRightModel = engine->newArray();
        int i = 0;

        QPlatformTheme *theme = QGuiApplicationPrivate::platformTheme();
        QPlatformDialogHelper::ButtonLayout layoutPolicy =
            static_cast<QPlatformDialogHelper::ButtonLayout>(theme->themeHint(QPlatformTheme::DialogButtonBoxLayout).toInt());
        const int *buttonLayout = QPlatformDialogHelper::buttonLayout(Qt::Horizontal, layoutPolicy);
        QJSValue *model = &m_standardButtonsLeftModel;
        for (int r = 0; buttonLayout[r] != QPlatformDialogHelper::EOL; ++r) {
            quint32 role = (buttonLayout[r] & ~QPlatformDialogHelper::Reverse);
            // Keep implementation in sync with that in QDialogButtonBoxPrivate::layoutButtons()
            // to the extent that QtQuick supports the same features
            switch (role) {
            case QPlatformDialogHelper::Stretch:
                model = &m_standardButtonsRightModel;
                i = 0;
                break;
            // TODO maybe: case QPlatformDialogHelper::AlternateRole:
            default: {
                    for (int e = QPlatformDialogHelper::LowestBit; e <= QPlatformDialogHelper::HighestBit; ++e) {
                        quint32 standardButton = 1 << e;
                        quint32 standardButtonRole = QPlatformDialogHelper::buttonRole(
                            static_cast<QPlatformDialogHelper::StandardButton>(standardButton));
                        if ((m_enabledButtons & standardButton) && standardButtonRole == role) {
                            QJSValue o = engine->newObject();
                            o.setProperty("text", theme->standardButtonText(standardButton));
                            o.setProperty("standardButton", standardButton);
                            o.setProperty("role", role);
                            model->setProperty(i++, o);
                        }
                   }
                } break;
            }
        }
    }
}

void QQuickDialog::setTitle(const QString &arg)
{
    if (m_title != arg) {
        m_title = arg;
        emit titleChanged();
    }
}

void QQuickDialog::setStandardButtons(StandardButtons buttons)
{
    m_enabledButtons = buttons;
    m_standardButtonsLeftModel = QJSValue();
    m_standardButtonsRightModel = QJSValue();
    emit standardButtonsChanged();
}

/*!
    \qmlproperty bool Dialog::visible

    This property holds whether the dialog is visible. By default this is false.
*/

/*!
    \qmlproperty QObject Dialog::contentItem

    The QML object which implements the dialog contents. Should be an \l Item.

    For example the following dialog will show custom content and no buttons:

    \qml
    import QtQuick 2.3
    import QtQuick.Controls 1.2
    import QtQuick.Dialogs 1.2

    Dialog {
        visible: true
        title: "Blue sky dialog"

        contentItem: Rectangle {
            color: "lightskyblue"
            implicitWidth: 400
            implicitHeight: 100
            Text {
                text: "Hello blue sky!"
                color: "navy"
                anchors.centerIn: parent
            }
        }
    }
    \endqml
*/

void QQuickDialog::click(QPlatformDialogHelper::StandardButton button, QPlatformDialogHelper::ButtonRole role)
{
    setVisible(false);
    m_clickedButton = static_cast<StandardButton>(button);
    emit buttonClicked();
    switch (role) {
    case QPlatformDialogHelper::AcceptRole:
        emit accept();
        break;
    case QPlatformDialogHelper::RejectRole:
        emit reject();
        break;
    case QPlatformDialogHelper::DestructiveRole:
        emit discard();
        break;
    case QPlatformDialogHelper::HelpRole:
        emit help();
        break;
    case QPlatformDialogHelper::YesRole:
        emit yes();
        break;
    case QPlatformDialogHelper::NoRole:
        emit no();
        break;
    case QPlatformDialogHelper::ApplyRole:
        emit apply();
        break;
    case QPlatformDialogHelper::ResetRole:
        emit reset();
        break;
    default:
        qWarning("unhandled Dialog button %d with role %d", (int)button, (int)role);
    }
}

void QQuickDialog::click(QQuickAbstractDialog::StandardButton button)
{
    click(static_cast<QPlatformDialogHelper::StandardButton>(button),
        static_cast<QPlatformDialogHelper::ButtonRole>(
            QPlatformDialogHelper::buttonRole(static_cast<QPlatformDialogHelper::StandardButton>(button))));
}

void QQuickDialog::accept() {
    // enter key is treated like OK
    if (m_clickedButton == NoButton)
        m_clickedButton = Ok;
    QQuickAbstractDialog::accept();
}

void QQuickDialog::reject() {
    // escape key is treated like cancel
    if (m_clickedButton == NoButton)
        m_clickedButton = Cancel;
    QQuickAbstractDialog::reject();
}

// TODO after QTBUG-35019 is fixed: fix links to this module's enums
// rather than linking to those in QMessageBox
/*!
    \enum QQuickStandardButton::StandardButton

    This enum specifies a button with a standard label to be used on a dialog.
*/

/*!
    \qmlproperty StandardButtons Dialog::standardButtons

    Dialog has a row of buttons along the bottom, each of which has a
    \l {QMessageBox::ButtonRole} {ButtonRole} that determines which signal
    will be emitted when the button is pressed. You can also find out which
    specific button was pressed after the fact via the \l clickedButton
    property. You can control which buttons are available by setting
    standardButtons to a bitwise-or combination of the following flags:

    \table
    \row \li StandardButton.Ok          \li An \gui OK button defined with the \l {QMessageBox::AcceptRole} {AcceptRole}.
    \row \li  StandardButton.Open       \li An \gui Open button defined with the \l {QMessageBox::AcceptRole} {AcceptRole}.
    \row \li  StandardButton.Save       \li A \gui Save button defined with the \l {QMessageBox::AcceptRole} {AcceptRole}.
    \row \li  StandardButton.Cancel     \li A \gui Cancel button defined with the \l {QMessageBox::RejectRole} {RejectRole}.
    \row \li  StandardButton.Close      \li A \gui Close button defined with the \l {QMessageBox::RejectRole} {RejectRole}.
    \row \li  StandardButton.Discard    \li A \gui Discard or \gui {Don't Save} button, depending on the platform,
                                        defined with the \l {QMessageBox::DestructiveRole} {DestructiveRole}.
    \row \li  StandardButton.Apply      \li An \gui Apply button defined with the \l {QMessageBox::ApplyRole} {ApplyRole}.
    \row \li  StandardButton.Reset      \li A \gui Reset button defined with the \l {QMessageBox::ResetRole} {ResetRole}.
    \row \li  StandardButton.RestoreDefaults \li A \gui {Restore Defaults} button defined with the \l {QMessageBox::ResetRole} {ResetRole}.
    \row \li  StandardButton.Help       \li A \gui Help button defined with the \l {QMessageBox::HelpRole} {HelpRole}.
    \row \li  StandardButton.SaveAll    \li A \gui {Save All} button defined with the \l {QMessageBox::AcceptRole} {AcceptRole}.
    \row \li  StandardButton.Yes        \li A \gui Yes button defined with the \l {QMessageBox::YesRole} {YesRole}.
    \row \li  StandardButton.YesToAll   \li A \gui {Yes to All} button defined with the \l {QMessageBox::YesRole} {YesRole}.
    \row \li  StandardButton.No         \li A \gui No button defined with the \l {QMessageBox::NoRole} {NoRole}.
    \row \li  StandardButton.NoToAll    \li A \gui {No to All} button defined with the \l {QMessageBox::NoRole} {NoRole}.
    \row \li  StandardButton.Abort      \li An \gui Abort button defined with the \l {QMessageBox::RejectRole} {RejectRole}.
    \row \li  StandardButton.Retry      \li A \gui Retry button defined with the \l {QMessageBox::AcceptRole} {AcceptRole}.
    \row \li  StandardButton.Ignore     \li An \gui Ignore button defined with the \l {QMessageBox::AcceptRole} {AcceptRole}.
    \endtable

    For example the following dialog will show a calendar with the ability to
    save or cancel a date:

    \qml
    import QtQuick 2.3
    import QtQuick.Controls 1.2
    import QtQuick.Dialogs 1.2

    Dialog {
        id: dateDialog
        visible: true
        title: "Choose a date"
        standardButtons: StandardButton.Save | StandardButton.Cancel

        onAccepted: console.log("Saving the date " +
            calendar.selectedDate.toLocaleDateString())

        Calendar {
            id: calendar
            onDoubleClicked: dateDialog.click(StandardButton.Save)
        }
    }
    \endqml

    The default is \c StandardButton.Ok.

    The enum values are the same as in \l QMessageBox::StandardButtons.
*/

QT_END_NAMESPACE
