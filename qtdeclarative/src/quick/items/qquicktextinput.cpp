/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquicktextinput_p.h"
#include "qquicktextinput_p_p.h"
#include "qquickwindow.h"
#include "qquicktextutil_p.h"

#include <private/qqmlglobal_p.h>


#include <QtCore/qcoreapplication.h>
#include <QtCore/qmimedata.h>
#include <QtQml/qqmlinfo.h>
#include <QtGui/qevent.h>
#include <QTextBoundaryFinder>
#include "qquicktextnode_p.h"
#include <QtQuick/qsgsimplerectnode.h>

#include <QtGui/qstylehints.h>
#include <QtGui/qinputmethod.h>
#include <QtCore/qmath.h>

#ifndef QT_NO_ACCESSIBILITY
#include "qaccessible.h"
#include "qquickaccessibleattached_p.h"
#endif

QT_BEGIN_NAMESPACE

DEFINE_BOOL_CONFIG_OPTION(qmlDisableDistanceField, QML_DISABLE_DISTANCEFIELD)

/*!
    \qmltype TextInput
    \instantiates QQuickTextInput
    \inqmlmodule QtQuick
    \ingroup qtquick-visual
    \ingroup qtquick-input
    \inherits Item
    \brief Displays an editable line of text

    The TextInput type displays a single line of editable plain text.

    TextInput is used to accept a line of text input. Input constraints
    can be placed on a TextInput item (for example, through a \l validator or \l inputMask),
    and setting \l echoMode to an appropriate value enables TextInput to be used for
    a password input field.

    On Mac OS X, the Up/Down key bindings for Home/End are explicitly disabled.
    If you want such bindings (on any platform), you will need to construct them in QML.

    \sa TextEdit, Text, {declarative/text/textselection}{Text Selection example}
*/
QQuickTextInput::QQuickTextInput(QQuickItem* parent)
: QQuickImplicitSizeItem(*(new QQuickTextInputPrivate), parent)
{
    Q_D(QQuickTextInput);
    d->init();
}

QQuickTextInput::~QQuickTextInput()
{
}

void QQuickTextInput::componentComplete()
{
    Q_D(QQuickTextInput);

    QQuickImplicitSizeItem::componentComplete();

    d->checkIsValid();
    d->updateLayout();
    updateCursorRectangle();
    if (d->cursorComponent && isCursorVisible())
        QQuickTextUtil::createCursor(d);
}

/*!
    \qmlproperty string QtQuick::TextInput::text

    The text in the TextInput.
*/
QString QQuickTextInput::text() const
{
    Q_D(const QQuickTextInput);

    QString content = d->m_text;
    QString res = d->m_maskData ? d->stripString(content) : content;
    return (res.isNull() ? QString::fromLatin1("") : res);
}

void QQuickTextInput::setText(const QString &s)
{
    Q_D(QQuickTextInput);
    if (s == text())
        return;

#ifndef QT_NO_IM
    d->cancelPreedit();
#endif
    d->internalSetText(s, -1, false);
}


/*!
    \qmlproperty enumeration QtQuick::TextInput::renderType

    Override the default rendering type for this component.

    Supported render types are:
    \list
    \li Text.QtRendering - the default
    \li Text.NativeRendering
    \endlist

    Select Text.NativeRendering if you prefer text to look native on the target platform and do
    not require advanced features such as transformation of the text. Using such features in
    combination with the NativeRendering render type will lend poor and sometimes pixelated
    results.
*/
QQuickTextInput::RenderType QQuickTextInput::renderType() const
{
    Q_D(const QQuickTextInput);
    return d->renderType;
}

void QQuickTextInput::setRenderType(QQuickTextInput::RenderType renderType)
{
    Q_D(QQuickTextInput);
    if (d->renderType == renderType)
        return;

    d->renderType = renderType;
    emit renderTypeChanged();

    if (isComponentComplete())
        d->updateLayout();
}

/*!
    \qmlproperty int QtQuick::TextInput::length

    Returns the total number of characters in the TextInput item.

    If the TextInput has an inputMask the length will include mask characters and may differ
    from the length of the string returned by the \l text property.

    This property can be faster than querying the length the \l text property as it doesn't
    require any copying or conversion of the TextInput's internal string data.
*/

int QQuickTextInput::length() const
{
    Q_D(const QQuickTextInput);
    return d->m_text.length();
}

/*!
    \qmlmethod string QtQuick::TextInput::getText(int start, int end)

    Returns the section of text that is between the \a start and \a end positions.

    If the TextInput has an inputMask the length will include mask characters.
*/

QString QQuickTextInput::getText(int start, int end) const
{
    Q_D(const QQuickTextInput);

    if (start > end)
        qSwap(start, end);

    return d->m_text.mid(start, end - start);
}

QString QQuickTextInputPrivate::realText() const
{
    QString res = m_maskData ? stripString(m_text) : m_text;
    return (res.isNull() ? QString::fromLatin1("") : res);
}

/*!
    \qmlproperty string QtQuick::TextInput::font.family

    Sets the family name of the font.

    The family name is case insensitive and may optionally include a foundry name, e.g. "Helvetica [Cronyx]".
    If the family is available from more than one foundry and the foundry isn't specified, an arbitrary foundry is chosen.
    If the family isn't available a family will be set using the font matching algorithm.
*/

/*!
    \qmlproperty bool QtQuick::TextInput::font.bold

    Sets whether the font weight is bold.
*/

/*!
    \qmlproperty enumeration QtQuick::TextInput::font.weight

    Sets the font's weight.

    The weight can be one of:
    \list
    \li Font.Light
    \li Font.Normal - the default
    \li Font.DemiBold
    \li Font.Bold
    \li Font.Black
    \endlist

    \qml
    TextInput { text: "Hello"; font.weight: Font.DemiBold }
    \endqml
*/

/*!
    \qmlproperty bool QtQuick::TextInput::font.italic

    Sets whether the font has an italic style.
*/

/*!
    \qmlproperty bool QtQuick::TextInput::font.underline

    Sets whether the text is underlined.
*/

/*!
    \qmlproperty bool QtQuick::TextInput::font.strikeout

    Sets whether the font has a strikeout style.
*/

/*!
    \qmlproperty real QtQuick::TextInput::font.pointSize

    Sets the font size in points. The point size must be greater than zero.
*/

/*!
    \qmlproperty int QtQuick::TextInput::font.pixelSize

    Sets the font size in pixels.

    Using this function makes the font device dependent.
    Use \c pointSize to set the size of the font in a device independent manner.
*/

/*!
    \qmlproperty real QtQuick::TextInput::font.letterSpacing

    Sets the letter spacing for the font.

    Letter spacing changes the default spacing between individual letters in the font.
    A positive value increases the letter spacing by the corresponding pixels; a negative value decreases the spacing.
*/

/*!
    \qmlproperty real QtQuick::TextInput::font.wordSpacing

    Sets the word spacing for the font.

    Word spacing changes the default spacing between individual words.
    A positive value increases the word spacing by a corresponding amount of pixels,
    while a negative value decreases the inter-word spacing accordingly.
*/

/*!
    \qmlproperty enumeration QtQuick::TextInput::font.capitalization

    Sets the capitalization for the text.

    \list
    \li Font.MixedCase - This is the normal text rendering option where no capitalization change is applied.
    \li Font.AllUppercase - This alters the text to be rendered in all uppercase type.
    \li Font.AllLowercase - This alters the text to be rendered in all lowercase type.
    \li Font.SmallCaps - This alters the text to be rendered in small-caps type.
    \li Font.Capitalize - This alters the text to be rendered with the first character of each word as an uppercase character.
    \endlist

    \qml
    TextInput { text: "Hello"; font.capitalization: Font.AllLowercase }
    \endqml
*/

QFont QQuickTextInput::font() const
{
    Q_D(const QQuickTextInput);
    return d->sourceFont;
}

void QQuickTextInput::setFont(const QFont &font)
{
    Q_D(QQuickTextInput);
    if (d->sourceFont == font)
        return;

    d->sourceFont = font;
    QFont oldFont = d->font;
    d->font = font;
    if (d->font.pointSizeF() != -1) {
        // 0.5pt resolution
        qreal size = qRound(d->font.pointSizeF()*2.0);
        d->font.setPointSizeF(size/2.0);
    }
    if (oldFont != d->font) {
        d->updateLayout();
        updateCursorRectangle();
#ifndef QT_NO_IM
        updateInputMethod(Qt::ImCursorRectangle | Qt::ImFont);
#endif
    }
    emit fontChanged(d->sourceFont);
}

/*!
    \qmlproperty color QtQuick::TextInput::color

    The text color.
*/
QColor QQuickTextInput::color() const
{
    Q_D(const QQuickTextInput);
    return d->color;
}

void QQuickTextInput::setColor(const QColor &c)
{
    Q_D(QQuickTextInput);
    if (c != d->color) {
        d->color = c;
        d->textLayoutDirty = true;
        d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
        update();
        emit colorChanged();
    }
}


/*!
    \qmlproperty color QtQuick::TextInput::selectionColor

    The text highlight color, used behind selections.
*/
QColor QQuickTextInput::selectionColor() const
{
    Q_D(const QQuickTextInput);
    return d->selectionColor;
}

void QQuickTextInput::setSelectionColor(const QColor &color)
{
    Q_D(QQuickTextInput);
    if (d->selectionColor == color)
        return;

    d->selectionColor = color;
    if (d->hasSelectedText()) {
        d->textLayoutDirty = true;
        d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
        update();
    }
    emit selectionColorChanged();
}
/*!
    \qmlproperty color QtQuick::TextInput::selectedTextColor

    The highlighted text color, used in selections.
*/
QColor QQuickTextInput::selectedTextColor() const
{
    Q_D(const QQuickTextInput);
    return d->selectedTextColor;
}

void QQuickTextInput::setSelectedTextColor(const QColor &color)
{
    Q_D(QQuickTextInput);
    if (d->selectedTextColor == color)
        return;

    d->selectedTextColor = color;
    if (d->hasSelectedText()) {
        d->textLayoutDirty = true;
        d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
        update();
    }
    emit selectedTextColorChanged();
}

/*!
    \qmlproperty enumeration QtQuick::TextInput::horizontalAlignment
    \qmlproperty enumeration QtQuick::TextInput::effectiveHorizontalAlignment
    \qmlproperty enumeration QtQuick::TextInput::verticalAlignment

    Sets the horizontal alignment of the text within the TextInput item's
    width and height. By default, the text alignment follows the natural alignment
    of the text, for example text that is read from left to right will be aligned to
    the left.

    TextInput does not have vertical alignment, as the natural height is
    exactly the height of the single line of text. If you set the height
    manually to something larger, TextInput will always be top aligned
    vertically. You can use anchors to align it however you want within
    another item.

    The valid values for \c horizontalAlignment are \c TextInput.AlignLeft, \c TextInput.AlignRight and
    \c TextInput.AlignHCenter.

    Valid values for \c verticalAlignment are \c TextInput.AlignTop (default),
    \c TextInput.AlignBottom \c TextInput.AlignVCenter.

    When using the attached property LayoutMirroring::enabled to mirror application
    layouts, the horizontal alignment of text will also be mirrored. However, the property
    \c horizontalAlignment will remain unchanged. To query the effective horizontal alignment
    of TextInput, use the read-only property \c effectiveHorizontalAlignment.
*/
QQuickTextInput::HAlignment QQuickTextInput::hAlign() const
{
    Q_D(const QQuickTextInput);
    return d->hAlign;
}

void QQuickTextInput::setHAlign(HAlignment align)
{
    Q_D(QQuickTextInput);
    bool forceAlign = d->hAlignImplicit && d->effectiveLayoutMirror;
    d->hAlignImplicit = false;
    if (d->setHAlign(align, forceAlign) && isComponentComplete()) {
        d->updateLayout();
        updateCursorRectangle();
    }
}

void QQuickTextInput::resetHAlign()
{
    Q_D(QQuickTextInput);
    d->hAlignImplicit = true;
    if (d->determineHorizontalAlignment() && isComponentComplete()) {
        d->updateLayout();
        updateCursorRectangle();
    }
}

QQuickTextInput::HAlignment QQuickTextInput::effectiveHAlign() const
{
    Q_D(const QQuickTextInput);
    QQuickTextInput::HAlignment effectiveAlignment = d->hAlign;
    if (!d->hAlignImplicit && d->effectiveLayoutMirror) {
        switch (d->hAlign) {
        case QQuickTextInput::AlignLeft:
            effectiveAlignment = QQuickTextInput::AlignRight;
            break;
        case QQuickTextInput::AlignRight:
            effectiveAlignment = QQuickTextInput::AlignLeft;
            break;
        default:
            break;
        }
    }
    return effectiveAlignment;
}

bool QQuickTextInputPrivate::setHAlign(QQuickTextInput::HAlignment alignment, bool forceAlign)
{
    Q_Q(QQuickTextInput);
    if ((hAlign != alignment || forceAlign) && alignment <= QQuickTextInput::AlignHCenter) { // justify not supported
        QQuickTextInput::HAlignment oldEffectiveHAlign = q->effectiveHAlign();
        hAlign = alignment;
        emit q->horizontalAlignmentChanged(alignment);
        if (oldEffectiveHAlign != q->effectiveHAlign())
            emit q->effectiveHorizontalAlignmentChanged();
        return true;
    }
    return false;
}

Qt::LayoutDirection QQuickTextInputPrivate::textDirection() const
{
    QString text = m_text;
#ifndef QT_NO_IM
    if (text.isEmpty())
        text = m_textLayout.preeditAreaText();
#endif

    const QChar *character = text.constData();
    while (!character->isNull()) {
        switch (character->direction()) {
        case QChar::DirL:
            return Qt::LeftToRight;
        case QChar::DirR:
        case QChar::DirAL:
        case QChar::DirAN:
            return Qt::RightToLeft;
        default:
            break;
        }
        character++;
    }
    return Qt::LayoutDirectionAuto;
}

Qt::LayoutDirection QQuickTextInputPrivate::layoutDirection() const
{
    Qt::LayoutDirection direction = m_layoutDirection;
    if (direction == Qt::LayoutDirectionAuto) {
        direction = textDirection();
#ifndef QT_NO_IM
        if (direction == Qt::LayoutDirectionAuto)
            direction = qApp->inputMethod()->inputDirection();
#endif
    }
    return (direction == Qt::LayoutDirectionAuto) ? Qt::LeftToRight : direction;
}

bool QQuickTextInputPrivate::determineHorizontalAlignment()
{
    if (hAlignImplicit) {
        // if no explicit alignment has been set, follow the natural layout direction of the text
        Qt::LayoutDirection direction = textDirection();
#ifndef QT_NO_IM
        if (direction == Qt::LayoutDirectionAuto)
            direction = qApp->inputMethod()->inputDirection();
#endif
        return setHAlign(direction == Qt::RightToLeft ? QQuickTextInput::AlignRight : QQuickTextInput::AlignLeft);
    }
    return false;
}

QQuickTextInput::VAlignment QQuickTextInput::vAlign() const
{
    Q_D(const QQuickTextInput);
    return d->vAlign;
}

void QQuickTextInput::setVAlign(QQuickTextInput::VAlignment alignment)
{
    Q_D(QQuickTextInput);
    if (alignment == d->vAlign)
        return;
    d->vAlign = alignment;
    emit verticalAlignmentChanged(d->vAlign);
    if (isComponentComplete()) {
        updateCursorRectangle();
    }
}

/*!
    \qmlproperty enumeration QtQuick::TextInput::wrapMode

    Set this property to wrap the text to the TextInput item's width.
    The text will only wrap if an explicit width has been set.

    \list
    \li TextInput.NoWrap - no wrapping will be performed. If the text contains insufficient newlines, then implicitWidth will exceed a set width.
    \li TextInput.WordWrap - wrapping is done on word boundaries only. If a word is too long, implicitWidth will exceed a set width.
    \li TextInput.WrapAnywhere - wrapping is done at any point on a line, even if it occurs in the middle of a word.
    \li TextInput.Wrap - if possible, wrapping occurs at a word boundary; otherwise it will occur at the appropriate point on the line, even in the middle of a word.
    \endlist

    The default is TextInput.NoWrap. If you set a width, consider using TextInput.Wrap.
*/
QQuickTextInput::WrapMode QQuickTextInput::wrapMode() const
{
    Q_D(const QQuickTextInput);
    return d->wrapMode;
}

