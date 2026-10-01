#include "SettingsTab.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

SettingsTab::SettingsTab(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    status = new QLabel(this);
    status->setObjectName(QStringLiteral("statusLabel"));
    status->setWordWrap(true);

    setupButton = new QPushButton(tr("Set up"), this);
    setupButton->setObjectName(QStringLiteral("setupButton"));
    setupButton->hide();

    manual = new QLabel(this);
    manual->setObjectName(QStringLiteral("manualCommands"));
    manual->setTextInteractionFlags(Qt::TextSelectableByMouse);
    manual->setWordWrap(true);
    manual->hide();

    restartButton = new QPushButton(tr("Restart Plasma power management"), this);
    restartButton->setObjectName(QStringLiteral("restartButton"));
    restartButton->hide();

    accentBox = new QCheckBox(tr("Follow Plasma's accent color"), this);
    accentBox->setObjectName(QStringLiteral("accentCheck"));

    table = new QTableWidget(0, 3, this);
    table->setObjectName(QStringLiteral("deviceTable"));
    table->setHorizontalHeaderLabels({tr("Device"), tr("Dim"), tr("Accent color")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    layout->addWidget(status);
    layout->addWidget(setupButton);
    layout->addWidget(manual);
    layout->addWidget(restartButton);
    layout->addWidget(accentBox);
    layout->addWidget(table);

    connect(setupButton, &QPushButton::clicked, this, &SettingsTab::setupRequested);
    connect(restartButton, &QPushButton::clicked, this, &SettingsTab::restartRequested);
    connect(accentBox, &QCheckBox::toggled, this, [this](bool enabled) {
        if(!updating)
        {
            emit accentToggled(enabled);
        }
    });
    connect(table, &QTableWidget::itemChanged, this, &SettingsTab::onItemChanged);
}

void SettingsTab::setStatusLines(const QStringList& lines)
{
    status->setText(lines.join(QLatin1Char('\n')));
}

void SettingsTab::setSetupVisible(bool visible)
{
    setupButton->setVisible(visible);
}

void SettingsTab::setManualCommands(const QString& commands)
{
    manual->setText(commands);
    manual->setVisible(!commands.isEmpty());
}

void SettingsTab::setRestartVisible(bool visible)
{
    restartButton->setVisible(visible);
}

void SettingsTab::setAccentEnabled(bool enabled)
{
    updating = true;
    accentBox->setChecked(enabled);
    updating = false;
}

void SettingsTab::setDevices(const std::vector<DeviceRow>& rows, const PluginSettings& settings)
{
    updating = true;
    table->setRowCount(int(rows.size()));
    for(int row = 0; row < int(rows.size()); row++)
    {
        const DeviceOptions options = settings.optionsFor(rows[std::size_t(row)].key);

        auto* nameItem = new QTableWidgetItem(QString::fromStdString(rows[std::size_t(row)].name));
        nameItem->setData(Qt::UserRole, QString::fromStdString(rows[std::size_t(row)].key));
        nameItem->setFlags(Qt::ItemIsEnabled);

        auto* dimItem = new QTableWidgetItem();
        dimItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        dimItem->setCheckState(options.dim ? Qt::Checked : Qt::Unchecked);

        auto* accentItem = new QTableWidgetItem();
        accentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        accentItem->setCheckState(options.accent ? Qt::Checked : Qt::Unchecked);

        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, dimItem);
        table->setItem(row, 2, accentItem);
    }
    updating = false;
}

void SettingsTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    emit shown();
}

void SettingsTab::onItemChanged(QTableWidgetItem* item)
{
    if(updating || item->column() == 0)
    {
        return;
    }
    const int row = item->row();
    const QString key = table->item(row, 0)->data(Qt::UserRole).toString();
    emit deviceOptionsChanged(key, table->item(row, 1)->checkState() == Qt::Checked, table->item(row, 2)->checkState() == Qt::Checked);
}
