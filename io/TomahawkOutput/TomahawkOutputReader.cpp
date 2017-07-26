#include <algorithm>
#include <bitset>
#include <queue>

#include "../../helpers.h"
#include "../../tomahawk/MagicConstants.h"
#include "TomahawkOutputReader.h"
#include "../../algorithm/sort/TomahawkOutputSort.h"

namespace Tomahawk {
namespace IO{
TomahawkOutputReader::TomahawkOutputReader() : position(0), size(0), interval_tree(nullptr), contigs(nullptr), contig_htable(nullptr), writer(nullptr), interval_tree_entries(nullptr)
{}

/*
std::cout << (int)(*this)[i].FLAGS << '\t' << (*this)[i].MAFMix << '\t' << (*this)[i].AcontigID << '\t' << (*this)[i].Aposition << '\t' << (*this)[i].BcontigID << '\t' << (*this)[i].Bposition
							<< '\t' << (*this)[i].p1 << '\t' << (*this)[i].p2 << '\t' << (*this)[i].q1 << '\t' << (*this)[i].q2 << '\t' << (*this)[i].D << '\t' << (*this)[i].Dprime
							<< '\t' << (*this)[i].R2 << '\t' << (*this)[i].P << '\t' << (*this)[i].chiSqFisher << '\t' << (*this)[i].chiSqModel << '\n';
*/


bool TomahawkOutputReader::view(const std::string& input){
	//if(!this->reader.setup(input))
	//	return false;

	return(this->__viewRegion());

	if(this->filter.isAnySet()){
		return(this->__viewFilter());
	} else return(this->__viewOnly());
}

bool TomahawkOutputReader::__viewRegion(void){
	if(this->interval_tree != nullptr){
		const Tomahawk::IO::TomahawkOutputEntry*  entry;
		std::vector<interval_type> rets;
		while(this->nextVariant(entry)){
			// If iTree for contigA exists
			if(this->interval_tree[entry->AcontigID] != nullptr){
				rets = this->interval_tree[entry->AcontigID]->findOverlapping(entry->Aposition, entry->Aposition);
				if(rets.size() > 0){
					for(U32 i = 0; i < rets.size(); ++i){
						if(rets[i].value != nullptr){
							if((entry->BcontigID == rets[i].value->contigID) &&
							   (entry->Bposition >= rets[i].value->start && entry->Bposition <= rets[i].value->stop)){
								std::cerr << "hit linked A" << std::endl;
								entry->write(std::cout, this->contigs);
								break;
							}
						} else {
							entry->write(std::cout, this->contigs);
							break;
						}
					}
					continue;
				}
			}

			// If iTree for contigB exists
			if(this->interval_tree[entry->BcontigID] != nullptr){
				rets = this->interval_tree[entry->BcontigID]->findOverlapping(entry->Bposition, entry->Bposition);
				if(rets.size() > 0){
					for(U32 i = 0; i < rets.size(); ++i){
						if(rets[i].value != nullptr){
							if((entry->AcontigID == rets[i].value->contigID) &&
							   (entry->Aposition >= rets[i].value->start && entry->Aposition <= rets[i].value->stop)){
								std::cerr << "hit linked B" << std::endl;
								entry->write(std::cout, this->contigs);
								break;
							}
						} else {
							entry->write(std::cout, this->contigs);
							break;
						}
					}
					continue;
				}
			}
		}
	}

	return true;
}

bool TomahawkOutputReader::__viewOnly(void){
	std::cerr << "illegal" << std::endl;
	exit(1);

	const entry_type* entry;
	while(this->reader.nextEntry(entry)){
		std::cout << *entry << '\n';
		//std::cout.write(reinterpret_cast<const char*>(entry), sizeof(entry_type));
	}

	return true;
}

bool TomahawkOutputReader::__viewFilter(void){
	const Tomahawk::IO::TomahawkOutputEntry*  entry;
	while(this->nextVariant(entry)){
		if(this->filter.filter(*entry))
//			std::cout << this->contigs[entry->AcontigID].name << '\t' << this->contigs[entry->BcontigID].name << '\t' << *entry << '\n';
			entry->write(std::cout, this->contigs);
	}

	return true;
}

bool TomahawkOutputReader::summary(const std::string& input){
	if(!this->reader.setup(input))
		return false;

	return true;
}

bool TomahawkOutputReader::index(const std::string& input){
	if(!this->reader.setup(input))
		return false;

	const entry_type* entry;
	if(!this->reader.nextEntry(entry))
		return false;

	//const entry_type* previous = entry;
	U32 currentAID = entry->AcontigID;
	U32 currentAPos = entry->Aposition;
	U32 currentBID = entry->BcontigID;
	U64 AIDSteps = 0;
	U64 APosSteps = 0;
	U64 BIDSteps = 0;

	double AposStepsR = 0;

	U64 outputEntries = 0;

	while(this->reader.nextEntry(entry)){
		/*
		if(!(*previous < *entry)){
			std::cerr << "file is not sorted" << std::endl;
			std::cerr << previous->AcontigID << '\t' << previous->Aposition << '\t' << previous->BcontigID << '\t' << previous->Bposition << std::endl;
			std::cerr << entry->AcontigID << '\t' << entry->Aposition << '\t' << entry->BcontigID << '\t' << entry->Bposition << std::endl;
			return false;
		}
		std::swap(entry, previous);
		*/
		++AIDSteps;
		++APosSteps;
		++BIDSteps;
		AposStepsR += entry->R2;

		if(entry->BcontigID != currentBID || entry->Aposition != currentAPos || entry->AcontigID != currentAID){
			//std::cerr << "switch: " << currentAID << ',' << currentAPos << ',' << currentBID << '\t' << entry->AcontigID << ',' << entry->Aposition << ',' << entry->BcontigID << '\t' << BIDSteps << std::endl;
			currentBID = entry->BcontigID;
			BIDSteps = 0;
			++outputEntries;
		}

		if(entry->Aposition != currentAPos || entry->AcontigID != currentAID){
			std::cout << currentAID << '\t' << currentAPos << '\t' << APosSteps << '\t' << AposStepsR/APosSteps << '\n';
			currentAPos = entry->Aposition;
			APosSteps = 0;
			AposStepsR = 0;
			++outputEntries;
		}

		if(entry->AcontigID != currentAID){
			//std::cerr << "switch: " << currentAID << "->" << entry->AcontigID << '\t' << AIDSteps << std::endl;
			currentAID = entry->AcontigID;
			AIDSteps = 0;
			++outputEntries;
		}

	}
	std::cerr << "Index would have: " << outputEntries << " entries..." << std::endl;

	return true;
}


}
} /* namespace Tomahawk */