void QQuickTextInput::setWrapMode(WrapMode mode)
{
    Q_D(QQuickTextInput);
    if (mode == d->wrapMode)
        return;
    d->wrapMode = mode;
    d->updateLayout();
    updateCursorRectangle();
    emit wrapModeChanged();
}

void QQuickTextInputPrivate::mirrorChange()
{
    Q_Q(QQuickTextInput);
    if (q->isComponentComplete()) {
        if (!hAlignImplicit && (hAlign == QQuickTextInput::AlignRight || hAlign == QQuickTextInput::AlignLeft)) {
            q->updateCursorRectangle();
            emit q->effectiveHorizontalAlignmentChanged();
        }
    }
}

/*!
    \qmlproperty bool QtQuick::TextInput::readOnly

    Sets whether user input can modify the contents of the TextInput.

    If readOnly is set to true, then user input will not affect the text
    property. Any bindings or attempts to set the text property will still
    work.
*/
bool QQuickTextInput::isReadOnly() const
{
    Q_D(const QQuickTextInput);
    return d->m_readOnly;
}

void QQuickTextInput::setReadOnly(bool ro)
{
    Q_D(QQuickTextInput);
    if (d->m_readOnly == ro)
        return;

#ifndef QT_NO_IM
    setFlag(QQuickItem::ItemAcceptsInputMethod, !ro);
#endif
    d->m_readOnly = ro;
    if (!ro)
        d->setCursorPosition(d->end());
#ifndef QT_NO_IM
    updateInputMethod(Qt::ImEnabled);
#endif
    q_canPasteChanged();
    d->emitUndoRedoChanged();
    emit readOnlyChanged(ro);
}

/*!
    \qmlproperty int QtQuick::TextInput::maximumLength
    The maximum permitted length of the text in the TextInput.

    If the text is too long, it is truncated at the limit.

    By default, this property contains a value of 32767.
*/
int QQuickTextInput::maxLength() const
{
    Q_D(const QQuickTextInput);
    return d->m_maxLength;
}

void QQuickTextInput::setMaxLength(int ml)
{
    Q_D(QQuickTextInput);
    if (d->m_maxLength == ml || d->m_maskData)
        return;

    d->m_maxLength = ml;
    d->internalSetText(d->m_text, -1, false);

    emit maximumLengthChanged(ml);
}

/*!
    \qmlproperty bool QtQuick::TextInput::cursorVisible
    Set to true when the TextInput shows a cursor.

    This property is set and unset when the TextInput gets active focus, so that other
    properties can be bound to whether the cursor is currently showing. As it
    gets set and unset automatically, when you set the value yourself you must
    keep in mind that your value may be overwritten.

    It can be set directly in script, for example if a KeyProxy might
    forward keys to it and you desire it to look active when this happens
    (but without actually giving it active focus).

    It should not be set directly on the item, like in the below QML,
    as the specified value will be overridden an lost on focus changes.

    \code
    TextInput {
        text: "Text"
        cursorVisible: false
    }
    \endcode

    In the above snippet the cursor will still become visible when the
    TextInput gains active focus.
*/
bool QQuickTextInput::isCursorVisible() const
{
    Q_D(const QQuickTextInput);
    return d->cursorVisible;
}

void QQuickTextInput::setCursorVisible(bool on)
{
    Q_D(QQuickTextInput);
    if (d->cursorVisible == on)
        return;
    d->cursorVisible = on;
    if (on && isComponentComplete())
        QQuickTextUtil::createCursor(d);
    if (!d->cursorItem) {
        d->setCursorBlinkPeriod(on ? qApp->styleHints()->cursorFlashTime() : 0);
        d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
        update();
    }
    emit cursorVisibleChanged(d->cursorVisible);
}

/*!
    \qmlproperty int QtQuick::TextInput::cursorPosition
    The position of the cursor in the TextInput.
*/
int QQuickTextInput::cursorPosition() const
{
    Q_D(const QQuickTextInput);
    return d->m_cursor;
}

void QQuickTextInput::setCursorPosition(int cp)
{
    Q_D(QQuickTextInput);
    if (cp < 0 || cp > text().length())
        return;
    d->moveCursor(cp);
}

/*!
    \qmlproperty rectangle QtQuick::TextInput::cursorRectangle

    The rectangle where the standard text cursor is rendered within the text input.  Read only.

    The position and height of a custom cursorDelegate are updated to follow the cursorRectangle
    automatically when it changes.  The width of the delegate is unaffected by changes in the
    cursor rectangle.
*/

QRectF QQuickTextInput::cursorRectangle() const
{
    Q_D(const QQuickTextInput);

    int c = d->m_cursor;
#ifndef QT_NO_IM
    c += d->m_preeditCursor;
#endif
    if (d->m_echoMode == NoEcho)
        c = 0;
    QTextLine l = d->m_textLayout.lineForTextPosition(c);
    if (!l.isValid())
        return QRectF();
    qreal x = l.cursorToX(c) - d->hscroll;
    qreal y = l.y() - d->vscroll;
    qreal height = l.ascent() + l.descent();
    return QRectF(x, y, 1, height);
}

/*!
    \qmlproperty int QtQuick::TextInput::selectionStart

    The cursor position before the first character in the current selection.

    This property is read-only. To change the selection, use select(start,end),
    selectAll(), or selectWord().

    \sa selectionEnd, cursorPosition, selectedText
*/
int QQuickTextInput::selectionStart() const
{
    Q_D(const QQuickTextInput);
    return d->lastSelectionStart;
}
/*!
    \qmlproperty int QtQuick::TextInput::selectionEnd

    The cursor position after the last character in the current selection.

    This property is read-only. To change the selection, use select(start,end),
    selectAll(), or selectWord().

    \sa selectionStart, cursorPosition, selectedText
*/
int QQuickTextInput::selectionEnd() const
{
    Q_D(const QQuickTextInput);
    return d->lastSelectionEnd;
}
/*!
    \qmlmethod QtQuick::TextInput::select(int start, int end)

    Causes the text from \a start to \a end to be selected.

    If either start or end is out of range, the selection is not changed.

    After calling this, selectionStart will become the lesser
    and selectionEnd will become the greater (regardless of the order passed
    to this method).

    \sa selectionStart, selectionEnd
*/
void QQuickTextInput::select(int start, int end)
{
    Q_D(QQuickTextInput);
    if (start < 0 || end < 0 || start > d->m_text.length() || end > d->m_text.length())
        return;
    d->setSelection(start, end-start);
}

/*!
    \qmlproperty string QtQuick::TextInput::selectedText

    This read-only property provides the text currently selected in the
    text input.

    It is equivalent to the following snippet, but is faster and easier
    to use.

    \js
    myTextInput.text.toString().substring(myTextInput.selectionStart,
        myTextInput.selectionEnd);
    \endjs
*/
QString QQuickTextInput::selectedText() const
{
    Q_D(const QQuickTextInput);
    return d->selectedText();
}

/*!
    \qmlproperty bool QtQuick::TextInput::activeFocusOnPress

    Whether the TextInput should gain active focus on a mouse press. By default this is
    set to true.
*/
bool QQuickTextInput::focusOnPress() const
{
    Q_D(const QQuickTextInput);
    return d->focusOnPress;
}

void QQuickTextInput::setFocusOnPress(bool b)
{
    Q_D(QQuickTextInput);
    if (d->focusOnPress == b)
        return;

    d->focusOnPress = b;

    emit activeFocusOnPressChanged(d->focusOnPress);
}
/*!
    \qmlproperty bool QtQuick::TextInput::autoScroll

    Whether the TextInput should scroll when the text is longer than the width. By default this is
    set to true.
*/
bool QQuickTextInput::autoScroll() const
{
    Q_D(const QQuickTextInput);
    return d->autoScroll;
}

void QQuickTextInput::setAutoScroll(bool b)
{
    Q_D(QQuickTextInput);
    if (d->autoScroll == b)
        return;

    d->autoScroll = b;
    //We need to repaint so that the scrolling is taking into account.
    updateCursorRectangle();
    emit autoScrollChanged(d->autoScroll);
}

#ifndef QT_NO_VALIDATOR

/*!
    \qmltype IntValidator
    \instantiates QIntValidator
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Defines a validator for integer values

    The IntValidator type provides a validator for integer values.

    If no \l locale is set IntValidator uses the \l {QLocale::setDefault()}{default locale} to
    interpret the number and will accept locale specific digits, group separators, and positive
    and negative signs.  In addition, IntValidator is always guaranteed to accept a number
    formatted according to the "C" locale.
*/


QQuickIntValidator::QQuickIntValidator(QObject *parent)
    : QIntValidator(parent)
{
}

/*!
    \qmlproperty string QtQuick::IntValidator::locale

    This property holds the name of the locale used to interpret the number.

    \sa {QtQml::Qt::locale()}{Qt.locale()}
*/

QString QQuickIntValidator::localeName() const
{
    return locale().name();
}

void QQuickIntValidator::setLocaleName(const QString &name)
{
    if (locale().name() != name) {
        setLocale(QLocale(name));
        emit localeNameChanged();
    }
}

void QQuickIntValidator::resetLocaleName()
{
    QLocale defaultLocale;
    if (locale() != defaultLocale) {
        setLocale(defaultLocale);
        emit localeNameChanged();
    }
}

/*!
    \qmlproperty int QtQuick::IntValidator::top

    This property holds the validator's highest acceptable value.
    By default, this property's value is derived from the highest signed integer available (typically 2147483647).
*/
/*!
    \qmlproperty int QtQuick::IntValidator::bottom

    This property holds the validator's lowest acceptable value.
    By default, this property's value is derived from the lowest signed integer available (typically -2147483647).
*/

/*!
    \qmltype DoubleValidator
    \instantiates QDoubleValidator
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Defines a validator for non-integer numbers

    The DoubleValidator type provides a validator for non-integer numbers.

    Input is accepted if it contains a double that is within the valid range
    and is in the  correct format.

    Input is accepected but invalid if it contains a double that is outside
    the range or is in the wrong format; e.g. with too many digits after the
    decimal point or is empty.

    Input is rejected if it is not a double.

    Note: If the valid range consists of just positive doubles (e.g. 0.0 to
    100.0) and input is a negative double then it is rejected. If \l notation
    is set to DoubleValidator.StandardNotation, and  the input contains more
    digits before the decimal point than a double in the valid range may have,
    it is also rejected. If \l notation is DoubleValidator.ScientificNotation,
    and the input is not in the valid range, it is accecpted but invalid. The
    value may yet become valid by changing the exponent.
*/

QQuickDoubleValidator::QQuickDoubleValidator(QObject *parent)
    : QDoubleValidator(parent)
{
}

/*!
    \qmlproperty string QtQuick::DoubleValidator::locale

    This property holds the name of the locale used to interpret the number.

    \sa {QtQml::Qt::locale()}{Qt.locale()}
*/

QString QQuickDoubleValidator::localeName() const
{
    return locale().name();
}

void QQuickDoubleValidator::setLocaleName(const QString &name)
{
    if (locale().name() != name) {
        setLocale(QLocale(name));
        emit localeNameChanged();
    }
}

void QQuickDoubleValidator::resetLocaleName()
{
    QLocale defaultLocale;
    if (locale() != defaultLocale) {
        setLocale(defaultLocale);
        emit localeNameChanged();
    }
}

/*!
    \qmlproperty real QtQuick::DoubleValidator::top

    This property holds the validator's maximum acceptable value.
    By default, this property contains a value of infinity.
*/
/*!
    \qmlproperty real QtQuick::DoubleValidator::bottom

    This property holds the validator's minimum acceptable value.
    By default, this property contains a value of -infinity.
*/
/*!
    \qmlproperty int QtQuick::DoubleValidator::decimals

    This property holds the validator's maximum number of digits after the decimal point.
    By default, this property contains a value of 1000.
*/
/*!
    \qmlproperty enumeration QtQuick::DoubleValidator::notation
    This property holds the notation of how a string can describe a number.

    The possible values for this property are:

    \list
    \li DoubleValidator.StandardNotation
    \li DoubleValidator.ScientificNotation (default)
    \endlist

    If this property is set to DoubleValidator.ScientificNotation, the written number may have an exponent part (e.g. 1.5E-2).
*/

/*!
    \qmltype RegExpValidator
    \instantiates QRegExpValidator
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Provides a string validator

    The RegExpValidator type provides a validator, which counts as valid any string which
    matches a specified regular expression.
*/
/*!
   \qmlproperty regExp QtQuick::RegExpValidator::regExp

   This property holds the regular expression used for validation.

   Note that this property should be a regular expression in JS syntax, e.g /a/ for the regular expression
   matching "a".

   By default, this property contains a regular expression with the pattern .* that matches any string.
*/

/*!
    \qmlproperty Validator QtQuick::TextInput::validator

    Allows you to set a validator on the TextInput. When a validator is set
    the TextInput will only accept input which leaves the text property in
    an acceptable or intermediate state. The accepted signal will only be sent
    if the text is in an acceptable state when enter is pressed.

    Currently supported validators are IntValidator, DoubleValidator and
    RegExpValidator. An example of using validators is shown below, which allows
    input of integers between 11 and 31 into the text input:

    \code
    import QtQuick 2.0
    TextInput{
        validator: IntValidator{bottom: 11; top: 31;}
        focus: true
    }
    \endcode

    \sa acceptableInput, inputMask
*/

QValidator* QQuickTextInput::validator() const
{
    Q_D(const QQuickTextInput);
    return d->m_validator;
}

void QQuickTextInput::setValidator(QValidator* v)
{
    Q_D(QQuickTextInput);
    if (d->m_validator == v)
        return;

    if (d->m_validator) {
        qmlobject_disconnect(
                d->m_validator, QValidator, SIGNAL(changed()),
                this, QQuickTextInput, SLOT(q_validatorChanged()));
    }

    d->m_validator = v;

    if (d->m_validator) {
        qmlobject_connect(
                d->m_validator, QValidator, SIGNAL(changed()),
                this, QQuickTextInput, SLOT(q_validatorChanged()));
    }

    if (isComponentComplete())
        d->checkIsValid();

    emit validatorChanged();
}

void QQuickTextInput::q_validatorChanged()
{
    Q_D(QQuickTextInput);
    d->checkIsValid();
}

#endif // QT_NO_VALIDATOR

void QQuickTextInputPrivate::checkIsValid()
{
    Q_Q(QQuickTextInput);

    ValidatorState state = hasAcceptableInput(m_text);
    m_validInput = state != InvalidInput;
    if (state != AcceptableInput) {
        if (m_acceptableInput) {
            m_acceptableInput = false;
            emit q->acceptableInputChanged();
        }
    } else if (!m_acceptableInput) {
        m_acceptableInput = true;
        emit q->acceptableInputChanged();
    }
}

/*!
    \qmlproperty string QtQuick::TextInput::inputMask

    Allows you to set an input mask on the TextInput, restricting the allowable
    text inputs. See QLineEdit::inputMask for further details, as the exact
    same mask strings are used by TextInput.

    \sa acceptableInput, validator
*/
QString QQuickTextInput::inputMask() const
{
    Q_D(const QQuickTextInput);
    return d->inputMask();
}

void QQuickTextInput::setInputMask(const QString &im)
{
    Q_D(QQuickTextInput);
    if (d->inputMask() == im)
        return;

    d->setInputMask(im);
    emit inputMaskChanged(d->inputMask());
}

/*!
    \qmlproperty bool QtQuick::TextInput::acceptableInput

    This property is always true unless a validator or input mask has been set.
    If a validator or input mask has been set, this property will only be true
    if the current text is acceptable to the validator or input mask as a final
    string (not as an intermediate string).
*/
bool QQuickTextInput::hasAcceptableInput() const
{
    Q_D(const QQuickTextInput);
    return d->hasAcceptableInput(d->m_text) == QQuickTextInputPrivate::AcceptableInput;
}

