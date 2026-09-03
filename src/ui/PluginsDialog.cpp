#include "PluginsDialog.h"

#include "app/PluginHost.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace PluginsDialog {

void show(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Plugins"));
    dialog.resize(420, 320);

    auto *layout = new QVBoxLayout(&dialog);

    auto *list = new QListWidget(&dialog);
    layout->addWidget(list, 1);

    auto refreshList = [list] {
        list->clear();
        for (const PluginHost::PluginInfo &info : PluginHost::discoveredPlugins()) {
            const QString label = info.version.isEmpty() ? info.name : QStringLiteral("%1 (%2)").arg(info.name, info.version);
            auto *item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, info.id);
            item->setToolTip(info.description);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(PluginHost::isEnabled(info.id) ? Qt::Checked : Qt::Unchecked);
        }
        if (list->count() == 0) {
            auto *empty = new QListWidgetItem(QObject::tr("No plugins found."), list);
            empty->setFlags(Qt::NoItemFlags);
        }
    };
    refreshList();

    QObject::connect(list, &QListWidget::itemChanged, &dialog, [refreshList](QListWidgetItem *item) {
        const QString pluginId = item->data(Qt::UserRole).toString();
        if (pluginId.isEmpty()) {
            return;
        }
        PluginHost::setEnabled(pluginId, item->checkState() == Qt::Checked);
        PluginHost::reload();
        refreshList(); // re-reads discoveredPlugins() in case loading it just failed
    });

    auto *openFolderButton = new QPushButton(QObject::tr("Open Plugins Folder"), &dialog);
    QObject::connect(openFolderButton, &QPushButton::clicked, &dialog,
                      [] { QDesktopServices::openUrl(QUrl::fromLocalFile(PluginHost::pluginsDirectory())); });
    layout->addWidget(openFolderButton);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    dialog.exec();
}

} // namespace PluginsDialog
