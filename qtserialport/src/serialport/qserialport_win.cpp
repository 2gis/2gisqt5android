/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Copyright (C) 2012 Andre Hartmann <aha_1980@gmx.de>
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

#include "qserialport_win_p.h"

#include <QtCore/qcoreevent.h>

#ifndef Q_OS_WINCE
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qvector.h>
#endif

#include <QtCore/qwineventnotifier.h>
#include <algorithm>

#ifndef CTL_CODE
#  define CTL_CODE(DeviceType, Function, Method, Access) ( \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
    )
#endif

#ifndef FILE_DEVICE_SERIAL_PORT
#  define FILE_DEVICE_SERIAL_PORT  27
#endif

#ifndef METHOD_BUFFERED
#  define METHOD_BUFFERED  0
#endif

#ifndef FILE_ANY_ACCESS
#  define FILE_ANY_ACCESS  0x00000000
#endif

#ifndef IOCTL_SERIAL_GET_DTRRTS
#  define IOCTL_SERIAL_GET_DTRRTS \
    CTL_CODE(FILE_DEVICE_SERIAL_PORT, 30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef SERIAL_DTR_STATE
#  define SERIAL_DTR_STATE  0x00000001
#endif

#ifndef SERIAL_RTS_STATE
#  define SERIAL_RTS_STATE  0x00000002
#endif

QT_BEGIN_NAMESPACE

#ifndef Q_OS_WINCE

static void initializeOverlappedStructure(OVERLAPPED &overlapped)
{
    overlapped.Internal = 0;
    overlapped.InternalHigh = 0;
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
}

QSerialPortPrivate::QSerialPortPrivate(QSerialPort *q)
    : QSerialPortPrivateData(q)
    , handle(INVALID_HANDLE_VALUE)
    , parityErrorOccurred(false)
    , readChunkBuffer(ReadChunkSize, 0)
    , readyReadEmitted(0)
    , writeStarted(false)
    , communicationNotifier(new QWinEventNotifier(q))
    , readCompletionNotifier(new QWinEventNotifier(q))
    , writeCompletionNotifier(new QWinEventNotifier(q))
    , originalEventMask(0)
    , triggeredEventMask(0)
{
    ::ZeroMemory(&communicationOverlapped, sizeof(communicationOverlapped));
    communicationOverlapped.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!communicationOverlapped.hEvent)
        q->setError(decodeSystemError());
    else {
        communicationNotifier->setHandle(communicationOverlapped.hEvent);
        q->connect(communicationNotifier, SIGNAL(activated(HANDLE)), q, SLOT(_q_completeAsyncCommunication()));
    }

    ::ZeroMemory(&readCompletionOverlapped, sizeof(readCompletionOverlapped));
    readCompletionOverlapped.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!readCompletionOverlapped.hEvent)
        q->setError(decodeSystemError());
    else {
        readCompletionNotifier->setHandle(readCompletionOverlapped.hEvent);
        q->connect(readCompletionNotifier, SIGNAL(activated(HANDLE)), q, SLOT(_q_completeAsyncRead()));
    }

    ::ZeroMemory(&writeCompletionOverlapped, sizeof(writeCompletionOverlapped));
    writeCompletionOverlapped.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!writeCompletionOverlapped.hEvent)
        q->setError(decodeSystemError());
    else {
        writeCompletionNotifier->setHandle(writeCompletionOverlapped.hEvent);
        q->connect(writeCompletionNotifier, SIGNAL(activated(HANDLE)), q, SLOT(_q_completeAsyncWrite()));
    }
}

