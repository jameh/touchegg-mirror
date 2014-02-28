/**
 * @file /src/touchegg/gestures/handler/GestureHandler.cpp
 *
 * This file is part of Touchégg.
 *
 * Touchégg is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License  as  published by  the  Free Software
 * Foundation,  either version 2 of the License,  or (at your option)  any later
 * version.
 *
 * Touchégg is distributed in the hope that it will be useful,  but  WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the  GNU General Public License  for more details.
 *
 * You should have received a copy of the  GNU General Public License along with
 * Touchégg. If not, see <http://www.gnu.org/licenses/>.
 *
 * @author José Expósito <jose.exposito89@gmail.com> (C) 2011 - 2012
 * @class  GestureHandler
 */
#include "GestureHandler.h"

#include <pthread.h>

// Lock to be used with Tap gestures
pthread_mutex_t gestureLock = PTHREAD_MUTEX_INITIALIZER;

// ****************************************************************************************************************** //
// **********                                  CONSTRUCTORS AND DESTRUCTOR                                 ********** //
// ****************************************************************************************************************** //


GestureHandler::GestureHandler(QObject *parent)
    : QObject(parent),
      currentGesture(NULL),
      gestureFact(GestureFactory::getInstance()),
      actionFact(ActionFactory::getInstance()),
      config(Config::getInstance())
{
    this->timeout = this->config->getComposedGesturesTime();
    this->consumeNextTap = false;
}

GestureHandler::~GestureHandler()
{
    pthread_mutex_lock(&gestureLock); 
    if (this->currentGesture)
	    delete this->currentGesture;
    pthread_mutex_unlock(&gestureLock); 
}


// ****************************************************************************************************************** //
// **********                                         PUBLIC SLOTS                                         ********** //
// ****************************************************************************************************************** //

void GestureHandler::executeGestureStart(const QString &type, int id, const QHash<QString, QVariant>& attrs)
{
    // If not gesture is running create one
    pthread_mutex_lock(&gestureLock); 
    if (this->currentGesture == NULL) {
        this->currentGesture = this->createGesture(type, id, attrs, false);
        if (this->currentGesture != NULL) {
	    qDebug() << "\tStarting " << type;
            this->currentGesture->start();
        }
    } else if (this->currentGesture->getId() == id) {
        qDebug() << "\tUpdating " << type;
	this->currentGesture->setAttrs(attrs);	
	this->currentGesture->update();	
    }
    pthread_mutex_unlock(&gestureLock); 
}

void GestureHandler::executeCompoundGesture(Gesture* gesture)
{
    qDebug() << "\tStarting compound gesture.";
    gesture->start();

    qDebug() << "\tUpdating compound gesture";
    gesture->update();

    // This is as far as Tap & Hold needs to go.
    if (gesture->getType() == GestureTypeEnum::DOUBLE_TAP) { 
	qDebug() << "\tFinishing double tap.";
	gesture->finish();
	delete gesture;
	gesture = NULL;
    } 
    
    delete this->currentGesture;
    this->currentGesture = gesture;
}


void GestureHandler::updateCurrentGesture(const QString &type, int id, const QHash<QString, QVariant>& attrs)
{
    // Just update non-tap based gestures.
    if (this->currentGesture->getType() != GestureTypeEnum::TAP) {
        this->currentGesture->setAttrs(attrs);
        this->currentGesture->update();
	qDebug() << "\tUpdating " << type;
	return;   
    }

    // Make our best attempt at upgrading a tap to a tap + hold
    // or a double tap.
    Gesture* gesture = this->createGesture(type, id, attrs, true);
    if (!gesture) {
	qDebug() << "Unrecognized gesture on update.";
	qDebug() << "Current Gesture NOT updated.";
        return;
    }

    int lastNumFingers = this->currentGesture->getAttrs().value(GEIS_GESTURE_ATTRIBUTE_TOUCHES).toInt();
    int curNumFingers  = attrs.value(GEIS_GESTURE_ATTRIBUTE_TOUCHES).toInt();
    if (lastNumFingers != curNumFingers) {
	qDebug() << "Number of fingers has changed.";
	qDebug() << "Current Gesture NOT updated.";
        return;
    }

    this->executeCompoundGesture(gesture);
}


void GestureHandler::executeGestureUpdate(const QString &type, int id, const QHash<QString, QVariant>& attrs)
{
    pthread_mutex_lock(&gestureLock);
        qDebug() << "\tGestureUpdate: " << type;
    // Consume any taps if needed.
    if (type == "Tap" && this->consumeNextTap) {
    	qDebug() << "\tTap consumed.";
	this->consumeNextTap = false;
	pthread_mutex_unlock(&gestureLock);
	return;
    } 

    if (!this->currentGesture) {
	this->currentGesture = this->createGesture(type, id, attrs, false);
	if (this->currentGesture->getActionType() == ActionTypeEnum::MOUSE_CLICK) {
		qDebug() << "\tFlagging next tap to be consumed.";
		this->consumeNextTap = true; 
	}

	if (this->currentGesture->getType() == GestureTypeEnum::TAP)
	    QTimer::singleShot(this->timeout, this, SLOT(executeTap()));
	else if (this->currentGesture->getType() == GestureTypeEnum::DRAG) {
	    qDebug() << "\tDrag start.";
	    this->currentGesture->start();
	    qDebug() << "\tFirst drag update.";
	    this->currentGesture->update();
	}
        qDebug() << "\tUpdate finished.";
	pthread_mutex_unlock(&gestureLock);
	return;
    }

    this->updateCurrentGesture(type, id, attrs);
    pthread_mutex_unlock(&gestureLock);
}

