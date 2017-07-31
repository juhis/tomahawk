#include "utility.h"
#include "io/vcf/VCFParser.h"

void import_usage(void){
	programMessage();
	std::cout <<
	"Usage: " << Tomahawk::Constants::PROGRAM_NAME << " import [options] -i <in.vcf|in.bcf> -o <out_prefix> \n"
	"\n"
	"Options:\n"
	"  -i FILE  input Tomahawk (required)\n"
	"  -o FILE  output file prefix (required)\n"
	"  -h FLOAT Hardy-Weinberg P-value cutoff\n"
	"  -m FLOAT Minor-allele frequency (MAF) cutoff\n"
	"  -M INT   Minor-allele count cutoff\n"
	"  -n FLOAT Missingness percentage cutoff (default: 0.2)\n"
	"  -s       Hide all program messages [null]\n";
}

int import(int argc, char** argv){
	int c;
	if(argc < 3){
		import_usage();
		return(1);
	}

	int option_index = 0;
	static struct option long_options[] = {
		{"input",		required_argument, 0,  'i' },
		{"output",		required_argument, 0,  'o' },
		{"silent",		no_argument, 0,  's' },
		{0,0,0,0}
	};

	std::string input;
	std::string output;
	SILENT = 0;

	while ((c = getopt_long(argc, argv, "i:o:s?", long_options, &option_index)) != -1){
		switch (c){
		case 0:
			std::cerr << "Case 0: " << option_index << '\t' << long_options[option_index].name << std::endl;
			break;
		case 'i':
			input = std::string(optarg);
			break;
		case 'o':
			output = std::string(optarg);
			break;

	  case 's':
		  SILENT = 1;
		  break;

	  default:
		  std::cout << Tomahawk::Helpers::timestamp("ERROR") << "Unrecognized option: " << (char)c << std::endl;
		  return(1);
		}
	}

	if(input.length() == 0){
		std::cout << Tomahawk::Helpers::timestamp("ERROR") << "No input value specified..." << std::endl;
		return(1);
	}

	if(output.length() == 0){
		std::cout << Tomahawk::Helpers::timestamp("ERROR") << "No output value specified..." << std::endl;
		return(1);
	}

	// Print messages
	if(!SILENT){
		programMessage();
		std::cerr << Tomahawk::Helpers::timestamp("LOG") << "Calling import..." << std::endl;
	}

	Tomahawk::VCF::VCFParser importer(input, output);
	if(!importer.Build()){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR") << "Failed build!" << std::endl;
		return 1;
	}

	return 0;
}