bool QSerialPortPrivate::open(QIODevice::OpenMode mode)
{
    Q_Q(QSerialPort);

    DWORD desiredAccess = 0;
    originalEventMask = EV_ERR;

    if (mode & QIODevice::ReadOnly) {
        desiredAccess |= GENERIC_READ;
        originalEventMask |= EV_RXCHAR;
    }
    if (mode & QIODevice::WriteOnly)
        desiredAccess |= GENERIC_WRITE;

    handle = ::CreateFile(reinterpret_cast<const wchar_t*>(systemLocation.utf16()),
                              desiredAccess, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        q->setError(decodeSystemError());
        return false;
    }

    ::ZeroMemory(&restoredDcb, sizeof(restoredDcb));
    restoredDcb.DCBlength = sizeof(restoredDcb);

    if (!::GetCommState(handle, &restoredDcb)) {
        q->setError(decodeSystemError());
        return false;
    }

    currentDcb = restoredDcb;
    currentDcb.fBinary = TRUE;
    currentDcb.fInX = FALSE;
    currentDcb.fOutX = FALSE;
    currentDcb.fAbortOnError = FALSE;
    currentDcb.fNull = FALSE;
    currentDcb.fErrorChar = FALSE;

    if (currentDcb.fDtrControl ==  DTR_CONTROL_HANDSHAKE)
        currentDcb.fDtrControl = DTR_CONTROL_DISABLE;

    if (!updateDcb())
        return false;

    if (!::GetCommTimeouts(handle, &restoredCommTimeouts)) {
        q->setError(decodeSystemError());
        return false;
    }

    ::ZeroMemory(&currentCommTimeouts, sizeof(currentCommTimeouts));
    currentCommTimeouts.ReadIntervalTimeout = MAXDWORD;

    if (!updateCommTimeouts())
        return false;

    if (mode & QIODevice::ReadOnly)
        readCompletionNotifier->setEnabled(true);

    if (mode & QIODevice::WriteOnly)
        writeCompletionNotifier->setEnabled(true);

    if (!::SetCommMask(handle, originalEventMask)) {
        q->setError(decodeSystemError());
        return false;
    }

    if (!startAsyncCommunication())
        return false;

    communicationNotifier->setEnabled(true);

    return true;
}

void QSerialPortPrivate::close()
{
    Q_Q(QSerialPort);

    if (!::CancelIo(handle))
        q->setError(decodeSystemError());

    readCompletionNotifier->setEnabled(false);
    writeCompletionNotifier->setEnabled(false);
    communicationNotifier->setEnabled(false);

    readBuffer.clear();

    writeStarted = false;
    writeBuffer.clear();

    readyReadEmitted = false;
    parityErrorOccurred = false;

    if (settingsRestoredOnClose) {
        if (!::SetCommState(handle, &restoredDcb))
            q->setError(decodeSystemError());
        else if (!::SetCommTimeouts(handle, &restoredCommTimeouts))
            q->setError(decodeSystemError());
    }

    if (!::CloseHandle(handle))
        q->setError(decodeSystemError());

    handle = INVALID_HANDLE_VALUE;
}

#endif // #ifndef Q_OS_WINCE

QSerialPort::PinoutSignals QSerialPortPrivate::pinoutSignals()
{
    Q_Q(QSerialPort);

    DWORD modemStat = 0;

    if (!::GetCommModemStatus(handle, &modemStat)) {
        q->setError(decodeSystemError());
        return QSerialPort::NoSignal;
    }

    QSerialPort::PinoutSignals ret = QSerialPort::NoSignal;

    if (modemStat & MS_CTS_ON)
        ret |= QSerialPort::ClearToSendSignal;
    if (modemStat & MS_DSR_ON)
        ret |= QSerialPort::DataSetReadySignal;
    if (modemStat & MS_RING_ON)
        ret |= QSerialPort::RingIndicatorSignal;
    if (modemStat & MS_RLSD_ON)
        ret |= QSerialPort::DataCarrierDetectSignal;

    DWORD bytesReturned = 0;
    if (!::DeviceIoControl(handle, IOCTL_SERIAL_GET_DTRRTS, NULL, 0,
                          &modemStat, sizeof(modemStat),
                          &bytesReturned, NULL)) {
        q->setError(decodeSystemError());
        return ret;
    }

    if (modemStat & SERIAL_DTR_STATE)
        ret |= QSerialPort::DataTerminalReadySignal;
    if (modemStat & SERIAL_RTS_STATE)
        ret |= QSerialPort::RequestToSendSignal;

    return ret;
}

