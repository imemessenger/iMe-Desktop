// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <glibmm/variant.h>

namespace base {
namespace Platform {

template <typename T>
auto MakeGlibVariant(T &&data) {
	return Glib::Variant<T>::create(data);
}

template <typename T>
auto MakeGlibVariant(const T &data) {
	return Glib::Variant<T>::create(data);
}

template <typename T>
auto GlibVariantCast(const Glib::VariantBase &data) {
	return Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(data).get();
}

} // namespace Platform
} // namespace base
