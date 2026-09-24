#ifndef XENIASAVEEXPORTWIZARD_H
#define XENIASAVEEXPORTWIZARD_H

// qt
#include <QWizard>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTreeWidgetItem>
#include <QStatusBar>
#include <QVector>
#include <QHash>
#include <QUuid>
#include "qthelpers.h"
#include "titleidfinder.h"

// xbox360
#include <XboxInternals/Stfs/StfsPackage.h>

namespace Ui
{
class XeniaSaveExportWizard;
}

class XeniaSaveExportWizard : public QWizard
{
    Q_OBJECT

public:
    explicit XeniaSaveExportWizard(QStatusBar *statusBar, QWidget *parent = nullptr);
    ~XeniaSaveExportWizard();

protected:
    // Description: re-checks the current page's completeness and, on the last page,
    // asks before overwriting files that already exist at the destination
    bool validateCurrentPage() override;

private slots:
    void onCurrentIdChanged(int id);

    void onFinished(int status);

    void on_btnBrowseSource_clicked();

    void on_btnBrowseProfile_clicked();

    void on_btnBrowseKv_clicked();

    void on_btnBrowseOutput_clicked();

    void on_txtTargetXuid_textChanged(const QString &text);

    void on_listSaves_itemChanged(QTreeWidgetItem *item, int column);

private:
    struct SaveEntry
    {
        QString titleId;
        QString contentType;
        QString slotName;
        QString headerPath;
        QString bodyPath;
        qint64 size;
    };

    Ui::XeniaSaveExportWizard *ui;
    QStatusBar *statusBar;

    QString sourceRoot;
    QString outputRoot;
    QString kvPath;
    QVector<SaveEntry> discoveredSaves;

    // Bumped every time scanSourceFolder() runs; lets in-flight async title name
    // lookups detect that the tree they targeted has since been torn down.
    int scanGeneration = 0;
    QHash<QString, QTreeWidgetItem*> titleNodesByTitleId;

    // Description: scan a Xenia profile content folder for savegame slots that have
    // both a body (savegame.bin) and matching header (Headers/.../*.header) file,
    // populating listSaves as a Title ID -> Content Type -> Slot tree
    void scanSourceFolder(const QString &root);

    // Description: kick off an async dbox.tools lookup to resolve a title ID to its
    // game name, updating the tree node's label if/when it succeeds
    void lookupTitleName(const QString &titleIdHex);

    // Description: human-readable "<hex> (<name>)" label for a content type folder,
    // falling back to just the hex value for unrecognized types
    QString contentTypeLabel(const QString &contentTypeHex) const;

    // Description: indices into discoveredSaves for every checked leaf in listSaves
    QVector<int> checkedSaveIndices() const;

    // Description: build the destination path for a save under the Content/<xuid>/... layout
    QString destinationPathFor(const SaveEntry &entry) const;

    // Description: concatenate header+body, open as an StfsPackage, retarget the
    // profile ID, and rehash
    void exportSave(const SaveEntry &entry);

    // Description: whether the given page's inputs are complete and valid
    bool isPageComplete(int id) const;

    // Description: destination paths of the checked saves that already exist on disk
    QStringList existingDestinations() const;

    // Description: re-evaluate whether Next/Finish should be enabled for the current page
    void updateButtonStates();

    bool isValidXuid(const QString &text) const;
};

#endif // XENIASAVEEXPORTWIZARD_H