/*!
    \qmlsignal QtQuick::TextInput::accepted()

    This signal is emitted when the Return or Enter key is pressed.
    Note that if there is a \l validator or \l inputMask set on the text
    input, the signal will only be emitted if the input is in an acceptable
    state.

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::TextInput::editingFinished()
    \since 5.2

    This signal is emitted when the Return or Enter key is pressed or
    the text input loses focus. Note that if there is a validator or
    inputMask set on the text input and enter/return is pressed, this
    signal will only be emitted if the input follows
    the inputMask and the validator returns an acceptable state.

    The corresponding handler is \c onEditingFinished.
*/

#ifndef QT_NO_IM
Qt::InputMethodHints QQuickTextInputPrivate::effectiveInputMethodHints() const
{
    Qt::InputMethodHints hints = inputMethodHints;
    if (m_echoMode == QQuickTextInput::Password || m_echoMode == QQuickTextInput::NoEcho)
        hints |= Qt::ImhHiddenText;
    else if (m_echoMode == QQuickTextInput::PasswordEchoOnEdit)
        hints &= ~Qt::ImhHiddenText;
    if (m_echoMode != QQuickTextInput::Normal)
        hints |= (Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText | Qt::ImhSensitiveData);
    return hints;
}
#endif

/*!
    \qmlproperty enumeration QtQuick::TextInput::echoMode

    Specifies how the text should be displayed in the TextInput.
    \list
    \li TextInput.Normal - Displays the text as it is. (Default)
    \li TextInput.Password - Displays platform-dependent password mask
    characters instead of the actual characters.
    \li TextInput.NoEcho - Displays nothing.
    \li TextInput.PasswordEchoOnEdit - Displays characters as they are entered
    while editing, otherwise identical to \c TextInput.Password.
    \endlist
*/
QQuickTextInput::EchoMode QQuickTextInput::echoMode() const
{
    Q_D(const QQuickTextInput);
    return QQuickTextInput::EchoMode(d->m_echoMode);
}

void QQuickTextInput::setEchoMode(QQuickTextInput::EchoMode echo)
{
    Q_D(QQuickTextInput);
    if (echoMode() == echo)
        return;
    d->cancelPasswordEchoTimer();
    d->m_echoMode = echo;
    d->m_passwordEchoEditing = false;
#ifndef QT_NO_IM
    updateInputMethod(Qt::ImHints);
#endif
    d->updateDisplayText();
    updateCursorRectangle();

    emit echoModeChanged(echoMode());
}

#ifndef QT_NO_IM
/*!
    \qmlproperty enumeration QtQuick::TextInput::inputMethodHints

    Provides hints to the input method about the expected content of the text input and how it
    should operate.

    The value is a bit-wise combination of flags, or Qt.ImhNone if no hints are set.

    Flags that alter behaviour are:

    \list
    \li Qt.ImhHiddenText - Characters should be hidden, as is typically used when entering passwords.
            This is automatically set when setting echoMode to \c TextInput.Password.
    \li Qt.ImhSensitiveData - Typed text should not be stored by the active input method
            in any persistent storage like predictive user dictionary.
    \li Qt.ImhNoAutoUppercase - The input method should not try to automatically switch to upper case
            when a sentence ends.
    \li Qt.ImhPreferNumbers - Numbers are preferred (but not required).
    \li Qt.ImhPreferUppercase - Upper case letters are preferred (but not required).
    \li Qt.ImhPreferLowercase - Lower case letters are preferred (but not required).
    \li Qt.ImhNoPredictiveText - Do not use predictive text (i.e. dictionary lookup) while typing.

    \li Qt.ImhDate - The text editor functions as a date field.
    \li Qt.ImhTime - The text editor functions as a time field.
    \endlist

    Flags that restrict input (exclusive flags) are:

    \list
    \li Qt.ImhDigitsOnly - Only digits are allowed.
    \li Qt.ImhFormattedNumbersOnly - Only number input is allowed. This includes decimal point and minus sign.
    \li Qt.ImhUppercaseOnly - Only upper case letter input is allowed.
    \li Qt.ImhLowercaseOnly - Only lower case letter input is allowed.
    \li Qt.ImhDialableCharactersOnly - Only characters suitable for phone dialing are allowed.
    \li Qt.ImhEmailCharactersOnly - Only characters suitable for email addresses are allowed.
    \li Qt.ImhUrlCharactersOnly - Only characters suitable for URLs are allowed.
    \endlist

    Masks:

    \list
    \li Qt.ImhExclusiveInputMask - This mask yields nonzero if any of the exclusive flags are used.
    \endlist
*/

Qt::InputMethodHints QQuickTextInput::inputMethodHints() const
{
    Q_D(const QQuickTextInput);
    return d->inputMethodHints;
}

void QQuickTextInput::setInputMethodHints(Qt::InputMethodHints hints)
{
    Q_D(QQuickTextInput);

    if (hints == d->inputMethodHints)
        return;

    d->inputMethodHints = hints;
    updateInputMethod(Qt::ImHints);
    emit inputMethodHintsChanged();
}
#endif // QT_NO_IM

/*!
    \qmlproperty Component QtQuick::TextInput::cursorDelegate
    The delegate for the cursor in the TextInput.

    If you set a cursorDelegate for a TextInput, this delegate will be used for
    drawing the cursor instead of the standard cursor. An instance of the
    delegate will be created and managed by the TextInput when a cursor is
    needed, and the x property of delegate instance will be set so as
    to be one pixel before the top left of the current character.

    Note that the root item of the delegate component must be a QQuickItem or
    QQuickItem derived item.
*/
QQmlComponent* QQuickTextInput::cursorDelegate() const
{
    Q_D(const QQuickTextInput);
    return d->cursorComponent;
}

void QQuickTextInput::setCursorDelegate(QQmlComponent* c)
{
    Q_D(QQuickTextInput);
    QQuickTextUtil::setCursorDelegate(d, c);
}

void QQuickTextInput::createCursor()
{
    Q_D(QQuickTextInput);
    d->cursorPending = true;
    QQuickTextUtil::createCursor(d);
}

/*!
    \qmlmethod rect QtQuick::TextInput::positionToRectangle(int pos)

    This function takes a character position and returns the rectangle that the
    cursor would occupy, if it was placed at that character position.

    This is similar to setting the cursorPosition, and then querying the cursor
    rectangle, but the cursorPosition is not changed.
*/
QRectF QQuickTextInput::positionToRectangle(int pos) const
{
    Q_D(const QQuickTextInput);
    if (d->m_echoMode == NoEcho)
        pos = 0;
#ifndef QT_NO_IM
    else if (pos > d->m_cursor)
        pos += d->preeditAreaText().length();
#endif
    QTextLine l = d->m_textLayout.lineForTextPosition(pos);
    if (!l.isValid())
        return QRectF();
    qreal x = l.cursorToX(pos) - d->hscroll;
    qreal y = l.y() - d->vscroll;
    qreal height = l.ascent() + l.descent();
    return QRectF(x, y, 1, height);
}

/*!
    \qmlmethod int QtQuick::TextInput::positionAt(real x, real y, CursorPosition position = CursorBetweenCharacters)

    This function returns the character position at
    x and y pixels from the top left  of the textInput. Position 0 is before the
    first character, position 1 is after the first character but before the second,
    and so on until position text.length, which is after all characters.

    This means that for all x values before the first character this function returns 0,
    and for all x values after the last character this function returns text.length.  If
    the y value is above the text the position will be that of the nearest character on
    the first line line and if it is below the text the position of the nearest character
    on the last line will be returned.

    The cursor position type specifies how the cursor position should be resolved.

    \list
    \li TextInput.CursorBetweenCharacters - Returns the position between characters that is nearest x.
    \li TextInput.CursorOnCharacter - Returns the position before the character that is nearest x.
    \endlist
*/

void QQuickTextInput::positionAt(QQmlV4Function *args) const
{
    Q_D(const QQuickTextInput);

    qreal x = 0;
    qreal y = 0;
    QTextLine::CursorPosition position = QTextLine::CursorBetweenCharacters;

    if (args->length() < 1)
        return;

    int i = 0;
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue arg(scope, (*args)[0]);
    x = arg->toNumber();

    if (++i < args->length()) {
        arg = (*args)[i];
        y = arg->toNumber();
    }

    if (++i < args->length()) {
        arg = (*args)[i];
        position = QTextLine::CursorPosition(arg->toInt32());
    }

    int pos = d->positionAt(x, y, position);
    const int cursor = d->m_cursor;
    if (pos > cursor) {
#ifndef QT_NO_IM
        const int preeditLength = d->preeditAreaText().length();
        pos = pos > cursor + preeditLength
                ? pos - preeditLength
                : cursor;
#else
        pos = cursor;
#endif
    }
    args->setReturnValue(QV4::Encode(pos));
}

int QQuickTextInputPrivate::positionAt(qreal x, qreal y, QTextLine::CursorPosition position) const
{
    x += hscroll;
    y += vscroll;
    QTextLine line = m_textLayout.lineAt(0);
    for (int i = 1; i < m_textLayout.lineCount(); ++i) {
        QTextLine nextLine = m_textLayout.lineAt(i);

        if (y < (line.rect().bottom() + nextLine.y()) / 2)
            break;
        line = nextLine;
    }
    return line.isValid() ? line.xToCursor(x, position) : 0;
}

void QQuickTextInput::keyPressEvent(QKeyEvent* ev)
{
    Q_D(QQuickTextInput);
    // Don't allow MacOSX up/down support, and we don't allow a completer.
    bool ignore = (ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down) && ev->modifiers() == Qt::NoModifier;
    if (!ignore && (d->lastSelectionStart == d->lastSelectionEnd) && (ev->key() == Qt::Key_Right || ev->key() == Qt::Key_Left)) {
        // Ignore when moving off the end unless there is a selection,
        // because then moving will do something (deselect).
        int cursorPosition = d->m_cursor;
        if (cursorPosition == 0)
            ignore = ev->key() == (d->layoutDirection() == Qt::LeftToRight ? Qt::Key_Left : Qt::Key_Right);
        if (!ignore && cursorPosition == d->m_text.length())
            ignore = ev->key() == (d->layoutDirection() == Qt::LeftToRight ? Qt::Key_Right : Qt::Key_Left);
    }
    if (ignore) {
        ev->ignore();
    } else {
        d->processKeyEvent(ev);
    }
    if (!ev->isAccepted())
        QQuickImplicitSizeItem::keyPressEvent(ev);
}

#ifndef QT_NO_IM
void QQuickTextInput::inputMethodEvent(QInputMethodEvent *ev)
{
    Q_D(QQuickTextInput);
    const bool wasComposing = d->hasImState;
    if (d->m_readOnly) {
        ev->ignore();
    } else {
        d->processInputMethodEvent(ev);
    }
    if (!ev->isAccepted())
        QQuickImplicitSizeItem::inputMethodEvent(ev);

    if (wasComposing != d->hasImState)
        emit inputMethodComposingChanged();
}
#endif

void QQuickTextInput::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickTextInput);

    if (d->selectByMouse && event->button() == Qt::LeftButton) {
#ifndef QT_NO_IM
        d->commitPreedit();
#endif
        int cursor = d->positionAt(event->localPos());
        d->selectWordAtPos(cursor);
        event->setAccepted(true);
        if (!d->hasPendingTripleClick()) {
            d->tripleClickStartPoint = event->localPos();
            d->tripleClickTimer.start();
        }
    } else {
        if (d->sendMouseEventToInputContext(event))
            return;
        QQuickImplicitSizeItem::mouseDoubleClickEvent(event);
    }
}

void QQuickTextInput::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickTextInput);

    d->pressPos = event->localPos();

    if (d->sendMouseEventToInputContext(event))
        return;

    if (d->selectByMouse) {
        setKeepMouseGrab(false);
        d->selectPressed = true;
        QPointF distanceVector = d->pressPos - d->tripleClickStartPoint;
        if (d->hasPendingTripleClick()
            && distanceVector.manhattanLength() < qApp->styleHints()->startDragDistance()) {
            event->setAccepted(true);
            selectAll();
            return;
        }
    }

    bool mark = (event->modifiers() & Qt::ShiftModifier) && d->selectByMouse;
    int cursor = d->positionAt(event->localPos());
    d->moveCursor(cursor, mark);

    if (d->focusOnPress) {
        bool hadActiveFocus = hasActiveFocus();
        forceActiveFocus();
#ifndef QT_NO_IM
        // re-open input panel on press if already focused
        if (hasActiveFocus() && hadActiveFocus && !d->m_readOnly)
            qGuiApp->inputMethod()->show();
#endif
    }

    event->setAccepted(true);
}

void QQuickTextInput::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickTextInput);

    if (d->selectPressed) {
        if (qAbs(int(event->localPos().x() - d->pressPos.x())) > qApp->styleHints()->startDragDistance())
            setKeepMouseGrab(true);

#ifndef QT_NO_IM
        if (d->composeMode()) {
            // start selection
            int startPos = d->positionAt(d->pressPos);
            int currentPos = d->positionAt(event->localPos());
            if (startPos != currentPos)
                d->setSelection(startPos, currentPos - startPos);
        } else
#endif
        {
            moveCursorSelection(d->positionAt(event->localPos()), d->mouseSelectionMode);
        }
        event->setAccepted(true);
    } else {
        QQuickImplicitSizeItem::mouseMoveEvent(event);
    }
}

void QQuickTextInput::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickTextInput);
    if (d->sendMouseEventToInputContext(event))
        return;
    if (d->selectPressed) {
        d->selectPressed = false;
        setKeepMouseGrab(false);
    }
#ifndef QT_NO_CLIPBOARD
    if (QGuiApplication::clipboard()->supportsSelection()) {
        if (event->button() == Qt::LeftButton) {
            d->copy(QClipboard::Selection);
        } else if (!d->m_readOnly && event->button() == Qt::MidButton) {
            d->deselect();
            d->insert(QGuiApplication::clipboard()->text(QClipboard::Selection));
        }
    }
#endif
    if (!event->isAccepted())
        QQuickImplicitSizeItem::mouseReleaseEvent(event);
}

bool QQuickTextInputPrivate::sendMouseEventToInputContext(QMouseEvent *event)
{
#if !defined QT_NO_IM
    if (composeMode()) {
        int tmp_cursor = positionAt(event->localPos());
        int mousePos = tmp_cursor - m_cursor;
        if (mousePos >= 0 && mousePos <= m_textLayout.preeditAreaText().length()) {
            if (event->type() == QEvent::MouseButtonRelease) {
                qApp->inputMethod()->invokeAction(QInputMethod::Click, mousePos);
            }
            return true;
        }
    }
#else
    Q_UNUSED(event);
#endif

    return false;
}

void QQuickTextInput::mouseUngrabEvent()
{
    Q_D(QQuickTextInput);
    d->selectPressed = false;
    setKeepMouseGrab(false);
}

bool QQuickTextInput::event(QEvent* ev)
{
#ifndef QT_NO_SHORTCUT
    Q_D(QQuickTextInput);
    if (ev->type() == QEvent::ShortcutOverride) {
        if (d->m_readOnly)
            return false;
        QKeyEvent* ke = static_cast<QKeyEvent*>(ev);
        if (ke == QKeySequence::Copy
            || ke == QKeySequence::Paste
            || ke == QKeySequence::Cut
            || ke == QKeySequence::Redo
            || ke == QKeySequence::Undo
            || ke == QKeySequence::MoveToNextWord
            || ke == QKeySequence::MoveToPreviousWord
            || ke == QKeySequence::MoveToStartOfDocument
            || ke == QKeySequence::MoveToEndOfDocument
            || ke == QKeySequence::SelectNextWord
            || ke == QKeySequence::SelectPreviousWord
            || ke == QKeySequence::SelectStartOfLine
            || ke == QKeySequence::SelectEndOfLine
            || ke == QKeySequence::SelectStartOfBlock
            || ke == QKeySequence::SelectEndOfBlock
            || ke == QKeySequence::SelectStartOfDocument
            || ke == QKeySequence::SelectAll
            || ke == QKeySequence::SelectEndOfDocument) {
            ke->accept();
            return true;
        } else if (ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::ShiftModifier
                   || ke->modifiers() == Qt::KeypadModifier) {
            if (ke->key() < Qt::Key_Escape) {
                ke->accept();
                return true;
            } else {
                switch (ke->key()) {
                case Qt::Key_Delete:
                case Qt::Key_Home:
                case Qt::Key_End:
                case Qt::Key_Backspace:
                case Qt::Key_Left:
                case Qt::Key_Right:
                    ke->accept();
                    return true;
                default:
                    break;
                }
            }
        }
    }
#endif

    return QQuickImplicitSizeItem::event(ev);
}

