/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "breakhandler.h"
#include "breakpointmarker.h"

#include "debuggeractions.h"
#include "debuggercore.h"
#include "debuggerengine.h"
#include "debuggerstringutils.h"

#include <utils/qtcassert.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>


//////////////////////////////////////////////////////////////////
//
// BreakHandler
//
//////////////////////////////////////////////////////////////////

namespace Debugger {
namespace Internal {

BreakHandler::BreakHandler()
  : m_breakpointIcon(_(":/debugger/images/breakpoint_16.png")),
    m_disabledBreakpointIcon(_(":/debugger/images/breakpoint_disabled_16.png")),
    m_pendingBreakPointIcon(_(":/debugger/images/breakpoint_pending_16.png")),
    //m_emptyIcon(_(":/debugger/images/watchpoint.png")),
    m_emptyIcon(_(":/debugger/images/breakpoint_pending_16.png")),
    //m_emptyIcon(_(":/debugger/images/debugger_empty_14.png")),
    m_watchpointIcon(_(":/debugger/images/watchpoint.png")),
    m_syncTimerId(-1)
{}

BreakHandler::~BreakHandler()
{}

int BreakHandler::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 8;
}

int BreakHandler::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_storage.size();
}

// FIXME: Only used by cdb. Move there?
bool BreakHandler::hasPendingBreakpoints() const
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->isPending())
            return true;
    return false;
}

static inline bool fileNameMatch(const QString &f1, const QString &f2)
{
#ifdef Q_OS_WIN
    return f1.compare(f2, Qt::CaseInsensitive) == 0;
#else
    return f1 == f2;
#endif
}

static bool isSimilarTo(const BreakpointData &data, const BreakpointResponse &needle)
{
    // Clear hit.
    // Clear miss.
    if (needle.bpType != UnknownType && data.type() != UnknownType
            && data.type() != needle.bpType)
        return false;

    // Clear hit.
    if (data.address() && data.address() == needle.bpAddress)
        return true;

    // At least at a position we were looking for.
    // FIXME: breaks multiple breakpoints at the same location
    if (!data.fileName().isEmpty()
            && fileNameMatch(data.fileName(), needle.bpFileName)
            && data.lineNumber() == needle.bpLineNumber)
        return true;

    // At least at a position we were looking for.
    // FIXME: breaks multiple breakpoints at the same location
    if (!data.fileName().isEmpty()
            && fileNameMatch(data.fileName(), needle.bpFileName)
            && data.lineNumber() == needle.bpLineNumber)
        return true;

    return false;
}

BreakpointId BreakHandler::findSimilarBreakpoint(const BreakpointResponse &needle) const
{
    // Search a breakpoint we might refer to.
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it) {
        const BreakpointId id = it.key();
        const BreakpointData &data = it->data;
        const BreakpointResponse &response = it->response;
        qDebug() << "COMPARING " << data.toString() << " WITH " << needle.toString();
        if (response.bpNumber && response.bpNumber == needle.bpNumber)
            return id;

        if (isSimilarTo(data, needle))
            return id;
    }
    return BreakpointId(-1);
}

BreakpointId BreakHandler::findBreakpointByNumber(int bpNumber) const
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->response.bpNumber == bpNumber)
            return it.key();
    return BreakpointId(-1);
}

BreakpointId BreakHandler::findBreakpointByFunction(const QString &functionName) const
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->data.functionName() == functionName)
            return it.key();
    return BreakpointId(-1);
}

BreakpointId BreakHandler::findBreakpointByAddress(quint64 address) const
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->data.address() == address)
            return it.key();
    return BreakpointId(-1);
}

BreakpointId BreakHandler::findBreakpointByFileAndLine(const QString &fileName,
    int lineNumber, bool useMarkerPosition)
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->data.isLocatedAt(fileName, lineNumber, useMarkerPosition))
            return it.key();
    return BreakpointId(-1);
}

BreakpointData *BreakHandler::breakpointById(BreakpointId id)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return 0);
    return &it->data;
}

BreakpointId BreakHandler::findWatchpointByAddress(quint64 address) const
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->data.isWatchpoint() && it->data.address() == address)
            return it.key();
    return BreakpointId(-1);
}

