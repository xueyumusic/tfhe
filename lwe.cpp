#include <cstdlib>
#include <iostream>
#include <random>
#include <cassert>
#include "lwe.h"
#include "lweparams.h"
#include "lwekey.h"
#include "lwesamples.h"
#include "ringlwe.h"
#include "ringgsw.h"
#include "lwekeyswitch.h"
#include "lwebootstrappingkey.h"

using namespace std;

static default_random_engine generator;
static const int64_t _two31 = INT64_C(1) << 31; // 2^31
static const int64_t _two32 = INT64_C(1) << 32; // 2^32
static const double _two32_double = _two32;
static const double _two31_double = _two31;

static uniform_int_distribution<Torus32> uniformTorus32_distrib(INT32_MIN, INT32_MAX);

// from double to Torus32
EXPORT Torus32 dtot32(double d) {
    return int32_t(int64_t((d - int64_t(d))*_two32));
}
// from Torus32 to double
EXPORT double t32tod(Torus32 x) {
    return double(x)/_two32_double;
}





// TorusPolynomial = 0
EXPORT void ClearTorusPolynomial(TorusPolynomial* result) {
    const int N = result->N;

    for (int i = 0; i < N; ++i) result->coefsT[i] = 0;
}

// TorusPolynomial = random
EXPORT void UniformTorusPolynomial(TorusPolynomial* result) {
    const int N = result->N;
    Torus32* x = result->coefsT;

    for (int i = 0; i < N; ++i) 
	x[i] = uniformTorus32_distrib(generator);
}

// TorusPolynomial = TorusPolynomial
EXPORT void CopyTorusPolynomial(
	TorusPolynomial* __restrict result, 
	const TorusPolynomial* __restrict sample) {
    assert(result!=sample);
    const int N = result->N;

    for (int i = 0; i < N; ++i) result->coefsT[i] = sample->coefsT[i];
}

// TorusPolynomial + TorusPolynomial
EXPORT void AddTorusPolynomial(TorusPolynomial* result, const TorusPolynomial* poly1, const TorusPolynomial* poly2) {
    const int N = poly1->N;
    Torus32* r = result->coefsT;
    const Torus32* a = poly1->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) 
	r[i] = a[i] + b[i];
}

// TorusPolynomial += TorusPolynomial
EXPORT void AddToTorusPolynomial(TorusPolynomial* result, const TorusPolynomial* poly2) {
    const int N = poly2->N;
    Torus32* r = result->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) 
	r[i] += b[i];
}


// TorusPolynomial - TorusPolynomial
EXPORT void SubTorusPolynomial(TorusPolynomial* result, const TorusPolynomial* poly1, const TorusPolynomial* poly2) {
    const int N = poly1->N;
    Torus32* r = result->coefsT;
    const Torus32* a = poly1->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) 
	r[i] = a[i] - b[i];
}

// TorusPolynomial -= TorusPolynomial
EXPORT void SubToTorusPolynomial(TorusPolynomial* result, const TorusPolynomial* poly2) {
    const int N = poly2->N;
    Torus32* r = result->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) 
	r[i] -= b[i];
}

// TorusPolynomial + p*TorusPolynomial
EXPORT void AddMulZTorusPolynomial(TorusPolynomial* result, const TorusPolynomial* poly1, int p, const TorusPolynomial* poly2) {
    const int N = poly1->N;
    Torus32* r = result->coefsT;
    const Torus32* a = poly1->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) 
	r[i] = a[i] + p*b[i];
}

// TorusPolynomial += p*TorusPolynomial
EXPORT void AddMulZToTorusPolynomial(TorusPolynomial* result, const int p, const TorusPolynomial* poly2) {
    const int N = poly2->N;
    Torus32* r = result->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) r[i] += p*b[i];
}

// TorusPolynomial - p*TorusPolynomial
EXPORT void SubMulZTorusPolynomial(TorusPolynomial* result, const TorusPolynomial* poly1, const int p, const TorusPolynomial* poly2) {
    int N = poly1->N;
    Torus32* r = result->coefsT;
    const Torus32* a = poly1->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) r[i] = a[i] - p*b[i];
}

// TorusPolynomial -= p*TorusPolynomial
EXPORT void SubMulZToTorusPolynomial(TorusPolynomial* result, int p, const TorusPolynomial* poly2) {
    int N = poly2->N;
    Torus32* r = result->coefsT;
    const Torus32* b = poly2->coefsT;

    for (int i = 0; i < N; ++i) r[i] += p*b[i];
}




