/**
 * @file /src/touchegg/actions/implementation/MaximizeRestoreWindow.cpp
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
 * @class  MaximizeRestoreWindow
 */
#include "MaximizeRestoreWindow.h"

// ****************************************************************************************************************** //
// **********                                  CONSTRUCTORS AND DESTRUCTOR                                 ********** //
// ****************************************************************************************************************** //

MaximizeRestoreWindow::MaximizeRestoreWindow(const QString &settings, Window window)
    : Action(settings, window)
{
    if (settings == "TOGGLE") {
        this->toggle = true;
        // leave this->maximize uninitialized
    }
    else if (settings == "MAXIMIZE") {
        this->toggle = false;
        this->maximize = true;
    }
    else if (settings == "RESTORE") {
        this->toggle = false;
        this->maximize = false;
    }
    else {
        qWarning() << "Error reading MAXIMIZE_RESTORE_WINDOW settings, using the default settings";
        this->toggle = true;
    }
}


// ****************************************************************************************************************** //
// **********                                        PUBLIC METHODS                                        ********** //
// ****************************************************************************************************************** //

void MaximizeRestoreWindow::executeStart(const QHash<QString, QVariant>&) {}

void MaximizeRestoreWindow::executeUpdate(const QHash<QString, QVariant>&) {}

void MaximizeRestoreWindow::executeFinish(const QHash<QString, QVariant>&)
{
    if (this->window == None)
        return;

    Atom atomMaxVert = XInternAtom(QX11Info::display(), "_NET_WM_STATE_MAXIMIZED_VERT", false);
    Atom atomMaxHorz = XInternAtom(QX11Info::display(), "_NET_WM_STATE_MAXIMIZED_HORZ", false);

    // Check if the window is maximized
    Atom atomRet;
    int size;
    unsigned long numItems, bytesAfterReturn;
    unsigned char *propRet;

    XGetWindowProperty(QX11Info::display(), this->window, XInternAtom(QX11Info::display(), "_NET_WM_STATE", false),
            0, 100, false, XA_ATOM, &atomRet, &size, &numItems, &bytesAfterReturn, &propRet);
    Atom *states = (Atom *)propRet;

    bool maxHor  = false;
    bool maxVert = false;
    for (unsigned int n = 0; n < numItems; n++) {
        if (states[n] == atomMaxVert)
            maxVert = true;
        if (states[n] == atomMaxHorz)
            maxHor = true;
    }
    bool maximized = maxHor && maxVert;
    XFree(propRet);

    if (this->toggle || (this->maximize && !maximized) || (!this->maximize && maximized)) {
        // If the window is maximized restore it
        XClientMessageEvent event;
        event.window = this->window;
        event.type = ClientMessage;
        event.message_type = XInternAtom(QX11Info::display(), "_NET_WM_STATE", false);
        event.format = 32;
        event.data.l[0] = maximized ? 0 : 1;
        event.data.l[1] = atomMaxVert;
        event.data.l[2] = atomMaxHorz;

        XSendEvent(QX11Info::display(), QX11Info::appRootWindow(QX11Info::appScreen()), false,
                (SubstructureNotifyMask | SubstructureRedirectMask), (XEvent *)&event);
        XFlush(QX11Info::display());
    }
}