void QQuickTextInput::geometryChanged(const QRectF &newGeometry,
                                  const QRectF &oldGeometry)
{
    Q_D(QQuickTextInput);
    if (!d->inLayout) {
        if (newGeometry.width() != oldGeometry.width())
            d->updateLayout();
        else if (newGeometry.height() != oldGeometry.height() && d->vAlign != QQuickTextInput::AlignTop)
            d->updateBaselineOffset();
        updateCursorRectangle();
    }
    QQuickImplicitSizeItem::geometryChanged(newGeometry, oldGeometry);
}

void QQuickTextInputPrivate::updateHorizontalScroll()
{
    Q_Q(QQuickTextInput);
#ifndef QT_NO_IM
    QTextLine currentLine = m_textLayout.lineForTextPosition(m_cursor + m_preeditCursor);
    const int preeditLength = m_textLayout.preeditAreaText().length();
#else
    QTextLine currentLine = m_textLayout.lineForTextPosition(m_cursor);
#endif
    const qreal width = qMax<qreal>(0, q->width());
    qreal cix = 0;
    qreal widthUsed = 0;
    if (currentLine.isValid()) {
#ifndef QT_NO_IM
        cix = currentLine.cursorToX(m_cursor + preeditLength);
#else
        cix = currentLine.cursorToX(m_cursor);
#endif
        const qreal cursorWidth = cix >= 0 ? cix : width - cix;
        widthUsed = qMax(currentLine.naturalTextWidth(), cursorWidth);
    }
    int previousScroll = hscroll;

    if (!autoScroll || widthUsed <=  width || m_echoMode == QQuickTextInput::NoEcho) {
        hscroll = 0;
    } else {
        Q_ASSERT(currentLine.isValid());
        if (cix - hscroll >= width) {
            // text doesn't fit, cursor is to the right of br (scroll right)
            hscroll = cix - width;
        } else if (cix - hscroll < 0 && hscroll < widthUsed) {
            // text doesn't fit, cursor is to the left of br (scroll left)
            hscroll = cix;
        } else if (widthUsed - hscroll < width) {
            // text doesn't fit, text document is to the left of br; align
            // right
            hscroll = widthUsed - width;
        } else if (width - hscroll > widthUsed) {
            // text doesn't fit, text document is to the right of br; align
            // left
            hscroll = width - widthUsed;
        }
#ifndef QT_NO_IM
        if (preeditLength > 0) {
            // check to ensure long pre-edit text doesn't push the cursor
            // off to the left
             cix = currentLine.cursorToX(m_cursor + qMax(0, m_preeditCursor - 1));
             if (cix < hscroll)
                 hscroll = cix;
        }
#endif
    }
    if (previousScroll != hscroll)
        textLayoutDirty = true;
}

void QQuickTextInputPrivate::updateVerticalScroll()
{
    Q_Q(QQuickTextInput);
#ifndef QT_NO_IM
    const int preeditLength = m_textLayout.preeditAreaText().length();
#endif
    const qreal height = qMax<qreal>(0, q->height());
    qreal heightUsed = contentSize.height();
    qreal previousScroll = vscroll;

    if (!autoScroll || heightUsed <=  height) {
        // text fits in br; use vscroll for alignment
        vscroll = -QQuickTextUtil::alignedY(
                    heightUsed, height, vAlign & ~(Qt::AlignAbsolute|Qt::AlignHorizontal_Mask));
    } else {
#ifndef QT_NO_IM
        QTextLine currentLine = m_textLayout.lineForTextPosition(m_cursor + preeditLength);
#else
        QTextLine currentLine = m_textLayout.lineForTextPosition(m_cursor);
#endif
        QRectF r = currentLine.isValid() ? currentLine.rect() : QRectF();
        qreal top = r.top();
        int bottom = r.bottom();

        if (bottom - vscroll >= height) {
            // text doesn't fit, cursor is to the below the br (scroll down)
            vscroll = bottom - height;
        } else if (top - vscroll < 0 && vscroll < heightUsed) {
            // text doesn't fit, cursor is above br (scroll up)
            vscroll = top;
        } else if (heightUsed - vscroll < height) {
            // text doesn't fit, text document is to the left of br; align
            // right
            vscroll = heightUsed - height;
        }
#ifndef QT_NO_IM
        if (preeditLength > 0) {
            // check to ensure long pre-edit text doesn't push the cursor
            // off the top
            currentLine = m_textLayout.lineForTextPosition(m_cursor + qMax(0, m_preeditCursor - 1));
            top = currentLine.isValid() ? currentLine.rect().top() : 0;
            if (top < vscroll)
                vscroll = top;
        }
#endif
    }
    if (previousScroll != vscroll)
        textLayoutDirty = true;
}

void QQuickTextInput::triggerPreprocess()
{
    Q_D(QQuickTextInput);
    if (d->updateType == QQuickTextInputPrivate::UpdateNone)
        d->updateType = QQuickTextInputPrivate::UpdateOnlyPreprocess;
    update();
}

QSGNode *QQuickTextInput::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    Q_D(QQuickTextInput);

    if (d->updateType != QQuickTextInputPrivate::UpdatePaintNode && oldNode != 0) {
        // Update done in preprocess() in the nodes
        d->updateType = QQuickTextInputPrivate::UpdateNone;
        return oldNode;
    }

    d->updateType = QQuickTextInputPrivate::UpdateNone;

    QQuickTextNode *node = static_cast<QQuickTextNode *>(oldNode);
    if (node == 0)
        node = new QQuickTextNode(this);
    d->textNode = node;

    if (!d->textLayoutDirty && oldNode != 0) {
        QSGSimpleRectNode *cursorNode = node->cursorNode();
        if (cursorNode != 0 && !isReadOnly()) {
            cursorNode->setRect(cursorRectangle());

            if (!d->cursorVisible || d->cursorItem || (!d->m_blinkStatus && d->m_blinkPeriod > 0)) {
                d->hideCursor();
            } else {
                d->showCursor();
            }
        }
    } else {
        node->setUseNativeRenderer(d->renderType == NativeRendering);
        node->deleteContent();
        node->setMatrix(QMatrix4x4());

        QPointF offset(0, 0);
        if (d->autoScroll && d->m_textLayout.lineCount() > 0) {
            QFontMetricsF fm(d->font);
            // the y offset is there to keep the baseline constant in case we have script changes in the text.
            offset = -QPoint(d->hscroll, d->vscroll + d->m_textLayout.lineAt(0).ascent() - fm.ascent());
        } else {
            offset = -QPoint(d->hscroll, d->vscroll);
        }

        if (!d->m_textLayout.text().isEmpty()
#ifndef QT_NO_IM
                || !d->m_textLayout.preeditAreaText().isEmpty()
#endif
                ) {
            node->addTextLayout(offset, &d->m_textLayout, d->color,
                                QQuickText::Normal, QColor(), QColor(),
                                d->selectionColor, d->selectedTextColor,
                                d->selectionStart(),
                                d->selectionEnd() - 1); // selectionEnd() returns first char after
                                                                 // selection
        }

        if (!isReadOnly() && d->cursorItem == 0) {
            node->setCursor(cursorRectangle(), d->color);
            if (!d->cursorVisible || (!d->m_blinkStatus && d->m_blinkPeriod > 0)) {
                d->hideCursor();
            } else {
                d->showCursor();
            }
        }

        d->textLayoutDirty = false;
    }

    return node;
}

#ifndef QT_NO_IM
QVariant QQuickTextInput::inputMethodQuery(Qt::InputMethodQuery property) const
{
    Q_D(const QQuickTextInput);
    switch (property) {
    case Qt::ImEnabled:
        return QVariant((bool)(flags() & ItemAcceptsInputMethod));
    case Qt::ImHints:
        return QVariant((int) d->effectiveInputMethodHints());
    case Qt::ImCursorRectangle:
        return cursorRectangle();
    case Qt::ImFont:
        return font();
    case Qt::ImCursorPosition:
        return QVariant(d->m_cursor);
    case Qt::ImSurroundingText:
        if (d->m_echoMode == PasswordEchoOnEdit && !d->m_passwordEchoEditing) {
            return QVariant(displayText());
        } else {
            return QVariant(d->realText());
        }
    case Qt::ImCurrentSelection:
        return QVariant(selectedText());
    case Qt::ImMaximumTextLength:
        return QVariant(maxLength());
    case Qt::ImAnchorPosition:
        if (d->selectionStart() == d->selectionEnd())
            return QVariant(d->m_cursor);
        else if (d->selectionStart() == d->m_cursor)
            return QVariant(d->selectionEnd());
        else
            return QVariant(d->selectionStart());
    default:
        return QQuickItem::inputMethodQuery(property);
    }
}
#endif // QT_NO_IM

/*!
    \qmlmethod QtQuick::TextInput::deselect()

    Removes active text selection.
*/
void QQuickTextInput::deselect()
{
    Q_D(QQuickTextInput);
    d->deselect();
}

/*!
    \qmlmethod QtQuick::TextInput::selectAll()

    Causes all text to be selected.
*/
void QQuickTextInput::selectAll()
{
    Q_D(QQuickTextInput);
    d->setSelection(0, text().length());
}

/*!
    \qmlmethod QtQuick::TextInput::isRightToLeft(int start, int end)

    Returns true if the natural reading direction of the editor text
    found between positions \a start and \a end is right to left.
*/
bool QQuickTextInput::isRightToLeft(int start, int end)
{
    if (start > end) {
        qmlInfo(this) << "isRightToLeft(start, end) called with the end property being smaller than the start.";
        return false;
    } else {
        return text().mid(start, end - start).isRightToLeft();
    }
}

#ifndef QT_NO_CLIPBOARD
/*!
    \qmlmethod QtQuick::TextInput::cut()

    Moves the currently selected text to the system clipboard.
*/
void QQuickTextInput::cut()
{
    Q_D(QQuickTextInput);
    if (!d->m_readOnly) {
        d->copy();
        d->del();
    }
}

/*!
    \qmlmethod QtQuick::TextInput::copy()

    Copies the currently selected text to the system clipboard.
*/
void QQuickTextInput::copy()
{
    Q_D(QQuickTextInput);
    d->copy();
}

/*!
    \qmlmethod QtQuick::TextInput::paste()

    Replaces the currently selected text by the contents of the system clipboard.
*/
void QQuickTextInput::paste()
{
    Q_D(QQuickTextInput);
    if (!d->m_readOnly)
        d->paste();
}
#endif // QT_NO_CLIPBOARD

/*!
    \qmlmethod QtQuick::TextInput::undo()

    Undoes the last operation if undo is \l {canUndo}{available}. Deselects any
    current selection, and updates the selection start to the current cursor
    position.
*/

void QQuickTextInput::undo()
{
    Q_D(QQuickTextInput);
    if (!d->m_readOnly) {
        d->internalUndo();
        d->finishChange(-1, true);
    }
}

/*!
    \qmlmethod QtQuick::TextInput::redo()

    Redoes the last operation if redo is \l {canRedo}{available}.
*/

void QQuickTextInput::redo()
{
    Q_D(QQuickTextInput);
    if (!d->m_readOnly) {
        d->internalRedo();
        d->finishChange();
    }
}

/*!
    \qmlmethod QtQuick::TextInput::insert(int position, string text)

    Inserts \a text into the TextInput at position.
*/

void QQuickTextInput::insert(int position, const QString &text)
{
    Q_D(QQuickTextInput);
    if (d->m_echoMode == QQuickTextInput::Password) {
        int delay = qGuiApp->styleHints()->passwordMaskDelay();
        if (delay > 0)
            d->m_passwordEchoTimer.start(delay, this);
    }
    if (position < 0 || position > d->m_text.length())
        return;

    const int priorState = d->m_undoState;

    QString insertText = text;

    if (d->hasSelectedText()) {
        d->addCommand(QQuickTextInputPrivate::Command(
                QQuickTextInputPrivate::SetSelection, d->m_cursor, 0, d->m_selstart, d->m_selend));
    }
    if (d->m_maskData) {
        insertText = d->maskString(position, insertText);
        for (int i = 0; i < insertText.length(); ++i) {
            d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::DeleteSelection, position + i, d->m_text.at(position + i), -1, -1));
            d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::Insert, position + i, insertText.at(i), -1, -1));
        }
        d->m_text.replace(position, insertText.length(), insertText);
        if (!insertText.isEmpty())
            d->m_textDirty = true;
        if (position < d->m_selend && position + insertText.length() > d->m_selstart)
            d->m_selDirty = true;
    } else {
        int remaining = d->m_maxLength - d->m_text.length();
        if (remaining != 0) {
            insertText = insertText.left(remaining);
            d->m_text.insert(position, insertText);
            for (int i = 0; i < insertText.length(); ++i)
               d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::Insert, position + i, insertText.at(i), -1, -1));
            if (d->m_cursor >= position)
                d->m_cursor += insertText.length();
            if (d->m_selstart >= position)
                d->m_selstart += insertText.length();
            if (d->m_selend >= position)
                d->m_selend += insertText.length();
            d->m_textDirty = true;
            if (position >= d->m_selstart && position <= d->m_selend)
                d->m_selDirty = true;
        }
    }

    d->addCommand(QQuickTextInputPrivate::Command(
            QQuickTextInputPrivate::SetSelection, d->m_cursor, 0, d->m_selstart, d->m_selend));
    d->finishChange(priorState);

    if (d->lastSelectionStart != d->lastSelectionEnd) {
        if (d->m_selstart != d->lastSelectionStart) {
            d->lastSelectionStart = d->m_selstart;
            emit selectionStartChanged();
        }
        if (d->m_selend != d->lastSelectionEnd) {
            d->lastSelectionEnd = d->m_selend;
            emit selectionEndChanged();
        }
    }
}

/*!
    \qmlmethod QtQuick::TextInput::remove(int start, int end)

    Removes the section of text that is between the \a start and \a end positions from the TextInput.
*/

void QQuickTextInput::remove(int start, int end)
{
    Q_D(QQuickTextInput);

    start = qBound(0, start, d->m_text.length());
    end = qBound(0, end, d->m_text.length());

    if (start > end)
        qSwap(start, end);
    else if (start == end)
        return;

    if (start < d->m_selend && end > d->m_selstart)
        d->m_selDirty = true;

    const int priorState = d->m_undoState;

    d->addCommand(QQuickTextInputPrivate::Command(
            QQuickTextInputPrivate::SetSelection, d->m_cursor, 0, d->m_selstart, d->m_selend));

    if (start <= d->m_cursor && d->m_cursor < end) {
        // cursor is within the selection. Split up the commands
        // to be able to restore the correct cursor position
        for (int i = d->m_cursor; i >= start; --i) {
            d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::DeleteSelection, i, d->m_text.at(i), -1, 1));
        }
        for (int i = end - 1; i > d->m_cursor; --i) {
            d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::DeleteSelection, i - d->m_cursor + start - 1, d->m_text.at(i), -1, -1));
        }
    } else {
        for (int i = end - 1; i >= start; --i) {
            d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::RemoveSelection, i, d->m_text.at(i), -1, -1));
        }
    }
    if (d->m_maskData) {
        d->m_text.replace(start, end - start,  d->clearString(start, end - start));
        for (int i = 0; i < end - start; ++i) {
            d->addCommand(QQuickTextInputPrivate::Command(
                    QQuickTextInputPrivate::Insert, start + i, d->m_text.at(start + i), -1, -1));
        }
    } else {
        d->m_text.remove(start, end - start);

        if (d->m_cursor > start)
            d->m_cursor -= qMin(d->m_cursor, end) - start;
        if (d->m_selstart > start)
            d->m_selstart -= qMin(d->m_selstart, end) - start;
        if (d->m_selend > end)
            d->m_selend -= qMin(d->m_selend, end) - start;
    }
    d->addCommand(QQuickTextInputPrivate::Command(
            QQuickTextInputPrivate::SetSelection, d->m_cursor, 0, d->m_selstart, d->m_selend));

    d->m_textDirty = true;
    d->finishChange(priorState);

    if (d->lastSelectionStart != d->lastSelectionEnd) {
        if (d->m_selstart != d->lastSelectionStart) {
            d->lastSelectionStart = d->m_selstart;
            emit selectionStartChanged();
        }
        if (d->m_selend != d->lastSelectionEnd) {
            d->lastSelectionEnd = d->m_selend;
            emit selectionEndChanged();
        }
    }
}


