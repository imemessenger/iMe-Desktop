/* enchant
 * Copyright (C) 2003 Dom Lachowicz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * In addition, as a special exception, Dom Lachowicz
 * gives permission to link the code of this program with
 * non-LGPL Spelling Provider libraries (eg: a MSFT Office
 * spell checker backend) and distribute linked combinations including
 * the two.  You must obey the GNU Lesser General Public License in all
 * respects for all of the code used other than said providers.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 *
 * Nicholas Guriev (email: guriev-ns@ya.ru) split the full <enchant++.h> header
 * into two files, linux_enchant.h and linux_enchant.cpp, to use within Desktop
 * App Toolkit. He also implemented explicit linking with dlopen/dlsym to avoid
 * rigid dependency on the Enchant library at runtime.
 */

#include <enchant.h>
#include <dlfcn.h>
#include "spellcheck/platform/linux/linux_enchant.h"

namespace {

struct {
	//decltype (enchant_broker_describe) * broker_describe;
	//decltype (enchant_broker_dict_exists) * broker_dict_exists;
	decltype (enchant_broker_free) * broker_free;
	decltype (enchant_broker_free_dict) * broker_free_dict;
	decltype (enchant_broker_get_error) * broker_get_error;
	decltype (enchant_broker_init) * broker_init;
	decltype (enchant_broker_list_dicts) * broker_list_dicts;
	decltype (enchant_broker_request_dict) * broker_request_dict;
	//decltype (enchant_broker_request_pwl_dict) * broker_request_pwl_dict;
	decltype (enchant_broker_set_ordering) * broker_set_ordering;
	decltype (enchant_dict_add) * dict_add;
	decltype (enchant_dict_add_to_session) * dict_add_to_session;
	decltype (enchant_dict_check) * dict_check;
	decltype (enchant_dict_describe) * dict_describe;
	decltype (enchant_dict_free_string_list) * dict_free_string_list;
	decltype (enchant_dict_get_error) * dict_get_error;
	decltype (enchant_dict_is_added) * dict_is_added;
	//decltype (enchant_dict_is_removed) * dict_is_removed;
	decltype (enchant_dict_remove) * dict_remove;
	decltype (enchant_dict_remove_from_session) * dict_remove_from_session;
	//decltype (enchant_dict_store_replacement) * dict_store_replacement;
	decltype (enchant_dict_suggest) * dict_suggest;
} f_enchant;

} // anonymous namespace

enchant::Exception::Exception (const char * ex)
	: std::exception (), m_ex ("") {
	if (ex)
		m_ex = ex;
}

enchant::Exception::~Exception () = default;

const char * enchant::Exception::what () const noexcept {
	return m_ex.c_str();
}

enchant::Dict::Dict (EnchantDict * dict, EnchantBroker * broker)
	: m_dict (dict), m_broker (broker) {
	f_enchant.dict_describe (m_dict, s_describe_fn, this);
}

enchant::Dict::~Dict () {
	f_enchant.broker_free_dict (m_broker, m_dict);
}

bool enchant::Dict::check (const std::string & utf8word) {
	int val;

	val = f_enchant.dict_check (m_dict, utf8word.c_str(), utf8word.size());
	if (val == 0)
		return true;
	else if (val > 0)
		return false;
	else {
		throw enchant::Exception (f_enchant.dict_get_error (m_dict));
	}

	return false; // never reached
}

void enchant::Dict::suggest (const std::string & utf8word,
			     std::vector<std::string> & out_suggestions) {
	size_t n_suggs;
	char ** suggs;

	out_suggestions.clear ();

	suggs = f_enchant.dict_suggest (m_dict, utf8word.c_str(),
					utf8word.size(), &n_suggs);

	if (suggs && n_suggs) {
		out_suggestions.reserve(n_suggs);

		for (size_t i = 0; i < n_suggs; i++) {
			out_suggestions.push_back (suggs[i]);
		}

		f_enchant.dict_free_string_list (m_dict, suggs);
	}
}

void enchant::Dict::add (const std::string & utf8word) {
	f_enchant.dict_add (m_dict, utf8word.c_str(), utf8word.size());
}

void enchant::Dict::add_to_session (const std::string & utf8word) {
	f_enchant.dict_add_to_session (m_dict, utf8word.c_str(), utf8word.size());
}

bool enchant::Dict::is_added (const std::string & utf8word) {
	return f_enchant.dict_is_added (m_dict, utf8word.c_str(),
					utf8word.size());
}

void enchant::Dict::remove (const std::string & utf8word) {
	f_enchant.dict_remove (m_dict, utf8word.c_str(), utf8word.size());
}

