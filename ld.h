#ifndef TWK_LD_H_
#define TWK_LD_H_

#include <cassert>
#include "core.h"
#include "twk_reader.h"
#include "fisher_math.h"

// Method 0: None
// Method 1: Performance
// Method 2: Debug correctness
// Method 3: Print LD difference Phased and Unphased
#define SLAVE_DEBUG_MODE 3

namespace tomahawk {

#define TWK_LD_REFREF 0
#define TWK_LD_ALTREF 1
#define TWK_LD_REFALT 2
#define TWK_LD_ALTALT 3

// Parameter thresholds for FLAGs
#define LOW_MAF_THRESHOLD       0.01
#define LOW_HWE_THRESHOLD       1e-6
#define LONG_RANGE_THRESHOLD    500e3
#define MINIMUM_ALLOWED_ALLELES 5		// Minimum number of alleles required for math to work out in the unphased case
#define ALLOWED_ROUNDING_ERROR  0.001

#define TWK_HAP_FREQ(A,POS) ((double)(A).alleleCounts[POS] / (A).totalHaplotypeCounts)

static uint32_t twk_debug_pos1 = 0;
static uint32_t twk_debug_pos1_2 = 0;
static uint32_t twk_debug_pos2 = 0;
static uint32_t twk_debug_pos2_2 = 0;

struct temp_t {
	double R, R2, D, Dprime;
	friend std::ostream& operator<<(std::ostream& stream, const temp_t& self){
		stream << twk_debug_pos1 << "\t" << twk_debug_pos2 << "\t" << self.R2 << "\t" << self.R2 << "\t" << self.D << "\t" << self.Dprime << "\n";
		return(stream);
	}
};

static temp_t twk_debug1;
static temp_t twk_debug2;

// SIMD trigger
#if SIMD_AVAILABLE == 1

#ifdef _popcnt64
#define POPCOUNT_ITER	_popcnt64
#else
#define POPCOUNT_ITER	__builtin_popcountll
#endif

#define UNPHASED_UPPER_MASK	170  // 10101010b
#define UNPHASED_LOWER_MASK	85   // 01010101b
#define FILTER_UNPHASED_BYTE(A, B)            ((((((A) & UNPHASED_UPPER_MASK) | ((B) & UNPHASED_LOWER_MASK)) & UNPHASED_LOWER_MASK) << 1) & (A))
#define FILTER_UNPHASED_BYTE_PAIR(A, B, C, D) ((FILTER_UNPHASED_BYTE((A), (B)) >> 1) | FILTER_UNPHASED_BYTE((C), (D)))
#define FILTER_UNPHASED_BYTE_SPECIAL(A)       ((((A) >> 1) & (A)) & UNPHASED_LOWER_MASK)

#if SIMD_VERSION == 6 // AVX-512: UNTESTED
#define VECTOR_TYPE	__m512i
const VECTOR_TYPE ONE_MASK         = _mm512_set1_epi8(255); // 11111111b
const VECTOR_TYPE maskUnphasedHigh = _mm512_set1_epi8(UNPHASED_UPPER_MASK);	// 10101010b
const VECTOR_TYPE maskUnphasedLow  = _mm512_set1_epi8(UNPHASED_LOWER_MASK);	// 01010101b

#define PHASED_ALTALT(A,B)        _mm512_and_si512(A, B)
#define PHASED_REFREF(A,B)        _mm512_and_si512(_mm512_xor_si512(A, ONE_MASK), _mm512_xor_si512(B, ONE_MASK))
#define PHASED_ALTREF(A,B)        _mm512_and_si512(_mm512_xor_si512(A, B), B)
#define PHASED_REFALT(A,B)        _mm512_and_si512(_mm512_xor_si512(A, B), A)
#define PHASED_ALTALT_MASK(A,B,M) _mm512_and_si512(PHASED_ALTALT(A, B), M)
#define PHASED_REFREF_MASK(A,B,M) _mm512_and_si512(PHASED_REFREF(A, B), M)
#define PHASED_ALTREF_MASK(A,B,M) _mm512_and_si512(PHASED_ALTREF(A, B), M)
#define PHASED_REFALT_MASK(A,B,M) _mm512_and_si512(PHASED_REFALT(A, B), M)
#define MASK_MERGE(A,B)           _mm512_xor_si512(_mm512_or_si512(A, B), ONE_MASK)

#define POPCOUNT(A, B) {									\
	__m256i tempA = _mm512_extracti64x4_epi64(B, 0);		\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 0));		\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 1)); 	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 2)); 	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 3)); 	\
	tempA = _mm512_extracti64x4_epi64(B, 1); 				\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 0));		\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 1)); 	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 2)); 	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(tempA, 3)); 	\
}

