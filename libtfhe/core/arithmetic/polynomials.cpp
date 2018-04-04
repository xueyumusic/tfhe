#include "polynomials.h"

using namespace std;


//
// IntPolynomial
//

IntPolynomial::IntPolynomial(const PolynomialParameters *params,
                             TfheThreadContext *context,
                             Allocator *alloc) {
    IntPolynomial *poly = alloc->newObject<IntPolynomial>(params, context, alloc);
    alloc->deleteObject(poly, params, context, alloc);
}

void IntPolynomial::destroy(const PolynomialParameters *params,
                            TfheThreadContext *context,
                            Allocator *alloc) {

}




//
// TorusPolynomial
//


template<typename TORUS>
TorusPolynomial<TORUS>::TorusPolynomial(const PolynomialParameters *params,
                                        TfheThreadContext *context,
                                        Allocator *alloc) {
    coefsT = alloc->newArray<TORUS>(params->N);

}

template<typename TORUS>
void TorusPolynomial<TORUS>::destroy(const PolynomialParameters *params,
                                     TfheThreadContext *context,
                                     Allocator *alloc) {
    alloc->deleteArray<TORUS>(params->N, coefsT);
}

void toto(Allocator *alloc, PolynomialParameters *x, TfheThreadContext *c) {
    IntPolynomial *t = alloc->newObject<IntPolynomial>(x, c, alloc);
    alloc->deleteObject(t, x, c, alloc);
}