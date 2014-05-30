/****************************************************************************
**
** Copyright (C) 2013 Laszlo Papp <lpapp@kde.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#ifndef QTUDEV_P_H
#define QTUDEV_P_H

#ifdef LINK_LIBUDEV
extern "C"
{
#include <libudev.h>
}
#else
#include <QtCore/qlibrary.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>

#define GENERATE_SYMBOL_VARIABLE(returnType, symbolName, ...) \
    typedef returnType (*fp_##symbolName)(__VA_ARGS__); \
    fp_##symbolName symbolName;

#define RESOLVE_SYMBOL(symbolName) \
    symbolName = (fp_##symbolName)resolveSymbol(udevLibrary, #symbolName); \
    if (!symbolName) \
        return false;

struct udev;

#define udev_list_entry_foreach(list_entry, first_entry) \
        for (list_entry = first_entry; \
             list_entry != NULL; \
             list_entry = udev_list_entry_get_next(list_entry))

struct udev_device;
struct udev_enumerate;
struct udev_list_entry;

GENERATE_SYMBOL_VARIABLE(struct ::udev *, udev_new);
GENERATE_SYMBOL_VARIABLE(struct ::udev_enumerate *, udev_enumerate_new, struct ::udev *)
GENERATE_SYMBOL_VARIABLE(int, udev_enumerate_add_match_subsystem, struct udev_enumerate *, const char *)
GENERATE_SYMBOL_VARIABLE(int, udev_enumerate_scan_devices, struct udev_enumerate *)
GENERATE_SYMBOL_VARIABLE(struct udev_list_entry *, udev_enumerate_get_list_entry, struct udev_enumerate *)
GENERATE_SYMBOL_VARIABLE(struct udev_list_entry *, udev_list_entry_get_next, struct udev_list_entry *)
GENERATE_SYMBOL_VARIABLE(struct udev_device *, udev_device_new_from_syspath, struct udev *udev, const char *syspath)
GENERATE_SYMBOL_VARIABLE(const char *, udev_list_entry_get_name, struct udev_list_entry *)
GENERATE_SYMBOL_VARIABLE(const char *, udev_device_get_devnode, struct udev_device *)
GENERATE_SYMBOL_VARIABLE(const char *, udev_device_get_sysname, struct udev_device *)
GENERATE_SYMBOL_VARIABLE(struct udev_device *, udev_device_get_parent, struct udev_device *)
GENERATE_SYMBOL_VARIABLE(const char *, udev_device_get_subsystem, struct udev_device *)
GENERATE_SYMBOL_VARIABLE(const char *, udev_device_get_property_value, struct udev_device *, const char *)
GENERATE_SYMBOL_VARIABLE(void, udev_device_unref, struct udev_device *)
GENERATE_SYMBOL_VARIABLE(void, udev_enumerate_unref, struct udev_enumerate *)
GENERATE_SYMBOL_VARIABLE(void, udev_unref, struct udev *)

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
inline QFunctionPointer resolveSymbol(QLibrary *udevLibrary, const char *symbolName)
{
    QFunctionPointer symbolFunctionPointer = udevLibrary->resolve(symbolName);
#else
inline void *resolveSymbol(QLibrary *udevLibrary, const char *symbolName)
{
    void *symbolFunctionPointer = udevLibrary->resolve(symbolName);
#endif
    if (!symbolFunctionPointer)
        qWarning("Failed to resolve the udev symbol: %s", symbolName);

    return symbolFunctionPointer;
}

inline bool resolveSymbols(QLibrary *udevLibrary)
{
    if (!udevLibrary->isLoaded()) {
        udevLibrary->setFileNameAndVersion(QStringLiteral("udev"), 1);
        if (!udevLibrary->load()) {
            udevLibrary->setFileNameAndVersion(QStringLiteral("udev"), 0);
            if (!udevLibrary->load()) {
                qWarning("Failed to load the library: %s, supported version(s): %i and %i", qPrintable(udevLibrary->fileName()), 1, 0);
                return false;
            }
        }
    }

    RESOLVE_SYMBOL(udev_new)
    RESOLVE_SYMBOL(udev_enumerate_new)
    RESOLVE_SYMBOL(udev_enumerate_add_match_subsystem)
    RESOLVE_SYMBOL(udev_enumerate_scan_devices)
    RESOLVE_SYMBOL(udev_enumerate_get_list_entry)
    RESOLVE_SYMBOL(udev_list_entry_get_next)
    RESOLVE_SYMBOL(udev_device_new_from_syspath)
    RESOLVE_SYMBOL(udev_list_entry_get_name)
    RESOLVE_SYMBOL(udev_device_get_devnode)
    RESOLVE_SYMBOL(udev_device_get_sysname)
    RESOLVE_SYMBOL(udev_device_get_parent)
    RESOLVE_SYMBOL(udev_device_get_subsystem)
    RESOLVE_SYMBOL(udev_device_get_property_value)
    RESOLVE_SYMBOL(udev_device_unref)
    RESOLVE_SYMBOL(udev_enumerate_unref)
    RESOLVE_SYMBOL(udev_unref)

    return true;
}

#endif

#endif