#define FILTER_UNPHASED(A, B)			 _mm512_and_si512(_mm512_slli_epi64(_mm512_and_si512(_mm512_or_si512(_mm512_and_si512(A, maskUnphasedHigh),_mm512_and_si512(B, maskUnphasedLow)), maskUnphasedLow), 1), A)
#define FILTER_UNPHASED_PAIR(A, B, C, D) _mm512_or_si512(_mm512_srli_epi64(FILTER_UNPHASED(A, B), 1), FILTER_UNPHASED(C, D))
#define FILTER_UNPHASED_SPECIAL(A)		 _mm512_and_si512(_mm512_and_si512(_mm512_srli_epi64(A, 1), A), maskUnphasedLow)


#elif SIMD_VERSION == 5 // AVX2
#define VECTOR_TYPE	__m256i
const VECTOR_TYPE ONE_MASK         = _mm256_set1_epi8(255); // 11111111b
const VECTOR_TYPE maskUnphasedHigh = _mm256_set1_epi8(UNPHASED_UPPER_MASK);	// 10101010b
const VECTOR_TYPE maskUnphasedLow  = _mm256_set1_epi8(UNPHASED_LOWER_MASK);	// 01010101b

#define PHASED_ALTALT(A,B)	        _mm256_and_si256(A, B)
#define PHASED_REFREF(A,B)	        _mm256_and_si256(_mm256_xor_si256(A, ONE_MASK), _mm256_xor_si256(B, ONE_MASK))
#define PHASED_ALTREF(A,B)	        _mm256_and_si256(_mm256_xor_si256(A, B), B)
#define PHASED_REFALT(A,B)	        _mm256_and_si256(_mm256_xor_si256(A, B), A)
#define PHASED_ALTALT_MASK(A,B,M)	_mm256_and_si256(PHASED_ALTALT(A, B), M)
#define PHASED_REFREF_MASK(A,B,M)	_mm256_and_si256(PHASED_REFREF(A, B), M)
#define PHASED_ALTREF_MASK(A,B,M)	_mm256_and_si256(PHASED_ALTREF(A, B), M)
#define PHASED_REFALT_MASK(A,B,M)	_mm256_and_si256(PHASED_REFALT(A, B), M)
#define MASK_MERGE(A,B)		        _mm256_xor_si256(_mm256_or_si256(A, B), ONE_MASK)

// Software intrinsic popcount
#define POPCOUNT(A, B) {							\
	A += POPCOUNT_ITER(_mm256_extract_epi64(B, 0));	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(B, 1));	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(B, 2));	\
	A += POPCOUNT_ITER(_mm256_extract_epi64(B, 3));	\
}

#define FILTER_UNPHASED(A, B)			 _mm256_and_si256(_mm256_slli_epi64(_mm256_and_si256(_mm256_or_si256(_mm256_and_si256(A, maskUnphasedHigh),_mm256_and_si256(B, maskUnphasedLow)), maskUnphasedLow), 1), A)
#define FILTER_UNPHASED_PAIR(A, B, C, D) _mm256_or_si256(_mm256_srli_epi64(FILTER_UNPHASED(A, B), 1), FILTER_UNPHASED(C, D))
#define FILTER_UNPHASED_SPECIAL(A)		 _mm256_and_si256(_mm256_and_si256(_mm256_srli_epi64(A, 1), A), maskUnphasedLow)

#elif SIMD_VERSION >= 2 // SSE2+
#define VECTOR_TYPE	__m128i
const VECTOR_TYPE ONE_MASK         = _mm_set1_epi8(255); // 11111111b
const VECTOR_TYPE maskUnphasedHigh = _mm_set1_epi8(UNPHASED_UPPER_MASK);	// 10101010b
const VECTOR_TYPE maskUnphasedLow  = _mm_set1_epi8(UNPHASED_LOWER_MASK);	// 01010101b

