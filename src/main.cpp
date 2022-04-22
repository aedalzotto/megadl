#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include "MegaDL.hpp"

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
	std::string url;
	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help", "produce help message")
			("url", po::value<std::string>(&url)->required(), "url to download from Mega")
		;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if(vm.count("help")){
			std::cout << desc << std::endl;
			return 1;
		}

		po::notify(vm);
	} catch(std::exception& e){
		std::cerr << "Error: " << e.what() << "\n";
        return 1;
	} catch(...){
        std::cerr << "Unknown error!" << "\n";
        return false;
    }
	
	MegaDL file = MegaDL(url);
	std::cout << "ID: " << file.get_id() << std::endl;

	size_t size = file.fetch_url();
	std::cout << "Real URL: " << file.get_url() << std::endl;
	std::cout << "Size: " << size << std::endl;

	std::ofstream fp(file.get_id());

	file.download(
		fp, 
		[&](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool
		{
			std::cout << "Downloaded " << downloadNow << " / " << downloadTotal << " bytes." << std::endl;
			return true;
		}
	);

	return 0;
}
