#include "scenemanagedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QEvent>

SceneManageDialog::SceneManageDialog(ConfigManager *config, QWidget *parent)
    : QDialog(parent), m_config(config)
{
    setupUi();
    refreshList();
    retranslateUi();
}

void SceneManageDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
}

void SceneManageDialog::retranslateUi()
{
    setWindowTitle(tr("场景管理"));
    if (m_listGroup) m_listGroup->setTitle(tr("场景列表"));
    if (m_editGroup) m_editGroup->setTitle(tr("场景属性"));
    if (m_nameLabel) m_nameLabel->setText(tr("场景名称:"));
    if (m_portLabel) m_portLabel->setText(tr("TCP端口:"));
    if (m_btnAdd) m_btnAdd->setText(tr("新建场景"));
    if (m_btnRemove) m_btnRemove->setText(tr("删除场景"));
    if (m_btnApply) m_btnApply->setText(tr("应用修改"));
}

void SceneManageDialog::setupUi()
{
    resize(500, 400);

    auto layout = new QVBoxLayout(this);

    // 场景列表
    m_listGroup = new QGroupBox;
    auto listLayout = new QVBoxLayout(m_listGroup);
    m_list = new QListWidget;
    listLayout->addWidget(m_list);

    auto btnLayout = new QHBoxLayout;
    m_btnAdd = new QPushButton;
    m_btnRemove = new QPushButton;
    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnRemove);
    listLayout->addLayout(btnLayout);
    layout->addWidget(m_listGroup);

    // 编辑区域
    m_editGroup = new QGroupBox;
    auto editLayout = new QVBoxLayout(m_editGroup);

    auto nameLayout = new QHBoxLayout;
    m_nameLabel = new QLabel;
    nameLayout->addWidget(m_nameLabel);
    m_nameEdit = new QLineEdit;
    nameLayout->addWidget(m_nameEdit);
    editLayout->addLayout(nameLayout);

    auto portLayout = new QHBoxLayout;
    m_portLabel = new QLabel;
    portLayout->addWidget(m_portLabel);
    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);
    portLayout->addWidget(m_portSpin);
    portLayout->addStretch();
    m_btnApply = new QPushButton;
    portLayout->addWidget(m_btnApply);
    editLayout->addLayout(portLayout);

    layout->addWidget(m_editGroup);

    // 确定/取消
    auto okLayout = new QHBoxLayout;
    okLayout->addStretch();
    auto btnOk = new QPushButton(tr("确定"));
    auto btnCancel = new QPushButton(tr("取消"));
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
        m_list->addItem(tr("%1 (端口:%2)").arg(s.name).arg(s.tcpPort));
}

void SceneManageDialog::onAddScene()
{
    SceneConfig scene;
    scene.name = tr("场景%1").arg(m_config->scenes().size() + 1);
    scene.tcpPort = 8080 + m_config->scenes().size();
    m_config->addScene(scene);
    refreshList();
    m_list->setCurrentRow(m_list->count() - 1);
}

void SceneManageDialog::onRemoveScene()
{
    int row = m_list->currentRow();
    if (row < 0) return;
    auto ret = QMessageBox::question(this, tr("确认"), tr("确定删除选中的场景?"));
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