// Norme Euclidienne d'un IntPolynomial
EXPORT int NormEuclidIntPolynomial(const IntPolynomial* poly){
    const int N = poly->N;
    int temp1 = 0;

    for (int i = 0; i < N; ++i)
    {
        int temp0 = poly->coefs[i]*poly->coefs[i];
        temp1 += temp0;
    }
    return temp1;
}




// Gaussian sample centered in message, with standard deviation sigma
Torus32 gaussian32(Torus32 message, double sigma){
    //Attention: all the implementation will use the stdev instead of the gaussian fourier param
    normal_distribution<double> distribution(0.,sigma); //TODO: can we create a global distrib of param 1 and multiply by sigma?
    double err = distribution(generator);
    return message + dtot32(err);
}


// Used to approximate the phase to the nearest message possible in the message space
// The constant Msize will indicate on which message space we are working (how many messages possible)
Torus32 approxPhase(Torus32 phase, int Msize){
    uint64_t interv = UINT64_C(-1)/Msize; // width of each intervall
    uint64_t half_interval = interv/2; // begin of the first intervall
    uint64_t phase64 = (uint64_t(phase)<<32) + half_interval;
    //floor to the nearest multiples of interv
    phase64 -= phase64%interv;
    //rescale to torus32
    return int32_t(phase64>>32); 
}







/**
 * This function generates a random LWE key for the given parameters.
 * The LWE key for the result must be allocated and initialized
 * (this means that the parameters are already in the result)
 */
EXPORT void lweKeyGen(LWEKey* result) {
  const int n = result->params->n;
  uniform_int_distribution<int> distribution(0,1);

  for (int i=0; i<n; i++) 
    result->key[i]=distribution(generator);
}



/**
 * This function encrypts message by using key, with stdev alpha
 * The LWE sample for the result must be allocated and initialized
 * (this means that the parameters are already in the result)
 */
EXPORT void lweSymEncrypt(LWESample* result, Torus32 message, double alpha, const LWEKey* key){
    const int n = key->params->n;

    result->b = gaussian32(message, alpha); 
    for (int i = 0; i < n; ++i)
    {
        result->a[i] = uniformTorus32_distrib(generator);
        result->b += result->a[i]*key->key[i];
    }

    result->current_variance = alpha*alpha;
}



/**
 * This function computes the phase of sample by using key : phi = b - a.s
 */
EXPORT Torus32 lwePhase(const LWESample* sample, const LWEKey* key){
    const int n = key->params->n;
    Torus32 axs = 0;
    const Torus32 *__restrict a = sample->a;
    const int * __restrict k = key->key;

    for (int i = 0; i < n; ++i) 
	   axs += a[i]*k[i]; 
    return sample->b - axs;
}


/**
 * This function computes the decryption of sample by using key
 * The constant Msize indicates the message space and is used to approximate the phase
 */
EXPORT Torus32 lweSymDecrypt(const LWESample* sample, const LWEKey* key, const int Msize){
    Torus32 phi;

    phi = lwePhase(sample, key);
    return approxPhase(phi, Msize);
}




//Arithmetic operations on LWE samples
/** result = (0,0) */
EXPORT void lweClear(LWESample* result, const LWEParams* params){
    const int n = params->n;

    for (int i = 0; i < n; ++i) result->a[i] = 0;
    result->b = 0;
    result->current_variance = 0.;
}

/** result = (0,mu) */
EXPORT void lweNoiselessTrivial(LWESample* result, Torus32 mu, const LWEParams* params){
    const int n = params->n;

    for (int i = 0; i < n; ++i) result->a[i] = 0;
    result->b = mu;
    result->current_variance = 0.;
}

/** result = result + sample */
EXPORT void lweAddTo(LWESample* result, const LWESample* sample, const LWEParams* params){
    const int n = params->n;

    for (int i = 0; i < n; ++i) result->a[i] += sample->a[i];
    result->b += sample->b;
    result->current_variance += sample->current_variance; 
}

/** result = result - sample */
EXPORT void lweSubTo(LWESample* result, const LWESample* sample, const LWEParams* params){
    const int n = params->n;

    for (int i = 0; i < n; ++i) result->a[i] -= sample->a[i];
    result->b -= sample->b;
    result->current_variance += sample->current_variance; 
}