/*!
    \qmlmethod QtQuick::TextInput::selectWord()

    Causes the word closest to the current cursor position to be selected.
*/
void QQuickTextInput::selectWord()
{
    Q_D(QQuickTextInput);
    d->selectWordAtPos(d->m_cursor);
}

/*!
   \qmlproperty string QtQuick::TextInput::passwordCharacter

   This is the character displayed when echoMode is set to Password or
   PasswordEchoOnEdit. By default it is the password character used by
   the platform theme.

   If this property is set to a string with more than one character,
   the first character is used. If the string is empty, the value
   is ignored and the property is not set.
*/
QString QQuickTextInput::passwordCharacter() const
{
    Q_D(const QQuickTextInput);
    return QString(d->m_passwordCharacter);
}

void QQuickTextInput::setPasswordCharacter(const QString &str)
{
    Q_D(QQuickTextInput);
    if (str.length() < 1)
        return;
    d->m_passwordCharacter = str.constData()[0];
    if (d->m_echoMode == Password || d->m_echoMode == PasswordEchoOnEdit)
        d->updateDisplayText();
    emit passwordCharacterChanged();
}

/*!
   \qmlproperty string QtQuick::TextInput::displayText

   This is the text displayed in the TextInput.

   If \l echoMode is set to TextInput::Normal, this holds the
   same value as the TextInput::text property. Otherwise,
   this property holds the text visible to the user, while
   the \l text property holds the actual entered text.

   \readonly
*/
QString QQuickTextInput::displayText() const
{
    Q_D(const QQuickTextInput);
    return d->m_textLayout.text();
}

/*!
    \qmlproperty bool QtQuick::TextInput::selectByMouse

    Defaults to false.

    If true, the user can use the mouse to select text in some
    platform-specific way. Note that for some platforms this may
    not be an appropriate interaction (eg. may conflict with how
    the text needs to behave inside a Flickable.
*/
bool QQuickTextInput::selectByMouse() const
{
    Q_D(const QQuickTextInput);
    return d->selectByMouse;
}

void QQuickTextInput::setSelectByMouse(bool on)
{
    Q_D(QQuickTextInput);
    if (d->selectByMouse != on) {
        d->selectByMouse = on;
        emit selectByMouseChanged(on);
    }
}

/*!
    \qmlproperty enumeration QtQuick::TextInput::mouseSelectionMode

    Specifies how text should be selected using a mouse.

    \list
    \li TextInput.SelectCharacters - The selection is updated with individual characters. (Default)
    \li TextInput.SelectWords - The selection is updated with whole words.
    \endlist

    This property only applies when \l selectByMouse is true.
*/

QQuickTextInput::SelectionMode QQuickTextInput::mouseSelectionMode() const
{
    Q_D(const QQuickTextInput);
    return d->mouseSelectionMode;
}

void QQuickTextInput::setMouseSelectionMode(SelectionMode mode)
{
    Q_D(QQuickTextInput);
    if (d->mouseSelectionMode != mode) {
        d->mouseSelectionMode = mode;
        emit mouseSelectionModeChanged(mode);
    }
}

/*!
    \qmlproperty bool QtQuick::TextInput::persistentSelection

    Whether the TextInput should keep its selection when it loses active focus to another
    item in the scene. By default this is set to false;
*/

bool QQuickTextInput::persistentSelection() const
{
    Q_D(const QQuickTextInput);
    return d->persistentSelection;
}

void QQuickTextInput::setPersistentSelection(bool on)
{
    Q_D(QQuickTextInput);
    if (d->persistentSelection == on)
        return;
    d->persistentSelection = on;
    emit persistentSelectionChanged();
}

/*!
    \qmlproperty bool QtQuick::TextInput::canPaste

    Returns true if the TextInput is writable and the content of the clipboard is
    suitable for pasting into the TextInput.
*/
bool QQuickTextInput::canPaste() const
{
#if !defined(QT_NO_CLIPBOARD)
    Q_D(const QQuickTextInput);
    if (!d->canPasteValid) {
        if (const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData())
            const_cast<QQuickTextInputPrivate *>(d)->canPaste = !d->m_readOnly && mimeData->hasText();
        const_cast<QQuickTextInputPrivate *>(d)->canPasteValid = true;
    }
    return d->canPaste;
#else
    return false;
#endif
}

/*!
    \qmlproperty bool QtQuick::TextInput::canUndo

    Returns true if the TextInput is writable and there are previous operations
    that can be undone.
*/

bool QQuickTextInput::canUndo() const
{
    Q_D(const QQuickTextInput);
    return d->canUndo;
}

/*!
    \qmlproperty bool QtQuick::TextInput::canRedo

    Returns true if the TextInput is writable and there are \l {undo}{undone}
    operations that can be redone.
*/

bool QQuickTextInput::canRedo() const
{
    Q_D(const QQuickTextInput);
    return d->canRedo;
}

/*!
    \qmlproperty real QtQuick::TextInput::contentWidth

    Returns the width of the text, including the width past the width
    which is covered due to insufficient wrapping if \l wrapMode is set.
*/

qreal QQuickTextInput::contentWidth() const
{
    Q_D(const QQuickTextInput);
    return d->contentSize.width();
}

/*!
    \qmlproperty real QtQuick::TextInput::contentHeight

    Returns the height of the text, including the height past the height
    that is covered if the text does not fit within the set height.
*/

qreal QQuickTextInput::contentHeight() const
{
    Q_D(const QQuickTextInput);
    return d->contentSize.height();
}

void QQuickTextInput::moveCursorSelection(int position)
{
    Q_D(QQuickTextInput);
    d->moveCursor(position, true);
}

/*!
    \qmlmethod QtQuick::TextInput::moveCursorSelection(int position, SelectionMode mode = TextInput.SelectCharacters)

    Moves the cursor to \a position and updates the selection according to the optional \a mode
    parameter.  (To only move the cursor, set the \l cursorPosition property.)

    When this method is called it additionally sets either the
    selectionStart or the selectionEnd (whichever was at the previous cursor position)
    to the specified position. This allows you to easily extend and contract the selected
    text range.

    The selection mode specifies whether the selection is updated on a per character or a per word
    basis.  If not specified the selection mode will default to TextInput.SelectCharacters.

    \list
    \li TextInput.SelectCharacters - Sets either the selectionStart or selectionEnd (whichever was at
    the previous cursor position) to the specified position.
    \li TextInput.SelectWords - Sets the selectionStart and selectionEnd to include all
    words between the specified position and the previous cursor position.  Words partially in the
    range are included.
    \endlist

    For example, take this sequence of calls:

    \code
        cursorPosition = 5
        moveCursorSelection(9, TextInput.SelectCharacters)
        moveCursorSelection(7, TextInput.SelectCharacters)
    \endcode

    This moves the cursor to position 5, extend the selection end from 5 to 9
    and then retract the selection end from 9 to 7, leaving the text from position 5 to 7
    selected (the 6th and 7th characters).

    The same sequence with TextInput.SelectWords will extend the selection start to a word boundary
    before or on position 5 and extend the selection end to a word boundary on or past position 9.
*/
void QQuickTextInput::moveCursorSelection(int pos, SelectionMode mode)
{
    Q_D(QQuickTextInput);

    if (mode == SelectCharacters) {
        d->moveCursor(pos, true);
    } else if (pos != d->m_cursor){
        const int cursor = d->m_cursor;
        int anchor;
        if (!d->hasSelectedText())
            anchor = d->m_cursor;
        else if (d->selectionStart() == d->m_cursor)
            anchor = d->selectionEnd();
        else
            anchor = d->selectionStart();

        if (anchor < pos || (anchor == pos && cursor < pos)) {
            const QString text = this->text();
            QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
            finder.setPosition(anchor);

            const QTextBoundaryFinder::BoundaryReasons reasons = finder.boundaryReasons();
            if (anchor < text.length() && (reasons == QTextBoundaryFinder::NotAtBoundary
                                           || (reasons & QTextBoundaryFinder::EndOfItem))) {
                finder.toPreviousBoundary();
            }
            anchor = finder.position() != -1 ? finder.position() : 0;

            finder.setPosition(pos);
            if (pos > 0 && !finder.boundaryReasons())
                finder.toNextBoundary();
            const int cursor = finder.position() != -1 ? finder.position() : text.length();

            d->setSelection(anchor, cursor - anchor);
        } else if (anchor > pos || (anchor == pos && cursor > pos)) {
            const QString text = this->text();
            QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
            finder.setPosition(anchor);

            const QTextBoundaryFinder::BoundaryReasons reasons = finder.boundaryReasons();
            if (anchor > 0 && (reasons == QTextBoundaryFinder::NotAtBoundary
                               || (reasons & QTextBoundaryFinder::StartOfItem))) {
                finder.toNextBoundary();
            }
            anchor = finder.position() != -1 ? finder.position() : text.length();

            finder.setPosition(pos);
            if (pos < text.length() && !finder.boundaryReasons())
                 finder.toPreviousBoundary();
            const int cursor = finder.position() != -1 ? finder.position() : 0;

            d->setSelection(anchor, cursor - anchor);
        }
    }
}

void QQuickTextInput::focusInEvent(QFocusEvent *event)
{
    Q_D(QQuickTextInput);
    d->handleFocusEvent(event);
    QQuickImplicitSizeItem::focusInEvent(event);
}

void QQuickTextInputPrivate::handleFocusEvent(QFocusEvent *event)
{
    Q_Q(QQuickTextInput);
    bool focus = event->gotFocus();
    q->setCursorVisible(focus);
    if (focus) {
        q->q_updateAlignment();
#ifndef QT_NO_IM
        if (focusOnPress && !m_readOnly)
            qGuiApp->inputMethod()->show();
        q->connect(qApp->inputMethod(), SIGNAL(inputDirectionChanged(Qt::LayoutDirection)),
                   q, SLOT(q_updateAlignment()));
#endif
    } else {
        if ((m_passwordEchoEditing || m_passwordEchoTimer.isActive())) {
            updatePasswordEchoEditing(false);//QQuickTextInputPrivate sets it on key events, but doesn't deal with focus events
        }

        if (event->reason() != Qt::ActiveWindowFocusReason
                && event->reason() != Qt::PopupFocusReason
                && hasSelectedText()
                && !persistentSelection)
            deselect();

        if (q->hasAcceptableInput() || fixup())
            emit q->editingFinished();

#ifndef QT_NO_IM
        q->disconnect(qApp->inputMethod(), SIGNAL(inputDirectionChanged(Qt::LayoutDirection)),
                      q, SLOT(q_updateAlignment()));
#endif
    }
}

void QQuickTextInput::focusOutEvent(QFocusEvent *event)
{
    Q_D(QQuickTextInput);
    d->handleFocusEvent(event);
    QQuickImplicitSizeItem::focusOutEvent(event);
}

#ifndef QT_NO_IM
/*!
    \qmlproperty bool QtQuick::TextInput::inputMethodComposing


    This property holds whether the TextInput has partial text input from an
    input method.

    While it is composing an input method may rely on mouse or key events from
    the TextInput to edit or commit the partial text.  This property can be
    used to determine when to disable events handlers that may interfere with
    the correct operation of an input method.
*/
bool QQuickTextInput::isInputMethodComposing() const
{
    Q_D(const QQuickTextInput);
    return d->hasImState;
}
#endif

void QQuickTextInputPrivate::init()
{
    Q_Q(QQuickTextInput);
#ifndef QT_NO_CLIPBOARD
    if (QGuiApplication::clipboard()->supportsSelection())
        q->setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    else
#endif
        q->setAcceptedMouseButtons(Qt::LeftButton);

#ifndef QT_NO_IM
    q->setFlag(QQuickItem::ItemAcceptsInputMethod);
#endif
    q->setFlag(QQuickItem::ItemHasContents);
#ifndef QT_NO_CLIPBOARD
    q->connect(QGuiApplication::clipboard(), SIGNAL(dataChanged()),
            q, SLOT(q_canPasteChanged()));
#endif // QT_NO_CLIPBOARD

    lastSelectionStart = 0;
    lastSelectionEnd = 0;
    determineHorizontalAlignment();

    if (!qmlDisableDistanceField()) {
        QTextOption option = m_textLayout.textOption();
        option.setUseDesignMetrics(renderType != QQuickTextInput::NativeRendering);
        m_textLayout.setTextOption(option);
    }
}

void QQuickTextInput::updateCursorRectangle()
{
    Q_D(QQuickTextInput);
    if (!isComponentComplete())
        return;

    d->updateHorizontalScroll();
    d->updateVerticalScroll();
    d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
    update();
    emit cursorRectangleChanged();
    if (d->cursorItem) {
        QRectF r = cursorRectangle();
        d->cursorItem->setPosition(r.topLeft());
        d->cursorItem->setHeight(r.height());
    }
#ifndef QT_NO_IM
    updateInputMethod(Qt::ImCursorRectangle);
#endif
}

void QQuickTextInput::selectionChanged()
{
    Q_D(QQuickTextInput);
    d->textLayoutDirty = true; //TODO: Only update rect in selection
    d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
    update();
    emit selectedTextChanged();

    if (d->lastSelectionStart != d->selectionStart()) {
        d->lastSelectionStart = d->selectionStart();
        if (d->lastSelectionStart == -1)
            d->lastSelectionStart = d->m_cursor;
        emit selectionStartChanged();
    }
    if (d->lastSelectionEnd != d->selectionEnd()) {
        d->lastSelectionEnd = d->selectionEnd();
        if (d->lastSelectionEnd == -1)
            d->lastSelectionEnd = d->m_cursor;
        emit selectionEndChanged();
    }
}

void QQuickTextInputPrivate::showCursor()
{
    if (textNode != 0 && textNode->cursorNode() != 0)
        textNode->cursorNode()->setColor(color);
}

void QQuickTextInputPrivate::hideCursor()
{
    if (textNode != 0 && textNode->cursorNode() != 0)
        textNode->cursorNode()->setColor(QColor(0, 0, 0, 0));
}

QRectF QQuickTextInput::boundingRect() const
{
    Q_D(const QQuickTextInput);

    int cursorWidth = d->cursorItem ? 0 : 1;

    qreal hscroll = d->hscroll;
    if (!d->autoScroll || d->contentSize.width() < width())
        hscroll -= QQuickTextUtil::alignedX(d->contentSize.width(), width(), effectiveHAlign());

    // Could include font max left/right bearings to either side of rectangle.
    QRectF r(-hscroll, -d->vscroll, d->contentSize.width(), d->contentSize.height());
    r.setRight(r.right() + cursorWidth);
    return r;
}

QRectF QQuickTextInput::clipRect() const
{
    Q_D(const QQuickTextInput);

    int cursorWidth = d->cursorItem ? d->cursorItem->width() : 1;

    // Could include font max left/right bearings to either side of rectangle.
    QRectF r = QQuickImplicitSizeItem::clipRect();
    r.setRight(r.right() + cursorWidth);
    return r;
}

void QQuickTextInput::q_canPasteChanged()
{
    Q_D(QQuickTextInput);
    bool old = d->canPaste;
#ifndef QT_NO_CLIPBOARD
    if (const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData())
        d->canPaste = !d->m_readOnly && mimeData->hasText();
    else
        d->canPaste = false;
#endif

    bool changed = d->canPaste != old || !d->canPasteValid;
    d->canPasteValid = true;
    if (changed)
        emit canPasteChanged();

}

void QQuickTextInput::q_updateAlignment()
{
    Q_D(QQuickTextInput);
    if (d->determineHorizontalAlignment()) {
        d->updateLayout();
        updateCursorRectangle();
    }
}

