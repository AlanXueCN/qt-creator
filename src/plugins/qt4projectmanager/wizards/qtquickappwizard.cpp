/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "mobileappwizardpages.h"

#include "qtquickapp.h"
#include "qtquickappwizard.h"
#include "qtquickappwizardpages.h"
#include "targetsetuppage.h"
#include "qt4projectmanagerconstants.h"

#include <qtsupport/qtsupportconstants.h>

#include <QtCore/QCoreApplication>
#include <QtGui/QIcon>

namespace Qt4ProjectManager {
namespace Internal {

class QtQuickAppWizardDialog : public AbstractMobileAppWizardDialog
{
    Q_OBJECT

public:
    explicit QtQuickAppWizardDialog(QWidget *parent = 0);

protected:
    bool validateCurrentPage();

private:
    class QtQuickAppWizardSourcesPage *m_qmlSourcesPage;
    class QtQuickComponentSetOptionsPage *m_componentOptionsPage;
    int m_componentOptionsPageId;

    Utils::WizardProgressItem *m_componentItem;

    friend class QtQuickAppWizard;
};

QtQuickAppWizardDialog::QtQuickAppWizardDialog(QWidget *parent)
    : AbstractMobileAppWizardDialog(parent, QtSupport::QtVersionNumber(4, 7, 1))
    , m_qmlSourcesPage(0)
{
    setWindowTitle(tr("New Qt Quick Application"));
    setIntroDescription(tr("This wizard generates a Qt Quick application project."));

    m_componentOptionsPage = new Internal::QtQuickComponentSetOptionsPage;
    m_componentOptionsPageId = addPageWithTitle(m_componentOptionsPage, tr("Component Set"));
    m_componentItem = wizardProgress()->item(m_componentOptionsPageId);

    AbstractMobileAppWizardDialog::addMobilePages();

    m_componentItem->setNextItems(QList<Utils::WizardProgressItem *>()
                                  << targetsPageItem());

    m_qmlSourcesPage = new QtQuickAppWizardSourcesPage;
    addPageWithTitle(m_qmlSourcesPage, tr("QML Sources"));
}

bool QtQuickAppWizardDialog::validateCurrentPage()
{
    if (currentPage() == m_componentOptionsPage) {
        if (m_componentOptionsPage->componentSet() == QtQuickApp::Symbian10Components) {
            setIgnoreGenericOptionsPage(true);
            targetsPage()->setMinimumQtVersion(QtSupport::QtVersionNumber(4, 7, 3));
        }
    }
    return AbstractMobileAppWizardDialog::validateCurrentPage();
}

class QtQuickAppWizardPrivate
{
    class QtQuickApp *app;
    class QtQuickAppWizardDialog *wizardDialog;
    friend class QtQuickAppWizard;
};

QtQuickAppWizard::QtQuickAppWizard()
    : AbstractMobileAppWizard(parameters())
    , m_d(new QtQuickAppWizardPrivate)
{
    m_d->app = new QtQuickApp;
    m_d->wizardDialog = 0;
}

QtQuickAppWizard::~QtQuickAppWizard()
{
    delete m_d->app;
    delete m_d;
}

Core::BaseFileWizardParameters QtQuickAppWizard::parameters()
{
    Core::BaseFileWizardParameters parameters(ProjectWizard);
    parameters.setIcon(QIcon(QLatin1String(Qt4ProjectManager::Constants::ICON_QTQUICK_APP)));
    parameters.setDisplayName(tr("Qt Quick Application"));
    parameters.setId(QLatin1String("QA.QMLA Application"));
    parameters.setDescription(tr("Creates a Qt Quick application project that can contain "
                                 "both QML and C++ code and includes a QDeclarativeView.\n\n"
                                 "You can build the application and deploy it on desktop and "
                                 "mobile target platforms. For example, you can create signed "
                                 "Symbian Installation System (SIS) packages for this type of "
                                 "projects. Moreover, you can select to use a set of premade "
                                 "UI components in your Qt Quick application. "
                                 "To utilize the components, Qt 4.7.3 or newer is required."));
    parameters.setCategory(QLatin1String(QtSupport::Constants::QML_WIZARD_CATEGORY));
    parameters.setDisplayCategory(QCoreApplication::translate(QtSupport::Constants::QML_WIZARD_TR_SCOPE,
                                                              QtSupport::Constants::QML_WIZARD_TR_CATEGORY));
    return parameters;
}

AbstractMobileAppWizardDialog *QtQuickAppWizard::createWizardDialogInternal(QWidget *parent) const
{
    m_d->wizardDialog = new QtQuickAppWizardDialog(parent);
    m_d->wizardDialog->m_componentOptionsPage->setComponentSet(m_d->app->componentSet());
    return m_d->wizardDialog;
}

void QtQuickAppWizard::projectPathChanged(const QString &path) const
{
    m_d->wizardDialog->targetsPage()->setProFilePath(path);
}

void QtQuickAppWizard::prepareGenerateFiles(const QWizard *w,
    QString *errorMessage) const
{
    Q_UNUSED(errorMessage)
    const QtQuickAppWizardDialog *wizard = qobject_cast<const QtQuickAppWizardDialog*>(w);
    if (wizard->m_qmlSourcesPage->mainQmlMode() == QtQuickApp::ModeGenerate) {
        m_d->app->setMainQml(QtQuickApp::ModeGenerate);
    } else {
        const QString mainQmlFile = wizard->m_qmlSourcesPage->mainQmlFile();
        m_d->app->setMainQml(QtQuickApp::ModeImport, mainQmlFile);
    }
    m_d->app->setComponentSet(wizard->m_componentOptionsPage->componentSet());
    if (m_d->app->componentSet() == QtQuickApp::Symbian10Components)
        m_d->app->setOrientation(AbstractMobileApp::ScreenOrientationImplicit);
}

QString QtQuickAppWizard::fileToOpenPostGeneration() const
{
    return m_d->app->path(QtQuickApp::MainQml);
}

AbstractMobileApp *QtQuickAppWizard::app() const
{
    return m_d->app;
}

AbstractMobileAppWizardDialog *QtQuickAppWizard::wizardDialog() const
{
    return m_d->wizardDialog;
}

} // namespace Internal
} // namespace Qt4ProjectManager

#include "qtquickappwizard.moc"
