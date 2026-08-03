#ifndef WHEELEVENTFILTER_H
#define WHEELEVENTFILTER_H
#include <QObject>
#include <QEvent>
#include <QApplication>
#include <QWidget>

class WheelEventFilter : public QObject
{
    Q_OBJECT
public:
    WheelEventFilter(QObject* parent = nullptr);
protected:
    bool eventFilter(QObject *watched, QEvent *event);
};

#endif // WHEELEVENTFILTER_H