/*!
    \internal

    Updates the display text based of the current edit text
    If the text has changed will emit displayTextChanged()
*/
void QQuickTextInputPrivate::updateDisplayText(bool forceUpdate)
{
    QString orig = m_textLayout.text();
    QString str;
    if (m_echoMode == QQuickTextInput::NoEcho)
        str = QString::fromLatin1("");
    else
        str = m_text;

    if (m_echoMode == QQuickTextInput::Password) {
         str.fill(m_passwordCharacter);
        if (m_passwordEchoTimer.isActive() && m_cursor > 0 && m_cursor <= m_text.length()) {
            int cursor = m_cursor - 1;
            QChar uc = m_text.at(cursor);
            str[cursor] = uc;
            if (cursor > 0 && uc.unicode() >= 0xdc00 && uc.unicode() < 0xe000) {
                // second half of a surrogate, check if we have the first half as well,
                // if yes restore both at once
                uc = m_text.at(cursor - 1);
                if (uc.unicode() >= 0xd800 && uc.unicode() < 0xdc00)
                    str[cursor - 1] = uc;
            }
        }
    } else if (m_echoMode == QQuickTextInput::PasswordEchoOnEdit && !m_passwordEchoEditing) {
        str.fill(m_passwordCharacter);
    }

    // replace certain non-printable characters with spaces (to avoid
    // drawing boxes when using fonts that don't have glyphs for such
    // characters)
    QChar* uc = str.data();
    for (int i = 0; i < (int)str.length(); ++i) {
        if ((uc[i] < 0x20 && uc[i] != 0x09)
            || uc[i] == QChar::LineSeparator
            || uc[i] == QChar::ParagraphSeparator
            || uc[i] == QChar::ObjectReplacementCharacter)
            uc[i] = QChar(0x0020);
    }

    if (str != orig || forceUpdate) {
        m_textLayout.setText(str);
        updateLayout(); // polish?
        emit q_func()->displayTextChanged();
    }
}

qreal QQuickTextInputPrivate::getImplicitWidth() const
{
    Q_Q(const QQuickTextInput);
    if (!requireImplicitWidth) {
        QQuickTextInputPrivate *d = const_cast<QQuickTextInputPrivate *>(this);
        d->requireImplicitWidth = true;

        if (q->isComponentComplete()) {
            // One time cost, only incurred if implicitWidth is first requested after
            // componentComplete.
            QTextLayout layout(m_text);

            QTextOption option = m_textLayout.textOption();
            option.setTextDirection(m_layoutDirection);
            option.setFlags(QTextOption::IncludeTrailingSpaces);
            option.setWrapMode(QTextOption::WrapMode(wrapMode));
            option.setAlignment(Qt::Alignment(q->effectiveHAlign()));
            layout.setTextOption(option);
            layout.setFont(font);
#ifndef QT_NO_IM
            layout.setPreeditArea(m_textLayout.preeditAreaPosition(), m_textLayout.preeditAreaText());
#endif
            layout.beginLayout();

            QTextLine line = layout.createLine();
            line.setLineWidth(INT_MAX);
            d->implicitWidth = qCeil(line.naturalTextWidth());

            layout.endLayout();
        }
    }
    return implicitWidth;
}

void QQuickTextInputPrivate::updateLayout()
{
    Q_Q(QQuickTextInput);

    if (!q->isComponentComplete())
        return;


    QTextOption option = m_textLayout.textOption();
    option.setTextDirection(layoutDirection());
    option.setWrapMode(QTextOption::WrapMode(wrapMode));
    option.setAlignment(Qt::Alignment(q->effectiveHAlign()));
    if (!qmlDisableDistanceField())
        option.setUseDesignMetrics(renderType != QQuickTextInput::NativeRendering);

    m_textLayout.setTextOption(option);
    m_textLayout.setFont(font);

    m_textLayout.beginLayout();

    QTextLine line = m_textLayout.createLine();
    if (requireImplicitWidth) {
        line.setLineWidth(INT_MAX);
        const bool wasInLayout = inLayout;
        inLayout = true;
        q->setImplicitWidth(qCeil(line.naturalTextWidth()));
        inLayout = wasInLayout;
        if (inLayout)       // probably the result of a binding loop, but by letting it
            return;         // get this far we'll get a warning to that effect.
    }
    qreal lineWidth = q->widthValid() ? q->width() : INT_MAX;
    qreal height = 0;
    qreal width = 0;
    do {
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));

        height += line.height();
        width = qMax(width, line.naturalTextWidth());

        line = m_textLayout.createLine();
    } while (line.isValid());
    m_textLayout.endLayout();

    option.setWrapMode(QTextOption::NoWrap);
    m_textLayout.setTextOption(option);

    textLayoutDirty = true;

    const QSizeF previousSize = contentSize;
    contentSize = QSizeF(width, height);

    updateType = UpdatePaintNode;
    q->update();

    if (!requireImplicitWidth && !q->widthValid())
        q->setImplicitSize(width, height);
    else
        q->setImplicitHeight(height);

    updateBaselineOffset();

    if (previousSize != contentSize)
        emit q->contentSizeChanged();
}

/*!
    \internal
    \brief QQuickTextInputPrivate::updateBaselineOffset

    Assumes contentSize.height() is already calculated.
 */
void QQuickTextInputPrivate::updateBaselineOffset()
{
    Q_Q(QQuickTextInput);
    if (!q->isComponentComplete())
        return;
    QFontMetricsF fm(font);
    qreal yoff = 0;
    if (q->heightValid()) {
        const qreal surplusHeight = q->height() - contentSize.height();
        if (vAlign == QQuickTextInput::AlignBottom)
            yoff = surplusHeight;
        else if (vAlign == QQuickTextInput::AlignVCenter)
            yoff = surplusHeight/2;
    }
    q->setBaselineOffset(fm.ascent() + yoff);
}

#ifndef QT_NO_CLIPBOARD
/*!
    \internal

    Copies the currently selected text into the clipboard using the given
    \a mode.

    \note If the echo mode is set to a mode other than Normal then copy
    will not work.  This is to prevent using copy as a method of bypassing
    password features of the line control.
*/
void QQuickTextInputPrivate::copy(QClipboard::Mode mode) const
{
    QString t = selectedText();
    if (!t.isEmpty() && m_echoMode == QQuickTextInput::Normal) {
        QGuiApplication::clipboard()->setText(t, mode);
    }
}

/*!
    \internal

    Inserts the text stored in the application clipboard into the line
    control.

    \sa insert()
*/
void QQuickTextInputPrivate::paste(QClipboard::Mode clipboardMode)
{
    QString clip = QGuiApplication::clipboard()->text(clipboardMode);
    if (!clip.isEmpty() || hasSelectedText()) {
        separate(); //make it a separate undo/redo command
        insert(clip);
        separate();
    }
}

#endif // !QT_NO_CLIPBOARD

#ifndef QT_NO_IM
/*!
    \internal
*/
void QQuickTextInputPrivate::commitPreedit()
{
    Q_Q(QQuickTextInput);

    if (!hasImState)
        return;

    qApp->inputMethod()->commit();

    if (!hasImState)
        return;

    QInputMethodEvent ev;
    QCoreApplication::sendEvent(q, &ev);
}

void QQuickTextInputPrivate::cancelPreedit()
{
    Q_Q(QQuickTextInput);

    if (!hasImState)
        return;

    qApp->inputMethod()->reset();

    QInputMethodEvent ev;
    QCoreApplication::sendEvent(q, &ev);
}
#endif // QT_NO_IM

/*!
    \internal

    Handles the behavior for the backspace key or function.
    Removes the current selection if there is a selection, otherwise
    removes the character prior to the cursor position.

    \sa del()
*/
void QQuickTextInputPrivate::backspace()
{
    int priorState = m_undoState;
    if (separateSelection()) {
        removeSelectedText();
    } else if (m_cursor) {
            --m_cursor;
            if (m_maskData)
                m_cursor = prevMaskBlank(m_cursor);
            QChar uc = m_text.at(m_cursor);
            if (m_cursor > 0 && uc.unicode() >= 0xdc00 && uc.unicode() < 0xe000) {
                // second half of a surrogate, check if we have the first half as well,
                // if yes delete both at once
                uc = m_text.at(m_cursor - 1);
                if (uc.unicode() >= 0xd800 && uc.unicode() < 0xdc00) {
                    internalDelete(true);
                    --m_cursor;
                }
            }
            internalDelete(true);
    }
    finishChange(priorState);
}

/*!
    \internal

    Handles the behavior for the delete key or function.
    Removes the current selection if there is a selection, otherwise
    removes the character after the cursor position.

    \sa del()
*/
void QQuickTextInputPrivate::del()
{
    int priorState = m_undoState;
    if (separateSelection()) {
        removeSelectedText();
    } else {
        int n = m_textLayout.nextCursorPosition(m_cursor) - m_cursor;
        while (n--)
            internalDelete();
    }
    finishChange(priorState);
}

/*!
    \internal

    Inserts the given \a newText at the current cursor position.
    If there is any selected text it is removed prior to insertion of
    the new text.
*/
void QQuickTextInputPrivate::insert(const QString &newText)
{
    int priorState = m_undoState;
    if (separateSelection())
        removeSelectedText();
    internalInsert(newText);
    finishChange(priorState);
}

/*!
    \internal

    Clears the line control text.
*/
void QQuickTextInputPrivate::clear()
{
    int priorState = m_undoState;
    separateSelection();
    m_selstart = 0;
    m_selend = m_text.length();
    removeSelectedText();
    separate();
    finishChange(priorState, /*update*/false, /*edited*/false);
}

/*!
    \internal

    Sets \a length characters from the given \a start position as selected.
    The given \a start position must be within the current text for
    the line control.  If \a length characters cannot be selected, then
    the selection will extend to the end of the current text.
*/
void QQuickTextInputPrivate::setSelection(int start, int length)
{
    Q_Q(QQuickTextInput);
#ifndef QT_NO_IM
    commitPreedit();
#endif

    if (start < 0 || start > (int)m_text.length()){
        qWarning("QQuickTextInputPrivate::setSelection: Invalid start position");
        return;
    }

    if (length > 0) {
        if (start == m_selstart && start + length == m_selend && m_cursor == m_selend)
            return;
        m_selstart = start;
        m_selend = qMin(start + length, (int)m_text.length());
        m_cursor = m_selend;
    } else if (length < 0){
        if (start == m_selend && start + length == m_selstart && m_cursor == m_selstart)
            return;
        m_selstart = qMax(start + length, 0);
        m_selend = start;
        m_cursor = m_selstart;
    } else if (m_selstart != m_selend) {
        m_selstart = 0;
        m_selend = 0;
        m_cursor = start;
    } else {
        m_cursor = start;
        emitCursorPositionChanged();
        return;
    }
    emit q->selectionChanged();
    emitCursorPositionChanged();
#ifndef QT_NO_IM
    q->updateInputMethod(Qt::ImCursorRectangle | Qt::ImAnchorPosition
                        | Qt::ImCursorPosition | Qt::ImCurrentSelection);
#endif
}

/*!
    \internal

    Sets the password echo editing to \a editing.  If password echo editing
    is true, then the text of the password is displayed even if the echo
    mode is set to QLineEdit::PasswordEchoOnEdit.  Password echoing editing
    does not affect other echo modes.
*/
void QQuickTextInputPrivate::updatePasswordEchoEditing(bool editing)
{
    cancelPasswordEchoTimer();
    m_passwordEchoEditing = editing;
    updateDisplayText();
}

/*!
    \internal

    Fixes the current text so that it is valid given any set validators.

    Returns true if the text was changed.  Otherwise returns false.
*/
bool QQuickTextInputPrivate::fixup() // this function assumes that validate currently returns != Acceptable
{
#ifndef QT_NO_VALIDATOR
    if (m_validator) {
        QString textCopy = m_text;
        int cursorCopy = m_cursor;
        m_validator->fixup(textCopy);
        if (m_validator->validate(textCopy, cursorCopy) == QValidator::Acceptable) {
            if (textCopy != m_text || cursorCopy != m_cursor)
                internalSetText(textCopy, cursorCopy);
            return true;
        }
    }
#endif
    return false;
}

/*!
    \internal

    Moves the cursor to the given position \a pos.   If \a mark is true will
    adjust the currently selected text.
*/
void QQuickTextInputPrivate::moveCursor(int pos, bool mark)
{
    Q_Q(QQuickTextInput);
#ifndef QT_NO_IM
    commitPreedit();
#endif

    if (pos != m_cursor) {
        separate();
        if (m_maskData)
            pos = pos > m_cursor ? nextMaskBlank(pos) : prevMaskBlank(pos);
    }
    if (mark) {
        int anchor;
        if (m_selend > m_selstart && m_cursor == m_selstart)
            anchor = m_selend;
        else if (m_selend > m_selstart && m_cursor == m_selend)
            anchor = m_selstart;
        else
            anchor = m_cursor;
        m_selstart = qMin(anchor, pos);
        m_selend = qMax(anchor, pos);
    } else {
        internalDeselect();
    }
    m_cursor = pos;
    if (mark || m_selDirty) {
        m_selDirty = false;
        emit q->selectionChanged();
    }
    emitCursorPositionChanged();
#ifndef QT_NO_IM
    q->updateInputMethod();
#endif
}

#ifndef QT_NO_IM
/*!
    \internal

    Applies the given input method event \a event to the text of the line
    control
*/
void QQuickTextInputPrivate::processInputMethodEvent(QInputMethodEvent *event)
{
    Q_Q(QQuickTextInput);

    int priorState = -1;
    bool isGettingInput = !event->commitString().isEmpty()
            || event->preeditString() != preeditAreaText()
            || event->replacementLength() > 0;
    bool cursorPositionChanged = false;
    bool selectionChange = false;
    m_preeditDirty = event->preeditString() != preeditAreaText();

    if (isGettingInput) {
        // If any text is being input, remove selected text.
        priorState = m_undoState;
        separateSelection();
        if (m_echoMode == QQuickTextInput::PasswordEchoOnEdit && !m_passwordEchoEditing) {
            updatePasswordEchoEditing(true);
            m_selstart = 0;
            m_selend = m_text.length();
        }
        removeSelectedText();
    }

    int c = m_cursor; // cursor position after insertion of commit string
    if (event->replacementStart() <= 0)
        c += event->commitString().length() - qMin(-event->replacementStart(), event->replacementLength());

    m_cursor += event->replacementStart();
    if (m_cursor < 0)
        m_cursor = 0;

    // insert commit string
    if (event->replacementLength()) {
        m_selstart = m_cursor;
        m_selend = m_selstart + event->replacementLength();
        m_selend = qMin(m_selend, m_text.length());
        removeSelectedText();
    }
    if (!event->commitString().isEmpty()) {
        internalInsert(event->commitString());
        cursorPositionChanged = true;
    }

    m_cursor = qBound(0, c, m_text.length());

    for (int i = 0; i < event->attributes().size(); ++i) {
        const QInputMethodEvent::Attribute &a = event->attributes().at(i);
        if (a.type == QInputMethodEvent::Selection) {
            m_cursor = qBound(0, a.start + a.length, m_text.length());
            if (a.length) {
                m_selstart = qMax(0, qMin(a.start, m_text.length()));
                m_selend = m_cursor;
                if (m_selend < m_selstart) {
                    qSwap(m_selstart, m_selend);
                }
                selectionChange = true;
            } else {
                m_selstart = m_selend = 0;
            }
            cursorPositionChanged = true;
        }
    }
    m_textLayout.setPreeditArea(m_cursor, event->preeditString());
    const int oldPreeditCursor = m_preeditCursor;
    m_preeditCursor = event->preeditString().length();
    hasImState = !event->preeditString().isEmpty();
    bool cursorVisible = true;
    QList<QTextLayout::FormatRange> formats;
    for (int i = 0; i < event->attributes().size(); ++i) {
        const QInputMethodEvent::Attribute &a = event->attributes().at(i);
        if (a.type == QInputMethodEvent::Cursor) {
            hasImState = true;
            m_preeditCursor = a.start;
            cursorVisible = a.length != 0;
        } else if (a.type == QInputMethodEvent::TextFormat) {
            hasImState = true;
            QTextCharFormat f = qvariant_cast<QTextFormat>(a.value).toCharFormat();
            if (f.isValid()) {
                QTextLayout::FormatRange o;
                o.start = a.start + m_cursor;
                o.length = a.length;
                o.format = f;
                formats.append(o);
            }
        }
    }
    m_textLayout.setAdditionalFormats(formats);

    updateDisplayText(/*force*/ true);
    if ((cursorPositionChanged && !emitCursorPositionChanged())
            || m_preeditCursor != oldPreeditCursor
            || isGettingInput) {
        q->updateCursorRectangle();
    }

    if (isGettingInput)
        finishChange(priorState);

    q->setCursorVisible(cursorVisible);

    if (selectionChange) {
        emit q->selectionChanged();
        q->updateInputMethod(Qt::ImCursorRectangle | Qt::ImAnchorPosition
                            | Qt::ImCursorPosition | Qt::ImCurrentSelection);
    }
}
#endif // QT_NO_IM

