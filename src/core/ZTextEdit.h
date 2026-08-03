#ifndef ZTEXTEDIT_H
#define ZTEXTEDIT_H

#include <QTextEdit>
#include <QContextMenuEvent>
#include <QMenu>
class ZTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    ZTextEdit(QWidget *parent = nullptr);
protected:
    void contextMenuEvent(QContextMenuEvent* event);
};

#endif // ZTEXTEDIT_H