void BreakHandler::setWatchpointByAddress(quint64 address)
{
    const int id = findWatchpointByAddress(address);
    if (id == -1) {
        BreakpointData data;
        data.setType(Watchpoint);
        data.setAddress(address);
        appendBreakpoint(data);
        scheduleSynchronization();
    } else {
        qDebug() << "WATCHPOINT EXISTS";
     //   removeBreakpoint(index);
    }
}

bool BreakHandler::hasWatchpointAt(quint64 address) const
{
    return findWatchpointByAddress(address) != BreakpointId(-1);
}

void BreakHandler::saveBreakpoints()
{
    //qDebug() << "SAVING BREAKPOINTS...";
    QTC_ASSERT(debuggerCore(), return);
    QList<QVariant> list;
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it) {
        const BreakpointData &data = it->data;
        QMap<QString, QVariant> map;
        // Do not persist Watchpoints.
        //if (data.isWatchpoint())
        //    continue;
        if (data.type() != BreakpointByFileAndLine)
            map.insert(_("type"), data.type());
        if (!data.fileName().isEmpty())
            map.insert(_("filename"), data.fileName());
        if (data.lineNumber())
            map.insert(_("linenumber"), data.lineNumber());
        if (!data.functionName().isEmpty())
            map.insert(_("funcname"), data.functionName());
        if (data.address())
            map.insert(_("address"), data.address());
        if (!data.condition().isEmpty())
            map.insert(_("condition"), data.condition());
        if (data.ignoreCount())
            map.insert(_("ignorecount"), data.ignoreCount());
        if (!data.threadSpec().isEmpty())
            map.insert(_("threadspec"), data.threadSpec());
        if (!data.isEnabled())
            map.insert(_("disabled"), _("1"));
        if (data.useFullPath())
            map.insert(_("usefullpath"), _("1"));
        list.append(map);
    }
    debuggerCore()->setSessionValue("Breakpoints", list);
    //qDebug() << "SAVED BREAKPOINTS" << this << list.size();
}

void BreakHandler::loadBreakpoints()
{
    QTC_ASSERT(debuggerCore(), return);
    //qDebug() << "LOADING BREAKPOINTS...";
    QVariant value = debuggerCore()->sessionValue("Breakpoints");
    QList<QVariant> list = value.toList();
    //clear();
    foreach (const QVariant &var, list) {
        const QMap<QString, QVariant> map = var.toMap();
        BreakpointData data;
        QVariant v = map.value(_("filename"));
        if (v.isValid())
            data.setFileName(v.toString());
        v = map.value(_("linenumber"));
        if (v.isValid())
            data.setLineNumber(v.toString().toInt());
        v = map.value(_("condition"));
        if (v.isValid())
            data.setCondition(v.toString().toLatin1());
        v = map.value(_("address"));
        if (v.isValid())
            data.setAddress(v.toString().toULongLong());
        v = map.value(_("ignorecount"));
        if (v.isValid())
            data.setIgnoreCount(v.toString().toInt());
        v = map.value(_("threadspec"));
        if (v.isValid())
            data.setThreadSpec(v.toString().toLatin1());
        v = map.value(_("funcname"));
        if (v.isValid())
            data.setFunctionName(v.toString());
        v = map.value(_("disabled"));
        if (v.isValid())
            data.setEnabled(!v.toInt());
        v = map.value(_("usefullpath"));
        if (v.isValid())
            data.setUseFullPath(bool(v.toInt()));
        v = map.value(_("type"));
        if (v.isValid())
            data.setType(BreakpointType(v.toInt()));
        data.setMarkerFileName(data.fileName());
        data.setMarkerLineNumber(data.lineNumber());
        appendBreakpoint(data);
    }
    //qDebug() << "LOADED BREAKPOINTS" << this << list.size();
}

void BreakHandler::updateMarkers()
{
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        updateMarker(it.key());
}

void BreakHandler::updateMarker(BreakpointId id)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    const BreakpointData &data = it->data;
    BreakpointMarker *marker = it->marker;

    if (marker && (data.m_markerFileName != marker->fileName()
                || data.m_markerLineNumber != marker->lineNumber()))
        it->destroyMarker();

    if (!marker && !data.m_markerFileName.isEmpty() && data.m_markerLineNumber > 0) {
        marker = new BreakpointMarker(id, data.m_markerFileName, data.m_markerLineNumber);
        it->marker = marker;
    }
}