void GestureHandler::executeGestureFinish(const QString &type, int id, const QHash<QString, QVariant>& attrs)
{
    pthread_mutex_lock(&gestureLock);
    if (this->currentGesture != NULL && this->currentGesture->getId() == id) {
        qDebug() << "\tFinishing Gesture.";
        this->currentGesture->setAttrs(attrs);
        this->currentGesture->finish();
        delete this->currentGesture;
        this->currentGesture = NULL;
    }
    pthread_mutex_unlock(&gestureLock);
}


// ****************************************************************************************************************** //
// **********                                         PRIVATE SLOTS                                        ********** //
// ****************************************************************************************************************** //

void GestureHandler::executeTap()
{
    pthread_mutex_lock(&gestureLock);
    Gesture* tmp = this->currentGesture;

    if (tmp && tmp->getType() == GestureTypeEnum::TAP) {
	this->currentGesture = NULL;
        qDebug() << "\tTap Sending...";
        pthread_mutex_unlock(&gestureLock);
        tmp->start();
        tmp->update();
        tmp->finish();
	delete tmp;
    } else {
        qDebug() << "\tTap was converted to a compound gesture.";
        pthread_mutex_unlock(&gestureLock);
    }
}

void GestureHandler::executeSafeTap()
{
    pthread_mutex_lock(&gestureLock);
    Gesture* tmp = this->currentGesture;

    if (tmp && tmp->getType() == GestureTypeEnum::TAP) {
	this->currentGesture = NULL;
	this->consumeNextTap = true;
        qDebug() << "\tTap Sending... will be consumed.";
    	pthread_mutex_unlock(&gestureLock);
        tmp->start();
        tmp->update();
        tmp->finish();
	delete tmp;
    } else {
        qDebug() << "\tTap was converted to a compound gesture.";
        pthread_mutex_unlock(&gestureLock);
    }
}




// ****************************************************************************************************************** //
// **********                                        PRIVATE METHODS                                       ********** //
// ****************************************************************************************************************** //

Gesture *GestureHandler::createGesture(const QString &type, int id, const QHash<QString, QVariant>& attrs,
    bool isComposedGesture) const
{
    // Creamos el gesto sin su acción
    Gesture *ret;
    if (isComposedGesture)
        ret = this->gestureFact->createComposedGesture(type, id, attrs);
    else
        ret = this->gestureFact->createSimpleGesture(type, id, attrs);

    if (ret == NULL)
        return NULL;

    // Vemos sobre que ventana se ha ejecutado
    Window gestureWindow = this->getGestureWindow(attrs.value(GEIS_GESTURE_ATTRIBUTE_CHILD_WINDOW_ID).toInt());
    //if (gestureWindow == None)
    //    return NULL;
    QString appClass = this->getAppClass(gestureWindow);

    // Creamos y asignamos la acción asociada al gesto
    ActionTypeEnum::ActionType actionType = this->config->getAssociatedAction(appClass, ret->getType(),
            ret->getNumFingers(), ret->getDirection());
    QString actionSettings = this->config->getAssociatedSettings(appClass, ret->getType(), ret->getNumFingers(),
            ret->getDirection());

    ret->setAction(this->actionFact->createAction(actionType, actionSettings, gestureWindow));

    // Mostramos los datos sobre el gesto
    qDebug() << "[+] New gesture:";
    qDebug() << "\tType      -> " << GestureTypeEnum::getValue(ret->getType());
    qDebug() << "\tFingers   -> " << ret->getNumFingers();
    qDebug() << "\tDirection -> " << GestureDirectionEnum::getValue(ret->getDirection());
    qDebug() << "\tAction    -> " << ActionTypeEnum::getValue(actionType);
    qDebug() << "\tApp Class -> " << appClass;

    return ret;
}

//------------------------------------------------------------------------------

Window GestureHandler::getGestureWindow(Window window) const
{
    Window topIn = this->getTopLevelWindow(window);
    if (topIn == None)
        return None;

    // Compare the top-level window of the specified window with the possible fake-top-level window (really they are not
    // top-level windows, but are the windows that stores the attributes and more), returning the window that contains
    // the title, the class...
    Atom atomRet;
    int size;
    unsigned long numItems, bytesAfterReturn;
    unsigned char *propRet;
    long offset = 0;
    long offsetSize = 100;

    Window ret = None;

    int status;
    Atom atomList = XInternAtom(QX11Info::display(), "_NET_CLIENT_LIST_STACKING", false);
    do {
        status = XGetWindowProperty(QX11Info::display(), QX11Info::appRootWindow(), atomList,
                offset, offsetSize, false, XA_WINDOW, &atomRet, &size,
                &numItems, &bytesAfterReturn, &propRet);

        if (status == Success) {
            Window *aux = (Window *)propRet;
            unsigned int n = 0;
            while (ret == None && n < numItems) {
                // Check if the top-level window of the window of the list coincides with the window passed as argument
                if (this->getTopLevelWindow(aux[n]) == topIn)
                    ret = aux[n];
                n++;
            }
            offset += offsetSize;
            XFree(propRet);
        }
    } while (status == Success && bytesAfterReturn != 0);

    return ret;
}

Window GestureHandler::getTopLevelWindow(Window window) const
{
    Window  root, parent;
    Window *children;
    unsigned int numChildren;

    if (XQueryTree(QX11Info::display(), window, &root, &parent, &children,
            &numChildren) != 0) {
        if (children != NULL)
            XFree(children);

        if (parent == root)
            return window;
        else
            return this->getTopLevelWindow(parent);

    } else {
        return None;
    }
}

QString GestureHandler::getAppClass(Window window) const
{
    XClassHint *classHint = XAllocClassHint();
    XGetClassHint(QX11Info::display(), window, classHint);
    QString ret = classHint->res_class;
    XFree(classHint->res_class);
    XFree(classHint->res_name);
    return ret;
}