/*!
    \internal

    Sets the selection to cover the word at the given cursor position.
    The word boundaries are defined by the behavior of QTextLayout::SkipWords
    cursor mode.
*/
void QQuickTextInputPrivate::selectWordAtPos(int cursor)
{
    int next = cursor + 1;
    if (next > end())
        --next;
    int c = m_textLayout.previousCursorPosition(next, QTextLayout::SkipWords);
    moveCursor(c, false);
    // ## text layout should support end of words.
    int end = m_textLayout.nextCursorPosition(c, QTextLayout::SkipWords);
    while (end > cursor && m_text[end-1].isSpace())
        --end;
    moveCursor(end, true);
}

/*!
    \internal

    Completes a change to the line control text.  If the change is not valid
    will undo the line control state back to the given \a validateFromState.

    If \a edited is true and the change is valid, will emit textEdited() in
    addition to textChanged().  Otherwise only emits textChanged() on a valid
    change.

    The \a update value is currently unused.
*/
bool QQuickTextInputPrivate::finishChange(int validateFromState, bool update, bool /*edited*/)
{
    Q_Q(QQuickTextInput);

    Q_UNUSED(update)
#ifndef QT_NO_IM
    bool inputMethodAttributesChanged = m_textDirty || m_selDirty;
#endif
    bool alignmentChanged = false;
    bool textChanged = false;

    if (m_textDirty) {
        // do validation
        bool wasValidInput = m_validInput;
        bool wasAcceptable = m_acceptableInput;
        m_validInput = true;
        m_acceptableInput = true;
#ifndef QT_NO_VALIDATOR
        if (m_validator) {
            QString textCopy = m_text;
            int cursorCopy = m_cursor;
            QValidator::State state = m_validator->validate(textCopy, cursorCopy);
            m_validInput = state != QValidator::Invalid;
            m_acceptableInput = state == QValidator::Acceptable;
            if (m_validInput) {
                if (m_text != textCopy) {
                    internalSetText(textCopy, cursorCopy);
                    return true;
                }
                m_cursor = cursorCopy;
            }
        }
#endif
        if (validateFromState >= 0 && wasValidInput && !m_validInput) {
            if (m_transactions.count())
                return false;
            internalUndo(validateFromState);
            m_history.resize(m_undoState);
            m_validInput = true;
            m_acceptableInput = wasAcceptable;
            m_textDirty = false;
        }

        if (m_textDirty) {
            textChanged = true;
            m_textDirty = false;
#ifndef QT_NO_IM
            m_preeditDirty = false;
#endif
            alignmentChanged = determineHorizontalAlignment();
            emit q->textChanged();
        }

        updateDisplayText(alignmentChanged);

        if (m_acceptableInput != wasAcceptable)
            emit q->acceptableInputChanged();
    }
#ifndef QT_NO_IM
    if (m_preeditDirty) {
        m_preeditDirty = false;
        if (determineHorizontalAlignment()) {
            alignmentChanged = true;
            updateLayout();
        }
    }
#endif

    if (m_selDirty) {
        m_selDirty = false;
        emit q->selectionChanged();
    }

#ifndef QT_NO_IM
    inputMethodAttributesChanged |= (m_cursor != m_lastCursorPos);
    if (inputMethodAttributesChanged)
        q->updateInputMethod();
#endif
    emitUndoRedoChanged();

    if (!emitCursorPositionChanged() && (alignmentChanged || textChanged))
        q->updateCursorRectangle();

    return true;
}

/*!
    \internal

    An internal function for setting the text of the line control.
*/
void QQuickTextInputPrivate::internalSetText(const QString &txt, int pos, bool edited)
{
    internalDeselect();
    QString oldText = m_text;
    if (m_maskData) {
        m_text = maskString(0, txt, true);
        m_text += clearString(m_text.length(), m_maxLength - m_text.length());
    } else {
        m_text = txt.isEmpty() ? txt : txt.left(m_maxLength);
    }
    m_history.clear();
    m_undoState = 0;
    m_cursor = (pos < 0 || pos > m_text.length()) ? m_text.length() : pos;
    m_textDirty = (oldText != m_text);

    bool changed = finishChange(-1, true, edited);
#ifdef QT_NO_ACCESSIBILITY
    Q_UNUSED(changed)
#else
    Q_Q(QQuickTextInput);
    if (changed && QAccessible::isActive()) {
        if (QObject *acc = QQuickAccessibleAttached::findAccessible(q, QAccessible::EditableText)) {
            QAccessibleTextUpdateEvent ev(acc, 0, oldText, m_text);
            QAccessible::updateAccessibility(&ev);
        }
    }
#endif
}


/*!
    \internal

    Adds the given \a command to the undo history
    of the line control.  Does not apply the command.
*/
void QQuickTextInputPrivate::addCommand(const Command &cmd)
{
    if (m_separator && m_undoState && m_history[m_undoState - 1].type != Separator) {
        m_history.resize(m_undoState + 2);
        m_history[m_undoState++] = Command(Separator, m_cursor, 0, m_selstart, m_selend);
    } else {
        m_history.resize(m_undoState + 1);
    }
    m_separator = false;
    m_history[m_undoState++] = cmd;
}

/*!
    \internal

    Inserts the given string \a s into the line
    control.

    Also adds the appropriate commands into the undo history.
    This function does not call finishChange(), and may leave the text
    in an invalid state.
*/
void QQuickTextInputPrivate::internalInsert(const QString &s)
{
    Q_Q(QQuickTextInput);
    if (m_echoMode == QQuickTextInput::Password) {
        int delay = qGuiApp->styleHints()->passwordMaskDelay();
        if (delay > 0)
            m_passwordEchoTimer.start(delay, q);
    }
    Q_ASSERT(!hasSelectedText());   // insert(), processInputMethodEvent() call removeSelectedText() first.
    if (m_maskData) {
        QString ms = maskString(m_cursor, s);
        for (int i = 0; i < (int) ms.length(); ++i) {
            addCommand (Command(DeleteSelection, m_cursor + i, m_text.at(m_cursor + i), -1, -1));
            addCommand(Command(Insert, m_cursor + i, ms.at(i), -1, -1));
        }
        m_text.replace(m_cursor, ms.length(), ms);
        m_cursor += ms.length();
        m_cursor = nextMaskBlank(m_cursor);
        m_textDirty = true;
    } else {
        int remaining = m_maxLength - m_text.length();
        if (remaining != 0) {
            m_text.insert(m_cursor, s.left(remaining));
            for (int i = 0; i < (int) s.left(remaining).length(); ++i)
               addCommand(Command(Insert, m_cursor++, s.at(i), -1, -1));
            m_textDirty = true;
        }
    }
}

/*!
    \internal

    deletes a single character from the current text.  If \a wasBackspace,
    the character prior to the cursor is removed.  Otherwise the character
    after the cursor is removed.

    Also adds the appropriate commands into the undo history.
    This function does not call finishChange(), and may leave the text
    in an invalid state.
*/
void QQuickTextInputPrivate::internalDelete(bool wasBackspace)
{
    if (m_cursor < (int) m_text.length()) {
        cancelPasswordEchoTimer();
        Q_ASSERT(!hasSelectedText());   // del(), backspace() call removeSelectedText() first.
        addCommand(Command((CommandType)((m_maskData ? 2 : 0) + (wasBackspace ? Remove : Delete)),
                   m_cursor, m_text.at(m_cursor), -1, -1));
        if (m_maskData) {
            m_text.replace(m_cursor, 1, clearString(m_cursor, 1));
            addCommand(Command(Insert, m_cursor, m_text.at(m_cursor), -1, -1));
        } else {
            m_text.remove(m_cursor, 1);
        }
        m_textDirty = true;
    }
}

/*!
    \internal

    removes the currently selected text from the line control.

    Also adds the appropriate commands into the undo history.
    This function does not call finishChange(), and may leave the text
    in an invalid state.
*/
void QQuickTextInputPrivate::removeSelectedText()
{
    if (m_selstart < m_selend && m_selend <= (int) m_text.length()) {
        cancelPasswordEchoTimer();
        int i ;
        if (m_selstart <= m_cursor && m_cursor < m_selend) {
            // cursor is within the selection. Split up the commands
            // to be able to restore the correct cursor position
            for (i = m_cursor; i >= m_selstart; --i)
                addCommand (Command(DeleteSelection, i, m_text.at(i), -1, 1));
            for (i = m_selend - 1; i > m_cursor; --i)
                addCommand (Command(DeleteSelection, i - m_cursor + m_selstart - 1, m_text.at(i), -1, -1));
        } else {
            for (i = m_selend-1; i >= m_selstart; --i)
                addCommand (Command(RemoveSelection, i, m_text.at(i), -1, -1));
        }
        if (m_maskData) {
            m_text.replace(m_selstart, m_selend - m_selstart,  clearString(m_selstart, m_selend - m_selstart));
            for (int i = 0; i < m_selend - m_selstart; ++i)
                addCommand(Command(Insert, m_selstart + i, m_text.at(m_selstart + i), -1, -1));
        } else {
            m_text.remove(m_selstart, m_selend - m_selstart);
        }
        if (m_cursor > m_selstart)
            m_cursor -= qMin(m_cursor, m_selend) - m_selstart;
        internalDeselect();
        m_textDirty = true;
    }
}

/*!
    \internal

    Adds the current selection to the undo history.

    Returns true if there is a current selection and false otherwise.
*/

bool QQuickTextInputPrivate::separateSelection()
{
    if (hasSelectedText()) {
        separate();
        addCommand(Command(SetSelection, m_cursor, 0, m_selstart, m_selend));
        return true;
    } else {
        return false;
    }
}

/*!
    \internal

    Parses the input mask specified by \a maskFields to generate
    the mask data used to handle input masks.
*/
void QQuickTextInputPrivate::parseInputMask(const QString &maskFields)
{
    int delimiter = maskFields.indexOf(QLatin1Char(';'));
    if (maskFields.isEmpty() || delimiter == 0) {
        if (m_maskData) {
            delete [] m_maskData;
            m_maskData = 0;
            m_maxLength = 32767;
            internalSetText(QString());
        }
        return;
    }

    if (delimiter == -1) {
        m_blank = QLatin1Char(' ');
        m_inputMask = maskFields;
    } else {
        m_inputMask = maskFields.left(delimiter);
        m_blank = (delimiter + 1 < maskFields.length()) ? maskFields[delimiter + 1] : QLatin1Char(' ');
    }

    // calculate m_maxLength / m_maskData length
    m_maxLength = 0;
    QChar c = 0;
    for (int i=0; i<m_inputMask.length(); i++) {
        c = m_inputMask.at(i);
        if (i > 0 && m_inputMask.at(i-1) == QLatin1Char('\\')) {
            m_maxLength++;
            continue;
        }
        if (c != QLatin1Char('\\') && c != QLatin1Char('!') &&
             c != QLatin1Char('<') && c != QLatin1Char('>') &&
             c != QLatin1Char('{') && c != QLatin1Char('}') &&
             c != QLatin1Char('[') && c != QLatin1Char(']'))
            m_maxLength++;
    }

    delete [] m_maskData;
    m_maskData = new MaskInputData[m_maxLength];

    MaskInputData::Casemode m = MaskInputData::NoCaseMode;
    c = 0;
    bool s;
    bool escape = false;
    int index = 0;
    for (int i = 0; i < m_inputMask.length(); i++) {
        c = m_inputMask.at(i);
        if (escape) {
            s = true;
            m_maskData[index].maskChar = c;
            m_maskData[index].separator = s;
            m_maskData[index].caseMode = m;
            index++;
            escape = false;
        } else if (c == QLatin1Char('<')) {
                m = MaskInputData::Lower;
        } else if (c == QLatin1Char('>')) {
            m = MaskInputData::Upper;
        } else if (c == QLatin1Char('!')) {
            m = MaskInputData::NoCaseMode;
        } else if (c != QLatin1Char('{') && c != QLatin1Char('}') && c != QLatin1Char('[') && c != QLatin1Char(']')) {
            switch (c.unicode()) {
            case 'A':
            case 'a':
            case 'N':
            case 'n':
            case 'X':
            case 'x':
            case '9':
            case '0':
            case 'D':
            case 'd':
            case '#':
            case 'H':
            case 'h':
            case 'B':
            case 'b':
                s = false;
                break;
            case '\\':
                escape = true;
            default:
                s = true;
                break;
            }

            if (!escape) {
                m_maskData[index].maskChar = c;
                m_maskData[index].separator = s;
                m_maskData[index].caseMode = m;
                index++;
            }
        }
    }
    internalSetText(m_text);
}


/*!
    \internal

    checks if the key is valid compared to the inputMask
*/
bool QQuickTextInputPrivate::isValidInput(QChar key, QChar mask) const
{
    switch (mask.unicode()) {
    case 'A':
        if (key.isLetter())
            return true;
        break;
    case 'a':
        if (key.isLetter() || key == m_blank)
            return true;
        break;
    case 'N':
        if (key.isLetterOrNumber())
            return true;
        break;
    case 'n':
        if (key.isLetterOrNumber() || key == m_blank)
            return true;
        break;
    case 'X':
        if (key.isPrint())
            return true;
        break;
    case 'x':
        if (key.isPrint() || key == m_blank)
            return true;
        break;
    case '9':
        if (key.isNumber())
            return true;
        break;
    case '0':
        if (key.isNumber() || key == m_blank)
            return true;
        break;
    case 'D':
        if (key.isNumber() && key.digitValue() > 0)
            return true;
        break;
    case 'd':
        if ((key.isNumber() && key.digitValue() > 0) || key == m_blank)
            return true;
        break;
    case '#':
        if (key.isNumber() || key == QLatin1Char('+') || key == QLatin1Char('-') || key == m_blank)
            return true;
        break;
    case 'B':
        if (key == QLatin1Char('0') || key == QLatin1Char('1'))
            return true;
        break;
    case 'b':
        if (key == QLatin1Char('0') || key == QLatin1Char('1') || key == m_blank)
            return true;
        break;
    case 'H':
        if (key.isNumber() || (key >= QLatin1Char('a') && key <= QLatin1Char('f')) || (key >= QLatin1Char('A') && key <= QLatin1Char('F')))
            return true;
        break;
    case 'h':
        if (key.isNumber() || (key >= QLatin1Char('a') && key <= QLatin1Char('f')) || (key >= QLatin1Char('A') && key <= QLatin1Char('F')) || key == m_blank)
            return true;
        break;
    default:
        break;
    }
    return false;
}

/*!
    \internal

    Returns true if the given text \a str is valid for any
    validator or input mask set for the line control.

    Otherwise returns false
*/
QQuickTextInputPrivate::ValidatorState QQuickTextInputPrivate::hasAcceptableInput(const QString &str) const
{
#ifndef QT_NO_VALIDATOR
    QString textCopy = str;
    int cursorCopy = m_cursor;
    if (m_validator) {
        QValidator::State state = m_validator->validate(textCopy, cursorCopy);
        if (state != QValidator::Acceptable)
            return ValidatorState(state);
    }
#endif

    if (!m_maskData)
        return AcceptableInput;

    if (str.length() != m_maxLength)
        return InvalidInput;

    for (int i=0; i < m_maxLength; ++i) {
        if (m_maskData[i].separator) {
            if (str.at(i) != m_maskData[i].maskChar)
                return InvalidInput;
        } else {
            if (!isValidInput(str.at(i), m_maskData[i].maskChar))
                return InvalidInput;
        }
    }
    return AcceptableInput;
}

