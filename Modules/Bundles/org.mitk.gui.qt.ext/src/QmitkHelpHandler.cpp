#include "QmitkHelpHandler.h"

#include <QKeyEvent>

#include "cherryQtAssistantUtil.h"


QmitkHelpHandler::QmitkHelpHandler(QObject* parent) : QObject(parent)
{

}

bool QmitkHelpHandler::eventFilter(QObject *obj, QEvent *event)
  {
  if (event->type() == QEvent::KeyPress)
    {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key()==16777264) //if the F1-key is pressed...
      {
      cherry::QtAssistantUtil::OpenHelpPage();
      return true;
      }
    }
  // standard event processing
  return QObject::eventFilter(obj, event);
  }
