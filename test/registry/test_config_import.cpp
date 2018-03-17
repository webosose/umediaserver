#include <iostream>
#include <Registry.h>

int main(int argc, char * argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0]
				  << " {config file path} {db file path}" << std::endl;
		return -1;
	}

	uMediaServer::Reg::Registry registry(argv[2]);
	libconfig::Config config;
	config.readFile(argv[1]);
	registry.apply(config);

	return 0;
}