QVariant BreakHandler::headerData(int section,
    Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        static QString headers[] = {
            tr("Number"),  tr("Function"), tr("File"), tr("Line"),
            tr("Condition"), tr("Ignore"), tr("Threads"), tr("Address")
        };
        return headers[section];
    }
    return QVariant();
}

BreakpointId BreakHandler::findBreakpointByIndex(const QModelIndex &index) const
{
    int r = index.row();
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for (int i = 0; it != et; ++it, ++i)
        if (i == r)
            return it.key();
    return BreakpointId(-1);
}

QVariant BreakHandler::data(const QModelIndex &mi, int role) const
{
    static const QString empty = QString(QLatin1Char('-'));

    if (!mi.isValid())
        return QVariant();

    BreakpointId id = findBreakpointByIndex(mi);
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return QVariant());
    const BreakpointData &data = it->data;
    const BreakpointResponse &response = it->response;

    switch (mi.column()) {
        case 0:
            if (role == Qt::DisplayRole) {
                return QString("%1 - %2").arg(id).arg(response.bpNumber);
            }
            if (role == Qt::DecorationRole) {
                if (data.isWatchpoint())
                    return m_watchpointIcon;
                if (!data.isEnabled())
                    return m_disabledBreakpointIcon;
                return it->isPending() ? m_pendingBreakPointIcon : m_breakpointIcon;
            }
            break;
        case 1:
            if (role == Qt::DisplayRole) {
                const QString str = it->isPending()
                    ? data.functionName() : response.bpFuncName;
                return str.isEmpty() ? empty : str;
            }
            break;
        case 2:
            if (role == Qt::DisplayRole) {
                QString str = it->isPending()
                    ? data.fileName() : response.bpFileName;
                str = QFileInfo(str).fileName();
                // FIXME: better?
                //if (data.bpMultiple && str.isEmpty() && !data.markerFileName.isEmpty())
                //    str = data.markerFileName;
                str = str.isEmpty() ? empty : str;
                if (data.useFullPath())
                    str = QDir::toNativeSeparators(QLatin1String("/.../") + str);
                return str;
            }
            break;
        case 3:
            if (role == Qt::DisplayRole) {
                // FIXME: better?
                //if (data.bpMultiple && str.isEmpty() && !data.markerFileName.isEmpty())
                //    str = data.markerLineNumber;
                const int nr = it->isPending()
                    ? data.lineNumber() : response.bpLineNumber;
                return nr ? QString::number(nr) : empty;
            }
            if (role == Qt::UserRole + 1)
                return data.lineNumber();
            break;
        case 4:
            if (role == Qt::DisplayRole)
                return it->isPending() ? data.condition() : response.bpCondition;
            if (role == Qt::ToolTipRole)
                return tr("Breakpoint will only be hit if this condition is met.");
            if (role == Qt::UserRole + 1)
                return data.condition();
            break;
        case 5:
            if (role == Qt::DisplayRole) {
                const int ignoreCount =
                    it->isPending() ? data.ignoreCount() : response.bpIgnoreCount;
                return ignoreCount ? QVariant(ignoreCount) : QVariant(QString());
            }
            if (role == Qt::ToolTipRole)
                return tr("Breakpoint will only be hit after being ignored so many times.");
            if (role == Qt::UserRole + 1)
                return data.ignoreCount();
            break;
        case 6:
            if (role == Qt::DisplayRole) {
                if (it->isPending())
                    return !data.threadSpec().isEmpty() ? data.threadSpec() : tr("(all)");
                else
                    return !response.bpThreadSpec.isEmpty() ? response.bpThreadSpec : tr("(all)");
            }
            if (role == Qt::ToolTipRole)
                return tr("Breakpoint will only be hit in the specified thread(s).");
            if (role == Qt::UserRole + 1)
                return data.threadSpec();
            break;
        case 7:
            if (role == Qt::DisplayRole) {
                QString displayValue;
                const quint64 address =
                    data.isWatchpoint() ? data.address() : response.bpAddress;
                if (address)
                    displayValue += QString::fromAscii("0x%1").arg(address, 0, 16);
                if (!response.bpState.isEmpty()) {
                    if (!displayValue.isEmpty())
                        displayValue += QLatin1Char(' ');
                    displayValue += QString::fromAscii(response.bpState);
                }
                return displayValue;
            }
            break;
    }
    if (role == Qt::ToolTipRole)
        return debuggerCore()->boolSetting(UseToolTipsInBreakpointsView)
                ? QVariant(it->toToolTip()) : QVariant();

    return QVariant();
}

