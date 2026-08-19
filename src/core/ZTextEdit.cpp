#include "ZTextEdit.h"
#include <QTextBlock>
ZTextEdit::ZTextEdit(QWidget *parent)
    :QTextEdit(parent)
{
//    connect(this,&QTextEdit::textChanged,this,[=]()
//    {
//        QTextDocument *doc = this->document();
//        if(doc->blockCount() > 200)
//        {
//            QTextCursor cursor = this->textCursor();
//            QTextCursor endCursor(doc->findBlockByLineNumber(200));
////            cursor.movePosition(QTextCursor::End);
//            cursor.setPosition(endCursor.position(),QTextCursor::KeepAnchor);
//            cursor.removeSelectedText();
//            this->setTextCursor(cursor);
//        }
//    });

}

void ZTextEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction* actClear = menu.addAction(tr("清空"));
    connect(actClear,&QAction::triggered,this,[this]()
    {
        this->clear();
    });
    menu.exec(event->globalPos());
    event->accept();
}
