#include <algorithm>

#include <nlohmann/json.hpp>
#include <cryptopp/secblock.h>
#include <cryptopp/base64.h>

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
	CryptoPP::SecByteBlock key(CryptoPP::AES::DEFAULT_KEYLENGTH);
	reinterpret_cast<uint64_t*>(key.data())[0] = reinterpret_cast<uint64_t*>(node_key.data())[0] ^ reinterpret_cast<uint64_t*>(node_key.data())[2];
	reinterpret_cast<uint64_t*>(key.data())[1] = reinterpret_cast<uint64_t*>(node_key.data())[1] ^ reinterpret_cast<uint64_t*>(node_key.data())[3];

	/* Unpack IV */
	CryptoPP::SecByteBlock iv(CryptoPP::AES::BLOCKSIZE);
	reinterpret_cast<uint64_t*>(iv.data())[0] = reinterpret_cast<uint64_t*>(node_key.data())[2];
	reinterpret_cast<uint64_t*>(iv.data())[1] = 0;

	/* Initialize AES context */
	aes.SetKeyWithIV(key.data(), key.size(), iv.data());
}

std::pair<std::string, std::string> MegaDL::decode_url(std::string url)
{
	// Check if old link format
	auto len = url.length();

	if(len < 52)
		throw std::invalid_argument("Invalid URL.");

	bool old_link = url.find("#!") < len;

	// Start from the last '/' and add 2 characters if old link format starting with '#!'
	auto init_pos = url.find_last_of('/') + (old_link ? 2 : 0) + 1;

	// End with the last '#' or "!" if its the old link format
	auto end_pos = url.find_last_of(old_link ? '!' : '#');

	// Finally crop the url
	std::string id = url.substr(init_pos, end_pos - init_pos);

	if(id.length() != 8)
		throw std::invalid_argument("Invalid URL ID.");

	// Crop the URL to get the file key
	end_pos++;
	std::string key = url.substr(end_pos, len - end_pos);

	// Replace URL characters with B64 characters
	std::replace(key.begin(), key.end(), '_', '/');
	std::replace(key.begin(), key.end(), '-', '+');

	// Add padding
	auto key_len = key.length();
	unsigned pad = 4 - key_len%4;
	key.append(pad, '=');

	if(key.length() != 44)
		throw std::invalid_argument("Invalid URL key.");

	return std::make_pair(id, key);
}

std::string MegaDL::decode_key(std::string key)
{
	CryptoPP::Base64Decoder decoder;
	decoder.Put(reinterpret_cast<CryptoPP::byte*>(key.data()), key.size());
	decoder.MessageEnd();

	std::string decoded;
	decoded.resize(decoder.MaxRetrievable());
	decoder.Get(reinterpret_cast<CryptoPP::byte*>(decoded.data()), decoded.size());

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
