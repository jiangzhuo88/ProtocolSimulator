#include "collapsiblegroupbox.h"
#include <QVBoxLayout>

CollapsibleGroupBox::CollapsibleGroupBox(const QString &title, QWidget *parent)
    : QFrame(parent), m_button(new QToolButton(this)), m_content(new QWidget(this))
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Sunken);

    m_button->setText(" " + title);
    m_button->setCheckable(true);
    m_button->setChecked(true);
    m_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_button->setArrowType(Qt::DownArrow);
    m_button->setStyleSheet("QToolButton { border: none; font-weight: bold; padding: 2px; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(2);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->addWidget(m_button, 0, Qt::AlignLeft);
    mainLayout->addWidget(m_content);

    connect(m_button, &QToolButton::toggled, this, &CollapsibleGroupBox::onToggle);
}

void CollapsibleGroupBox::setContentWidget(QWidget *widget)
{
    if (m_content->layout()) {
        delete m_content->layout();
    }
    auto *layout = new QVBoxLayout(m_content);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(widget);
}

QWidget* CollapsibleGroupBox::contentWidget() const
{
    return m_content;
}

bool CollapsibleGroupBox::isExpanded() const
{
    return m_button->isChecked();
}

void CollapsibleGroupBox::setExpanded(bool expanded)
{
    if (m_button->isChecked() != expanded)
        m_button->setChecked(expanded);
}

void CollapsibleGroupBox::onToggle(bool checked)
{
    m_button->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    m_content->setVisible(checked);
}
