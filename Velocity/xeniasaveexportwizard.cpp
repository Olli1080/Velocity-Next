#include "xeniasaveexportwizard.h"
#include "ui_xeniasaveexportwizard.h"

#include <algorithm>
#include <cstring>

#include <QRegularExpression>

XeniaSaveExportWizard::XeniaSaveExportWizard(QStatusBar *statusBar, QWidget *parent) :
    QWizard(parent), ui(new Ui::XeniaSaveExportWizard), statusBar(statusBar)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);
    QtHelpers::GenAdjustWidgetAppearanceToOS(this);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(onCurrentIdChanged(int)));
    connect(this, SIGNAL(finished(int)), this, SLOT(onFinished(int)));

    updateButtonStates();
}

XeniaSaveExportWizard::~XeniaSaveExportWizard()
{
    delete ui;
}

bool XeniaSaveExportWizard::isValidXuid(const QString &text) const
{
    // QtHelpers::VerifyHexString strips every "0x" before checking digits, so it would
    // accept e.g. "12340x5678ABCDEF"; require exactly 16 hex digits instead, and reject
    // the all-zero XUID (what Xenia / LIVE / PIRS packages carry as a profile ID).
    static const QRegularExpression hex16("^[0-9A-Fa-f]{16}$");
    if (!hex16.match(text).hasMatch())
        return false;

    bool ok = false;
    return text.toULongLong(&ok, 16) != 0 && ok;
}

QVector<int> XeniaSaveExportWizard::checkedSaveIndices() const
{
    QVector<int> result;
    for (int i = 0; i < ui->listSaves->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *titleNode = ui->listSaves->topLevelItem(i);
        for (int j = 0; j < titleNode->childCount(); j++)
        {
            QTreeWidgetItem *contentTypeNode = titleNode->child(j);
            for (int k = 0; k < contentTypeNode->childCount(); k++)
            {
                QTreeWidgetItem *slotItem = contentTypeNode->child(k);
                if (slotItem->checkState(0) == Qt::Checked)
                    result.append(slotItem->data(0, Qt::UserRole).toInt());
            }
        }
    }
    return result;
}

bool XeniaSaveExportWizard::isPageComplete(int id) const
{
    switch (id)
    {
        case 0:
            return !sourceRoot.isEmpty() && !checkedSaveIndices().isEmpty();
        case 1:
            return isValidXuid(ui->txtTargetXuid->text());
        case 2:
            return !outputRoot.isEmpty();
        default:
            return true;
    }
}

void XeniaSaveExportWizard::updateButtonStates()
{
    if (currentId() >= 0 && currentId() <= 2)
        button(QWizard::NextButton)->setEnabled(isPageComplete(currentId()));
}

QStringList XeniaSaveExportWizard::existingDestinations() const
{
    QStringList existing;
    for (int index : checkedSaveIndices())
    {
        QString path = destinationPathFor(discoveredSaves[index]);
        if (QFile::exists(path))
            existing << path;
    }
    return existing;
}

bool XeniaSaveExportWizard::validateCurrentPage()
{
    // QWizard has no isComplete() here (the pages are plain QWizardPages from the .ui),
    // so re-check the page's condition regardless of what state Next was left in.
    if (!isPageComplete(currentId()))
        return false;

    if (currentId() == 3)
    {
        QStringList existing = existingDestinations();
        if (!existing.isEmpty())
        {
            QStringList shown = existing.mid(0, 5);
            if (existing.size() > shown.size())
                shown << tr("... and %1 more").arg(existing.size() - shown.size());

            QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Overwrite existing files?"),
                    tr("%1 file(s) already exist at the destination and will be replaced:\n\n%2\n\nContinue?")
                        .arg(existing.size()).arg(shown.join("\n")),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return false;
        }
    }

    return QWizard::validateCurrentPage();
}

