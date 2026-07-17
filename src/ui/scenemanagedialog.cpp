#include "scenemanagedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>

SceneManageDialog::SceneManageDialog(ConfigManager *config, QWidget *parent)
    : QDialog(parent), m_config(config)
{
    setupUi();
    refreshList();
}

void SceneManageDialog::setupUi()
{
    setWindowTitle("场景管理");
    resize(500, 400);

    auto layout = new QVBoxLayout(this);

    // 场景列表
    auto listGroup = new QGroupBox("场景列表");
    auto listLayout = new QVBoxLayout(listGroup);
    m_list = new QListWidget;
    listLayout->addWidget(m_list);

    auto btnLayout = new QHBoxLayout;
    m_btnAdd = new QPushButton("新建场景");
    m_btnRemove = new QPushButton("删除场景");
    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnRemove);
    listLayout->addLayout(btnLayout);
    layout->addWidget(listGroup);

    // 编辑区域
    auto editGroup = new QGroupBox("场景属性");
    auto editLayout = new QVBoxLayout(editGroup);

    auto nameLayout = new QHBoxLayout;
    nameLayout->addWidget(new QLabel("场景名称:"));
    m_nameEdit = new QLineEdit;
    nameLayout->addWidget(m_nameEdit);
    editLayout->addLayout(nameLayout);

    auto portLayout = new QHBoxLayout;
    portLayout->addWidget(new QLabel("TCP端口:"));
    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);
    portLayout->addWidget(m_portSpin);
    portLayout->addStretch();
    m_btnApply = new QPushButton("应用修改");
    portLayout->addWidget(m_btnApply);
    editLayout->addLayout(portLayout);

    layout->addWidget(editGroup);

    // 确定/取消
    auto okLayout = new QHBoxLayout;
    okLayout->addStretch();
    auto btnOk = new QPushButton("确定");
    auto btnCancel = new QPushButton("取消");
    okLayout->addWidget(btnOk);
    okLayout->addWidget(btnCancel);
    layout->addLayout(okLayout);

    connect(m_btnAdd, &QPushButton::clicked, this, &SceneManageDialog::onAddScene);
    connect(m_btnRemove, &QPushButton::clicked, this, &SceneManageDialog::onRemoveScene);
    connect(m_list, &QListWidget::currentRowChanged, this, &SceneManageDialog::onSceneSelectionChanged);
    connect(m_btnApply, &QPushButton::clicked, this, &SceneManageDialog::onApplyChanges);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void SceneManageDialog::refreshList()
{
    m_list->clear();
    for (const auto &s : m_config->scenes())
        m_list->addItem(QString("%1 (端口:%2)").arg(s.name).arg(s.tcpPort));
}

void SceneManageDialog::onAddScene()
{
    SceneConfig scene;
    scene.name = QString("场景%1").arg(m_config->scenes().size() + 1);
    scene.tcpPort = 8080 + m_config->scenes().size();
    m_config->addScene(scene);
    refreshList();
    m_list->setCurrentRow(m_list->count() - 1);
}

void SceneManageDialog::onRemoveScene()
{
    int row = m_list->currentRow();
    if (row < 0) return;
    auto ret = QMessageBox::question(this, "确认", "确定删除选中的场景?");
    if (ret == QMessageBox::Yes) {
        m_config->removeScene(row);
        refreshList();
    }
}

void SceneManageDialog::onSceneSelectionChanged()
{
    int row = m_list->currentRow();
    if (row < 0 || row >= m_config->scenes().size()) return;
    const SceneConfig &scene = m_config->scenes()[row];
    m_nameEdit->setText(scene.name);
    m_portSpin->setValue(scene.tcpPort);
}

void SceneManageDialog::onApplyChanges()
{
    int row = m_list->currentRow();
    if (row < 0 || row >= m_config->scenes().size()) return;
    SceneConfig &scene = m_config->scenes()[row];
    scene.name = m_nameEdit->text().trimmed();
    scene.tcpPort = m_portSpin->value();
    refreshList();
    m_list->setCurrentRow(row);
}
