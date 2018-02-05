#ifndef TOMAHAWK_TOMAHAWKSUPPORT_H_
#define TOMAHAWK_TOMAHAWKSUPPORT_H_

namespace Tomahawk{
namespace Support{

#pragma pack(push, 1)
template <class T>
struct __attribute__((packed, aligned(1))) TomahawkRun{
public:
	TomahawkRun(){}
	TomahawkRun(const char* const buffer){
		T* t = reinterpret_cast<T*>(this->alleleA);
		*t   = *reinterpret_cast<T*>(buffer);
	}
	~TomahawkRun(){}

	T alleleA: Constants::TOMAHAWK_ALLELE_PACK_WIDTH,
	  alleleB: Constants::TOMAHAWK_ALLELE_PACK_WIDTH,
	  runs:    sizeof(T)*8 - Constants::TOMAHAWK_SNP_PACK_WIDTH;
};


template <class T>
struct __attribute__((packed, aligned(1))) TomahawkRunPacked{
public:
	TomahawkRunPacked(){}
	TomahawkRunPacked(const char* const buffer){
		T* t = reinterpret_cast<T*>(this->alleles);
		*t   = *reinterpret_cast<T*>(buffer);
	}
	~TomahawkRunPacked(){}

	T alleles: Constants::TOMAHAWK_SNP_PACK_WIDTH,
	  runs:    sizeof(T)*8 - Constants::TOMAHAWK_SNP_PACK_WIDTH;
};

#pragma pack(pop)

} // end support

namespace Constants{

// Dummy variables used for Twk > VCF output
const std::string PASS("PASS");
const std::string GT("GT");
const std::string QUAL("100");

}

}



#endif /* TOMAHAWK_TOMAHAWKSUPPORT_H_ */