bool QSerialPortPrivate::setDataTerminalReady(bool set)
{
    Q_Q(QSerialPort);

    if (!::EscapeCommFunction(handle, set ? SETDTR : CLRDTR)) {
        q->setError(decodeSystemError());
        return false;
    }

    currentDcb.fDtrControl = set ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    return true;
}

bool QSerialPortPrivate::setRequestToSend(bool set)
{
    Q_Q(QSerialPort);

    if (!::EscapeCommFunction(handle, set ? SETRTS : CLRRTS)) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

#ifndef Q_OS_WINCE

bool QSerialPortPrivate::flush()
{
    Q_Q(QSerialPort);

    bool returnValue = true;

    if (!startAsyncWrite())
        returnValue = false;

    if (!::FlushFileBuffers(handle)) {
        q->setError(decodeSystemError());
        returnValue = false;
    }

    return returnValue;

}

bool QSerialPortPrivate::clear(QSerialPort::Directions directions)
{
    Q_Q(QSerialPort);

    DWORD flags = 0;
    if (directions & QSerialPort::Input)
        flags |= PURGE_RXABORT | PURGE_RXCLEAR;
    if (directions & QSerialPort::Output) {
        flags |= PURGE_TXABORT | PURGE_TXCLEAR;
        writeStarted = false;
    }
    if (!::PurgeComm(handle, flags)) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

#endif

bool QSerialPortPrivate::sendBreak(int duration)
{
    if (!setBreakEnabled(true))
        return false;

    ::Sleep(duration);

    if (!setBreakEnabled(false))
        return false;

    return true;
}

bool QSerialPortPrivate::setBreakEnabled(bool set)
{
    Q_Q(QSerialPort);

    if (set ? !::SetCommBreak(handle) : !::ClearCommBreak(handle)) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

#ifndef Q_OS_WINCE

void QSerialPortPrivate::startWriting()
{
    if (!writeStarted)
        startAsyncWrite();
}

bool QSerialPortPrivate::waitForReadyRead(int msecs)
{
    Q_Q(QSerialPort);

    QElapsedTimer stopWatch;
    stopWatch.start();

    const qint64 initialReadBufferSize = readBuffer.size();
    qint64 currentReadBufferSize = initialReadBufferSize;

    do {
        bool timedOut = false;
        HANDLE triggeredEvent = 0;

        if (!waitAnyEvent(timeoutValue(msecs, stopWatch.elapsed()), &timedOut, &triggeredEvent) || !triggeredEvent) {
            // This is occur timeout or another error
            if (!timedOut)
                q->setError(decodeSystemError());
            return false;
        }

        if (triggeredEvent == communicationOverlapped.hEvent) {
            _q_completeAsyncCommunication();
        } else if (triggeredEvent == readCompletionOverlapped.hEvent) {
            _q_completeAsyncRead();
            if (qint64(readBuffer.size()) != currentReadBufferSize)
                currentReadBufferSize = readBuffer.size();
            else if (initialReadBufferSize != currentReadBufferSize)
                return true;
        } else if (triggeredEvent == writeCompletionOverlapped.hEvent) {
            _q_completeAsyncWrite();
        } else {
            return false;
        }

    } while (msecs == -1 || timeoutValue(msecs, stopWatch.elapsed()) > 0);

    return false;
}

bool QSerialPortPrivate::waitForBytesWritten(int msecs)
{
    Q_Q(QSerialPort);

    if (writeBuffer.isEmpty())
        return false;

    QElapsedTimer stopWatch;
    stopWatch.start();

    forever {
        bool timedOut = false;
        HANDLE triggeredEvent = 0;

        if (!waitAnyEvent(timeoutValue(msecs, stopWatch.elapsed()), &timedOut, &triggeredEvent) || !triggeredEvent) {
            if (!timedOut)
                q->setError(decodeSystemError());
            return false;
        }

        if (triggeredEvent == communicationOverlapped.hEvent) {
             _q_completeAsyncRead();
        } else if (triggeredEvent == readCompletionOverlapped.hEvent) {
            _q_completeAsyncRead();
        } else if (triggeredEvent == writeCompletionOverlapped.hEvent) {
            _q_completeAsyncWrite();
            return error == QSerialPort::NoError;
        } else {
            return false;
        }

    }

    return false;
}

#endif // #ifndef Q_OS_WINCE

bool QSerialPortPrivate::setBaudRate()
{
    return setBaudRate(inputBaudRate, QSerialPort::AllDirections);
}

bool QSerialPortPrivate::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_Q(QSerialPort);

    if (directions != QSerialPort::AllDirections) {
        q->setError(QSerialPort::UnsupportedOperationError);
        return false;
    }
    currentDcb.BaudRate = baudRate;
    return updateDcb();
}

bool QSerialPortPrivate::setDataBits(QSerialPort::DataBits dataBits)
{
    currentDcb.ByteSize = dataBits;
    return updateDcb();
}

bool QSerialPortPrivate::setParity(QSerialPort::Parity parity)
{
    currentDcb.fParity = TRUE;
    switch (parity) {
    case QSerialPort::NoParity:
        currentDcb.Parity = NOPARITY;
        currentDcb.fParity = FALSE;
        break;
    case QSerialPort::OddParity:
        currentDcb.Parity = ODDPARITY;
        break;
    case QSerialPort::EvenParity:
        currentDcb.Parity = EVENPARITY;
        break;
    case QSerialPort::MarkParity:
        currentDcb.Parity = MARKPARITY;
        break;
    case QSerialPort::SpaceParity:
        currentDcb.Parity = SPACEPARITY;
        break;
    default:
        currentDcb.Parity = NOPARITY;
        currentDcb.fParity = FALSE;
        break;
    }
    return updateDcb();
}

bool QSerialPortPrivate::setStopBits(QSerialPort::StopBits stopBits)
{
    switch (stopBits) {
    case QSerialPort::OneStop:
        currentDcb.StopBits = ONESTOPBIT;
        break;
    case QSerialPort::OneAndHalfStop:
        currentDcb.StopBits = ONE5STOPBITS;
        break;
    case QSerialPort::TwoStop:
        currentDcb.StopBits = TWOSTOPBITS;
        break;
    default:
        currentDcb.StopBits = ONESTOPBIT;
        break;
    }
    return updateDcb();
}

bool QSerialPortPrivate::setFlowControl(QSerialPort::FlowControl flowControl)
{
    currentDcb.fInX = FALSE;
    currentDcb.fOutX = FALSE;
    currentDcb.fOutxCtsFlow = FALSE;
    currentDcb.fRtsControl = RTS_CONTROL_DISABLE;
    switch (flowControl) {
    case QSerialPort::NoFlowControl:
        break;
    case QSerialPort::SoftwareControl:
        currentDcb.fInX = TRUE;
        currentDcb.fOutX = TRUE;
        break;
    case QSerialPort::HardwareControl:
        currentDcb.fOutxCtsFlow = TRUE;
        currentDcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
        break;
    default:
        break;
    }
    return updateDcb();
}

bool QSerialPortPrivate::setDataErrorPolicy(QSerialPort::DataErrorPolicy policy)
{
    policy = policy;
    return true;
}

#ifndef Q_OS_WINCE

void QSerialPortPrivate::_q_completeAsyncCommunication()
{
    Q_Q(QSerialPort);

    DWORD numberOfBytesTransferred = 0;

    if (!::GetOverlappedResult(handle, &communicationOverlapped, &numberOfBytesTransferred, FALSE))
        q->setError(decodeSystemError());

    bool error = false;

    // Check for unexpected event. This event triggered when pulled previously
    // opened device from the system, when opened as for not to read and not to
    // write options and so forth.
    if (triggeredEventMask == 0)
        error = true;

    // Workaround for standard CDC ACM serial ports, for which triggered an
    // unexpected event EV_TXEMPTY at data transmission.
    if ((originalEventMask & triggeredEventMask) == 0) {
        if ((triggeredEventMask & EV_TXEMPTY) == 0)
            error = true;
    }

    // Start processing a caught error.
    if (error || (EV_ERR & triggeredEventMask))
        processIoErrors(error);

    if (!error)
        startAsyncRead();
}

void QSerialPortPrivate::_q_completeAsyncRead()
{
    Q_Q(QSerialPort);

    DWORD numberOfBytesTransferred = 0;
    if (!::GetOverlappedResult(handle, &readCompletionOverlapped, &numberOfBytesTransferred, FALSE))
        q->setError(decodeSystemError());

    if (numberOfBytesTransferred > 0) {

        readBuffer.append(readChunkBuffer.left(numberOfBytesTransferred));

        if (!emulateErrorPolicy())
            emitReadyRead();
    }

    // start async read for possible remainder into driver queue
    if ((numberOfBytesTransferred > 0) && (policy == QSerialPort::IgnorePolicy))
        startAsyncRead();
    else // driver queue is emplty, so startup wait comm event
        startAsyncCommunication();
}

void QSerialPortPrivate::_q_completeAsyncWrite()
{
    Q_Q(QSerialPort);

    DWORD numberOfBytesTransferred = 0;
    if (!::GetOverlappedResult(handle, &writeCompletionOverlapped, &numberOfBytesTransferred, FALSE)) {
        numberOfBytesTransferred = 0;
        q->setError(decodeSystemError());
    }

    if (numberOfBytesTransferred > 0) {
        writeBuffer.free(numberOfBytesTransferred);
        emit q->bytesWritten(numberOfBytesTransferred);
    }

    writeStarted = false;
    startAsyncWrite();
}

bool QSerialPortPrivate::startAsyncCommunication()
{
    Q_Q(QSerialPort);

    initializeOverlappedStructure(communicationOverlapped);
    if (!::WaitCommEvent(handle, &triggeredEventMask, &communicationOverlapped)) {
        const QSerialPort::SerialPortError error = decodeSystemError();
        if (error != QSerialPort::NoError) {
            q->setError(decodeSystemError());
            return false;
        }
    }
    return true;
}

bool QSerialPortPrivate::startAsyncRead()
{
    Q_Q(QSerialPort);

    DWORD bytesToRead = policy == QSerialPort::IgnorePolicy ? ReadChunkSize : 1;

    if (readBufferMaxSize && bytesToRead > (readBufferMaxSize - readBuffer.size())) {
        bytesToRead = readBufferMaxSize - readBuffer.size();
        if (bytesToRead == 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
            return false;
        }
    }

    initializeOverlappedStructure(readCompletionOverlapped);
    if (::ReadFile(handle, readChunkBuffer.data(), bytesToRead, NULL, &readCompletionOverlapped))
        return true;

    QSerialPort::SerialPortError error = decodeSystemError();
    if (error != QSerialPort::NoError) {
        if (error != QSerialPort::ResourceError)
            error = QSerialPort::ReadError;
        q->setError(error);

        return false;
    }

    return true;
}

bool QSerialPortPrivate::startAsyncWrite()
{
    Q_Q(QSerialPort);

    if (writeBuffer.isEmpty() || writeStarted)
        return true;

    initializeOverlappedStructure(writeCompletionOverlapped);
    if (!::WriteFile(handle, writeBuffer.readPointer(),
                     writeBuffer.nextDataBlockSize(),
                     NULL, &writeCompletionOverlapped)) {

        QSerialPort::SerialPortError error = decodeSystemError();
        if (error != QSerialPort::NoError) {
            if (error != QSerialPort::ResourceError)
                error = QSerialPort::WriteError;
            q->setError(error);
            return false;
        }
    }

    writeStarted = true;
    return true;
}

bool QSerialPortPrivate::emulateErrorPolicy()
{
    if (!parityErrorOccurred)
        return false;

    parityErrorOccurred = false;

    switch (policy) {
    case QSerialPort::SkipPolicy:
        readBuffer.getChar();
        break;
    case QSerialPort::PassZeroPolicy:
        readBuffer.getChar();
        readBuffer.putChar('\0');
        emitReadyRead();
        break;
    case QSerialPort::IgnorePolicy:
        return false;
    case QSerialPort::StopReceivingPolicy:
        emitReadyRead();
        break;
    default:
        return false;
    }

    return true;
}

void QSerialPortPrivate::emitReadyRead()
{
    Q_Q(QSerialPort);

    readyReadEmitted = true;
    emit q->readyRead();
}

#endif // #ifndef Q_OS_WINCE

void QSerialPortPrivate::processIoErrors(bool error)
{
    Q_Q(QSerialPort);

    if (error) {
        q->setError(QSerialPort::ResourceError);
        return;
    }

    DWORD errors = 0;
    if (!::ClearCommError(handle, &errors, NULL)) {
        q->setError(decodeSystemError());
        return;
    }

    if (errors & CE_FRAME) {
        q->setError(QSerialPort::FramingError);
    } else if (errors & CE_RXPARITY) {
        q->setError(QSerialPort::ParityError);
        parityErrorOccurred = true;
    } else if (errors & CE_BREAK) {
        q->setError(QSerialPort::BreakConditionError);
    } else {
        q->setError(QSerialPort::UnknownError);
    }
}

#ifndef Q_OS_WINCE

bool QSerialPortPrivate::updateDcb()
{
    Q_Q(QSerialPort);

    if (!::SetCommState(handle, &currentDcb)) {
        q->setError(decodeSystemError());
        return false;
    }
    return true;
}

bool QSerialPortPrivate::updateCommTimeouts()
{
    Q_Q(QSerialPort);

    if (!::SetCommTimeouts(handle, &currentCommTimeouts)) {
        q->setError(decodeSystemError());
        return false;
    }
    return true;
}

#endif // #ifndef Q_OS_WINCE

QSerialPort::SerialPortError QSerialPortPrivate::decodeSystemError() const
{
    QSerialPort::SerialPortError error;
    switch (::GetLastError()) {
    case ERROR_IO_PENDING:
        error = QSerialPort::NoError;
        break;
    case ERROR_MORE_DATA:
        error = QSerialPort::NoError;
        break;
    case ERROR_FILE_NOT_FOUND:
        error = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_INVALID_NAME:
        error = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_ACCESS_DENIED:
        error = QSerialPort::PermissionError;
        break;
    case ERROR_INVALID_HANDLE:
        error = QSerialPort::ResourceError;
        break;
    case ERROR_INVALID_PARAMETER:
        error = QSerialPort::UnsupportedOperationError;
        break;
    case ERROR_BAD_COMMAND:
        error = QSerialPort::ResourceError;
        break;
    case ERROR_DEVICE_REMOVED:
        error = QSerialPort::ResourceError;
        break;
    default:
        error = QSerialPort::UnknownError;
        break;
    }
    return error;
}

#ifndef Q_OS_WINCE

bool QSerialPortPrivate::waitAnyEvent(int msecs, bool *timedOut, HANDLE *triggeredEvent)
{
    Q_Q(QSerialPort);

    Q_ASSERT(timedOut);

    QVector<HANDLE> handles = QVector<HANDLE>()
            << communicationOverlapped.hEvent
            << readCompletionOverlapped.hEvent
            << writeCompletionOverlapped.hEvent;

    DWORD waitResult = ::WaitForMultipleObjects(handles.count(),
                                                handles.constData(),
                                                FALSE, // wait any event
                                                msecs == -1 ? INFINITE : msecs);
    if (waitResult == WAIT_TIMEOUT) {
        *timedOut = true;
        q->setError(QSerialPort::TimeoutError);
        return false;
    }

    if (waitResult >= DWORD(WAIT_OBJECT_0 + handles.count()))
        return false;

    *triggeredEvent = handles.at(waitResult - WAIT_OBJECT_0);
    return true;
}

static const QString defaultPathPrefix = QStringLiteral("\\\\.\\");

QString QSerialPortPrivate::portNameToSystemLocation(const QString &port)
{
    QString ret = port;
    if (!ret.contains(defaultPathPrefix))
        ret.prepend(defaultPathPrefix);
    return ret;
}

QString QSerialPortPrivate::portNameFromSystemLocation(const QString &location)
{
    QString ret = location;
    if (ret.contains(defaultPathPrefix))
        ret.remove(defaultPathPrefix);
    return ret;
}

#endif // #ifndef Q_OS_WINCE

// This table contains standard values of baud rates that
// are defined in MSDN and/or in Win SDK file winbase.h

static const QList<qint32> standardBaudRatePairList()
{

    static const QList<qint32> standardBaudRatesTable = QList<qint32>()

        #ifdef CBR_110
            << CBR_110
        #endif

        #ifdef CBR_300
            << CBR_300
        #endif

        #ifdef CBR_600
            << CBR_600
        #endif

        #ifdef CBR_1200
            << CBR_1200
        #endif

        #ifdef CBR_2400
            << CBR_2400
        #endif

        #ifdef CBR_4800
            << CBR_4800
        #endif

        #ifdef CBR_9600
            << CBR_9600
        #endif

        #ifdef CBR_14400
            << CBR_14400
        #endif

        #ifdef CBR_19200
            << CBR_19200
        #endif

        #ifdef CBR_38400
            << CBR_38400
        #endif

        #ifdef CBR_56000
            << CBR_56000
        #endif

        #ifdef CBR_57600
            << CBR_57600
        #endif

        #ifdef CBR_115200
            << CBR_115200
        #endif

        #ifdef CBR_128000
            << CBR_128000
        #endif

        #ifdef CBR_256000
            << CBR_256000
        #endif
    ;

    return standardBaudRatesTable;
};

qint32 QSerialPortPrivate::baudRateFromSetting(qint32 setting)
{
    const QList<qint32> baudRatePairs = standardBaudRatePairList();
    const QList<qint32>::const_iterator baudRatePairListConstIterator
            = std::find(baudRatePairs.constBegin(), baudRatePairs.constEnd(), setting);

    return (baudRatePairListConstIterator != baudRatePairs.constEnd()) ? *baudRatePairListConstIterator : 0;
}

qint32 QSerialPortPrivate::settingFromBaudRate(qint32 baudRate)
{
    const QList<qint32> baudRatePairList = standardBaudRatePairList();
    const QList<qint32>::const_iterator baudRatePairListConstIterator
            = std::find(baudRatePairList.constBegin(), baudRatePairList.constEnd(), baudRate);

    return (baudRatePairListConstIterator != baudRatePairList.constEnd()) ? *baudRatePairListConstIterator : 0;
}

QList<qint32> QSerialPortPrivate::standardBaudRates()
{
    return standardBaudRatePairList();
}

QSerialPort::Handle QSerialPort::handle() const
{
    Q_D(const QSerialPort);
    return d->handle;
}

QT_END_NAMESPACE
