// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/options.h"

#include "base/call_delayed.h"
#include "base/variant.h"
#include "base/debug_log.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QFile>

namespace base::options {
namespace details {
namespace {

constexpr auto kSaveDelay = crl::time(1000);

bool WriteScheduled/* = false*/;

struct Compare {
	bool operator()(const char *a, const char *b) const noexcept {
		return strcmp(a, b) < 0;
	}
};
using MapType = base::flat_map<const char*, not_null<BasicOption*>, Compare>;

[[nodiscard]] MapType &Map() {
	static auto result = MapType();
	return result;
}

[[nodiscard]] QString &LocalPath() {
	static auto result = QString();
	return result;
}

void Read(const QString &path) {
	auto file = QFile(path);
	if (!file.exists()) {
		return;
	} else if (!file.open(QIODevice::ReadOnly)) {
		LOG(("Experimental: Error opening file from '%1'.").arg(path));
		return;
	}
	auto error = QJsonParseError();
	const auto parsed = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError) {
		LOG(("Experimental: Error parsing json from '%1': %2 (%3)"
			).arg(path
			).arg(error.error
			).arg(error.errorString()));
		return;
	} else if (!parsed.isObject()) {
		LOG(("Experimental: Non object in json from '%1'.").arg(path));
		return;
	}
	auto &map = Map();
	const auto values = parsed.object();
	for (auto i = values.begin(); i != values.end(); ++i) {
		const auto key = i.key().toLatin1() + char(0);
		const auto j = map.find(key.data());
		if (j == end(map)) {
			LOG(("Experimental: Unknown option '%1'.").arg(i.key()));
			continue;
		}
		const auto value = *i;
		v::match(j->second->value(), [&](const auto &current) {
			using T = std::remove_cvref_t<decltype(current)>;
			if constexpr (std::is_same_v<T, bool>) {
				if (value.isBool()) {
					j->second->set(value.toBool());
					return;
				}
			} else if constexpr (std::is_same_v<T, int>) {
				if (value.isDouble()) {
					j->second->set(value.toInt());
					return;
				}
			} else if constexpr (std::is_same_v<T, QString>) {
				if (value.isString()) {
					j->second->set(value.toString());
					return;
				}
			} else {
				static_assert(unsupported_type(T()));
			}
			LOG(("Experimental: Wrong option value type for '%1'."
				).arg(i.key()));
		});
	}
}

void Write() {
	const auto &path = LocalPath();
	if (!WriteScheduled || path.isEmpty()) {
		return;
	}
	WriteScheduled = false;

	auto map = QJsonObject();
	for (const auto &[name, option] : Map()) {
		const auto &value = option->value();
		if (value != option->defaultValue()) {
			map.insert(name, v::match(value, [](const auto &current) {
				using T = std::remove_cvref_t<decltype(current)>;
				if constexpr (std::is_same_v<T, bool>
					|| std::is_same_v<T, int>
					|| std::is_same_v<T, QString>) {
					return QJsonValue(current);
				} else {
					static_assert(unsupported_type(T()));
				}
			}));
		}
	}
	if (map.isEmpty()) {
		QFile(path).remove();
	} else if (auto file = QFile(path); file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument(map).toJson(QJsonDocument::Indented));
	} else {
		LOG(("Experimental: Could not write '%1'.").arg(path));
	}
}

} // namespace

BasicOption::BasicOption(
	const char id[],
	const char name[],
	const char description[],
	ValueType defaultValue,
	Scope scope,
	bool restartRequired)
: _value(defaultValue)
, _defaultValue(std::move(defaultValue))
, _id(QString::fromUtf8(id))
, _name(QString::fromUtf8(name))
, _description(QString::fromUtf8(description))
, _scope(scope)
, _restartRequired(restartRequired) {
	const auto [i, ok] = Map().emplace(id, this);

	Ensures(ok);
}

void BasicOption::set(ValueType value) {
	Expects(value.index() == _value.index());

	_value = std::move(value);
	if (!WriteScheduled && !LocalPath().isEmpty()) {
		WriteScheduled = true;
		call_delayed(kSaveDelay, [] { Write(); });
	}
}

const ValueType &BasicOption::value() const {
	return _value;
}

const ValueType &BasicOption::defaultValue() const {
	return _defaultValue;
}

const QString &BasicOption::id() const {
	return _id;
}

const QString &BasicOption::name() const {
	return _name;
}

const QString &BasicOption::description() const {
	return _description;
}

bool BasicOption::relevant() const {
#ifdef Q_OS_WIN
	return _scope & windows;
#elif defined Q_OS_MAC // Q_OS_WIN
	return _scope & macos;
#else // Q_OS_MAC || Q_OS_WIN
	return _scope & linux;
#endif // Q_OS_MAC || Q_OS_WIN
}

bool BasicOption::restartRequired() const {
	return _restartRequired;
}

Scope BasicOption::scope() const {
	return _scope;
}

BasicOption &Lookup(const char id[]) {
	const auto i = Map().find(id);

	Ensures(i != end(Map()));
	return *i->second;
}

} // namespace details

bool changed() {
	for (const auto &[name, option] : details::Map()) {
		if (option->value() != option->defaultValue()) {
			return true;
		}
	}
	return false;
}

void reset() {
	for (const auto &[name, option] : details::Map()) {
		if (option->value() != option->defaultValue()) {
			option->set(option->defaultValue());
		}
	}
}

void init(const QString &path) {
	Expects(details::LocalPath().isEmpty());

	if (!path.isEmpty()) {
		details::Read(path);

		details::LocalPath() = path;
		static const auto guard = gsl::finally([] {
			details::Write();
		});
	}
}

} // namespace base::options