/** result = result + p.sample */
EXPORT void lweAddMulTo(LWESample* result, int p, const LWESample* sample, const LWEParams* params){
    const int n = params->n;

    for (int i = 0; i < n; ++i) result->a[i] += p*sample->a[i];
    result->b += p*sample->b;
    result->current_variance += (p*p)*sample->current_variance; 
}

/** result = result - p.sample */
EXPORT void lweSubMulTo(LWESample* result, int p, const LWESample* sample, const LWEParams* params){
    const int n = params->n;

    for (int i = 0; i < n; ++i) result->a[i] -= p*sample->a[i];
    result->b -= p*sample->b;
    result->current_variance += (p*p)*sample->current_variance; 
}


EXPORT void createKeySwitchKey(LWEKeySwitchKey* result, const LWEKey* in_key, const LWEKey* out_key);
//voir si on le garde ou on fait lweAdd (laisser en suspense)

// EXPORT void lweLinearCombination(LWESample* result, const int* combi, const LWESample** samples, const LWEParams* params);

EXPORT void lweKeySwitch(LWESample* result, const LWEKeySwitchKey* ks, const LWESample* sample);







// RingLWE
EXPORT void ringLweKeyGen(RingLWEKey* result){
    const int N = result->params->N;
    const int k = result->params->k;
    uniform_int_distribution<int> distribution(0,1);

    for (int i = 0; i < k; ++i)
        for (int j = 0; j < N; ++j)
            result->key[i].coefs[j] = distribution(generator);
}


EXPORT void ringLweSymEncrypt(RingLWESample* result, TorusPolynomial* message, double alpha, const RingLWEKey* key){
    const int N = key->params->N;
    const int k = key->params->k;
    
    for (int j = 0; j < N; ++j)
        result->b->coefsT[j] = gaussian32(0, alpha) + message->coefsT[j];   
    
    for (int i = 0; i < k; ++i)
    {
	UniformTorusPolynomial(&result->a[i]);
	addMulRToTorusPolynomial(result->b, &key->key[i], &result->a[i]);
    }

    result->current_variance = alpha*alpha;
}

/**
 * encrypts a constant message
 */
EXPORT void ringLweSymEncryptT(RingLWESample* result, Torus32 message, double alpha, const RingLWEKey* key){
    const int N = key->params->N;
    const int k = key->params->k;
    
    for (int j = 0; j < N; ++j)
        result->b->coefsT[j] = gaussian32(0, alpha);
    result->b->coefsT[0]+=message;

    
    for (int i = 0; i < k; ++i)
    {
	UniformTorusPolynomial(&result->a[i]);
	addMulRToTorusPolynomial(result->b, &key->key[i], &result->a[i]);
    }

    result->current_variance = alpha*alpha;
}



/**
 * This function computes the phase of sample by using key : phi = b - a.s
 */
EXPORT void ringLwePhase(TorusPolynomial* phase, const RingLWESample* sample, const RingLWEKey* key){
    const int k = key->params->k;

    CopyTorusPolynomial(phase, sample->b); // phi = b

    for (int i = 0; i < k; ++i)
        subMulRToTorusPolynomial(phase, &key->key[i], &sample->a[i]);
}


/**
 * This function computes the approximation of the phase 
 * à revoir, surtout le Msize
 */
EXPORT void ringLweApproxPhase(TorusPolynomial* message, const TorusPolynomial* phase, int Msize, int N){
    for (int i = 0; i < N; ++i) message->coefsT[i] = approxPhase(phase->coefsT[i], Msize);
}




EXPORT void ringLweSymDecrypt(TorusPolynomial* result, const RingLWESample* sample, const RingLWEKey* key, int Msize){
    ringLwePhase(result, sample, key);
    ringLweApproxPhase(result, result, Msize, key->params->N);
}


EXPORT void ringLweSymDecryptT(Torus32 result, const RingLWESample* sample, const RingLWEKey* key, int Msize){
    TorusPolynomial* phase = new_TorusPolynomial(key->params->N);

    ringLwePhase(phase, sample, key);
    result = approxPhase(phase->coefsT[0], Msize);

    delete_TorusPolynomial(phase);
}




//Arithmetic operations on RingLWE samples
/** result = (0,0) */
EXPORT void RingLweClear(RingLWESample* result, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) ClearTorusPolynomial(&result->a[i]);
    ClearTorusPolynomial(result->b);
    result->current_variance = 0.;
}

