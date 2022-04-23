#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include "MegaDL.hpp"

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
	std::string url;
	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "produce help message")
		("url", po::value<std::string>(&url)->required(), "url to download from Mega")
	;

	try {
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);

		if(vm.count("help")){
			std::cout << desc << std::endl;
			return 1;
		}

		po::notify(vm);
	} catch(const std::exception& e){
		std::cerr << "Error: " << e.what() << std::endl;
		std::cout << desc << std::endl;
        return 1;
	} catch(...){
        std::cerr << "Unknown error!" << std::endl;
        return 1;
    }
	
	try {
		MegaDL file(url);
		std::cout << "ID: " << file.get_id() << std::endl;

		size_t size = file.fetch_url();
		std::cout << "Real URL: " << file.get_url() << std::endl;
		std::cout << "Size: " << size << std::endl;

		std::ofstream fp(file.get_id());

		file.download(
			fp, 
			[&](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool
			{
				if(downloadTotal >0)
					std::cout << "\rDownloaded " << downloadNow << " / " << downloadTotal << " bytes.";
				return true;
			}
		);
		
		std::cout << std::endl;
		
	} catch(const std::invalid_argument& e){
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	} catch(const std::runtime_error& e){
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	} catch(const std::exception& e){
		std::cerr << "Error: " << e.what() << std::endl;
        return 1;
	}

	return 0;
}
