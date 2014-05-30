/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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
import Enginio 1.0
import "config.js" as AppConfig


// NOTE this test will only work if the server has the right setup:
// objects.files needs to have fileAttachment set to be a ref to files.


Item {
    id: root

    EnginioClient {
        id: enginio
        backendId: AppConfig.backendData.id
        serviceUrl: AppConfig.backendData.serviceUrl

        onError: {
            console.log("\n\n### ERROR")
            console.log(reply.errorString)
            reply.dumpDebugInfo()
            console.log("\n###\n")
        }
    }

    SignalSpy {
        id: finishedSpy
        target: enginio
        signalName: "finished"
    }

    SignalSpy {
        id: errorSpy
        target: enginio
        signalName: "error"
    }

    TestCase {
        name: "Files"

        function init() {
            finishedSpy.clear()
            errorSpy.clear()
        }

        function test_upload_download() {
            var finished = 0

            //! [upload-create-object]
            var fileObject = {
                "objectType": AppConfig.testObjectType,
                "title": "Example object with file attachment",
            }
            var reply = enginio.create(fileObject);
            //! [upload-create-object]

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)
            verify(reply.data.id.length > 0)

            var fileName = AppConfig.testSourcePath + "/../common/enginio.png";

            //! [upload]
            var objectId = reply.data.id
            var uploadData = {
                "file":{
                    "fileName":"test.png"
                },
                "targetFileProperty": {
                    "objectType": AppConfig.testObjectType,
                    "id": objectId,
                    "propertyName": "fileAttachment"
                },
            }
            var uploadReply = enginio.uploadFile(uploadData, fileName)
            //! [upload]

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)

            //! [download]
            var downloadData = {
                "id": uploadReply.data.id,
            }
            var downloadReply = enginio.downloadUrl(downloadData)
            //! [download]

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)

            verify(downloadReply.data.expiringUrl.length > 0)
        }
    }
}
