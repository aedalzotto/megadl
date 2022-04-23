#pragma once

#include <string>
#include <fstream>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

class MegaDL {
public:
	/**
	 * @brief Constructs the MegaDL object
	 */
	MegaDL();

	/**
	 * @brief Constructs the MegaDL object
	 * 
	 * @details The constructor calls build_context(url)
	 * 
	 * @param url URL of the file to download
	 * 
	 * @throw std::invalid_argument on invalid URL format
	 */
	MegaDL(std::string url);

	/**
	 * @brief Builds the cryptography context and the file ID
	 * 
	 * @param url URL to download from
	 * 
	 * @throw std::invalid_argument on invalid URL format
	 */
	void build_context(std::string url);

	/**
	 * @brief Gets the file ID
	 * 
	 * @details Need to call build_context() or construct with URL first.
	 * 
	 * @throw std::runtime_error on empty ID
	 * 
	 * @return ID of the file extracted from the url
	 */
	std::string get_id();

	/**
	 * @brief Gets the real download URL
	 * 
	 * @details Should call fetch_url() first
	 * 
	 * @throw std::runtime_error on empty URL
	 * 
	 * @return The real download URL fetched from Mega
	 */
	std::string get_url();

	/**
	 * @brief Fetches the real download URL from Mega
	 * 
	 * @details Need to call build_context() or construct with URL first.
	 * 
	 * @throw std::runtime_error on empty ID
	 * 
	 * @return The size of the file to download
	 */
	size_t fetch_url();

	/**
	 * @brief Downloads the file
	 * 
	 * @details Should have called fetch_url() first
	 * 
	 * @throw std::runtime_error on empty URL
	 * 
	 * @param file File object to write to
	 * @param progress_callback Pointer to progress callback function. Nullptr to use standard.
	 */
	void download(
		std::string path, 
		int (*progress_callback)(void *, double, double, double, double)
	);

private:
	CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption aes;
	std::string id;
	std::string url;
	std::ofstream output;

	std::pair<std::string, std::string> decode_url(std::string url);
	std::string decode_id();
	std::string decode_key(std::string key);
};