void XeniaSaveExportWizard::onCurrentIdChanged(int id)
{
    if (id == 3)
    {
        QStringList lines;
        lines << tr("Source: %1").arg(sourceRoot);
        lines << tr("Target profile XUID: %1").arg(ui->txtTargetXuid->text().toUpper());
        lines << tr("Destination: %1").arg(outputRoot);
        lines << tr("Console KeyVault: %1").arg(kvPath.isEmpty() ? tr("(none - Rehash only)") : kvPath);
        lines << QString();
        lines << tr("Save(s) to export:");

        for (int index : checkedSaveIndices())
        {
            const SaveEntry &entry = discoveredSaves[index];
            lines << tr("  - %1 / %2 / %3").arg(entry.titleId, entry.contentType, entry.slotName);
        }

        ui->lblSummary->setText(lines.join("\n"));

        ui->lblCaveat->setText(kvPath.isEmpty()
                ? tr("Note: no KeyVault was provided, so exported packages are only rehashed, not "
                     "console-signed. Xbox 360 (including RGH/JTAG consoles - the signature check is not "
                     "bypassed by those exploits) will report them as corrupted. Go back and browse for the "
                     "target console's KV.bin to produce a properly signed save.")
                : tr("Exported packages will be signed with the provided KeyVault, so the target console "
                     "should accept them as belonging to it."));
    }

    updateButtonStates();
}

void XeniaSaveExportWizard::on_btnBrowseSource_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Xenia Profile Content Folder"),
            QtHelpers::DefaultLocation());
    if (dir.isEmpty())
        return;

    sourceRoot = dir;
    ui->lblSourcePath->setText(dir);

    QString folderXuid = QFileInfo(dir).fileName().toUpper();
    ui->lblSourceXuid->setText(tr("Detected source profile XUID: %1").arg(folderXuid));

    scanSourceFolder(dir);
    updateButtonStates();
}

QString XeniaSaveExportWizard::contentTypeLabel(const QString &contentTypeHex) const
{
    bool ok = false;
    DWORD value = contentTypeHex.toUInt(&ok, 16);
    if (ok)
    {
        try
        {
            return tr("%1 (%2)").arg(contentTypeHex.toUpper(),
                    QString::fromStdString(ContentTypeToString((ContentType)value)));
        }
        catch (...)
        {
            // unrecognized content type value - fall through to the raw hex label
        }
    }
    return contentTypeHex.toUpper();
}

void XeniaSaveExportWizard::lookupTitleName(const QString &titleIdHex)
{
    int expectedGeneration = scanGeneration;

    auto *finder = new TitleIdFinder(titleIdHex, this);
    connect(finder, &TitleIdFinder::SearchFinished, this,
            [this, titleIdHex, expectedGeneration, finder](QList<TitleData> matches)
    {
        finder->deleteLater();

        // the source folder may have been rescanned (or a different one browsed to)
        // while this lookup was in flight - the tree it targeted no longer exists
        if (expectedGeneration != scanGeneration)
            return;

        if (matches.isEmpty() || matches.first().titleName.isEmpty())
            return;

        QTreeWidgetItem *titleNode = titleNodesByTitleId.value(titleIdHex, nullptr);
        if (titleNode)
            titleNode->setText(0, tr("%1 (%2)").arg(matches.first().titleName, titleIdHex.toUpper()));
    });
    finder->StartSearch();
}

