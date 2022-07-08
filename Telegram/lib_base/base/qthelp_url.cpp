// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/qthelp_url.h"

namespace qthelp {
namespace {

QRegularExpression CreateRegExp(const QString &expression) {
	auto result = QRegularExpression(
		expression,
		QRegularExpression::UseUnicodePropertiesOption);
	result.optimize();
	return result;
}

QString ExpressionDomain() {
	// Matches any domain name, containing at least one '.', including "file.txt".
	return QString::fromUtf8("(?<![\\w\\$\\-\\_%=\\.])(?:([a-zA-Z]+)://)?((?:[A-Za-z" "\xD0\x90-\xD0\xAF\xD0\x81" "\xD0\xB0-\xD1\x8F\xD1\x91" "0-9\\-\\_]+\\.){1,10}([A-Za-z" "\xD1\x80\xD1\x84" "\\-\\d]{2,22})(\\:\\d+)?)");
}

QString ExpressionDomainExplicit() {
	// Matches any domain name, containing a protocol, including "test://localhost".
	return QString::fromUtf8("(?<![\\w\\$\\-\\_%=\\.])(?:([a-zA-Z]+)://)((?:[A-Za-z" "\xD0\x90-\xD0\xAF\xD0\x81" "\xD0\xB0-\xD1\x8F\xD1\x91" "0-9\\-\\_]+\\.){0,10}([A-Za-z" "\xD1\x80\xD1\x84" "\\-\\d]{2,22})(\\:\\d+)?)");
}

QString ExpressionIpExplicit() {
	//Matches any ip, containing a protocol, including "test://127.0.0.1:1234"	
	return QString::fromUtf8("(?<![\\w\\$\\-\\_%=\\.])(?:([a-zA-Z]+)://)(((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(\\:\\d+)?)");
}

bool IsGoodProtocol(const QString &protocol) {
	const auto equals = [&](QLatin1String string) {
		return protocol.compare(string, Qt::CaseInsensitive) == 0;
	};
	return equals(qstr("http"))
		|| equals(qstr("https"))
		|| equals(qstr("tg"));
}

} // namespace

const QRegularExpression &RegExpDomain() {
	static const auto result = CreateRegExp(ExpressionDomain());
	return result;
}

const QRegularExpression &RegExpDomainExplicit() {
	static const auto result = CreateRegExp(ExpressionDomainExplicit());
	return result;
}

const QRegularExpression &RegExpIpExplicit() {
	static const auto result = CreateRegExp(ExpressionIpExplicit());
	return result;
}

QRegularExpression RegExpProtocol() {
	static const auto result = CreateRegExp("^([a-zA-Z]+)://");
	return result;
}

QMap<QString, QString> url_parse_params(
		const QString &params,
		UrlParamNameTransform transform) {
	auto result = QMap<QString, QString>();

	const auto transformParamName = [transform](const QString &name) {
		if (transform == UrlParamNameTransform::ToLower) {
			return name.toLower();
		}
		return name;
	};
	for (const auto &param : params.split('&')) {
		// Skip params without a name (starting with '=').
		if (auto separatorPosition = param.indexOf('=')) {
			const auto paramName = transformParamName(
				(separatorPosition > 0)
					? param.mid(0, separatorPosition)
					: param);
			const auto paramValue = (separatorPosition > 0)
				? url_decode(param.mid(separatorPosition + 1))
				: QString();
			if (!result.contains(paramName)) {
				result.insert(paramName, paramValue);
			}
		}
	}
	return result;
}

bool is_ipv6(const QString &ip) {
	//static const auto regexp = QRegularExpression("^[a-fA-F0-9:]+$");
	//return regexp.match(ip).hasMatch();
	return ip.indexOf('.') < 0 && ip.indexOf(':') >= 0;
}

QString url_append_query_or_hash(const QString &url, const QString &add) {
	const auto query = url.lastIndexOf('?');
	if (query < 0) {
		return url + '?' + add;
	}
	const auto hash = url.lastIndexOf('#');
	return url
		+ (query >= 0 && query > hash ? '&' : '?')
		+ add;
}

QString validate_url(const QString &value) {
	const auto trimmed = value.trimmed();
	if (trimmed.isEmpty()) {
		return QString();
	}
	const auto domainMatch = RegExpDomainExplicit().match(trimmed);
	const auto ipMatch = RegExpIpExplicit().match(trimmed);
	if (!domainMatch.hasMatch() && !ipMatch.hasMatch()) {
		const auto domain = RegExpDomain().match(trimmed);
		if (!domain.hasMatch() || domain.capturedStart() != 0) {
			return QString();
		}
		return qstr("http://") + trimmed;
	} else if (domainMatch.capturedStart() != 0 && ipMatch.capturedStart() != 0) {
		return QString();
	}
	const auto protocolMatch = RegExpProtocol().match(trimmed);
	Assert(protocolMatch.hasMatch());
	return IsGoodProtocol(protocolMatch.captured(1)) ? trimmed : QString();
}

} // namespace qthelp
