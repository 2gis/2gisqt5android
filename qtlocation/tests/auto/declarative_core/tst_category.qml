/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import "utils.js" as Utils

TestCase {
    id: testCase

    name: "Category"

    Category { id: emptyCategory }

    function test_empty() {
        compare(emptyCategory.categoryId, "");
        compare(emptyCategory.name, "");
        compare(emptyCategory.visibility, Category.UnspecifiedVisibility);
        compare(emptyCategory.status, Category.Ready);
        compare(emptyCategory.plugin, null);
        verify(emptyCategory.icon);
    }

    Category {
        id: qmlCategory

        plugin: testPlugin

        categoryId: "test-category-id"
        name: "Test Category"
        visibility: Category.DeviceVisibility

        icon: Icon {
            Component.onCompleted:  {
                parameters.singleUrl = "http://example.com/icons/test-category.png"
            }
        }
    }

    function test_qmlConstructedCategory() {
        compare(qmlCategory.categoryId, "test-category-id");
        compare(qmlCategory.name, "Test Category");
        compare(qmlCategory.visibility, Category.DeviceVisibility);
        compare(qmlCategory.status, Category.Ready);
        compare(qmlCategory.plugin, testPlugin);
        verify(qmlCategory.icon);
        compare(qmlCategory.icon.url(), "http://example.com/icons/test-category.png");
        compare(qmlCategory.icon.parameters.singleUrl, "http://example.com/icons/test-category.png");
        compare(qmlCategory.icon.plugin, qmlCategory.plugin);
    }

    Category {
        id: testCategory
    }

    Plugin {
        id: testPlugin
        name: "qmlgeo.test.plugin"
        allowExperimental: true
    }

    Plugin {
        id: invalidPlugin
    }

    Icon {
        id: testIcon
    }

    Category {
        id: saveCategory

        name: "Test Category"
        visibility: Place.DeviceVisibility
    }

    VisualDataModel {
        id: categoryModel

        model: CategoryModel {
            plugin: testPlugin
        }
        delegate: Item { }
    }

    function test_setAndGet_data() {
        return [
            { tag: "name", property: "name", signal: "nameChanged", value: "Test Category", reset: "" },
            { tag: "categoryId", property: "categoryId", signal: "categoryIdChanged", value: "test-category-id-1", reset: "" },
            { tag: "visibility", property: "visibility", signal: "visibilityChanged", value: Place.PublicVisibility, reset: Place.UnspecifiedVisibility },
            { tag: "plugin", property: "plugin", signal: "pluginChanged", value: testPlugin },
            { tag: "icon", property: "icon", signal: "iconChanged", value: testIcon }
        ];
    }

    function test_setAndGet(data) {
        Utils.testObjectProperties(testCase, testCategory, data);
    }

    function test_save() {
        categoryModel.model.update();
        tryCompare(categoryModel.model, "status", CategoryModel.Ready);
        compare(categoryModel.count, 0);

        saveCategory.plugin = testPlugin;
        saveCategory.categoryId = "invalid-category-id";

        saveCategory.save();

        compare(saveCategory.status, Category.Saving);
        verify(saveCategory.errorString().length === 0);

        tryCompare(saveCategory, "status", Category.Error);
        verify(saveCategory.errorString().length > 0);

        // try again without an invalid categoryId
        saveCategory.categoryId = "";
        saveCategory.save();

        compare(saveCategory.status, Category.Saving);

        tryCompare(saveCategory, "status", Category.Ready);
        verify(saveCategory.errorString().length === 0);

        verify(saveCategory.categoryId !== "");


        // Verify that the category was added to the model
        categoryModel.model.update();
        compare(categoryModel.model.status, CategoryModel.Loading);

        tryCompare(categoryModel.model, "status", CategoryModel.Ready);

        compare(categoryModel.count, 1);
        var modelCategory = categoryModel.model.data(categoryModel.modelIndex(0),
                                                     CategoryModel.CategoryRole);
        compare(modelCategory.categoryId, saveCategory.categoryId);
        compare(modelCategory.name, saveCategory.name);


        // Remove a category
        saveCategory.remove();

        compare(saveCategory.status, Category.Removing);

        tryCompare(saveCategory, "status", Category.Ready);
        verify(saveCategory.errorString().length === 0);


        // Verify that the category was removed from the model
        categoryModel.model.update();
        compare(categoryModel.model.status, CategoryModel.Loading);

        tryCompare(categoryModel.model, "status", CategoryModel.Ready);

        compare(categoryModel.count, 0);


        // Try again, this time fail because category does not exist
        saveCategory.remove();

        compare(saveCategory.status, Category.Removing);

        tryCompare(saveCategory, "status", Category.Error);

        verify(saveCategory.errorString().length > 0);
    }

    function test_saveWithoutPlugin() {
        saveCategory.plugin = null;
        saveCategory.categoryId = "";

        saveCategory.save();

        tryCompare(saveCategory, "status", Category.Error);

        verify(saveCategory.errorString().length > 0);
        compare(saveCategory.categoryId, "");

        saveCategory.plugin = invalidPlugin;

        saveCategory.save();

        compare(saveCategory.status, Category.Error);

        verify(saveCategory.errorString().length > 0);
        compare(saveCategory.categoryId, "");
    }

    function test_removeWithoutPlugin() {
        saveCategory.plugin = null;
        saveCategory.categoryId = "test-category-id";

        saveCategory.remove();

        compare(saveCategory.status, Category.Error);

        verify(saveCategory.errorString().length > 0);
        compare(saveCategory.categoryId, "test-category-id");

        saveCategory.plugin = invalidPlugin;

        saveCategory.remove();

        compare(saveCategory.status, Category.Error);

        verify(saveCategory.errorString().length > 0);
        compare(saveCategory.categoryId, "test-category-id");
    }
}