#define GETTER(type, getter) \
type BreakHandler::getter(BreakpointId id) const \
{ \
    ConstIterator it = m_storage.find(id); \
    QTC_ASSERT(it != m_storage.end(), \
        qDebug() << "ID" << id << "NOT KNOWN"; \
        return type()); \
    return it->data.getter(); \
}

#define SETTER(type, setter) \
void BreakHandler::setter(BreakpointId id, const type &value) \
{ \
    Iterator it = m_storage.find(id); \
    QTC_ASSERT(it != m_storage.end(), \
        qDebug() << "ID" << id << "NOT KNOWN"; return); \
    if (it->data.setter(value)) \
        scheduleSynchronization(); \
}

#define PROPERTY(type, getter, setter) \
    GETTER(type, getter) \
    SETTER(type, setter)


PROPERTY(bool, useFullPath, setUseFullPath)
PROPERTY(QString, markerFileName, setMarkerFileName)
PROPERTY(QString, fileName, setFileName)
PROPERTY(QString, functionName, setFunctionName)
PROPERTY(int, markerLineNumber, setMarkerLineNumber)
PROPERTY(BreakpointType, type, setType)
PROPERTY(QByteArray, threadSpec, setThreadSpec)
PROPERTY(QByteArray, condition, setCondition)
PROPERTY(int, lineNumber, setLineNumber)
PROPERTY(quint64, address, setAddress)
PROPERTY(int, ignoreCount, setIgnoreCount)

bool BreakHandler::isEnabled(BreakpointId id) const
{
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return BreakpointDead);
    return it->data.isEnabled();
}

void BreakHandler::setEnabled(BreakpointId id, bool on)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    //qDebug() << "SET ENABLED: " << id << it->data.isEnabled() << on;
    if (it->data.setEnabled(on)) {
        it->destroyMarker();
        updateMarker(id);
        scheduleSynchronization();
    }
}

BreakpointState BreakHandler::state(BreakpointId id) const
{
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return BreakpointDead);
    return it->state;
}

void BreakHandler::setState(BreakpointId id, BreakpointState state)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    it->state = state;
    updateMarker(id);
}

DebuggerEngine *BreakHandler::engine(BreakpointId id) const
{
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return 0);
    return it->engine;
}

void BreakHandler::setEngine(BreakpointId id, DebuggerEngine *value)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    QTC_ASSERT(it->state == BreakpointNew, /**/);
    QTC_ASSERT(!it->engine, return);
    it->engine = value;
    it->state = BreakpointInsertRequested;
    updateMarker(id);
    scheduleSynchronization();
}

void BreakHandler::ackCondition(BreakpointId id)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    it->response.bpCondition = it->data.condition();
    updateMarker(id);
}

void BreakHandler::ackIgnoreCount(BreakpointId id)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    it->response.bpIgnoreCount = it->data.ignoreCount();
    updateMarker(id);
}

Qt::ItemFlags BreakHandler::flags(const QModelIndex &index) const
{
//    switch (index.column()) {
//        //case 0:
//        //    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
//        default:
            return QAbstractTableModel::flags(index);
//    }
}

void BreakHandler::removeBreakpoint(BreakpointId id)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    if (it->state == BreakpointInserted) {
        qDebug() << "MARK AS CHANGED: " << id;
        it->state = BreakpointRemoveRequested;
        QTC_ASSERT(it->engine, return);
        debuggerCore()->synchronizeBreakpoints();
    } else if (it->state == BreakpointNew) {
        it->state = BreakpointDead;
        cleanupBreakpoint(id);
    } else {
        qDebug() << "CANNOT REMOVE IN STATE " << it->state;
    }
}

