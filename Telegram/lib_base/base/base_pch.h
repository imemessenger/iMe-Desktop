// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QMutex>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>

#include <crl/crl.h>
#include <rpl/rpl.h>

#include <vector>
#include <unordered_map>
#include <set>

#include <range/v3/all.hpp>

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "base/optional.h"
#include "base/algorithm.h"
#include "base/basic_types.h"