/** result = (0,mu) */
EXPORT void RingLweNoiselessTrivial(RingLWESample* result, const TorusPolynomial* mu, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) ClearTorusPolynomial(&result->a[i]);
    CopyTorusPolynomial(result->b, mu);
    result->current_variance = 0.;
}

/** result = (0,mu) where mu is constant*/
EXPORT void RingLweNoiselessTrivialT(RingLWESample* result, const Torus32 mu, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) ClearTorusPolynomial(&result->a[i]);
    ClearTorusPolynomial(result->b);
    result->b->coefsT[0]=mu;
    result->current_variance = 0.;
}

/** result = result + sample */
EXPORT void RingLweAddTo(RingLWESample* result, const RingLWESample* sample, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) 
	AddToTorusPolynomial(&result->a[i], &sample->a[i]);
    AddToTorusPolynomial(result->b, sample->b);
    result->current_variance += sample->current_variance;
}

/** result = result - sample */
EXPORT void RingLweSubTo(RingLWESample* result, const RingLWESample* sample, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) 
	SubToTorusPolynomial(&result->a[i], &sample->a[i]);
    SubToTorusPolynomial(result->b, sample->b);
    result->current_variance += sample->current_variance; 
}

/** result = result + p.sample */
EXPORT void RingLweAddMulTo(RingLWESample* result, int p, const RingLWESample* sample, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) 
	AddMulZToTorusPolynomial(&result->a[i], p, &sample->a[i]);
    AddMulZToTorusPolynomial(result->b, p, sample->b);
    result->current_variance += (p*p)*sample->current_variance;
}

/** result = result - p.sample */
EXPORT void RingLweSubMulTo(RingLWESample* result, int p, const RingLWESample* sample, const RingLWEParams* params){
    const int k = params->k;

    for (int i = 0; i < k; ++i) 
	SubMulZToTorusPolynomial(&result->a[i], p, &sample->a[i]);
    SubMulZToTorusPolynomial(result->b, p, sample->b);
    result->current_variance += (p*p)*sample->current_variance; 
}


/** result = result + p.sample */
EXPORT void RingLweAddMulRTo(RingLWESample* result, const IntPolynomial* p, const RingLWESample* sample, const RingLWEParams* params){
    const int k = params->k;
    
    for (int i = 0; i <= k; ++i)
       addMulRToTorusPolynomial(result->a+i, p, sample->a+i);	
    result->current_variance += NormEuclidIntPolynomial(p)*sample->current_variance; 
}






// RingGSW
/** meme chose que RingLWE */
EXPORT void ringGswKeyGen(RingGSWKey* result){
    const int N = result->params->ringlwe_params->N;
    const int k = result->params->ringlwe_params->k;
    uniform_int_distribution<int> distribution(0,1);

    for (int i = 0; i < k; ++i)
        for (int j = 0; j < N; ++j)
            result->key[i].coefs[j] = distribution(generator);
}



// support Functions for RingGSW
// Result = 0
EXPORT void ringGSWClear(RingGSWSample* result, const RingGSWParams* params){
    const int kpl = params->kpl;

    for (int p = 0; p < kpl; ++p)
        RingLweClear(&result->all_sample[p], params->ringlwe_params);
}

// Result += H
EXPORT void ringGSWAddH(RingGSWSample* result, const RingGSWParams* params){
    const int kpl = params->kpl;
    const int l = params->l;
    int q; // quotient of p/l
    const Torus32* h = params->h;

    // compute result += H
    for (int p = 0; p < kpl; ++p)
    {
        q = (int) (((double) p)/((double) l)); 
        result->all_sample[p].a[q].coefsT[0] += h[p%l]; // when q=k adds to a[k] = b
    }
}

// Result += mu*H
EXPORT void ringGSWAddMuH(RingGSWSample* result, const IntPolynomial* message, const RingGSWParams* params){
    const int N = params->ringlwe_params->N;
    const int kpl = params->kpl;
    const int l = params->l;
    TorusPolynomial* message_h = new_TorusPolynomial_array(l, N); 
    int q; // quotient of p/l
    const Torus32* h = params->h;
    //const int* message = message->coefs;

    // compute mu*h = mu*[1/Bg,...,1/Bg^l]
    for (int i = 0; i < l; ++i)
        for (int j = 0; j < N; ++j)
            message_h[i].coefsT[j] = message->coefs[j]*h[i]; 

    // compute result += mu*H
    for (int p = 0; p < kpl; ++p)
    {
        q = (int) (((double) p)/((double) l)); 
        AddToTorusPolynomial(&result->all_sample[p].a[q], &message_h[p%l]); // when q=k adds to a[k] = b
    }

    delete_TorusPolynomial_array(l, message_h);
}