void BreakHandler::appendBreakpoint(const BreakpointData &data)
{
    // Ok to be not thread-safe. The order does not matter and only the gui
    // produces authoritative ids.
    static quint64 currentId = 0;

    BreakpointId id(++currentId);
    BreakpointItem item;
    item.data = data;
    m_storage.insert(id, item);
    scheduleSynchronization();
}

void BreakHandler::toggleBreakpoint(const QString &fileName, int lineNumber,
                                    quint64 address /* = 0 */)
{
    BreakpointId id(-1);

    if (address) {
        id = findBreakpointByAddress(address);
    } else {
        id = findBreakpointByFileAndLine(fileName, lineNumber, true);
        if (id == BreakpointId(-1))
            id = findBreakpointByFileAndLine(fileName, lineNumber, false);
    }

    if (id != BreakpointId(-1)) {
        removeBreakpoint(id);
    } else {
        BreakpointData data;
        if (address) {
            data.setAddress(address);
        } else {
            data.setFileName(fileName);
            data.setLineNumber(lineNumber);
        }
        data.setMarkerFileName(fileName);
        data.setMarkerLineNumber(lineNumber);
        appendBreakpoint(data);
    }
    debuggerCore()->synchronizeBreakpoints();
}

void BreakHandler::saveSessionData()
{
    saveBreakpoints();
}

void BreakHandler::loadSessionData()
{
    m_storage.clear();
    loadBreakpoints();
}

void BreakHandler::breakByFunction(const QString &functionName)
{
    // One breakpoint per function is enough for now. This does not handle
    // combinations of multiple conditions and ignore counts, though.
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it) {
        const BreakpointData &data = it->data;
        if (data.functionName() == functionName
                && data.condition().isEmpty()
                && data.ignoreCount() == 0)
            return;
    }
    BreakpointData data;
    data.setType(BreakpointByFunction);
    data.setFunctionName(functionName);
    appendBreakpoint(data);
}

QIcon BreakHandler::icon(BreakpointId id) const
{
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return pendingBreakPointIcon());
    if (!it->data.isEnabled())
        return m_disabledBreakpointIcon;
    if (it->state == BreakpointInserted)
        return breakpointIcon();
    return pendingBreakPointIcon();
}

void BreakHandler::scheduleSynchronization()
{
    if (m_syncTimerId == -1)
        m_syncTimerId = startTimer(10);
}

void BreakHandler::timerEvent(QTimerEvent *event)
{
    QTC_ASSERT(event->timerId() == m_syncTimerId, return);
    killTimer(m_syncTimerId);
    m_syncTimerId = -1;
    //qDebug() << "BREAKPOINT SYNCRONIZATION STARTED";
    debuggerCore()->synchronizeBreakpoints();
    updateMarkers();
    emit layoutChanged();
    saveBreakpoints();  // FIXME: remove?
}

void BreakHandler::gotoLocation(BreakpointId id) const
{
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    debuggerCore()->gotoLocation(
        it->data.fileName(), it->data.lineNumber(), false);
}

void BreakHandler::updateLineNumberFromMarker(BreakpointId id, int lineNumber)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    //if (data.markerLineNumber == lineNumber)
    //    return;
    if (it->data.markerLineNumber() != lineNumber) {
        it->data.setMarkerLineNumber(lineNumber);
        // FIXME: Should we tell gdb about the change?
        // Ignore it for now, as we would require re-compilation
        // and debugger re-start anyway.
        //if (0 && data.bpLineNumber) {
        //    if (!data.bpNumber.trimmed().isEmpty()) {
        //        data.pending = true;
        //    }
        //}
    }
    // Ignore updates to the "real" line number while the debugger is
    // running, as this can be triggered by moving the breakpoint to
    // the next line that generated code.
    // FIXME: Do we need yet another data member?
    if (it->response.bpNumber == 0) {
        it->data.setLineNumber(lineNumber);
        updateMarker(id);
    }
}

BreakpointIds BreakHandler::allBreakpointIds() const
{
    BreakpointIds ids;
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        ids.append(it.key());
    return ids;
}

BreakpointIds BreakHandler::unclaimedBreakpointIds() const
{
    return engineBreakpointIds(0);
}