/*!
    \internal

    Applies the inputMask on \a str starting from position \a pos in the mask. \a clear
    specifies from where characters should be gotten when a separator is met in \a str - true means
    that blanks will be used, false that previous input is used.
    Calling this when no inputMask is set is undefined.
*/
QString QQuickTextInputPrivate::maskString(uint pos, const QString &str, bool clear) const
{
    if (pos >= (uint)m_maxLength)
        return QString::fromLatin1("");

    QString fill;
    fill = clear ? clearString(0, m_maxLength) : m_text;

    int strIndex = 0;
    QString s = QString::fromLatin1("");
    int i = pos;
    while (i < m_maxLength) {
        if (strIndex < str.length()) {
            if (m_maskData[i].separator) {
                s += m_maskData[i].maskChar;
                if (str[(int)strIndex] == m_maskData[i].maskChar)
                    strIndex++;
                ++i;
            } else {
                if (isValidInput(str[(int)strIndex], m_maskData[i].maskChar)) {
                    switch (m_maskData[i].caseMode) {
                    case MaskInputData::Upper:
                        s += str[(int)strIndex].toUpper();
                        break;
                    case MaskInputData::Lower:
                        s += str[(int)strIndex].toLower();
                        break;
                    default:
                        s += str[(int)strIndex];
                    }
                    ++i;
                } else {
                    // search for separator first
                    int n = findInMask(i, true, true, str[(int)strIndex]);
                    if (n != -1) {
                        if (str.length() != 1 || i == 0 || (i > 0 && (!m_maskData[i-1].separator || m_maskData[i-1].maskChar != str[(int)strIndex]))) {
                            s += fill.mid(i, n-i+1);
                            i = n + 1; // update i to find + 1
                        }
                    } else {
                        // search for valid m_blank if not
                        n = findInMask(i, true, false, str[(int)strIndex]);
                        if (n != -1) {
                            s += fill.mid(i, n-i);
                            switch (m_maskData[n].caseMode) {
                            case MaskInputData::Upper:
                                s += str[(int)strIndex].toUpper();
                                break;
                            case MaskInputData::Lower:
                                s += str[(int)strIndex].toLower();
                                break;
                            default:
                                s += str[(int)strIndex];
                            }
                            i = n + 1; // updates i to find + 1
                        }
                    }
                }
                ++strIndex;
            }
        } else
            break;
    }

    return s;
}



/*!
    \internal

    Returns a "cleared" string with only separators and blank chars.
    Calling this when no inputMask is set is undefined.
*/
QString QQuickTextInputPrivate::clearString(uint pos, uint len) const
{
    if (pos >= (uint)m_maxLength)
        return QString();

    QString s;
    int end = qMin((uint)m_maxLength, pos + len);
    for (int i = pos; i < end; ++i)
        if (m_maskData[i].separator)
            s += m_maskData[i].maskChar;
        else
            s += m_blank;

    return s;
}

/*!
    \internal

    Strips blank parts of the input in a QQuickTextInputPrivate when an inputMask is set,
    separators are still included. Typically "127.0__.0__.1__" becomes "127.0.0.1".
*/
QString QQuickTextInputPrivate::stripString(const QString &str) const
{
    if (!m_maskData)
        return str;

    QString s;
    int end = qMin(m_maxLength, (int)str.length());
    for (int i = 0; i < end; ++i) {
        if (m_maskData[i].separator)
            s += m_maskData[i].maskChar;
        else if (str[i] != m_blank)
            s += str[i];
    }

    return s;
}

/*!
    \internal
    searches forward/backward in m_maskData for either a separator or a m_blank
*/
int QQuickTextInputPrivate::findInMask(int pos, bool forward, bool findSeparator, QChar searchChar) const
{
    if (pos >= m_maxLength || pos < 0)
        return -1;

    int end = forward ? m_maxLength : -1;
    int step = forward ? 1 : -1;
    int i = pos;

    while (i != end) {
        if (findSeparator) {
            if (m_maskData[i].separator && m_maskData[i].maskChar == searchChar)
                return i;
        } else {
            if (!m_maskData[i].separator) {
                if (searchChar.isNull())
                    return i;
                else if (isValidInput(searchChar, m_maskData[i].maskChar))
                    return i;
            }
        }
        i += step;
    }
    return -1;
}

void QQuickTextInputPrivate::internalUndo(int until)
{
    if (!isUndoAvailable())
        return;
    cancelPasswordEchoTimer();
    internalDeselect();
    while (m_undoState && m_undoState > until) {
        Command& cmd = m_history[--m_undoState];
        switch (cmd.type) {
        case Insert:
            m_text.remove(cmd.pos, 1);
            m_cursor = cmd.pos;
            break;
        case SetSelection:
            m_selstart = cmd.selStart;
            m_selend = cmd.selEnd;
            m_cursor = cmd.pos;
            break;
        case Remove:
        case RemoveSelection:
            m_text.insert(cmd.pos, cmd.uc);
            m_cursor = cmd.pos + 1;
            break;
        case Delete:
        case DeleteSelection:
            m_text.insert(cmd.pos, cmd.uc);
            m_cursor = cmd.pos;
            break;
        case Separator:
            continue;
        }
        if (until < 0 && m_undoState) {
            Command& next = m_history[m_undoState-1];
            if (next.type != cmd.type
                    && next.type < RemoveSelection
                    && (cmd.type < RemoveSelection || next.type == Separator)) {
                break;
            }
        }
    }
    separate();
    m_textDirty = true;
}

void QQuickTextInputPrivate::internalRedo()
{
    if (!isRedoAvailable())
        return;
    internalDeselect();
    while (m_undoState < (int)m_history.size()) {
        Command& cmd = m_history[m_undoState++];
        switch (cmd.type) {
        case Insert:
            m_text.insert(cmd.pos, cmd.uc);
            m_cursor = cmd.pos + 1;
            break;
        case SetSelection:
            m_selstart = cmd.selStart;
            m_selend = cmd.selEnd;
            m_cursor = cmd.pos;
            break;
        case Remove:
        case Delete:
        case RemoveSelection:
        case DeleteSelection:
            m_text.remove(cmd.pos, 1);
            m_selstart = cmd.selStart;
            m_selend = cmd.selEnd;
            m_cursor = cmd.pos;
            break;
        case Separator:
            m_selstart = cmd.selStart;
            m_selend = cmd.selEnd;
            m_cursor = cmd.pos;
            break;
        }
        if (m_undoState < (int)m_history.size()) {
            Command& next = m_history[m_undoState];
            if (next.type != cmd.type
                    && cmd.type < RemoveSelection
                    && next.type != Separator
                    && (next.type < RemoveSelection || cmd.type == Separator)) {
                break;
            }
        }
    }
    m_textDirty = true;
}

void QQuickTextInputPrivate::emitUndoRedoChanged()
{
    Q_Q(QQuickTextInput);
    const bool previousUndo = canUndo;
    const bool previousRedo = canRedo;

    canUndo = isUndoAvailable();
    canRedo = isRedoAvailable();

    if (previousUndo != canUndo)
        emit q->canUndoChanged();
    if (previousRedo != canRedo)
        emit q->canRedoChanged();
}

/*!
    \internal

    If the current cursor position differs from the last emitted cursor
    position, emits cursorPositionChanged().
*/
bool QQuickTextInputPrivate::emitCursorPositionChanged()
{
    Q_Q(QQuickTextInput);
    if (m_cursor != m_lastCursorPos) {
        m_lastCursorPos = m_cursor;

        q->updateCursorRectangle();
        emit q->cursorPositionChanged();

        if (!hasSelectedText()) {
            if (lastSelectionStart != m_cursor) {
                lastSelectionStart = m_cursor;
                emit q->selectionStartChanged();
            }
            if (lastSelectionEnd != m_cursor) {
                lastSelectionEnd = m_cursor;
                emit q->selectionEndChanged();
            }
        }

#ifndef QT_NO_ACCESSIBILITY
        if (QAccessible::isActive()) {
            if (QObject *acc = QQuickAccessibleAttached::findAccessible(q, QAccessible::EditableText)) {
                QAccessibleTextCursorEvent ev(acc, m_cursor);
                QAccessible::updateAccessibility(&ev);
            }
        }
#endif

        return true;
    }
    return false;
}


void QQuickTextInputPrivate::setCursorBlinkPeriod(int msec)
{
    Q_Q(QQuickTextInput);
    if (msec == m_blinkPeriod)
        return;
    if (m_blinkTimer) {
        q->killTimer(m_blinkTimer);
    }
    if (msec) {
        m_blinkTimer = q->startTimer(msec / 2);
        m_blinkStatus = 1;
    } else {
        m_blinkTimer = 0;
        if (m_blinkStatus == 1) {
            updateType = UpdatePaintNode;
            q->update();
        }
    }
    m_blinkPeriod = msec;
}

void QQuickTextInput::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickTextInput);
    if (event->timerId() == d->m_blinkTimer) {
        d->m_blinkStatus = !d->m_blinkStatus;
        d->updateType = QQuickTextInputPrivate::UpdatePaintNode;
        update();
    } else if (event->timerId() == d->m_passwordEchoTimer.timerId()) {
        d->m_passwordEchoTimer.stop();
        d->updateDisplayText();
        updateCursorRectangle();
    }
}

void QQuickTextInputPrivate::processKeyEvent(QKeyEvent* event)
{
    Q_Q(QQuickTextInput);

    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (q->hasAcceptableInput() || fixup()) {
            emit q->accepted();
            emit q->editingFinished();
        }
        event->ignore();
        return;
    }

    if (m_echoMode == QQuickTextInput::PasswordEchoOnEdit
        && !m_passwordEchoEditing
        && !m_readOnly
        && !event->text().isEmpty()
        && !(event->modifiers() & Qt::ControlModifier)) {
        // Clear the edit and reset to normal echo mode while editing; the
        // echo mode switches back when the edit loses focus
        // ### resets current content.  dubious code; you can
        // navigate with keys up, down, back, and select(?), but if you press
        // "left" or "right" it clears?
        updatePasswordEchoEditing(true);
        clear();
    }

    bool unknown = false;
    bool visual = cursorMoveStyle() == Qt::VisualMoveStyle;

    if (false) {
    }
#ifndef QT_NO_SHORTCUT
    else if (event == QKeySequence::Undo) {
        q->undo();
    }
    else if (event == QKeySequence::Redo) {
        q->redo();
    }
    else if (event == QKeySequence::SelectAll) {
        selectAll();
    }
#ifndef QT_NO_CLIPBOARD
    else if (event == QKeySequence::Copy) {
        copy();
    }
    else if (event == QKeySequence::Paste) {
        if (!m_readOnly) {
            QClipboard::Mode mode = QClipboard::Clipboard;
            paste(mode);
        }
    }
    else if (event == QKeySequence::Cut) {
        if (!m_readOnly) {
            copy();
            del();
        }
    }
    else if (event == QKeySequence::DeleteEndOfLine) {
        if (!m_readOnly)
            deleteEndOfLine();
    }
#endif //QT_NO_CLIPBOARD
    else if (event == QKeySequence::MoveToStartOfLine || event == QKeySequence::MoveToStartOfBlock) {
        home(0);
    }
    else if (event == QKeySequence::MoveToEndOfLine || event == QKeySequence::MoveToEndOfBlock) {
        end(0);
    }
    else if (event == QKeySequence::SelectStartOfLine || event == QKeySequence::SelectStartOfBlock) {
        home(1);
    }
    else if (event == QKeySequence::SelectEndOfLine || event == QKeySequence::SelectEndOfBlock) {
        end(1);
    }
    else if (event == QKeySequence::MoveToNextChar) {
        if (hasSelectedText()) {
            moveCursor(selectionEnd(), false);
        } else {
            cursorForward(0, visual ? 1 : (layoutDirection() == Qt::LeftToRight ? 1 : -1));
        }
    }
    else if (event == QKeySequence::SelectNextChar) {
        cursorForward(1, visual ? 1 : (layoutDirection() == Qt::LeftToRight ? 1 : -1));
    }
    else if (event == QKeySequence::MoveToPreviousChar) {
        if (hasSelectedText()) {
            moveCursor(selectionStart(), false);
        } else {
            cursorForward(0, visual ? -1 : (layoutDirection() == Qt::LeftToRight ? -1 : 1));
        }
    }
    else if (event == QKeySequence::SelectPreviousChar) {
        cursorForward(1, visual ? -1 : (layoutDirection() == Qt::LeftToRight ? -1 : 1));
    }
    else if (event == QKeySequence::MoveToNextWord) {
        if (m_echoMode == QQuickTextInput::Normal)
            layoutDirection() == Qt::LeftToRight ? cursorWordForward(0) : cursorWordBackward(0);
        else
            layoutDirection() == Qt::LeftToRight ? end(0) : home(0);
    }
    else if (event == QKeySequence::MoveToPreviousWord) {
        if (m_echoMode == QQuickTextInput::Normal)
            layoutDirection() == Qt::LeftToRight ? cursorWordBackward(0) : cursorWordForward(0);
        else if (!m_readOnly) {
            layoutDirection() == Qt::LeftToRight ? home(0) : end(0);
        }
    }
    else if (event == QKeySequence::SelectNextWord) {
        if (m_echoMode == QQuickTextInput::Normal)
            layoutDirection() == Qt::LeftToRight ? cursorWordForward(1) : cursorWordBackward(1);
        else
            layoutDirection() == Qt::LeftToRight ? end(1) : home(1);
    }
    else if (event == QKeySequence::SelectPreviousWord) {
        if (m_echoMode == QQuickTextInput::Normal)
            layoutDirection() == Qt::LeftToRight ? cursorWordBackward(1) : cursorWordForward(1);
        else
            layoutDirection() == Qt::LeftToRight ? home(1) : end(1);
    }
    else if (event == QKeySequence::Delete) {
        if (!m_readOnly)
            del();
    }
    else if (event == QKeySequence::DeleteEndOfWord) {
        if (!m_readOnly)
            deleteEndOfWord();
    }
    else if (event == QKeySequence::DeleteStartOfWord) {
        if (!m_readOnly)
            deleteStartOfWord();
    }
#endif // QT_NO_SHORTCUT
    else {
        bool handled = false;
        if (event->modifiers() & Qt::ControlModifier) {
            switch (event->key()) {
            case Qt::Key_Backspace:
                if (!m_readOnly)
                    deleteStartOfWord();
                break;
            default:
                if (!handled)
                    unknown = true;
            }
        } else { // ### check for *no* modifier
            switch (event->key()) {
            case Qt::Key_Backspace:
                if (!m_readOnly) {
                    backspace();
                }
                break;
            default:
                if (!handled)
                    unknown = true;
            }
        }
    }

    if (event->key() == Qt::Key_Direction_L || event->key() == Qt::Key_Direction_R) {
        setLayoutDirection((event->key() == Qt::Key_Direction_L) ? Qt::LeftToRight : Qt::RightToLeft);
        unknown = false;
    }

    if (unknown && !m_readOnly) {
        QString t = event->text();
        if (!t.isEmpty() && t.at(0).isPrint()) {
            insert(t);
            event->accept();
            return;
        }
    }

    if (unknown)
        event->ignore();
    else
        event->accept();
}

/*!
    \internal

    Deletes the portion of the word before the current cursor position.
*/

void QQuickTextInputPrivate::deleteStartOfWord()
{
    int priorState = m_undoState;
    Command cmd(SetSelection, m_cursor, 0, m_selstart, m_selend);
    separate();
    cursorWordBackward(true);
    addCommand(cmd);
    removeSelectedText();
    finishChange(priorState);
}

/*!
    \internal

    Deletes the portion of the word after the current cursor position.
*/

void QQuickTextInputPrivate::deleteEndOfWord()
{
    int priorState = m_undoState;
    Command cmd(SetSelection, m_cursor, 0, m_selstart, m_selend);
    separate();
    cursorWordForward(true);
    // moveCursor (sometimes) calls separate() so we need to add the command after that so the
    // cursor position and selection are restored in the same undo operation as the remove.
    addCommand(cmd);
    removeSelectedText();
    finishChange(priorState);
}

/*!
    \internal

    Deletes all text from the cursor position to the end of the line.
*/

void QQuickTextInputPrivate::deleteEndOfLine()
{
    int priorState = m_undoState;
    Command cmd(SetSelection, m_cursor, 0, m_selstart, m_selend);
    separate();
    setSelection(m_cursor, end());
    addCommand(cmd);
    removeSelectedText();
    finishChange(priorState);
}

QT_END_NAMESPACE