void XeniaSaveExportWizard::scanSourceFolder(const QString &root)
{
    discoveredSaves.clear();
    ui->listSaves->clear();
    titleNodesByTitleId.clear();
    scanGeneration++;

    // every setCheckState() below would otherwise fire itemChanged and walk the whole
    // tree again via updateButtonStates(); the caller refreshes the buttons once instead
    const QSignalBlocker treeSignalBlocker(ui->listSaves);

    static const QRegularExpression titleIdPattern("^[0-9A-Fa-f]{8}$");

    QHash<QString, QTreeWidgetItem*> contentTypeNodes;

    QDir rootDir(root);
    const QStringList titleDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &titleId : titleDirs)
    {
        if (titleId.compare("FFFE07D1", Qt::CaseInsensitive) == 0)
            continue; // dashboard/profile package - out of scope

        QDir titleDir(rootDir.filePath(titleId));
        const QStringList contentTypeDirs = titleDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &contentType : contentTypeDirs)
        {
            if (contentType.compare("Headers", Qt::CaseInsensitive) == 0)
                continue;

            QDir contentDir(titleDir.filePath(contentType));
            const QStringList slotDirs = contentDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

            for (const QString &slot : slotDirs)
            {
                QString bodyPath = contentDir.filePath(slot) + "/savegame.bin";
                QString headerPath = titleDir.filePath("Headers/" + contentType + "/" + slot + ".header");

                if (!QFile::exists(bodyPath) || !QFile::exists(headerPath))
                    continue;

                SaveEntry entry;
                entry.titleId = titleId;
                entry.contentType = contentType;
                entry.slotName = slot;
                entry.headerPath = headerPath;
                entry.bodyPath = bodyPath;
                entry.size = QFileInfo(headerPath).size() + QFileInfo(bodyPath).size();
                discoveredSaves.append(entry);
                int entryIndex = discoveredSaves.size() - 1;

                QTreeWidgetItem *titleNode = titleNodesByTitleId.value(titleId, nullptr);
                if (!titleNode)
                {
                    titleNode = new QTreeWidgetItem(ui->listSaves);
                    titleNode->setText(0, titleId.toUpper());
                    titleNode->setFlags(titleNode->flags() | Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable);
                    titleNode->setCheckState(0, Qt::Checked);
                    titleNodesByTitleId.insert(titleId, titleNode);

                    // TitleIdFinder falls back to a partial *name* search for anything
                    // that isn't exactly 8 hex digits, which would label a stray folder
                    // with an unrelated game
                    if (titleIdPattern.match(titleId).hasMatch())
                        lookupTitleName(titleId);
                }

                QString contentTypeKey = titleId + "/" + contentType;
                QTreeWidgetItem *contentTypeNode = contentTypeNodes.value(contentTypeKey, nullptr);
                if (!contentTypeNode)
                {
                    contentTypeNode = new QTreeWidgetItem(titleNode);
                    contentTypeNode->setText(0, contentTypeLabel(contentType));
                    contentTypeNode->setFlags(contentTypeNode->flags() | Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable);
                    contentTypeNode->setCheckState(0, Qt::Checked);
                    contentTypeNodes.insert(contentTypeKey, contentTypeNode);
                }

                auto *slotItem = new QTreeWidgetItem(contentTypeNode);
                slotItem->setText(0, tr("%1  (%2 KB)").arg(slot).arg(entry.size / 1024));
                slotItem->setFlags(slotItem->flags() | Qt::ItemIsUserCheckable);
                slotItem->setCheckState(0, Qt::Checked);
                slotItem->setData(0, Qt::UserRole, entryIndex);
            }
        }
    }

    ui->listSaves->expandAll();
}

void XeniaSaveExportWizard::on_listSaves_itemChanged(QTreeWidgetItem *item, int column)
{
    if (column == 0 && item->childCount() > 0 && item->checkState(0) != Qt::PartiallyChecked)
    {
        // propagate a user check/uncheck on a Title or Content Type node down to its
        // children; Qt::ItemIsAutoTristate already handles the reverse direction
        // (parent reflecting a mix of checked/unchecked children) automatically
        Qt::CheckState state = item->checkState(0);
        QList<QTreeWidgetItem*> stack;
        stack.append(item);

        ui->listSaves->blockSignals(true);
        while (!stack.isEmpty())
        {
            QTreeWidgetItem *current = stack.takeLast();
            for (int i = 0; i < current->childCount(); i++)
            {
                QTreeWidgetItem *child = current->child(i);
                child->setCheckState(0, state);
                stack.append(child);
            }
        }
        ui->listSaves->blockSignals(false);
    }

    updateButtonStates();
}

