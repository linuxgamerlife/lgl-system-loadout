#pragma once
#include <QWizard>
#include <QMap>
#include <QVariant>
#include "installworker.h"

enum PageId {
    PAGE_WELCOME = 0,
    PAGE_REPOS,
    PAGE_SYSTEMTOOLS,
    PAGE_PYTHON,
    PAGE_MULTIMEDIA,
    PAGE_CONTENT,
    PAGE_GPU,
    PAGE_GAMING,
    PAGE_VIRT,
    PAGE_BROWSERS,
    PAGE_COMMS,
    PAGE_THEMING,
    PAGE_CACHYOS,
    PAGE_REVIEW,
    PAGE_INSTALL,
    PAGE_DONE
};

class MainWizard : public QWizard {
    Q_OBJECT
public:
    explicit MainWizard(QWidget *parent = nullptr);

    void     setOpt(const QString &key, const QVariant &val);
    QVariant getOpt(const QString &key, const QVariant &def = {}) const;

    QString targetUser()    const { return m_targetUser; }
    QString fedoraVersion() const { return m_fedoraVersion; }

    QList<InstallStep> buildSteps() const;

    // Returns estimated disk space needed in MB based on current selections
    int estimateDiskMB() const;

    // Returns available disk space on / in MB (-1 if unknown)
    static int availableDiskMB();

private:
    void detectSystem();
    QString                 m_targetUser;
    QString                 m_fedoraVersion;
    QMap<QString,QVariant>  m_opts;
};