#define PHASED_ALTALT(A,B)	        _mm_and_si128(A, B)
#define PHASED_REFREF(A,B)	        _mm_and_si128(_mm_xor_si128(A, ONE_MASK), _mm_xor_si128(B, ONE_MASK))
#define PHASED_ALTREF(A,B)	        _mm_and_si128(_mm_xor_si128(A, B), B)
#define PHASED_REFALT(A,B)	        _mm_and_si128(_mm_xor_si128(A, B), A)
#define PHASED_ALTALT_MASK(A,B,M)	_mm_and_si128(PHASED_ALTALT(A, B), M)
#define PHASED_REFREF_MASK(A,B,M)	_mm_and_si128(PHASED_REFREF(A, B), M)
#define PHASED_ALTREF_MASK(A,B,M)	_mm_and_si128(PHASED_ALTREF(A, B), M)
#define PHASED_REFALT_MASK(A,B,M)	_mm_and_si128(PHASED_REFALT(A, B), M)
#define MASK_MERGE(A,B)		        _mm_xor_si128(_mm_or_si128(A, B), ONE_MASK)

#if SIMD_VERSION >= 3
#define POPCOUNT(A, B) {								\
	A += POPCOUNT_ITER(_mm_extract_epi64(B, 0));		\
	A += POPCOUNT_ITER(_mm_extract_epi64(B, 1));		\
}
#else
#define POPCOUNT(A, B) { \
	uint64_t temp = _mm_extract_epi16(B, 0) << 6 | _mm_extract_epi16(B, 1) << 4 | _mm_extract_epi16(B, 2) << 2 | _mm_extract_epi16(B, 3); \
	A += POPCOUNT_ITER(temp);		\
	temp = _mm_extract_epi16(B, 4) << 6 | _mm_extract_epi16(B, 5) << 4 | _mm_extract_epi16(B, 6) << 2 | _mm_extract_epi16(B, 7); \
	A += POPCOUNT_ITER(temp);		\
}
#endif

#define FILTER_UNPHASED(A, B)			 _mm_and_si128(_mm_slli_epi64(_mm_and_si128(_mm_or_si128(_mm_and_si128(A, maskUnphasedHigh),_mm_and_si128(B, maskUnphasedLow)), maskUnphasedLow), 1), A)
#define FILTER_UNPHASED_PAIR(A, B, C, D) _mm_or_si128(_mm_srli_epi64(FILTER_UNPHASED(A, B), 1), FILTER_UNPHASED(C, D))
#define FILTER_UNPHASED_SPECIAL(A)		 _mm_and_si128(_mm_and_si128(_mm_srli_epi64(A, 1), A), maskUnphasedLow)
#endif
#endif // ENDIF SIMD_AVAILABLE == 1

struct twk_ld_simd {
public:
	twk_ld_simd(void);
	~twk_ld_simd();

public:
	uint64_t *counters;
	uint8_t  *scalarA, *scalarB, *scalarC, *scalarD;
} __attribute__((aligned(16)));

struct twk_ld_count {
	twk_ld_count(){}
	~twk_ld_count(){}

	void resetPhased(void){
		this->alleleCounts[0]  = 0;
		this->alleleCounts[1]  = 0;
		this->alleleCounts[4]  = 0;
		this->alleleCounts[5]  = 0;
		haplotypeCounts[0] = 0;
		haplotypeCounts[1] = 0;
		haplotypeCounts[2] = 0;
		haplotypeCounts[3] = 0;
		// All other values can legally overflow
		// They are not used
	}

	void resetUnphased(void){
		this->alleleCounts[0]  = 0;
		this->alleleCounts[1]  = 0;
		this->alleleCounts[4]  = 0;
		this->alleleCounts[5]  = 0;
		this->alleleCounts[16] = 0;
		this->alleleCounts[17] = 0;
		this->alleleCounts[20] = 0;
		this->alleleCounts[21] = 0;
		this->alleleCounts[64] = 0;
		this->alleleCounts[65] = 0;
		this->alleleCounts[68] = 0;
		this->alleleCounts[69] = 0;
		this->alleleCounts[80] = 0;
		this->alleleCounts[81] = 0;
		this->alleleCounts[84] = 0;
		this->alleleCounts[85] = 0;
		// All other values can legally overflow
		// They are not used
	}

