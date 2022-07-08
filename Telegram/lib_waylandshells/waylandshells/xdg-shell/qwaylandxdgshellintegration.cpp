/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandxdgshellintegration_p.h"
#include "qwaylandxdgdecorationv1_p.h"

#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>

#include <QtGui/private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

namespace WaylandShells {

using namespace QtWaylandClient;

namespace {

bool InUse = false;

}

bool XdgShell() {
	// initialize shell integration before querying
	if (const auto integration = static_cast<QWaylandIntegration*>(
		QGuiApplicationPrivate::platformIntegration())) {
		integration->shellIntegration();
	}
    return InUse;
}

bool QWaylandXdgShellIntegration::initialize(QWaylandDisplay *display)
{
    InUse = false;

    for (QWaylandDisplay::RegistryGlobal global : display->globals()) {
        if (global.interface == QLatin1String("xdg_wm_base")) {
            m_xdgShell.reset(new QWaylandXdgShell(display, global.id, global.version));
            break;
        }
    }

    if (!m_xdgShell) {
        qCDebug(lcQpaWayland) << "Couldn't find global xdg_wm_base for xdg-shell stable";
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    InUse = true;
#else
    InUse = QWaylandShellIntegration::initialize(display);
#endif
    return InUse;
}

QWaylandShellSurface *QWaylandXdgShellIntegration::createShellSurface(QWaylandWindow *window)
{
    return m_xdgShell->getXdgSurface(window);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
void QWaylandXdgShellIntegration::handleKeyboardFocusChanged(QWaylandWindow *newFocus, QWaylandWindow *oldFocus)
{
    if (newFocus) {
        auto *xdgSurface = qobject_cast<QWaylandXdgSurface *>(newFocus->shellSurface());
        if (xdgSurface && !xdgSurface->handlesActiveState())
            m_display->handleWindowActivated(newFocus);
    }
    if (oldFocus && qobject_cast<QWaylandXdgSurface *>(oldFocus->shellSurface())) {
        auto *xdgSurface = qobject_cast<QWaylandXdgSurface *>(oldFocus->shellSurface());
        if (xdgSurface && !xdgSurface->handlesActiveState())
            m_display->handleWindowDeactivated(oldFocus);
    }
}
#endif

void *QWaylandXdgShellIntegration::nativeResourceForWindow(const QByteArray &resource, QWindow *window)
{
    if (auto waylandWindow = static_cast<QWaylandWindow *>(window->handle())) {
        if (auto xdgSurface = qobject_cast<QWaylandXdgSurface *>(waylandWindow->shellSurface())) {
            return xdgSurface->nativeResource(resource);
        }
    }
    return nullptr;
}

}

QT_END_NAMESPACE