void XeniaSaveExportWizard::on_txtTargetXuid_textChanged(const QString &text)
{
    if (isValidXuid(text))
        QtHelpers::ClearErrorStyle(ui->txtTargetXuid);
    else
        QtHelpers::SetErrorStyle(ui->txtTargetXuid);

    updateButtonStates();
}

void XeniaSaveExportWizard::on_btnBrowseProfile_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select a Profile Package or Save"),
            QtHelpers::DefaultLocation(), tr("All Files (*.*)"));
    if (path.isEmpty())
        return;

    try
    {
        StfsPackage pkg(path.toStdString(), StfsPackageDontReadFileListing);
        QString xuid = QtHelpers::ByteArrayToString(pkg.metaData->profileID, 8, false);
        if (!isValidXuid(xuid))
        {
            QMessageBox::warning(this, tr("No profile ID"),
                    tr("The selected package has no usable profile ID (it is all zeros, as in "
                       "Xenia-created or LIVE/PIRS packages). Choose a package owned by the target profile."));
            return;
        }
        ui->txtTargetXuid->setText(xuid);
    }
    catch (...)
    {
        QMessageBox::warning(this, tr("Error"),
                tr("Could not read a profile ID from the selected file. It may not be a valid STFS package."));
    }
}

void XeniaSaveExportWizard::on_btnBrowseKv_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Target Console KeyVault"),
            QtHelpers::DefaultLocation(), tr("KeyVault (KV*.bin);;All Files (*.*)"));
    if (path.isEmpty())
        return;

    kvPath = path;
    ui->txtKvPath->setText(path);
}

void XeniaSaveExportWizard::on_btnBrowseOutput_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"),
            QtHelpers::DefaultLocation());
    if (dir.isEmpty())
        return;

    outputRoot = dir;
    ui->lblOutputPath->setText(dir);

    QString xuid = ui->txtTargetXuid->text().toUpper();
    ui->lblOutputPreview->setText(tr("Example output file:\n%1/Content/%2/<titleID>/<contentType>/<slotName>")
            .arg(dir, xuid));

    updateButtonStates();
}

QString XeniaSaveExportWizard::destinationPathFor(const SaveEntry &entry) const
{
    QString xuid = ui->txtTargetXuid->text().toUpper();
    return QString("%1/Content/%2/%3/%4/%5")
            .arg(outputRoot, xuid, entry.titleId, entry.contentType, entry.slotName);
}