	// Counters
	double R, R2;             // Correlation coefficients
	double D, Dprime, Dmax;   // D values
	double P;                 // Fisher or Chi-Squared P value for 2x2 contingency table
	double chiSqModel;        // Chi-Squared critical value for 3x3 contingency table
	uint64_t alleleCounts[171];
	uint64_t haplotypeCounts[4];
	uint64_t totalHaplotypeCounts; // Total number of alleles

};

struct ld_perf {
	uint64_t* cycles;
	uint64_t* freq;
};

class LDEngine {
public:
	typedef bool (LDEngine::*func)(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf);
	typedef bool (LDEngine::*ep[10])(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf);

public:
	void SetSamples(const uint32_t samples){
		n_samples  = samples;
		byte_width = ceil((double)samples/4);
		byte_aligned_end = byte_width/(GENOTYPE_TRIP_COUNT/4)*(GENOTYPE_TRIP_COUNT/4);
		vector_cycles    = byte_aligned_end*4/GENOTYPE_TRIP_COUNT;
		phased_unbalanced_adjustment   = (samples*2)%8;
		unphased_unbalanced_adjustment = samples%4;
	}

	// Phased functions
	bool PhasedRunlength(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);
	bool PhasedList(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);
	bool PhasedVectorized(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);
	bool PhasedVectorizedNoMissing(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);
	bool PhasedVectorizedNoMissingNoTable(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);

	/**<
	 * Unphased functions for calculating linkage-disequilibrium. These functions
	 * are all prefixed with Unphased_.
	 * @param b1
	 * @param p1
	 * @param b2
	 * @param p2
	 * @param perf
	 * @return
	 */
	bool UnphasedRunlength(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);
	bool UnphasedVectorized(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);
	bool UnphasedVectorizedNoMissing(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr);

	// Hybrid functions
	__attribute__((always_inline))
	inline bool HybridPhased(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr){
		const uint32_t n_cycles = b1.list[p1].l_list < b2.list[p2].l_list ? b1.list[p1].l_list : b2.list[p2].l_list;
		if(n_cycles < 60)
			return(PhasedList(b1,p1,b2,p2,perf));
		else return(PhasedVectorizedNoMissingNoTable(b1,p1,b2,p2,perf));
	}

	__attribute__((always_inline))
	inline bool HybridUnphased(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr){
		if(b1.blk->rcds[p1].gt->n + b2.blk->rcds[p2].gt->n < 40)
			return(UnphasedRunlength(b1,p1,b2,p2,perf));
		else return(UnphasedVectorizedNoMissing(b1,p1,b2,p2,perf));
	}

	inline bool AllAlgorithms(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2, ld_perf* perf = nullptr){
		//this->PhasedVectorized(b1,p1,b2,p2,perf);
		this->PhasedVectorizedNoMissing(b1,p1,b2,p2,perf);
		//this->PhasedVectorizedNoMissingNoTable(b1,p1,b2,p2,perf);
		//this->UnphasedVectorized(b1,p1,b2,p2,perf);
		this->UnphasedVectorizedNoMissing(b1,p1,b2,p2,perf);
		//this->PhasedRunlength(b1,p1,b2,p2,perf);
		//this->PhasedList(b1,p1,b2,p2,perf);
		//this->UnphasedRunlength(b1,p1,b2,p2,perf);
		return true;
	}

