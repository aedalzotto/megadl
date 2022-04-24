#include <algorithm>

#include <nlohmann/json.hpp>
#include <cryptopp/filters.h>
#include <mbedtls/base64.h>

#include <curl/curl.h>

#include "MegaDL.hpp"

using json = nlohmann::json;

MegaDL::MegaDL()
{

}

MegaDL::MegaDL(std::string url)
{
	build_context(url);
}

void MegaDL::build_context(std::string url)
{
	/* Get file ID and Key from Mega URL */
	auto id_key = decode_url(url);

	id = id_key.first;

	/* Decode the base64 encoded key */
	std::string node_key = decode_key(id_key.second);

	/* Unpack AES key */
	std::string key(16, 0);
	reinterpret_cast<uint64_t*>(key.data())[0] = 
		reinterpret_cast<uint64_t*>(node_key.data())[0] ^ 
		reinterpret_cast<uint64_t*>(node_key.data())[2]
	;
	reinterpret_cast<uint64_t*>(key.data())[1] = 
		reinterpret_cast<uint64_t*>(node_key.data())[1] ^ 
		reinterpret_cast<uint64_t*>(node_key.data())[3]
	;

	/* Unpack IV */
	std::string iv(16, 0);
	reinterpret_cast<uint64_t*>(iv.data())[0] = 
		reinterpret_cast<uint64_t*>(node_key.data())[2]
	;

	/* Initialize AES context */
	aes.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size(), reinterpret_cast<const CryptoPP::byte*>(iv.data()));
}

std::pair<std::string, std::string> MegaDL::decode_url(std::string url)
{
	/* Check if old link format */
	auto len = url.length();

	if(len < 52)
		throw std::invalid_argument("Invalid URL.");

	bool old_link = url.find("#!") < len;

	/* Start from the last '/' and add 2 characters if old link format starting with '#!' */
	auto init_pos = url.find_last_of('/') + (old_link ? 2 : 0) + 1;

	/* End with the last '#' or "!" if its the old link format */
	auto end_pos = url.find_last_of(old_link ? '!' : '#');

	/* Finally crop the url */
	std::string id = url.substr(init_pos, end_pos - init_pos);

	if(id.length() != 8)
		throw std::invalid_argument("Invalid URL ID.");

	/* Crop the URL to get the file key */
	end_pos++;
	std::string key = url.substr(end_pos, len - end_pos);

	/* Replace URL characters with B64 characters */
	std::replace(key.begin(), key.end(), '_', '/');
	std::replace(key.begin(), key.end(), '-', '+');

	/* Add padding */
	auto key_len = key.length();
	unsigned pad = 4 - key_len%4;
	key.append(pad, '=');

	/* The encoded key should have 44 characters to produce a 32 byte node key */
	if(key.length() != 44)
		throw std::invalid_argument("Invalid URL key.");

	return std::make_pair(id, key);
}

std::string MegaDL::decode_key(std::string key)
{
	/**
	 * Allocate space for n * 6 / 8 bytes when decoding from base64 as each
	 * character in base64 is 6 bits and each character in our string is 8 bits
	 */
	std::string decoded(key.size()*3/4, 0);
	size_t olen = 0;

	mbedtls_base64_decode(
		reinterpret_cast<unsigned char*>(decoded.data()),
		decoded.size(),
		&olen,
		reinterpret_cast<const unsigned char*>(key.c_str()),
		key.size()
	);

	/**
	 * The encoded base64 is (usually?) 43 characters long. With padding it goes
	 * to 44. When we calculate the decoded size, we need to allocate 33 bytes.
	 * But the last encoded character is padding, and combined with the last 
	 * valid character, it should produce a 32 byte node key.
	 */
	decoded.resize(olen);
	if(decoded.size() != 32)
		throw std::invalid_argument("Invalid node key.");

	return decoded;
}

std::string MegaDL::get_id()
{
	if(id.empty())
		throw std::runtime_error("Empty ID. Need to build context first by calling build_context.");

	return id;
}

size_t MegaDL::fetch_url()
{
	json request = json::array({
		{
			{"a", "g"},
			{"g", 1},
			{"p", get_id()},
		}
	});

	std::string body = request.dump();
	std::string output;

	auto curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, "https://g.api.mega.co.nz/cs");
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
	// curl_easy_setopt(curl, CURLOPT_USERAGENT, API_AGENT);
	// curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(
		curl, 
		CURLOPT_WRITEFUNCTION, 
		+[](void *buffer, size_t size, size_t nmemb, void *userp) -> size_t 
		{
			std::string* output = reinterpret_cast<std::string*>(userp);
			size_t actual_size = size * nmemb;
			output->append(reinterpret_cast<char*>(buffer), actual_size);
			return actual_size;
		}
	);

	curl_easy_perform(curl);

	json response = json::parse(output);

	curl_easy_cleanup(curl);

	url = response[0]["g"];
	return response[0]["s"];
}

std::string MegaDL::get_url()
{
	if(url.empty())
		throw std::runtime_error("Empty URL. Need to fetch URL first by calling fetch_url.");

	return url;
}

void MegaDL::download(std::string path, int (*progress_callback)(void *, double, double, double, double))
{
	output.open(path+get_id());
	std::string url = get_url();

	auto curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
	// curl_easy_setopt(curl, CURLOPT_USERAGENT, API_AGENT);
	// curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(
		curl, 
		CURLOPT_WRITEFUNCTION, 
		+[](void *buffer, size_t size, size_t nmemb, void *userp) -> size_t 
		{
			MegaDL* instance = reinterpret_cast<MegaDL*>(userp);

			size_t actual_size = size * nmemb;			

			std::string output;
			CryptoPP::StringSource s(
				reinterpret_cast<CryptoPP::byte*>(buffer), actual_size, true, 
				new CryptoPP::StreamTransformationFilter(
					instance->aes,
					new CryptoPP::StringSink(output)
				)
			);
			
			instance->output << output;

			return actual_size;
		}
	);
	curl_easy_setopt(
		curl, 
		CURLOPT_PROGRESSFUNCTION, 
		progress_callback
	);

	curl_easy_perform(curl);

	output.close();

	curl_easy_cleanup(curl);
}
