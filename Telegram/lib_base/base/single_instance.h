// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/file_lock.h"

#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

namespace base {

class SingleInstance final {
public:
	struct Message {
		uint32 id = 0;
		QByteArray data;
	};

	SingleInstance();
	~SingleInstance();

	void start(
		const QString &uniqueApplicationName,
		const QString &path,
		Fn<void()> primary,
		Fn<void()> secondary,
		Fn<void()> fail);

	void send(const QByteArray &command, Fn<void()> done);
	[[nodiscard]] rpl::producer<Message> commands() const;
	void reply(uint32 commandId, QWidget *activate = nullptr);

private:
	void clearSocket();
	void clearLock();
	[[nodiscard]] bool closeExisting();

	void newInstanceConnected();
	void readClient(not_null<QLocalSocket*> client);
	void removeClient(not_null<QLocalSocket*> client);

	QString _name;
	QLocalServer _server;
	QLocalSocket _socket;
	uint32 _lastMessageId = 0;
	base::flat_map<not_null<QLocalSocket*>, Message> _clients;
	rpl::event_stream<Message> _commands;

	QFile _lockFile;
	FileLock _lock;

};

} // namespace base