	bool PhasedMath(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2){
		// Trigger phased flag
		//helper.setUsedPhasedMath();

		// Total amount of non-missing alleles
		helper.totalHaplotypeCounts = helper.alleleCounts[0] + helper.alleleCounts[1] + helper.alleleCounts[4] + helper.alleleCounts[5];

		// All values are missing
		//if(helper.totalHaplotypeCounts < MINIMUM_ALLOWED_ALLELES){
			//++this->insufficent_alleles;
		//	return false;
		//}
		//++this->possible;

		// Filter by total minor haplotype frequency
		//if(helper.getTotalAltHaplotypeCount() < this->parameters.minimum_sum_alternative_haplotype_count){
		//	//std::cerr << "FILTER: " << helper.getTotalAltHaplotypeCount() << std::endl;
		//	return false;
		//}


		// Frequencies
		//const double p = (2.0*helper.alleleCounts[0] + helper.alleleCounts[1] + helper.alleleCounts[4]) / (2.0*helper.totalHaplotypeCounts);
		//const double q = (2.0*helper.alleleCounts[5] + helper.alleleCounts[1] + helper.alleleCounts[4]) / (2.0*helper.totalHaplotypeCounts);

		// Haplotype frequencies
		double pA = TWK_HAP_FREQ(helper,0); // pA
		double qA = TWK_HAP_FREQ(helper,1); // qA
		double pB = TWK_HAP_FREQ(helper,4); // pB
		double qB = TWK_HAP_FREQ(helper,5); // qB

		//std::cerr << pA << "," << pB << "," << qA << "," << qB << std::endl;
		//assert(abs((pA+qA+pB+qB)-1) < 0.01);

		if(pA*qB - qA*pB == 0){
			//std::cerr << "is zero return -> " << (pA*qB) << "-" << (qA*pB) << " with " << pA << "," << pB << "," << qA << "," << qB << std::endl;
			//std::cerr << "cnts=" << helper.alleleCounts[0] << "," << helper.alleleCounts[1] << "," << helper.alleleCounts[4] << "," << helper.alleleCounts[5] << std::endl;

			return false;
		}

		// Allelic frequencies
		const double g0 = ((double)helper.alleleCounts[0] + helper.alleleCounts[1]) / (helper.totalHaplotypeCounts);
		const double g1 = ((double)helper.alleleCounts[4] + helper.alleleCounts[5]) / (helper.totalHaplotypeCounts);
		const double h0 = ((double)helper.alleleCounts[0] + helper.alleleCounts[4]) / (helper.totalHaplotypeCounts);
		const double h1 = ((double)helper.alleleCounts[1] + helper.alleleCounts[5]) / (helper.totalHaplotypeCounts);

		helper.D  = pA*qB - qA*pB;
		helper.R2 = helper.D*helper.D / (g0*g1*h0*h1);
		helper.R  = sqrt(helper.R2);

		if(helper.D >= 0) helper.Dmax = g0*h1 < h0*g1 ? g0*h1 : h0*g1;
		else helper.Dmax = g0*g1 < h0*h1 ? -g0*g1 : -h0*h1;

		helper.Dprime = helper.D / helper.Dmax;

		//assert(helper.Dprime >= 0 && helper.Dprime <= 1.01);
		assert(helper.R2 >= 0 && helper.R2 <= 1.01);

		//if(helper.R2 >= 0.5 && helper.R2 <= 1){
#if SLAVE_DEBUG_MODE == 3
		if(helper.R2 > 0.1){
			twk_debug1.D = helper.D;
			twk_debug1.Dprime = helper.Dprime;
			twk_debug1.R = helper.R;
			twk_debug1.R2 = helper.R2;
			twk_debug_pos1 = b1.blk->rcds[p1].pos;
			twk_debug_pos1_2 = b2.blk->rcds[p2].pos;
	//std::cout << "P\t" << b1.blk->rcds[p1].pos << "\t" << b2.blk->rcds[p2].pos << "\t" << helper.D << "\t" << helper.Dprime << "\t" << helper.R << "\t" << helper.R2 << '\n';
		}
#endif

	/*
			std::cerr << "cnts=" << helper.alleleCounts[0] << "," << helper.alleleCounts[1] << "," << helper.alleleCounts[4] << "," << helper.alleleCounts[5] << std::endl;
			std::cerr << "math=" << pA << "," << qA << "," << pB << "," << qA << std::endl;
			std::cerr << "more=" << g0 << "," << g1 << "," << h0 << "," << h1 << std::endl;
			std::cerr << "D=" << helper.D << ",R2=" << helper.R2 << ",R=" << helper.R << ",Dmax=" << helper.Dmax << ",Dprime=" << helper.Dprime << std::endl;
*/

			// Calculate Fisher's exact test P-value
			double left,right,both;
			kt_fisher_exact(helper.alleleCounts[0],
								helper.alleleCounts[1],
								helper.alleleCounts[4],
								helper.alleleCounts[5],
								&left, &right, &both);
			helper.P = both;

			//std::cerr << "P=" << helper.P << std::endl;

			// Fisher's exact test P value filter
			//if(helper.P > this->parameters.P_threshold){
			//	return false;
			//}

			//helper.setCompleteLD(helper[0] == 0 || helper[1] == 0 || helper[4] == 0 || helper[5] == 0);
			//helper.setPerfectLD(helper.R2 > 0.99);

			// Calculate Chi-Sq CV from 2x2 contingency table
			helper.chiSqModel = 0;
			//helper.chiSqFisher = chi_squared(helper[0],helper[1],helper[4],helper[5]);
			//helper.chiSqFisher = helper.totalHaplotypeCounts * helper.R2;
			//helper.chiSqFisher = 0;

			return true;
		//}
		return false;
	}

