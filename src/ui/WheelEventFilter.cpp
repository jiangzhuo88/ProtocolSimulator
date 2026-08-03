#include "WheelEventFilter.h"
#include <QComboBox>
#include <QAbstractItemView>
WheelEventFilter::WheelEventFilter(QObject *parent) : QObject(parent)
{

}

bool WheelEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::Wheel)
    {
        QComboBox* comb = qobject_cast<QComboBox*>(watched);
        if(comb)
        {
            if(comb->view()->isVisible())
            {
                return false;
            }

            return true;
        }
    }
    return QObject::eventFilter(watched,event);
}
