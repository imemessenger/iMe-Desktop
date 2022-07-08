// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/single_instance.h"

#include "base/crc32hash.h"
#include "base/qt/qt_common_adapters.h"
#include "base/platform/base_platform_process.h"

#include <QtCore/QStandardPaths>
#include <QtWidgets/QWidget>
#include <QtGui/QWindow>

namespace base {
namespace {

[[nodiscard]] QString NameForPath(
		const QString &uniqueApplicationName,
		const QString &path) {
	const auto hash = [](const QString &text) {
		return crc32(text.data(), text.size() * sizeof(QChar));
	};
	const auto ints = std::array{ hash(uniqueApplicationName), hash(path) };
	auto raw = QByteArray(ints.size() * sizeof(ints[0]), Qt::Uninitialized);
	memcpy(raw.data(), ints.data(), raw.size());
	return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding));
}

void CleanName(const QString &name) {
#ifndef Q_OS_WIN
	QFile(name).remove();
#endif // Q_OS_WIN
}

[[nodiscard]] QByteArray EncodeMessage(const QByteArray &message) {
	return message.toBase64(QByteArray::Base64UrlEncoding) + ';';
}

[[nodiscard]] std::optional<QByteArray> DecodeMessage(const QByteArray &message) {
	if (!message.endsWith(';')) {
		return std::nullopt;
	}
	return QByteArray::fromBase64(
		message.mid(0, message.size() - 1),
		QByteArray::Base64UrlEncoding);
}

} // namespace

SingleInstance::SingleInstance() = default;

void SingleInstance::start(
		const QString &uniqueApplicationName,
		const QString &path,
		Fn<void()> primary,
		Fn<void()> secondary,
		Fn<void()> fail) {
	const auto handleError = [=](QLocalSocket::LocalSocketError error) {
		clearSocket();
		QObject::connect(
			&_server,
			&QLocalServer::newConnection,
			[=] { newInstanceConnected(); });
		if (closeExisting() && _server.listen(_name)) {
			primary();
		} else {
			fail();
		}
	};

	QObject::connect(&_socket, &QLocalSocket::connected, secondary);
	QObject::connect(&_socket, QLocalSocket_error, handleError);
	QObject::connect(
		&_socket,
		&QLocalSocket::disconnected,
		[=] { handleError(QLocalSocket::PeerClosedError); });

	_lockFile.setFileName(path + "_single_instance.tmp");
	_name = NameForPath(uniqueApplicationName, path);
	_socket.connectToServer(_name);
}

SingleInstance::~SingleInstance() {
	clearSocket();
	clearLock();
}

bool SingleInstance::closeExisting() {
	if (!_lock.lock(_lockFile, QIODevice::WriteOnly)) {
		return false;
	}
	CleanName(_name);
	return true;
}

void SingleInstance::clearSocket() {
	QObject::disconnect(
		&_socket,
		&QLocalSocket::connected,
		nullptr,
		nullptr);
	QObject::disconnect(
		&_socket,
		&QLocalSocket::disconnected,
		nullptr,
		nullptr);
	QObject::disconnect(
		&_socket,
		QLocalSocket_error,
		nullptr,
		nullptr);
	QObject::disconnect(
		&_socket,
		&QLocalSocket::readyRead,
		nullptr,
		nullptr);
	_socket.close();
}

void SingleInstance::clearLock() {
	if (!_lock.locked()) {
		return;
	}
	_lock.unlock();
	_lockFile.close();
	_lockFile.remove();
}

void SingleInstance::send(const QByteArray &command, Fn<void()> done) {
	Expects(_socket.state() == QLocalSocket::ConnectedState);

	const auto received = std::make_shared<QByteArray>();
	const auto handleRead = [=] {
		Expects(_socket.state() == QLocalSocket::ConnectedState);

		received->append(_socket.readAll());
		if (const auto response = DecodeMessage(*received)) {
			const auto match = QRegularExpression(
				"^PID:(\\d+);WND:(\\d+);$"
			).match(QString::fromLatin1(*response));
			if (const auto pid = match.captured(1).toULongLong()) {
				Platform::ActivateProcessWindow(
					static_cast<int64>(pid),
					static_cast<WId>(match.captured(2).toULongLong()));
			}
			done();
		}
	};
	QObject::connect(&_socket, &QLocalSocket::readyRead, handleRead);
	_socket.write(EncodeMessage(command));
}

void SingleInstance::newInstanceConnected() {
	while (const auto client = _server.nextPendingConnection()) {
		_clients.emplace(client, Message{ ++_lastMessageId });
		QObject::connect(client, &QLocalSocket::readyRead, [=] {
			readClient(client);
		});
		QObject::connect(client, &QLocalSocket::disconnected, [=] {
			removeClient(client);
		});
	}
}

void SingleInstance::readClient(not_null<QLocalSocket*> client) {
	Expects(_clients.contains(client));

	auto &info = _clients[client];
	info.data.append(client->readAll());
	if (const auto message = DecodeMessage(info.data)) {
		_commands.fire({ info.id, *message });
	}
}

void SingleInstance::removeClient(not_null<QLocalSocket*> client) {
	QObject::disconnect(client, &QLocalSocket::readyRead, nullptr, nullptr);
	QObject::disconnect(
		client,
		&QLocalSocket::disconnected,
		nullptr,
		nullptr);
	_clients.remove(client);
}

auto SingleInstance::commands() const -> rpl::producer<Message> {
	return _commands.events();
}

void SingleInstance::reply(uint32 commandId, QWidget *activate) {
	const auto window = activate
		? activate->window()->windowHandle()
		: nullptr;
	const auto wid = window
		? static_cast<uint64>(window->winId())
		: 0ULL;
	const auto pid = wid
		? static_cast<uint64>(QCoreApplication::applicationPid())
		: 0ULL;
	auto response = QString("PID:%1;WND:%2;").arg(pid).arg(wid).toLatin1();
	for (const auto &[client, data] : _clients) {
		if (data.id == commandId) {
			client->write(EncodeMessage(response));
			return;
		}
	}
}

} // namespace base