void XeniaSaveExportWizard::exportSave(const SaveEntry &entry)
{
    // Xenia does not store its saves as a monolithic, block-encoded STFS file. Its
    // ".header" file is a real STFS header, but with an empty volume descriptor (zero
    // allocated blocks / zero-length file table) - the actual save content
    // ("savegame.bin") is kept as a plain file on Xenia's own host filesystem, entirely
    // outside of the STFS block/hash-tree encoding. Naively concatenating header+body
    // bytes therefore produces a file that *parses* (Rehash() won't even complain, since
    // it just rehashes whatever's there) but whose file table still describes zero
    // files - which is exactly the "corrupted savegame" a console or Xenia reports.
    //
    // The correct approach is to build a brand-new, properly block-encoded STFS package
    // and inject "savegame.bin" into it via StfsPackage::InjectFile(), which allocates
    // blocks and builds the file table correctly, then copy over the descriptive
    // metadata (content type, title ID, thumbnails, etc.) from the source header.

    QString targetXuidHex = ui->txtTargetXuid->text();

    if (!QFile::exists(entry.bodyPath))
        throw std::string("Source file no longer exists: " + entry.bodyPath.toStdString());

    // Read the source header, patching its console-type byte if needed (see below),
    // and open it read-only just to pull out its metadata fields.
    QFile headerFile(entry.headerPath);
    if (!headerFile.open(QIODevice::ReadOnly))
        throw std::string("Could not read: " + entry.headerPath.toStdString());
    QByteArray headerBytes = headerFile.readAll();
    headerFile.close();

    // Xenia writes a fully-zeroed CON certificate block (it doesn't emulate real
    // console signing hardware), so the console-type byte at header offset 31 is 0 -
    // neither DevKit(1) nor Retail(2). XboxInternals' certificate reader validates this
    // strictly and throws "Invalid console type" before StfsPackage can even open the
    // file, so it must be patched here, before we hand the bytes to StfsPackage.
    if (headerBytes.size() > 31)
    {
        BYTE consoleTypeByte = static_cast<BYTE>(headerBytes[31]);
        if ((consoleTypeByte & 3) != DevKit && (consoleTypeByte & 3) != Retail)
            headerBytes[31] = static_cast<char>((consoleTypeByte & 0xFC) | Retail);
    }

    QString tempHeaderPath = QDir::tempPath() + "/" + QUuid::createUuid().toString().replace("{",
            "").replace("}", "").replace("-", "");
    {
        QFile tempHeaderFile(tempHeaderPath);
        if (!tempHeaderFile.open(QIODevice::WriteOnly))
            throw std::string("Could not create a temporary file to read source metadata.");
        tempHeaderFile.write(headerBytes);
    }

    // Build into "<dest>.part" and only swap it into place once the whole package
    // (inject, rehash, resign) succeeded, so a failure never leaves a half-built or
    // unsigned file at the real destination and never destroys an existing save.
    QString destPath = destinationPathFor(entry);
    QString partPath = destPath + ".part";
    QDir().mkpath(QFileInfo(destPath).absolutePath());
    QFile::remove(partPath);

    try
    {
        // scoped so both packages release their file handles before the rename below
        {
        StfsPackage srcPkg(tempHeaderPath.toStdString(), StfsPackageDontReadFileListing);
        StfsPackage newPkg(partPath.toStdString(), StfsPackageCreate);

        newPkg.metaData->contentType = srcPkg.metaData->contentType;
        newPkg.metaData->titleID = srcPkg.metaData->titleID;
        newPkg.metaData->mediaID = srcPkg.metaData->mediaID;
        newPkg.metaData->version = srcPkg.metaData->version;
        newPkg.metaData->baseVersion = srcPkg.metaData->baseVersion;
        newPkg.metaData->platform = srcPkg.metaData->platform;
        newPkg.metaData->executableType = srcPkg.metaData->executableType;
        newPkg.metaData->discNumber = srcPkg.metaData->discNumber;
        newPkg.metaData->discInSet = srcPkg.metaData->discInSet;
        newPkg.metaData->savegameID = srcPkg.metaData->savegameID;
        // Xenia's own header reports contentSize=0 (it doesn't track this for its
        // split storage format); use the real body size instead of inheriting that.
        newPkg.metaData->contentSize = static_cast<UINT64>(QFileInfo(entry.bodyPath).size());
        newPkg.metaData->displayName = srcPkg.metaData->displayName;
        newPkg.metaData->displayDescription = srcPkg.metaData->displayDescription;
        newPkg.metaData->publisherName = srcPkg.metaData->publisherName;
        newPkg.metaData->titleName = srcPkg.metaData->titleName;
        newPkg.metaData->transferFlags = srcPkg.metaData->transferFlags;
        newPkg.metaData->thumbnailImage = srcPkg.metaData->thumbnailImage;
        newPkg.metaData->thumbnailImageSize = srcPkg.metaData->thumbnailImageSize;
        newPkg.metaData->titleThumbnailImage = srcPkg.metaData->titleThumbnailImage;
        newPkg.metaData->titleThumbnailImageSize = srcPkg.metaData->titleThumbnailImageSize;
        std::memcpy(newPkg.metaData->consoleID, srcPkg.metaData->consoleID, 5);
        std::memcpy(newPkg.metaData->deviceID, srcPkg.metaData->deviceID, 0x14);

        // Xenia's saves carry a fully-empty license table (every entry Unused) - real
        // console-created saves for this game instead use a single Unrestricted entry
        // (no per-profile lock at the license level; ownership is conveyed by the
        // profileID header field alone). StfsPackageCreate already initializes new
        // packages with that same sane Unrestricted default, so only overwrite it with
        // the source's table when the source actually has real license data - blindly
        // copying Xenia's empty table would clobber that default with an all-Unused
        // table, which is exactly the kind of malformed license data a console refuses
        // to load ("corrupted savegame").
        bool sourceHasRealLicense = false;
        for (int i = 0; i < 0x10; i++)
        {
            if (srcPkg.metaData->licenseData[i].type != Unused)
            {
                sourceHasRealLicense = true;
                break;
            }
        }

        QtHelpers::ParseHexStringBuffer(targetXuidHex, newPkg.metaData->profileID, 8);

        if (sourceHasRealLicense)
        {
            std::copy(srcPkg.metaData->licenseData, srcPkg.metaData->licenseData + 0x10, newPkg.metaData->licenseData);

            bool ok = false;
            UINT64 xuidValue = targetXuidHex.toULongLong(&ok, 16);
            if (ok)
            {
                for (int i = 0; i < 0x10; i++)
                {
                    LicenseEntry &license = newPkg.metaData->licenseData[i];
                    // the on-disk field is 48 bits (type occupies the top 16), so a full
                    // 64-bit XUID would corrupt the license type when written
                    if (license.type == ConsoleProfileLicense && license.data != 0)
                        license.data = xuidValue & 0xFFFFFFFFFFFFULL;
                }
            }
        }

        newPkg.metaData->WriteMetaData();
        newPkg.InjectFile(entry.bodyPath.toStdString(), "savegame.bin");

        newPkg.Rehash();

        // Console-sign with the target console's own KeyVault, if provided. Without
        // this, the package is only internally consistent, not console-signed, and a
        // real Xbox 360 - RGH/JTAG included, since that exploit doesn't bypass the
        // dashboard's own content signature check - will report it as corrupted.
        if (!kvPath.isEmpty())
        {
            try
            {
                newPkg.Resign(kvPath.toStdString());
            }
            catch (std::string &e)
            {
                throw std::string("KeyVault signing failed (wrong or incomplete KV.bin?): " + e);
            }
        }

        newPkg.Close();
        }

        if (QFile::exists(destPath) && !QFile::remove(destPath))
            throw std::string("Could not replace existing file: " + destPath.toStdString());
        if (!QFile::rename(partPath, destPath))
            throw std::string("Could not move the finished package into place: " + destPath.toStdString());
    }
    catch (...)
    {
        QFile::remove(partPath);
        QFile::remove(tempHeaderPath);
        throw;
    }

    QFile::remove(tempHeaderPath);
}