// Result += mu*H, mu integer
EXPORT void ringGSWAddMuIntH(RingGSWSample* result, const int message, const RingGSWParams* params){
    const int kpl = params->kpl;
    const int l = params->l;
    int q; // quotient of p/l
    const Torus32* h = params->h;

    // compute result += mu*H
    for (int p = 0; p < kpl; ++p)
    {
        q = (int) (((double) p)/((double) l)); 
        result->all_sample[p].a[q].coefsT[0] += message*h[p%l]; // when q=k adds to a[k] = b
    }
}

// Result = RingGsw(0)
EXPORT void ringGSWEncryptZero(RingGSWSample* result, double alpha, const RingGSWKey* key){
    const int N = key->params->ringlwe_params->N;
    const int k = key->params->ringlwe_params->k;
    const int kpl = key->params->kpl;
        
    for (int p = 0; p < kpl; ++p) // each line of the RingGSWSample is a RingLWESample (on aurait pu appeler la fonction ringLweSymEncrypt)
    {
        for (int j = 0; j < N; ++j)
            result->all_sample[p].b->coefsT[j] = gaussian32(0, alpha);   
    
        for (int i = 0; i < k; ++i)
        {
            UniformTorusPolynomial(&result->all_sample[p].a[i]);
            addMulRToTorusPolynomial(result->all_sample[p].b, &key->key[i], &result->all_sample[p].a[i]);
        }
    }
}



void TorusPolynomialMulByXaiMinusOne(TorusPolynomial* result, int a, const TorusPolynomial* bk){
const int N=bk->N;
Torus32* out=result->coefsT;
Torus32* in =bk->coefsT; 

for (int i=0;i<a;i++)//sur que i-a<0
    out[i]= -in[i-a+N]-in[i];

for (int i=a;i<N;i++)//sur que N>i-a>=0
    out[i]= in[i-a]-in[i];
}



//mult externe de X^ai-1 par bki
EXPORT void ringLWEMulByXaiMinusOne(RingLWESample* result, int ai, const RingLWESample* bk, const RingLWEParams* params){
const int k=params->k;
for(int i=0;i<=k;i++)
TorusPolynomialMulByXaiMinusOne(&result->a[i],ai,&bk->a[i]);
}

//mult externe de X^{a_i} par bki
EXPORT void ringGSWMulByXaiMinusOne(RingGSWSample* result, int ai, const RingGSWSample* bk, const RingGSWParams* params){
const RingLWEParams* par=params->ringlwe_params;
const int kpl=params->kpl;
for (int i=0;i<kpl;i++)
ringLWEMulByXaiMinusOne(&result->all_sample[i],ai,&bk->all_sample[i],par);
}

//Update l'accumulateur ligne 5 de l'algo toujours
//void ringLWEDecompH(IntPolynomial* result, const RingLWESample* sample,const RingGSWParams* params);	
EXPORT void ringLWEExternMulRGSWTo(RingLWESample* accum, const RingGSWSample* sample,const RingGSWParams* params){
const RingLWEParams* par=params->ringlwe_params;
const int N=par->N;
const int kpl=params->kpl;
IntPolynomial* dec =new_IntPolynomial_array(kpl,N);
ringLWEDecompH(dec,accum,params);
RingLweClear(accum,par);
for (int i=0; i<kpl;i++) 
    RingLweAddMulRTo(accum,&dec[i],&sample->all_sample[i],par);
}

//crée la clé de KeySwitching
EXPORT void createKeySwitchKey(LWEKeySwitchKey* result, const LWEKey* in_key, const LWEKey* out_key){
    const int n=result->n;
    //const int B=result->base;
    const int l=result->l;
    const int Bl=result->basebit;
    for(int i=0;i<n;i++){
	for(int j=0;j<l;j++){
	    for(int k=0;i<Bl;i++){
		Torus32 x=(in_key->key[i]*k)*(1<<(32-j*Bl));

		lweSymEncrypt(&result->ks[i][j][k],x,out_key->params->alpha_min,out_key);
	    }
	}
    }
}

