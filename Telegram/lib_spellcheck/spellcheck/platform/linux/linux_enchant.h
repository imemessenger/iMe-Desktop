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

#pragma once

#include <string>
#include <vector>
#include <exception>

#ifndef ENCHANT_H
typedef struct str_enchant_broker EnchantBroker;
typedef struct str_enchant_dict   EnchantDict;

/**
 * EnchantBrokerDescribeFn
 * @provider_name: The provider's identifier, such as "ispell" or "aspell" in UTF8 encoding
 * @provider_desc: A description of the provider, such as "Aspell 0.53" in UTF8 encoding
 * @provider_dll_file: The provider's DLL filename in Glib file encoding (UTF8 on Windows)
 * @user_data: Supplied user data, or %null if you don't care
 *
 * Callback used to enumerate and describe Enchant's various providers
 */
typedef void (*EnchantBrokerDescribeFn) (const char * const provider_name,
					 const char * const provider_desc,
					 const char * const provider_dll_file,
					 void * user_data);

/**
 * EnchantDictDescribeFn
 * @lang_tag: The dictionary's language tag (eg: en_US, de_AT, ...)
 * @provider_name: The provider's name (eg: Aspell) in UTF8 encoding
 * @provider_desc: The provider's description (eg: Aspell 0.50.3) in UTF8 encoding
 * @provider_file: The DLL/SO where this dict's provider was loaded from in Glib file encoding (UTF8 on Windows)
 * @user_data: Supplied user data, or %null if you don't care
 *
 * Callback used to describe an individual dictionary
 */
typedef void (*EnchantDictDescribeFn) (const char * const lang_tag,
				       const char * const provider_name,
				       const char * const provider_desc,
				       const char * const provider_file,
				       void * user_data);
#endif  // !ENCHANT_H

namespace enchant
{
	class Broker;

	class Exception : public std::exception
		{
		public:
			explicit Exception (const char * ex);
			virtual ~Exception () noexcept;
			virtual const char * what () const noexcept;

		private:
			std::string m_ex;
		}; // class enchant::Exception

	class Dict
		{
			friend class enchant::Broker;

		public:

			~Dict ();

			bool check (const std::string & utf8word);
			void suggest (const std::string & utf8word,
				      std::vector<std::string> & out_suggestions);

			std::vector<std::string> suggest (const std::string & utf8word) {
				std::vector<std::string> result;
				suggest (utf8word, result);
				return result;
			}

			void add (const std::string & utf8word);
			void add_to_session (const std::string & utf8word);
			bool is_added (const std::string & utf8word);
			void remove (const std::string & utf8word);
			void remove_from_session (const std::string & utf8word);
			bool is_removed (const std::string & utf8word);
			void store_replacement (const std::string & utf8bad,
						const std::string & utf8good);

			const std::string & get_lang () const {
				return m_lang;
			}

			const std::string & get_provider_name () const {
				return m_provider_name;
			}

			const std::string & get_provider_desc () const {
				return m_provider_desc;
			}

			const std::string & get_provider_file () const {
				return m_provider_file;
			}

		private:

			// space reserved for API/ABI expansion
			void * _private[5];

			static void s_describe_fn (const char * const lang,
						   const char * const provider_name,
						   const char * const provider_desc,
						   const char * const provider_file,
						   void * user_data) {
				enchant::Dict * dict = static_cast<enchant::Dict *> (user_data);

				dict->m_lang = lang;
				dict->m_provider_name = provider_name;
				dict->m_provider_desc = provider_desc;
				dict->m_provider_file = provider_file;
			}

			Dict (EnchantDict * dict, EnchantBroker * broker);

			// private, unimplemented
			Dict () = delete;
			Dict (const Dict & rhs) = delete;
			Dict& operator=(const Dict & rhs) = delete;

			EnchantDict * m_dict;
			EnchantBroker * m_broker;

			std::string m_lang;
			std::string m_provider_name;
			std::string m_provider_desc;
			std::string m_provider_file;
		}; // class enchant::Dict

	class Broker
		{

		public:

			Broker ();
			~Broker ();

			Dict * request_dict (const std::string & lang);
			Dict * request_pwl_dict (const std::string & pwl);
			bool dict_exists (const std::string & lang);
			void set_ordering (const std::string & tag, const std::string & ordering);
			void describe (EnchantBrokerDescribeFn fn, void * user_data = nullptr);
			void list_dicts (EnchantDictDescribeFn fn, void * user_data = nullptr);

		private:

			// space reserved for API/ABI expansion
			void * _private[5];

			// not implemented
			Broker (const Broker & rhs) = delete;
			Broker& operator=(const Broker & rhs) = delete;

			EnchantBroker * m_broker;
		}; // class enchant::Broker

	namespace loader {
		bool do_explicit_linking ();
	} // loader subnamespace

} // enchant namespace

// vi: ts=8 sw=8