BreakpointIds BreakHandler::engineBreakpointIds(DebuggerEngine *engine) const
{
    BreakpointIds ids;
    ConstIterator it = m_storage.constBegin(), et = m_storage.constEnd();
    for ( ; it != et; ++it)
        if (it->engine == engine)
            ids.append(it.key());
    return ids;
}

void BreakHandler::notifyBreakpointInsertOk(BreakpointId id)
{
    QTC_ASSERT(state(id)== BreakpointInsertProceeding, /**/);
    setState(id, BreakpointInserted);
}

void BreakHandler::notifyBreakpointInsertFailed(BreakpointId id)
{
    QTC_ASSERT(state(id)== BreakpointInsertProceeding, /**/);
    setState(id, BreakpointDead);
}

void BreakHandler::notifyBreakpointRemoveOk(BreakpointId id)
{
    QTC_ASSERT(state(id) == BreakpointRemoveProceeding, /**/);
    setState(id, BreakpointDead);
    cleanupBreakpoint(id);
}

void BreakHandler::notifyBreakpointRemoveFailed(BreakpointId id)
{
    QTC_ASSERT(state(id) == BreakpointRemoveProceeding, /**/);
    setState(id, BreakpointDead);
    cleanupBreakpoint(id);
}

void BreakHandler::notifyBreakpointChangeOk(BreakpointId id)
{
    QTC_ASSERT(state(id) == BreakpointChangeProceeding, /**/);
    setState(id, BreakpointInserted);
}

void BreakHandler::notifyBreakpointChangeFailed(BreakpointId id)
{
    QTC_ASSERT(state(id) == BreakpointChangeProceeding, /**/);
    setState(id, BreakpointDead);
}

void BreakHandler::notifyBreakpointReleased(BreakpointId id)
{
    //QTC_ASSERT(state(id) == BreakpointChangeProceeding, /**/);
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    it->state = BreakpointNew;
    it->engine = 0;
    it->response = BreakpointResponse();
    delete it->marker;
    it->marker = 0;
    updateMarker(id);
}

void BreakHandler::cleanupBreakpoint(BreakpointId id)
{
    QTC_ASSERT(state(id) == BreakpointDead, /**/);
    BreakpointItem item = m_storage.take(id);
    item.destroyMarker();
}

BreakpointResponse BreakHandler::response(BreakpointId id) const
{
    ConstIterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return BreakpointResponse());
    return it->response;
}

void BreakHandler::setResponse(BreakpointId id, const BreakpointResponse &data)
{
    Iterator it = m_storage.find(id);
    QTC_ASSERT(it != m_storage.end(), return);
    it->response = BreakpointResponse(data);
    updateMarker(id);
}
#if 0
void BreakHandler::notifyBreakpointAdjusted(BreakpointId id)
{
    QTC_ASSERT(state(id)== BreakpointChangeProceeding, /**/);
    bp->bpNumber      = rbp.bpNumber;
    bp->bpCondition   = rbp.bpCondition;
    bp->bpIgnoreCount = rbp.bpIgnoreCount;
    bp->bpFileName    = rbp.bpFileName;
    bp->bpFullName    = rbp.bpFullName;
    bp->bpLineNumber  = rbp.bpLineNumber;
    bp->bpCorrectedLineNumber = rbp.bpCorrectedLineNumber;
    bp->bpThreadSpec  = rbp.bpThreadSpec;
    bp->bpFuncName    = rbp.bpFuncName;
    bp->bpAddress     = rbp.bpAddress;
    bp->bpMultiple    = rbp.bpMultiple;
    bp->bpEnabled     = rbp.bpEnabled;
    setState(id, BreakpointOk);
}
#endif

void BreakHandler::BreakpointItem::destroyMarker()
{
    BreakpointMarker *m = marker;
    marker = 0;
    delete m;
}

static void formatAddress(QTextStream &str, quint64 address)
{
    if (address) {
        str << "0x";
        str.setIntegerBase(16);
        str << address;
        str.setIntegerBase(10);
    }
}

static QString stateToString(BreakpointState state)
{
    switch (state) {
        case BreakpointNew: return "new";
        case BreakpointInsertRequested: return "insertion requested";
        case BreakpointInsertProceeding: return "insertion proceeding";
        case BreakpointChangeRequested: return "change requested";
        case BreakpointChangeProceeding: return "change proceeding";
        case BreakpointPending: return "breakpoint pending";
        case BreakpointInserted: return "breakpoint inserted";
        case BreakpointRemoveRequested: return "removal requested";
        case BreakpointRemoveProceeding: return "removal is proceeding";
        case BreakpointDead: return "dead";
        default: return "<invalid state>";
    }
};