void XeniaSaveExportWizard::onFinished(int status)
{
    if (status == 0)
        return;

    QStringList failures;
    int successCount = 0;
    int attemptCount = 0;

    for (int index : checkedSaveIndices())
    {
        attemptCount++;
        const SaveEntry &entry = discoveredSaves[index];

        try
        {
            exportSave(entry);
            successCount++;
        }
        catch (std::string &ex)
        {
            failures << tr("%1/%2/%3: %4").arg(entry.titleId, entry.contentType, entry.slotName,
                    QString::fromStdString(ex));
        }
        catch (QString &ex)
        {
            failures << tr("%1/%2/%3: %4").arg(entry.titleId, entry.contentType, entry.slotName, ex);
        }
        catch (std::exception &ex)
        {
            failures << tr("%1/%2/%3: %4").arg(entry.titleId, entry.contentType, entry.slotName, ex.what());
        }
        catch (...)
        {
            failures << tr("%1/%2/%3: unknown error").arg(entry.titleId, entry.contentType, entry.slotName);
        }
    }

    QString summary = tr("Exported %1 of %2 save(s) successfully.").arg(successCount).arg(attemptCount);
    if (!failures.isEmpty())
        summary += "\n\n" + tr("Failed:") + "\n" + failures.join("\n");

    QMessageBox::information(this, tr("Export Complete"), summary);

    if (statusBar)
        statusBar->showMessage(tr("Exported %1 of %2 save(s) to a different profile")
                .arg(successCount).arg(attemptCount), 5000);
}
