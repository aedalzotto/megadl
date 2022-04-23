#include <algorithm>

#include <nlohmann/json.hpp>
#include <cryptopp/secblock.h>
#include <cryptopp/base64.h>

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

	cpr::Response r = cpr::Post(
		cpr::Url{"https://g.api.mega.co.nz/cs"},
        cpr::Body{request.dump()},
		cpr::Header{{"Content-Type", "text/plain"}}
	);

	json response = json::parse(r.text);

	url = response[0]["g"];
	return response[0]["s"];
}

std::string MegaDL::get_url()
{
	if(url.empty())
		throw std::runtime_error("Empty URL. Need to fetch URL first by calling fetch_url.");

	return url;
}

void MegaDL::download(std::ofstream &file, std::function<bool(cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata)> p_callback)
{
	cpr::Get(
		cpr::Url{get_url()},
		cpr::WriteCallback(
			[&](std::string data, intptr_t userdata) -> bool
			{
				std::string output;

				CryptoPP::StringSource s(
					data, true, 
					new CryptoPP::StreamTransformationFilter(
						aes,
						new CryptoPP::StringSink(output)
					)
				);

				file << output;

				return true;
			}
		),
		cpr::ProgressCallback(p_callback)
	);
}
