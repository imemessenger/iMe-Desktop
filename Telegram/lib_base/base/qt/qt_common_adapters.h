// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtNetwork/QLocalSocket>
#include <QtNetwork/QNetworkReply>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/QInputDevice>
#else // Qt >= 6.0.0
#include <QtGui/QTouchDevice>
#endif // Qt < 6.0.0

namespace base {

using QLocalSocketErrorSignal =
	void(QLocalSocket::*)(QLocalSocket::LocalSocketError);
using QNetworkReplyErrorSignal =
	void(QNetworkReply::*)(QNetworkReply::NetworkError);
using QTcpSocketErrorSignal =
	void(QTcpSocket::*)(QAbstractSocket::SocketError);

inline constexpr auto QLocalSocket_error =
	QLocalSocketErrorSignal(&QLocalSocket::errorOccurred);
inline constexpr auto QNetworkReply_error =
	QNetworkReplyErrorSignal(&QNetworkReply::errorOccurred);
inline constexpr auto QTcpSocket_error =
	QTcpSocketErrorSignal(&QAbstractSocket::errorOccurred);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using NativeEventResult = qintptr;
using TouchDevice = QInputDevice::DeviceType;
#else // Qt >= 6.0.0
using NativeEventResult = long;
using TouchDevice = QTouchDevice;
#endif // Qt < 6.0.0

} // namespace base
