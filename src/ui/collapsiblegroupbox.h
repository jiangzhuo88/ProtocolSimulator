#ifndef COLLAPSIBLEGROUPBOX_H
#define COLLAPSIBLEGROUPBOX_H

#include <QFrame>
#include <QToolButton>

class CollapsibleGroupBox : public QFrame
{
    Q_OBJECT
public:
    explicit CollapsibleGroupBox(const QString &title, QWidget *parent = nullptr);
    void setContentWidget(QWidget *widget);
    QWidget* contentWidget() const;
    bool isExpanded() const;
    void setExpanded(bool expanded);

private slots:
    void onToggle(bool checked);

private:
    QToolButton *m_button;
    QWidget *m_content;
};

#endif // COLLAPSIBLEGROUPBOX_H
