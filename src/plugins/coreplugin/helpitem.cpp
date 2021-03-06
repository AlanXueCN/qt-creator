/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "helpitem.h"
#include "helpmanager.h"

#include <utils/htmldocextractor.h>

using namespace Core;

HelpItem::HelpItem() = default;

HelpItem::HelpItem(const char *helpId)
    : m_helpId(QString::fromUtf8(helpId))
{}

HelpItem::HelpItem(const QString &helpId)
    : m_helpId(helpId)
{}

HelpItem::HelpItem(const QUrl &url)
    : m_helpUrl(url)
{}

HelpItem::HelpItem(const QUrl &url, const QString &docMark, HelpItem::Category category)
    : m_helpUrl(url)
    , m_docMark(docMark)
    , m_category(category)
{}

HelpItem::HelpItem(const QUrl &url,
                   const QString &docMark,
                   HelpItem::Category category,
                   const QMap<QString, QUrl> &helpLinks)
    : m_helpUrl(url)
    , m_docMark(docMark)
    , m_category(category)
    , m_helpLinks(helpLinks)
{}

HelpItem::HelpItem(const QString &helpId, const QString &docMark, Category category) :
    m_helpId(helpId), m_docMark(docMark), m_category(category)
{}

HelpItem::HelpItem(const QString &helpId, const QString &docMark, Category category,
                   const QMap<QString, QUrl> &helpLinks) :
    m_helpId(helpId), m_docMark(docMark), m_category(category), m_helpLinks(helpLinks)
{}

void HelpItem::setHelpUrl(const QUrl &url)
{
    m_helpUrl = url;
}

const QUrl &HelpItem::helpUrl() const
{
    return m_helpUrl;
}

void HelpItem::setHelpId(const QString &id)
{ m_helpId = id; }

const QString &HelpItem::helpId() const
{ return m_helpId; }

void HelpItem::setDocMark(const QString &mark)
{ m_docMark = mark; }

const QString &HelpItem::docMark() const
{ return m_docMark; }

void HelpItem::setCategory(Category cat)
{ m_category = cat; }

HelpItem::Category HelpItem::category() const
{ return m_category; }

bool HelpItem::isValid() const
{
    if (m_helpUrl.isEmpty() && m_helpId.isEmpty())
        return false;
    return !links().isEmpty();
}

QString HelpItem::extractContent(bool extended) const
{
    Utils::HtmlDocExtractor htmlExtractor;
    if (extended)
        htmlExtractor.setMode(Utils::HtmlDocExtractor::Extended);
    else
        htmlExtractor.setMode(Utils::HtmlDocExtractor::FirstParagraph);

    QString contents;
    for (const QUrl &url : links()) {
        const QString html = QString::fromUtf8(Core::HelpManager::fileData(url));
        switch (m_category) {
        case Brief:
            contents = htmlExtractor.getClassOrNamespaceBrief(html, m_docMark);
            break;
        case ClassOrNamespace:
            contents = htmlExtractor.getClassOrNamespaceDescription(html, m_docMark);
            break;
        case Function:
            contents = htmlExtractor.getFunctionDescription(html, m_docMark);
            break;
        case Enum:
            contents = htmlExtractor.getEnumDescription(html, m_docMark);
            break;
        case Typedef:
            contents = htmlExtractor.getTypedefDescription(html, m_docMark);
            break;
        case Macro:
            contents = htmlExtractor.getMacroDescription(html, m_docMark);
            break;
        case QmlComponent:
            contents = htmlExtractor.getQmlComponentDescription(html, m_docMark);
            break;
        case QmlProperty:
            contents = htmlExtractor.getQmlPropertyDescription(html, m_docMark);
            break;
        case QMakeVariableOfFunction:
            contents = htmlExtractor.getQMakeVariableOrFunctionDescription(html, m_docMark);
            break;

        default:
            break;
        }

        if (!contents.isEmpty())
            break;
    }
    return contents;
}

const QMap<QString, QUrl> &HelpItem::links() const
{
    if (!m_helpLinks) {
        if (!m_helpUrl.isEmpty())
            m_helpLinks.emplace(QMap<QString, QUrl>({{m_helpUrl.toString(), m_helpUrl}}));
        else
            m_helpLinks = Core::HelpManager::linksForIdentifier(m_helpId);
    }
    return *m_helpLinks;
}
