#pragma once
#include <QWizard>
#include <QMap>
#include <QVariant>
#include <QProcess>
#include "installworker.h"
#include <functional>

enum PageId {
    PAGE_WELCOME = 0,
    PAGE_UPDATE,
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
    PAGE_CACHYOS,
    PAGE_SCX,
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

    // Launch the privileged helper via pkexec and return the socket path.
    // Returns an empty string on failure. Should be called once, just before
    // the install worker is started. The helper process is owned by m_helperProcess
    // and will be waited on / killed during wizard destruction.
    QString launchHelper();

    // Send a single command to the helper and stream output via outputLine callback.
    // Launches the helper first if not already running.
    // Returns the exit code, or -1 on failure.
    // Must be called from the main thread — uses a local event loop internally.
    int runHelperCommand(const QStringList &command,
                         std::function<void(const QString &)> outputLine);

    // Returns estimated disk space needed in MB based on current selections.
    int estimateDiskMB() const;

    // Returns available disk space on / in MB (-1 if unknown).
    static int availableDiskMB();

private:
    void detectSystem();

    QString                 m_targetUser;
    QString                 m_fedoraVersion;
    QMap<QString,QVariant>  m_opts;

    // Helper process — owned by the wizard, lives for the install session.
    // Null until launchHelper() is called.
    QProcess               *m_helperProcess = nullptr;
};