	bool PhasedMathSimple(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2){
		//++this->possible;

		// If haplotype count for (0,0) >
		//if(helper[0] > 2*n_samples - this->parameters.minimum_sum_alternative_haplotype_count)
		//	return false;

		// D = (joint HOM_HOM) - (HOM_A * HOM_B) = pAB - pApB

		const double af1 = (double)b1.blk->rcds[p1].ac / (2.0f*n_samples);
		const double af2 = (double)b2.blk->rcds[p2].ac / (2.0f*n_samples);
		if(af1 == 0 || af2 == 0) return false;

		//double D, Dprime, R2, R;

		helper.D  = helper.haplotypeCounts[0]/(2*n_samples) - af1*af2;
		helper.R2 = helper.D*helper.D / (af1*(1-af1)*af2*(1-af2));
		helper.R  = sqrt(helper.R2);

		//if(R2 > 0.1){
		if(helper.D >= 0) helper.Dmax = af1*af2 < ((1-af1)*(1-af2)) ? af1*af2 : ((1-af1)*(1-af2));
		else helper.Dmax = (1-af1)*af2 < af1*(1-af2) ? -(1-af1)*af2 : -af1*(1-af2);
		helper.Dprime = helper.D/helper.Dmax;


#if SLAVE_DEBUG_MODE == 3
		if(helper.R2 > 0.1){
			twk_debug1.D = helper.D;
			twk_debug1.Dprime = helper.Dprime;
			twk_debug1.R = helper.R;
			twk_debug1.R2 = helper.R2;
			twk_debug_pos1 = b1.blk->rcds[p1].pos;
			twk_debug_pos1_2 = b2.blk->rcds[p2].pos;
			//std::cout << "F\t" << b1.blk->rcds[p1].pos << "\t" << b2.blk->rcds[p2].pos << "\t" << helper.D << "\t" << helper.Dprime << "\t" << helper.R << "\t" << helper.R2 << '\n';
		}
	#endif

			//std::cerr << "fast-cnts=" << helper.alleleCounts[0] << "," << helper.alleleCounts[1] << "," << helper.alleleCounts[4] << "," << helper.alleleCounts[5] << std::endl;
			//std::cerr << "D=" << helper.D << ",R2=" << helper.R2 << ",R=" << helper.R << ",Dmax=" << helper.Dmax << ",Dprime=" << helper.Dprime << std::endl;


			//std::cerr << "D=" << D << ",Dp=" << Dprime << ",R=" << R << ",R2=" << R2 << std::endl;

			return true;
		//}

		return false;

	}

	bool UnphasedMath(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2);
	double ChiSquaredUnphasedTable(const double& target, const double& p, const double& q);
	bool ChooseF11Calculate(const twk1_ldd_blk& b1, const uint32_t& p1, const twk1_ldd_blk& b2, const uint32_t& p2,const double& target, const double& p, const double& q);

public:
	uint32_t n_samples;
	uint32_t byte_width; // Number of bytes required per variant site
	uint32_t byte_aligned_end; // End byte position
	uint32_t vector_cycles; // Number of SIMD cycles (genotypes/2/vector width)
	uint32_t phased_unbalanced_adjustment; // Modulus remainder
	uint32_t unphased_unbalanced_adjustment; // Modulus remainder

	twk_ld_simd  helper_simd;
	twk_ld_count helper;
};

}


#endif /* LD_H_ */