QString BreakHandler::BreakpointItem::toToolTip() const
{
    QString t;

    switch (data.type()) {
        case BreakpointByFileAndLine:
            t = tr("Breakpoint by File and Line");
            break;
        case BreakpointByFunction:
            t = tr("Breakpoint by Function");
            break;
        case BreakpointByAddress:
            t = tr("Breakpoint by Address");
            break;
        case Watchpoint:
            t = tr("Watchpoint");
            break;
        case UnknownType:
            t = tr("Unknown Breakpoint Type");
    }

    QString rc;
    QTextStream str(&rc);
    str << "<html><body><table>"
        //<< "<tr><td>" << tr("Id:") << "</td><td>" << m_id << "</td></tr>"
        << "<tr><td>" << tr("State:")
        << "</td><td>" << state << "   (" << stateToString(state) << ")</td></tr>"
        << "<tr><td>" << tr("Engine:")
        << "</td><td>" << (engine ? engine->objectName() : "0") << "</td></tr>"
        << "<tr><td>" << tr("Marker File:")
        << "</td><td>" << QDir::toNativeSeparators(data.m_markerFileName) << "</td></tr>"
        << "<tr><td>" << tr("Marker Line:")
        << "</td><td>" << data.m_markerLineNumber << "</td></tr>"
        << "<tr><td>" << tr("Breakpoint Number:")
        << "</td><td>" << response.bpNumber << "</td></tr>"
        << "<tr><td>" << tr("Breakpoint Type:")
        << "</td><td>" << t << "</td></tr>"
        << "<tr><td>" << tr("State:")
        << "</td><td>" << response.bpState << "</td></tr>"
        << "</table><br><hr><table>"
        << "<tr><th>" << tr("Property")
        << "</th><th>" << tr("Requested")
        << "</th><th>" << tr("Obtained") << "</th></tr>"
        << "<tr><td>" << tr("Internal Number:")
        << "</td><td>&mdash;</td><td>" << response.bpNumber << "</td></tr>"
        << "<tr><td>" << tr("File Name:")
        << "</td><td>" << QDir::toNativeSeparators(data.m_fileName)
        << "</td><td>" << QDir::toNativeSeparators(response.bpFileName)
        << "</td></tr>"
        << "<tr><td>" << tr("Function Name:")
        << "</td><td>" << data.m_functionName
        << "</td><td>" << response.bpFuncName << "</td></tr>"
        << "<tr><td>" << tr("Line Number:") << "</td><td>";
    if (data.m_lineNumber)
        str << data.m_lineNumber;
    str << "</td><td>";
    if (response.bpLineNumber)
        str << response.bpLineNumber;
    str << "</td></tr>"
        << "<tr><td>" << tr("Breakpoint Address:")
        << "</td><td>";
    formatAddress(str, data.m_address);
    str << "</td><td>";
    formatAddress(str, response.bpAddress);
    //str <<  "</td></tr>"
    //    << "<tr><td>" << tr("Corrected Line Number:")
    //    << "</td><td>-</td><td>";
    //if (response.bpCorrectedLineNumber > 0)
    //    str << response.bpCorrectedLineNumber;
    //else
    //    str << '-';
    str << "</td></tr>"
        << "<tr><td>" << tr("Condition:")
        << "</td><td>" << data.m_condition
        << "</td><td>" << response.bpCondition << "</td></tr>"
        << "<tr><td>" << tr("Ignore Count:") << "</td><td>";
    if (data.m_ignoreCount)
        str << data.m_ignoreCount;
    str << "</td><td>";
    if (response.bpIgnoreCount)
        str << response.bpIgnoreCount;
    str << "</td></tr>"
        << "<tr><td>" << tr("Thread Specification:")
        << "</td><td>" << data.m_threadSpec
        << "</td><td>" << response.bpThreadSpec << "</td></tr>"
        << "</table></body></html>";
    return rc;
}

} // namespace Internal
} // namespace Debugger
