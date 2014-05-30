/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of Qt Designer.  This header
// file may change from version to version without notice, or even be removed.
//
// We mean it.
//

#ifndef FORMWINDOWBASE_H
#define FORMWINDOWBASE_H

#include "shared_global_p.h"

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtCore/QVariantMap>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE

class QDesignerDnDItemInterface;
class QMenu;
class QtResourceSet;
class QDesignerPropertySheet;

namespace qdesigner_internal {

class QEditorFormBuilder;
class DeviceProfile;
class Grid;

class DesignerPixmapCache;
class DesignerIconCache;
class FormWindowBasePrivate;

class QDESIGNER_SHARED_EXPORT FormWindowBase: public QDesignerFormWindowInterface
{
    Q_OBJECT
public:
    enum HighlightMode  { Restore, Highlight };

    explicit FormWindowBase(QDesignerFormEditorInterface *core, QWidget *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~FormWindowBase();

    QVariantMap formData();
    void setFormData(const QVariantMap &vm);

    virtual QStringList checkContents() const;

    // Deprecated
    virtual QPoint grid() const;

    // Deprecated
    virtual void setGrid(const QPoint &grid);

    virtual bool hasFeature(Feature f) const;
    virtual Feature features() const;
    virtual void setFeatures(Feature f);

    const Grid &designerGrid() const;
    void setDesignerGrid(const  Grid& grid);

    bool hasFormGrid() const;
    void setHasFormGrid(bool b);

    bool gridVisible() const;

    virtual ResourceFileSaveMode resourceFileSaveMode() const;
    virtual void setResourceFileSaveMode(ResourceFileSaveMode behaviour);

    static const Grid &defaultDesignerGrid();
    static void setDefaultDesignerGrid(const Grid& grid);

    // Overwrite to initialize and return a full popup menu for a managed widget
    virtual QMenu *initializePopupMenu(QWidget *managedWidget);
    // Helper to create a basic popup menu from task menu extensions (internal/public)
    static QMenu *createExtensionTaskMenu(QDesignerFormWindowInterface *fw, QObject *o, bool trailingSeparator = true);

    virtual bool dropWidgets(const QList<QDesignerDnDItemInterface*> &item_list, QWidget *target,
                             const QPoint &global_mouse_pos) = 0;

    // Helper to find the widget at the mouse position with some flags.
    enum WidgetUnderMouseMode { FindSingleSelectionDropTarget, FindMultiSelectionDropTarget };
    QWidget *widgetUnderMouse(const QPoint &formPos, WidgetUnderMouseMode m);

    virtual QWidget *widgetAt(const QPoint &pos) = 0;
    virtual QWidget *findContainer(QWidget *w, bool excludeLayout) const = 0;

    void deleteWidgetList(const QWidgetList &widget_list);

    virtual void highlightWidget(QWidget *w, const QPoint &pos, HighlightMode mode = Highlight) = 0;

    enum PasteMode { PasteAll, PasteActionsOnly };
#ifndef QT_NO_CLIPBOARD
    virtual void paste(PasteMode pasteMode) = 0;
#endif

    // Factory method to create a form builder
    virtual QEditorFormBuilder *createFormBuilder() = 0;

    virtual bool blockSelectionChanged(bool blocked) = 0;
    virtual void emitSelectionChanged() = 0;

    DesignerPixmapCache *pixmapCache() const;
    DesignerIconCache *iconCache() const;
    virtual QtResourceSet *resourceSet() const;
    virtual void setResourceSet(QtResourceSet *resourceSet);
    void addReloadableProperty(QDesignerPropertySheet *sheet, int index);
    void removeReloadableProperty(QDesignerPropertySheet *sheet, int index);
    void addReloadablePropertySheet(QDesignerPropertySheet *sheet, QObject *object);
    void removeReloadablePropertySheet(QDesignerPropertySheet *sheet);
    void reloadProperties();

    void emitWidgetRemoved(QWidget *w);
    void emitObjectRemoved(QObject *o);

    DeviceProfile deviceProfile() const;
    QString styleName() const;
    QString deviceProfileName() const;

    enum LineTerminatorMode {
        LFLineTerminator,
        CRLFLineTerminator,
        NativeLineTerminator =
#if defined (Q_OS_WIN)
            CRLFLineTerminator
#else
            LFLineTerminator
#endif
    };

    void setLineTerminatorMode(LineTerminatorMode mode);
    LineTerminatorMode lineTerminatorMode() const;

public slots:
    void resourceSetActivated(QtResourceSet *resourceSet, bool resourceSetChanged);

private slots:
    void triggerDefaultAction(QWidget *w);

private:
    void syncGridFeature();

    FormWindowBasePrivate *m_d;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // FORMWINDOWBASE_H