//sample=(a',b')
EXPORT void lweKeySwitch(LWESample* result, const LWEKeySwitchKey* ks, const LWESample* sample){
    const LWEParams* par=ks->out_params;
    const int n=ks->in_params->n;
    const int Bl=ks->basebit;
    const int l=ks->l;
    const uint32_t mask=ks->mask;
    lweNoiselessTrivial(result,sample->b,par);
    for (int i=0;i<n;i++){
	uint32_t ai=sample->a[i];
	for (int j=0;j<l;j++){
	    uint32_t aij=(ai>>(32-(j+1)*Bl))& mask;
	    lweSubTo(result,&ks->ks[i][j][aij],par);	
	}
    }
}


/**
 * encrypts a poly message
 */
EXPORT void ringGswSymEncrypt(RingGSWSample* result, const IntPolynomial* message, double alpha, const RingGSWKey* key){
    ringGSWEncryptZero(result, alpha, key);
    ringGSWAddMuH(result, message, key->params);
}


/**
 * encrypts a constant message
 */
EXPORT void ringGswSymEncryptInt(RingGSWSample* result, const int message, double alpha, const RingGSWKey* key){
    ringGSWEncryptZero(result, alpha, key);
    ringGSWAddMuIntH(result, message, key->params);
}


/**
 * encrypts a message = 0 ou 1
 */
EXPORT void ringGSWEncryptB(RingGSWSample* result, const int message, double alpha, const RingGSWKey* key){
    ringGSWEncryptZero(result, alpha, key);
    if (message == 1)
        ringGSWAddH(result, key->params);    
}






EXPORT void ringGswSymDecrypt(IntPolynomial* result, const RingGSWSample* sample, const RingGSWKey* key); 
EXPORT int ringGswSymDecryptInt(const RingGSWSample* sample, const RingGSWKey* key); 
//do we really decrypt GSW samples?
// EXPORT void ringGSWMulByXaiMinusOne(GSW* result, int ai, const GSW* bk);
// EXPORT void ringLWEExternMulRLWETo(RLWE* accum, GSW* a); //  accum = a \odot accum








//fonction de decomposition
EXPORT void ringLWEDecompH(IntPolynomial* result, const RingLWESample* sample, const RingGSWParams* params){
    const int k = params->ringlwe_params->k;
    const int l = params->l;

    for (int i = 0; i <= k; ++i) // b=a[k]
        Torus32PolynomialDecompH(result+(i*l), &sample->a[i], params);
}


EXPORT void Torus32PolynomialDecompH(IntPolynomial* result, const TorusPolynomial* sample, const RingGSWParams* params){
    const int N = params->ringlwe_params->N;
    const int l = params->l;
    const int Bgbit = params->Bgbit;
    const uint32_t maskMod = params->maskMod;
    const int32_t halfBg = params->halfBg;
    const uint32_t offset = params->offset;
    
    for (int j = 0; j < N; ++j)
    {
        uint32_t temp0 = sample->coefsT[j] + offset;
        for (int p = 0; p < l; ++p)
        {
            uint32_t temp1 = (temp0 >> ((32-p-1)*Bgbit)) &maskMod; // doute
            result[p].coefs[j] = temp1 - halfBg;
        }   
    }
}
    










//TODO: Ilaria.
EXPORT void ringGSWExternProduct(RingLWESample* result, const RingGSWSample* a, const RingLWESample* b, const RingGSWParams* params){
    const RingLWEParams* parlwe = params->ringlwe_params;
    const int N = parlwe->N;
    const int kpl = params->kpl;
    IntPolynomial* dec = new_IntPolynomial_array(kpl,N);
    
    ringLWEDecompH(dec, b, params);
    
    for (int i = 0; i < kpl; i++) 
        RingLweAddMulRTo(result, &dec[i], &a->all_sample[i], parlwe);
}






//TODO: mettre les mêmes fonctions arithmétiques que pour LWE
//      pour les opérations externes, prévoir int et intPolynomial

//extractions RingLWE -> LWE
EXPORT void ringLweExtractKey(LWEKey* result, const RingLWEKey*); //sans doute un param supplémentaire
EXPORT void ringLweExtractSample(LWESample* result, const RingLWESample* x);

//extraction RingGSW -> SemiRingGSW
EXPORT void ringGswExtractKey(SemiRingGSWSample* result, const RingGSWKey* key);
EXPORT void ringGswExtractSample(RingLWESample* result, const RingGSWSample* x);