void enchant::Dict::remove_from_session (const std::string & utf8word) {
	f_enchant.dict_remove_from_session (m_dict, utf8word.c_str(),
					    utf8word.size());
}

//bool enchant::Dict::is_removed (const std::string & utf8word) {
//	return f_enchant.dict_is_removed (m_dict, utf8word.c_str(),
//					  utf8word.size());
//}

//void enchant::Dict::store_replacement (const std::string & utf8bad,
//				       const std::string & utf8good) {
//	f_enchant.dict_store_replacement (m_dict,
//					  utf8bad.c_str(), utf8bad.size(),
//					  utf8good.c_str(), utf8good.size());
//}

enchant::Broker::Broker ()
	: m_broker (f_enchant.broker_init ())
	{
	}

enchant::Broker::~Broker () {
	f_enchant.broker_free (m_broker);
}

enchant::Dict * enchant::Broker::request_dict (const std::string & lang) {
	EnchantDict * dict = f_enchant.broker_request_dict (m_broker, lang.c_str());

	if (!dict) {
		throw enchant::Exception (f_enchant.broker_get_error (m_broker));
		return 0; // never reached
	}

	return new Dict (dict, m_broker);
}

//enchant::Dict * enchant::Broker::request_pwl_dict (const std::string & pwl) {
//	EnchantDict * dict = f_enchant.broker_request_pwl_dict (m_broker, pwl.c_str());
//
//	if (!dict) {
//		throw enchant::Exception (f_enchant.broker_get_error (m_broker));
//		return 0; // never reached
//	}
//
//	return new Dict (dict, m_broker);
//}

//bool enchant::Broker::dict_exists (const std::string & lang) {
//	if (f_enchant.broker_dict_exists (m_broker, lang.c_str()))
//		return true;
//	return false;
//}

void enchant::Broker::set_ordering (const std::string & tag, const std::string & ordering) {
	f_enchant.broker_set_ordering (m_broker, tag.c_str(), ordering.c_str());
}

//void enchant::Broker::describe (EnchantBrokerDescribeFn fn, void * user_data) {
//	f_enchant.broker_describe (m_broker, fn, user_data);
//}

void enchant::Broker::list_dicts (EnchantDictDescribeFn fn, void * user_data) {
	f_enchant.broker_list_dicts (m_broker, fn, user_data);
}

#define GET_SYMBOL_enchant(func_name) do { \
	typedef decltype (enchant_ ## func_name) * Fp; \
	f_enchant.func_name = reinterpret_cast<Fp> ( dlsym (handle, "enchant_" # func_name)); \
	if (!f_enchant.func_name) { \
		return false; \
	} \
} while(0)

bool enchant::loader::do_explicit_linking () {
	static enum { NotLoadedYet, LoadSuccessful, LoadFailed = -1 } load_status;
	if (load_status == NotLoadedYet) {
		load_status = LoadFailed;
		void * handle = dlopen ("libenchant.so.1", RTLD_NOW)
				?: dlopen ("libenchant-2.so.2", RTLD_NOW)
				?: dlopen ("libenchant.so.2", RTLD_NOW);
		if (!handle) {
			// logs ?
			return false;
		}
		//GET_SYMBOL_enchant (broker_describe);
		//GET_SYMBOL_enchant (broker_dict_exists);
		GET_SYMBOL_enchant (broker_free);
		GET_SYMBOL_enchant (broker_free_dict);
		GET_SYMBOL_enchant (broker_get_error);
		GET_SYMBOL_enchant (broker_init);
		GET_SYMBOL_enchant (broker_list_dicts);
		GET_SYMBOL_enchant (broker_request_dict);
		//GET_SYMBOL_enchant (broker_request_pwl_dict);
		GET_SYMBOL_enchant (broker_set_ordering);
		GET_SYMBOL_enchant (dict_add);
		GET_SYMBOL_enchant (dict_add_to_session);
		GET_SYMBOL_enchant (dict_check);
		GET_SYMBOL_enchant (dict_describe);
		GET_SYMBOL_enchant (dict_free_string_list);
		GET_SYMBOL_enchant (dict_get_error);
		GET_SYMBOL_enchant (dict_is_added);
		//GET_SYMBOL_enchant (dict_is_removed);
		GET_SYMBOL_enchant (dict_remove);
		GET_SYMBOL_enchant (dict_remove_from_session);
		//GET_SYMBOL_enchant (dict_store_replacement);
		GET_SYMBOL_enchant (dict_suggest);
		load_status = LoadSuccessful;
	}
	return load_status == LoadSuccessful;
}

// vi: ts=8 sw=8
