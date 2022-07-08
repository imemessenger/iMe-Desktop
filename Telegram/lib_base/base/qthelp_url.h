// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtCore/QRegularExpression>

namespace qthelp {

const QRegularExpression &RegExpDomain();
const QRegularExpression &RegExpDomainExplicit();
QRegularExpression RegExpProtocol();

inline QString url_encode(const QString &part) {
	return QString::fromLatin1(QUrl::toPercentEncoding(part));
}

inline QString url_decode(QString encoded) {
	return QUrl::fromPercentEncoding(encoded.replace('+', ' ').toUtf8());
}

enum class UrlParamNameTransform {
	NoTransform,
	ToLower,
};
// Parses a string like "p1=v1&p2=v2&..&pn=vn" to a map.
QMap<QString, QString> url_parse_params(
	const QString &params,
	UrlParamNameTransform transform = UrlParamNameTransform::NoTransform);

QString url_append_query_or_hash(const QString &url, const QString &add);

bool is_ipv6(const QString &ip);

QString validate_url(const QString &value);

} // namespace qthelp