/*
//calcule l'arrondi inférieur d'un élément Torus32
int bar(uint64_t b, uint64_t Nx2){
uint64_t xx=b*Nx2+(1l<<31);
return (xx>>32)%Nx2;
}
*/

//LWE to LWE Single gate bootstrapping
//TODO: Malika
EXPORT void bootstrap(LWESample* result, const LWEBootstrappingKey* bk, Torus32 mu1, Torus32 mu0, const LWESample* x){
    Torus32 a=(mu1-mu0)/2;
    const int N=bk->accum_params->N;
    
    const int Ns2=N/2;
    const uint64_t Nx2= 2*N;
    int barb=bar(x->b,Nx2);//TODO
    
    RingLWEParams* accum_par=bk->accum_params;
    TorusPolynomial* testvect=new_TorusPolynomial(N);//je definis le test vector (multiplié par a inclus !
    TorusPolynomial* testvectbis=new_TorusPolynomial(N);
    for (int i=0;i<Ns2;i++){
	testvect->coefsT[i]=a;
    }    
    for (int i=Ns2;i<N;i++){
	testvect->coefsT[i]=-a;
    }
    for (int i=0;i< barb;i++)
	testvectbis->coefsT[i]=-testvect->coefsT[i-barb+N];
    for (int i=barb;i<N;i++)
	testvectbis->coefsT[i]=testvect->coefsT[i-barb];

    //RingLWESample* result, const TorusPolynomial* mu, const RingLWEParams* params
    RingLWESample* acc;
    acc=new_RingLWESample(accum_par);
    RingLweNoiselessTrivial(acc,testvect,accum_par);

    for (int i=0;i<N;i++){

    }
}


//these functions call the bootstrapping, assuming that the message space is {0,1/4} 
EXPORT void lweNand(LWESample* result, const LWEBootstrappingKey* bk, const LWESample* a, const LWESample* b);
EXPORT void lweOr(LWESample* result, const LWEBootstrappingKey* bk, const LWESample* a, const LWESample* b);
EXPORT void lweAnd(LWESample* result, const LWEBootstrappingKey* bk, const LWESample* a, const LWESample* b);
EXPORT void lweXor(LWESample* result, const LWEBootstrappingKey* bk, const LWESample* a, const LWESample* b);
// mux(a,b,c) = a?b:c = a et b + not(a) et c 
EXPORT void lweMux(LWESample* result, const LWEBootstrappingKey* bk, const LWESample* a, const LWESample* b, const LWESample* c);
EXPORT void lweNot(LWESample* result, LWESample* a);


//leveled functions

//LWE to SemiRing Bootstrapping
EXPORT void semiRingBootstrap(LWESample* result, const LWEBootstrappingKey* bk, Torus32 mu1, Torus32 mu0, const LWESample* x);



// EXPORT void ringGswPolyCombination(LWESample* result, const int* combi, const LWESample* samples, const LWEParams* params);

//extractions Ring LWE -> LWE
EXPORT void keyExtract(LWEKey* result, const RingLWEKey* key) //sans doute un param supplémentaire
{
    const int N = key->params->N;
    const int k = key->params->k;
    const int n = result->params->n;
    assert(n == k*N);
    for (int i=0; i<k; i++) {
	result->key[i*N]=key->key[i].coefs[0];
	for (int j=1; j<N; j++)
	    result->key[i*N+j]=-key->key[i].coefs[N-j];
    }
}
    
EXPORT void sampleExtract(LWESample* result, const RingLWESample* x, const LWEParams* params,  const RingLWEParams* rparams) {
    const int N = rparams->N;
    const int k = rparams->k;
    const int n = params->n;
    assert(n == k*N);
    for (int i=0; i<k; i++) {
	for (int j=0; j<N; j++)
	    result->a[i*N+j]=x->a[i].coefsT[j];
    }
    result->b=x->b->coefsT[0];
}

//extraction RingGSW -> GSW
// EXPORT void gswKeyExtract(RingLWEKey* result, const RingGSWKey* key); //sans doute un param supplémentaire
// EXPORT void gswSampleExtract(RingLWESample* result, const RingGSWSample* x);

//bootstrapping
// EXPORT void bootstrap(LWESample* result, const LWEBootstrappingKey* bk, double mu1, double mu0, const LWESample* x);
